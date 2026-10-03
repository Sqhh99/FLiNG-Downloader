//! Right-hand drawer with the selected trainer's cover, metadata, versions
//! and options.

use fling_app::{Command, CoverState, DetailState, Selection};
use gpui_kit::assets::IconName;
use gpui_kit::component::Disableable as _;
use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::select::{Select, SelectState};
use gpui_kit::component::spinner::Spinner;
use gpui_kit::component::{IndexPath, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::state::AppModel;
use crate::theme::palette;
use crate::views::widgets::icon_button;

pub enum DrawerEvent {
    Close,
    /// A download was queued; the download list should open.
    DownloadStarted,
}

impl EventEmitter<DrawerEvent> for DetailDrawer {}

pub struct DetailDrawer {
    model: Entity<AppModel>,
    versions: Entity<SelectState<Vec<SharedString>>>,
    version_labels: Vec<String>,
    _subscriptions: Vec<Subscription>,
}

impl DetailDrawer {
    pub fn new(model: Entity<AppModel>, window: &mut Window, cx: &mut Context<Self>) -> Self {
        let versions = cx.new(|cx| SelectState::new(Vec::<SharedString>::new(), None, window, cx));
        let subscriptions = vec![cx.observe_in(&model, window, |this, model, window, cx| {
            let labels: Vec<String> = model
                .read(cx)
                .selection
                .as_ref()
                .map(|s| {
                    s.modifier
                        .versions
                        .iter()
                        .map(|v| v.label.clone())
                        .collect()
                })
                .unwrap_or_default();
            if labels != this.version_labels {
                let items: Vec<SharedString> =
                    labels.iter().cloned().map(SharedString::from).collect();
                let selected = (!items.is_empty()).then(IndexPath::default);
                this.versions.update(cx, |state, cx| {
                    state.set_items(items, window, cx);
                    state.set_selected_index(selected, window, cx);
                });
                this.version_labels = labels;
            }
            cx.notify();
        })];
        Self {
            model,
            versions,
            version_labels: Vec::new(),
            _subscriptions: subscriptions,
        }
    }

    fn download(&mut self, cx: &mut Context<Self>) {
        if self.version_labels.is_empty() {
            return;
        }
        let version_index = self
            .versions
            .read(cx)
            .selected_index(cx)
            .map_or(0, |ix| ix.row);
        self.model
            .read(cx)
            .send(Command::Download { version_index });
        cx.emit(DrawerEvent::DownloadStarted);
    }

    fn render_cover(&self, selection: &Selection, cx: &App) -> AnyElement {
        let c = palette(cx);
        let placeholder = |content: AnyElement| {
            v_flex()
                .w(px(100.))
                .h(px(140.))
                .flex_shrink_0()
                .items_center()
                .justify_center()
                .gap_2()
                .rounded(px(6.))
                .bg(c.alternate_row)
                .text_xs()
                .text_color(c.text_muted)
                .child(content)
        };
        match &selection.cover {
            CoverState::Ready(path) => img(path.clone())
                .max_w(px(110.))
                .max_h(px(140.))
                .object_fit(ObjectFit::Contain)
                .rounded(px(6.))
                .into_any_element(),
            CoverState::Loading => placeholder(
                v_flex()
                    .items_center()
                    .gap_2()
                    .child(Spinner::new())
                    .child(tr!("detail.cover_loading"))
                    .into_any_element(),
            )
            .into_any_element(),
            CoverState::Missing => placeholder(
                v_flex()
                    .items_center()
                    .gap_2()
                    .child(gpui_kit::component::Icon::new(IconName::ImageOff).large())
                    .child(tr!("detail.no_cover"))
                    .into_any_element(),
            )
            .into_any_element(),
        }
    }

    fn info_row(label: SharedString, value: String, cx: &App) -> impl IntoElement {
        let c = palette(cx);
        let value = if value.is_empty() {
            "-".to_owned()
        } else {
            value
        };
        h_flex()
            .gap_1()
            .text_sm()
            .child(div().text_color(c.text_secondary).child(label))
            .child(div().text_color(c.text).truncate().child(value))
    }

    fn group(title: SharedString, cx: &App) -> Div {
        let c = palette(cx);
        v_flex()
            .gap_2()
            .p_3()
            .rounded(px(6.))
            .border_1()
            .border_color(c.border)
            .bg(c.card)
            .child(
                div()
                    .text_sm()
                    .font_weight(FontWeight::SEMIBOLD)
                    .text_color(c.text)
                    .child(title),
            )
    }
}

impl Render for DetailDrawer {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let selection = self.model.read(cx).selection.clone().unwrap_or_default();
        let m = &selection.modifier;
        let close = icon_button("drawer-close", IconName::X, tr!("common.close"))
            .on_click(cx.listener(|_, _, _, cx| cx.emit(DrawerEvent::Close)));

        let busy = matches!(
            selection.detail,
            DetailState::Loading | DetailState::Error | DetailState::Idle
        );
        let body = v_flex()
            .id("drawer-body")
            .flex_1()
            .min_h_0()
            .overflow_y_scroll()
            .gap_3()
            .child(
                h_flex()
                    .gap_3()
                    .items_start()
                    .child(self.render_cover(&selection, cx))
                    .child(
                        v_flex()
                            .gap_2()
                            .min_w_0()
                            .child(Self::info_row(
                                tr!("detail.game_version"),
                                m.game_version.clone(),
                                cx,
                            ))
                            .child(Self::info_row(
                                tr!("detail.options_count"),
                                m.options_count.to_string(),
                                cx,
                            ))
                            .child(Self::info_row(
                                tr!("detail.last_update"),
                                m.last_update.clone(),
                                cx,
                            )),
                    ),
            )
            .map(|body| {
                if selection.detail == DetailState::Empty {
                    body.child(
                        div()
                            .text_sm()
                            .text_color(c.text_muted)
                            .child(tr!("detail.no_versions")),
                    )
                } else {
                    body.child(
                        Self::group(tr!("detail.select_version"), cx).child(
                            h_flex()
                                .gap_2()
                                .child(div().flex_1().min_w_0().child(Select::new(&self.versions)))
                                .child(
                                    Button::new("download")
                                        .primary()
                                        .icon(gpui_kit::component::Icon::new(IconName::Download))
                                        .tooltip(tr!("detail.download"))
                                        .disabled(self.version_labels.is_empty())
                                        .on_click(cx.listener(|this, _, _, cx| this.download(cx))),
                                ),
                        ),
                    )
                }
            })
            .child(
                Self::group(tr!("detail.options"), cx).child(
                    v_flex()
                        .gap_0p5()
                        .text_sm()
                        .text_color(c.text)
                        .children(m.options.iter().map(|line| {
                            let header = line.starts_with('●');
                            div()
                                .when(header, |d| {
                                    d.mt_1()
                                        .font_weight(FontWeight::SEMIBOLD)
                                        .text_color(c.primary_text)
                                })
                                .child(line.clone())
                        })),
                ),
            );

        v_flex()
            .id("detail-drawer")
            .size_full()
            .p_4()
            .gap_3()
            .bg(c.surface)
            .border_l_1()
            .border_color(c.border)
            .shadow_lg()
            .occlude()
            .child(
                h_flex()
                    .gap_2()
                    .items_start()
                    .child(
                        div()
                            .flex_1()
                            .min_w_0()
                            .text_lg()
                            .font_weight(FontWeight::BOLD)
                            .text_color(c.text)
                            .child(m.name.clone()),
                    )
                    .child(close),
            )
            .child(div().h(px(1.)).bg(c.border))
            .map(|drawer| {
                if !busy {
                    return drawer.child(body);
                }
                let overlay = v_flex().flex_1().items_center().justify_center().gap_3();
                drawer.child(if selection.detail == DetailState::Error {
                    overlay
                        .child(
                            div()
                                .font_weight(FontWeight::SEMIBOLD)
                                .text_color(c.text)
                                .child(tr!("detail.load_failed")),
                        )
                        .child(
                            div()
                                .text_sm()
                                .text_color(c.text_muted)
                                .text_center()
                                .child(tr!("detail.load_failed_hint")),
                        )
                        .child(
                            Button::new("retry")
                                .primary()
                                .label(tr!("common.retry"))
                                .on_click(cx.listener(|this, _, _, cx| {
                                    this.model.read(cx).send(Command::RetryDetail)
                                })),
                        )
                } else {
                    overlay
                        .child(Spinner::new().large())
                        .child(div().text_color(c.text_muted).child(tr!("common.loading")))
                })
            })
    }
}
