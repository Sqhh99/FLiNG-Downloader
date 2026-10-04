//! Small building blocks shared by the views.

use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::{Icon, IconNamed, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::theme::palette;

/// A borderless icon-only button with a tooltip.
pub fn icon_button(
    id: impl Into<ElementId>,
    icon: impl IconNamed,
    tooltip: impl Into<SharedString>,
) -> Button {
    Button::new(id)
        .ghost()
        .small()
        .icon(Icon::new(icon))
        .tooltip(tooltip)
}

/// A table column: header text and relative width.
pub struct Column {
    pub title: SharedString,
    pub weight: f32,
}

impl Column {
    pub fn new(title: SharedString, weight: f32) -> Self {
        Self { title, weight }
    }
}

fn total_weight(columns: &[Column]) -> f32 {
    columns.iter().map(|c| c.weight).sum::<f32>().max(1.0)
}

/// The 36 px header row.
pub fn table_header(columns: &[Column], cx: &App) -> impl IntoElement {
    let c = palette(cx);
    let total = total_weight(columns);
    h_flex()
        .h(px(36.))
        .flex_shrink_0()
        .px_2()
        .bg(c.surface)
        .border_b_1()
        .border_color(c.border)
        .text_sm()
        .font_weight(FontWeight::SEMIBOLD)
        .text_color(c.text_secondary)
        .children(columns.iter().map(|col| {
            div()
                .w(relative(col.weight / total))
                .px_2()
                .truncate()
                .child(col.title.clone())
        }))
}

/// Cells for one 40 px row; `cells` must line up with `columns`.
pub fn table_row(
    id: impl Into<ElementId>,
    columns: &[Column],
    cells: Vec<AnyElement>,
    index: usize,
    selected: bool,
    cx: &App,
) -> Stateful<Div> {
    let c = palette(cx);
    let total = total_weight(columns);
    let base = if selected {
        c.selected
    } else if index % 2 == 1 {
        c.alternate_row
    } else {
        c.surface
    };
    h_flex()
        .id(id)
        .h(px(40.))
        .flex_shrink_0()
        .px_2()
        .bg(base)
        .when(!selected, |row| row.hover(|s| s.bg(c.hover)))
        .text_sm()
        .text_color(c.text)
        .children(columns.iter().zip(cells).map(|(col, cell)| {
            div()
                .w(relative(col.weight / total))
                .px_2()
                .overflow_hidden()
                .child(cell)
        }))
}

/// The main line of a trainer name: the backend's localized `display_name`,
/// or the site name before one is filled in.
pub fn trainer_name(display_name: &str, name: &str) -> SharedString {
    if display_name.is_empty() {
        name.to_owned().into()
    } else {
        display_name.to_owned().into()
    }
}

/// A trainer-name table cell. A translated name is two lines, the title over
/// the smaller English title, both fitting the 40 px row; an untranslated one
/// is a single line.
pub fn trainer_name_cell(display_name: &str, subtitle: &str, name: &str, cx: &App) -> AnyElement {
    let title = trainer_name(display_name, name);
    if subtitle.is_empty() {
        return text_cell(title);
    }
    v_flex()
        .min_w_0()
        .child(div().truncate().line_height(px(17.)).child(title))
        .child(
            div()
                .truncate()
                .text_xs()
                .line_height(px(15.))
                .text_color(palette(cx).text_muted)
                .child(subtitle.to_owned()),
        )
        .into_any_element()
}

/// Single-line text cell that truncates.
pub fn text_cell(text: impl Into<SharedString>) -> AnyElement {
    div().truncate().child(text.into()).into_any_element()
}

/// The "no data" placeholder of an empty table.
pub fn empty_table(cx: &App) -> impl IntoElement {
    v_flex()
        .flex_1()
        .items_center()
        .justify_center()
        .text_color(palette(cx).text_muted)
        .child(tr!("table.empty"))
}

/// Overlay that dims what is underneath and swallows its mouse input.
pub fn scrim(id: impl Into<ElementId>) -> Stateful<Div> {
    div()
        .id(id)
        .absolute()
        .inset_0()
        .bg(Hsla { a: 0.35, ..black() })
        .occlude()
}
