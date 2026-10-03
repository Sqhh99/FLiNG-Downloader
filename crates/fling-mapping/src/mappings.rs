//! CN/JA → English title lookup. Port of `GameMappingManager`.

use std::collections::{BTreeMap, HashMap};

use fling_core::text::normalize_lookup_text;

use crate::GameRecord;

#[derive(Debug, Clone)]
struct MappingInfo {
    english: String,
    chinese: String,
    japanese: String,
    normalized_english: String,
    normalized_chinese: String,
    normalized_japanese: String,
}

/// An immutable index over the translation rows. Rebuild it after the
/// database changes.
#[derive(Debug, Default)]
pub struct GameMappings {
    /// Keyed by Chinese title, so the contains-fallback scans in that order and
    /// a later duplicate Chinese title replaces an earlier one (as Qt's `QMap`).
    by_chinese: BTreeMap<String, MappingInfo>,
    /// Trimmed, lower-cased title → English. The first value for a key wins.
    exact: HashMap<String, String>,
    /// `normalize_lookup_text` form → English. The first value for a key wins.
    normalized: HashMap<String, String>,
}

/// Qt used `trimmed().toCaseFolded()`; `to_lowercase` matches it for every
/// script in the database.
fn exact_key(value: &str) -> String {
    value.trim().to_lowercase()
}

fn add_lookup(map: &mut HashMap<String, String>, key: String, english: &str) {
    if !key.is_empty() && !english.is_empty() {
        map.entry(key).or_insert_with(|| english.to_owned());
    }
}

fn contains_either_way(
    input_lower: &str,
    normalized_input: &str,
    candidate: &str,
    normalized: &str,
) -> bool {
    if !candidate.is_empty() {
        let candidate_lower = candidate.to_lowercase();
        if input_lower.contains(&candidate_lower) || candidate_lower.contains(input_lower) {
            return true;
        }
    }
    if normalized_input.is_empty() || normalized.is_empty() {
        return false;
    }
    normalized_input.contains(normalized) || normalized.contains(normalized_input)
}

impl GameMappings {
    /// Rows with an empty Chinese or English title are skipped.
    pub fn from_records(records: &[GameRecord]) -> Self {
        let mut mappings = Self::default();
        for record in records {
            if record.chinese_simplified.is_empty() || record.english.is_empty() {
                continue;
            }
            let normalized_english = if record.normalized_english.is_empty() {
                normalize_lookup_text(&record.english)
            } else {
                normalize_lookup_text(&record.normalized_english)
            };
            let info = MappingInfo {
                english: record.english.clone(),
                chinese: record.chinese_simplified.clone(),
                japanese: record.japanese.clone(),
                normalized_chinese: normalize_lookup_text(&record.chinese_simplified),
                normalized_japanese: normalize_lookup_text(&record.japanese),
                normalized_english,
            };

            let english = &info.english;
            for key in [
                &info.chinese,
                &info.english,
                &info.japanese,
                &info.normalized_english,
            ] {
                add_lookup(&mut mappings.exact, exact_key(key), english);
            }
            for key in [
                &info.normalized_chinese,
                &info.normalized_english,
                &info.normalized_japanese,
            ] {
                add_lookup(&mut mappings.normalized, key.clone(), english);
            }
            mappings.by_chinese.insert(info.chinese.clone(), info);
        }
        mappings
    }

    pub fn len(&self) -> usize {
        self.by_chinese.len()
    }

    pub fn is_empty(&self) -> bool {
        self.by_chinese.is_empty()
    }

    /// Exact and normalized-exact matches only, so broad Latin queries such as
    /// "ace combat" stay site searches instead of collapsing to one title.
    /// Returns `None` when there is no such match.
    pub fn translate_for_search(&self, input: &str) -> Option<String> {
        self.translate(input, false)
    }

    /// Like [`translate_for_search`](Self::translate_for_search), then falls back
    /// to the first title (in Chinese-title order) that contains, or is
    /// contained in, the input.
    pub fn translate_to_english(&self, input: &str) -> Option<String> {
        self.translate(input, true)
    }

