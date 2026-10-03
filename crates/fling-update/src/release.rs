//! Parsing GitHub/Gitee `releases/latest` responses (both use the same keys).

use fling_core::UpdateSource;
use fling_core::version::{is_valid_version, normalize_version};
use serde_json::Value;

const INSTALLER_PREFIX: &str = "FLiNG-Downloader-v";
const INSTALLER_SUFFIX: &str = "-win-x64-setup.exe";
const DATABASE_ASSET: &str = "fling_translations.db";

/// A release with the one asset this app wants from it.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ReleaseInfo {
    /// Normalized (no leading `v`).
    pub version: String,
    pub source: UpdateSource,
    pub release_url: String,
    pub published_at: String,
    pub asset_name: String,
    pub download_url: String,
}

impl ReleaseInfo {
    pub fn is_valid(&self) -> bool {
        !self.version.is_empty() && !self.asset_name.is_empty() && !self.download_url.is_empty()
    }
}

/// `FLiNG-Downloader-v{version}-win-x64-setup.exe`, the name the release
/// workflow gives the installer.
pub fn installer_asset_name(version: &str) -> String {
    format!(
        "{INSTALLER_PREFIX}{}{INSTALLER_SUFFIX}",
        normalize_version(version)
    )
}

fn text(object: &Value, key: &str) -> String {
    object
        .get(key)
        .and_then(Value::as_str)
        .unwrap_or_default()
        .to_owned()
}

fn first_text(object: &Value, keys: &[&str]) -> String {
    keys.iter()
        .map(|k| text(object, k))
        .find(|v| !v.is_empty())
        .unwrap_or_default()
}

/// The release's tag (`tag_name`, else `tag`, else `name`), normalized.
fn release_version(object: &Value) -> String {
    ["tag_name", "tag", "name"]
        .iter()
        .map(|key| normalize_version(&text(object, key)).to_owned())
        .find(|v| !v.is_empty())
        .unwrap_or_default()
}

/// `browser_download_url`, else Gitee's `download_url`; only http(s).
fn asset_url(asset: &Value) -> Option<String> {
    ["browser_download_url", "download_url"]
        .iter()
        .map(|k| text(asset, k))
        .find(|url| {
            let lower = url.to_ascii_lowercase();
            lower.starts_with("https://") || lower.starts_with("http://")
        })
}

fn base_info(object: &Value, source: UpdateSource) -> ReleaseInfo {
    ReleaseInfo {
        source,
        release_url: first_text(object, &["html_url", "url"]),
        published_at: first_text(object, &["published_at", "created_at"]),
        ..ReleaseInfo::default()
    }
}

fn assets(object: &Value) -> impl Iterator<Item = &Value> {
    object
        .get("assets")
        .and_then(Value::as_array)
        .into_iter()
        .flatten()
        .filter(|a| a.is_object())
}

fn version_from_installer_name(name: &str) -> Option<String> {
    let lower = name.to_ascii_lowercase();
    if !lower.starts_with(&INSTALLER_PREFIX.to_ascii_lowercase())
        || !lower.ends_with(INSTALLER_SUFFIX)
        || name.len() < INSTALLER_PREFIX.len() + INSTALLER_SUFFIX.len()
    {
        return None;
    }
    let version = &name[INSTALLER_PREFIX.len()..name.len() - INSTALLER_SUFFIX.len()];
    Some(normalize_version(version).to_owned()).filter(|v| !v.is_empty())
}

/// The app release and its installer. A tag that is not valid semver is
/// ignored, in which case the version is taken from the installer's name.
pub fn parse_app_release(body: &[u8], source: UpdateSource) -> ReleaseInfo {
    let Ok(object) = serde_json::from_slice::<Value>(body) else {
        return ReleaseInfo {
            source,
            ..ReleaseInfo::default()
        };
    };
    if !object.is_object() {
        return ReleaseInfo {
            source,
            ..ReleaseInfo::default()
        };
    }
    let mut info = base_info(&object, source);
    info.version = Some(release_version(&object))
        .filter(|v| is_valid_version(v))
        .unwrap_or_default();
    let expected = (!info.version.is_empty()).then(|| installer_asset_name(&info.version));

    for asset in assets(&object) {
        let name = text(asset, "name");
        if name.is_empty() {
            continue;
        }
        let asset_version = match &expected {
            Some(expected) if name.eq_ignore_ascii_case(expected) => info.version.clone(),
            Some(_) => continue,
            None => match version_from_installer_name(&name) {
                Some(version) => version,
                None => continue,
            },
        };
        let Some(url) = asset_url(asset) else {
            continue;
        };
        info.asset_name = name;
        info.download_url = url;
        if info.version.is_empty() {
            info.version = asset_version;
        }
        break;
    }
    info
}

