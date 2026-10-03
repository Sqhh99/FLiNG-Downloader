//! The backend's single state-owning task. Port of the orchestration in
//! `Backend.cpp`, minus everything QML-specific.

use std::future::Future;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::sync::atomic::{AtomicU32, Ordering};
use std::time::Duration;

use fling_config::{AppPaths, Settings};
use fling_core::ModifierInfo;
use fling_core::text::cover_game_id;
use fling_core::version::normalize_version;
use fling_cover::{CoverCache, CoverExtractor, CoverResult};
use fling_download::{DownloadQueue, Library, QueueEvent, RunTarget};
use fling_mapping::{GameMappings, SuggestionIndex, TranslationDatabase};
use fling_net::GetError;
use fling_site::SiteClient;
use fling_update::{CheckResult, ReleaseInfo, ReleaseKind, UpdateError, Updater, launch_installer};
use tokio::runtime::Handle;
use tokio::sync::mpsc::{UnboundedReceiver, WeakUnboundedSender};

use crate::api::*;
use crate::{BackendConfig, SharedLookup, StartupTimings};

/// The installer gets a moment to start before the app quits.
const QUIT_AFTER_INSTALLER: Duration = Duration::from_millis(300);

pub(crate) enum Msg {
    Command(Command),
    SearchDone {
        id: u64,
        results: Vec<ModifierInfo>,
    },
    DetailDone {
        id: u64,
        url: String,
        result: Result<ModifierInfo, GetError>,
    },
    CoverDone {
        game_id: String,
        url: String,
        result: CoverResult,
    },
    Queue(QueueEvent),
    CheckDone(ReleaseKind, Result<CheckResult, UpdateError>),
    UpdateProgress(ReleaseKind, f32),
    DownloadDone(ReleaseKind, Result<PathBuf, UpdateError>),
    AutoCheck(ReleaseKind),
    QuitAfterInstaller,
}

struct UpdateFlow {
    state: UpdateState,
    release: Option<ReleaseInfo>,
}

pub(crate) struct Backend {
    runtime: Handle,
    tx: WeakUnboundedSender<Msg>,
    events: async_channel::Sender<Event>,
    lookup: Arc<SharedLookup>,
    paths: AppPaths,
    app_version: String,
    startup: Option<StartupTimings>,

    settings: Settings,
    database: Arc<TranslationDatabase>,
    mappings: Arc<GameMappings>,
    site: SiteClient,
    queue: DownloadQueue,
    library: Library,
    updater: Arc<Updater>,
    covers: CoverExtractor,

    results: Vec<ModifierInfo>,
    sort: SortOrder,
    next_search_id: u64,
    active_search_id: u64,
    search_loading: bool,

    selection: Option<Selection>,
    next_detail_id: u64,
    active_detail_id: u64,
    /// Staleness guard for covers: a result counts only if both the game and
    /// the screenshot still match what is selected.
    cover_game_id: String,
    cover_url: Option<String>,

    app_update: UpdateFlow,
    database_update: UpdateFlow,
}

fn send(tx: &WeakUnboundedSender<Msg>, msg: Msg) {
    if let Some(tx) = tx.upgrade() {
        let _ = tx.send(msg);
    }
}

fn sort_results(list: &mut [ModifierInfo], order: SortOrder) {
    match order {
        SortOrder::RecentlyUpdated => list.sort_by(|a, b| b.last_update.cmp(&a.last_update)),
        SortOrder::Name => list.sort_by_cached_key(|m| m.name.to_lowercase()),
        SortOrder::OptionsCount => list.sort_by_key(|m| std::cmp::Reverse(m.options_count)),
    }
}

/// The folder holding `file`, if it exists.
fn existing_parent(file: &str) -> Option<PathBuf> {
    if file.is_empty() {
        return None;
    }
    Path::new(file)
        .parent()
        .filter(|p| p.is_dir())
        .map(Path::to_path_buf)
}

