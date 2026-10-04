//! The window: title bar, tabs, pages and the overlays above them.

use std::time::{Duration, Instant};

use fling_app::Command;
use gpui_kit::assets::IconName;
use gpui_kit::component::{TitleBar, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::state::AppModel;
use crate::theme::palette;
use crate::views::detail_drawer::{DetailDrawer, DrawerEvent};
use crate::views::downloads_panel::DownloadsPanel;
use crate::views::library_page::LibraryPage;
use crate::views::motion;
use crate::views::search_page::{SearchPage, SearchPageEvent};
use crate::views::settings_panel::{SettingsEvent, SettingsPanel};
use crate::views::widgets::{icon_button, scrim};

#[derive(Clone, Copy, PartialEq, Eq)]
enum Tab {
    Search,
    Library,
}

pub struct Root {
    model: Entity<AppModel>,
    focus: FocusHandle,
    search: Entity<SearchPage>,
    library: Entity<LibraryPage>,
    drawer: Entity<DetailDrawer>,
    downloads: Entity<DownloadsPanel>,
    settings: Entity<SettingsPanel>,
    tab: Tab,
    /// Result row whose details are showing, if the drawer is open.
    drawer_row: Option<usize>,
    downloads_open: bool,
    /// When a click outside last closed the download list; a click on the
    /// title-bar button right after must not reopen it.
    downloads_closed_at: Option<Instant>,
    settings_open: bool,
    debug_drawer_done: bool,
    _subscriptions: Vec<Subscription>,
}

/// `FLING_DEBUG_OPEN`: which screen a debug build opens at startup.
pub(crate) fn debug_open() -> Option<String> {
    std::env::var("FLING_DEBUG_OPEN").ok()
}

impl Root {
    pub fn new(model: Entity<AppModel>, window: &mut Window, cx: &mut Context<Self>) -> Self {
        let search = cx.new(|cx| SearchPage::new(model.clone(), window, cx));
        let library = cx.new(|cx| LibraryPage::new(model.clone(), cx));
        let drawer = cx.new(|cx| DetailDrawer::new(model.clone(), window, cx));
        let downloads = cx.new(|cx| DownloadsPanel::new(model.clone(), cx));
        let settings = cx.new(|cx| SettingsPanel::new(model.clone(), window, cx));

        let subscriptions = vec![
            cx.observe(&model, |this, model, cx| {
                // Debug builds: FLING_DEBUG_OPEN=drawer[:row] opens that result's
                // details once results arrive, for screenshot checks.
                let debug_row = debug_open().and_then(|v| {
                    v.strip_prefix("drawer")
                        .map(|rest| rest.trim_start_matches(':').parse().unwrap_or(0))
                });
                if cfg!(debug_assertions)
                    && let Some(row) = debug_row
                    && !this.debug_drawer_done
                    && model.read(cx).results.len() > row
                {
                    this.debug_drawer_done = true;
                    this.drawer_row = Some(row);
                    model.read(cx).send(Command::Select(row));
                }
                cx.notify();
            }),
            cx.subscribe(
                &search,
                |this, _, event: &SearchPageEvent, cx| match event {
                    SearchPageEvent::ToggleDetails(row) => {
                        if this.drawer_row == Some(*row) {
                            this.drawer_row = None;
                        } else {
                            this.drawer_row = Some(*row);
                            this.model.read(cx).send(Command::Select(*row));
                        }
                        cx.notify();
                    }
                },
            ),
            cx.subscribe(&drawer, |this, _, event: &DrawerEvent, cx| {
                match event {
                    DrawerEvent::Close => this.drawer_row = None,
                    DrawerEvent::DownloadStarted => this.downloads_open = true,
                }
                cx.notify();
            }),
            cx.subscribe(&settings, |this, _, _: &SettingsEvent, cx| {
                this.settings_open = false;
                cx.notify();
            }),
        ];

        let focus = cx.focus_handle();
        focus.focus(window, cx);
        Self {
            model,
            focus,
            search,
            library,
            drawer,
            downloads,
            settings,
            tab: Tab::Search,
            drawer_row: None,
            downloads_open: cfg!(debug_assertions) && debug_open().as_deref() == Some("downloads"),
            downloads_closed_at: None,
            settings_open: cfg!(debug_assertions)
                && debug_open().is_some_and(|v| v.starts_with("settings")),
            debug_drawer_done: false,
            _subscriptions: subscriptions,
        }
    }

    fn toggle_downloads(&mut self, cx: &mut Context<Self>) {
        let just_closed = self
            .downloads_closed_at
            .is_some_and(|t| t.elapsed() < Duration::from_millis(250));
        self.downloads_open = !self.downloads_open && !just_closed;
        cx.notify();
    }

    fn on_key(&mut self, event: &KeyDownEvent, _: &mut Window, cx: &mut Context<Self>) {
        if event.keystroke.key != "escape" {
            return;
        }
        if self.settings_open {
            self.settings_open = false;
        } else if self.downloads_open {
            self.downloads_open = false;
        } else if self.drawer_row.is_some() {
            self.drawer_row = None;
        } else {
            return;
        }
        cx.notify();
    }

    fn render_title_bar(&self, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let active = self.model.read(cx).active_downloads();
        let badge = (active > 0).then(|| {
            div()
                .absolute()
                .top(px(-2.))
                .right(px(-2.))
                .min_w(px(16.))
                .h(px(16.))
                .px(px(3.))
                .rounded_full()
                .bg(c.danger)
                .flex()
                .items_center()
                .justify_center()
                .text_size(px(10.))
                .text_color(white())
                .child(if active > 9 {
                    "9+".to_owned()
                } else {
                    active.to_string()
                })
        });
        TitleBar::new()
            .child(
                h_flex()
                    .gap_2()
                    .child(img(crate::assets::APP_LOGO).size(px(20.)))
                    .child(
                        div()
                            .text_sm()
                            .font_weight(FontWeight::BOLD)
                            .text_color(c.text)
                            .child(tr!("app.title")),
                    ),
            )
            .child(
                // The title bar is a caption drag area; occluding keeps the
                // OS from turning clicks on these buttons into window drags.
                h_flex()
                    .id("title-actions")
                    .occlude()
                    .gap_1()
                    .pr_2()
                    .child(
                        div()
                            .relative()
                            .child(
                                icon_button(
                                    "downloads",
                                    IconName::Download,
                                    tr!("titlebar.downloads"),
                                )
                                .on_click(cx.listener(|this, _, _, cx| this.toggle_downloads(cx))),
                            )
                            .children(badge),
                    )
                    .child(
                        icon_button("settings", IconName::Settings, tr!("titlebar.settings"))
                            .on_click(cx.listener(|this, _, _, cx| {
                                this.settings_open = true;
                                cx.notify();
                            })),
                    ),
            )
    }

    fn render_tab(
        &self,
        tab: Tab,
        label: SharedString,
        cx: &mut Context<Self>,
    ) -> impl IntoElement {
        let c = palette(cx);
        let active = self.tab == tab;
        div()
            .id(label.clone())
            .h_full()
            .px_4()
            .flex()
            .items_center()
            .border_b_2()
            .border_color(if active {
                c.primary
            } else {
                transparent_black()
            })
            .text_sm()
            .when(active, |d| {
                d.font_weight(FontWeight::BOLD).text_color(c.primary_text)
            })
            .when(!active, |d| {
                d.text_color(c.text_secondary)
                    .hover(|d| d.text_color(c.text))
            })
            .child(label)
            .on_click(cx.listener(move |this, _, _, cx| {
                this.tab = tab;
                cx.notify();
            }))
    }
}

impl Render for Root {
    fn render(&mut self, window: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let drawer_width = px(400.).min(window.viewport_size().width * 0.4);
        let page = match self.tab {
            Tab::Search => self.search.clone().into_any_element(),
            Tab::Library => self.library.clone().into_any_element(),
        };
        // A new id per tab replays the fade when switching.
        let page = motion::fade_in(
            div().size_full().child(page),
            ("page", self.tab as usize),
            motion::FADE_MS,
            cx,
        );

        // Overlays stay mounted while they animate out.
        let drawer = motion::presence(
            "drawer",
            self.tab == Tab::Search && self.drawer_row.is_some(),
            motion::DRAWER_MS,
            window,
            cx,
        );
        let downloads = motion::presence(
            "downloads",
            self.downloads_open,
            motion::POPUP_MS,
            window,
            cx,
        );
        let settings = motion::presence(
            "settings",
            self.settings_open,
            motion::OVERLAY_MS,
            window,
            cx,
        );

        v_flex()
            .id("root")
            .track_focus(&self.focus)
            .on_key_down(cx.listener(Self::on_key))
            .relative()
            .size_full()
            .bg(c.background)
            .text_color(c.text)
            .child(self.render_title_bar(cx))
            .child(
                h_flex()
                    .h(px(40.))
                    .flex_shrink_0()
                    .bg(c.surface)
                    .border_b_1()
                    .border_color(c.border)
                    .child(self.render_tab(Tab::Search, tr!("tab.search"), cx))
                    .child(self.render_tab(Tab::Library, tr!("tab.library"), cx)),
            )
            .child(div().flex_1().min_h_0().child(page))
            // The drawer spans the whole window height, like the Qt build, and
            // slides in from the right edge.
            .when_some(drawer, |root, shown| {
                root.child(
                    div()
                        .absolute()
                        .top_0()
                        .bottom_0()
                        .right(drawer_width * (shown - 1.0))
                        .w(drawer_width)
                        .child(self.drawer.clone()),
                )
            })
            // The download list drops down from under the title bar.
            .when_some(downloads, |root, shown| {
                root.child(
                    div()
                        .absolute()
                        .top(px(38. - (1.0 - shown) * 8.))
                        .right(px(8.))
                        .opacity(shown)
                        .when(self.downloads_open, |d| {
                            d.on_mouse_down_out(cx.listener(|this, _, _, cx| {
                                this.downloads_open = false;
                                this.downloads_closed_at = Some(Instant::now());
                                cx.notify();
                            }))
                        })
                        .child(self.downloads.clone()),
                )
            })
            // Settings fade in over a dimmed window while rising slightly.
            .when_some(settings, |root, shown| {
                root.child(
                    scrim("settings-scrim")
                        .opacity(shown)
                        .flex()
                        .items_center()
                        .justify_center()
                        .child(
                            div()
                                .relative()
                                .top(px((1.0 - shown) * 24.))
                                .child(self.settings.clone()),
                        ),
                )
            })
    }
}
