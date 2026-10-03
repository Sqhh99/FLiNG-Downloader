//! Domain types and pure, IO-free helpers shared by every FLiNG Downloader crate.
//!
//! Nothing in this crate touches the network or the file system, so everything
//! here is unit-testable without fixtures.

pub mod file_kind;
pub mod model;
pub mod text;
pub mod version;

pub use model::*;
