//! The backend driven through its public command/event API, with a fake
//! network and throwaway directories.

use std::collections::HashMap;
use std::path::Path;
use std::sync::Arc;
use std::time::Duration;

use fling_app::*;
use fling_config::AppPaths;
use fling_cover::DetectorLoader;
use fling_mapping::test_util::create_database;
use fling_net::fake::FakeHttpClient;
use fling_net::{DownloadError, DownloadOutcome, DownloadRequest, GetError, HttpClient};
use futures::future::BoxFuture;
use parking_lot::Mutex;
use tokio::sync::Notify;

const BASE: &str = "https://fling.test/";
const DB_RELEASE_API: &str =
    "https://api.github.com/repos/Sqhh99/game-mappings-updater/releases/latest";

fn homepage(entries: &[(&str, &str)]) -> String {
    entries
        .iter()
        .map(|(name, date_day)| {
            format!(
                r#"<article class="post-standard"><div class="post-details-day">{date_day}</div>
<div class="post-details-month">Sep</div><div class="post-details-year">2026</div>
<h2 class="post-title"><a href="{BASE}{name}/">{name} trainer</a></h2><p>12 Options</p></article>"#
            )
        })
        .collect()
}

fn detail_page(download: &str) -> String {
    format!(
        r#"<p>Game Version: v1.0+ · Last Updated: 2026.09.01</p>
<table><tr><td><a class="attachment-link" href="{download}">Trainer v1.0</a></td></tr></table>
<p>Num 1 – Infinite Health<br></p>"#
    )
}

/// Wraps the fake client; GETs to a gated URL wait until it is released.
#[derive(Clone, Default)]
struct GatedClient {
    inner: FakeHttpClient,
    gates: Arc<Mutex<HashMap<String, Arc<Notify>>>>,
}

impl GatedClient {
    fn gate(&self, url: &str) -> Arc<Notify> {
        self.gates.lock().entry(url.to_owned()).or_default().clone()
    }
}

impl HttpClient for GatedClient {
    fn get(&self, url: &str) -> BoxFuture<'static, Result<Vec<u8>, GetError>> {
        let gate = self.gates.lock().get(url).cloned();
        let inner = self.inner.get(url);
        Box::pin(async move {
            if let Some(gate) = gate {
                gate.notified().await;
            }
            inner.await
        })
    }

    fn download(
        &self,
        request: DownloadRequest,
    ) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>> {
        self.inner.download(request)
    }
}

struct Harness {
    _dir: tempfile::TempDir,
    paths: AppPaths,
    handle: BackendHandle,
    events: async_channel::Receiver<Event>,
}

impl Harness {
    fn start(http: Arc<dyn HttpClient>, startup: bool) -> Self {
        let dir = tempfile::tempdir().unwrap();
        let paths = AppPaths::rooted(dir.path());
        create_database(
            &paths.exe_dir.join("resources/fling_translations.db"),
            "v0.0.1",
            "1",
            &[("Elden Ring", "elden ring", "艾尔登法环", "エルデンリング")],
        );
        let no_model: DetectorLoader = Arc::new(|| None);
        let config = BackendConfig {
            paths: paths.clone(),
            app_version: "1.1.10".into(),
            http,
            site_base_url: BASE.into(),
            cover_detector: no_model,
            startup: startup.then(|| StartupTimings {
                cover_warm_up: Duration::from_millis(0),
                app_update_check: Duration::from_secs(3600),
                database_update_check: Duration::from_secs(3600),
            }),
        };
        let (handle, events) = start(config).unwrap();
        Self {
            _dir: dir,
            paths,
            handle,
            events,
        }
    }

    /// The next event matching `pick`, skipping others.
    async fn next<T>(&self, mut pick: impl FnMut(Event) -> Option<T>) -> T {
        let wait = async {
            loop {
                let event = self.events.recv().await.expect("backend stopped");
                if let Some(value) = pick(event) {
                    return value;
                }
            }
        };
        tokio::time::timeout(Duration::from_secs(5), wait)
            .await
            .expect("timed out waiting for event")
    }

    async fn selection_where(&self, mut check: impl FnMut(&Selection) -> bool) -> Selection {
        self.next(|e| match e {
            Event::Selection(s) if check(&s) => Some(s),
            _ => None,
        })
        .await
    }
}