impl Backend {
    pub(crate) fn new(
        config: BackendConfig,
        runtime: Handle,
        tx: WeakUnboundedSender<Msg>,
        events: async_channel::Sender<Event>,
        lookup: Arc<SharedLookup>,
    ) -> Self {
        let BackendConfig {
            paths,
            app_version,
            http,
            site_base_url,
            cover_detector,
            startup,
        } = config;
        if let Err(err) = paths.ensure_dirs() {
            tracing::warn!(%err, "failed to create app directories");
        }
        let settings = Settings::load(&paths);
        *lookup.language.write() = settings.language();

        let database = Arc::new(TranslationDatabase::new(
            paths.database_override(),
            paths.bundled_database(),
        ));
        let queue_tx = tx.clone();
        let queue = DownloadQueue::new(
            http.clone(),
            runtime.clone(),
            Arc::new(move |event| send(&queue_tx, Msg::Queue(event))),
        );
        let app_update = UpdateFlow {
            state: UpdateState {
                current_version: app_version.clone(),
                ..UpdateState::default()
            },
            release: None,
        };

        let mut backend = Self {
            site: SiteClient::with_base_url(http.clone(), &site_base_url),
            library: Library::load(paths.library_file()),
            updater: Arc::new(Updater::new(
                http.clone(),
                paths.app_update_dir(),
                paths.database_update_dir(),
            )),
            covers: CoverExtractor::new(http, CoverCache::new(paths.covers_dir()), cover_detector),
            runtime,
            tx,
            events,
            lookup,
            paths,
            app_version,
            startup,
            settings,
            database,
            mappings: Arc::default(),
            queue,
            results: Vec::new(),
            sort: SortOrder::default(),
            next_search_id: 0,
            active_search_id: 0,
            search_loading: false,
            selection: None,
            next_detail_id: 0,
            active_detail_id: 0,
            cover_game_id: String::new(),
            cover_url: None,
            app_update,
            database_update: UpdateFlow {
                state: UpdateState::default(),
                release: None,
            },
        };
        if !backend.reload_mappings() {
            tracing::warn!("no game mappings loaded from the translation database");
        }
        backend.refresh_database_version();
        backend
    }

    pub(crate) fn settings_snapshot(&self) -> SettingsSnapshot {
        SettingsSnapshot {
            theme: self.settings.theme(),
            language: self.settings.language(),
            update_source: self.settings.update_source(),
            auto_check_app_updates: self.settings.auto_check_app_updates(),
            auto_check_database_updates: self.settings.auto_check_database_updates(),
            download_directory: self.settings.download_directory(),
            app_version: self.app_version.clone(),
        }
    }

    fn emit(&self, event: Event) {
        let _ = self.events.try_send(event);
    }

    /// Runs `work` on the runtime and feeds its result back into the loop.
    fn spawn(&self, work: impl Future<Output = Msg> + Send + 'static) {
        let tx = self.tx.clone();
        self.runtime.spawn(async move { send(&tx, work.await) });
    }

    fn after(&self, delay: Duration, msg: Msg) {
        self.spawn(async move {
            tokio::time::sleep(delay).await;
            msg
        });
    }

    pub(crate) async fn run(mut self, mut rx: UnboundedReceiver<Msg>) {
        self.emit(Event::Settings(self.settings_snapshot()));
        self.emit(Event::Library(self.library.items().to_vec()));
        self.emit(Event::Tasks(Vec::new()));
        self.emit(Event::AppUpdate(self.app_update.state.clone()));
        self.emit(Event::DatabaseUpdate(self.database_update.state.clone()));

        if let Some(timings) = self.startup.clone() {
            self.fetch_recent();
            let covers = self.covers.clone();
            self.runtime.spawn(async move {
                tokio::time::sleep(timings.cover_warm_up).await;
                let _ = tokio::task::spawn_blocking(move || covers.warm_up()).await;
            });
            if self.settings.auto_check_app_updates() {
                self.after(timings.app_update_check, Msg::AutoCheck(ReleaseKind::App));
            }
            if self.settings.auto_check_database_updates() {
                self.after(
                    timings.database_update_check,
                    Msg::AutoCheck(ReleaseKind::Database),
                );
            }
        }

        while let Some(msg) = rx.recv().await {
            self.handle(msg);
        }
    }

