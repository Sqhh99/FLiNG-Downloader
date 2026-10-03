//! The download list that drops down from the title bar.

use fling_app::{Command, DownloadTask, FolderTarget, TaskId, TaskStatus};
use gpui_kit::assets::IconName;
use gpui_kit::component::progress::Progress;
use gpui_kit::component::{h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::{format_bytes, format_speed, task_status, tr};
use crate::state::AppModel;
use crate::theme::{Colors, palette};
use crate::views::widgets::icon_button;

pub struct DownloadsPanel {
    model: Entity<AppModel>,
    _subscriptions: Vec<Subscription>,
}

impl DownloadsPanel {
    pub fn new(model: Entity<AppModel>, cx: &mut Context<Self>) -> Self {
        let subscriptions = vec![cx.observe(&model, |_, _, cx| cx.notify())];
        Self {
            model,
            _subscriptions: subscriptions,
        }
    }

    fn on_click(
        cx: &Context<Self>,
        id: TaskId,
        command: fn(TaskId) -> Command,
    ) -> impl Fn(&ClickEvent, &mut Window, &mut App) + 'static {
        cx.listener(move |this: &mut Self, _: &ClickEvent, _, cx| {
            this.model.read(cx).send(command(id.clone()));
        })
    }

    fn status_color(status: TaskStatus, c: &Colors) -> Hsla {
        match status {
            TaskStatus::Completed => c.success,
            TaskStatus::Failed | TaskStatus::Canceled => c.danger,
            TaskStatus::Paused => c.warning,
            TaskStatus::Queued | TaskStatus::Downloading => c.text_secondary,
        }
    }

    fn info_line(task: &DownloadTask) -> String {
        match task.status {
            TaskStatus::Completed => format_bytes(task.bytes_received),
            TaskStatus::Failed => task.error_message.clone(),
            _ if task.bytes_total > 0 => format!(
                "{} / {}  {:.0}%  {}",
                format_bytes(task.bytes_received),
                format_bytes(task.bytes_total),
                task.progress().unwrap_or(0.0) * 100.0,
                format_speed(task.speed),
            ),
            _ => format!(
                "{}  {}",
                format_bytes(task.bytes_received),
                format_speed(task.speed)
            ),
        }
    }

    fn render_task(
        &self,
        index: usize,
        task: &DownloadTask,
        cx: &mut Context<Self>,
    ) -> impl IntoElement {
        let c = palette(cx);
        let status = task.status;
        let send = |command: fn(TaskId) -> Command| Self::on_click(cx, task.id.clone(), command);
        let active = matches!(
            status,
            TaskStatus::Queued | TaskStatus::Downloading | TaskStatus::Paused
        );
        let name = if task.file_name.is_empty() {
            tr!("downloads.unknown_file")
        } else {
            task.file_name.clone().into()
        };

        let mut buttons = h_flex().gap_0p5();
        if active {
            buttons = buttons.child(if status == TaskStatus::Paused {
                icon_button(("resume", index), IconName::Play, tr!("downloads.resume"))
                    .on_click(send(Command::ResumeTask))
            } else {
                icon_button(("pause", index), IconName::Pause, tr!("downloads.pause"))
                    .on_click(send(Command::PauseTask))
            });
            buttons = buttons.child(
                icon_button(("cancel", index), IconName::X, tr!("downloads.cancel"))
                    .on_click(send(Command::CancelTask)),
            );
        }
        if status == TaskStatus::Failed {
            buttons = buttons.child(
                icon_button(("retry", index), IconName::RotateCw, tr!("downloads.retry"))
                    .on_click(send(Command::ResumeTask)),
            );
        }
        if status == TaskStatus::Completed {
            let id = task.id.clone();
            buttons = buttons.child(
                icon_button(
                    ("folder", index),
                    IconName::FolderOpen,
                    tr!("downloads.open_folder"),
                )
                .on_click(cx.listener(move |this, _, _, cx| {
                    this.model
                        .read(cx)
                        .send(Command::OpenFolder(FolderTarget::Task(id.clone())))
                })),
            );
        }
        if status.is_terminal() {
            buttons = buttons.child(
                icon_button(("remove", index), IconName::Trash, tr!("downloads.remove"))
                    .on_click(send(Command::RemoveTask)),
            );
        }

        let progress = match task.progress() {
            Some(p) => Progress::new(("progress", index)).value(p * 100.0),
            None => Progress::new(("progress", index)).loading(status == TaskStatus::Downloading),
        };

        v_flex()
            .gap_1()
            .px_3()
            .py_2()
            .border_b_1()
            .border_color(c.border)
            .child(
                h_flex()
                    .gap_2()
                    .child(
                        div()
                            .flex_1()
                            .min_w_0()
                            .truncate()
                            .text_sm()
                            .text_color(c.text)
                            .child(name),
                    )
                    .child(
                        div()
                            .text_xs()
                            .text_color(Self::status_color(status, &c))
                            .child(task_status(status)),
                    ),
            )
            .when(status != TaskStatus::Completed, |row| {
                row.child(progress.h(px(4.)))
            })
            .child(
                h_flex()
                    .gap_2()
                    .child(
                        div()
                            .flex_1()
                            .min_w_0()
                            .truncate()
                            .text_xs()
                            .text_color(if status == TaskStatus::Failed {
                                c.danger
                            } else {
                                c.text_muted
                            })
                            .child(Self::info_line(task)),
                    )
                    .child(buttons),
            )
    }
}

impl Render for DownloadsPanel {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let model = self.model.read(cx);
        let tasks = model.tasks.clone();
        let active = model.active_downloads();
        let subtitle = if active > 0 {
            tr!("downloads.active", count = active)
        } else {
            tr!("downloads.none_active")
        };
        let rows: Vec<_> = tasks
            .iter()
            .enumerate()
            .map(|(i, t)| self.render_task(i, t, cx).into_any_element())
            .collect();

        v_flex()
            .id("downloads-panel")
            .w(px(320.))
            .max_h(px(360.))
            .rounded(px(8.))
            .border_1()
            .border_color(c.border)
            .bg(c.surface)
            .shadow_lg()
            .occlude()
            .child(
                v_flex()
                    .px_3()
                    .py_2()
                    .border_b_1()
                    .border_color(c.border)
                    .child(
                        div()
                            .font_weight(FontWeight::SEMIBOLD)
                            .text_color(c.text)
                            .child(tr!("downloads.title")),
                    )
                    .child(div().text_xs().text_color(c.text_muted).child(subtitle)),
            )
            .map(|panel| {
                if rows.is_empty() {
                    panel.child(
                        div()
                            .p_6()
                            .flex()
                            .justify_center()
                            .text_sm()
                            .text_color(c.text_muted)
                            .child(tr!("downloads.empty")),
                    )
                } else {
                    panel.child(
                        v_flex()
                            .id("download-rows")
                            .overflow_y_scroll()
                            .children(rows),
                    )
                }
            })
    }
}
