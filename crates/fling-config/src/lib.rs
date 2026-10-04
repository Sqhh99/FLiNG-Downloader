//! Where the app keeps its files, and the user's settings.
//!
//! Paths and the `settings.ini` format match the Qt build exactly, so a user
//! upgrading keeps their settings, library, cover cache and database override.

mod paths;
mod qsettings;
mod settings;

pub use paths::AppPaths;
pub use qsettings::IniDocument;
pub use settings::{Settings, THEME_COUNT};
