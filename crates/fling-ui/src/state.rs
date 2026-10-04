//! The UI's mirror of backend state. One entity, updated only from backend
//! events; views read it and send commands through it.

use fling_app::*;
use gpui_kit::{App, AppContext as _, Context, Entity};

use crate::{i18n, theme};

pub struct AppModel {
    handle: BackendHandle,
    pub settings: SettingsSnapshot,
    pub results: Vec<ModifierInfo>,
    /// Bumped whenever `results` is replaced (new search, re-sort), so views
    /// can replay entrance animations and reset scrolling.
    pub results_generation: u64,
    pub search_loading: bool,
    pub selection: Option<Selection>,
    pub tasks: Vec<DownloadTask>,
    pub library: Vec<DownloadedModifier>,
    pub app_update: UpdateState,
    pub database_update: UpdateState,
    /// `(name, file path, serial)` of the last failed delete; the serial lets
    /// the banner restart its timer for a repeated failure.
    pub delete_error: Option<(String, String, u64)>,
    delete_serial: u64,
}

impl AppModel {
    pub fn new(handle: BackendHandle, events: EventReceiver, cx: &mut App) -> Entity<Self> {
        let settings = handle.initial_settings().clone();
        cx.new(|cx| {
            cx.spawn(async move |this, cx| {
                while let Ok(event) = events.recv().await {
                    if this
                        .update(cx, |model: &mut AppModel, cx| model.apply(event, cx))
                        .is_err()
                    {
                        break;
                    }
                }
            })
            .detach();
            Self {
                handle,
                settings,
                results: Vec::new(),
                results_generation: 0,
                search_loading: false,
                selection: None,
                tasks: Vec::new(),
                library: Vec::new(),
                app_update: UpdateState::default(),
                database_update: UpdateState::default(),
                delete_error: None,
                delete_serial: 0,
            }
        })
    }

    pub fn send(&self, command: Command) {
        self.handle.send(command);
    }

    pub fn suggestions(&self, keyword: &str, max: usize) -> Vec<Suggestion> {
        self.handle.suggestions(keyword, max)
    }

    pub fn active_downloads(&self) -> usize {
        self.tasks
            .iter()
            .filter(|t| t.status == TaskStatus::Downloading)
            .count()
    }

    fn apply(&mut self, event: Event, cx: &mut Context<Self>) {
        match event {
            Event::SearchLoading(loading) => self.search_loading = loading,
            Event::Results(results) => {
                self.results = results;
                self.results_generation += 1;
            }
            Event::Selection(selection) => self.selection = Some(selection),
            Event::Tasks(tasks) => self.tasks = tasks,
            Event::Library(library) => self.library = library,
            Event::DeleteFailed { name, file_path } => {
                self.delete_serial += 1;
                self.delete_error = Some((name, file_path, self.delete_serial));
            }
            Event::AppUpdate(state) => self.app_update = state,
            Event::DatabaseUpdate(state) => self.database_update = state,
            Event::Settings(settings) => {
                if settings.language != self.settings.language {
                    i18n::set_language(settings.language);
                }
                if settings.theme != self.settings.theme {
                    theme::apply(settings.theme, cx);
                }
                self.settings = settings;
            }
            Event::Open(path) => {
                if let Err(err) = opener::open(&path) {
                    tracing::warn!(?path, %err, "failed to open path");
                }
            }
            Event::Quit => cx.quit(),
        }
        cx.notify();
    }

    pub fn clear_delete_error(&mut self, serial: u64, cx: &mut Context<Self>) {
        if self
            .delete_error
            .as_ref()
            .is_some_and(|(_, _, s)| *s == serial)
        {
            self.delete_error = None;
            cx.notify();
        }
    }
}
