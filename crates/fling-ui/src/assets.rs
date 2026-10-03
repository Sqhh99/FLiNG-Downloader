//! Asset source: the Lucide icons the app uses on top of GPUI Kit's default
//! icon set, plus the app's own images.

use std::borrow::Cow;

use gpui_kit::{AssetSource, Result, SharedString};

// Lucide icons outside GPUI Kit's default set.
gpui_kit::assets::icon_assets!(
    AppIcons,
    [
        Download, Trash, Languages, Palette, ImageOff, FolderOpen, RotateCw, Gamepad2, Library,
        ListVideo
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
