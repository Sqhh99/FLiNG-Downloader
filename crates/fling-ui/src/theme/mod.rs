//! The app's nine palettes (from the Qt build) mapped onto the GPUI Kit theme.
//!
//! Components get their colors from [`gpui_kit::component::Theme`]; the app's
//! own views read the original tokens through [`palette`].

mod palettes;

use gpui_kit::component::{Theme, ThemeMode};
use gpui_kit::{App, Global, Hsla, Rgba, px, rgb};

/// One theme, in the Qt build's token names (`0xRRGGBB`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Palette {
    pub is_dark: bool,
    pub primary_color: u32,
    pub secondary_color: u32,
    pub background_color: u32,
    pub surface_color: u32,
    pub card_color: u32,
    pub input_background: u32,
    pub text_primary: u32,
    pub text_secondary: u32,
    pub text_disabled: u32,
    pub primary_text_color: u32,
    pub text_muted: u32,
    pub border_color: u32,
    pub hover_color: u32,
    pub selected_color: u32,
    pub alternate_row_color: u32,
    pub disabled_color: u32,
}

pub const THEME_COUNT: usize = palettes::PALETTES.len();

pub const SUCCESS: u32 = 0x4CAF50;
pub const WARNING: u32 = 0xFF9800;
pub const DANGER: u32 = 0xF44336;
pub const INFO: u32 = 0x2196F3;

/// Colors of the active palette, ready for styling.
#[derive(Debug, Clone, Copy)]
pub struct Colors {
    pub is_dark: bool,
    pub primary: Hsla,
    pub background: Hsla,
    pub surface: Hsla,
    pub card: Hsla,
    pub input: Hsla,
    pub text: Hsla,
    pub text_secondary: Hsla,
    pub primary_text: Hsla,
    pub text_muted: Hsla,
    pub border: Hsla,
    pub hover: Hsla,
    pub selected: Hsla,
    pub alternate_row: Hsla,
    pub disabled: Hsla,
    pub success: Hsla,
    pub warning: Hsla,
    pub danger: Hsla,
    pub info: Hsla,
    /// Text on a `primary` fill.
    pub on_primary: Hsla,
}

fn hsla(color: u32) -> Hsla {
    rgb(color).into()
}

/// The Qt build's `contrastOn`: dark text on light fills, white on dark ones.
fn contrast_on(color: u32) -> Hsla {
    let c: Rgba = rgb(color);
    let y = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
    if y > 0.55 {
        hsla(0x1A1A1A)
    } else {
        hsla(0xFFFFFF)
    }
}

fn shade(color: Hsla, by: f32) -> Hsla {
    Hsla {
        l: (color.l + by).clamp(0.0, 1.0),
        ..color
    }
}

impl Palette {
    fn colors(&self) -> Colors {
        Colors {
            is_dark: self.is_dark,
            primary: hsla(self.primary_color),
            background: hsla(self.background_color),
            surface: hsla(self.surface_color),
            card: hsla(self.card_color),
            input: hsla(self.input_background),
            text: hsla(self.text_primary),
            text_secondary: hsla(self.text_secondary),
            primary_text: hsla(self.primary_text_color),
            text_muted: hsla(self.text_muted),
            border: hsla(self.border_color),
            hover: hsla(self.hover_color),
            selected: hsla(self.selected_color),
            alternate_row: hsla(self.alternate_row_color),
            disabled: hsla(self.disabled_color),
            success: hsla(SUCCESS),
            warning: hsla(WARNING),
            danger: hsla(DANGER),
            info: hsla(INFO),
            on_primary: contrast_on(self.primary_color),
        }
    }
}

struct ActivePalette(Colors);

impl Global for ActivePalette {}

/// The active palette's colors.
pub fn palette(cx: &App) -> Colors {
    cx.global::<ActivePalette>().0
}

