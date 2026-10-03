//! Trainer detail pages. Port of `ModifierParser::parseModifierDetailHTML`
//! plus the version-label formatting `ModifierManager::getModifierDetail` applied.

use fling_core::text::format_version_string;
use fling_core::{DownloadVersion, ModifierInfo};

use super::{contains_ci, lazy_re, parse_options};

lazy_re!(GAME_VERSION, r"(?i)Game Version:\s*([^<·]+)");
lazy_re!(ALT_VERSION, r"(?i)Version\s*:\s*([^<\n]+)");
lazy_re!(TITLE_VERSION, r"(?is)<title>.*?v([\d\.]+).*?</title>");
lazy_re!(
    META_VERSION,
    r#"(?is)<meta[^>]*content\s*=\s*["'].*?version\s*[:\s]\s*([^,"'<>]+)["']"#
);
lazy_re!(
    POST_META,
    r#"(?s)<div class="post-meta[^"]*"[^>]*>(.*?)</div>"#
);
lazy_re!(POST_META_VERSION, r"(?i)Game Version:\s*([^<]+)");
lazy_re!(
    FLEX_CONTENT,
    r#"(?s)<div class="flex-content[^"]*"[^>]*>(.*?)</div>"#
);

lazy_re!(LAST_UPDATED, r"(?i)Last Updated:\s*([^<\n]+)");
lazy_re!(
    DIV_LAST_UPDATED,
    r"(?is)<div[^>]*>.*?Last Updated:\s*([\d\.]+).*?</div>"
);
lazy_re!(
    ATTACHMENT_DATE,
    r#"(?i)<td class="attachment-date"[^>]*>([^<]+)</td>"#
);
lazy_re!(OPTIONS_COUNT, r"(?i)Options:\s*(\d+)");

