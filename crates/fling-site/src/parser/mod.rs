//! Regex ports of `ModifierParser.cpp` and the homepage parsers in
//! `SearchManager.cpp`.
//!
//! The Qt build ran these through PCRE without UCP, where `\s` and `\d` are
//! ASCII-only; [`re`] keeps that meaning so results match byte for byte.

mod detail;
mod homepage;
mod list;
mod options;

pub use detail::parse_modifier_detail;
pub use homepage::{parse_featured, parse_recently_updated};
pub use list::parse_modifier_list;
pub use options::parse_options;

use regex::Regex;

/// Compiles `pattern` with PCRE-compatible ASCII `\s` / `\d`.
pub(crate) fn re(pattern: &str) -> Regex {
    let ascii = pattern
        .replace(r"\s", r"[\t\n\x0B\x0C\r ]")
        .replace(r"\d", "[0-9]");
    Regex::new(&ascii).unwrap_or_else(|err| panic!("invalid pattern {pattern:?}: {err}"))
}

/// Declares a lazily compiled [`re`] pattern.
macro_rules! lazy_re {
    ($name:ident, $pattern:expr) => {
        static $name: std::sync::LazyLock<regex::Regex> =
            std::sync::LazyLock::new(|| $crate::parser::re($pattern));
    };
}
pub(crate) use lazy_re;

/// Port of `decodeHtmlEntities`: only the handful of entities the site uses.
pub fn decode_html_entities(text: &str) -> String {
    const ENTITIES: [(&str, &str); 12] = [
        ("&amp;", "&"),
        ("&lt;", "<"),
        ("&gt;", ">"),
        ("&quot;", "\""),
        ("&#39;", "'"),
        ("&apos;", "'"),
        ("&#8217;", "'"),
        ("&#8216;", "'"),
        ("&#8220;", "\""),
        ("&#8221;", "\""),
        ("&#8211;", "-"),
        ("&#8212;", "—"),
    ];
    ENTITIES
        .iter()
        .fold(text.to_owned(), |acc, (entity, replacement)| {
            acc.replace(entity, replacement)
        })
}

/// Case-insensitive `contains` (Qt's `contains(.., Qt::CaseInsensitive)`).
pub(crate) fn contains_ci(haystack: &str, needle: &str) -> bool {
    haystack.to_lowercase().contains(&needle.to_lowercase())
}

/// Length in UTF-16 code units, as `QString::length`.
pub(crate) fn qt_len(text: &str) -> usize {
    text.encode_utf16().count()
}

/// `yyyy-MM-dd` from the site's day/month/year blocks. Unknown months map to
/// `01`, as in the Qt build.
pub(crate) fn format_post_date(year: &str, month_number: &str, day: &str) -> String {
    format!("{year}-{month_number}-{day:0>2}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn re_keeps_whitespace_and_digits_ascii() {
        let pattern = re(r"a\s+\d");
        assert!(pattern.is_match("a \t7"));
        assert!(!pattern.is_match("a\u{a0}7"));
        assert!(!pattern.is_match("a ٣"));
    }

    #[test]
    fn decodes_site_entities_only() {
        assert_eq!(
            decode_html_entities("Tom &amp; Jerry&#8217;s &#8211; &copy;"),
            "Tom & Jerry's - &copy;"
        );
    }
}
