//! Small building blocks shared by the views.

use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::tooltip::Tooltip;
use gpui_kit::component::{Icon, IconNamed, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::theme::palette;

/// A borderless icon-only button. The tooltip text is also its accessible
/// name, so screen readers announce more than "button".
pub fn icon_button(
    id: impl Into<ElementId>,
    icon: impl IconNamed,
    tooltip: impl Into<SharedString>,
) -> Button {
    let tooltip = tooltip.into();
    Button::new(id)
        .ghost()
        .small()
        .icon(Icon::new(icon))
        .accessibility_label(tooltip.clone())
        .tooltip(tooltip)
}

/// A tooltip builder showing `text`, for content that may be cut off.
pub fn text_tooltip(
    text: impl Into<SharedString>,
) -> impl Fn(&mut Window, &mut App) -> AnyView + 'static {
    let text = text.into();
    move |window, cx| Tooltip::new(text.clone()).build(window, cx)
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

/// Cells for one 40 px row; `cells` must line up with `columns`. The row is
/// a Tab stop (Enter and Space click it) and a list item announced as
/// `label`.
pub fn table_row(
    id: impl Into<ElementId>,
    label: SharedString,
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
        .role(Role::ListItem)
        .aria_label(label)
        .aria_selected(selected)
        .tab_index(0)
        .h(px(40.))
        .flex_shrink_0()
        .px_2()
        .border_1()
        .border_color(transparent_black())
        .focus_visible(|s| s.border_color(c.primary))
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

/// One-line form of a trainer name: "title (subtitle)", or just the title.
pub fn trainer_full_name(display_name: &str, subtitle: &str, name: &str) -> SharedString {
    let title = trainer_name(display_name, name);
    if subtitle.is_empty() {
        title
    } else {
        format!("{title} ({subtitle})").into()
    }
}

/// A trainer-name table cell. A translated name is two lines, the title over
/// the smaller English title, both fitting the 40 px row; an untranslated one
/// is a single line. Hovering shows the whole name.
pub fn trainer_name_cell(
    id: impl Into<ElementId>,
    display_name: &str,
    subtitle: &str,
    name: &str,
    cx: &App,
) -> AnyElement {
    let title = trainer_name(display_name, name);
    let full = trainer_full_name(display_name, subtitle, name);
    if subtitle.is_empty() {
        return tooltip_cell(id, title);
    }
    v_flex()
        .id(id)
        .tooltip(text_tooltip(full))
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

/// Single-line text cell that truncates and shows its whole text on hover.
pub fn tooltip_cell(id: impl Into<ElementId>, text: impl Into<SharedString>) -> AnyElement {
    let text = text.into();
    div()
        .id(id)
        .truncate()
        .tooltip(text_tooltip(text.clone()))
        .child(text)
        .into_any_element()
}

/// The placeholder of an empty table: what the emptiness means, and what to
/// do about it.
pub fn empty_table(message: SharedString, cx: &App) -> impl IntoElement {
    v_flex()
        .flex_1()
        .items_center()
        .justify_center()
        .px_6()
        .text_center()
        .text_color(palette(cx).text_muted)
        .child(message)
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
