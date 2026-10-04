//! Plain-data domain types. These are the snapshots the backend hands to the UI.

use chrono::NaiveDateTime;
use serde::{Deserialize, Serialize};

/// One downloadable build of a trainer, as listed on its detail page.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct DownloadVersion {
    /// Human-readable label, already passed through `text::format_version_string`.
    pub label: String,
    /// Download link exactly as scraped (not made absolute).
    pub url: String,
}

/// A trainer as scraped from flingtrainer.com.
///
/// List pages fill `name`, `url`, `last_update` and usually `screenshot_url`;
/// detail pages and search enrichment fill the rest.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct ModifierInfo {
    /// The site's title, e.g. "Elden Ring Trainer" (entities not decoded).
    /// Canonical: cover ids, the library and relevance all key on it.
    pub name: String,
    /// The name's main line in the configured trainer-name language, e.g.
    /// "艾尔登法环", or the site name when there is no translation. Filled by
    /// the backend; empty until then.
    pub display_name: String,
    /// The English game title under a translated name, e.g. "Elden Ring";
    /// empty when `display_name` is not a translation.
    pub display_subtitle: String,
    /// The game version the trainer supports, e.g. "v1.02-v1.05+", "Latest".
    pub game_version: String,
    /// `yyyy-MM-dd` on list pages, raw site text on detail pages.
    pub last_update: String,
    pub options_count: u32,
    pub versions: Vec<DownloadVersion>,
    /// Formatted option lines, including the "● ..." category headers.
    pub options: Vec<String>,
    /// Detail page URL.
    pub url: String,
    /// Screenshot used for cover extraction.
    pub screenshot_url: String,
}

/// A trainer file in the local library (`downloaded_modifiers.json`).
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct DownloadedModifier {
    #[serde(default)]
    pub name: String,
    /// Trainer build label (the download version), not the game version.
    #[serde(default)]
    pub version: String,
    #[serde(default)]
    pub game_version: String,
    /// Local time, serialized like Qt's `Qt::ISODate` (`yyyy-MM-ddTHH:mm:ss`).
    #[serde(default, with = "qt_iso_date")]
    pub download_date: Option<NaiveDateTime>,
    #[serde(default)]
    pub file_path: String,
    #[serde(default)]
    pub url: String,
    /// Like [`ModifierInfo::display_name`]. Filled by the backend and never
    /// persisted.
    #[serde(skip)]
    pub display_name: String,
    /// Like [`ModifierInfo::display_subtitle`]; never persisted.
    #[serde(skip)]
    pub display_subtitle: String,
}

impl DownloadedModifier {
    /// The library table's date column: `yyyy-MM-dd HH:mm`, empty when unknown.
    pub fn display_date(&self) -> String {
        self.download_date
            .map(|d| d.format("%Y-%m-%d %H:%M").to_string())
            .unwrap_or_default()
    }
}

mod qt_iso_date {
    use chrono::NaiveDateTime;
    use serde::{Deserialize, Deserializer, Serializer};

    const FORMAT: &str = "%Y-%m-%dT%H:%M:%S";

    pub fn serialize<S: Serializer>(
        value: &Option<NaiveDateTime>,
        s: S,
    ) -> Result<S::Ok, S::Error> {
        match value {
            Some(date) => s.serialize_str(&date.format(FORMAT).to_string()),
            None => s.serialize_str(""),
        }
    }

    pub fn deserialize<'de, D: Deserializer<'de>>(d: D) -> Result<Option<NaiveDateTime>, D::Error> {
        let text = Option::<String>::deserialize(d)?.unwrap_or_default();
        // Qt writes no fractional seconds or offset for local times, but tolerate both.
        let text = text.trim();
        let text = text.split(['+', 'Z']).next().unwrap_or_default();
        Ok(NaiveDateTime::parse_from_str(text, FORMAT)
            .or_else(|_| NaiveDateTime::parse_from_str(text, "%Y-%m-%dT%H:%M:%S%.f"))
            .ok())
    }
}

/// UI language. The numeric values match the Qt build's persisted `currentLanguage`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub enum Language {
    #[default]
    Chinese = 0,
    English = 1,
    Japanese = 2,
}

impl Language {
    pub const ALL: [Language; 3] = [Language::Chinese, Language::English, Language::Japanese];

    pub fn from_index(index: i64) -> Self {
        match index {
            1 => Self::English,
            2 => Self::Japanese,
            _ => Self::Chinese,
        }
    }

    pub fn index(self) -> i64 {
        self as i64
    }
}

/// The language trainer names are shown (and new downloads are named) in.
/// Persisted as `"auto"` / `"en"` / `"zh"` / `"ja"`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub enum TrainerNameLanguage {
    /// Whatever the UI language is.
    #[default]
    FollowUi,
    English,
    Chinese,
    Japanese,
}

impl TrainerNameLanguage {
    pub const ALL: [TrainerNameLanguage; 4] = [
        TrainerNameLanguage::FollowUi,
        TrainerNameLanguage::English,
        TrainerNameLanguage::Chinese,
        TrainerNameLanguage::Japanese,
    ];