    fn handle(&mut self, msg: Msg) {
        match msg {
            Msg::Command(command) => self.command(command),
            Msg::SearchDone { id, results } => self.search_done(id, results),
            Msg::DetailDone { id, url, result } => self.detail_done(id, &url, result),
            Msg::CoverDone {
                game_id,
                url,
                result,
            } => self.cover_done(&game_id, &url, result),
            Msg::Queue(QueueEvent::Tasks(tasks)) => self.emit(Event::Tasks(tasks)),
            Msg::Queue(QueueEvent::Completed(record)) => {
                self.library.upsert(record);
                self.emit(Event::Library(self.library.items().to_vec()));
            }
            Msg::CheckDone(kind, result) => self.check_done(kind, result),
            Msg::UpdateProgress(kind, progress) => {
                let flow = self.flow_mut(kind);
                if flow.state.downloading {
                    flow.state.progress = progress;
                    self.emit_update(kind);
                }
            }
            Msg::DownloadDone(kind, result) => self.download_done(kind, result),
            Msg::AutoCheck(kind) => self.check_update(kind),
            Msg::QuitAfterInstaller => self.emit(Event::Quit),
        }
    }

    fn command(&mut self, command: Command) {
        match command {
            Command::Search(term) => self.search(term),
            Command::FetchRecent => self.fetch_recent(),
            Command::SetSort(order) => {
                self.sort = order;
                sort_results(&mut self.results, order);
                self.emit(Event::Results(self.results.clone()));
            }
            Command::Select(index) => self.select(index),
            Command::RetryDetail => {
                if self
                    .selection
                    .as_ref()
                    .is_some_and(|s| !s.modifier.url.is_empty())
                {
                    self.request_detail();
                }
            }
            Command::Download { version_index } => {
                if let Some(selection) = &self.selection {
                    let dir = self.settings.download_directory();
                    self.queue.enqueue(&selection.modifier, version_index, &dir);
                }
            }
            Command::PauseTask(id) => self.queue.pause(&id),
            Command::ResumeTask(id) => self.queue.resume(&id),
            Command::CancelTask(id) => self.queue.cancel(&id),
            Command::RemoveTask(id) => self.queue.remove(&id),
            Command::RunLibraryItem(index) => match self.library.run_target(index) {
                Some(RunTarget::Open(path)) => self.emit(Event::Open(path)),
                Some(RunTarget::Missing) => {
                    self.emit(Event::Library(self.library.items().to_vec()))
                }
                None => {}
            },
            Command::DeleteLibraryItem(index) => match self.library.delete(index) {
                Ok(true) => self.emit(Event::Library(self.library.items().to_vec())),
                Ok(false) => {}
                Err(err) => {
                    tracing::warn!(file = err.file_path, "could not delete trainer file");
                    self.emit(Event::DeleteFailed {
                        name: err.name,
                        file_path: err.file_path,
                    });
                }
            },
            Command::OpenFolder(target) => self.open_folder(target),
            Command::CheckAppUpdate => self.check_update(ReleaseKind::App),
            Command::DownloadAppUpdate => self.download_update(ReleaseKind::App),
            Command::CheckDatabaseUpdate => self.check_update(ReleaseKind::Database),
            Command::DownloadDatabaseUpdate => self.download_update(ReleaseKind::Database),
            Command::SetTheme(index) => {
                self.settings.set_theme(index);
                self.emit_settings();
            }
            Command::SetLanguage(language) => {
                self.settings.set_language(language);
                *self.lookup.language.write() = language;
                self.emit_settings();
            }
            Command::SetUpdateSource(source) => {
                self.settings.set_update_source(source);
                self.emit_settings();
            }
            Command::SetAutoCheckAppUpdates(enabled) => {
                self.settings.set_auto_check_app_updates(enabled);
                self.emit_settings();
            }
            Command::SetAutoCheckDatabaseUpdates(enabled) => {
                self.settings.set_auto_check_database_updates(enabled);
                self.emit_settings();
            }
            Command::SetDownloadDirectory(dir) => {
                if let Err(err) = self.settings.set_download_directory(&dir) {
                    tracing::warn!(?dir, %err, "cannot use download directory");
                }
                self.emit_settings();
            }
        }
    }

    fn emit_settings(&self) {
        self.emit(Event::Settings(self.settings_snapshot()));
    }

    // ---- search -------------------------------------------------------------

    fn begin_search(&mut self) -> u64 {
        self.next_search_id += 1;
        self.active_search_id = self.next_search_id;
        if !self.search_loading {
            self.search_loading = true;
            self.emit(Event::SearchLoading(true));
        }
        self.active_search_id
    }

