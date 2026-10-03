//! Release checks and downloads. Port of `AppUpdateManager` and
//! `DatabaseUpdateManager`.
//!
//! The configured [`UpdateSource`] is used strictly: a GitHub failure is
//! reported, never retried against Gitee (and vice versa).

mod release;
mod updater;

pub use release::{ReleaseInfo, installer_asset_name, parse_app_release, parse_database_release};
pub use updater::{CheckResult, UpdateError, Updater, launch_installer};

pub use fling_core::UpdateSource;

/// What a release check is for.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ReleaseKind {
    App,
    Database,
}

impl ReleaseKind {
    /// The `releases/latest` API endpoint for this kind and source.
    pub fn latest_release_url(self, source: UpdateSource) -> &'static str {
        match (self, source) {
            (Self::App, UpdateSource::GitHub) => {
                "https://api.github.com/repos/Sqhh99/FLiNG-Downloader/releases/latest"
            }
            (Self::App, UpdateSource::Gitee) => {
                "https://gitee.com/api/v5/repos/sqhh99/fli-ng-downloader/releases/latest"
            }
            (Self::Database, UpdateSource::GitHub) => {
                "https://api.github.com/repos/Sqhh99/game-mappings-updater/releases/latest"
            }
            (Self::Database, UpdateSource::Gitee) => {
                "https://gitee.com/api/v5/repos/sqhh99/game-mappings-updater/releases/latest"
            }
        }
    }
}
