//! Everything specific to flingtrainer.com.
//!
//! [`parser`] turns pages into [`fling_core::ModifierInfo`] values and is pure;
//! [`SiteClient`] fetches pages through an [`fling_net::HttpClient`] and runs
//! the search, detail and recently-updated workflows on top of the parsers.

pub mod parser;
mod recent_cache;
mod site;

pub use recent_cache::{load_recent_cache, save_recent_cache};
pub use site::{DEFAULT_BASE_URL, SiteClient, sort_by_relevance};
