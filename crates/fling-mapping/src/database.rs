//! The translation database file: validation, bundled-vs-override resolution,
//! reading rows and installing updates.
//!
//! Port of `TranslationDatabase.cpp`. Error strings are kept verbatim because
//! update status messages surface them.

use std::cmp::Ordering;
use std::fs;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::SystemTime;

use fling_core::version::{compare_versions, normalize_version};
use parking_lot::Mutex;
use rusqlite::{Connection, OpenFlags, OptionalExtension};

const SUPPORTED_SCHEMA_VERSION: &str = "1";
const REQUIRED_GAME_COLUMNS: [&str; 4] = [
    "english",
    "normalized_english",
    "chinese_simplified",
    "japanese",
];

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum DatabaseError {
    #[error("Database file does not exist")]
    Missing,
    #[error("Database schema is incomplete")]
    SchemaIncomplete,
    #[error("Required column missing: games.{0}")]
    MissingColumn(&'static str),
    #[error("Database metadata is missing release_tag")]
    MissingReleaseTag,
    #[error("Unsupported schema_version: {0}")]
    UnsupportedSchema(String),
    #[error("Failed to create database directory")]
    CreateDirectory,
    #[error("Failed to copy database update into place")]
    Copy,
    #[error("Failed to activate updated database")]
    Activate,
    #[error("{0}")]
    Sqlite(String),
}

impl From<rusqlite::Error> for DatabaseError {
    fn from(err: rusqlite::Error) -> Self {
        Self::Sqlite(err.to_string())
    }
}

/// One `games` row. NULL columns read as empty strings.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct GameRecord {
    pub english: String,
    pub normalized_english: String,
    pub chinese_simplified: String,
    pub japanese: String,
}

#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Metadata {
    pub release_tag: String,
    pub schema_version: String,
}

/// File identity used to notice changes on disk without re-validating.
#[derive(Debug, Clone, PartialEq, Eq)]
struct FileStamp {
    modified: Option<SystemTime>,
    size: Option<u64>,
}

