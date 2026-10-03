//! The search tab: search box with suggestions, sort selector and results.

use std::time::Duration;

use fling_app::{Command, Language, SortOrder, Suggestion};
use gpui_kit::assets::IconName;
use gpui_kit::component::Disableable as _;
use gpui_kit::component::button::{Button, ButtonVariants as _};
use gpui_kit::component::input::{Input, InputEvent, InputState};
use gpui_kit::component::select::{Select, SelectEvent, SelectState};
use gpui_kit::component::spinner::Spinner;
use gpui_kit::component::{IndexPath, Sizable as _, h_flex, v_flex};
use gpui_kit::prelude::FluentBuilder as _;
use gpui_kit::*;

use crate::i18n::tr;
use crate::state::AppModel;
use crate::theme::palette;
use crate::views::widgets::{Column, empty_table, icon_button, table_header, table_row, text_cell};

const MAX_SUGGESTIONS: usize = 8;
const SUGGESTION_ROW: f32 = 32.;

pub enum SearchPageEvent {
    /// Details requested for result row `n` (button or double click).
    ToggleDetails(usize),
}

impl EventEmitter<SearchPageEvent> for SearchPage {}

type SortSelect = SelectState<Vec<SharedString>>;

pub struct SearchPage {
    model: Entity<AppModel>,
    input: Entity<InputState>,
    sort: Entity<SortSelect>,
    suggestions: Vec<Suggestion>,
    highlighted: Option<usize>,
    show_suggestions: bool,
    selected_row: Option<usize>,
    language: Language,
    _subscriptions: Vec<Subscription>,
}

fn sort_labels() -> Vec<SharedString> {
    vec![tr!("sort.recent"), tr!("sort.name"), tr!("sort.options")]
}

fn sort_from_index(index: usize) -> SortOrder {
    match index {
        1 => SortOrder::Name,
        2 => SortOrder::OptionsCount,
        _ => SortOrder::RecentlyUpdated,
    }
}

impl SearchPage {
    pub fn new(model: Entity<AppModel>, window: &mut Window, cx: &mut Context<Self>) -> Self {
        let input = cx.new(|cx| InputState::new(window, cx).placeholder(tr!("search.placeholder")));
        let sort =
            cx.new(|cx| SelectState::new(sort_labels(), Some(IndexPath::default()), window, cx));
        let language = model.read(cx).settings.language;

        let subscriptions = vec![
            cx.subscribe_in(
                &input,
                window,
                |this, input, event: &InputEvent, window, cx| match event {
                    InputEvent::Change => {
                        let text = input.read(cx).value().to_string();
                        this.suggestions = this.model.read(cx).suggestions(&text, MAX_SUGGESTIONS);
                        this.highlighted = None;
                        this.show_suggestions = !this.suggestions.is_empty();
                        cx.notify();
                    }
                    InputEvent::PressEnter { .. } => this.submit(window, cx),
                    InputEvent::Blur => {
                        // Late enough for a click on a suggestion to land first.
                        cx.spawn(async move |this, cx| {
                            cx.background_executor()
                                .timer(Duration::from_millis(150))
                                .await;
                            let _ = this.update(cx, |this, cx| {
                                this.show_suggestions = false;
                                cx.notify();
                            });
                        })
                        .detach();
                    }
                    InputEvent::Focus => {}
                },
            ),
            cx.subscribe_in(
                &sort,
                window,
                |this, sort, _: &SelectEvent<Vec<SharedString>>, _, cx| {
                    let index = sort.read(cx).selected_index(cx).map_or(0, |ix| ix.row);
                    this.model
                        .read(cx)
                        .send(Command::SetSort(sort_from_index(index)));
                },
            ),
            cx.observe_in(&model, window, |this, model, window, cx| {
                let language = model.read(cx).settings.language;
                if language != this.language {
                    this.language = language;
                    this.relabel(window, cx);
                }
                cx.notify();
            }),
        ];

        Self {
            model,
            input,
            sort,
            suggestions: Vec::new(),
            highlighted: None,
            show_suggestions: false,
            selected_row: None,
            language,
            _subscriptions: subscriptions,
        }
    }

    fn relabel(&mut self, window: &mut Window, cx: &mut Context<Self>) {
        self.input.update(cx, |input, cx| {
            input.set_placeholder(tr!("search.placeholder"), window, cx)
        });
        self.sort.update(cx, |sort, cx| {
            let selected = sort.selected_index(cx);
            sort.set_items(sort_labels(), window, cx);
            sort.set_selected_index(selected, window, cx);
        });
    }

    /// Enter or the search button: pick the highlighted suggestion, else
    /// search the text (an empty box reloads the recent list).
    fn submit(&mut self, window: &mut Window, cx: &mut Context<Self>) {
        if self.show_suggestions
            && let Some(index) = self.highlighted
        {
            self.pick(index, window, cx);
            return;
        }
        self.show_suggestions = false;
        let text = self.input.read(cx).value().trim().to_owned();
        let command = if text.is_empty() {
            Command::FetchRecent
        } else {
            Command::Search(text)
        };
        self.model.read(cx).send(command);
        cx.notify();
    }

    fn pick(&mut self, index: usize, window: &mut Window, cx: &mut Context<Self>) {
        let Some(suggestion) = self.suggestions.get(index).cloned() else {
            return;
        };
        // `set_value` emits no change event, so the popup does not reopen.
        self.input.update(cx, |input, cx| {
            input.set_value(suggestion.input_text.clone(), window, cx)
        });
        self.show_suggestions = false;
        self.model
            .read(cx)
            .send(Command::Search(suggestion.search_keyword));
        cx.notify();
    }

