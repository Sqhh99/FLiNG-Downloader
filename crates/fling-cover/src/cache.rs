//! `<cache>/covers/`: `<gameId>.png` for found covers, and
//! `<gameId>.<model>.nocover` listing screenshot URLs the model found nothing
//! in. Same layout as the Qt build.

use std::fs::{self, OpenOptions};
use std::io::{self, Write};
use std::path::{Path, PathBuf};

/// Part of the no-cover marker name: shipping a new model retries old misses.
pub const MODEL_NAME: &str = "game-cover-v2";

#[derive(Debug, Clone)]
pub struct CoverCache {
    dir: PathBuf,
}

impl CoverCache {
    pub fn new(dir: PathBuf) -> Self {
        Self { dir }
    }

    pub fn dir(&self) -> &Path {
        &self.dir
    }

    pub fn cover_path(&self, game_id: &str) -> PathBuf {
        self.dir.join(format!("{game_id}.png"))
    }

    fn marker_path(&self, game_id: &str) -> PathBuf {
        self.dir.join(format!("{game_id}.{MODEL_NAME}.nocover"))
    }

    /// The cached cover, if a non-empty one exists.
    pub fn cached_cover(&self, game_id: &str) -> Option<PathBuf> {
        if game_id.is_empty() {
            return None;
        }
        let path = self.cover_path(game_id);
        fs::metadata(&path)
            .ok()
            .filter(|m| m.is_file() && m.len() > 0)
            .map(|_| path)
    }

    /// Whether the model already looked at exactly this screenshot and found nothing.
    pub fn is_known_without_cover(&self, game_id: &str, image_url: &str) -> bool {
        if game_id.is_empty() || image_url.is_empty() {
            return false;
        }
        fs::read_to_string(self.marker_path(game_id))
            .map(|text| text.lines().any(|line| line == image_url))
            .unwrap_or(false)
    }

    pub fn record_no_cover(&self, game_id: &str, image_url: &str) -> io::Result<()> {
        fs::create_dir_all(&self.dir)?;
        let mut marker = OpenOptions::new()
            .create(true)
            .append(true)
            .open(self.marker_path(game_id))?;
        writeln!(marker, "{image_url}")
    }

    /// Writes `png` atomically (temp file + rename), so a reader never sees a
    /// half-written cover.
    pub fn store(&self, game_id: &str, png: &[u8]) -> io::Result<PathBuf> {
        fs::create_dir_all(&self.dir)?;
        let path = self.cover_path(game_id);
        let temp = self
            .dir
            .join(format!("{game_id}.png.{}.tmp", std::process::id()));
        fs::write(&temp, png)?;
        fs::rename(&temp, &path).inspect_err(|_| {
            let _ = fs::remove_file(&temp);
        })?;
        Ok(path)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn cache_layout_and_markers() {
        let dir = tempfile::tempdir().unwrap();
        let cache = CoverCache::new(dir.path().join("covers"));
        assert_eq!(cache.cached_cover("Elden_Ring"), None);
        assert_eq!(cache.cached_cover(""), None);

        let path = cache.store("Elden_Ring", b"png").unwrap();
        assert_eq!(path, dir.path().join("covers/Elden_Ring.png"));
        assert_eq!(cache.cached_cover("Elden_Ring"), Some(path));

        assert!(!cache.is_known_without_cover("G", "https://a"));
        cache.record_no_cover("G", "https://a").unwrap();
        cache.record_no_cover("G", "https://b").unwrap();
        assert!(cache.is_known_without_cover("G", "https://a"));
        assert!(cache.is_known_without_cover("G", "https://b"));
        assert!(!cache.is_known_without_cover("G", "https://c"));
        assert!(dir.path().join("covers/G.game-cover-v2.nocover").exists());
    }
}
