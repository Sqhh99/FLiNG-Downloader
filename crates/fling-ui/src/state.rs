//! The UI's mirror of backend state. One entity, updated only from backend
//! events; views read it and send commands through it.

use fling_app::*;
use gpui_kit::{App, AppContext as _, Context, Entity};

use crate::{i18n, theme};

/// A short-lived message above the library table.
#[derive(Clone, PartialEq)]
pub enum LibraryNotice {
    /// The file exists but could not be deleted.
    DeleteFailed { name: String, file_path: String },
    /// The entry was opened but its file is gone, so it was removed.
    FileMissing { name: String, file_path: String },
}

pub struct AppModel {
    handle: BackendHandle,
    pub settings: SettingsSnapshot,
    pub results: Vec<ModifierInfo>,
    /// Bumped whenever `results` is replaced (new search, re-sort), so views
    /// can replay entrance animations and reset scrolling.
    pub results_generation: u64,
    pub search_loading: bool,
    /// The last search could not reach the site.
    pub search_failed: bool,
    pub selection: Option<Selection>,
    pub tasks: Vec<DownloadTask>,
    pub library: Vec<DownloadedModifier>,
    pub app_update: UpdateState,
    pub database_update: UpdateState,
    /// The current library notice and its serial; the serial lets the banner
    /// restart its timer when the same notice comes again.
    pub library_notice: Option<(LibraryNotice, u64)>,
    notice_serial: u64,
}

impl AppModel {
    pub fn new(handle: BackendHandle, events: EventReceiver, cx: &mut App) -> Entity<Self> {
        let settings = handle.initial_settings().clone();
        cx.new(|cx| {
            cx.spawn(async move |this, cx| {
                while let Ok(event) = events.recv().await {
                    if this
                        .update(cx, |model: &mut AppModel, cx| model.apply(event, cx))
                        .is_err()
                    {
                        break;
                    }
                }
            })
            .detach();
            Self {
                handle,
                settings,
                results: Vec::new(),
                results_generation: 0,
                search_loading: false,
                search_failed: false,
                selection: None,
                tasks: Vec::new(),
                library: Vec::new(),
                app_update: UpdateState::default(),
                database_update: UpdateState::default(),
                library_notice: None,
                notice_serial: 0,
            }
        })
    }

    pub fn send(&self, command: Command) {
        self.handle.send(command);
    }

    pub fn suggestions(&self, keyword: &str, max: usize) -> Vec<Suggestion> {
        self.handle.suggestions(keyword, max)
    }

    pub fn active_downloads(&self) -> usize {
        self.count_tasks(|s| s == TaskStatus::Downloading)
    }

    /// Downloads running or waiting for a slot.
    pub fn pending_downloads(&self) -> usize {
        self.count_tasks(|s| matches!(s, TaskStatus::Downloading | TaskStatus::Queued))
    }

    pub fn failed_downloads(&self) -> usize {
        self.count_tasks(|s| s == TaskStatus::Failed)
    }

    fn count_tasks(&self, pick: impl Fn(TaskStatus) -> bool) -> usize {
        self.tasks.iter().filter(|t| pick(t.status)).count()
    }

    fn notify_library(&mut self, notice: LibraryNotice) {
        self.notice_serial += 1;
        self.library_notice = Some((notice, self.notice_serial));
    }

    fn apply(&mut self, event: Event, cx: &mut Context<Self>) {
        match event {
            Event::SearchLoading(loading) => self.search_loading = loading,
            Event::SearchFailed(failed) => self.search_failed = failed,
            Event::Results(results) => {
                self.results = results;
                self.results_generation += 1;
            }
            Event::Selection(selection) => self.selection = Some(selection),
            Event::Tasks(tasks) => self.tasks = tasks,
            Event::Library(library) => self.library = library,
            Event::DeleteFailed { name, file_path } => {
                self.notify_library(LibraryNotice::DeleteFailed { name, file_path })
            }
            Event::LibraryFileMissing { name, file_path } => {
                self.notify_library(LibraryNotice::FileMissing { name, file_path })
            }
            Event::AppUpdate(state) => self.app_update = state,
            Event::DatabaseUpdate(state) => self.database_update = state,
            Event::Settings(settings) => {
                if settings.language != self.settings.language {
                    i18n::set_language(settings.language);
                }
                if settings.theme != self.settings.theme {
                    theme::apply(settings.theme, cx);
                }
                self.settings = settings;
            }
            Event::Open(path) => {
                if let Err(err) = opener::open(&path) {
                    tracing::warn!(?path, %err, "failed to open path");
                }
            }
            Event::Quit => cx.quit(),
        }
        cx.notify();
    }

    pub fn clear_library_notice(&mut self, serial: u64, cx: &mut Context<Self>) {
        if self
            .library_notice
            .as_ref()
            .is_some_and(|(_, s)| *s == serial)
        {
            self.library_notice = None;
            cx.notify();
        }
    }
}
