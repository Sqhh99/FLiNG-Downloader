//! `recent_modifiers_cache.json`: the last homepage list, shown at startup
//! before the network answers. Same format as the Qt build.

use std::fs;
use std::path::Path;

use fling_core::ModifierInfo;
use serde::{Deserialize, Serialize};

const MAX_CACHED_ITEMS: usize = 80;

#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct CachedModifier {
    #[serde(default)]
    name: String,
    #[serde(default)]
    url: String,
    #[serde(default)]
    last_update: String,
    #[serde(default)]
    game_version: String,
    #[serde(default)]
    options_count: u32,
    #[serde(default)]
    screenshot_url: String,
}

/// Cached entries without a name or URL are dropped; a missing or corrupt
/// file reads as empty.
pub fn load_recent_cache(path: &Path) -> Vec<ModifierInfo> {
    let Ok(text) = fs::read_to_string(path) else {
        return Vec::new();
    };
    let Ok(items) = serde_json::from_str::<Vec<serde_json::Value>>(&text) else {
        return Vec::new();
    };
    items
        .into_iter()
        .filter_map(|value| serde_json::from_value::<CachedModifier>(value).ok())
        .filter(|c| !c.name.is_empty() && !c.url.is_empty())
        .map(|c| ModifierInfo {
            name: c.name,
            url: c.url,
            last_update: c.last_update,
            game_version: c.game_version,
            options_count: c.options_count,
            screenshot_url: c.screenshot_url,
            ..ModifierInfo::default()
        })
        .collect()
}

/// Writes up to 80 entries with a name and URL. An empty list leaves the
/// existing cache alone.
pub fn save_recent_cache(path: &Path, modifiers: &[ModifierInfo]) {
    if modifiers.is_empty() {
        return;
    }
    let items: Vec<_> = modifiers
        .iter()
        .filter(|m| !m.name.is_empty() && !m.url.is_empty())
        .take(MAX_CACHED_ITEMS)
        .map(|m| CachedModifier {
            name: m.name.clone(),
            url: m.url.clone(),
            last_update: m.last_update.clone(),
            game_version: m.game_version.clone(),
            options_count: m.options_count,
            screenshot_url: m.screenshot_url.clone(),
        })
        .collect();
    let result = serde_json::to_string_pretty(&items)
        .map_err(std::io::Error::other)
        .and_then(|json| {
            if let Some(dir) = path.parent() {
                fs::create_dir_all(dir)?;
            }
            fs::write(path, json)
        });
    if let Err(err) = result {
        tracing::warn!(?path, %err, "failed to save recent modifiers cache");
    }
}

/// Whether two lists show the same rows (the fields the cache stores).
pub(crate) fn same_recent_list(a: &[ModifierInfo], b: &[ModifierInfo]) -> bool {
    a.len() == b.len()
        && a.iter().zip(b).all(|(x, y)| {
            x.name == y.name
                && x.url == y.url
                && x.last_update == y.last_update
                && x.game_version == y.game_version
                && x.options_count == y.options_count
                && x.screenshot_url == y.screenshot_url
        })
}

#[cfg(test)]
mod tests {
    use super::*;

    fn item(name: &str, url: &str) -> ModifierInfo {
        ModifierInfo {
            name: name.into(),
            url: url.into(),
            last_update: "2026-09-01".into(),
            options_count: 12,
            ..ModifierInfo::default()
        }
    }

    #[test]
    fn round_trips_and_filters() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("data/recent_modifiers_cache.json");
        let mut list: Vec<_> = (0..90)
            .map(|i| item(&format!("Game {i}"), &format!("https://x/{i}")))
            .collect();
        list.insert(0, item("", "https://x/noname"));
        save_recent_cache(&path, &list);

        let loaded = load_recent_cache(&path);
        assert_eq!(loaded.len(), 80);
        assert_eq!(loaded[0].name, "Game 0");
        assert_eq!(loaded[0].options_count, 12);
        assert!(same_recent_list(&loaded, &list[1..81]));
    }

    #[test]
    fn reads_qt_json_and_ignores_bad_entries() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("c.json");
        fs::write(
            &path,
            r#"[{"name":"A","url":"https://a","optionsCount":5}, {"name":"","url":"x"}, 3, {"name":"B"}]"#,
        )
        .unwrap();
        let loaded = load_recent_cache(&path);
        assert_eq!(loaded.len(), 1);
        assert_eq!(loaded[0].options_count, 5);
        fs::write(&path, "not json").unwrap();
        assert!(load_recent_cache(&path).is_empty());
    }
}
