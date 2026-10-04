//! The downloaded-trainer library, persisted as `downloaded_modifiers.json`
//! (a pretty-printed JSON array, same format as the Qt build).

use std::fs;
use std::io;
use std::path::{Path, PathBuf};

use fling_core::DownloadedModifier;

/// What [`Library::run_target`] found.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum RunTarget {
    /// Open this file with the OS.
    Open(PathBuf),
    /// The file was gone; its record has been dropped.
    Missing,
}

/// The file exists but could not be deleted; the record is kept.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DeleteError {
    pub name: String,
    pub file_path: String,
}

#[derive(Debug)]
pub struct Library {
    path: PathBuf,
    items: Vec<DownloadedModifier>,
}

impl Library {
    /// Loads the library, dropping (and saving away) entries whose file no
    /// longer exists. A missing or corrupt file is an empty library.
    pub fn load(path: PathBuf) -> Self {
        let items: Vec<DownloadedModifier> = fs::read_to_string(&path)
            .ok()
            .and_then(|text| serde_json::from_str::<Vec<serde_json::Value>>(&text).ok())
            .unwrap_or_default()
            .into_iter()
            .filter_map(|value| serde_json::from_value(value).ok())
            .collect();
        let total = items.len();
        let items: Vec<_> = items
            .into_iter()
            .filter(|item: &DownloadedModifier| {
                !item.file_path.is_empty() && Path::new(&item.file_path).exists()
            })
            .collect();
        let library = Self { path, items };
        if library.items.len() != total {
            library.save_logged();
        }
        library
    }

    pub fn items(&self) -> &[DownloadedModifier] {
        &self.items
    }

    pub fn save(&self) -> io::Result<()> {
        if let Some(dir) = self.path.parent() {
            fs::create_dir_all(dir)?;
        }
        let json = serde_json::to_string_pretty(&self.items).map_err(io::Error::other)?;
        fs::write(&self.path, json + "\n")
    }

    fn save_logged(&self) {
        if let Err(err) = self.save() {
            tracing::warn!(path = ?self.path, %err, "failed to save downloaded modifiers");
        }
    }

    /// Inserts `item`, or replaces the entry with the same name and version,
    /// then saves. Returns the entry's index.
    pub fn upsert(&mut self, item: DownloadedModifier) -> usize {
        let index = match self
            .items
            .iter()
            .position(|e| e.name == item.name && e.version == item.version)
        {
            Some(index) => {
                self.items[index] = item;
                index
            }
            None => {
                self.items.push(item);
                self.items.len() - 1
            }
        };
        self.save_logged();
        index
    }

    /// The file to open for entry `index`. A record whose file vanished is
    /// dropped instead. `None` for an out-of-range index.
    pub fn run_target(&mut self, index: usize) -> Option<RunTarget> {
        let path = PathBuf::from(&self.items.get(index)?.file_path);
        if path.exists() {
            return Some(RunTarget::Open(path));
        }
        self.items.remove(index);
        self.save_logged();
        Some(RunTarget::Missing)
    }

    /// Deletes entry `index`'s file and record. If the file exists but cannot
    /// be deleted (open elsewhere, access denied) the record is kept.
    /// `Ok(false)` for an out-of-range index.
    pub fn delete(&mut self, index: usize) -> Result<bool, DeleteError> {
        let Some(item) = self.items.get(index) else {
            return Ok(false);
        };
        let path = Path::new(&item.file_path);
        if path.exists() && fs::remove_file(path).is_err() {
            return Err(DeleteError {
                name: item.name.clone(),
                file_path: item.file_path.clone(),
            });
        }
        self.items.remove(index);
        self.save_logged();
        Ok(true)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn item(name: &str, version: &str, file: &Path) -> DownloadedModifier {
        DownloadedModifier {
            name: name.into(),
            version: version.into(),
            game_version: "v1".into(),
            download_date: None,
            file_path: file.to_string_lossy().into_owned(),
            url: "https://x".into(),
            display_name: String::new(),
            display_subtitle: String::new(),
        }
    }

    #[test]
    fn load_prunes_missing_files_and_saves() {
        let dir = tempfile::tempdir().unwrap();
        let present = dir.path().join("a.zip");
        fs::write(&present, b"x").unwrap();
        let json_path = dir.path().join("data/downloaded_modifiers.json");
        fs::create_dir_all(json_path.parent().unwrap()).unwrap();
        let entries = vec![
            item("A", "v1", &present),
            item("B", "v1", &dir.path().join("gone.zip")),
        ];
        fs::write(&json_path, serde_json::to_string(&entries).unwrap()).unwrap();

        let library = Library::load(json_path.clone());
        assert_eq!(library.items().len(), 1);
        let saved: Vec<DownloadedModifier> =
            serde_json::from_str(&fs::read_to_string(&json_path).unwrap()).unwrap();
        assert_eq!(saved.len(), 1);
    }

    #[test]
    fn upsert_replaces_same_name_and_version() {
        let dir = tempfile::tempdir().unwrap();
        let mut library = Library::load(dir.path().join("lib.json"));
        let file = dir.path().join("a.zip");
        assert_eq!(library.upsert(item("A", "v1", &file)), 0);
        assert_eq!(library.upsert(item("A", "v2", &file)), 1);
        let mut updated = item("A", "v1", &file);
        updated.game_version = "v9".into();
        assert_eq!(library.upsert(updated), 0);
        assert_eq!(library.items().len(), 2);
        assert_eq!(library.items()[0].game_version, "v9");
    }

    #[test]
    fn run_target_drops_missing_entries() {
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("a.exe");
        fs::write(&file, b"MZ").unwrap();
        let mut library = Library::load(dir.path().join("lib.json"));
        library.upsert(item("A", "v1", &file));
        assert_eq!(library.run_target(0), Some(RunTarget::Open(file.clone())));
        fs::remove_file(&file).unwrap();
        assert_eq!(library.run_target(0), Some(RunTarget::Missing));
        assert!(library.items().is_empty());
        assert_eq!(library.run_target(0), None);
    }

    #[test]
    fn delete_removes_file_and_record() {
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("a.zip");
        fs::write(&file, b"x").unwrap();
        let mut library = Library::load(dir.path().join("lib.json"));
        library.upsert(item("A", "v1", &file));
        assert_eq!(library.delete(0), Ok(true));
        assert!(!file.exists());
        assert_eq!(library.delete(0), Ok(false));
    }

    #[cfg(windows)]
    #[test]
    fn delete_keeps_record_when_file_is_locked() {
        use std::os::windows::fs::OpenOptionsExt;
        let dir = tempfile::tempdir().unwrap();
        let file = dir.path().join("locked.zip");
        fs::write(&file, b"x").unwrap();
        // No FILE_SHARE_DELETE: deleting fails while this handle is open.
        let _handle = fs::OpenOptions::new()
            .read(true)
            .share_mode(0)
            .open(&file)
            .unwrap();
        let mut library = Library::load(dir.path().join("lib.json"));
        library.upsert(item("A", "v1", &file));
        let err = library.delete(0).unwrap_err();
        assert_eq!(err.name, "A");
        assert_eq!(library.items().len(), 1);
    }
}
