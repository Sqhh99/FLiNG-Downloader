//! Homepage parsers. Ports of `SearchManager::parseRecentlyUpdatedModifiersFromHtml`
//! and the parsing half of `SearchManager::loadFeaturedModifiers`.

use std::collections::HashSet;

use fling_core::ModifierInfo;
use fling_core::text::format_modifier_name;

use super::{format_post_date, lazy_re, parse_modifier_list};

lazy_re!(
    STANDARD_ARTICLE,
    r#"(?is)<article[^>]*class="[^"]*post-standard[^"]*"[^>]*>(.*?)</article>"#
);
lazy_re!(
    TITLE,
    r#"(?is)<h[123][^>]*class="[^"]*post-title[^"]*"[^>]*>\s*<a[^>]*href="([^"]*)"[^>]*>([^<]+)</a>"#
);
lazy_re!(
    POST_DATE,
    r#"(?is)<div class="post-details-day">(\d+)</div>\s*<div class="post-details-month">([^<]+)</div>\s*<div class="post-details-year">(\d+)</div>"#
);
lazy_re!(OPTIONS, r"(?i)(\d+)\s*Options");
lazy_re!(GAME_VERSION, r"(?i)Game Version:\s*([^·<]+)");
lazy_re!(LOOSE_VERSION, r"(?i)v([0-9\.]+)\+?");

lazy_re!(
    FEATURED_SECTION,
    r#"<h2[^>]*>Latest Trainers|<h2[^>]*>Recent Trainers|<div[^>]*class="recent-posts"#
);
lazy_re!(FEATURED_ARTICLE, r"(?s)<article[^>]*>(.+?)</article>");
lazy_re!(
    FEATURED_TITLE,
    r#"<h2[^>]*>\s*<a[^>]*href="([^"]+)"[^>]*>([^<]+)</a>"#
);

/// Lower-cased, first three letters; unknown months map to `01`.
fn month_number(month: &str) -> &'static str {
    let lower = month.trim().to_lowercase();
    let key: String = lower.chars().take(3).collect();
    match key.as_str() {
        "jan" => "01",
        "feb" => "02",
        "mar" => "03",
        "apr" => "04",
        "may" => "05",
        "jun" => "06",
        "jul" => "07",
        "aug" => "08",
        "sep" => "09",
        "oct" => "10",
        "nov" => "11",
        "dec" => "12",
        _ => "01",
    }
}

/// The homepage "recently updated" list, deduplicated by URL. Falls back to
/// the generic list parser (capped at 30) if the page layout changed.
pub fn parse_recently_updated(html: &str) -> Vec<ModifierInfo> {
    let mut seen = HashSet::new();
    let mut list = Vec::new();

    for article in STANDARD_ARTICLE.captures_iter(html) {
        let article = &article[1];
        let Some(title) = TITLE.captures(article) else {
            continue;
        };
        let url = title[1].trim().to_owned();
        if url.is_empty() || !seen.insert(url.clone()) {
            continue;
        }

        let mut modifier = ModifierInfo {
            name: format_modifier_name(title[2].trim()),
            url,
            ..ModifierInfo::default()
        };
        if let Some(date) = POST_DATE.captures(article) {
            modifier.last_update =
                format_post_date(date[3].trim(), month_number(&date[2]), &date[1]);
        }
        modifier.options_count = OPTIONS
            .captures(article)
            .and_then(|c| c[1].parse().ok())
            .unwrap_or(0);
        modifier.game_version = if let Some(v) = GAME_VERSION.captures(article) {
            v[1].trim().to_owned()
        } else if let Some(v) = LOOSE_VERSION.captures(article) {
            // The Qt build always appended "+" here.
            format!("v{}+", &v[1])
        } else {
            "Latest".to_owned()
        };
        list.push(modifier);
    }

    if list.is_empty() {
        for mut modifier in parse_modifier_list(html, "") {
            if modifier.url.is_empty() || !seen.insert(modifier.url.clone()) {
                continue;
            }
            modifier.name = format_modifier_name(&modifier.name);
            list.push(modifier);
            if list.len() >= 30 {
                break;
            }
        }
    }
    list
}

