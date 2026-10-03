//! FLiNG Downloader: the desktop frontend.
//!
//! This crate only presents state. Everything else lives behind
//! [`fling_app`]: the frontend starts the backend, mirrors its events into
//! [`state::AppModel`] and sends it commands.

#![windows_subsystem = "windows"]

rust_i18n::i18n!("locales", fallback = "en");

mod assets;
mod i18n;
mod state;
mod theme;
mod views;

use std::sync::Arc;

use fling_app::BackendConfig;
use gpui_kit::component::TitleBar;
use gpui_kit::*;

fn main() {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| tracing_subscriber::EnvFilter::new("info")),
        )
        .with_writer(std::io::stderr)
        .init();

    let version = env!("FLING_APP_VERSION").to_owned();
    // The cover model is wired in with the ONNX detector; until then covers
    // fall back to the "no cover" placeholder.
    let config =
        BackendConfig::for_app(version, Arc::new(|| None)).expect("failed to create HTTP client");
    let (handle, events) = fling_app::start(config).expect("failed to start backend");
    let initial = handle.initial_settings().clone();
    i18n::set_language(initial.language);

    gpui_kit::application()
        .with_assets(assets::Assets)
        .run(move |cx| {
            gpui_kit::init(cx);
            theme::apply(initial.theme, cx);
            let model = state::AppModel::new(handle, events, cx);

            let options = WindowOptions {
                window_bounds: Some(WindowBounds::centered(size(px(950.), px(650.)), cx)),
                window_min_size: Some(size(px(800.), px(600.))),
                titlebar: Some(TitlebarOptions {
                    title: Some("FLiNG Downloader".into()),
                    ..TitleBar::title_bar_options()
                }),
                app_id: Some("FLiNG Downloader".into()),
                ..TitleBar::window_options()
            };
            gpui_kit::open_window(options, cx, |window, cx| {
                cx.new(|cx| views::Root::new(model, window, cx))
            })
            .expect("failed to open window");
            cx.on_window_closed(|cx, _| {
                if cx.windows().is_empty() {
                    cx.quit();
                }
            })
            .detach();
        });
}
