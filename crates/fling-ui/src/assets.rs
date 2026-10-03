//! Asset source: the Lucide icons the app uses on top of GPUI Kit's default
//! icon set, plus the app's own images.

use std::borrow::Cow;

use gpui_kit::{AssetSource, Result, SharedString};

// Every Lucide icon the views use, registered explicitly: GPUI Kit's default
// set lacks some (e.g. `x`), and a missing SVG renders as an empty box. The
// test below fails when a view uses an icon that is not listed here.
gpui_kit::assets::icon_assets!(
    AppIcons,
    [
        Check, Download, FolderOpen, Github, ImageOff, Info, Languages, Palette, Pause, Play,
        RotateCw, Settings, Trash, X,
    ]
);

#[derive(rust_embed::RustEmbed)]
#[folder = "../../resources/icons"]
#[include = "app_icon.png"]
#[prefix = "app/"]
struct AppImages;

pub struct Assets;

impl AssetSource for Assets {
    fn load(&self, path: &str) -> Result<Option<Cow<'static, [u8]>>> {
        if let Some(found) = AppIcons.load(path)? {
            return Ok(Some(found));
        }
        if let Some(file) = AppImages::get(path) {
            return Ok(Some(file.data));
        }
        gpui_kit::assets::Assets.load(path)
    }

    fn list(&self, path: &str) -> Result<Vec<SharedString>> {
        let mut all = AppIcons.list(path)?;
        all.extend(
            AppImages::iter()
                .filter(|p| p.starts_with(path))
                .map(|p| SharedString::from(p.to_string())),
        );
        all.extend(gpui_kit::assets::Assets.list(path)?);
        Ok(all)
    }
}

/// Path of the app logo within [`Assets`].
pub const APP_LOGO: &str = "app/app_icon.png";

#[cfg(test)]
mod tests {
    use super::*;

    fn kebab(name: &str) -> String {
        let mut out = String::new();
        for (i, c) in name.chars().enumerate() {
            if c.is_ascii_uppercase() && i > 0 {
                out.push('-');
            }
            out.push(c.to_ascii_lowercase());
        }
        out
    }

    #[test]
    fn every_icon_used_by_a_view_is_registered() {
        let dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("src/views");
        let pattern = regex_lite("IconName::");
        for entry in std::fs::read_dir(dir).unwrap() {
            let source = std::fs::read_to_string(entry.unwrap().path()).unwrap();
            for name in pattern(&source) {
                let path = format!("icons/{}.svg", kebab(&name));
                assert!(
                    AppIcons.load(&path).unwrap().is_some(),
                    "{name} ({path}) is not in AppIcons"
                );
            }
        }
    }

    /// Identifiers following `prefix` in `source`.
    fn regex_lite(prefix: &'static str) -> impl Fn(&str) -> Vec<String> {
        move |source| {
            source
                .match_indices(prefix)
                .map(|(i, _)| {
                    source[i + prefix.len()..]
                        .chars()
                        .take_while(|c| c.is_ascii_alphanumeric())
                        .collect::<String>()
                })
                .filter(|name| !name.is_empty())
                .collect()
        }
    }
}
