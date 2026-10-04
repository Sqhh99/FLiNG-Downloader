//! File-type detection for downloaded trainers, by magic bytes and by name.
//!
//! Port of `DownloadManager::detectFileFormat` / `getFileExtension`.

/// Number of leading bytes [`detect`] needs to recognize every kind.
pub const SNIFF_LEN: usize = 262;

/// Detects the file kind from its first bytes, returning the extension the
/// file should carry (`zip`, `rar`, `7z`, `exe`, `gz`, `bz2`, `tar`), or `None`.
///
/// Pass at least [`SNIFF_LEN`] bytes when available; tar detection reads the
/// `ustar` magic at offset 257.
pub fn detect(header: &[u8]) -> Option<&'static str> {
    const SIGNATURES: &[(&[u8], &str)] = &[
        (b"PK\x03\x04", "zip"),
        (b"PK\x05\x06", "zip"),
        (b"PK\x07\x08", "zip"),
        (b"Rar!\x1A\x07\x00", "rar"),
        (b"Rar!\x1A\x07\x01", "rar"),
        (b"\x37\x7A\xBC\xAF\x27\x1C", "7z"),
        (b"MZ", "exe"),
        (b"\x1F\x8B", "gz"),
        (b"BZh", "bz2"),
    ];
    if let Some((_, kind)) = SIGNATURES
        .iter()
        .find(|(magic, _)| header.starts_with(magic))
    {
        return Some(kind);
    }
    (header.len() >= SNIFF_LEN && &header[257..262] == b"ustar").then_some("tar")
}

/// The extension of a path or URL: query and fragment removed, double
/// extensions (`tar.gz`, `tar.bz2`, `tar.xz`) kept together, lower-cased.
/// Returns an empty string when the file name has no dot.
pub fn extension_of(path_or_url: &str) -> String {
    let path = path_or_url.split('?').next().unwrap_or_default();
    let path = path.split('#').next().unwrap_or_default();
    // Qt only split on '/'; Rust paths on Windows use '\' too.
    let file_name = path.rsplit(['/', '\\']).next().unwrap_or_default();

    if file_name.contains(".tar.") {
        for ext in ["gz", "bz2", "xz"] {
            if file_name.ends_with(&format!(".{ext}")) {
                return format!("tar.{ext}");
            }
        }
    }
    for ext in ["tgz", "tbz2", "txz"] {
        if file_name.ends_with(&format!(".{ext}")) {
            return ext.to_owned();
        }
    }
    match file_name.rsplit_once('.') {
        Some((_, ext)) => ext.to_lowercase(),
        None => String::new(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn detects_magic_bytes() {
        assert_eq!(detect(b"PK\x03\x04rest"), Some("zip"));
        assert_eq!(detect(b"Rar!\x1A\x07\x01\x00"), Some("rar"));
        assert_eq!(detect(b"7z\xBC\xAF\x27\x1C"), Some("7z"));
        assert_eq!(detect(b"MZ\x90\x00"), Some("exe"));
        assert_eq!(detect(b"\x1F\x8B\x08"), Some("gz"));
        assert_eq!(detect(b"BZh9"), Some("bz2"));
        assert_eq!(detect(b"<html>"), None);
        assert_eq!(detect(b""), None);

        let mut tar = vec![0u8; 512];
        tar[257..262].copy_from_slice(b"ustar");
        assert_eq!(detect(&tar), Some("tar"));
        assert_eq!(detect(&tar[..261]), None);
    }

    #[test]
    fn extension_handles_urls_and_double_extensions() {
        assert_eq!(extension_of("https://x/a/Trainer.ZIP?x=1#f"), "zip");
        assert_eq!(extension_of("a.tar.gz"), "tar.gz");
        assert_eq!(extension_of("a.tgz"), "tgz");
        assert_eq!(extension_of("C:/dir.v1/name"), "");
        assert_eq!(extension_of(r"C:\dir.v1\name"), "");
        assert_eq!(extension_of("name_v1.0.zip.crdownload"), "crdownload");
    }
}
