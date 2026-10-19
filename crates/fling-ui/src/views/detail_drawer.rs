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
use crate::views::motion;
use crate::views::smooth_scroll::SmoothScroll;
use crate::views::widgets::{icon_button, text_tooltip, trainer_full_name, trainer_name};

pub enum DrawerEvent {
    Close,
    /// A download was queued; the download list should open.
    DownloadStarted,
}

impl EventEmitter<DrawerEvent> for DetailDrawer {}

pub struct DetailDrawer {
    model: Entity<AppModel>,
    versions: Entity<SelectState<Vec<SharedString>>>,
    /// Full version labels; the picker shows [`short_version_labels`].
    version_labels: Vec<String>,
    /// Display size of the last cover, so repaints don't reread the file.
    cover_size: Option<(std::path::PathBuf, (f32, f32))>,
    options_scroll: SmoothScroll,
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
                let items: Vec<SharedString> = short_version_labels(&labels)
                    .into_iter()
                    .map(SharedString::from)
                    .collect();
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
            cover_size: None,
            options_scroll: SmoothScroll::new(),
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
            CoverState::Ready(path) => {
                // Size the element to the picture itself so the row around it
                // follows the cover's real shape.
                let (w, h) = self
                    .cover_size
                    .as_ref()
                    .filter(|(cached, _)| cached == path)
                    .map_or(COVER_MAX, |(_, size)| *size);
                img(path.clone())
                    .w(px(w))
                    .h(px(h))
                    .flex_shrink_0()
                    .object_fit(ObjectFit::Contain)
                    .rounded(px(6.))
                    .into_any_element()
            }
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
            .gap_2()
            .text_size(BODY_TEXT)
            .line_height(LINE_HEIGHT)
            .child(
                div()
                    .flex_shrink_0()
                    .text_color(c.text_secondary)
                    .child(label),
            )
            .child(div().min_w_0().text_color(c.text).truncate().child(value))
    }

    /// Small caption above a bordered box, as in the Qt drawer.
    fn caption(text: SharedString, cx: &App) -> impl IntoElement {
        div()
            .text_size(BODY_TEXT)
            .text_color(palette(cx).text_secondary)
            .child(text)
    }

    /// Bordered box on the drawer background (no fill of its own).
    fn framed(cx: &App) -> Div {
        div()
            .rounded(px(6.))
            .border_1()
            .border_color(palette(cx).border)
    }
}

/// The largest cover box, as in the Qt drawer.
const COVER_MAX: (f32, f32) = (110., 140.);

/// Display size of a cached cover: its aspect ratio fitted into `COVER_MAX`.
/// Covers are always PNG (the extractor writes PNG), so the size comes from
/// the IHDR chunk; an unreadable file gets the full box.
fn cover_size(path: &std::path::Path) -> (f32, f32) {
    let mut header = [0u8; 24];
    let read =
        std::fs::File::open(path).and_then(|mut f| std::io::Read::read_exact(&mut f, &mut header));
    let (w, h) = match read {
        Ok(()) if &header[..8] == b"\x89PNG\r\n\x1a\n" => (
            u32::from_be_bytes([header[16], header[17], header[18], header[19]]) as f32,
            u32::from_be_bytes([header[20], header[21], header[22], header[23]]) as f32,
        ),
        _ => return COVER_MAX,
    };
    if w <= 0. || h <= 0. {
        return COVER_MAX;
    }
    let scale = (COVER_MAX.0 / w).min(COVER_MAX.1 / h);
    (w * scale, h * scale)
}

/// The part of a version label that tells versions apart:
/// "Elden.Ring.Nightreign.v1.01-v1.03.Plus.26.Trainer-FLiNG" becomes
/// "v1.01-v1.03 Plus 26". A label without a `v<digit>` word stays whole.
fn short_version_label(label: &str) -> String {
    let bytes = label.as_bytes();
    let start = (0..bytes.len()).find(|&i| {
        matches!(bytes[i], b'v' | b'V')
            && bytes.get(i + 1).is_some_and(u8::is_ascii_digit)
            && (i == 0 || matches!(bytes[i - 1], b'.' | b' ' | b'_' | b'-'))
    });
    let Some(start) = start else {
        return label.to_owned();
    };
    let mut short = &label[start..];
    for suffix in ["-FLiNG", ".Trainer", " Trainer"] {
        let cut = short.len().saturating_sub(suffix.len());
        if short
            .get(cut..)
            .is_some_and(|tail| tail.eq_ignore_ascii_case(suffix))
        {
            short = &short[..cut];
        }
    }
    short.replace(".Plus.", " Plus ")
}

