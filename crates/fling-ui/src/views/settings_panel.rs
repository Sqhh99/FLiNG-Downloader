//! The settings dialog: appearance, download folder, language and about/updates.

use fling_app::{Command, Language, UpdateSource, UpdateState};
use gpui_kit::assets::IconName;
use gpui_kit::component::Disableable as _;
use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::progress::Progress;
use gpui_kit::component::select::{Select, SelectEvent, SelectState};
use gpui_kit::component::switch::Switch;
use gpui_kit::component::{Icon, IndexPath, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::{language_name, source_name, theme_name, tr, update_status};
use crate::state::AppModel;
use crate::theme::{self, THEME_COUNT, palette};
use crate::views::widgets::{card, icon_button};

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
    source_select: Entity<TextSelect>,
    language: Language,
    _subscriptions: Vec<Subscription>,
}

fn language_items() -> Vec<SharedString> {
    Language::ALL.iter().map(|l| language_name(*l)).collect()
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
                }
                cx.notify();
            }),
        ];
        Self {
            model,
            section: Section::Appearance,
            language_select,
            source_select,
            language: settings.language,
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

    fn section_title(title: SharedString, cx: &App) -> impl IntoElement {
        div()
            .text_lg()
            .font_weight(FontWeight::SEMIBOLD)
            .text_color(palette(cx).text)
            .child(title)
    }

    fn render_appearance(&self, cx: &mut Context<Self>) -> Div {
        let c = palette(cx);
        let current = self.model.read(cx).settings.theme;
        let swatches =
            (0..THEME_COUNT).map(|i| {
                let (bg, fg, primary) = theme::preview(i);
                let selected = i == current;
                v_flex()
                    .id(("theme", i))
                    .w(px(72.))
                    .h(px(50.))
                    .rounded(px(6.))
                    .border_2()
                    .border_color(if selected { c.primary } else { c.border })
                    .bg(bg)
                    .overflow_hidden()
                    .child(div().h(px(8.)).bg(primary))
                    .child(
                        h_flex()
                            .flex_1()
                            .justify_center()
                            .gap_1()
                            .text_xs()
                            .text_color(fg)
                            .when(selected, |d| d.child(Icon::new(IconName::Check).xsmall()))
                            .child(theme_name(i)),
                    )
                    .on_click(cx.listener(move |this, _, _, cx| {
                        this.model.read(cx).send(Command::SetTheme(i))
                    }))
            });
        v_flex()
            .gap_3()
            .child(Self::section_title(tr!("settings.appearance_title"), cx))
            .child(
                card(tr!("settings.theme"), cx)
                    .child(div().flex().flex_wrap().gap_2().children(swatches)),
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
        v_flex()
            .gap_3()
            .child(Self::section_title(tr!("settings.download_title"), cx))
            .child(
                card(tr!("settings.download_dir"), cx)
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
        v_flex()
            .gap_3()
            .child(Self::section_title(tr!("settings.language_title"), cx))
            .child(
                card(tr!("settings.ui_language"), cx)
                    .child(div().w(px(200.)).child(Select::new(&self.language_select))),
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
        if database {
            let current = if state.current_version.is_empty() {
                tr!("common.unknown")
            } else {
                state.current_version.clone().into()
            };
            details.push(tr!("settings.current_version", version = current));
        }
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

        card(title, cx)
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
            )
            .child(
                div()
                    .text_sm()
                    .text_color(c.text)
                    .child(update_status(&state, database)),
            )
            .when(!details.is_empty(), |d| {
                d.child(
                    div().text_xs().text_color(c.text_muted).child(
                        details
                            .iter()
                            .map(|s| s.to_string())
                            .collect::<Vec<_>>()
                            .join("  ·  "),
                    ),
                )
            })
            .when(state.downloading, |d| {
                d.child(Progress::new((id, 1usize)).value(state.progress * 100.0))
            })
            .child(
                h_flex()
                    .gap_2()
                    .child(
                        Button::new((id, 2usize))
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
        v_flex()
            .gap_3()
            .child(
                h_flex()
                    .gap_3()
                    .child(img(crate::assets::APP_LOGO).size(px(48.)))
                    .child(
                        v_flex()
                            .child(
                                div()
                                    .text_lg()
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
                            .child(
                                div()
                                    .text_xs()
                                    .text_color(c.text_muted)
                                    .child(tr!("settings.author")),
                            ),
                    ),
            )
            .child(
                Button::new("github")
                    .ghost()
                    .small()
                    .icon(Icon::new(IconName::Github))
                    .label(REPOSITORY_URL)
                    .on_click(|_, _, cx| cx.open_url(REPOSITORY_URL)),
            )
            .child(
                card(tr!("settings.update_source"), cx)
                    .child(div().w(px(200.)).child(Select::new(&self.source_select)))
                    .child(
                        div()
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
        v_flex()
            .id("settings-panel")
            .w(px(640.))
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
                            .w(px(150.))
                            .h_full()
                            .p_2()
                            .gap_1()
                            .border_r_1()
                            .border_color(c.border)
                            .bg(c.surface)
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
                            .id("settings-content")
                            .flex_1()
                            .h_full()
                            .p_4()
                            .overflow_y_scroll()
                            .child(content),
                    ),
            )
    }
}
