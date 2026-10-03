//! Search-box suggestions. Port of `Backend::getSuggestionItems` and its
//! `makeSuggestion*Text` helpers.

use std::collections::HashSet;

use fling_core::Language;
use fling_core::text::{is_english_like, normalize_lookup_text};

use crate::GameRecord;

/// One suggestion row.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Suggestion {
    /// What the popup shows, e.g. "艾尔登法环 (Elden Ring)".
    pub display_text: String,
    /// What replaces the search box text when picked.
    pub input_text: String,
    /// The canonical English title that is actually searched.
    pub search_keyword: String,
}

#[derive(Debug, Clone)]
struct Entry {
    chinese: String,
    english: String,
    japanese: String,
    /// The database's `normalized_english`, only lower-cased (spaces kept).
    normalized_english: String,
    chinese_lower: String,
    english_lower: String,
    japanese_lower: String,
    chinese_normalized: String,
    english_normalized: String,
    japanese_normalized: String,
}

/// Pre-lowered/normalized titles for instant per-keystroke matching.
///
/// Unlike [`GameMappings`](crate::GameMappings), rows without a Chinese title
/// are kept; only rows with no title at all are skipped.
#[derive(Debug, Default)]
pub struct SuggestionIndex {
    entries: Vec<Entry>,
}

#[derive(Clone, Copy)]
struct Matched {
    chinese: bool,
    english: bool,
    normalized_english: bool,
    japanese: bool,
}

impl SuggestionIndex {
    pub fn from_records(records: &[GameRecord]) -> Self {
        let entries = records
            .iter()
            .filter(|r| {
                !(r.english.is_empty() && r.chinese_simplified.is_empty() && r.japanese.is_empty())
            })
            .map(|r| {
                let english_normalized = normalize_lookup_text(&r.english);
                Entry {
                    chinese: r.chinese_simplified.clone(),
                    english: r.english.clone(),
                    japanese: r.japanese.clone(),
                    normalized_english: if r.normalized_english.is_empty() {
                        english_normalized.clone()
                    } else {
                        r.normalized_english.to_lowercase()
                    },
                    chinese_lower: r.chinese_simplified.to_lowercase(),
                    english_lower: r.english.to_lowercase(),
                    japanese_lower: r.japanese.to_lowercase(),
                    chinese_normalized: normalize_lookup_text(&r.chinese_simplified),
                    japanese_normalized: normalize_lookup_text(&r.japanese),
                    english_normalized,
                }
            })
            .collect();
        Self { entries }
    }

    /// Up to `max` suggestions for `keyword`, deduplicated by English title,
    /// in database order. Wording depends on the UI `language`.
    pub fn suggest(&self, keyword: &str, max: usize, language: Language) -> Vec<Suggestion> {
        let query = keyword.trim();
        let normalized = normalize_lookup_text(query);
        if query.is_empty() || normalized.is_empty() {
            return Vec::new();
        }
        let lower = query.to_lowercase();
        let english_input = is_english_like(query);

        let mut results = Vec::new();
        let mut seen = HashSet::new();
        for e in &self.entries {
            if results.len() >= max {
                break;
            }
            let hit = |name: &str, name_lower: &str, name_normalized: &str| {
                !name.is_empty()
                    && (name_lower.contains(&lower) || name_normalized.contains(&normalized))
            };
            let m = Matched {
                chinese: hit(&e.chinese, &e.chinese_lower, &e.chinese_normalized),
                english: hit(&e.english, &e.english_lower, &e.english_normalized),
                normalized_english: !e.normalized_english.is_empty()
                    && e.normalized_english.contains(&normalized),
                japanese: hit(&e.japanese, &e.japanese_lower, &e.japanese_normalized),
            };
            if !(m.chinese || m.english || m.normalized_english || m.japanese) {
                continue;
            }
            if e.english.is_empty() || !seen.insert(e.english.clone()) {
                continue;
            }
            results.push(Suggestion {
                display_text: display_text(e, english_input, m, language),
                input_text: input_text(e, english_input, m, language),
                search_keyword: e.english.clone(),
            });
        }
        results
    }
}

