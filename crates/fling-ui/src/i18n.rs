//! UI strings (`locales/app.yml`) and display formatting.

use fling_app::{Language, TaskStatus, UpdateError, UpdateSource, UpdateState, UpdateStatus};
use gpui_kit::SharedString;

/// Looks up a UI string, with optional `%{name}` arguments.
macro_rules! tr {
    ($key:expr) => {
        gpui_kit::SharedString::from(rust_i18n::t!($key).into_owned())
    };
    ($key:expr, $($name:ident = $value:expr),+ $(,)?) => {
        gpui_kit::SharedString::from(rust_i18n::t!($key, $($name = $value),+).into_owned())
    };
}
pub(crate) use tr;

pub fn locale_of(language: Language) -> &'static str {
    match language {
        Language::Chinese => "zh-CN",
        Language::English => "en",
        Language::Japanese => "ja",
    }
}

pub fn set_language(language: Language) {
    rust_i18n::set_locale(locale_of(language));
}

pub fn language_name(language: Language) -> SharedString {
    match language {
        Language::Chinese => tr!("lang.zh"),
        Language::English => tr!("lang.en"),
        Language::Japanese => tr!("lang.ja"),
    }
}

pub fn theme_name(index: usize) -> SharedString {
    SharedString::from(rust_i18n::t!(format!("theme.{index}")).into_owned())
}

pub fn source_name(source: UpdateSource) -> &'static str {
    match source {
        UpdateSource::GitHub => "GitHub",
        UpdateSource::Gitee => "Gitee",
    }
}

pub fn task_status(status: TaskStatus) -> SharedString {
    match status {
        TaskStatus::Queued => tr!("status.queued"),
        TaskStatus::Downloading => tr!("status.downloading"),
        TaskStatus::Paused => tr!("status.paused"),
        TaskStatus::Completed => tr!("status.completed"),
        TaskStatus::Failed => tr!("status.failed"),
        TaskStatus::Canceled => tr!("status.canceled"),
    }
}

/// The status line of an update card. `database` picks the `db.*` strings.
pub fn update_status(state: &UpdateState, database: bool) -> SharedString {
    let key = |name: &str| format!("{}.{name}", if database { "db" } else { "update" });
    let simple = |name: &str| SharedString::from(rust_i18n::t!(key(name)).into_owned());
    let version = state.latest_version.as_str();
    match &state.status {
        UpdateStatus::Idle => simple("idle"),
        UpdateStatus::Checking => simple("checking"),
        UpdateStatus::UpToDate => simple("up_to_date"),
        UpdateStatus::Available => rust_i18n::t!(key("available"), version = version)
            .into_owned()
            .into(),
        UpdateStatus::CheckFailed(UpdateError::RequestFailed(_, source)) => {
            rust_i18n::t!(key("request_failed"), source = source_name(*source))
                .into_owned()
                .into()
        }
        UpdateStatus::CheckFailed(UpdateError::NoValidAsset(_, source)) => {
            rust_i18n::t!(key("no_asset"), source = source_name(*source))
                .into_owned()
                .into()
        }
        UpdateStatus::CheckFailed(other) => other.to_string().into(),
        UpdateStatus::Downloading => simple("downloading"),
        UpdateStatus::DownloadFailed(error) => rust_i18n::t!(key("download_failed"), error = error)
            .into_owned()
            .into(),
        UpdateStatus::LaunchingInstaller => tr!("update.launching"),
        UpdateStatus::LaunchFailed => tr!("update.launch_failed"),
        UpdateStatus::Installed => tr!("db.installed", version = state.current_version.as_str()),
        UpdateStatus::InstallFailed(error) => tr!("db.install_failed", error = error),
        UpdateStatus::ReloadFailed => tr!("db.reload_failed"),
    }
}

/// `B`, `KB`, `MB`, `GB` with one decimal, as the Qt download popup.
pub fn format_bytes(bytes: u64) -> String {
    const UNITS: [&str; 4] = ["B", "KB", "MB", "GB"];
    let mut value = bytes as f64;
    let mut unit = 0;
    while value >= 1024.0 && unit < UNITS.len() - 1 {
        value /= 1024.0;
        unit += 1;
    }
    if unit == 0 {
        format!("{bytes} B")
    } else {
        format!("{value:.1} {}", UNITS[unit])
    }
}

pub fn format_speed(bytes_per_second: u64) -> String {
    format!("{}/s", format_bytes(bytes_per_second))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn formats_sizes() {
        assert_eq!(format_bytes(512), "512 B");
        assert_eq!(format_bytes(1536), "1.5 KB");
        assert_eq!(format_bytes(5 * 1024 * 1024), "5.0 MB");
        assert_eq!(format_speed(2048), "2.0 KB/s");
    }

    #[test]
    fn every_locale_has_every_key() {
        let yaml = include_str!("../locales/app.yml");
        let mut key = None;
        let mut seen = Vec::new();
        for line in yaml.lines() {
            if line.starts_with(' ') {
                if let Some(locale) = line.trim().split(':').next() {
                    seen.push(locale.to_owned());
                }
            } else if let Some(k) = line.strip_suffix(':') {
                if let Some(prev) = key.replace(k.to_owned()) {
                    check(&prev, &seen);
                }
                seen.clear();
            }
        }
        if let Some(last) = key {
            check(&last, &seen);
        }
        fn check(key: &str, seen: &[String]) {
            for locale in ["zh-CN", "en", "ja"] {
                assert!(seen.iter().any(|s| s == locale), "{key} lacks {locale}");
            }
        }
    }
}
