//! The "downloaded" tab: the local trainer library.

use std::time::Duration;

use fling_app::{Command, FolderTarget};
use gpui_kit::assets::IconName;
use gpui_kit::component::{h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::state::AppModel;
use crate::theme::palette;
use crate::views::widgets::{Column, empty_table, icon_button, table_header, table_row, text_cell};

/// How long the delete-failure banner stays up.
const BANNER_TIME: Duration = Duration::from_secs(6);

pub struct LibraryPage {
    model: Entity<AppModel>,
    selected_row: Option<usize>,
    banner_serial: u64,
    _subscriptions: Vec<Subscription>,
}

impl LibraryPage {
    pub fn new(model: Entity<AppModel>, cx: &mut Context<Self>) -> Self {
        let subscriptions = vec![cx.observe(&model, |this, model, cx| {
            // Start the hide timer once per failure.
            if let Some((_, _, serial)) = model.read(cx).delete_error.clone()
                && serial != this.banner_serial
            {
                this.banner_serial = serial;
                let model = model.downgrade();
                cx.spawn(async move |_, cx| {
                    cx.background_executor().timer(BANNER_TIME).await;
                    let _ = model.update(cx, |model, cx| model.clear_delete_error(serial, cx));
                })
                .detach();
            }
            cx.notify();
        })];
        Self {
            model,
            selected_row: None,
            banner_serial: 0,
            _subscriptions: subscriptions,
        }
    }
}

impl Render for LibraryPage {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let model = self.model.read(cx);
        let columns = [
            Column::new(tr!("col.mod_name"), 3.),
            Column::new(tr!("col.version"), 3.),
            Column::new(tr!("col.download_date"), 2.),
            Column::new(tr!("col.actions"), 2.),
        ];
        let banner = model.delete_error.clone();
        let rows: Vec<_> = model
            .library
            .iter()
            .enumerate()
            .map(|(i, item)| {
                let actions = h_flex()
                    .gap_1()
                    .child(
                        icon_button(
                            ("open", i),
                            IconName::FolderOpen,
                            tr!("library.open_folder"),
                        )
                        .on_click(cx.listener(move |this, _, _, cx| {
                            this.model
                                .read(cx)
                                .send(Command::OpenFolder(FolderTarget::LibraryItem(i)));
                        })),
                    )
                    .child(
                        icon_button(("delete", i), IconName::Trash, tr!("library.delete"))
                            .on_click(cx.listener(move |this, _, _, cx| {
                                this.selected_row = None;
                                this.model.read(cx).send(Command::DeleteLibraryItem(i));
                            })),
                    )
                    .into_any_element();
                let cells = vec![
                    text_cell(item.name.clone()),
                    text_cell(item.version.clone()),
                    text_cell(item.display_date()),
                    actions,
                ];
                table_row(
                    ("library", i),
                    &columns,
                    cells,
                    i,
                    self.selected_row == Some(i),
                    cx,
                )
                .on_click(cx.listener(move |this, event: &ClickEvent, _, cx| {
                    this.selected_row = Some(i);
                    if event.click_count() >= 2 {
                        this.model.read(cx).send(Command::RunLibraryItem(i));
                    }
                    cx.notify();
                }))
            })
            .collect();
        let empty = rows.is_empty();

        v_flex()
            .size_full()
            .p_3()
            .gap_3()
            .when_some(banner, |page, (name, path, _)| {
                page.child(
                    div()
                        .p_2()
                        .rounded(px(6.))
                        .border_1()
                        .border_color(c.danger)
                        .bg(Hsla { a: 0.1, ..c.danger })
                        .text_sm()
                        .text_color(c.danger)
                        .child(tr!("library.delete_failed", name = name, path = path)),
                )
            })
            .child(
                v_flex()
                    .flex_1()
                    .min_h_0()
                    .rounded(px(6.))
                    .border_1()
                    .border_color(c.border)
                    .bg(c.surface)
                    .overflow_hidden()
                    .child(table_header(&columns, cx))
                    .map(|table| {
                        if empty {
                            table.child(empty_table(cx))
                        } else {
                            table.child(
                                v_flex()
                                    .id("library-rows")
                                    .flex_1()
                                    .overflow_y_scroll()
                                    .children(rows),
                            )
                        }
                    }),
            )
    }
}