/// The homepage "Latest Trainers" section used for an empty search: the
/// generic list parse of that section, followed by up to 15 more entries with
/// placeholder metadata (`today`, "Latest", 10 options). The two passes can
/// list the same trainer twice; that is the Qt behavior.
pub fn parse_featured(html: &str, today: &str) -> Vec<ModifierInfo> {
    let Some(found) = FEATURED_SECTION.find(html) else {
        return Vec::new();
    };
    let start = found.start();
    let rest = &html[start..];
    let end = rest
        .find("</section>")
        .or_else(|| rest.find("</div><!-- .site-content -->"))
        .unwrap_or_else(|| floor_char_boundary(rest, 15000));
    let section = &rest[..end];

    let mut list = parse_modifier_list(section, "");
    list.extend(
        FEATURED_ARTICLE
            .captures_iter(section)
            .filter_map(|article| {
                let title = FEATURED_TITLE.captures(&article[1])?;
                Some(ModifierInfo {
                    url: title[1].to_owned(),
                    name: format_modifier_name(title[2].trim()),
                    last_update: today.to_owned(),
                    game_version: "Latest".to_owned(),
                    options_count: 10,
                    ..ModifierInfo::default()
                })
            })
            .take(15),
    );
    list
}

fn floor_char_boundary(text: &str, index: usize) -> usize {
    if index >= text.len() {
        return text.len();
    }
    (0..=index)
        .rev()
        .find(|&i| text.is_char_boundary(i))
        .unwrap_or(0)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn standard(title: &str, href: &str, extra: &str) -> String {
        format!(
            r#"<article class="post-standard post"><div class="post-details-day">7</div>
<div class="post-details-month">SEPTEMBER</div><div class="post-details-year">2026</div>
<h2 class="post-title"><a href="{href}">{title}</a></h2>{extra}</article>"#
        )
    }

    #[test]
    fn parses_recent_articles() {
        let html = [
            standard(
                "elden ring trainer",
                "https://x/elden/",
                "<p>24 Options · Game Version: v1.02+ · Steam</p>",
            ),
            standard("Dup", "https://x/elden/", ""),
            standard(
                "hades ii trainer",
                "https://x/hades/",
                "<p>Early build v0.9</p>",
            ),
            standard("no version trainer", "https://x/none/", ""),
        ]
        .concat();
        let list = parse_recently_updated(&html);
        assert_eq!(list.len(), 3);
        assert_eq!(list[0].name, "Elden Ring Trainer");
        assert_eq!(list[0].last_update, "2026-09-07");
        assert_eq!(list[0].options_count, 24);
        assert_eq!(list[0].game_version, "v1.02+");
        assert_eq!(list[1].game_version, "v0.9+");
        assert_eq!(list[2].game_version, "Latest");
        assert!(list.iter().all(|m| m.screenshot_url.is_empty()));
    }

    #[test]
    fn recent_falls_back_to_generic_parser() {
        let html = r#"<html><article class="x"><h3 class="post-title"><a href="https://x/a-trainer/">a game</a></h3></article></html>"#;
        let list = parse_recently_updated(html);
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].name, "A Game");
    }

    #[test]
    fn featured_section_adds_placeholder_entries() {
        let html = r#"<p>before</p><h2 class="t">Latest Trainers</h2>
<article class="post"><h2><a href="https://x/one-trainer/">one trainer</a></h2></article>
</section><article><h2><a href="https://x/outside/">outside</a></h2></article>"#;
        let list = parse_featured(html, "2026-10-03");
        assert_eq!(list.len(), 1);
        assert_eq!(list[0].name, "One Trainer");
        assert_eq!(list[0].last_update, "2026-10-03");
        assert_eq!(list[0].options_count, 10);
        assert!(parse_featured("<p>no section</p>", "2026-10-03").is_empty());
    }
}