    fn translate(&self, input: &str, allow_contains: bool) -> Option<String> {
        let trimmed = input.trim();
        if trimmed.is_empty() {
            return None;
        }
        if let Some(english) = self.exact.get(&exact_key(trimmed)) {
            return Some(english.clone());
        }
        let normalized = normalize_lookup_text(trimmed);
        if normalized.is_empty() {
            return None;
        }
        if let Some(english) = self.normalized.get(&normalized) {
            return Some(english.clone());
        }
        if !allow_contains {
            return None;
        }
        let lower = trimmed.to_lowercase();
        self.by_chinese
            .values()
            .find(|info| {
                contains_either_way(&lower, &normalized, &info.chinese, &info.normalized_chinese)
                    || contains_either_way(
                        &lower,
                        &normalized,
                        &info.english,
                        &info.normalized_english,
                    )
                    || contains_either_way(
                        &lower,
                        &normalized,
                        &info.japanese,
                        &info.normalized_japanese,
                    )
            })
            .map(|info| info.english.clone())
    }

    /// Every Chinese title, sorted.
    pub fn chinese_names(&self) -> impl Iterator<Item = &str> {
        self.by_chinese.keys().map(String::as_str)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const ACE_JA: &str = "エースコンバット7 スカイズ・アンノウン";
    const ACE_JA_MIDDLE_DOT: &str = "エースコンバット7・スカイズ・アンノウン";

    fn record(english: &str, normalized: &str, chinese: &str, japanese: &str) -> GameRecord {
        GameRecord {
            english: english.into(),
            normalized_english: normalized.into(),
            chinese_simplified: chinese.into(),
            japanese: japanese.into(),
        }
    }

    fn sample() -> GameMappings {
        GameMappings::from_records(&[
            record(
                "Ace Combat 7: Skies Unknown",
                "ace combat 7 skies unknown",
                "皇牌空战7：未知空域",
                ACE_JA,
            ),
            record(
                "Ace Combat Assault Horizon",
                "ace combat assault horizon",
                "皇牌空战：突击地平线",
                "",
            ),
            record("Elden Ring", "elden ring", "艾尔登法环", "エルデンリング"),
            record("No Chinese", "no chinese", "", "ノー"),
        ])
    }

    #[test]
    fn resolves_japanese_title_and_variants() {
        let m = sample();
        let expected = m.translate_to_english(ACE_JA).unwrap();
        assert!(expected.contains("Ace Combat 7"));
        assert_eq!(
            m.translate_to_english("ace combat 7 skies unknown")
                .as_deref(),
            Some(expected.as_str())
        );
        assert_eq!(
            m.translate_to_english(ACE_JA_MIDDLE_DOT).as_deref(),
            Some(expected.as_str())
        );
    }

    #[test]
    fn search_translation_is_exact_only() {
        let m = sample();
        assert_eq!(
            m.translate_for_search("  艾尔登法环 ").as_deref(),
            Some("Elden Ring")
        );
        assert_eq!(
            m.translate_for_search("ELDEN ring").as_deref(),
            Some("Elden Ring")
        );
        assert_eq!(
            m.translate_for_search("eldenring").as_deref(),
            Some("Elden Ring")
        );
        assert_eq!(m.translate_for_search("ace combat"), None);
        assert_eq!(m.translate_for_search("   "), None);
        assert_eq!(m.translate_for_search("::"), None);
    }

    #[test]
    fn contains_fallback_scans_in_chinese_title_order() {
        let m = sample();
        // "皇牌空战7：未知空域" sorts before "皇牌空战：突击地平线".
        assert_eq!(
            m.translate_to_english("皇牌空战").as_deref(),
            Some("Ace Combat 7: Skies Unknown")
        );
        assert_eq!(m.translate_to_english("elden"), Some("Elden Ring".into()));
    }

    #[test]
    fn skips_rows_without_chinese_title() {
        let m = sample();
        assert_eq!(m.len(), 3);
        assert_eq!(m.translate_for_search("No Chinese"), None);
    }

    #[test]
    fn first_lookup_value_wins_but_last_chinese_row_is_kept() {
        let m = GameMappings::from_records(&[
            record("First", "", "同名", ""),
            record("Second", "", "同名", ""),
        ]);
        assert_eq!(m.translate_for_search("同名").as_deref(), Some("First"));
        assert_eq!(m.len(), 1);
        assert_eq!(
            m.translate_to_english("同名游戏").as_deref(),
            Some("Second")
        );
    }
}