lazy_re!(TABLE, r"(?s)<table[^>]*>.*?</table>");
lazy_re!(LINK, r#"(?s)<a[^>]*href="([^"]+)"[^>]*>(.*?)</a>"#);
lazy_re!(TAG, r"<[^>]*>");
lazy_re!(
    DOWNLOAD_DIV,
    r#"(?is)<div[^>]*class="[^"]*download[^"]*"[^>]*>(.*?)</div>"#
);
lazy_re!(
    ATTACHMENT_LINK,
    r#"(?s)<a[^>]*class="[^"]*attachment-link[^"]*"[^>]*href="([^"]+)"[^>]*>([^<]*)</a>"#
);

lazy_re!(
    SCREENSHOT,
    r#"(?i)<img[^>]*src\s*=\s*["']([^"']*\.(jpg|jpeg|png|gif))["'][^>]*>"#
);
lazy_re!(ANY_IMG, r#"(?i)<img[^>]+src\s*=\s*["']([^"']+)["'][^>]*>"#);

fn capture_trimmed(re: &regex::Regex, text: &str) -> Option<String> {
    re.captures(text).map(|c| c[1].trim().to_owned())
}

/// The game-version cascade: first pattern that matches wins.
fn game_version(html: &str) -> String {
    let mut version = if let Some(v) = capture_trimmed(&GAME_VERSION, html) {
        v
    } else if let Some(v) = capture_trimmed(&ALT_VERSION, html) {
        v
    } else if let Some(v) = capture_trimmed(&TITLE_VERSION, html) {
        format!("v{v}")
    } else if let Some(v) = capture_trimmed(&META_VERSION, html) {
        v
    } else if let Some(meta) = POST_META.captures(html) {
        let content = &meta[1];
        if let Some(v) = capture_trimmed(&POST_META_VERSION, content) {
            v
        } else if contains_ci(content, "Early Access") {
            "Early Access".to_owned()
        } else {
            String::new()
        }
    } else if let Some(flex) = FLEX_CONTENT.captures(html) {
        if contains_ci(&flex[1], "Early Access") {
            "Early Access".to_owned()
        } else {
            String::new()
        }
    } else {
        "Latest".to_owned()
    };
    // Case-sensitive, anywhere on the page.
    if html.contains("Early Access+") {
        version = "Early Access+".to_owned();
    }
    version
}

fn last_update(html: &str) -> String {
    capture_trimmed(&LAST_UPDATED, html)
        .or_else(|| capture_trimmed(&DIV_LAST_UPDATED, html))
        .or_else(|| capture_trimmed(&ATTACHMENT_DATE, html))
        .unwrap_or_default()
}

fn strip_tags(text: &str) -> String {
    TAG.replace_all(text, "").into_owned()
}

/// Download links: the first download-looking table, else the first
/// download `<div>`, then always every `attachment-link` anchor (which can
/// repeat a table link).
fn download_versions(html: &str) -> Vec<(String, String)> {
    let mut versions: Vec<(String, String)> = Vec::new();

    let table = TABLE.find_iter(html).map(|m| m.as_str()).find(|t| {
        contains_ci(t, "download") || contains_ci(t, "attachment") || contains_ci(t, "file")
    });
    if let Some(table) = table {
        for link in LINK.captures_iter(table) {
            let href = &link[1];
            let text = strip_tags(link[2].trim());
            let looks_like_download = contains_ci(href, "download")
                || contains_ci(href, "trainer")
                || contains_ci(href, "file")
                || contains_ci(&text, "download");
            if !looks_like_download || contains_ci(href, "javascript:") {
                continue;
            }
            let label = if text.is_empty() || contains_ci(&text, "img") {
                if contains_ci(table, "Early Access") {
                    "Early Access".to_owned()
                } else if contains_ci(table, "Auto") {
                    "Auto Update".to_owned()
                } else {
                    format!("Download #{}", versions.len() + 1)
                }
            } else {
                text
            };
            versions.push((label, href.to_owned()));
        }
    }

    if versions.is_empty()
        && let Some(section) = DOWNLOAD_DIV.captures(html)
    {
        for link in LINK.captures_iter(&section[1]) {
            let text = strip_tags(link[2].trim());
            let label = if text.is_empty() || contains_ci(&text, "img") {
                format!("Download #{}", versions.len() + 1)
            } else {
                text
            };
            versions.push((label, link[1].to_owned()));
        }
    }

    for link in ATTACHMENT_LINK.captures_iter(html) {
        let mut label = link[2].trim().to_owned();
        if label.is_empty() {
            // Look at the enclosing <div> for a hint, located the way Qt did:
            // by the first occurrence of the matched text.
            let whole = &link[0];
            let pos = html.find(whole).unwrap_or(0);
            let start = html[..pos].rfind("<div").unwrap_or(0);
            let end = html[pos..]
                .find("</div>")
                .map_or(html.len(), |i| pos + i + "</div>".len());
            let context = &html[start..end];
            label = if contains_ci(context, "Early Access") {
                "Early Access".to_owned()
            } else if contains_ci(context, "FLiNG") {
                "FLiNG Version".to_owned()
            } else {
                format!("Download #{}", versions.len() + 1)
            };
        }
        versions.push((label, link[1].to_owned()));
    }
    versions
}

fn screenshot_url(html: &str) -> String {
    if let Some(url) = SCREENSHOT.captures(html).map(|c| c[1].to_owned()) {
        return url;
    }
    ANY_IMG
        .captures_iter(html)
        .map(|c| c[1].to_owned())
        .find(|url| {
            !contains_ci(url, "icon")
                && !contains_ci(url, "logo")
                && (contains_ci(url, "screenshot")
                    || contains_ci(url, "image")
                    || url.to_lowercase().ends_with(".jpg")
                    || url.to_lowercase().ends_with(".png"))
        })
        .unwrap_or_default()
}

/// Everything a detail page offers. `name` and `url` are left empty; the
/// caller keeps the list row's.
pub fn parse_modifier_detail(html: &str) -> ModifierInfo {
    let options = parse_options(html);
    let mut options_count = OPTIONS_COUNT
        .captures(html)
        .and_then(|c| c[1].parse::<u32>().ok())
        .unwrap_or(0);
    if options_count == 0 {
        // Counts the category header lines too, as the Qt build did.
        options_count = options.len() as u32;
    }

    ModifierInfo {
        game_version: game_version(html),
        last_update: last_update(html),
        options_count,
        versions: download_versions(html)
            .into_iter()
            .map(|(label, url)| DownloadVersion {
                label: format_version_string(&label),
                url,
            })
            .collect(),
        options,
        screenshot_url: screenshot_url(html),
        ..ModifierInfo::default()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn version_cascade() {
        assert_eq!(
            game_version("<p>Game Version: v1.02-v1.05+ · Steam</p>"),
            "v1.02-v1.05+"
        );
        assert_eq!(game_version("<p>Version : 2.0\n</p>"), "2.0");
        assert_eq!(game_version("<title>Foo v1.3 Trainer</title>"), "v1.3");
        assert_eq!(
            game_version(r#"<div class="post-meta x"><span>Early Access</span></div>"#),
            "Early Access"
        );
        assert_eq!(
            game_version(r#"<div class="post-meta x">nothing</div>"#),
            ""
        );
        assert_eq!(
            game_version(r#"<div class="flex-content">Early Access</div>"#),
            "Early Access"
        );
        assert_eq!(game_version("<p>nothing here</p>"), "Latest");
        assert_eq!(
            game_version("<p>Game Version: v1</p> Early Access+"),
            "Early Access+"
        );
    }

    #[test]
    fn last_update_patterns() {
        assert_eq!(last_update("Last Updated: 2026.09.01<br>"), "2026.09.01");
        assert_eq!(
            last_update(r#"<td class="attachment-date">2026-09-01 </td>"#),
            "2026-09-01"
        );
        assert_eq!(last_update("none"), "");
    }

    #[test]
    fn download_links_from_table_then_attachment_links() {
        let html = r#"
<table class="da-attachments-table"><tr>
  <td><a class="attachment-link" href="https://flingtrainer.com/downloads/abc,,">Elden Ring v1.02 Plus 30 Trainer</a></td>
  <td><a href="javascript:void(0)">download</a></td>
  <td><a href="https://example.com/about/">About</a></td>
  <td><a href="https://flingtrainer.com/about/">About FLiNG</a></td>
</tr></table>"#;
        let label = "Elden Ring v1.02 Plus 30 Trainer".to_owned();
        let link = "https://flingtrainer.com/downloads/abc,,".to_owned();
        // Every flingtrainer.com URL contains "trainer", so the about link
        // counts as a download; and the table pass and the attachment-link
        // pass both see the first link. Both quirks are the Qt behavior.
        assert_eq!(
            download_versions(html),
            [
                (label.clone(), link.clone()),
                (
                    "About FLiNG".to_owned(),
                    "https://flingtrainer.com/about/".to_owned()
                ),
                (label, link),
            ]
        );
    }

    #[test]
    fn image_only_links_get_descriptive_labels() {
        let html = r#"<table><tr><td>Auto Update</td><td><a href="/downloads/x"><img src="d.png"></a></td></tr></table>"#;
        assert_eq!(
            download_versions(html),
            [("Auto Update".to_owned(), "/downloads/x".to_owned())]
        );

        let html = r#"<div class="download-box"><a href="/f/1"></a><a href="/f/2">v2.0</a></div>"#;
        assert_eq!(
            download_versions(html),
            [
                ("Download #1".to_owned(), "/f/1".to_owned()),
                ("v2.0".to_owned(), "/f/2".to_owned())
            ]
        );
    }

    #[test]
    fn empty_attachment_label_uses_enclosing_div() {
        let html =
            r#"<div class="x">FLiNG build <a class="attachment-link" href="/dl/1"></a></div>"#;
        assert_eq!(
            download_versions(html),
            [("FLiNG Version".to_owned(), "/dl/1".to_owned())]
        );
    }

    #[test]
    fn detail_formats_version_labels_and_counts_options() {
        let html = r#"<title>X</title><p>Game Version: v1.0</p>
<table><tr><td><a href="/downloads/a">1.02</a></td></tr></table>
<p>Num 1 – Infinite Health<br>Num 2 – Infinite Stamina<br></p>
<img src="https://flingtrainer.com/wp-content/uploads/logo.png">"#;
        let detail = parse_modifier_detail(html);
        assert_eq!(
            detail.versions,
            [DownloadVersion {
                label: "v1.02".into(),
                url: "/downloads/a".into()
            }]
        );
        // Two options plus the "● Basic Options" header.
        assert_eq!(detail.options_count, 3);
        assert_eq!(
            detail.screenshot_url,
            "https://flingtrainer.com/wp-content/uploads/logo.png"
        );
    }

    #[test]
    fn explicit_options_count_wins() {
        assert_eq!(parse_modifier_detail("Options: 24").options_count, 24);
    }

    #[test]
    fn screenshot_fallback_skips_icons() {
        let html = r#"<img src="/icon.webp"><img src="/images/shot.webp?x=1">"#;
        assert_eq!(screenshot_url(html), "/images/shot.webp?x=1");
    }
}