/// Applies theme `index` (out of range means Light) to every window.
pub fn apply(index: usize, cx: &mut App) {
    let palette = palettes::PALETTES
        .get(index)
        .unwrap_or(&palettes::PALETTES[0]);
    let c = palette.colors();
    cx.set_global(ActivePalette(c));

    // Load the matching base theme first; colors are edited in a second
    // update because switching mode reloads them.
    let mode = if c.is_dark {
        ThemeMode::Dark
    } else {
        ThemeMode::Light
    };
    Theme::change(mode, None, cx);
    Theme::update(cx, |theme| {
        theme.radius = px(6.);
        theme.radius_lg = px(8.);
        let t = &mut theme.colors;
        t.background = c.background;
        t.foreground = c.text;
        t.muted = c.alternate_row;
        t.muted_foreground = c.text_muted;
        t.border = c.border;
        t.input = c.border;
        t.ring = c.primary;
        t.caret = c.primary;
        t.selection = c.selected;
        t.accent = c.hover;
        t.accent_foreground = c.text;
        t.popover = c.surface;
        t.popover_foreground = c.text;
        t.overlay = Hsla {
            a: 0.4,
            ..hsla(0x000000)
        };
        t.window_border = c.border;
        t.link = c.primary_text;
        t.link_hover = c.primary;
        t.link_active = c.primary;

        t.primary = c.primary;
        t.primary_hover = shade(c.primary, -0.06);
        t.primary_active = shade(c.primary, -0.12);
        t.primary_foreground = c.on_primary;
        t.secondary = c.hover;
        t.secondary_hover = c.selected;
        t.secondary_active = c.selected;
        t.secondary_foreground = c.primary_text;

        t.button = c.card;
        t.button_hover = c.hover;
        t.button_active = c.selected;
        t.button_foreground = c.text;
        t.button_primary = c.primary;
        t.button_primary_hover = shade(c.primary, -0.06);
        t.button_primary_active = shade(c.primary, -0.12);
        t.button_primary_foreground = c.on_primary;
        t.button_secondary = c.hover;
        t.button_secondary_hover = c.selected;
        t.button_secondary_active = c.selected;
        t.button_secondary_foreground = c.primary_text;

        t.danger = c.danger;
        t.danger_hover = shade(c.danger, -0.06);
        t.danger_active = shade(c.danger, -0.12);
        t.danger_foreground = hsla(0xFFFFFF);
        t.success = c.success;
        t.success_hover = shade(c.success, -0.06);
        t.success_active = shade(c.success, -0.12);
        t.success_foreground = hsla(0xFFFFFF);
        t.warning = c.warning;
        t.warning_hover = shade(c.warning, -0.06);
        t.warning_active = shade(c.warning, -0.12);
        t.warning_foreground = hsla(0x1A1A1A);
        t.info = c.info;
        t.info_hover = shade(c.info, -0.06);
        t.info_active = shade(c.info, -0.12);
        t.info_foreground = hsla(0xFFFFFF);

        t.list = c.surface;
        t.list_hover = c.hover;
        t.list_active = c.selected;
        t.list_active_border = c.primary;
        t.list_even = c.alternate_row;
        t.list_head = c.surface;
        t.table = c.surface;
        t.table_hover = c.hover;
        t.table_active = c.selected;
        t.table_active_border = c.primary;
        t.table_even = c.alternate_row;
        t.table_head = c.surface;
        t.table_head_foreground = c.text_secondary;
        t.table_row_border = c.border;

        t.title_bar = c.surface;
        t.title_bar_border = c.border;
        t.tab_bar = c.surface;
        t.tab = c.surface;
        t.tab_active = c.surface;
        t.tab_foreground = c.text_secondary;
        t.tab_active_foreground = c.primary_text;
        t.switch = c.disabled;
        t.switch_thumb = hsla(0xFFFFFF);
        t.progress_bar = c.primary;
        t.slider_bar = c.primary;
        t.slider_thumb = c.primary;
        t.scrollbar_thumb = shade(c.border, if c.is_dark { 0.1 } else { -0.1 });
        t.scrollbar_thumb_hover = shade(c.border, if c.is_dark { 0.2 } else { -0.2 });
        t.group_box = c.card;
        t.group_box_foreground = c.text;
    });
}

/// `(background, text, primary)` of theme `index`, for the theme picker.
pub fn preview(index: usize) -> (Hsla, Hsla, Hsla) {
    let p = palettes::PALETTES
        .get(index)
        .unwrap_or(&palettes::PALETTES[0]);
    (
        hsla(p.background_color),
        hsla(p.text_primary),
        hsla(p.primary_color),
    )
}
