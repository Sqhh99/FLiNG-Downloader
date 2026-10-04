//! Typed access to `settings.ini`. Port of the live parts of `ConfigManager`.

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use fling_core::{Language, UpdateSource};

use crate::AppPaths;
use crate::qsettings::{GENERAL, IniDocument};

/// Number of built-in themes (indices `0..THEME_COUNT`).
pub const THEME_COUNT: i64 = 9;

const KEY_DOWNLOAD_DIRECTORY: &str = "downloadDirectory";
const KEY_AUTO_CHECK_APP: &str = "autoCheckUpdates";
const KEY_AUTO_CHECK_DB: &str = "autoCheckDatabaseUpdates";
const KEY_UPDATE_SOURCE: &str = "updateSource";
const KEY_THEME: &str = "currentTheme";
const KEY_LANGUAGE: &str = "currentLanguage";

/// User settings backed by `settings.ini`. Every setter writes the file
/// immediately, like the Qt build's `QSettings::sync()` calls.
#[derive(Debug)]
pub struct Settings {
    file: PathBuf,
    default_download_dir: PathBuf,
    doc: IniDocument,
}

impl Settings {
    /// Loads the settings file; a missing or unreadable file means defaults.
    pub fn load(paths: &AppPaths) -> Self {
        let file = paths.settings_file();
        let doc = match fs::read_to_string(&file) {
            Ok(text) => IniDocument::parse(&text),
            Err(err) if err.kind() == io::ErrorKind::NotFound => IniDocument::default(),
            Err(err) => {
                tracing::warn!(?file, %err, "failed to read settings; using defaults");
                IniDocument::default()
            }
        };
        Self {
            file,
            default_download_dir: paths.downloads.clone(),
            doc,
        }
    }

    pub fn file(&self) -> &Path {
        &self.file
    }

    fn save(&self) -> io::Result<()> {
        if let Some(dir) = self.file.parent() {
            fs::create_dir_all(dir)?;
        }
        let temp = self.file.with_extension("ini.tmp");
        fs::write(&temp, self.doc.to_ini_string())?;
        fs::rename(&temp, &self.file)
    }

    fn save_logged(&self) {
        if let Err(err) = self.save() {
            tracing::warn!(file = ?self.file, %err, "failed to save settings");
        }
    }

    pub fn download_directory(&self) -> PathBuf {
        self.doc
            .get_string(GENERAL, KEY_DOWNLOAD_DIRECTORY)
            .filter(|s| !s.is_empty())
            .map(PathBuf::from)
            .unwrap_or_else(|| self.default_download_dir.clone())
    }

    /// Creates the directory first and refuses to store a path that cannot be created.
    pub fn set_download_directory(&mut self, dir: &Path) -> io::Result<()> {
        fs::create_dir_all(dir)?;
        // Qt stored paths with forward slashes; keep doing so for downgrades.
        let value = dir.to_string_lossy().replace('\\', "/");
        self.doc.set_string(GENERAL, KEY_DOWNLOAD_DIRECTORY, &value);
        self.save()
    }

    pub fn auto_check_app_updates(&self) -> bool {
        self.doc
            .get_bool(GENERAL, KEY_AUTO_CHECK_APP)
            .unwrap_or(true)
    }

    pub fn set_auto_check_app_updates(&mut self, enabled: bool) {
        self.doc.set_bool(GENERAL, KEY_AUTO_CHECK_APP, enabled);
        self.save_logged();
    }

    pub fn auto_check_database_updates(&self) -> bool {
        self.doc
            .get_bool(GENERAL, KEY_AUTO_CHECK_DB)
            .unwrap_or(true)
    }

    pub fn set_auto_check_database_updates(&mut self, enabled: bool) {
        self.doc.set_bool(GENERAL, KEY_AUTO_CHECK_DB, enabled);
        self.save_logged();
    }

    pub fn update_source(&self) -> UpdateSource {
        self.doc
            .get_string(GENERAL, KEY_UPDATE_SOURCE)
            .map(|s| UpdateSource::from_key(&s))
            .unwrap_or_default()
    }

