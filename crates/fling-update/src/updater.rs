//! Release checks and asset downloads over an [`HttpClient`].

use std::path::{Path, PathBuf};
use std::sync::Arc;

use fling_core::UpdateSource;
use fling_core::version::{compare_versions, normalize_version};
use fling_net::{CancellationToken, DownloadError, DownloadRequest, HttpClient, ProgressFn};

use crate::ReleaseKind;
use crate::release::{ReleaseInfo, parse_app_release, parse_database_release};

/// Why a check or download failed. The `Display` strings are the Qt build's
/// English messages; the UI shows localized text instead.
#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum UpdateError {
    #[error("{} {}release request failed", source_name(*.1), kind_word(*.0))]
    RequestFailed(ReleaseKind, UpdateSource),
    #[error("{} {}release response did not contain a valid {} asset", source_name(*.1), kind_word(*.0), asset_word(*.0))]
    NoValidAsset(ReleaseKind, UpdateSource),
    #[error("Update information is incomplete")]
    Incomplete,
    #[error("Failed to create update cache directory")]
    CreateDirectory,
    #[error(transparent)]
    Download(#[from] DownloadError),
}

fn source_name(source: UpdateSource) -> &'static str {
    match source {
        UpdateSource::GitHub => "GitHub",
        UpdateSource::Gitee => "Gitee",
    }
}

fn kind_word(kind: ReleaseKind) -> &'static str {
    match kind {
        ReleaseKind::App => "",
        ReleaseKind::Database => "database ",
    }
}