/// Short labels for the version picker, or the full ones when shortening
/// would make two versions look the same.
fn short_version_labels(labels: &[String]) -> Vec<String> {
    let short: Vec<String> = labels.iter().map(|l| short_version_label(l)).collect();
    let mut seen = std::collections::HashSet::new();
    if short.iter().all(|s| seen.insert(s.as_str())) {
        short
    } else {
        labels.to_vec()
    }
}

/// Text sizes of the Qt drawer: 18 px title, 13 px everything else.
const TITLE_TEXT: Pixels = px(18.);
const BODY_TEXT: Pixels = px(13.);
const LINE_HEIGHT: Pixels = px(20.);

impl Render for DetailDrawer {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let selection = self.model.read(cx).selection.clone().unwrap_or_default();
        let m = &selection.modifier;
        if let CoverState::Ready(path) = &selection.cover
            && self
                .cover_size
                .as_ref()
                .is_none_or(|(cached, _)| cached != path)
        {
            self.cover_size = Some((path.clone(), cover_size(path)));
        }
        // The full file name of the chosen version, on hover over the picker.
        let selected_label = self
            .versions
            .read(cx)
            .selected_index(cx)
            .and_then(|ix| self.version_labels.get(ix.row).cloned())
            .unwrap_or_default();
        let close = icon_button("drawer-close", IconName::X, tr!("common.close"))
            .on_click(cx.listener(|_, _, _, cx| cx.emit(DrawerEvent::Close)));

        let busy = matches!(
            selection.detail,
            DetailState::Loading | DetailState::Error | DetailState::Idle
        );
        let versions: AnyElement = if selection.detail == DetailState::Empty {
            div()
                .text_size(BODY_TEXT)
                .text_color(c.text_muted)
                .child(tr!("detail.no_versions"))
                .into_any_element()
        } else {
            v_flex()
                .gap_1()
                .child(Self::caption(tr!("detail.select_version"), cx))
                .child(
                    Self::framed(cx).p_2().child(
                        h_flex()
                            .gap_2()
                            .child(
                                div()
                                    .id("version-picker")
                                    .flex_1()
                                    .min_w_0()
                                    .tooltip(text_tooltip(selected_label))
                                    .child(
                                        Select::new(&self.versions)
                                            .accessibility_label(tr!("detail.select_version")),
                                    ),
                            )
                            // The drawer's main action: labelled and primary.
                            .child(
                                Button::new("download")
                                    .primary()
                                    .icon(gpui_kit::component::Icon::new(IconName::Download))
                                    .label(tr!("detail.download"))
                                    .disabled(self.version_labels.is_empty())
                                    .on_click(cx.listener(|this, _, _, cx| this.download(cx))),
                            ),
                    ),
                )
                .into_any_element()
        };

