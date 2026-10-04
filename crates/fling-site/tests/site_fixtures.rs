//! Parsers against pages saved from flingtrainer.com on 2026-10-03
//! (`tests/fixtures/`). Re-save them when the site layout changes.

use fling_core::text::format_modifier_name;
use fling_site::parser::{parse_modifier_detail, parse_modifier_list, parse_recently_updated};

fn fixture(name: &str) -> String {
    let path = std::path::Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("tests/fixtures")
        .join(name);
    std::fs::read_to_string(path).unwrap()
}

#[test]
fn search_results_page() {
    let list = parse_modifier_list(&fixture("search_elden_ring.html"), "Elden Ring");
    let names: Vec<_> = list.iter().map(|m| format_modifier_name(&m.name)).collect();
    assert_eq!(
        names,
        [
            "Elden Ring Shadow Of The Erdtree Trainer",
            "Elden Ring Nightreign Trainer"
        ]
    );
    let nightreign = &list[1];
    assert_eq!(
        nightreign.url,
        "https://flingtrainer.com/trainer/elden-ring-nightreign-trainer/"
    );
    assert_eq!(nightreign.last_update, "2025-12-18");
    assert!(
        nightreign.screenshot_url.ends_with(".jpg"),
        "{}",
        nightreign.screenshot_url
    );
}

#[test]
fn homepage_recently_updated() {
    let list = parse_recently_updated(&fixture("homepage.html"));
    assert_eq!(list.len(), 15);
    let first = &list[0];
    assert_eq!(
        first.name,
        "Dynasty Warriors 3: Complete Edition Remastered Trainer"
    );
    assert_eq!(first.last_update, "2026-10-01");
    assert_eq!(first.options_count, 29);
    assert_eq!(first.game_version, "v1.0+");
    assert!(
        list.iter()
            .all(|m| m.url.starts_with("https://flingtrainer.com/trainer/"))
    );
    assert!(list.iter().all(|m| m.last_update.len() == 10));
}

#[test]
fn trainer_detail_page() {
    let detail = parse_modifier_detail(&fixture("detail_elden_ring_nightreign.html"));
    assert_eq!(detail.game_version, "v1.01-v1.03.1+");
    assert_eq!(detail.last_update, "2025.12.18");
    let labels: Vec<_> = detail.versions.iter().map(|v| v.label.as_str()).collect();
    assert_eq!(
        labels,
        [
            "Elden.Ring.Nightreign.v1.01-v1.03.1.Plus.26.Trainer-FLiNG",
            "Elden.Ring.Nightreign.v1.01-v1.03.Plus.26.Trainer-FLiNG",
            "Elden.Ring.Nightreign.v1.01.Plus.26.Trainer-FLiNG",
        ]
    );
    assert!(
        detail
            .versions
            .iter()
            .all(|v| v.url.starts_with("https://flingtrainer.com/downloads/"))
    );
    assert_eq!(detail.options[0], "● Basic Options");
    assert!(
        detail
            .options
            .contains(&"• Num 1 – God Mode/Ignore Hits".to_owned())
    );
    assert!(
        detail
            .options
            .contains(&"• Ctrl+Num 3 – Won't Lose Runes When Player Dies".to_owned())
    );
    // No "Options: N" on the page, so the count includes the four headers.
    assert_eq!(detail.options_count as usize, detail.options.len());
    assert!(
        detail
            .screenshot_url
            .starts_with("https://flingtrainer.com/wp-content/uploads/")
    );
}
