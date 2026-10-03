//! Search-result and generic list pages. Port of `ModifierParser::parseModifierListHTML`.

use fling_core::ModifierInfo;

use super::{contains_ci, decode_html_entities, format_post_date, lazy_re};

lazy_re!(
    SEARCH_ARTICLE,
    r#"(?s)<article[^>]*class="[^"]*post[^"]*"[^>]*>(.*?)</article>"#
);
lazy_re!(ANY_ARTICLE, r"(?s)<article[^>]*>(.*?)</article>");
lazy_re!(
    TITLE,
    r#"(?s)<h[123][^>]*class="[^"]*post-title[^"]*"[^>]*>\s*<a[^>]*href="([^"]*)"[^>]*>([^<]+)</a>"#
);
lazy_re!(
    SCREENSHOT,
    r#"(?i)<img[^>]*src="([^"]*\.(?:jpg|png|gif))"[^>]*>"#
);
lazy_re!(
    POST_DATE,
    r#"(?s)<div class="post-details-day">(\d+)</div>\s*<div class="post-details-month">([^<]+)</div>\s*<div class="post-details-year">(\d+)</div>"#
);
lazy_re!(TRAILING_TRAINER, r"(?i)\s+Trainer\s*$");

const SEQUENCE_WORDS: [&str; 27] = [
    "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix", "x", "one", "two", "three", "four",
    "five", "six", "seven", "eight", "nine", "ten", "age", "episode", "part", "vol", "volume",
    "season", "chapter",
];

const STOP_WORDS: [&str; 5] = ["the", "and", "for", "of", "in"];

/// `(key, equivalents)`: a search word equal to the key matches a title containing
/// any equivalent, and vice versa. Substring checks, as in the Qt build.
const NUMBER_EQUIVALENTS: [(&str, &[&str]); 9] = [
    ("1", &["i", "one", "first"]),
    ("2", &["ii", "two", "second", "age"]),
    ("3", &["iii", "three", "third"]),
    ("4", &["iv", "four", "fourth"]),
    ("5", &["v", "five", "fifth"]),
    ("6", &["vi", "six", "sixth"]),
    ("age", &["2", "ii", "two", "second"]),
    ("episode", &["part", "vol", "volume"]),
    ("part", &["episode", "vol", "volume"]),
];

/// `QString::toInt` succeeds: optional sign, then ASCII digits that fit an `i32`.
fn is_qt_int(word: &str) -> bool {
    word.trim().parse::<i32>().is_ok()
}

fn is_number_or_sequence_word(word: &str) -> bool {
    let lower = word.to_lowercase();
    is_qt_int(&lower) || SEQUENCE_WORDS.contains(&lower.as_str())
}

fn contains_equivalent_number(title_lower: &str, word_lower: &str) -> bool {
    NUMBER_EQUIVALENTS.iter().any(|(key, values)| {
        (word_lower == *key && values.iter().any(|v| title_lower.contains(v)))
            || (values.contains(&word_lower) && title_lower.contains(key))
    })
}

/// Words of `search_term` that take part in fuzzy matching: numbers and
/// sequence words always, other words only when longer than two characters,
/// minus a few stop words.
fn search_words(search_term: &str) -> Vec<String> {
    decode_html_entities(search_term)
        .split(' ')
        .map(str::trim)
        .filter(|w| !w.is_empty())
        .filter(|w| is_number_or_sequence_word(w) || super::qt_len(w) > 2)
        .filter(|w| !STOP_WORDS.iter().any(|s| s.eq_ignore_ascii_case(w)))
        .map(str::to_owned)
        .collect()
}

fn matches_search(
    decoded_title: &str,
    stripped_name: &str,
    search_term: &str,
    words: &[String],
) -> bool {
    if search_term.is_empty()
        || contains_ci(decoded_title, search_term)
        || contains_ci(stripped_name, search_term)
    {
        return true;
    }
    if !words.is_empty() {
        let title_lower = decoded_title.to_lowercase();
        let matched = words
            .iter()
            .filter(|word| {
                let word_lower = word.to_lowercase();
                title_lower.contains(&word_lower)
                    || (is_number_or_sequence_word(word)
                        && contains_equivalent_number(&title_lower, &word_lower))
            })
            .count();
        let ratio = matched as f64 / words.len() as f64;
        if ratio >= 0.6 || (matched >= 1 && words.len() <= 2) || (matched >= 2 && ratio >= 0.4) {
            return true;
        }
    }
    contains_ci(search_term, "Red Dead") && contains_ci(decoded_title, "Red Dead Redemption")
}

/// The month abbreviation table of the list parser (case-sensitive).
fn month_number(month: &str) -> &'static str {
    match month {
        "Jan" => "01",
        "Feb" => "02",
        "Mar" => "03",
        "Apr" => "04",
        "May" => "05",
        "Jun" => "06",
        "Jul" => "07",
        "Aug" => "08",
        "Sep" => "09",
        "Oct" => "10",
        "Nov" => "11",
        "Dec" => "12",
        _ => "01",
    }
}

