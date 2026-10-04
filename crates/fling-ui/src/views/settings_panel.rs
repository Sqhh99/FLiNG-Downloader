//! The settings dialog: appearance, download folder, language and about/updates.

use fling_app::{Command, Language, TrainerNameLanguage, UpdateSource, UpdateState};
use gpui_kit::assets::IconName;
use gpui_kit::component::Disableable as _;
use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::progress::Progress;
use gpui_kit::component::select::{Select, SelectEvent, SelectState};
use gpui_kit::component::switch::Switch;
use gpui_kit::component::{Icon, IndexPath, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::{
    language_name, source_name, theme_name, tr, trainer_name_language_name, update_status,
};
use crate::state::AppModel;
use crate::theme::{self, THEME_COUNT, palette};
use crate::views::motion;
use crate::views::smooth_scroll::SmoothScroll;
use crate::views::widgets::icon_button;

const REPOSITORY_URL: &str = "https://github.com/Sqhh99/FLiNG-Downloader";

#[derive(Clone, Copy, PartialEq, Eq)]
enum Section {
    Appearance,
    Download,
    Language,
    About,
}

pub enum SettingsEvent {
    Close,
}

impl EventEmitter<SettingsEvent> for SettingsPanel {}

type TextSelect = SelectState<Vec<SharedString>>;

pub struct SettingsPanel {
    model: Entity<AppModel>,
    section: Section,
    language_select: Entity<TextSelect>,
    trainer_names_select: Entity<TextSelect>,
    source_select: Entity<TextSelect>,
    language: Language,
    content_scroll: SmoothScroll,
    _subscriptions: Vec<Subscription>,
}

fn language_items() -> Vec<SharedString> {
    Language::ALL.iter().map(|l| language_name(*l)).collect()
}

fn trainer_name_items() -> Vec<SharedString> {
    TrainerNameLanguage::ALL
        .iter()
        .map(|choice| trainer_name_language_name(*choice))
        .collect()
}

fn trainer_name_index(choice: TrainerNameLanguage) -> usize {
    TrainerNameLanguage::ALL
        .iter()
        .position(|c| *c == choice)
        .unwrap_or(0)
}

fn source_items() -> Vec<SharedString> {
    vec!["GitHub".into(), "Gitee".into()]
}

fn source_index(source: UpdateSource) -> usize {
    match source {
        UpdateSource::GitHub => 0,
        UpdateSource::Gitee => 1,
    }
}

impl SettingsPanel {
    pub fn new(model: Entity<AppModel>, window: &mut Window, cx: &mut Context<Self>) -> Self {
        let settings = model.read(cx).settings.clone();
        let language_select = cx.new(|cx| {
            SelectState::new(
                language_items(),
                Some(IndexPath::new(settings.language.index() as usize)),
                window,
                cx,
            )
        });
        let trainer_names_select = cx.new(|cx| {
            SelectState::new(
                trainer_name_items(),
                Some(IndexPath::new(trainer_name_index(
                    settings.trainer_name_language,
                ))),
                window,
                cx,
            )
        });
        let source_select = cx.new(|cx| {
            SelectState::new(
                source_items(),
                Some(IndexPath::new(source_index(settings.update_source))),
                window,
                cx,
            )
        });
        let subscriptions = vec![
            cx.subscribe_in(
                &language_select,
                window,
                |this, select, _: &SelectEvent<Vec<SharedString>>, _, cx| {
                    let index = select.read(cx).selected_index(cx).map_or(0, |ix| ix.row);
                    this.model
                        .read(cx)
                        .send(Command::SetLanguage(Language::from_index(index as i64)));
                },
            ),
            cx.subscribe_in(
                &trainer_names_select,
                window,
                |this, select, _: &SelectEvent<Vec<SharedString>>, _, cx| {
                    let index = select.read(cx).selected_index(cx).map_or(0, |ix| ix.row);
                    let choice = TrainerNameLanguage::ALL
                        .get(index)
                        .copied()
                        .unwrap_or_default();
                    this.model
                        .read(cx)
                        .send(Command::SetTrainerNameLanguage(choice));
                },
            ),
            cx.subscribe_in(
                &source_select,
                window,
                |this, select, _: &SelectEvent<Vec<SharedString>>, _, cx| {
                    let source = if select
                        .read(cx)
                        .selected_index(cx)
                        .is_some_and(|ix| ix.row == 1)
                    {
                        UpdateSource::Gitee
                    } else {
                        UpdateSource::GitHub
                    };
                    this.model.read(cx).send(Command::SetUpdateSource(source));
                },
            ),
            cx.observe_in(&model, window, |this, model, window, cx| {
                let language = model.read(cx).settings.language;
                if language != this.language {
                    this.language = language;
                    this.language_select.update(cx, |select, cx| {
                        select.set_items(language_items(), window, cx);
                        select.set_selected_index(
                            Some(IndexPath::new(language.index() as usize)),
                            window,
                            cx,
                        );
                    });
                    // The option names are in the UI language too.
                    let choice = model.read(cx).settings.trainer_name_language;
                    this.trainer_names_select.update(cx, |select, cx| {
                        select.set_items(trainer_name_items(), window, cx);
                        select.set_selected_index(
                            Some(IndexPath::new(trainer_name_index(choice))),
                            window,
                            cx,
                        );
                    });
                }
                cx.notify();
            }),
        ];
        Self {
            model,
            section: match crate::views::root::debug_open().as_deref() {
                Some("settings-about") if cfg!(debug_assertions) => Section::About,
                Some("settings-download") if cfg!(debug_assertions) => Section::Download,
                Some("settings-language") if cfg!(debug_assertions) => Section::Language,
                _ => Section::Appearance,
            },
            language_select,
            trainer_names_select,
            source_select,
            language: settings.language,
            content_scroll: SmoothScroll::new(),
            _subscriptions: subscriptions,
        }
    }

    fn pick_folder(&mut self, cx: &mut Context<Self>) {
        let current = self.model.read(cx).settings.download_directory.clone();
        let dialog = rfd::AsyncFileDialog::new()
            .set_title(tr!("settings.pick_folder").to_string())
            .set_directory(current);
        cx.spawn(async move |this, cx| {
            if let Some(folder) = dialog.pick_folder().await {
                let path = folder.path().to_path_buf();
                let _ = this.update(cx, |this, cx| {
                    this.model
                        .read(cx)
                        .send(Command::SetDownloadDirectory(path))
                });
            }
        })
        .detach();
    }

    fn nav_item(
        &self,
        section: Section,
        icon: IconName,
        label: SharedString,
        cx: &mut Context<Self>,
    ) -> impl IntoElement {
        let c = palette(cx);
        let active = self.section == section;
        h_flex()
            .id(label.clone())
            .gap_2()
            .px_3()
            .h(px(36.))
            .rounded(px(6.))
            .text_sm()
            .when(active, |d| {
                d.bg(c.selected)
                    .text_color(c.primary_text)
                    .font_weight(FontWeight::SEMIBOLD)
            })
            .when(!active, |d| d.text_color(c.text).hover(|d| d.bg(c.hover)))
            .child(Icon::new(icon).small())
            .child(label)
            .on_click(cx.listener(move |this, _, _, cx| {
                this.section = section;
                cx.notify();
            }))
    }

    /// Pane heading with the divider line under it.
    fn section(title: SharedString, cx: &App) -> Div {
        let c = palette(cx);
        v_flex().gap_3().child(
            div()
                .pb_2()
                .border_b_1()
                .border_color(c.border)
                .text_lg()
                .font_weight(FontWeight::SEMIBOLD)
                .text_color(c.text)
                .child(title),
        )
    }

    /// A labelled group inside a pane.
    fn field(label: SharedString, cx: &App) -> Div {
        v_flex().gap_2().child(
            div()
                .text_sm()
                .text_color(palette(cx).text_secondary)
                .child(label),
        )
    }

    fn render_appearance(&self, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        let current = self.model.read(cx).settings.theme;
        let swatches =
            (0..THEME_COUNT).map(|i| {
                let (bg, fg, _) = theme::preview(i);
                let selected = i == current;
                div()
                    .id(("theme", i))
                    .relative()
                    .w(px(90.))
                    .h(px(60.))
                    .flex()
                    .items_center()
                    .justify_center()
                    .rounded(px(6.))
                    .border_color(if selected { c.primary } else { c.border })
                    .map(|d| if selected { d.border_2() } else { d.border_1() })
                    .bg(bg)
                    .text_sm()
                    .text_color(fg)
                    .cursor_pointer()
                    .child(theme_name(i))
                    .when(selected, |d| {
                        d.child(
                            div()
                                .absolute()
                                .top(px(-6.))
                                .right(px(-6.))
                                .size(px(18.))
                                .rounded_full()
                                .bg(c.primary)
                                .flex()
                                .items_center()
                                .justify_center()
                                .text_color(c.on_primary)
                                .child(Icon::new(IconName::Check).xsmall()),
                        )
                    })
                    .on_click(cx.listener(move |this, _, _, cx| {
                        this.model.read(cx).send(Command::SetTheme(i))
                    }))
            });
        Self::section(tr!("settings.appearance_title"), cx).child(
            Self::field(tr!("settings.theme"), cx)
                .child(div().flex().flex_wrap().gap_2().pt_1().children(swatches)),
        )
    }

    fn render_download(&self, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        let dir = self
            .model
            .read(cx)
            .settings
            .download_directory
            .to_string_lossy()
            .into_owned();
        let shown = if dir.is_empty() {
            tr!("settings.not_set")
        } else {
            dir.into()
        };
        Self::section(tr!("settings.download_title"), cx).child(
            Self::field(tr!("settings.download_dir"), cx)
                .child(
                    h_flex()
                        .gap_2()
                        .child(
                            div()
                                .flex_1()
                                .min_w_0()
                                .h(px(36.))
                                .px_3()
                                .flex()
                                .items_center()
                                .rounded(px(6.))
                                .border_1()
                                .border_color(c.border)
                                .bg(c.input)
                                .text_sm()
                                .text_color(c.text)
                                .child(div().truncate().child(shown)),
                        )
                        .child(
                            Button::new("browse")
                                .secondary()
                                .label(tr!("settings.browse"))
                                .on_click(cx.listener(|this, _, _, cx| this.pick_folder(cx))),
                        ),
                )
                .child(
                    div()
                        .text_xs()
                        .text_color(c.text_muted)
                        .child(tr!("settings.download_hint")),
                ),
        )
    }

    fn render_language(&self, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        Self::section(tr!("settings.language_title"), cx)
            .child(
                Self::field(tr!("settings.ui_language"), cx)
                    .child(div().w(px(200.)).child(Select::new(&self.language_select))),
            )
            .child(
                Self::field(tr!("settings.trainer_names"), cx)
                    .child(
                        div()
                            .w(px(280.))
                            .child(Select::new(&self.trainer_names_select)),
                    )
                    .child(
                        div()
                            .text_xs()
                            .text_color(c.text_muted)
                            .child(tr!("settings.trainer_names_hint")),
                    ),
            )
    }

    fn update_card(&self, database: bool, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        let model = self.model.read(cx);
        let (state, auto, title): (UpdateState, bool, SharedString) = if database {
            (
                model.database_update.clone(),
                model.settings.auto_check_database_updates,
                tr!("settings.db_updates"),
            )
        } else {
            (
                model.app_update.clone(),
                model.settings.auto_check_app_updates,
                tr!("settings.app_updates"),
            )
        };
        let id = if database { "db" } else { "app" };
        let mut details = Vec::new();
        if !state.latest_version.is_empty() {
            details.push(tr!(
                "settings.latest",
                version = state.latest_version.as_str()
            ));
        }
        if let Some(source) = state.source {
            details.push(tr!("settings.source", source = source_name(source)));
        }
        if !state.published_at.is_empty() {
            details.push(tr!(
                "settings.published",
                date = state.published_at.as_str()
            ));
        }
        let current = (database && !state.current_version.is_empty()).then(|| {
            tr!(
                "settings.current_version",
                version = state.current_version.as_str()
            )
        });

        let check_label = if state.checking {
            tr!("settings.checking")
        } else if database {
            tr!("settings.check_db")
        } else {
            tr!("settings.check")
        };
        let install_label = if state.downloading {
            tr!("settings.downloading")
        } else if database {
            tr!("settings.apply_db")
        } else {
            tr!("settings.install")
        };

        v_flex()
            .gap_1p5()
            .p_3()
            .rounded(px(8.))
            .border_1()
            .border_color(c.border)
            // The section tint (as the navigation column), not a stark card white.
            .bg(c.alternate_row)
            .child(
                h_flex()
                    .justify_between()
                    .child(
                        div()
                            .font_weight(FontWeight::SEMIBOLD)
                            .text_color(c.text)
                            .child(title),
                    )
                    .child(
                        Switch::new((id, 0usize))
                            .label(tr!("settings.auto_check"))
                            .checked(auto)
                            .on_change(cx.listener(move |this, checked: &bool, _, cx| {
                                let command = if database {
                                    Command::SetAutoCheckDatabaseUpdates(*checked)
                                } else {
                                    Command::SetAutoCheckAppUpdates(*checked)
                                };
                                this.model.read(cx).send(command);
                            })),
                    ),
            )
            .when_some(current, |d, current| {
                d.child(div().text_sm().text_color(c.text_secondary).child(current))
            })
            .child(
                div()
                    .text_sm()
                    .text_color(c.text)
                    .child(update_status(&state, database)),
            )
            .when(!details.is_empty(), |d| {
                d.child(
                    h_flex()
                        .gap_3()
                        .flex_wrap()
                        .text_xs()
                        .text_color(c.text_muted)
                        .children(details),
                )
            })
            .when(state.downloading, |d| {
                d.child(Progress::new((id, 1usize)).value(state.progress * 100.0))
            })
            .child(
                h_flex()
                    .pt_1()
                    .gap_2()
                    .child(
                        Button::new((id, 2usize))
                            .secondary()
                            .label(check_label)
                            .disabled(state.checking || state.downloading)
                            .on_click(cx.listener(move |this, _, _, cx| {
                                let command = if database {
                                    Command::CheckDatabaseUpdate
                                } else {
                                    Command::CheckAppUpdate
                                };
                                this.model.read(cx).send(command);
                            })),
                    )
                    .when(state.available, |d| {
                        d.child(
                            Button::new((id, 3usize))
                                .primary()
                                .label(install_label)
                                .disabled(state.downloading)
                                .on_click(cx.listener(move |this, _, _, cx| {
                                    let command = if database {
                                        Command::DownloadDatabaseUpdate
                                    } else {
                                        Command::DownloadAppUpdate
                                    };
                                    this.model.read(cx).send(command);
                                })),
                        )
                    }),
            )
    }

    fn render_about(&self, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        let version = self.model.read(cx).settings.app_version.clone();
        Self::section(tr!("settings.about"), cx)
            .child(
                v_flex()
                    .gap_1()
                    .child(
                        h_flex()
                            .gap_2()
                            .child(
                                div()
                                    .font_weight(FontWeight::BOLD)
                                    .text_color(c.text)
                                    .child(tr!("app.title")),
                            )
                            .child(
                                div()
                                    .text_sm()
                                    .text_color(c.text_secondary)
                                    .child(format!("v{version}")),
                            )
                            .child(div().flex_1())
                            .child(
                                div()
                                    .text_sm()
                                    .text_color(c.text_secondary)
                                    .child(tr!("settings.author")),
                            ),
                    )
                    .child(
                        div()
                            .id("github")
                            .text_sm()
                            .text_color(c.primary_text)
                            .underline()
                            .cursor_pointer()
                            .child(format!("GitHub: {REPOSITORY_URL}"))
                            .on_click(|_, _, cx| cx.open_url(REPOSITORY_URL)),
                    ),
            )
            .child(
                h_flex()
                    .gap_3()
                    .child(
                        div()
                            .text_sm()
                            .text_color(c.text)
                            .child(tr!("settings.update_source")),
                    )
                    .child(div().w(px(160.)).child(Select::new(&self.source_select)))
                    .child(
                        div()
                            .flex_1()
                            .min_w_0()
                            .truncate()
                            .text_xs()
                            .text_color(c.text_muted)
                            .child(tr!("settings.update_source_hint")),
                    ),
            )
            .child(self.update_card(false, cx))
            .child(self.update_card(true, cx))
    }
}

impl Render for SettingsPanel {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let content = match self.section {
            Section::Appearance => self.render_appearance(cx),
            Section::Download => self.render_download(cx),
            Section::Language => self.render_language(cx),
            Section::About => self.render_about(cx),
        };
        // Switching panes fades the new one in.
        let content = motion::fade_in(
            content,
            ("settings-pane", self.section as usize),
            motion::FADE_MS,
            cx,
        );
        v_flex()
            .id("settings-panel")
            .w(px(700.))
            .h(px(560.))
            .rounded(px(8.))
            .border_1()
            .border_color(c.border)
            .bg(c.background)
            .shadow_lg()
            .overflow_hidden()
            .occlude()
            .child(
                h_flex()
                    .h(px(50.))
                    .px_4()
                    .border_b_1()
                    .border_color(c.border)
                    .bg(c.surface)
                    .child(
                        div()
                            .flex_1()
                            .text_lg()
                            .font_weight(FontWeight::SEMIBOLD)
                            .text_color(c.text)
                            .child(tr!("settings.title")),
                    )
                    .child(
                        icon_button("settings-close", IconName::X, tr!("common.close"))
                            .on_click(cx.listener(|_, _, _, cx| cx.emit(SettingsEvent::Close))),
                    ),
            )
            .child(
                h_flex()
                    .flex_1()
                    .min_h_0()
                    .items_start()
                    .child(
                        v_flex()
                            .w(px(160.))
                            .h_full()
                            .m_3()
                            .mr_0()
                            .p_2()
                            .gap_1()
                            .rounded(px(8.))
                            .bg(c.alternate_row)
                            .child(self.nav_item(
                                Section::Appearance,
                                IconName::Palette,
                                tr!("settings.appearance"),
                                cx,
                            ))
                            .child(self.nav_item(
                                Section::Download,
                                IconName::Download,
                                tr!("settings.download"),
                                cx,
                            ))
                            .child(self.nav_item(
                                Section::Language,
                                IconName::Languages,
                                tr!("settings.language"),
                                cx,
                            ))
                            .child(self.nav_item(
                                Section::About,
                                IconName::Info,
                                tr!("settings.about"),
                                cx,
                            )),
                    )
                    .child(
                        div()
                            .relative()
                            .flex_1()
                            .min_w_0()
                            .h_full()
                            .child(
                                div()
                                    .id("settings-content")
                                    .size_full()
                                    .p_4()
                                    .overflow_y_scroll()
                                    .track_scroll(self.content_scroll.handle())
                                    .child(content),
                            )
                            .child(self.content_scroll.driver()),
                    ),
            )
    }
}