#[tokio::test]
async fn startup_reports_state_and_loads_recent_list() {
    let fake = FakeHttpClient::new();
    fake.page(BASE, homepage(&[("alpha", "1"), ("beta", "9")]));
    let h = Harness::start(Arc::new(fake), true);

    let settings = h
        .next(|e| {
            if let Event::Settings(s) = e {
                Some(s)
            } else {
                None
            }
        })
        .await;
    assert_eq!(settings.app_version, "1.1.10");
    assert_eq!(settings.download_directory, h.paths.downloads);

    let db = h
        .next(|e| {
            if let Event::DatabaseUpdate(s) = e {
                Some(s)
            } else {
                None
            }
        })
        .await;
    assert_eq!(db.current_version, "0.0.1");

    let results = h
        .next(|e| {
            if let Event::Results(r) = e {
                Some(r)
            } else {
                None
            }
        })
        .await;
    // Default order: most recently updated first.
    let names: Vec<_> = results.iter().map(|m| m.name.as_str()).collect();
    assert_eq!(names, ["Beta Trainer", "Alpha Trainer"]);
    h.next(|e| matches!(e, Event::SearchLoading(false)).then_some(()))
        .await;
    assert!(h.paths.recent_cache_file().exists());
}

#[tokio::test]
async fn select_detail_and_download_into_library() {
    let fake = FakeHttpClient::new();
    fake.page(BASE, homepage(&[("alpha", "1")]));
    fake.page(
        &format!("{BASE}alpha/"),
        detail_page("https://fling.test/downloads/abc,,"),
    );
    fake.file(
        "https://fling.test/downloads/abc",
        b"MZ\x90\x00exe".to_vec(),
    );
    let h = Harness::start(Arc::new(fake), true);
    h.next(|e| {
        if let Event::Results(r) = e {
            (!r.is_empty()).then_some(())
        } else {
            None
        }
    })
    .await;

    h.handle.send(Command::Select(0));
    let selection = h.selection_where(|s| s.detail == DetailState::Ready).await;
    assert_eq!(selection.modifier.name, "Alpha Trainer");
    assert_eq!(
        selection.modifier.game_version, "Latest",
        "list value kept; detail does not overwrite it"
    );
    // The table pass and the attachment-link pass both list the link (Qt quirk).
    assert_eq!(selection.modifier.versions.len(), 2);
    assert_eq!(
        selection.modifier.options,
        ["● Basic Options", "• Num 1 – Infinite Health"]
    );

    h.handle.send(Command::Download { version_index: 0 });
    let library = h
        .next(|e| {
            if let Event::Library(l) = e {
                (!l.is_empty()).then_some(l)
            } else {
                None
            }
        })
        .await;
    let expected = h.paths.downloads.join("Alpha Trainer_Trainer v1.0.exe");
    assert_eq!(Path::new(&library[0].file_path), expected);
    assert!(expected.exists());
    assert_eq!(library[0].version, "Trainer v1.0");
    assert!(h.paths.library_file().exists());

    h.handle
        .send(Command::OpenFolder(FolderTarget::LibraryItem(0)));
    let opened = h
        .next(|e| {
            if let Event::Open(p) = e {
                Some(p)
            } else {
                None
            }
        })
        .await;
    assert_eq!(opened, h.paths.downloads);

    h.handle.send(Command::DeleteLibraryItem(0));
    h.next(|e| {
        if let Event::Library(l) = e {
            l.is_empty().then_some(())
        } else {
            None
        }
    })
    .await;
    assert!(!expected.exists());
}