    pub fn set_update_source(&mut self, source: UpdateSource) {
        self.doc
            .set_string(GENERAL, KEY_UPDATE_SOURCE, source.key());
        self.save_logged();
    }

    /// Theme index in `0..THEME_COUNT`; out-of-range values read as 0 (Light).
    pub fn theme(&self) -> usize {
        let index = self.doc.get_int(GENERAL, KEY_THEME).unwrap_or(0);
        if (0..THEME_COUNT).contains(&index) {
            index as usize
        } else {
            0
        }
    }

    pub fn set_theme(&mut self, index: usize) {
        let index = if (index as i64) < THEME_COUNT {
            index as i64
        } else {
            0
        };
        self.doc.set_int(GENERAL, KEY_THEME, index);
        self.save_logged();
    }

    pub fn language(&self) -> Language {
        Language::from_index(self.doc.get_int(GENERAL, KEY_LANGUAGE).unwrap_or(0))
    }

    pub fn set_language(&mut self, language: Language) {
        self.doc.set_int(GENERAL, KEY_LANGUAGE, language.index());
        self.save_logged();
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn paths() -> (tempfile::TempDir, AppPaths) {
        let dir = tempfile::tempdir().unwrap();
        let paths = AppPaths::rooted(dir.path());
        (dir, paths)
    }

    #[test]
    fn defaults_without_file() {
        let (_dir, paths) = paths();
        let settings = Settings::load(&paths);
        assert_eq!(settings.download_directory(), paths.downloads);
        assert!(settings.auto_check_app_updates());
        assert!(settings.auto_check_database_updates());
        assert_eq!(settings.update_source(), UpdateSource::GitHub);
        assert_eq!(settings.theme(), 0);
        assert_eq!(settings.language(), Language::Chinese);
    }

    #[test]
    fn reads_existing_qt_file_and_keeps_foreign_keys() {
        let (_dir, paths) = paths();
        fs::create_dir_all(paths.config_dir()).unwrap();
        fs::write(
            paths.settings_file(),
            "[General]\ncurrentTheme=3\ncurrentLanguage=2\nupdateSource=gitee\nautoCheckUpdates=false\n\
             userAgent=custom\n\n[SearchManager]\nSearchHistory=a, b\n",
        )
        .unwrap();

        let mut settings = Settings::load(&paths);
        assert_eq!(settings.theme(), 3);
        assert_eq!(settings.language(), Language::Japanese);
        assert_eq!(settings.update_source(), UpdateSource::Gitee);
        assert!(!settings.auto_check_app_updates());

        settings.set_theme(7);
        let text = fs::read_to_string(paths.settings_file()).unwrap();
        assert!(text.contains("currentTheme=7"));
        assert!(text.contains("userAgent=custom"));
        assert!(text.contains("SearchHistory=a, b"));
        assert_eq!(Settings::load(&paths).theme(), 7);
    }

    #[test]
    fn out_of_range_values_fall_back() {
        let (_dir, paths) = paths();
        fs::create_dir_all(paths.config_dir()).unwrap();
        fs::write(
            paths.settings_file(),
            "[General]\ncurrentTheme=42\ncurrentLanguage=9\nupdateSource=Gitee\n",
        )
        .unwrap();
        let settings = Settings::load(&paths);
        assert_eq!(settings.theme(), 0);
        assert_eq!(settings.language(), Language::Chinese);
        assert_eq!(settings.update_source(), UpdateSource::GitHub);
    }

    #[test]
    fn download_directory_is_created_and_persisted() {
        let (dir, paths) = paths();
        let mut settings = Settings::load(&paths);
        let target = dir.path().join("my, trainers");
        settings.set_download_directory(&target).unwrap();
        assert!(target.is_dir());
        let reloaded = Settings::load(&paths).download_directory();
        assert_eq!(
            reloaded.to_string_lossy().replace('\\', "/"),
            target.to_string_lossy().replace('\\', "/")
        );
    }
}
