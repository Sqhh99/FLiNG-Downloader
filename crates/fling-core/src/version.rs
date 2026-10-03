//! Release-version handling shared by the app updater and the translation database.
//!
//! Semantics follow the Qt build's `AppUpdateManager::compareVersions`: a strict
//! `MAJOR.MINOR.PATCH[-pre][+build]` grammar, with a case-insensitive string
//! comparison as the fallback when either side does not parse.

use std::cmp::Ordering;

/// Trims and strips one leading `v`/`V`.
pub fn normalize_version(version: &str) -> &str {
    let trimmed = version.trim();
    trimmed.strip_prefix(['v', 'V']).unwrap_or(trimmed)
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct ParsedVersion<'a> {
    major: i64,
    minor: i64,
    patch: i64,
    prerelease: Vec<&'a str>,
}

fn parse_version(version: &str) -> Option<ParsedVersion<'_>> {
    let normalized = normalize_version(version);
    if normalized.is_empty() {
        return None;
    }
    let without_build = normalized.split('+').next().unwrap_or_default();
    let (core, prerelease) = match without_build.split_once('-') {
        Some((core, pre)) => (core, pre),
        None => (without_build, ""),
    };

    let mut parts = core.split('.');
    let (Some(major), Some(minor), Some(patch), None) =
        (parts.next(), parts.next(), parts.next(), parts.next())
    else {
        return None;
    };
    let number = |s: &str| s.trim().parse::<i64>().ok();
    Some(ParsedVersion {
        major: number(major)?,
        minor: number(minor)?,
        patch: number(patch)?,
        prerelease: prerelease.split('.').filter(|p| !p.is_empty()).collect(),
    })
}

/// Whether `version` is a valid `MAJOR.MINOR.PATCH[-pre][+build]` string.
pub fn is_valid_version(version: &str) -> bool {
    parse_version(version).is_some()
}

fn compare_case_insensitive(lhs: &str, rhs: &str) -> Ordering {
    lhs.to_lowercase().cmp(&rhs.to_lowercase())
}

fn is_numeric(token: &str) -> bool {
    !token.is_empty() && token.bytes().all(|b| b.is_ascii_digit())
}

/// Compares two release versions. A prerelease sorts below its release;
/// `+build` metadata is ignored.
pub fn compare_versions(lhs: &str, rhs: &str) -> Ordering {
    let (Some(left), Some(right)) = (parse_version(lhs), parse_version(rhs)) else {
        return compare_case_insensitive(normalize_version(lhs), normalize_version(rhs));
    };

    let core = (left.major, left.minor, left.patch).cmp(&(right.major, right.minor, right.patch));
    if core != Ordering::Equal {
        return core;
    }

    match (left.prerelease.is_empty(), right.prerelease.is_empty()) {
        (true, true) => return Ordering::Equal,
        (false, true) => return Ordering::Less,
        (true, false) => return Ordering::Greater,
        (false, false) => {}
    }

    for i in 0..left.prerelease.len().max(right.prerelease.len()) {
        let (Some(l), Some(r)) = (left.prerelease.get(i), right.prerelease.get(i)) else {
            return left.prerelease.len().cmp(&right.prerelease.len());
        };
        if l == r {
            continue;
        }
        let ordering = match (is_numeric(l), is_numeric(r)) {
            (true, true) => l
                .parse::<u128>()
                .unwrap_or(0)
                .cmp(&r.parse::<u128>().unwrap_or(0)),
            (true, false) => Ordering::Less,
            (false, true) => Ordering::Greater,
            (false, false) => compare_case_insensitive(l, r),
        };
        if ordering != Ordering::Equal {
            return ordering;
        }
    }
    Ordering::Equal
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cmp::Ordering::*;

    #[test]
    fn handles_prerelease_and_build_metadata() {
        assert_eq!(compare_versions("1.2.3-beta.1", "1.2.3"), Less);
        assert_eq!(compare_versions("1.2.3+build.5", "1.2.3"), Equal);
        assert_eq!(compare_versions("1.2.4", "1.2.3"), Greater);
    }

    #[test]
    fn prerelease_tokens_compare_numerically_then_textually() {
        assert_eq!(compare_versions("1.0.0-dev.2", "1.0.0-dev.10"), Less);
        assert_eq!(compare_versions("1.0.0-1", "1.0.0-alpha"), Less);
        assert_eq!(compare_versions("1.0.0-alpha", "1.0.0-alpha.1"), Less);
        assert_eq!(compare_versions("1.0.0-ALPHA", "1.0.0-alpha"), Equal);
        assert_eq!(compare_versions("v1.1.10-dev.3+g1234", "1.1.10"), Less);
    }

    #[test]
    fn falls_back_to_case_insensitive_string_compare() {
        assert_eq!(compare_versions("v0.0.3", "0.0.3"), Equal);
        assert_eq!(compare_versions("release-b", "Release-A"), Greater);
        assert!(!is_valid_version("1.2"));
        assert!(is_valid_version("V1.2.3-rc.1+abc"));
    }

    #[test]
    fn normalize_strips_single_prefix() {
        assert_eq!(normalize_version("  v1.2.3 "), "1.2.3");
        assert_eq!(normalize_version("vv1"), "v1");
    }
}