    fn search(&mut self, term: String) {
        let id = self.begin_search();
        let site = self.site.clone();
        let mappings = self.mappings.clone();
        self.spawn(async move {
            let results = site.search(&term, &mappings).await.unwrap_or_else(|err| {
                tracing::warn!(%err, "search failed");
                Vec::new()
            });
            Msg::SearchDone { id, results }
        });
    }

    fn fetch_recent(&mut self) {
        let id = self.begin_search();
        let site = self.site.clone();
        let cache = self.paths.recent_cache_file();
        let tx = self.tx.clone();
        // May deliver twice: the cached list at once, then a changed network list.
        self.runtime.spawn(async move {
            site.fetch_recent(&cache, |results| send(&tx, Msg::SearchDone { id, results }))
                .await;
        });
    }

    fn search_done(&mut self, id: u64, mut results: Vec<ModifierInfo>) {
        // A newer request is in flight; this one's answer is stale.
        if id != self.active_search_id {
            return;
        }
        sort_results(&mut results, self.sort);
        self.results = results;
        self.emit(Event::Results(self.results.clone()));
        if self.search_loading {
            self.search_loading = false;
            self.emit(Event::SearchLoading(false));
        }
    }

    // ---- selection, detail and cover ---------------------------------------

    fn emit_selection(&self) {
        if let Some(selection) = &self.selection {
            self.emit(Event::Selection(selection.clone()));
        }
    }

    fn selection_mut(&mut self) -> &mut Selection {
        self.selection.get_or_insert_with(Selection::default)
    }

    fn select(&mut self, index: usize) {
        let Some(row) = self.results.get(index) else {
            return;
        };
        let mut modifier = row.clone();
        // Until the detail page answers, versions and options on screen would
        // belong to the previous selection.
        modifier.versions.clear();
        modifier.options.clear();

        let game_id = cover_game_id(&modifier.name);
        let cached = self.covers.cache().cached_cover(&game_id);
        self.cover_game_id = game_id.clone();
        self.cover_url = None;
        let list_screenshot = modifier.screenshot_url.clone();
        self.selection = Some(Selection {
            index,
            modifier,
            detail: DetailState::Idle,
            cover: cached
                .clone()
                .map_or(CoverState::Loading, CoverState::Ready),
        });
        self.emit_selection();

        // The list row usually carries the screenshot already, so the cover
        // need not wait for the detail page.
        if cached.is_none() && !game_id.is_empty() {
            self.start_cover_fetch(list_screenshot);
        }
        self.request_detail();
    }

    fn stop_cover_loading(&mut self) {
        let selection = self.selection_mut();
        if selection.cover == CoverState::Loading {
            selection.cover = CoverState::Missing;
        }
    }

    fn request_detail(&mut self) {
        let url = self.selection_mut().modifier.url.clone();
        if url.is_empty() {
            self.selection_mut().detail = DetailState::Empty;
            if self.cover_url.is_none() {
                self.stop_cover_loading();
            }
            self.emit_selection();
            return;
        }
        // Tag the request so a late reply for another game cannot write its
        // versions (and download URLs) into this selection.
        self.next_detail_id += 1;
        let id = self.next_detail_id;
        self.active_detail_id = id;
        self.selection_mut().detail = DetailState::Loading;
        self.emit_selection();

        let site = self.site.clone();
        self.spawn(async move {
            let result = site.detail(&url).await;
            Msg::DetailDone { id, url, result }
        });
    }

    fn detail_done(&mut self, id: u64, url: &str, result: Result<ModifierInfo, GetError>) {
        let current_url = self.selection.as_ref().map(|s| s.modifier.url.as_str());
        if id != self.active_detail_id || current_url != Some(url) {
            return;
        }
        match result {
            Err(err) => {
                tracing::warn!(url, %err, "failed to load trainer details");
                self.selection_mut().detail = DetailState::Error;
                // A cover fetch started from the list row finishes on its own.
                if self.cover_url.is_none() {
                    self.stop_cover_loading();
                }
                self.emit_selection();
            }
            Ok(detail) => {
                let selection = self.selection_mut();
                let modifier = &mut selection.modifier;
                modifier.versions = detail.versions;
                modifier.options = detail.options;
                modifier.options_count = detail.options_count;
                modifier.screenshot_url = detail.screenshot_url;
                // A page can parse fine and still offer no downloads.
                selection.detail = if modifier.versions.is_empty() {
                    DetailState::Empty
                } else {
                    DetailState::Ready
                };
                self.emit_selection();
                self.extract_cover();
            }
        }
    }