fn asset_word(kind: ReleaseKind) -> &'static str {
    match kind {
        ReleaseKind::App => "installer",
        ReleaseKind::Database => "database",
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CheckResult {
    pub release: ReleaseInfo,
    /// `current < release.version`.
    pub update_available: bool,
}

pub struct Updater {
    http: Arc<dyn HttpClient>,
    app_update_dir: PathBuf,
    database_update_dir: PathBuf,
}

impl Updater {
    pub fn new(
        http: Arc<dyn HttpClient>,
        app_update_dir: PathBuf,
        database_update_dir: PathBuf,
    ) -> Self {
        Self {
            http,
            app_update_dir,
            database_update_dir,
        }
    }

    /// Checks `source` only, never the other one.
    pub async fn check(
        &self,
        kind: ReleaseKind,
        current_version: &str,
        source: UpdateSource,
    ) -> Result<CheckResult, UpdateError> {
        let body = self
            .http
            .get(kind.latest_release_url(source))
            .await
            .map_err(|_| UpdateError::RequestFailed(kind, source))?;
        let release = match kind {
            ReleaseKind::App => parse_app_release(&body, source),
            ReleaseKind::Database => parse_database_release(&body, source),
        };
        if !release.is_valid() {
            return Err(UpdateError::NoValidAsset(kind, source));
        }
        let update_available = compare_versions(current_version, &release.version).is_lt();
        Ok(CheckResult {
            release,
            update_available,
        })
    }

    /// Downloads the release's asset into the cache: the installer to
    /// `<cache>/updates/<asset name>`, the database to
    /// `<cache>/db-updates/fling_translations-<version>.db`.
    pub async fn download(
        &self,
        kind: ReleaseKind,
        release: &ReleaseInfo,
        progress: Option<ProgressFn>,
    ) -> Result<PathBuf, UpdateError> {
        if !release.is_valid() {
            return Err(UpdateError::Incomplete);
        }
        let (dir, file_name) = match kind {
            ReleaseKind::App => (&self.app_update_dir, release.asset_name.clone()),
            ReleaseKind::Database => {
                let token = normalize_version(&release.version).replace(['/', '\\'], "_");
                (
                    &self.database_update_dir,
                    format!("fling_translations-{token}.db"),
                )
            }
        };
        // The asset name comes from the server; never let it leave the cache dir.
        let file_name = Path::new(&file_name)
            .file_name()
            .map(|n| n.to_owned())
            .ok_or(UpdateError::Incomplete)?;
        std::fs::create_dir_all(dir).map_err(|_| UpdateError::CreateDirectory)?;
        let dest = dir.join(file_name);
        self.http
            .download(DownloadRequest {
                url: release.download_url.clone(),
                dest: dest.clone(),
                resume_from: 0,
                keep_partial: false,
                cancel: CancellationToken::new(),
                progress,
            })
            .await?;
        Ok(dest)
    }
}

/// Starts the installer detached (it is interactive; the app should quit
/// right after).
pub fn launch_installer(path: &Path) -> std::io::Result<()> {
    std::process::Command::new(path).spawn().map(|_| ())
}

#[cfg(test)]
mod tests {
    use fling_net::GetError;
    use fling_net::fake::FakeHttpClient;

    use super::*;

    fn updater(fake: &FakeHttpClient, dir: &Path) -> Updater {
        Updater::new(
            Arc::new(fake.clone()),
            dir.join("updates"),
            dir.join("db-updates"),
        )
    }

    const APP_RELEASE: &str = r#"{"tag_name":"v1.2.0","assets":[
        {"name":"FLiNG-Downloader-v1.2.0-win-x64-setup.exe","browser_download_url":"https://x/setup.exe"}]}"#;

    #[tokio::test]
    async fn github_app_check_reports_update() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page(
            ReleaseKind::App.latest_release_url(UpdateSource::GitHub),
            APP_RELEASE,
        );
        let result = updater(&fake, dir.path())
            .check(ReleaseKind::App, "1.1.0", UpdateSource::GitHub)
            .await
            .unwrap();
        assert!(result.update_available);
        assert_eq!(result.release.version, "1.2.0");
        assert_eq!(
            result.release.asset_name,
            "FLiNG-Downloader-v1.2.0-win-x64-setup.exe"
        );

        let same = updater(&fake, dir.path())
            .check(ReleaseKind::App, "1.2.0", UpdateSource::GitHub)
            .await;
        assert!(!same.unwrap().update_available);
    }

    #[tokio::test]
    async fn github_error_has_no_gitee_fallback() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page_error(
            ReleaseKind::App.latest_release_url(UpdateSource::GitHub),
            GetError::TimedOut,
        );
        fake.page(
            ReleaseKind::App.latest_release_url(UpdateSource::Gitee),
            APP_RELEASE,
        );
        let err = updater(&fake, dir.path())
            .check(ReleaseKind::App, "1.1.0", UpdateSource::GitHub)
            .await
            .unwrap_err();
        assert_eq!(err.to_string(), "GitHub release request failed");
        assert_eq!(fake.requested_gets().len(), 1);

        let err = updater(&fake, dir.path())
            .check(ReleaseKind::Database, "1.0.0", UpdateSource::GitHub)
            .await
            .unwrap_err();
        assert_eq!(err.to_string(), "GitHub database release request failed");
    }

    #[tokio::test]
    async fn gitee_database_check_uses_gitee_directly() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page(
            ReleaseKind::Database.latest_release_url(UpdateSource::Gitee),
            r#"{"tag_name":"v1.1.0","assets":[{"name":"fling_translations.db","download_url":"https://gitee.com/db"}]}"#,
        );
        let result = updater(&fake, dir.path())
            .check(ReleaseKind::Database, "1.0.0", UpdateSource::Gitee)
            .await
            .unwrap();
        assert!(result.update_available);
        assert_eq!(result.release.download_url, "https://gitee.com/db");
        assert!(
            fake.requested_gets()
                .iter()
                .all(|u| u.contains("gitee.com"))
        );
    }

    #[tokio::test]
    async fn invalid_asset_and_downloads() {
        let dir = tempfile::tempdir().unwrap();
        let fake = FakeHttpClient::new();
        fake.page(
            ReleaseKind::Database.latest_release_url(UpdateSource::GitHub),
            r#"{"tag_name":"v1"}"#,
        );
        let err = updater(&fake, dir.path())
            .check(ReleaseKind::Database, "1.0.0", UpdateSource::GitHub)
            .await
            .unwrap_err();
        assert_eq!(
            err.to_string(),
            "GitHub database release response did not contain a valid database asset"
        );

        let release = ReleaseInfo {
            version: "v0.0.4/x".into(),
            asset_name: "fling_translations.db".into(),
            download_url: "https://x/db".into(),
            ..ReleaseInfo::default()
        };
        fake.file("https://x/db", b"SQLite".to_vec());
        let path = updater(&fake, dir.path())
            .download(ReleaseKind::Database, &release, None)
            .await
            .unwrap();
        assert_eq!(
            path,
            dir.path().join("db-updates/fling_translations-0.0.4_x.db")
        );

        let installer = ReleaseInfo {
            asset_name: "../evil.exe".into(),
            download_url: "https://x/db".into(),
            ..release
        };
        let path = updater(&fake, dir.path())
            .download(ReleaseKind::App, &installer, None)
            .await
            .unwrap();
        assert_eq!(path, dir.path().join("updates/evil.exe"));
    }
}
