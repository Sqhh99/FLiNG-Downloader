//! Text helpers ported from the Qt build. Each one documents the C++ function it
//! replaces; their behavior (including quirks) is pinned by the tests below
//! because cache keys and file names on users' disks depend on it.

use std::sync::LazyLock;

use regex::Regex;

/// Lower-cases, trims and strips separators/punctuation so titles that differ
/// only in spacing or punctuation compare equal.
///
/// Port of `TranslationTextUtils::normalizeLookupText`. The Qt pattern used
/// PCRE's ASCII-only `\s`, so only ASCII whitespace is stripped inside the text
/// (an ideographic space U+3000 survives unless it is leading/trailing).
pub fn normalize_lookup_text(value: &str) -> String {
    static IGNORED: LazyLock<Regex> = LazyLock::new(|| {
        Regex::new(r#"[ \t\n\x0B\x0C\r\-_:：·・'"()\[\]{}.,!?/\\]+"#).expect("valid regex")
    });
    let lowered = value.to_lowercase();
    IGNORED.replace_all(lowered.trim(), "").into_owned()
}

/// Port of `TranslationTextUtils::hasNormalizedLookupText`.
pub fn has_normalized_lookup_text(value: &str) -> bool {
    !normalize_lookup_text(value).is_empty()
}

/// Makes `text` safe as a single Windows path component.
///
/// Port of `sanitizePathComponent` in `Backend.cpp`: reserved characters and
/// control characters become `_`, surrounding whitespace and dots are removed,
/// and an empty result falls back to `fallback`.
pub fn sanitize_path_component(text: &str, fallback: &str) -> String {
    let replaced: String = text
        .chars()
        .map(|c| match c {
            '\\' | '/' | ':' | '*' | '?' | '"' | '<' | '>' | '|' => '_',
            c if (c as u32) < 0x20 => '_',
            c => c,
        })
        .collect();
    let sanitized = replaced.trim().trim_matches('.').trim();
    if sanitized.is_empty() {
        fallback.to_owned()
    } else {
        sanitized.to_owned()
    }
}

/// Cover cache key: every character that is not ASCII alphanumeric becomes `_`.
///
/// Port of `Backend::coverGameId`. Qt iterated UTF-16 code units, so a
/// character outside the BMP becomes two underscores; that is kept so existing
/// cache files keep matching.
pub fn cover_game_id(name: &str) -> String {
    let mut id = String::with_capacity(name.len());
    for c in name.chars() {
        if c.is_ascii_alphanumeric() {
            id.push(c);
        } else {
            for _ in 0..c.len_utf16() {
                id.push('_');
            }
        }
    }
    id
}

/// Upper-cases the first character of every space-separated word and joins the
/// words with single spaces.
///
/// Port of `ModifierInfoManager::formatModifierName`. Qt's `QChar::toUpper` is
/// a one-to-one mapping, so characters whose upper case expands (e.g. `ß`)
/// are left unchanged.
pub fn format_modifier_name(name: &str) -> String {
    name.split(' ')
        .filter(|w| !w.is_empty())
        .map(|word| {
            let mut chars = word.chars();
            let first = chars.next().expect("non-empty word");
            let mut upper = first.to_uppercase();
            let first = match (upper.next(), upper.next()) {
                (Some(u), None) => u,
                _ => first,
            };
            std::iter::once(first).chain(chars).collect::<String>()
        })
        .collect::<Vec<_>>()
        .join(" ")
}

/// Normalizes a download-version label to a `v` prefix.
///
/// Port of `ModifierInfoManager::formatVersionString`: a bare number gains a
/// `v`; `v`, `V`, `ver.` and `version` prefixes (case-sensitive) become `v`.
pub fn format_version_string(version: &str) -> String {
    static BARE_NUMBER: LazyLock<Regex> =
        LazyLock::new(|| Regex::new(r"^[0-9]+\.?[0-9]*$").expect("valid regex"));
    static PREFIXED: LazyLock<Regex> = LazyLock::new(|| {
        Regex::new(r"^(?:v|ver\.|version|V)[ \t\n\x0B\x0C\r]*([0-9].*)$").expect("valid regex")
    });
    if BARE_NUMBER.is_match(version) {
        return format!("v{version}");
    }
    match PREFIXED.captures(version) {
        Some(caps) => format!("v{}", &caps[1]),
        None => version.to_owned(),
    }
}

/// CJK Unified Ideographs (U+4E00–U+9FFF). Port of `GameMappingManager::containsChinese`.
pub fn contains_chinese(text: &str) -> bool {
    text.chars().any(|c| ('\u{4E00}'..='\u{9FFF}').contains(&c))
}

/// Hiragana or katakana (U+3040–U+30FF). Port of `containsJapaneseScript`.
pub fn contains_japanese_kana(text: &str) -> bool {
    text.chars().any(|c| ('\u{3040}'..='\u{30FF}').contains(&c))
}

/// Has an ASCII letter or digit and no Chinese or kana. Port of `isEnglishLikeInput`.
pub fn is_english_like(text: &str) -> bool {
    text.chars().any(|c| c.is_ascii_alphanumeric())
        && !contains_chinese(text)
        && !contains_japanese_kana(text)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_strips_punctuation_and_ascii_space() {
        assert_eq!(
            normalize_lookup_text("  Ace Combat 7: Skies Unknown "),
            "acecombat7skiesunknown"
        );
        assert_eq!(
            normalize_lookup_text("エースコンバット7・スカイズ・アンノウン"),
            normalize_lookup_text("エースコンバット7スカイズアンノウン")
        );
        assert_eq!(normalize_lookup_text("a\u{3000}b"), "a\u{3000}b");
        assert_eq!(normalize_lookup_text("\u{3000}ab\u{3000}"), "ab");
        assert_eq!(normalize_lookup_text("Tom & Jerry™"), "tom&jerry™");
        assert!(!has_normalized_lookup_text(" - : ."));
    }

    #[test]
    fn sanitize_replaces_reserved_and_trims_dots() {
        assert_eq!(sanitize_path_component("a/b:c*?\"<>|", "x"), "a_b_c______");
        assert_eq!(sanitize_path_component(" ..name.. ", "x"), "name");
        assert_eq!(sanitize_path_component("tab\there", "x"), "tab_here");
        assert_eq!(sanitize_path_component(" . ", "trainer"), "trainer");
    }

    #[test]
    fn cover_id_counts_utf16_units() {
        assert_eq!(cover_game_id("Elden Ring: 2"), "Elden_Ring__2");
        assert_eq!(cover_game_id("艾尔"), "__");
        assert_eq!(cover_game_id("a😀"), "a__");
    }

    #[test]
    fn format_name_title_cases_words() {
        assert_eq!(
            format_modifier_name("elden  ring trainer"),
            "Elden Ring Trainer"
        );
        assert_eq!(format_modifier_name("ßtraße"), "ßtraße");
        assert_eq!(format_modifier_name(""), "");
    }

    #[test]
    fn format_version_normalizes_prefix() {
        assert_eq!(format_version_string("1.02"), "v1.02");
        assert_eq!(format_version_string("version 2.1"), "v2.1");
        assert_eq!(format_version_string("ver.3"), "v3");
        assert_eq!(format_version_string("V 1.0+"), "v1.0+");
        assert_eq!(format_version_string("Version 1.0"), "Version 1.0");
        assert_eq!(format_version_string("Early Access"), "Early Access");
    }

    #[test]
    fn script_detection() {
        assert!(contains_chinese("艾尔登法环"));
        assert!(contains_japanese_kana("エース"));
        assert!(is_english_like("ace combat"));
        assert!(!is_english_like("ace 艾尔"));
        assert!(!is_english_like("---"));
    }
}