        let body = v_flex()
            .flex_1()
            .min_h_0()
            .gap_3()
            .child(
                // The info column is centered on the cover, so the row is as
                // tall as the cover (or the three lines, if the cover is flat).
                h_flex()
                    .gap_3()
                    .items_center()
                    .child(self.render_cover(&selection, cx))
                    .child(
                        v_flex()
                            .gap_1()
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
            .child(versions)
            .child(
                v_flex()
                    .flex_1()
                    .min_h_0()
                    .gap_1()
                    .child(Self::caption(tr!("detail.options"), cx))
                    .child(
                        div()
                            .relative()
                            .flex_1()
                            .min_h_0()
                            .child(
                                Self::framed(cx)
                                    .id("drawer-options")
                                    .size_full()
                                    .px_3()
                                    .py_2()
                                    .overflow_y_scroll()
                                    .track_scroll(self.options_scroll.handle())
                                    .text_size(BODY_TEXT)
                                    .line_height(LINE_HEIGHT)
                                    .text_color(c.text)
                                    .children(m.options.iter().map(|line| {
                                        let header = line.starts_with('●');
                                        div()
                                            .when(header, |d| d.font_weight(FontWeight::SEMIBOLD))
                                            .child(line.clone())
                                    })),
                            )
                            .child(self.options_scroll.driver()),
                    ),
            );

        v_flex()
            .id("detail-drawer")
            .size_full()
            .px_3()
            .pb_3()
            .gap_3()
            .bg(c.surface)
            .border_l_1()
            .border_color(c.border)
            .shadow_lg()
            .occlude()
            .child(
                h_flex()
                    .min_h(px(48.))
                    .py_2()
                    .flex_shrink_0()
                    .gap_2()
                    .child(
                        v_flex()
                            .id("drawer-title")
                            .flex_1()
                            .min_w_0()
                            .tooltip(text_tooltip(trainer_full_name(
                                &m.display_name,
                                &m.display_subtitle,
                                &m.name,
                            )))
                            .child(
                                div()
                                    .truncate()
                                    .text_size(TITLE_TEXT)
                                    .font_weight(FontWeight::BOLD)
                                    .text_color(c.text)
                                    .child(trainer_name(&m.display_name, &m.name)),
                            )
                            // The English title under a translated name.
                            .when(!m.display_subtitle.is_empty(), |title| {
                                title.child(
                                    div()
                                        .truncate()
                                        .text_size(BODY_TEXT)
                                        .text_color(c.text_secondary)
                                        .child(m.display_subtitle.clone()),
                                )
                            }),
                    )
                    .child(close),
            )
            .child(div().h(px(1.)).flex_shrink_0().bg(c.border))
            .map(|drawer| {
                if !busy {
                    // Fades in once per selection's loaded details.
                    let id = SharedString::from(format!("detail-{}", m.url));
                    return drawer.child(motion::fade_in(body, id, motion::FADE_MS, cx));
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

#[cfg(test)]
mod tests {
    // Not `super::*`: that brings in GPUI's `test` macro, which shadows `#[test]`.
    use super::{COVER_MAX, cover_size, short_version_label, short_version_labels};

    #[test]
    fn version_labels_keep_the_part_that_differs() {
        assert_eq!(
            short_version_label("Elden.Ring.Nightreign.v1.01-v1.03.1.Plus.26.Trainer-FLiNG"),
            "v1.01-v1.03.1 Plus 26"
        );
        assert_eq!(
            short_version_label("Elden Ring v1.02 Plus 30 Trainer"),
            "v1.02 Plus 30"
        );
        // "V" starting a word of the game name is not a version.
        assert_eq!(
            short_version_label("V.Rising.v1.0.Plus.20.Trainer"),
            "v1.0 Plus 20"
        );
        assert_eq!(
            short_version_label("Early Access Trainer"),
            "Early Access Trainer"
        );
        let labels = [
            "Game.v1.0.Trainer".to_owned(),
            "Game.v1.0.Trainer-FLiNG".to_owned(),
        ];
        assert_eq!(short_version_labels(&labels), labels, "would look the same");
    }

    fn png(dir: &std::path::Path, w: u32, h: u32) -> std::path::PathBuf {
        let mut bytes = b"\x89PNG\r\n\x1a\n\0\0\0\x0dIHDR".to_vec();
        bytes.extend_from_slice(&w.to_be_bytes());
        bytes.extend_from_slice(&h.to_be_bytes());
        let path = dir.join(format!("{w}x{h}.png"));
        std::fs::write(&path, bytes).unwrap();
        path
    }

    #[test]
    fn cover_fits_box_keeping_aspect_ratio() {
        let dir = std::env::temp_dir().join(format!("fling-cover-size-{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        assert_eq!(cover_size(&png(&dir, 330, 440)), (105., 140.));
        assert_eq!(cover_size(&png(&dir, 440, 200)), (110., 50.));
        assert_eq!(cover_size(&dir.join("missing.png")), COVER_MAX);
        let _ = std::fs::remove_dir_all(&dir);
    }
}
