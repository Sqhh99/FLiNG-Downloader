//! A round-tripping reader/writer for Qt's `QSettings::IniFormat` files.
//!
//! Only what this app stores is interpreted (plain strings, booleans,
//! integers); every other section and key is kept verbatim, so a file written
//! by the Qt build survives being rewritten by this one. Value escaping
//! follows `QSettingsPrivate::iniEscapedString` / `iniUnescapedStringList`.

use std::fmt::Write as _;

#[derive(Debug, Clone, PartialEq, Eq)]
struct Section {
    name: String,
    /// `(key, raw value)`, in file order.
    entries: Vec<(String, String)>,
}

/// An INI document, edited in place.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct IniDocument {
    sections: Vec<Section>,
}

/// Top-level keys live in `[General]`, as Qt writes them.
pub const GENERAL: &str = "General";

impl IniDocument {
    pub fn parse(text: &str) -> Self {
        let mut doc = Self::default();
        let mut current: Option<usize> = None;
        for line in text.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with(';') || line.starts_with('#') {
                continue;
            }
            if let Some(name) = line.strip_prefix('[').and_then(|l| l.strip_suffix(']')) {
                current = Some(doc.section_index_or_insert(name.trim()));
                continue;
            }
            let Some((key, value)) = line.split_once('=') else {
                continue;
            };
            let index = *current.get_or_insert_with(|| doc.section_index_or_insert(GENERAL));
            doc.sections[index]
                .entries
                .push((key.trim().to_owned(), value.trim().to_owned()));
        }
        doc
    }

    fn section_index_or_insert(&mut self, name: &str) -> usize {
        if let Some(i) = self.sections.iter().position(|s| s.name == name) {
            return i;
        }
        self.sections.push(Section {
            name: name.to_owned(),
            entries: Vec::new(),
        });
        self.sections.len() - 1
    }

    fn raw(&self, section: &str, key: &str) -> Option<&str> {
        self.sections
            .iter()
            .find(|s| s.name == section)?
            .entries
            .iter()
            .rev()
            .find(|(k, _)| k == key)
            .map(|(_, v)| v.as_str())
    }

    fn set_raw(&mut self, section: &str, key: &str, raw: String) {
        let index = self.section_index_or_insert(section);
        let entries = &mut self.sections[index].entries;
        match entries.iter_mut().find(|(k, _)| k == key) {
            Some(entry) => entry.1 = raw,
            None => entries.push((key.to_owned(), raw)),
        }
    }

    /// The value as a string; `None` when the key is absent. A Qt string list
    /// with more than one element reads as `None` (as `QVariant::toString`
    /// would give an empty string for it).
    pub fn get_string(&self, section: &str, key: &str) -> Option<String> {
        let mut items = unescape_list(self.raw(section, key)?);
        if items.len() != 1 {
            return None;
        }
        let value = items.remove(0);
        Some(match value.strip_prefix('@') {
            Some(rest) if rest.starts_with('@') => rest.to_owned(),
            _ => value,
        })
    }

    /// `QVariant::toBool` on a string: false for empty, `0` or `false`.
    pub fn get_bool(&self, section: &str, key: &str) -> Option<bool> {
        let value = self.get_string(section, key)?;
        let value = value.trim();
        Some(!(value.is_empty() || value == "0" || value.eq_ignore_ascii_case("false")))
    }

    /// `QVariant::toInt` on a string: 0 when it does not parse.
    pub fn get_int(&self, section: &str, key: &str) -> Option<i64> {
        Some(self.get_string(section, key)?.trim().parse().unwrap_or(0))
    }

    pub fn set_string(&mut self, section: &str, key: &str, value: &str) {
        // Qt doubles a leading '@' so the value is not read back as a variant.
        let value = if value.starts_with('@') {
            format!("@{value}")
        } else {
            value.to_owned()
        };
        self.set_raw(section, key, escape(&value));
    }

    pub fn set_bool(&mut self, section: &str, key: &str, value: bool) {
        self.set_raw(section, key, value.to_string());
    }

    pub fn set_int(&mut self, section: &str, key: &str, value: i64) {
        self.set_raw(section, key, value.to_string());
    }

    /// Serializes with `[General]` first, like Qt.
    pub fn to_ini_string(&self) -> String {
        let mut out = String::new();
        let general = self.sections.iter().filter(|s| s.name == GENERAL);
        let others = self.sections.iter().filter(|s| s.name != GENERAL);
        for section in general.chain(others).filter(|s| !s.entries.is_empty()) {
            if !out.is_empty() {
                out.push('\n');
            }
            let _ = writeln!(out, "[{}]", section.name);
            for (key, value) in &section.entries {
                let _ = writeln!(out, "{key}={value}");
            }
        }
        out
    }
}

