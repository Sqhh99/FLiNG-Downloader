#![windows_subsystem = "windows"]

use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::{TitleBar, v_flex};
use gpui_kit::*;

struct Spike;

impl Render for Spike {
    fn render(&mut self, _: &mut Window, _: &mut Context<Self>) -> impl IntoElement {
        v_flex()
            .size_full()
            .child(TitleBar::new().child("FLiNG Downloader"))
            .child(
                v_flex()
                    .flex_1()
                    .items_center()
                    .justify_center()
                    .gap_2()
                    .child("GPUI spike")
                    .child(Button::new("ok").primary().label("OK")),
            )
    }
}

fn main() {
    gpui_kit::application()
        .with_assets(gpui_kit::assets::Assets)
        .run(|cx| {
            gpui_kit::init(cx);
            let options = WindowOptions {
                window_bounds: Some(WindowBounds::centered(size(px(950.), px(650.)), cx)),
                window_min_size: Some(size(px(800.), px(600.))),
                ..TitleBar::window_options()
            };
            gpui_kit::open_window(options, cx, |_, cx| cx.new(|_| Spike))
                .expect("failed to open window");
        });
}