/// Trainers on a search-results or list page, filtered against `search_term`.
///
/// `name` is the raw title as it appears on the page (entities not decoded,
/// "Trainer" suffix kept); callers apply `format_modifier_name`. Options count
/// and game version are not on these pages and stay empty.
pub fn parse_modifier_list(html: &str, search_term: &str) -> Vec<ModifierInfo> {
    if html.len() < 10 {
        return Vec::new();
    }
    let is_search_page = contains_ci(html, "SEARCH RESULTS") || !search_term.is_empty();
    let words = if search_term.is_empty() {
        Vec::new()
    } else {
        search_words(search_term)
    };
    let articles = if is_search_page {
        &*SEARCH_ARTICLE
    } else {
        &*ANY_ARTICLE
    };

    let mut result = Vec::new();
    for article in articles.captures_iter(html) {
        let article_html = &article[1];
        let Some(title_match) = TITLE.captures(article_html) else {
            continue;
        };
        let url = &title_match[1];
        let title = title_match[2].trim();
        if !(contains_ci(title, "Trainer") || contains_ci(url, "trainer")) {
            continue;
        }

        let decoded_title = decode_html_entities(title);
        let stripped_name = TRAILING_TRAINER.replace(&decoded_title, "");
        if !matches_search(&decoded_title, &stripped_name, search_term, &words) {
            continue;
        }

        let mut modifier = ModifierInfo {
            // The Qt build overwrote its decoded name with the raw title here.
            name: title.to_owned(),
            url: url.to_owned(),
            ..ModifierInfo::default()
        };
        if let Some(shot) = SCREENSHOT.captures(article_html) {
            modifier.screenshot_url = shot[1].to_owned();
        }
        if let Some(date) = POST_DATE.captures(article_html) {
            modifier.last_update = format_post_date(&date[3], month_number(&date[2]), &date[1]);
        }
        result.push(modifier);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn article(title: &str, href: &str) -> String {
        format!(
            r#"<article class="post type-post"><div class="post-details-day">5</div>
<div class="post-details-month">Sep</div>
<div class="post-details-year">2026</div>
<img src="https://flingtrainer.com/wp-content/uploads/shot.jpg" alt="">
<h2 class="post-title entry-title"> <a href="{href}" rel="bookmark">{title}</a></h2></article>"#
        )
    }

    fn page(articles: &[(&str, &str)]) -> String {
        let body: String = articles.iter().map(|(t, h)| article(t, h)).collect();
        format!("<html><body><h1>SEARCH RESULTS</h1>{body}</body></html>")
    }

    #[test]
    fn extracts_fields_and_keeps_raw_title() {
        let html = page(&[(
            "Tom &amp; Jerry Trainer",
            "https://flingtrainer.com/trainer/tom-jerry-trainer/",
        )]);
        let list = parse_modifier_list(&html, "");
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].name, "Tom &amp; Jerry Trainer");
        assert_eq!(
            list[0].url,
            "https://flingtrainer.com/trainer/tom-jerry-trainer/"
        );
        assert_eq!(list[0].last_update, "2026-09-05");
        assert_eq!(
            list[0].screenshot_url,
            "https://flingtrainer.com/wp-content/uploads/shot.jpg"
        );
        assert_eq!(list[0].options_count, 0);
        assert!(list[0].game_version.is_empty());
    }

    #[test]
    fn skips_non_trainer_posts() {
        let html = page(&[
            ("Site News", "https://example.com/news/"),
            ("Elden Ring Trainer", "https://x/"),
        ]);
        let names: Vec<_> = parse_modifier_list(&html, "")
            .into_iter()
            .map(|m| m.name)
            .collect();
        assert_eq!(names, ["Elden Ring Trainer"]);
    }

    #[test]
    fn word_matching_thresholds() {
        let html = page(&[
            ("Ace Combat 7: Skies Unknown Trainer", "https://x/1"),
            ("Dark Souls III Trainer", "https://x/2"),
            ("Unrelated Game Trainer", "https://x/3"),
        ]);
        let names = |term: &str| -> Vec<String> {
            parse_modifier_list(&html, term)
                .into_iter()
                .map(|m| m.name)
                .collect()
        };
        assert_eq!(names("ace combat"), ["Ace Combat 7: Skies Unknown Trainer"]);
        // "3" matches "iii" through the equivalence table.
        assert_eq!(names("dark souls 3"), ["Dark Souls III Trainer"]);
        assert!(names("completely different words").is_empty());
    }

    #[test]
    fn number_equivalence_is_substring_based() {
        // "2" matches any title containing "age" - a known quirk kept from Qt.
        let html = page(&[("Stage Fright Trainer", "https://x/1")]);
        assert_eq!(parse_modifier_list(&html, "zzz 2").len(), 1);
    }

    #[test]
    fn red_dead_special_case() {
        let html = page(&[("Red Dead Redemption 2 Trainer", "https://x/1")]);
        assert_eq!(
            parse_modifier_list(&html, "red dead online stuff here now").len(),
            1
        );
    }

    #[test]
    fn non_search_page_accepts_any_article() {
        let html = r#"<html><article id="x"><h3 class="post-title"><a href="https://x/elden-trainer/">Elden Ring</a></h3></article></html>"#;
        let list = parse_modifier_list(html, "");
        assert_eq!(list.len(), 1);
        assert!(list[0].last_update.is_empty());
        assert!(parse_modifier_list("short", "").is_empty());
    }

    #[test]
    fn unknown_month_defaults_to_january() {
        let html = page(&[("A Trainer", "https://x/1")]).replace(">Sep<", ">September<");
        assert_eq!(parse_modifier_list(&html, "")[0].last_update, "2026-01-05");
    }
}