    /// Unknown keys mean [`FollowUi`](Self::FollowUi).
    pub fn from_key(key: &str) -> Self {
        match key {
            "en" => Self::English,
            "zh" => Self::Chinese,
            "ja" => Self::Japanese,
            _ => Self::FollowUi,
        }
    }

    pub fn key(self) -> &'static str {
        match self {
            Self::FollowUi => "auto",
            Self::English => "en",
            Self::Chinese => "zh",
            Self::Japanese => "ja",
        }
    }

    /// The concrete language, given the current UI language.
    pub fn resolve(self, ui: Language) -> Language {
        match self {
            Self::FollowUi => ui,
            Self::English => Language::English,
            Self::Chinese => Language::Chinese,
            Self::Japanese => Language::Japanese,
        }
    }
}

/// Where release checks go. Persisted as `"github"` / `"gitee"`.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq, Hash)]
pub enum UpdateSource {
    #[default]
    GitHub,
    Gitee,
}

impl UpdateSource {
    /// Anything other than exactly `"gitee"` is GitHub, as in the Qt build.
    pub fn from_key(key: &str) -> Self {
        if key == "gitee" {
            Self::Gitee
        } else {
            Self::GitHub
        }
    }

    pub fn key(self) -> &'static str {
        match self {
            Self::GitHub => "github",
            Self::Gitee => "gitee",
        }
    }
}

/// Identifier of a download task, unique within one app session (`task_N`).
#[derive(Debug, Clone, PartialEq, Eq, Hash, PartialOrd, Ord)]
pub struct TaskId(pub String);

impl std::fmt::Display for TaskId {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str(&self.0)
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum TaskStatus {
    Queued,
    Downloading,
    Paused,
    Completed,
    Failed,
    Canceled,
}

impl TaskStatus {
    /// Completed, failed and canceled tasks never move again on their own.
    pub fn is_terminal(self) -> bool {
        matches!(self, Self::Completed | Self::Failed | Self::Canceled)
    }
}

/// Snapshot of one entry in the session download queue.
#[derive(Debug, Clone, PartialEq)]
pub struct DownloadTask {
    pub id: TaskId,
    /// Display name: the final file name once known.
    pub file_name: String,
    pub status: TaskStatus,
    pub bytes_received: u64,
    /// 0 when the server did not report a size.
    pub bytes_total: u64,
    /// Bytes per second, sampled once a second; 0 when idle.
    pub speed: u64,
    pub version: String,
    /// Final destination (before extension correction).
    pub save_path: String,
    pub error_message: String,
    pub created_at: NaiveDateTime,
}

impl DownloadTask {
    /// Fraction in `0.0..=1.0`, or `None` when the total size is unknown.
    pub fn progress(&self) -> Option<f32> {
        (self.bytes_total > 0)
            .then(|| (self.bytes_received as f64 / self.bytes_total as f64).clamp(0.0, 1.0) as f32)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn downloaded_modifier_reads_qt_json() {
        let json = r#"{"name":"Elden Ring","version":"v1.02","gameVersion":"v1.02+",
            "downloadDate":"2026-09-01T12:34:56","filePath":"C:/x.zip","url":"https://a"}"#;
        let item: DownloadedModifier = serde_json::from_str(json).unwrap();
        assert_eq!(item.display_date(), "2026-09-01 12:34");
        let back = serde_json::to_value(&item).unwrap();
        assert_eq!(back["downloadDate"], "2026-09-01T12:34:56");
        assert_eq!(back["gameVersion"], "v1.02+");
    }

    #[test]
    fn display_name_is_not_persisted() {
        let item: DownloadedModifier =
            serde_json::from_str(r#"{"name":"A","displayName":"B","displaySubtitle":"C"}"#)
                .unwrap();
        assert!(item.display_name.is_empty());
        assert!(item.display_subtitle.is_empty());
        let item = DownloadedModifier {
            display_name: "艾尔登法环".into(),
            display_subtitle: "A".into(),
            ..item
        };
        let back = serde_json::to_value(&item).unwrap();
        assert!(back.get("displayName").is_none());
        assert!(back.get("displaySubtitle").is_none());
    }

    #[test]
    fn trainer_name_language_keys_round_trip_and_resolve() {
        for language in TrainerNameLanguage::ALL {
            assert_eq!(TrainerNameLanguage::from_key(language.key()), language);
        }
        assert_eq!(
            TrainerNameLanguage::from_key("fr"),
            TrainerNameLanguage::FollowUi
        );
        assert_eq!(
            TrainerNameLanguage::FollowUi.resolve(Language::Japanese),
            Language::Japanese
        );
        assert_eq!(
            TrainerNameLanguage::English.resolve(Language::Chinese),
            Language::English
        );
    }

    #[test]
    fn downloaded_modifier_tolerates_missing_fields() {
        let item: DownloadedModifier = serde_json::from_str(r#"{"name":"A"}"#).unwrap();
        assert_eq!(item.download_date, None);
        assert!(item.file_path.is_empty());
    }
}