fn with_secondary(primary: &str, secondary: &str) -> String {
    if secondary.is_empty() {
        primary.to_owned()
    } else {
        format!("{primary} ({secondary})")
    }
}

fn localized_secondary(e: &Entry, language: Language) -> &str {
    match language {
        Language::Japanese if !e.japanese.is_empty() => &e.japanese,
        Language::Chinese if !e.chinese.is_empty() => &e.chinese,
        Language::Japanese if !e.chinese.is_empty() => &e.chinese,
        Language::Chinese if !e.japanese.is_empty() => &e.japanese,
        _ => "",
    }
}

fn display_text(e: &Entry, english_input: bool, m: Matched, language: Language) -> String {
    if language == Language::English {
        return e.english.clone();
    }
    if english_input || m.english || m.normalized_english {
        return with_secondary(&e.english, localized_secondary(e, language));
    }
    if m.japanese && !e.japanese.is_empty() {
        return with_secondary(&e.japanese, &e.english);
    }
    if m.chinese && !e.chinese.is_empty() {
        return with_secondary(&e.chinese, &e.english);
    }
    with_secondary(&e.english, localized_secondary(e, language))
}

fn input_text(e: &Entry, english_input: bool, m: Matched, language: Language) -> String {
    if language == Language::English || english_input || m.english || m.normalized_english {
        return e.english.clone();
    }
    if m.japanese && !e.japanese.is_empty() {
        return e.japanese.clone();
    }
    if m.chinese && !e.chinese.is_empty() {
        return e.chinese.clone();
    }
    match language {
        Language::Japanese if !e.japanese.is_empty() => e.japanese.clone(),
        Language::Chinese if !e.chinese.is_empty() => e.chinese.clone(),
        _ => e.english.clone(),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn index() -> SuggestionIndex {
        let r = |en: &str, n: &str, zh: &str, ja: &str| GameRecord {
            english: en.into(),
            normalized_english: n.into(),
            chinese_simplified: zh.into(),
            japanese: ja.into(),
        };
        SuggestionIndex::from_records(&[
            r("Elden Ring", "elden ring", "艾尔登法环", "エルデンリング"),
            r("Elden Ring", "elden ring", "艾尔登法环 重复", ""),
            r("English Only", "", "", ""),
            r("", "", "", ""),
        ])
    }

    #[test]
    fn chinese_query_shows_chinese_first() {
        let s = index().suggest("艾尔登", 8, Language::Chinese);
        assert_eq!(s.len(), 1, "deduplicated by English title");
        assert_eq!(s[0].display_text, "艾尔登法环 (Elden Ring)");
        assert_eq!(s[0].input_text, "艾尔登法环");
        assert_eq!(s[0].search_keyword, "Elden Ring");
    }

    #[test]
    fn english_query_shows_localized_secondary() {
        let idx = index();
        let zh = idx.suggest("elden", 8, Language::Chinese);
        assert_eq!(zh[0].display_text, "Elden Ring (艾尔登法环)");
        assert_eq!(zh[0].input_text, "Elden Ring");
        let ja = idx.suggest("elden", 8, Language::Japanese);
        assert_eq!(ja[0].display_text, "Elden Ring (エルデンリング)");
        let en = idx.suggest("エルデン", 8, Language::English);
        assert_eq!(en[0].display_text, "Elden Ring");
    }

    #[test]
    fn keeps_rows_without_chinese_and_respects_max() {
        let idx = index();
        let s = idx.suggest("english", 8, Language::Chinese);
        assert_eq!(s[0].display_text, "English Only");
        assert_eq!(idx.suggest("n", 1, Language::Chinese).len(), 1);
        assert!(idx.suggest("  ", 8, Language::Chinese).is_empty());
    }
}
