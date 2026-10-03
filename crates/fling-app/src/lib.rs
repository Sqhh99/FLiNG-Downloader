//! The backend facade. A frontend calls [`start`], sends [`Command`]s through
//! the returned [`BackendHandle`] and renders the [`Event`]s it receives.
//! Nothing here knows which UI toolkit is on the other end.
//!
//! The backend runs on its own thread with its own tokio runtime; one task
//! owns all state and handles commands and finished background work in order,
//! so there are no locks around application state.

mod api;
mod backend;

use std::sync::Arc;
use std::time::Duration;

pub use api::*;
/// The receiving end of the backend's event stream.
pub type EventReceiver = async_channel::Receiver<Event>;
use fling_config::AppPaths;
use fling_cover::DetectorLoader;
use fling_mapping::SuggestionIndex;
use fling_net::HttpClient;
use parking_lot::RwLock;
use tokio::sync::mpsc;

use backend::{Backend, Msg};

/// Delays of the work the Qt build ran shortly after startup.
#[derive(Debug, Clone)]
pub struct StartupTimings {
    pub cover_warm_up: Duration,
    pub app_update_check: Duration,
    pub database_update_check: Duration,
}

impl Default for StartupTimings {
    fn default() -> Self {
        Self {
            cover_warm_up: Duration::from_millis(1000),
            app_update_check: Duration::from_millis(1500),
            database_update_check: Duration::from_millis(2200),
        }
    }
}

pub struct BackendConfig {
    pub paths: AppPaths,
    /// The running app's version (`FLING_APP_VERSION`).
    pub app_version: String,
    pub http: Arc<dyn HttpClient>,
    /// flingtrainer.com, or a test server.
    pub site_base_url: String,
    /// Loads the cover model on first use.
    pub cover_detector: DetectorLoader,
    /// `None` skips the startup fetch, warm-up and automatic update checks.
    pub startup: Option<StartupTimings>,
}

impl BackendConfig {
    /// The real app: system paths, real network, flingtrainer.com and the
    /// bundled cover model.
    pub fn for_app(app_version: String) -> Result<Self, String> {
        let http = fling_net::ReqwestClient::new().map_err(|e| e.to_string())?;
        Ok(Self {
            paths: AppPaths::system(),
            app_version,
            http: Arc::new(http),
            site_base_url: fling_site::DEFAULT_BASE_URL.to_owned(),
            cover_detector: bundled_cover_detector(),
            startup: Some(StartupTimings::default()),
        })
    }
}

/// State the handle answers synchronously, without a round trip.
struct SharedLookup {
    suggestions: RwLock<Arc<SuggestionIndex>>,
    language: RwLock<Language>,
}

/// Cheap to clone. Dropping every handle stops the backend.
#[derive(Clone)]
pub struct BackendHandle {
    tx: mpsc::UnboundedSender<Msg>,
    lookup: Arc<SharedLookup>,
    initial_settings: Arc<SettingsSnapshot>,
}

impl BackendHandle {
    pub fn send(&self, command: Command) {
        if self.tx.send(Msg::Command(command)).is_err() {
            tracing::warn!("backend has stopped; command dropped");
        }
    }

    /// Search-box suggestions for `keyword`, worded for the current UI
    /// language. Synchronous: cheap enough to call on every keystroke.
    pub fn suggestions(&self, keyword: &str, max: usize) -> Vec<Suggestion> {
        let language = *self.lookup.language.read();
        self.lookup
            .suggestions
            .read()
            .suggest(keyword, max, language)
    }

    /// Settings as loaded at startup, for styling the first frame.
    pub fn initial_settings(&self) -> &SettingsSnapshot {
        &self.initial_settings
    }
}

/// Loads settings, the translation database and the library, then starts the
/// backend thread. Events arrive on the returned receiver; the first ones
/// describe the initial state.
pub fn start(
    config: BackendConfig,
) -> std::io::Result<(BackendHandle, async_channel::Receiver<Event>)> {
    let (tx, rx) = mpsc::unbounded_channel();
    let (events, event_rx) = async_channel::unbounded();
    let runtime = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .thread_name("fling-backend")
        .build()?;

    let lookup = Arc::new(SharedLookup {
        suggestions: RwLock::new(Arc::default()),
        language: RwLock::new(Language::default()),
    });
    let backend = Backend::new(
        config,
        runtime.handle().clone(),
        tx.downgrade(),
        events,
        lookup.clone(),
    );
    let initial_settings = Arc::new(backend.settings_snapshot());

    std::thread::Builder::new()
        .name("fling-backend".into())
        .spawn(move || {
            runtime.block_on(backend.run(rx));
            // Let in-flight transfers and file writes wind down briefly.
            runtime.shutdown_timeout(Duration::from_secs(2));
        })?;

    Ok((
        BackendHandle {
            tx,
            lookup,
            initial_settings,
        },
        event_rx,
    ))
}

/// Loads `models/game-cover-v2.onnx` from next to the executable (the
/// release layout) or `resources/models/` (a source checkout) on first use.
pub fn bundled_cover_detector() -> DetectorLoader {
    Arc::new(|| {
        let paths = AppPaths::system();
        let model = paths
            .bundled_resource(&format!("models/{}.onnx", fling_cover::MODEL_NAME))
            .or_else(|| {
                paths.bundled_resource(&format!(
                    "resources/models/{}.onnx",
                    fling_cover::MODEL_NAME
                ))
            });
        let Some(model) = model else {
            tracing::warn!("cover model not found; covers are disabled");
            return None;
        };
        match fling_cover::OnnxCoverDetector::load(&model) {
            Ok(detector) => Some(Arc::new(detector) as Arc<dyn fling_cover::CoverDetector>),
            Err(err) => {
                tracing::warn!(?model, %err, "failed to load cover model");
                None
            }
        }
    })
}