#[tokio::test]
async fn trainer_names_follow_settings_and_name_new_downloads() {
    let fake = FakeHttpClient::new();
    fake.page(BASE, homepage(&[("Elden Ring", "1")]));
    fake.page(
        &format!("{BASE}Elden Ring/"),
        detail_page("https://fling.test/downloads/er"),
    );
    fake.file("https://fling.test/downloads/er", b"MZ\x90\x00exe".to_vec());
    let h = Harness::start(Arc::new(fake), true);
    let results_where = |check: fn(&ModifierInfo) -> bool| {
        h.next(move |e| match e {
            Event::Results(r) if r.first().is_some_and(check) => Some(r),
            _ => None,
        })
    };

    // Default: follow the UI language, which defaults to Chinese.
    let results = results_where(|m| !m.name.is_empty()).await;
    assert_eq!(results[0].name, "Elden Ring Trainer");
    assert_eq!(results[0].display_name, "艾尔登法环");
    assert_eq!(results[0].display_subtitle, "Elden Ring");

    h.handle.send(Command::Select(0));
    let selection = h.selection_where(|s| s.detail == DetailState::Ready).await;
    assert_eq!(selection.modifier.display_name, "艾尔登法环");
    assert_eq!(selection.modifier.display_subtitle, "Elden Ring");

    h.handle.send(Command::Download { version_index: 0 });
    let library = h
        .next(|e| match e {
            Event::Library(l) if !l.is_empty() => Some(l),
            _ => None,
        })
        .await;
    let expected = h
        .paths
        .downloads
        .join("艾尔登法环 (Elden Ring)_Trainer v1.0.exe");
    assert_eq!(Path::new(&library[0].file_path), expected);
    assert!(expected.exists());
    assert_eq!(library[0].name, "Elden Ring Trainer");
    assert_eq!(library[0].display_name, "艾尔登法环");
    assert_eq!(library[0].display_subtitle, "Elden Ring");
    // The library file keeps the site name and never stores display names.
    let json = std::fs::read_to_string(h.paths.library_file()).unwrap();
    assert!(json.contains("\"Elden Ring Trainer\""));
    assert!(!json.contains("艾尔登法环\""));
    assert!(!json.contains("displayName"));
    assert!(!json.contains("displaySubtitle"));

    h.handle.send(Command::SetTrainerNameLanguage(
        TrainerNameLanguage::English,
    ));
    let results = results_where(|m| m.display_name == m.name).await;
    assert_eq!(results[0].display_name, "Elden Ring Trainer");
    assert!(results[0].display_subtitle.is_empty());
    let selection = h
        .selection_where(|s| s.modifier.display_name == "Elden Ring Trainer")
        .await;
    assert_eq!(selection.detail, DetailState::Ready);
    let library = h
        .next(|e| match e {
            Event::Library(l) if !l.is_empty() => Some(l),
            _ => None,
        })
        .await;
    assert_eq!(library[0].display_name, "Elden Ring Trainer");
    assert!(library[0].display_subtitle.is_empty());

    // Back to following the UI language, then switch the UI to Japanese.
    h.handle.send(Command::SetTrainerNameLanguage(
        TrainerNameLanguage::FollowUi,
    ));
    results_where(|m| m.display_name == "艾尔登法环").await;
    h.handle.send(Command::SetLanguage(Language::Japanese));
    let settings = h
        .next(|e| match e {
            Event::Settings(s) if s.language == Language::Japanese => Some(s),
            _ => None,
        })
        .await;
    assert_eq!(
        settings.trainer_name_language,
        TrainerNameLanguage::FollowUi
    );
    results_where(|m| m.display_name == "エルデンリング").await;
    let ini = std::fs::read_to_string(h.paths.settings_file()).unwrap();
    assert!(ini.contains("trainerNameLanguage=auto"), "{ini}");
}

#[tokio::test]
async fn late_detail_reply_for_previous_selection_is_ignored() {
    let client = GatedClient::default();
    client
        .inner
        .page(BASE, homepage(&[("alpha", "2"), ("beta", "1")]));
    client.inner.page(
        &format!("{BASE}alpha/"),
        detail_page("https://fling.test/downloads/alpha"),
    );
    client.inner.page(
        &format!("{BASE}beta/"),
        detail_page("https://fling.test/downloads/beta"),
    );
    let alpha_gate = client.gate(&format!("{BASE}alpha/"));
    let h = Harness::start(Arc::new(client), true);
    h.next(|e| {
        if let Event::Results(r) = e {
            (r.len() == 2).then_some(())
        } else {
            None
        }
    })
    .await;

    h.handle.send(Command::Select(0)); // alpha: detail held at the gate
    h.handle.send(Command::Select(1)); // beta
    let beta = h.selection_where(|s| s.detail == DetailState::Ready).await;
    assert_eq!(
        beta.modifier.versions[0].url,
        "https://fling.test/downloads/beta"
    );

    alpha_gate.notify_one();
    tokio::time::sleep(Duration::from_millis(100)).await;
    while let Ok(event) = h.events.try_recv() {
        if let Event::Selection(s) = event {
            assert_eq!(
                s.modifier.name, "Beta Trainer",
                "stale alpha detail leaked into the selection"
            );
        }
    }
}