/// Port of `iniEscapedString` for UTF-8 output.
fn escape(value: &str) -> String {
    let mut out = String::with_capacity(value.len() + 2);
    let mut needs_quotes = false;
    let mut escape_next_if_hex = false;
    for c in value.chars() {
        if matches!(c, ';' | ',' | '=') {
            needs_quotes = true;
        }
        if escape_next_if_hex && c.is_ascii_hexdigit() {
            let _ = write!(out, "\\x{:x}", c as u32);
            continue;
        }
        escape_next_if_hex = false;
        match c {
            '\0' => {
                out.push_str("\\0");
                escape_next_if_hex = true;
            }
            '\x07' => out.push_str("\\a"),
            '\x08' => out.push_str("\\b"),
            '\x0C' => out.push_str("\\f"),
            '\n' => out.push_str("\\n"),
            '\r' => out.push_str("\\r"),
            '\t' => out.push_str("\\t"),
            '\x0B' => out.push_str("\\v"),
            '"' | '\\' => {
                out.push('\\');
                out.push(c);
            }
            c if (c as u32) <= 0x1F => {
                let _ = write!(out, "\\x{:x}", c as u32);
                escape_next_if_hex = true;
            }
            c => out.push(c),
        }
    }
    if needs_quotes || out.starts_with(' ') || out.ends_with(' ') {
        format!("\"{out}\"")
    } else {
        out
    }
}

/// Port of `iniUnescapedStringList`: quotes group text, backslash escapes are
/// decoded, and unquoted commas split a list. Unquoted surrounding spaces are
/// trimmed from each item.
fn unescape_list(raw: &str) -> Vec<String> {
    let mut items = Vec::new();
    let mut current = String::new();
    // Length of `current` that must survive trimming (ends at the last quoted
    // or escaped character).
    let mut protected_len = 0;
    let mut in_quotes = false;
    let mut chars = raw.chars().peekable();

    let finish = |current: &mut String, protected_len: &mut usize, items: &mut Vec<String>| {
        let keep = (*protected_len).min(current.len());
        let tail = current[keep..].trim_end().len();
        current.truncate(keep + tail);
        items.push(std::mem::take(current));
        *protected_len = 0;
    };

    while let Some(c) = chars.next() {
        match c {
            '"' => {
                in_quotes = !in_quotes;
                protected_len = current.len();
            }
            ',' if !in_quotes => finish(&mut current, &mut protected_len, &mut items),
            ' ' | '\t' if !in_quotes && current.len() == protected_len && current.is_empty() => {}
            '\\' => {
                match chars.next() {
                    Some('a') => current.push('\x07'),
                    Some('b') => current.push('\x08'),
                    Some('f') => current.push('\x0C'),
                    Some('n') => current.push('\n'),
                    Some('r') => current.push('\r'),
                    Some('t') => current.push('\t'),
                    Some('v') => current.push('\x0B'),
                    Some('x') => {
                        let mut code = 0u32;
                        while let Some(d) = chars.peek().and_then(|d| d.to_digit(16)) {
                            code = code.saturating_mul(16).saturating_add(d);
                            chars.next();
                        }
                        current.extend(char::from_u32(code));
                    }
                    Some(d @ '0'..='7') => {
                        let mut code = d.to_digit(8).unwrap_or(0);
                        while let Some(d) = chars.peek().and_then(|d| d.to_digit(8)) {
                            code = code.saturating_mul(8).saturating_add(d);
                            chars.next();
                        }
                        current.extend(char::from_u32(code));
                    }
                    Some(other) => current.push(other),
                    None => {}
                }
                protected_len = current.len();
            }
            c => {
                current.push(c);
                if in_quotes {
                    protected_len = current.len();
                }
            }
        }
    }
    finish(&mut current, &mut protected_len, &mut items);
    items
}

