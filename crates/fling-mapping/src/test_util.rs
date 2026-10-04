//! Throwaway SQLite fixtures for tests in this and downstream crates.

use std::fs;
use std::path::Path;

use rusqlite::{Connection, params};

/// `(english, normalized_english, chinese_simplified, japanese)`.
pub type Row<'a> = (&'a str, &'a str, &'a str, &'a str);

/// Creates (replacing) a valid translation database. An empty
/// `release_tag` / `schema_version` leaves that metadata key out.
pub fn create_database(path: &Path, release_tag: &str, schema_version: &str, rows: &[Row<'_>]) {
    create(path, release_tag, schema_version, true, rows);
}

/// Same, but the `games` table lacks `normalized_english`.
pub fn create_database_missing_normalized_english(path: &Path, release_tag: &str) {
    create(
        path,
        release_tag,
        "1",
        false,
        &[("Sample Game", "", "示例游戏", "サンプルゲーム")],
    );
}

fn create(path: &Path, tag: &str, schema: &str, with_normalized: bool, rows: &[Row<'_>]) {
    if let Some(dir) = path.parent() {
        fs::create_dir_all(dir).unwrap();
    }
    let _ = fs::remove_file(path);
    let db = Connection::open(path).unwrap();
    db.execute_batch("CREATE TABLE metadata (key TEXT PRIMARY KEY, value TEXT NOT NULL);")
        .unwrap();
    if with_normalized {
        db.execute_batch(
            "CREATE TABLE games (english TEXT NOT NULL, normalized_english TEXT, \
             chinese_simplified TEXT, japanese TEXT);",
        )
        .unwrap();
    } else {
        db.execute_batch(
            "CREATE TABLE games (english TEXT NOT NULL, chinese_simplified TEXT, japanese TEXT);",
        )
        .unwrap();
    }
    for (key, value) in [("release_tag", tag), ("schema_version", schema)] {
        if !value.is_empty() {
            db.execute(
                "INSERT INTO metadata(key, value) VALUES (?1, ?2)",
                params![key, value],
            )
            .unwrap();
        }
    }
    for (english, normalized, chinese, japanese) in rows {
        let nullable = |s: &str| (!s.is_empty()).then(|| s.to_owned());
        if with_normalized {
            db.execute(
                "INSERT INTO games(english, normalized_english, chinese_simplified, japanese) \
                 VALUES (?1, ?2, ?3, ?4)",
                params![
                    english,
                    nullable(normalized),
                    nullable(chinese),
                    nullable(japanese)
                ],
            )
            .unwrap();
        } else {
            db.execute(
                "INSERT INTO games(english, chinese_simplified, japanese) VALUES (?1, ?2, ?3)",
                params![english, nullable(chinese), nullable(japanese)],
            )
            .unwrap();
        }
    }
}
