//! The contract between the backend and any frontend: plain data only.

use std::path::PathBuf;

pub use fling_core::{
    DownloadTask, DownloadedModifier, Language, ModifierInfo, TaskId, TaskStatus,
    TrainerNameLanguage, UpdateSource,
};
pub use fling_mapping::Suggestion;
pub use fling_update::UpdateError;

/// Order of the search-result table. Kept across result sets.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum SortOrder {
    /// `last_update` descending (string order, as the Qt build).
    #[default]
    RecentlyUpdated,
    /// Name ascending, case-insensitive.
    Name,
    /// Options count descending.
    OptionsCount,
}

/// What "open folder" refers to.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum FolderTarget {
    /// The configured download directory.
    Downloads,
    /// The folder of a download task's file.
    Task(TaskId),
    /// The folder of a library entry's file.
    LibraryItem(usize),
}

/// Everything a frontend can ask for.
#[derive(Debug, Clone, PartialEq)]
pub enum Command {
    /// Search the site. An empty string loads the homepage's featured list.
    Search(String),
    /// Reload the homepage "recently updated" list.
    FetchRecent,
    SetSort(SortOrder),

    /// Select row `n` of the latest [`Event::Results`] and load its details.
    Select(usize),
    RetryDetail,

    /// Queue version `version_index` of the selected trainer.
    Download {
        version_index: usize,
    },
    PauseTask(TaskId),
    ResumeTask(TaskId),
    CancelTask(TaskId),
    RemoveTask(TaskId),

    /// Open library entry `n`'s file (dropping it if the file is gone).
    RunLibraryItem(usize),
    DeleteLibraryItem(usize),
    OpenFolder(FolderTarget),

    CheckAppUpdate,
    DownloadAppUpdate,
    CheckDatabaseUpdate,
    DownloadDatabaseUpdate,

    SetTheme(usize),
    SetLanguage(Language),
    /// Language of trainer names in every list and of new download files.
    SetTrainerNameLanguage(TrainerNameLanguage),
    SetUpdateSource(UpdateSource),
    SetAutoCheckAppUpdates(bool),
    SetAutoCheckDatabaseUpdates(bool),
    SetDownloadDirectory(PathBuf),
}

/// Loading state of the selected trainer's detail page.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub enum DetailState {
    #[default]
    Idle,
    Loading,
    /// Loaded, with at least one download.
    Ready,
    /// Loaded (or nothing to load) but no downloads.
    Empty,
    Error,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub enum CoverState {
    Loading,
    Ready(PathBuf),
    /// No cover to show (none found, or extraction failed).
    #[default]
    Missing,
}

/// The selected trainer: the list row merged with its detail page.
#[derive(Debug, Clone, Default, PartialEq)]
pub struct Selection {
    pub index: usize,
    pub modifier: ModifierInfo,
    pub detail: DetailState,
    pub cover: CoverState,
}

/// Where an update flow stands; the UI turns this into text.
#[derive(Debug, Clone, Default, PartialEq)]
pub enum UpdateStatus {
    #[default]
    Idle,
    Checking,
    UpToDate,
    /// A newer version exists.
    Available,
    CheckFailed(UpdateError),
    Downloading,
    DownloadFailed(String),
    /// App only: the installer is starting and the app is about to quit.
    LaunchingInstaller,
    LaunchFailed,
    /// Database only: the new database is active.
    Installed,
    InstallFailed(String),
    /// Database only: installed but the mappings could not be reloaded.
    ReloadFailed,
}

#[derive(Debug, Clone, Default, PartialEq)]
pub struct UpdateState {
    pub checking: bool,
    pub downloading: bool,
    pub available: bool,
    /// The running app version, or the active database's release tag.
    pub current_version: String,
    pub latest_version: String,
    pub source: Option<UpdateSource>,
    pub published_at: String,
    /// `0.0..=1.0` while downloading.
    pub progress: f32,
    pub status: UpdateStatus,
}

#[derive(Debug, Clone, PartialEq)]
pub struct SettingsSnapshot {
    pub theme: usize,
    pub language: Language,
    pub trainer_name_language: TrainerNameLanguage,
    pub update_source: UpdateSource,
    pub auto_check_app_updates: bool,
    pub auto_check_database_updates: bool,
    pub download_directory: PathBuf,
    pub app_version: String,
}

/// Everything the backend tells a frontend. Each variant carries the full
/// current value of its piece of state, so a frontend can simply replace it.
#[derive(Debug, Clone, PartialEq)]
pub enum Event {
    SearchLoading(bool),
    Results(Vec<ModifierInfo>),
    Selection(Selection),
    Tasks(Vec<DownloadTask>),
    Library(Vec<DownloadedModifier>),
    /// A library file exists but could not be deleted.
    DeleteFailed {
        name: String,
        file_path: String,
    },
    AppUpdate(UpdateState),
    DatabaseUpdate(UpdateState),
    Settings(SettingsSnapshot),
    /// Open this file or folder with the OS.
    Open(PathBuf),
    /// The installer is running; the frontend should exit.
    Quit,
}