    fn show_all(&mut self, window: &mut Window, cx: &mut Context<Self>) {
        self.input
            .update(cx, |input, cx| input.set_value("", window, cx));
        self.show_suggestions = false;
        self.model.read(cx).send(Command::FetchRecent);
    }

    fn on_key(&mut self, event: &KeyDownEvent, window: &mut Window, cx: &mut Context<Self>) {
        if !self.show_suggestions || self.suggestions.is_empty() {
            return;
        }
        let last = self.suggestions.len() - 1;
        match event.keystroke.key.as_str() {
            "down" => self.highlighted = Some(self.highlighted.map_or(0, |i| (i + 1).min(last))),
            "up" => self.highlighted = Some(self.highlighted.map_or(last, |i| i.saturating_sub(1))),
            "escape" => self.show_suggestions = false,
            "enter" if self.highlighted.is_some() => self.submit(window, cx),
            _ => return,
        }
        cx.stop_propagation();
        cx.notify();
    }

    fn render_suggestions(&self, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let height = (self.suggestions.len() as f32 * SUGGESTION_ROW + 8.).min(208.);
        v_flex()
            .id("suggestions")
            .absolute()
            .top(px(40.))
            .left_0()
            .right_0()
            .h(px(height))
            .p_1()
            .overflow_y_scroll()
            .rounded(px(6.))
            .border_1()
            .border_color(c.border)
            .bg(c.surface)
            .shadow_md()
            .occlude()
            .children(self.suggestions.iter().enumerate().map(|(i, s)| {
                let highlighted = self.highlighted == Some(i);
                div()
                    .id(("suggestion", i))
                    .h(px(SUGGESTION_ROW))
                    .flex_shrink_0()
                    .px_2()
                    .flex()
                    .items_center()
                    .rounded(px(4.))
                    .truncate()
                    .text_sm()
                    .text_color(c.text)
                    .when(highlighted, |d| d.bg(c.selected))
                    .hover(|d| d.bg(c.hover))
                    .child(s.display_text.clone())
                    .on_click(cx.listener(move |this, _, window, cx| this.pick(i, window, cx)))
            }))
    }

    fn render_table(&self, cx: &mut Context<Self>) -> impl IntoElement {
        let c = palette(cx);
        let model = self.model.read(cx);
        let columns = [
            Column::new(tr!("col.game_name"), 3.),
            Column::new(tr!("col.update_date"), 2.),
            Column::new(tr!("col.game_version"), 2.),
            Column::new(tr!("col.options"), 2.),
            Column::new(tr!("col.actions"), 1.),
        ];
        let rows: Vec<_> = model
            .results
            .iter()
            .enumerate()
            .map(|(i, m)| {
                let cells = vec![
                    text_cell(m.name.clone()),
                    text_cell(m.last_update.clone()),
                    text_cell(m.game_version.clone()),
                    text_cell(m.options_count.to_string()),
                    icon_button(("details", i), IconName::Info, tr!("detail.details"))
                        .on_click(cx.listener(move |this, _, _, cx| {
                            this.selected_row = Some(i);
                            cx.emit(SearchPageEvent::ToggleDetails(i));
                        }))
                        .into_any_element(),
                ];
                table_row(
                    ("result", i),
                    &columns,
                    cells,
                    i,
                    self.selected_row == Some(i),
                    cx,
                )
                .on_click(cx.listener(move |this, event: &ClickEvent, _, cx| {
                    this.selected_row = Some(i);
                    if event.click_count() >= 2 {
                        cx.emit(SearchPageEvent::ToggleDetails(i));
                    }
                    cx.notify();
                }))
            })
            .collect();
        let loading = model.search_loading;
        let empty = rows.is_empty();

        v_flex()
            .relative()
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
                            .id("results")
                            .flex_1()
                            .overflow_y_scroll()
                            .children(rows),
                    )
                }
            })
            .when(loading, |table| {
                table.child(
                    v_flex()
                        .id("loading")
                        .absolute()
                        .inset_0()
                        .items_center()
                        .justify_center()
                        .gap_2()
                        .bg(Hsla {
                            a: 0.92,
                            ..c.surface
                        })
                        .occlude()
                        .child(Spinner::new().large())
                        .child(div().text_color(c.text_muted).child(tr!("common.loading"))),
                )
            })
    }
}

impl Render for SearchPage {
    fn render(&mut self, _: &mut Window, cx: &mut Context<Self>) -> impl IntoElement {
        let loading = self.model.read(cx).search_loading;
        v_flex()
            .size_full()
            .p_3()
            .gap_3()
            .child(
                h_flex()
                    .gap_2()
                    .child(
                        div()
                            .relative()
                            .flex_1()
                            .capture_key_down(cx.listener(Self::on_key))
                            .child(Input::new(&self.input).cleanable(true).disabled(loading))
                            .when(self.show_suggestions && !self.suggestions.is_empty(), |d| {
                                d.child(self.render_suggestions(cx))
                            }),
                    )
                    .child(
                        Button::new("search")
                            .primary()
                            .label(tr!("search.button"))
                            .disabled(loading)
                            .on_click(cx.listener(|this, _, window, cx| this.submit(window, cx))),
                    )
                    .child(
                        div()
                            .w(px(130.))
                            .child(Select::new(&self.sort).disabled(loading)),
                    )
                    .child(
                        Button::new("show-all")
                            .label(tr!("search.show_all"))
                            .disabled(loading)
                            .on_click(cx.listener(|this, _, window, cx| this.show_all(window, cx))),
                    ),
            )
            .child(self.render_table(cx))
    }
}
