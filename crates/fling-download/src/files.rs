//! File-level helpers. Ports of `DownloadManager::cleanUrl` and
//! `DownloadManager::correctFileExtension`.

use std::fs::File;
use std::io::Read;
use std::path::{Path, PathBuf};

use fling_core::file_kind::{self, SNIFF_LEN};

/// Trims, drops trailing commas, and rejects anything that is not http(s).
pub fn clean_url(url: &str) -> Option<String> {
    let cleaned = url.trim().trim_end_matches(',');
    (cleaned.starts_with("http://") || cleaned.starts_with("https://")).then(|| cleaned.to_owned())
}

fn sniff(path: &Path) -> Option<&'static str> {
    let mut header = Vec::with_capacity(SNIFF_LEN);
    File::open(path)
        .ok()?
        .take(SNIFF_LEN as u64)
        .read_to_end(&mut header)
        .ok()?;
    file_kind::detect(&header)
}

/// Renames `path` so its extension matches its content (a `.zip` that is
/// really an `MZ` executable becomes `.exe`). Returns the path the file ends
/// up at; on an unknown format, or if the target name is taken, the file
/// stays where it is.
pub fn correct_file_extension(path: &Path) -> PathBuf {
    let Some(detected) = sniff(path) else {
        return path.to_path_buf();
    };
    if file_kind::extension_of(&path.to_string_lossy()) == detected {
        return path.to_path_buf();
    }
    let Some(stem) = path.file_stem() else {
        return path.to_path_buf();
    };
    let mut name = stem.to_os_string();
    name.push(".");
    name.push(detected);
    let target = path.with_file_name(name);
    // QFile::rename never overwrote; std::fs::rename would on Windows.
    if target.exists() || std::fs::rename(path, &target).is_err() {
        return path.to_path_buf();
    }
    target
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn clean_url_rules() {
        assert_eq!(
            clean_url("  https://x/a,, ").as_deref(),
            Some("https://x/a")
        );
        assert_eq!(clean_url("http://x").as_deref(), Some("http://x"));
        assert_eq!(clean_url("ftp://x"), None);
        assert_eq!(clean_url(""), None);
    }

    #[test]
    fn renames_detected_executable() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("Elden Ring_v1.02.zip");
        std::fs::write(&path, b"MZ\x90\x00rest").unwrap();
        let fixed = correct_file_extension(&path);
        assert_eq!(fixed, dir.path().join("Elden Ring_v1.02.exe"));
        assert!(fixed.exists() && !path.exists());
    }

    #[test]
    fn keeps_matching_unknown_or_blocked_files() {
        let dir = tempfile::tempdir().unwrap();
        let zip = dir.path().join("a.zip");
        std::fs::write(&zip, b"PK\x03\x04").unwrap();
        assert_eq!(correct_file_extension(&zip), zip);

        let unknown = dir.path().join("b.zip");
        std::fs::write(&unknown, b"<html>").unwrap();
        assert_eq!(correct_file_extension(&unknown), unknown);

        let blocked = dir.path().join("c.zip");
        std::fs::write(&blocked, b"MZ").unwrap();
        std::fs::write(dir.path().join("c.exe"), b"other").unwrap();
        assert_eq!(correct_file_extension(&blocked), blocked);
    }

    #[test]
    fn detects_tar_by_ustar_magic() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("t.zip");
        let mut data = vec![0u8; 600];
        data[257..262].copy_from_slice(b"ustar");
        std::fs::write(&path, data).unwrap();
        assert_eq!(correct_file_extension(&path), dir.path().join("t.tar"));
    }
}