impl FileStamp {
    fn of(path: Option<&Path>) -> Self {
        let meta = path
            .and_then(|p| fs::metadata(p).ok())
            .filter(|m| m.is_file());
        Self {
            modified: meta.as_ref().and_then(|m| m.modified().ok()),
            size: meta.as_ref().map(|m| m.len()),
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
struct CacheKey {
    override_stamp: FileStamp,
    bundled_stamp: FileStamp,
}

#[derive(Default)]
struct Cache {
    path: Option<(CacheKey, Option<PathBuf>)>,
    games: Option<(CacheKey, Arc<Vec<GameRecord>>)>,
}

/// The bundled database plus the AppData override that updates write.
///
/// Resolving the active file opens and validates both candidates, so the
/// answer (and the row list) is cached until either file changes on disk or an
/// update is installed.
pub struct TranslationDatabase {
    override_path: PathBuf,
    bundled_path: Option<PathBuf>,
    cache: Mutex<Cache>,
}

impl TranslationDatabase {
    /// `bundled_path` is `None` when no bundled copy was found next to the exe.
    pub fn new(override_path: PathBuf, bundled_path: Option<PathBuf>) -> Self {
        Self {
            override_path,
            bundled_path,
            cache: Mutex::new(Cache::default()),
        }
    }

    pub fn override_path(&self) -> &Path {
        &self.override_path
    }

    pub fn bundled_path(&self) -> Option<&Path> {
        self.bundled_path.as_deref()
    }

    fn cache_key(&self) -> CacheKey {
        CacheKey {
            override_stamp: FileStamp::of(Some(&self.override_path)),
            bundled_stamp: FileStamp::of(self.bundled_path.as_deref()),
        }
    }

    fn invalidate_cache(&self) {
        *self.cache.lock() = Cache::default();
    }

    /// The database to read, or `None` when neither candidate is valid.
    pub fn database_path(&self) -> Option<PathBuf> {
        let key = self.cache_key();
        if let Some((cached_key, path)) = &self.cache.lock().path
            && *cached_key == key
        {
            return path.clone();
        }
        let path = self.resolve_database_path();
        self.cache.lock().path = Some((key, path.clone()));
        path
    }

    pub fn is_available(&self) -> bool {
        self.database_path().is_some()
    }

    fn resolve_database_path(&self) -> Option<PathBuf> {
        let override_valid = self.override_path.is_file()
            && match validate_database_file(&self.override_path) {
                Ok(()) => true,
                Err(err) => {
                    tracing::warn!(%err, "override translation database invalid, falling back to bundled copy");
                    false
                }
            };
        let bundled_valid = self.bundled_path.as_deref().is_some_and(|p| {
            validate_database_file(p)
                .inspect_err(|err| tracing::warn!(%err, "bundled translation database invalid"))
                .is_ok()
        });

        match (override_valid, bundled_valid) {
            (true, true) => {
                let bundled = self.bundled_path.clone().expect("bundled path is valid");
                let override_tag = read_metadata(&self.override_path).release_tag;
                let bundled_tag = read_metadata(&bundled).release_tag;
                let override_version = normalize_version(&override_tag);
                let bundled_version = normalize_version(&bundled_tag);
                if compare_versions(override_version, bundled_version) != Ordering::Less {
                    Some(self.override_path.clone())
                } else {
                    tracing::warn!(
                        bundled_version,
                        override_version,
                        "override translation database is older than the bundled copy; using bundled"
                    );
                    Some(bundled)
                }
            }
            (true, false) => Some(self.override_path.clone()),
            (false, true) => self.bundled_path.clone(),
            (false, false) => None,
        }
    }

    /// Every row of the active database, ordered by English title (case-insensitive).
    pub fn load_all_games(&self) -> Arc<Vec<GameRecord>> {
        let key = self.cache_key();
        if let Some((cached_key, games)) = &self.cache.lock().games
            && *cached_key == key
        {
            return games.clone();
        }

        let Some(path) = self.database_path() else {
            tracing::warn!(
                bundled = ?self.bundled_path,
                override_path = ?self.override_path,
                "no valid translation database found"
            );
            return Arc::default();
        };
        let games = match read_games(&path) {
            Ok(games) => Arc::new(games),
            Err(err) => {
                tracing::warn!(?path, %err, "failed to load games");
                return Arc::default();
            }
        };
        self.cache.lock().games = Some((key, games.clone()));
        games
    }

    /// Metadata of the active database (empty when none is available).
    pub fn read_metadata(&self) -> Metadata {
        self.database_path()
            .map(|p| read_metadata(&p))
            .unwrap_or_default()
    }

    /// `metadata.release_tag` of the active database, as stored.
    pub fn current_release_tag(&self) -> String {
        self.read_metadata().release_tag
    }

    /// Validates `source`, then atomically replaces the override with it.
    pub fn install_override(&self, source: &Path) -> Result<(), DatabaseError> {
        validate_database_file(source)?;

        let target = &self.override_path;
        if let Some(dir) = target.parent() {
            fs::create_dir_all(dir).map_err(|_| DatabaseError::CreateDirectory)?;
        }
        let temp = append_extension(target, "tmp");
        let _ = fs::remove_file(&temp);
        fs::copy(source, &temp).map_err(|_| DatabaseError::Copy)?;

        if let Err(err) = validate_database_file(&temp) {
            let _ = fs::remove_file(&temp);
            return Err(err);
        }

        let _ = fs::remove_file(target);
        let renamed = fs::rename(&temp, target);
        // The copy keeps the source's timestamp on Windows, so the file stamp
        // alone cannot be trusted to notice the swap.
        self.invalidate_cache();
        if renamed.is_err() {
            let _ = fs::remove_file(&temp);
            return Err(DatabaseError::Activate);
        }
        Ok(())
    }
}

fn append_extension(path: &Path, ext: &str) -> PathBuf {
    let mut os = path.as_os_str().to_owned();
    os.push(".");
    os.push(ext);
    PathBuf::from(os)
}

fn open_read_only(path: &Path) -> rusqlite::Result<Connection> {
    Connection::open_with_flags(
        path,
        OpenFlags::SQLITE_OPEN_READ_ONLY | OpenFlags::SQLITE_OPEN_NO_MUTEX,
    )
}

fn has_table(db: &Connection, name: &str) -> rusqlite::Result<bool> {
    db.query_row(
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name = ?1 LIMIT 1",
        [name],
        |_| Ok(()),
    )
    .optional()
    .map(|row| row.is_some())
}

fn read_metadata_from(db: &Connection) -> rusqlite::Result<Metadata> {
    let mut metadata = Metadata::default();
    let mut stmt = db.prepare(
        "SELECT key, value FROM metadata WHERE key IN ('release_tag', 'schema_version')",
    )?;
    let rows = stmt.query_map([], |row| {
        Ok((
            row.get::<_, Option<String>>(0)?.unwrap_or_default(),
            row.get::<_, Option<String>>(1)?.unwrap_or_default(),
        ))
    })?;
    for row in rows {
        let (key, value) = row?;
        match key.as_str() {
            "release_tag" => metadata.release_tag = value,
            "schema_version" => metadata.schema_version = value,
            _ => {}
        }
    }
    Ok(metadata)
}

/// Reads `release_tag` / `schema_version` from any database file; empty on error.
pub(crate) fn read_metadata(path: &Path) -> Metadata {
    open_read_only(path)
        .and_then(|db| read_metadata_from(&db))
        .inspect_err(|err| tracing::warn!(?path, %err, "failed to read translation metadata"))
        .unwrap_or_default()
}

/// Checks that `path` is a translation database this build can read.
pub fn validate_database_file(path: &Path) -> Result<(), DatabaseError> {
    if !path.is_file() {
        return Err(DatabaseError::Missing);
    }
    let db = open_read_only(path)?;
    if !has_table(&db, "games")? || !has_table(&db, "metadata")? {
        return Err(DatabaseError::SchemaIncomplete);
    }

    let mut stmt = db.prepare("PRAGMA table_info(games)")?;
    let columns = stmt
        .query_map([], |row| row.get::<_, String>(1))?
        .collect::<rusqlite::Result<Vec<_>>>()?;
    if let Some(missing) = REQUIRED_GAME_COLUMNS
        .iter()
        .find(|required| !columns.iter().any(|c| c == *required))
    {
        return Err(DatabaseError::MissingColumn(missing));
    }

    let metadata = read_metadata_from(&db)?;
    if metadata.release_tag.is_empty() {
        return Err(DatabaseError::MissingReleaseTag);
    }
    if !metadata.schema_version.is_empty() && metadata.schema_version != SUPPORTED_SCHEMA_VERSION {
        return Err(DatabaseError::UnsupportedSchema(metadata.schema_version));
    }
    Ok(())
}

fn read_games(path: &Path) -> rusqlite::Result<Vec<GameRecord>> {
    let db = open_read_only(path)?;
    let mut stmt = db.prepare(
        "SELECT english, normalized_english, chinese_simplified, japanese \
         FROM games ORDER BY english COLLATE NOCASE ASC",
    )?;
    let text = |row: &rusqlite::Row<'_>, i| -> rusqlite::Result<String> {
        Ok(row.get::<_, Option<String>>(i)?.unwrap_or_default())
    };
    stmt.query_map([], |row| {
        Ok(GameRecord {
            english: text(row, 0)?,
            normalized_english: text(row, 1)?,
            chinese_simplified: text(row, 2)?,
            japanese: text(row, 3)?,
        })
    })?
    .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::test_util::{create_database, create_database_missing_normalized_english};

    struct Fixture {
        _dir: tempfile::TempDir,
        db: TranslationDatabase,
        bundled: PathBuf,
    }

    /// A bundled copy at `bundled_tag` with three games, and an empty override slot.
    fn fixture(bundled_tag: &str) -> Fixture {
        let dir = tempfile::tempdir().unwrap();
        let bundled = dir.path().join("resources/fling_translations.db");
        create_database(
            &bundled,
            bundled_tag,
            "1",
            &[
                ("Beta Game", "beta game", "贝塔", ""),
                ("alpha Game", "alpha game", "阿尔法", ""),
                ("Gamma", "gamma", "伽马", ""),
            ],
        );
        let override_path = dir.path().join("data/fling_translations.db");
        let db = TranslationDatabase::new(override_path, Some(bundled.clone()));
        Fixture {
            _dir: dir,
            db,
            bundled,
        }
    }

    #[test]
    fn prefers_bundled_copy_when_override_is_older() {
        let f = fixture("v0.0.3");
        create_database(f.db.override_path(), "v0.0.2", "1", &[]);
        assert_eq!(f.db.database_path(), Some(f.bundled.clone()));
    }

    #[test]
    fn prefers_override_when_it_is_newer_or_equal() {
        let f = fixture("v0.0.3");
        create_database(f.db.override_path(), "0.0.3", "1", &[]);
        assert_eq!(f.db.database_path().as_deref(), Some(f.db.override_path()));

        create_database(f.db.override_path(), "999.0.0", "1", &[]);
        assert_eq!(f.db.current_release_tag(), "999.0.0");
    }

    #[test]
    fn rejects_database_missing_required_games_columns() {
        let f = fixture("v0.0.3");
        create_database_missing_normalized_english(f.db.override_path(), "999.0.0");
        let err = validate_database_file(f.db.override_path()).unwrap_err();
        assert!(
            err.to_string().contains("games.normalized_english"),
            "{err}"
        );
        assert_eq!(f.db.database_path(), Some(f.bundled.clone()));
    }

    #[test]
    fn rejects_unsupported_schema_version() {
        let f = fixture("v0.0.3");
        create_database(f.db.override_path(), "999.0.0", "2", &[]);
        let err = validate_database_file(f.db.override_path()).unwrap_err();
        assert!(
            err.to_string().contains("Unsupported schema_version"),
            "{err}"
        );
    }

    #[test]
    fn accepts_missing_schema_version_and_rejects_missing_file() {
        let f = fixture("v0.0.3");
        create_database(f.db.override_path(), "999.0.0", "", &[]);
        assert_eq!(validate_database_file(f.db.override_path()), Ok(()));
        assert_eq!(
            validate_database_file(Path::new("does/not/exist.db")),
            Err(DatabaseError::Missing)
        );
    }

    #[test]
    fn cached_resolution_follows_override_changes_on_disk() {
        let f = fixture("v0.0.3");
        assert_eq!(f.db.database_path(), Some(f.bundled.clone()));
        let names: Vec<_> =
            f.db.load_all_games()
                .iter()
                .map(|g| g.english.clone())
                .collect();
        assert_eq!(names, ["alpha Game", "Beta Game", "Gamma"]);

        create_database(
            f.db.override_path(),
            "999.0.0",
            "1",
            &[("Sample Game", "samplegame", "示例游戏", "サンプルゲーム")],
        );
        assert_eq!(f.db.database_path().as_deref(), Some(f.db.override_path()));
        let games = f.db.load_all_games();
        assert_eq!(games.len(), 1);
        assert_eq!(games[0].english, "Sample Game");

        fs::remove_file(f.db.override_path()).unwrap();
        assert_eq!(f.db.database_path(), Some(f.bundled.clone()));
        assert_eq!(f.db.load_all_games().len(), 3);
    }

    #[test]
    fn install_override_validates_and_activates() {
        let f = fixture("v0.0.3");
        let dir = tempfile::tempdir().unwrap();
        let bad = dir.path().join("bad.db");
        create_database(&bad, "", "1", &[]);
        assert_eq!(
            f.db.install_override(&bad),
            Err(DatabaseError::MissingReleaseTag)
        );

        let good = dir.path().join("good.db");
        create_database(&good, "v1.0.0", "1", &[("Sample Game", "", "示例游戏", "")]);
        f.db.install_override(&good).unwrap();
        assert_eq!(f.db.database_path().as_deref(), Some(f.db.override_path()));
        assert_eq!(f.db.load_all_games()[0].english, "Sample Game");
        assert!(!append_extension(f.db.override_path(), "tmp").exists());
    }

    #[test]
    fn nothing_available_without_valid_candidates() {
        let dir = tempfile::tempdir().unwrap();
        let db = TranslationDatabase::new(dir.path().join("none.db"), None);
        assert!(!db.is_available());
        assert!(db.load_all_games().is_empty());
        assert_eq!(db.current_release_tag(), "");
    }
}
