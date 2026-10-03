//! CN/JA → canonical English title resolution.
//!
//! [`TranslationDatabase`] picks between the bundled SQLite file and the
//! AppData override and reads its rows; [`GameMappings`] indexes those rows for
//! search translation and [`suggestions`].

mod database;
mod mappings;
mod suggestions;

pub use database::{
    DatabaseError, GameRecord, Metadata, TranslationDatabase, validate_database_file,
};
pub use mappings::GameMappings;
pub use suggestions::{Suggestion, SuggestionIndex};

#[cfg(any(test, feature = "test-util"))]
pub mod test_util;
