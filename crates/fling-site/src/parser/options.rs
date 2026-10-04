//! Trainer option lists. Port of `ModifierParser::parseOptionsFromHTML`.

use std::collections::BTreeMap;

use super::{contains_ci, lazy_re, qt_len};

// The Qt pattern ended in the lookahead `(?=\s*<(?:span|script|br))`. Consuming
// that suffix instead captures the same text: what it eats (whitespace plus
// the start of a tag) can never begin the next hotkey.
lazy_re!(
    TOOLTIP_OPTION,
    r"(?is)((?:Num|Ctrl\+Num|Alt\+Num|Shift\+(?:Num|F\d))\s*[\d\+\-\.]+|(?:Ctrl|Alt|Shift)\+[\d\+\-\.]+)\s*(?:&#8211;|[-–])\s*([^<]+?)\s*<(?:span|script|br)"
);
lazy_re!(
    BR_OPTION,
    r"(?is)((?:Num|Ctrl\+Num|Alt\+Num|Shift\+(?:Num|F\d))\s*[\d\+\-\.]+|(?:Ctrl|Alt|Shift)\+[\d\+\-\.]+)\s*(?:&#8211;|[-–])\s*([^<]+?)\s*<br"
);
lazy_re!(WHITESPACE, r"\s+");

fn clean_description(raw: &str) -> String {
    raw.trim()
        .replace("&amp;", "&")
        .replace("&#8211;", "-")
        .replace("&#046;", ".")
        .replace("&#8217;", "'")
        .trim()
        .to_owned()
}

fn is_valid(description: &str, reject_tooltip: bool) -> bool {
    !description.is_empty()
        && !contains_ci(description, "jQuery")
        && !contains_ci(description, "function")
        && !contains_ci(description, "script")
        && !(reject_tooltip && contains_ci(description, "tooltip"))
        && qt_len(description) > 2
}

/// Option lines grouped under `● Basic Options`, `● Ctrl Hotkey Options`,
/// `● Alt Hotkey Options` and `● Shift Hotkey Options` headers, each line
/// formatted `• {hotkey} – {description}`.
///
/// Within a group lines are in hotkey string order ("Num 1" < "Num 10" <
/// "Num 2"), as the Qt `QMap` produced them.
pub fn parse_options(html: &str) -> Vec<String> {
    let mut options: BTreeMap<String, String> = BTreeMap::new();
    let hotkey_of = |raw: &str| WHITESPACE.replace_all(raw.trim(), " ").into_owned();

    for caps in TOOLTIP_OPTION.captures_iter(html) {
        let description = clean_description(&caps[2]);
        if is_valid(&description, true) {
            options.insert(hotkey_of(&caps[1]), description);
        }
    }
    for caps in BR_OPTION.captures_iter(html) {
        let description = clean_description(&caps[2]);
        if !is_valid(&description, false) {
            continue;
        }
        let hotkey = hotkey_of(&caps[1]);
        // Only replace a description with a longer one.
        if options
            .get(&hotkey)
            .is_none_or(|current| qt_len(current) < qt_len(&description))
        {
            options.insert(hotkey, description);
        }
    }

    let mut groups: [(&str, Vec<String>); 4] = [
        ("● Basic Options", Vec::new()),
        ("● Ctrl Hotkey Options", Vec::new()),
        ("● Alt Hotkey Options", Vec::new()),
        ("● Shift Hotkey Options", Vec::new()),
    ];
    for (hotkey, description) in &options {
        let lower = hotkey.to_lowercase();
        let group = if lower.starts_with("ctrl+") {
            1
        } else if lower.starts_with("alt+") {
            2
        } else if lower.starts_with("shift+") {
            3
        } else {
            0
        };
        groups[group].1.push(format!("• {hotkey} – {description}"));
    }

    groups
        .into_iter()
        .filter(|(_, lines)| !lines.is_empty())
        .flat_map(|(header, lines)| std::iter::once(header.to_owned()).chain(lines))
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn groups_and_orders_options() {
        let html = "<p>Num 1 – Infinite Health<br>Num 10 &#8211; Super Speed<br>\
            Num 2 - Infinite Stamina<span class=\"tooltip\">?</span>\
            Ctrl+Num 1 – Edit Gold &amp; Gems<br>Alt+1 – Set Game Speed<br>\
            Shift+F12 – One Hit Kill<br></p>";
        assert_eq!(
            parse_options(html),
            [
                "● Basic Options",
                "• Num 1 – Infinite Health",
                "• Num 10 – Super Speed",
                "• Num 2 – Infinite Stamina",
                "● Ctrl Hotkey Options",
                "• Ctrl+Num 1 – Edit Gold & Gems",
                "● Alt Hotkey Options",
                "• Alt+1 – Set Game Speed",
                "● Shift Hotkey Options",
                "• Shift+F12 – One Hit Kill",
            ]
        );
    }

    #[test]
    fn rejects_scripts_and_short_text() {
        let html = "Num 1 – ok<br>Num 2 – jQuery(function(){})<br>Num 3 – tooltip text<span>\
            Num 4 – A tooltip mention<br>";
        // "ok" is too short; jQuery/function rejected; the tooltip pass rejects
        // "tooltip text" but the <br> pass accepts "A tooltip mention".
        assert_eq!(
            parse_options(html),
            ["● Basic Options", "• Num 4 – A tooltip mention"]
        );
    }

    #[test]
    fn collapses_hotkey_whitespace() {
        assert_eq!(
            parse_options("Num  5 – Freeze Timer<br>")[1],
            "• Num 5 – Freeze Timer"
        );
        assert!(parse_options("").is_empty());
    }
}
