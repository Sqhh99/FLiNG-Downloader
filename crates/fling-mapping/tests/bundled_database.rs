//! Checks against the real `resources/fling_translations.db` shipped with the app.

use std::path::PathBuf;

use fling_core::Language;
use fling_mapping::{GameMappings, SuggestionIndex, TranslationDatabase, validate_database_file};

fn bundled() -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR")).join("../../resources/fling_translations.db")
}

fn load() -> (tempfile::TempDir, TranslationDatabase) {
    let dir = tempfile::tempdir().unwrap();
    let db = TranslationDatabase::new(dir.path().join("fling_translations.db"), Some(bundled()));
    (dir, db)
}

#[test]
fn bundled_database_is_valid_and_populated() {
    assert_eq!(validate_database_file(&bundled()), Ok(()));
    let (_dir, db) = load();
    assert!(!db.current_release_tag().is_empty());
    assert!(db.load_all_games().len() > 1000);
}

#[test]
fn ace_combat_resolves_from_every_script() {
    let (_dir, db) = load();
    let mappings = GameMappings::from_records(&db.load_all_games());
    let expected = mappings
        .translate_to_english("エースコンバット7 スカイズ・アンノウン")
        .expect("japanese title resolves");
    assert!(
        expected.to_lowercase().contains("ace combat 7"),
        "{expected}"
    );
    for variant in [
        "エースコンバット7・スカイズ・アンノウン",
        "ace combat 7 skies unknown",
    ] {
        assert_eq!(
            mappings.translate_to_english(variant).as_deref(),
            Some(expected.as_str())
        );
    }
    assert_eq!(mappings.translate_for_search("ace combat"), None);
}

#[test]
fn suggestions_work_on_bundled_rows() {
    let (_dir, db) = load();
    let index = SuggestionIndex::from_records(&db.load_all_games());
    let results = index.suggest("ace combat", 8, Language::Chinese);
    assert!(!results.is_empty());
    assert!(results.len() <= 8);
    assert!(
        results
            .iter()
            .all(|s| s.search_keyword.to_lowercase().contains("ace combat"))
    );
}
