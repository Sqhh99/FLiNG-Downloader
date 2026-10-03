//! GPUI views. Each reads [`crate::state::AppModel`] and sends commands
//! through it; none of them talks to the backend directly.

mod detail_drawer;
mod downloads_panel;
mod library_page;
mod root;
mod search_page;
mod settings_panel;
mod widgets;

pub use root::Root;