#[cfg(test)]
mod tests {
    use super::*;

    const QT_FILE: &str = "[General]\ncurrentTheme=3\ncurrentLanguage=0\nupdateSource=github\n\
        downloadDirectory=D:/Games/Trainers\n\n[SearchManager]\nSearchHistory=Red Dead, 只狼, D4：暗梦\n\n\
        [meta]\nlegacySettingsMigrated=true\n";

    #[test]
    fn reads_qt_written_values() {
        let doc = IniDocument::parse(QT_FILE);
        assert_eq!(doc.get_int(GENERAL, "currentTheme"), Some(3));
        assert_eq!(
            doc.get_string(GENERAL, "updateSource").as_deref(),
            Some("github")
        );
        assert_eq!(
            doc.get_string(GENERAL, "downloadDirectory").as_deref(),
            Some("D:/Games/Trainers")
        );
        assert_eq!(doc.get_bool("meta", "legacySettingsMigrated"), Some(true));
        // A multi-item list is not a string.
        assert_eq!(doc.get_string("SearchManager", "SearchHistory"), None);
        assert_eq!(doc.get_string(GENERAL, "missing"), None);
    }

    #[test]
    fn round_trip_preserves_unknown_sections() {
        let mut doc = IniDocument::parse(QT_FILE);
        doc.set_int(GENERAL, "currentTheme", 5);
        doc.set_bool(GENERAL, "autoCheckUpdates", false);
        let text = doc.to_ini_string();
        assert!(text.starts_with("[General]\ncurrentTheme=5\n"), "{text}");
        assert!(text.contains("autoCheckUpdates=false\n"));
        assert!(text.contains("SearchHistory=Red Dead, 只狼, D4：暗梦\n"));
        assert!(text.contains("[meta]\nlegacySettingsMigrated=true\n"));
        assert_eq!(IniDocument::parse(&text), doc);
    }

    #[test]
    fn escapes_like_qt() {
        assert_eq!(escape(r"D:\Games, Trainers"), r#""D:\\Games, Trainers""#);
        assert_eq!(escape(" padded "), "\" padded \"");
        assert_eq!(escape("tab\there"), r"tab\there");
        assert_eq!(escape("say \"hi\""), r#"say \"hi\""#);
        assert_eq!(escape("下载"), "下载");
    }

    #[test]
    fn string_values_round_trip() {
        for value in [
            r"C:\Users\me\Downloads",
            "a, b; c=d",
            " lead",
            "@at",
            "q\"uote",
            "日本語",
            "x\u{1}1",
        ] {
            let mut doc = IniDocument::default();
            doc.set_string(GENERAL, "k", value);
            let reparsed = IniDocument::parse(&doc.to_ini_string());
            assert_eq!(
                reparsed.get_string(GENERAL, "k").as_deref(),
                Some(value),
                "{value:?}"
            );
        }
    }

    #[test]
    fn bool_and_int_conversions_match_qvariant() {
        let doc = IniDocument::parse("[General]\na=false\nb=0\nc=yes\nd=\ne=abc\nf=7\n");
        assert_eq!(doc.get_bool(GENERAL, "a"), Some(false));
        assert_eq!(doc.get_bool(GENERAL, "b"), Some(false));
        assert_eq!(doc.get_bool(GENERAL, "c"), Some(true));
        assert_eq!(doc.get_bool(GENERAL, "d"), Some(false));
        assert_eq!(doc.get_int(GENERAL, "e"), Some(0));
        assert_eq!(doc.get_int(GENERAL, "f"), Some(7));
    }

    #[test]
    fn keys_before_any_section_go_to_general() {
        let doc = IniDocument::parse("currentTheme=2\n[Other]\nx=1\n");
        assert_eq!(doc.get_int(GENERAL, "currentTheme"), Some(2));
    }
}
