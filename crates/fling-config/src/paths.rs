//! File-system locations. Mirrors Qt's `QStandardPaths` results for an app
//! named "FLiNG Downloader" with an empty organization name.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

const APP_DIR_NAME: &str = "FLiNG Downloader";

/// Every directory the app reads or writes. Construct with [`AppPaths::system`]
/// in the app, or [`AppPaths::rooted`] in tests.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct AppPaths {
    /// `%APPDATA%\FLiNG Downloader` (roaming).
    pub app_data: PathBuf,
    /// `%LOCALAPPDATA%\FLiNG Downloader\cache`.
    pub cache: PathBuf,
    /// The user's Downloads folder; the default download directory.
    pub downloads: PathBuf,
    /// Directory of the running executable, used to find bundled resources.
    pub exe_dir: PathBuf,
}

impl AppPaths {
    /// The real per-user locations.
    pub fn system() -> Self {
        let home = dirs::home_dir().unwrap_or_else(|| PathBuf::from("."));
        let roaming = dirs::config_dir().unwrap_or_else(|| home.join("AppData/Roaming"));
        let local = dirs::cache_dir().unwrap_or_else(|| home.join("AppData/Local"));
        let exe_dir = std::env::current_exe()
            .ok()
            .and_then(|p| p.parent().map(Path::to_path_buf))
            .unwrap_or_else(|| PathBuf::from("."));
        Self {
            app_data: roaming.join(APP_DIR_NAME),
            cache: local.join(APP_DIR_NAME).join("cache"),
            downloads: dirs::download_dir().unwrap_or_else(|| home.join("Downloads")),
            exe_dir,
        }
    }

    /// Everything under one root directory; for tests.
    pub fn rooted(root: &Path) -> Self {
        Self {
            app_data: root.join("appdata"),
            cache: root.join("cache"),
            downloads: root.join("downloads"),
            exe_dir: root.join("exe"),
        }
    }

    pub fn config_dir(&self) -> PathBuf {
        self.app_data.join("config")
    }

    /// Persistent business data: library, recent-list cache, database override.
    pub fn data_dir(&self) -> PathBuf {
        self.app_data.join("data")
    }

    pub fn settings_file(&self) -> PathBuf {
        self.config_dir().join("settings.ini")
    }

    pub fn library_file(&self) -> PathBuf {
        self.data_dir().join("downloaded_modifiers.json")
    }

    pub fn recent_cache_file(&self) -> PathBuf {
        self.data_dir().join("recent_modifiers_cache.json")
    }

    pub fn database_override(&self) -> PathBuf {
        self.data_dir().join("fling_translations.db")
    }

    pub fn covers_dir(&self) -> PathBuf {
        self.cache.join("covers")
    }

    pub fn app_update_dir(&self) -> PathBuf {
        self.cache.join("updates")
    }

    pub fn database_update_dir(&self) -> PathBuf {
        self.cache.join("db-updates")
    }

    /// Creates the config, data and cache directories.
    pub fn ensure_dirs(&self) -> io::Result<()> {
        for dir in [self.config_dir(), self.data_dir(), self.cache.clone()] {
            fs::create_dir_all(dir)?;
        }
        Ok(())
    }

    /// Finds a file shipped next to the app: tries the exe directory, then
    /// `..` and `../..` (so it also works from `target/debug`).
    pub fn bundled_resource(&self, relative: &str) -> Option<PathBuf> {
        let mut root = Some(self.exe_dir.as_path());
        for _ in 0..3 {
            let dir = root?;
            let candidate = dir.join(relative);
            if candidate.is_file() {
                return Some(candidate);
            }
            root = dir.parent();
        }
        None
    }

    pub fn bundled_database(&self) -> Option<PathBuf> {
        self.bundled_resource("resources/fling_translations.db")
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout_matches_qt_build() {
        let paths = AppPaths::rooted(Path::new("/r"));
        assert_eq!(
            paths.settings_file(),
            Path::new("/r/appdata/config/settings.ini")
        );
        assert_eq!(
            paths.library_file(),
            Path::new("/r/appdata/data/downloaded_modifiers.json")
        );
        assert_eq!(
            paths.database_override(),
            Path::new("/r/appdata/data/fling_translations.db")
        );
        assert_eq!(paths.covers_dir(), Path::new("/r/cache/covers"));
    }

    #[test]
    fn bundled_resource_searches_up_and_ignores_directories() {
        let dir = tempfile::tempdir().unwrap();
        let mut paths = AppPaths::rooted(dir.path());
        paths.exe_dir = dir.path().join("a/b/exe");
        fs::create_dir_all(&paths.exe_dir).unwrap();

        // A directory with the resource's name must not count.
        fs::create_dir_all(paths.exe_dir.join("resources/fling_translations.db")).unwrap();
        assert_eq!(paths.bundled_database(), None);

        let file = dir.path().join("a/resources/fling_translations.db");
        fs::create_dir_all(file.parent().unwrap()).unwrap();
        fs::write(&file, b"x").unwrap();
        assert_eq!(paths.bundled_database(), Some(file));
    }
}
