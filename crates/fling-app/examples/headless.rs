//! Drives the real backend against the live site, without a UI, in a
//! throwaway data directory (never the user's AppData).
//!
//! ```text
//! cargo run -p fling-app --example headless -- recent
//! cargo run -p fling-app --example headless -- search 艾尔登法环
//! ```

use std::time::Duration;

use fling_app::{BackendConfig, Command, CoverState, DetailState, Event, start};
use fling_config::AppPaths;

fn main() {
    tracing_subscriber::fmt()
        .with_env_filter(std::env::var("RUST_LOG").unwrap_or_else(|_| "warn".into()))
        .init();
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
    let mut config = BackendConfig::for_app("0.0.0-headless".into()).expect("http client");
    config.cover_detector = fling_app::bundled_cover_detector(paths.clone());
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
            Event::Selection(s) => {
                println!(
                    "selection: detail {:?}, cover {:?}, screenshot {:?}",
                    s.detail, s.cover, s.modifier.screenshot_url
                );
                let detail_done = matches!(
                    s.detail,
                    DetailState::Ready | DetailState::Empty | DetailState::Error
                );
                if detail_done && s.cover != CoverState::Loading {
                    for v in &s.modifier.versions {
                        println!("  version {} -> {}", v.label, v.url);
                    }
                    println!("  {} option line(s)", s.modifier.options.len());
                    break;
                }
            }
            _ => {}
        }
    }
}