    fn extract_cover(&mut self) {
        let selection = self.selection_mut();
        let screenshot = selection.modifier.screenshot_url.clone();
        let already_shown = matches!(selection.cover, CoverState::Ready(_));
        if already_shown
            || self
                .covers
                .cache()
                .cached_cover(&self.cover_game_id)
                .is_some()
        {
            return;
        }
        if self.cover_game_id.is_empty() || screenshot.is_empty() {
            if self.cover_url.is_none() {
                self.stop_cover_loading();
                self.emit_selection();
            }
            return;
        }
        self.start_cover_fetch(screenshot);
    }

    fn start_cover_fetch(&mut self, url: String) {
        let game_id = self.cover_game_id.clone();
        // A second request for the screenshot already in hand changes nothing.
        if game_id.is_empty() || url.is_empty() || self.cover_url.as_deref() == Some(url.as_str()) {
            return;
        }
        self.cover_url = Some(url.clone());

        // The model already looked at this exact screenshot and found nothing.
        if self.covers.cache().is_known_without_cover(&game_id, &url) {
            self.stop_cover_loading();
            self.emit_selection();
            return;
        }
        let selection = self.selection_mut();
        if selection.cover != CoverState::Loading {
            selection.cover = CoverState::Loading;
            self.emit_selection();
        }
        let covers = self.covers.clone();
        self.spawn(async move {
            let result = covers.extract(&url, &game_id).await;
            Msg::CoverDone {
                game_id,
                url,
                result,
            }
        });
    }

    fn cover_done(&mut self, game_id: &str, url: &str, result: CoverResult) {
        if game_id != self.cover_game_id || self.cover_url.as_deref() != Some(url) {
            return;
        }
        self.selection_mut().cover = match result {
            CoverResult::Saved(path) => CoverState::Ready(path),
            CoverResult::NoCover | CoverResult::Failed => CoverState::Missing,
        };
        self.emit_selection();
    }

    // ---- library and folders -----------------------------------------------

    fn open_folder(&self, target: FolderTarget) {
        // Fall back to the download directory only when the item has no usable
        // folder of its own.
        let own = match target {
            FolderTarget::Downloads => None,
            FolderTarget::Task(id) => self
                .queue
                .tasks()
                .into_iter()
                .find(|t| t.id == id)
                .and_then(|t| existing_parent(&t.save_path)),
            FolderTarget::LibraryItem(index) => self
                .library
                .items()
                .get(index)
                .and_then(|item| existing_parent(&item.file_path)),
        };
        self.emit(Event::Open(
            own.unwrap_or_else(|| self.settings.download_directory()),
        ));
    }

    // ---- translation database ----------------------------------------------

    /// Rebuilds the lookup indexes from the active database. `false` when it
    /// has no usable rows.
    fn reload_mappings(&mut self) -> bool {
        let games = self.database.load_all_games();
        self.mappings = Arc::new(GameMappings::from_records(&games));
        *self.lookup.suggestions.write() = Arc::new(SuggestionIndex::from_records(&games));
        !self.mappings.is_empty()
    }

    fn refresh_database_version(&mut self) {
        self.database_update.state.current_version =
            normalize_version(&self.database.current_release_tag()).to_owned();
    }

    // ---- updates ------------------------------------------------------------

    fn flow_mut(&mut self, kind: ReleaseKind) -> &mut UpdateFlow {
        match kind {
            ReleaseKind::App => &mut self.app_update,
            ReleaseKind::Database => &mut self.database_update,
        }
    }

    fn emit_update(&self, kind: ReleaseKind) {
        self.emit(match kind {
            ReleaseKind::App => Event::AppUpdate(self.app_update.state.clone()),
            ReleaseKind::Database => Event::DatabaseUpdate(self.database_update.state.clone()),
        });
    }