#[tokio::test]
async fn search_translates_and_respects_sort_order() {
    let fake = FakeHttpClient::new();
    fake.page(
        &format!("{BASE}?s=Elden+Ring"),
        r#"<html>SEARCH RESULTS
<article class="post"><h2 class="post-title"><a href="https://fling.test/b-trainer/">Elden Ring Nightreign Trainer</a></h2><p>Game Version: v1</p></article>
<article class="post"><h2 class="post-title"><a href="https://fling.test/a-trainer/">Elden Ring Trainer</a></h2></article></html>"#,
    );
    fake.page("https://fling.test/b-trainer/", "<p>30 Options</p>");
    fake.page("https://fling.test/a-trainer/", "<p>5 Options</p>");
    let h = Harness::start(Arc::new(fake.clone()), false);

    h.handle.send(Command::SetSort(SortOrder::OptionsCount));
    h.handle.send(Command::Search("艾尔登法环".into()));
    let results = h
        .next(|e| {
            if let Event::Results(r) = e {
                (!r.is_empty()).then_some(r)
            } else {
                None
            }
        })
        .await;
    let counts: Vec<_> = results.iter().map(|m| m.options_count).collect();
    assert_eq!(counts, [30, 5]);
    assert!(
        fake.requested_gets()
            .contains(&format!("{BASE}?s=Elden+Ring"))
    );

    h.handle.send(Command::SetSort(SortOrder::Name));
    let results = h
        .next(|e| {
            if let Event::Results(r) = e {
                Some(r)
            } else {
                None
            }
        })
        .await;
    assert_eq!(results[0].name, "Elden Ring Nightreign Trainer");
}

#[tokio::test]
async fn database_update_installs_and_reloads_suggestions() {
    let dir = tempfile::tempdir().unwrap();
    let new_db = dir.path().join("new.db");
    create_database(
        &new_db,
        "v999.0.0",
        "1",
        &[("Brand New Game", "brand new game", "全新游戏", "")],
    );
    let fake = FakeHttpClient::new();
    fake.page(
        DB_RELEASE_API,
        r#"{"tag_name":"v999.0.0","assets":[{"name":"fling_translations.db","browser_download_url":"https://dl.test/db"}]}"#,
    );
    fake.file("https://dl.test/db", std::fs::read(&new_db).unwrap());
    let h = Harness::start(Arc::new(fake), false);
    assert!(h.handle.suggestions("全新", 8).is_empty());

    h.handle.send(Command::CheckDatabaseUpdate);
    let state = h
        .next(|e| match e {
            Event::DatabaseUpdate(s) if s.status == UpdateStatus::Available => Some(s),
            _ => None,
        })
        .await;
    assert_eq!(state.latest_version, "999.0.0");
    assert_eq!(state.current_version, "0.0.1");

    h.handle.send(Command::DownloadDatabaseUpdate);
    let state = h
        .next(|e| match e {
            Event::DatabaseUpdate(s) if s.status == UpdateStatus::Installed => Some(s),
            _ => None,
        })
        .await;
    assert_eq!(state.current_version, "999.0.0");
    assert!(!state.available);
    assert!(h.paths.database_override().exists());
    assert_eq!(
        h.handle.suggestions("全新", 8)[0].search_keyword,
        "Brand New Game"
    );
}

#[tokio::test]
async fn app_update_check_failure_and_settings_persist() {
    let fake = FakeHttpClient::new();
    let h = Harness::start(Arc::new(fake), false);

    h.handle.send(Command::CheckAppUpdate);
    let state = h
        .next(|e| match e {
            Event::AppUpdate(s) if matches!(s.status, UpdateStatus::CheckFailed(_)) => Some(s),
            _ => None,
        })
        .await;
    assert!(!state.checking);
    assert_eq!(
        state.status,
        UpdateStatus::CheckFailed(UpdateError::RequestFailed(
            fling_update::ReleaseKind::App,
            UpdateSource::GitHub
        ))
    );

    h.handle.send(Command::SetTheme(4));
    h.handle.send(Command::SetLanguage(Language::Japanese));
    let settings = h
        .next(|e| match e {
            Event::Settings(s) if s.language == Language::Japanese => Some(s),
            _ => None,
        })
        .await;
    assert_eq!(settings.theme, 4);
    let ini = std::fs::read_to_string(h.paths.settings_file()).unwrap();
    assert!(
        ini.contains("currentTheme=4") && ini.contains("currentLanguage=2"),
        "{ini}"
    );
}
