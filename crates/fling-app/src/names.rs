//! Trainer names in the configured language: "Elden Ring Trainer" becomes the
//! title "艾尔登法环" over the subtitle "Elden Ring" when the translation
//! database knows the game, and the file name "艾尔登法环 (Elden Ring)".

use fling_core::Language;
use fling_mapping::GameMappings;
use fling_site::parser::decode_html_entities;

/// How the UI shows one trainer name.
#[derive(Debug, Clone, PartialEq, Eq)]
pub(crate) struct TrainerLabel {
    /// The translated game title, or the site name unchanged.
    pub title: String,
    /// The English game title under a translation; empty otherwise.
    pub subtitle: String,
}

impl TrainerLabel {
    /// One-line form for file names and messages: "<title> (<subtitle>)".
    pub fn one_line(&self) -> String {
        if self.subtitle.is_empty() {
            self.title.clone()
        } else {
            format!("{} ({})", self.title, self.subtitle)
        }
    }
}

/// The game title inside a site trainer name: entities decoded and a trailing
/// "Trainer" word removed (the rule `parser::list` uses for matching).
fn game_title(site_name: &str) -> String {
    let decoded = decode_html_entities(site_name);
    let trimmed = decoded.trim_end();
    let split = trimmed.len().saturating_sub("trainer".len());
    match (trimmed.get(..split), trimmed.get(split..)) {
        (Some(head), Some(tail))
            if tail.eq_ignore_ascii_case("trainer") && head.ends_with(char::is_whitespace) =>
        {
            head.trim_end().to_owned()
        }
        _ => trimmed.to_owned(),
    }
}

/// `site_name` as the UI shows it in `language`: the translated title over
/// the English title, or `site_name` unchanged (no subtitle) for English and
/// for games the database has no distinct title for.
pub(crate) fn label(site_name: &str, language: Language, mappings: &GameMappings) -> TrainerLabel {
    let untranslated = || TrainerLabel {
        title: site_name.to_owned(),
        subtitle: String::new(),
    };
    if language == Language::English {
        return untranslated();
    }
    let english = game_title(site_name);
    match mappings.localized_title(&english, language) {
        Some(translated) => TrainerLabel {
            title: translated.to_owned(),
            subtitle: english,
        },
        None => untranslated(),
    }
}

#[cfg(test)]
mod tests {
    use fling_mapping::GameRecord;

    use super::*;

    fn mappings() -> GameMappings {
        let record = |english: &str, chinese: &str, japanese: &str| GameRecord {
            english: english.into(),
            normalized_english: String::new(),
            chinese_simplified: chinese.into(),
            japanese: japanese.into(),
        };
        GameMappings::from_records(&[
            record("Elden Ring", "艾尔登法环", "エルデンリング"),
            record("Tom & Jerry", "猫和老鼠", "トムとジェリー"),
            record("Assassin's Creed 3", "刺客信条3", "アサシン クリード III"),
            record("Trainer Simulator", "训练师模拟器", ""),
        ])
    }

    fn pair(label: TrainerLabel) -> (String, String) {
        (label.title, label.subtitle)
    }

    #[test]
    fn translates_into_title_and_english_subtitle() {
        let m = mappings();
        let zh = label("Elden Ring Trainer", Language::Chinese, &m);
        assert_eq!(zh.one_line(), "艾尔登法环 (Elden Ring)");
        assert_eq!(pair(zh), ("艾尔登法环".into(), "Elden Ring".into()));
        assert_eq!(
            pair(label("Elden Ring trainer ", Language::Japanese, &m)),
            ("エルデンリング".into(), "Elden Ring".into())
        );
    }

    #[test]
    fn decodes_site_entities_before_lookup() {
        let m = mappings();
        assert_eq!(
            pair(label("Tom &amp; Jerry Trainer", Language::Chinese, &m)),
            ("猫和老鼠".into(), "Tom & Jerry".into())
        );
        assert_eq!(
            label("Assassin&#8217;s Creed 3 Trainer", Language::Japanese, &m).one_line(),
            "アサシン クリード III (Assassin's Creed 3)"
        );
    }

    #[test]
    fn only_a_separate_trailing_word_is_stripped() {
        assert_eq!(game_title("Trainer Simulator Trainer"), "Trainer Simulator");
        assert_eq!(game_title("Retrainer"), "Retrainer");
        assert_eq!(game_title("Trainer"), "Trainer");
        assert_eq!(
            label("Trainer Simulator Trainer", Language::Chinese, &mappings()).one_line(),
            "训练师模拟器 (Trainer Simulator)"
        );
    }

    #[test]
    fn english_and_misses_keep_the_site_name_without_subtitle() {
        let m = mappings();
        let untranslated = |name: &str| (name.to_owned(), String::new());
        assert_eq!(
            pair(label("Tom &amp; Jerry Trainer", Language::English, &m)),
            untranslated("Tom &amp; Jerry Trainer")
        );
        assert_eq!(
            pair(label("Unknown Game Trainer", Language::Chinese, &m)),
            untranslated("Unknown Game Trainer")
        );
        // No Japanese title in the database.
        let ja = label("Trainer Simulator Trainer", Language::Japanese, &m);
        assert_eq!(ja.one_line(), "Trainer Simulator Trainer");
        assert_eq!(pair(ja), untranslated("Trainer Simulator Trainer"));
    }
}
