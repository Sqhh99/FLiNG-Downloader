//! The "downloaded" tab: the local trainer library.

use std::time::Duration;

use fling_app::{Command, FolderTarget};
use gpui_kit::assets::IconName;
use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::{Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::state::AppModel;
use crate::theme::palette;
use crate::views::motion;
use crate::views::smooth_scroll::SmoothScroll;
use crate::views::widgets::{
    Column, empty_table, icon_button, table_header, table_row, text_cell, trainer_name,
    trainer_name_cell,
};

/// How long the delete-failure banner stays up.
const BANNER_TIME: Duration = Duration::from_secs(6);

pub struct LibraryPage {
    model: Entity<AppModel>,
    selected_row: Option<usize>,
    /// File of the entry whose Delete is waiting for confirmation. Keyed by
    /// file, not row, so a library change cannot retarget it.
    pending_delete: Option<String>,
    banner_serial: u64,
    debug_delete_done: bool,
    scroll: SmoothScroll,
    _subscriptions: Vec<Subscription>,
}

impl LibraryPage {
    pub fn new(model: Entity<AppModel>, cx: &mut Context<Self>) -> Self {
        let subscriptions = vec![cx.observe(&model, |this, model, cx| {
            // Debug builds: FLING_DEBUG_OPEN=library-delete[:row] asks to
            // delete that entry once the library loads, for screenshot checks.
            let debug_row = crate::views::root::debug_open().and_then(|v| {
                v.strip_prefix("library-delete")
                    .map(|rest| rest.trim_start_matches(':').parse().unwrap_or(0))
            });
            if cfg!(debug_assertions)
                && let Some(row) = debug_row
                && !this.debug_delete_done
                && let Some(item) = model.read(cx).library.get(row)
            {
                this.debug_delete_done = true;
                this.selected_row = Some(row);
                this.pending_delete = Some(item.file_path.clone());
            }
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
            scroll: SmoothScroll::new(),
            selected_row: None,
            pending_delete: None,
            banner_serial: 0,
            debug_delete_done: false,
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
        // The entry may have gone meanwhile (deleted, or its file vanished).
        let confirm = self.pending_delete.as_ref().and_then(|file| {
            model
                .library
                .iter()
                .find(|item| &item.file_path == file)
                .map(|item| (file.clone(), trainer_name(&item.display_name, &item.name)))
        });
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
                    .child({
                        let file = item.file_path.clone();
                        icon_button(("delete", i), IconName::Trash, tr!("library.delete")).on_click(
                            cx.listener(move |this, _, _, cx| {
                                this.selected_row = Some(i);
                                this.pending_delete = Some(file.clone());
                                cx.notify();
                            }),
                        )
                    })
                    .into_any_element();
                let cells = vec![
                    trainer_name_cell(&item.display_name, &item.display_subtitle, &item.name, cx),
                    text_cell(item.version.clone()),
                    text_cell(item.display_date()),
                    actions,
                ];
                let row = table_row(
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
                }));
                // Keyed by file, so a new download rises in and the rest stay put.
                motion::row_in(
                    row,
                    SharedString::from(format!("lib-{}", item.file_path)),
                    i,
                    cx,
                )
            })
            .collect();
        let empty = rows.is_empty();

        v_flex()
            .size_full()
            .p_3()
            .gap_3()
            .when_some(confirm, |page, (file, name)| {
                page.child(
                    h_flex()
                        .gap_3()
                        .p_2()
                        .pl_3()
                        .rounded(px(6.))
                        .border_1()
                        .border_color(c.danger)
                        .bg(Hsla { a: 0.1, ..c.danger })
                        .child(
                            div()
                                .flex_1()
                                .min_w_0()
                                .text_sm()
                                .text_color(c.text)
                                .child(tr!("library.confirm_delete", name = name)),
                        )
                        .child(
                            Button::new("cancel-delete")
                                .ghost()
                                .small()
                                .label(tr!("common.cancel"))
                                .on_click(cx.listener(|this, _, _, cx| {
                                    this.pending_delete = None;
                                    cx.notify();
                                })),
                        )
                        .child(
                            Button::new("confirm-delete")
                                .danger()
                                .small()
                                .label(tr!("library.delete"))
                                .on_click(cx.listener(move |this, _, _, cx| {
                                    this.pending_delete = None;
                                    this.selected_row = None;
                                    let model = this.model.read(cx);
                                    // Resolved now: rows may have moved since the click.
                                    if let Some(index) =
                                        model.library.iter().position(|item| item.file_path == file)
                                    {
                                        model.send(Command::DeleteLibraryItem(index));
                                    }
                                    cx.notify();
                                })),
                        ),
                )
            })
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
                                div()
                                    .relative()
                                    .flex_1()
                                    .min_h_0()
                                    .child(
                                        v_flex()
                                            .id("library-rows")
                                            .size_full()
                                            .overflow_y_scroll()
                                            .track_scroll(self.scroll.handle())
                                            .children(rows),
                                    )
                                    .child(self.scroll.driver()),
                            )
                        }
                    }),
            )
    }
}