    fn check_update(&mut self, kind: ReleaseKind) {
        if kind == ReleaseKind::Database {
            self.refresh_database_version();
        }
        let source = self.settings.update_source();
        let flow = self.flow_mut(kind);
        if flow.state.checking || flow.state.downloading {
            return;
        }
        flow.state.checking = true;
        flow.state.status = UpdateStatus::Checking;
        let current = flow.state.current_version.clone();
        self.emit_update(kind);

        let updater = self.updater.clone();
        self.spawn(
            async move { Msg::CheckDone(kind, updater.check(kind, &current, source).await) },
        );
    }

    fn check_done(&mut self, kind: ReleaseKind, result: Result<CheckResult, UpdateError>) {
        let flow = self.flow_mut(kind);
        flow.state.checking = false;
        match result {
            Err(err) => {
                flow.release = None;
                flow.state.available = false;
                flow.state.latest_version.clear();
                flow.state.source = None;
                flow.state.published_at.clear();
                flow.state.status = UpdateStatus::CheckFailed(err);
            }
            Ok(CheckResult {
                release,
                update_available,
            }) => {
                flow.state.available = update_available;
                flow.state.latest_version = release.version.clone();
                flow.state.source = Some(release.source);
                flow.state.published_at = release.published_at.clone();
                flow.state.status = if update_available {
                    UpdateStatus::Available
                } else {
                    UpdateStatus::UpToDate
                };
                flow.release = Some(release);
            }
        }
        self.emit_update(kind);
    }

    fn download_update(&mut self, kind: ReleaseKind) {
        let flow = self.flow_mut(kind);
        let Some(release) = flow.release.clone().filter(|r| r.is_valid()) else {
            return;
        };
        if !flow.state.available || flow.state.downloading {
            return;
        }
        flow.state.downloading = true;
        flow.state.progress = 0.0;
        flow.state.status = UpdateStatus::Downloading;
        self.emit_update(kind);

        // Report whole percents only; a byte-level stream would flood the UI.
        let tx = self.tx.clone();
        let last_percent = Arc::new(AtomicU32::new(0));
        let progress = Arc::new(move |received: u64, total: u64| {
            if total == 0 {
                return;
            }
            let percent = (received.saturating_mul(100) / total).min(100) as u32;
            if last_percent.swap(percent, Ordering::Relaxed) != percent {
                send(&tx, Msg::UpdateProgress(kind, percent as f32 / 100.0));
            }
        });
        let updater = self.updater.clone();
        self.spawn(async move {
            Msg::DownloadDone(kind, updater.download(kind, &release, Some(progress)).await)
        });
    }

    fn download_done(&mut self, kind: ReleaseKind, result: Result<PathBuf, UpdateError>) {
        let path = match result {
            Ok(path) => path,
            Err(err) => {
                let flow = self.flow_mut(kind);
                flow.state.downloading = false;
                flow.state.progress = 0.0;
                flow.state.status = UpdateStatus::DownloadFailed(err.to_string());
                self.emit_update(kind);
                return;
            }
        };
        match kind {
            ReleaseKind::App => {
                let flow = self.flow_mut(kind);
                flow.state.downloading = false;
                flow.state.progress = 1.0;
                flow.state.status = UpdateStatus::LaunchingInstaller;
                self.emit_update(kind);
                match launch_installer(&path) {
                    Ok(()) => self.after(QUIT_AFTER_INSTALLER, Msg::QuitAfterInstaller),
                    Err(err) => {
                        tracing::warn!(?path, %err, "failed to launch installer");
                        self.app_update.state.status = UpdateStatus::LaunchFailed;
                        self.emit_update(kind);
                    }
                }
            }
            ReleaseKind::Database => {
                let installed = self.database.install_override(&path);
                let reloaded = installed.is_ok() && self.reload_mappings();
                self.refresh_database_version();
                let current = self.database_update.state.current_version.clone();
                let state = &mut self.database_update.state;
                state.downloading = false;
                match installed {
                    Err(err) => {
                        state.progress = 0.0;
                        state.status = UpdateStatus::InstallFailed(err.to_string());
                    }
                    Ok(()) if !reloaded => {
                        state.progress = 0.0;
                        state.status = UpdateStatus::ReloadFailed;
                    }
                    Ok(()) => {
                        state.available = false;
                        state.latest_version = current;
                        state.progress = 1.0;
                        state.status = UpdateStatus::Installed;
                    }
                }
                self.emit_update(kind);
            }
        }
    }
}
