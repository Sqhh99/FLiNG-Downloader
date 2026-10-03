//! Drives the real backend against the live site, without a UI, in a
//! throwaway data directory (never the user's AppData).
//!
//! ```text
//! cargo run -p fling-app --example headless -- recent
//! cargo run -p fling-app --example headless -- search 艾尔登法环
//! ```

use std::sync::Arc;
use std::time::Duration;

use fling_app::{BackendConfig, Command, DetailState, Event, start};
use fling_config::AppPaths;

fn main() {
    tracing_subscriber::fmt().with_env_filter("warn").init();
    let args: Vec<String> = std::env::args().skip(1).collect();
    let command = match args.first().map(String::as_str) {
        Some("search") => {
            let term = args[1..].join(" ");
            println!("searching {term:?}");
            Command::Search(term)
        }
        Some("recent") | None => Command::FetchRecent,
        Some(other) => panic!("unknown mode {other:?}; use `recent` or `search <term>`"),
    };

    let dir = tempfile::tempdir().expect("temp dir");
    let mut paths = AppPaths::rooted(dir.path());
    // Find resources/fling_translations.db from the repository checkout.
    paths.exe_dir = std::path::Path::new(env!("CARGO_MANIFEST_DIR")).join("../..");
    let mut config =
        BackendConfig::for_app("0.0.0-headless".into(), Arc::new(|| None)).expect("http client");
    config.paths = paths;
    config.startup = None;

    let (handle, events) = start(config).expect("backend");
    handle.send(command);

    let deadline = std::time::Instant::now() + Duration::from_secs(90);
    let mut selected = false;
    while std::time::Instant::now() < deadline {
        let Ok(event) = events.recv_blocking() else {
            break;
        };
        match event {
            Event::Results(results) if !selected => {
                println!("{} result(s)", results.len());
                for m in results.iter().take(10) {
                    println!(
                        "  {} | {} | {} options | {}",
                        m.name, m.last_update, m.options_count, m.game_version
                    );
                }
                if results.is_empty() {
                    break;
                }
                selected = true;
                handle.send(Command::Select(0));
            }
            Event::Selection(s)
                if matches!(
                    s.detail,
                    DetailState::Ready | DetailState::Empty | DetailState::Error
                ) =>
            {
                println!("detail of {:?}: {:?}", s.modifier.name, s.detail);
                for v in &s.modifier.versions {
                    println!("  version {} -> {}", v.label, v.url);
                }
                println!("  {} option line(s)", s.modifier.options.len());
                break;
            }
            _ => {}
        }
    }
}