/// The database release and its `fling_translations.db` asset. The tag is
/// not required to be semver.
pub fn parse_database_release(body: &[u8], source: UpdateSource) -> ReleaseInfo {
    let Ok(object) = serde_json::from_slice::<Value>(body) else {
        return ReleaseInfo {
            source,
            ..ReleaseInfo::default()
        };
    };
    let mut info = base_info(&object, source);
    info.version = release_version(&object);
    if let Some((name, url)) = assets(&object).find_map(|asset| {
        let name = text(asset, "name");
        if !name.eq_ignore_ascii_case(DATABASE_ASSET) {
            return None;
        }
        Some((name, asset_url(asset)?))
    }) {
        info.asset_name = name;
        info.download_url = url;
    }
    info
}

#[cfg(test)]
mod tests {
    use super::*;

    const GITHUB_APP: &str = r#"{
        "tag_name": "v1.2.0",
        "html_url": "https://github.com/Sqhh99/FLiNG-Downloader/releases/tag/v1.2.0",
        "published_at": "2026-09-01T00:00:00Z",
        "assets": [
            {"name": "FLiNG-Downloader-v1.2.0-win-x64-portable.zip", "browser_download_url": "https://x/portable.zip"},
            {"name": "fling-downloader-V1.2.0-WIN-X64-SETUP.EXE", "browser_download_url": "https://x/setup.exe"}
        ]
    }"#;

    #[test]
    fn parses_github_app_release() {
        let info = parse_app_release(GITHUB_APP.as_bytes(), UpdateSource::GitHub);
        assert!(info.is_valid());
        assert_eq!(info.version, "1.2.0");
        assert_eq!(info.asset_name, "fling-downloader-V1.2.0-WIN-X64-SETUP.EXE");
        assert_eq!(info.download_url, "https://x/setup.exe");
        assert_eq!(info.published_at, "2026-09-01T00:00:00Z");
        assert_eq!(info.source, UpdateSource::GitHub);
    }

    #[test]
    fn version_comes_from_asset_when_tag_is_not_semver() {
        let body = br#"{"tag_name": "latest", "assets": [
            {"name": "FLiNG-Downloader-v1.3.0-beta.1-win-x64-setup.exe", "download_url": "https://gitee/x.exe"}]}"#;
        let info = parse_app_release(body, UpdateSource::Gitee);
        assert_eq!(info.version, "1.3.0-beta.1");
        assert_eq!(info.download_url, "https://gitee/x.exe");
    }

    #[test]
    fn app_release_without_matching_asset_is_invalid() {
        let body = br#"{"tag_name": "v1.2.0", "assets": [
            {"name": "FLiNG-Downloader-v1.1.0-win-x64-setup.exe", "browser_download_url": "https://x"},
            {"name": "FLiNG-Downloader-v1.2.0-win-x64-setup.exe", "browser_download_url": "ftp://x"}]}"#;
        assert!(!parse_app_release(body, UpdateSource::GitHub).is_valid());
        assert!(!parse_app_release(b"[]", UpdateSource::GitHub).is_valid());
        assert!(!parse_app_release(b"garbage", UpdateSource::GitHub).is_valid());
    }

    #[test]
    fn parses_gitee_database_release() {
        let body = br#"{"tag_name": "v0.0.4", "name": "db", "created_at": "2026-09-02",
            "assets": [{"name": "FLING_TRANSLATIONS.DB", "browser_download_url": "",
                        "download_url": "https://gitee.com/x/fling_translations.db"}]}"#;
        let info = parse_database_release(body, UpdateSource::Gitee);
        assert!(info.is_valid());
        assert_eq!(info.version, "0.0.4");
        assert_eq!(info.published_at, "2026-09-02");
        assert_eq!(
            info.download_url,
            "https://gitee.com/x/fling_translations.db"
        );
    }
}
