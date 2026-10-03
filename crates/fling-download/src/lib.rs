//! Downloading trainers and remembering them.
//!
//! [`DownloadQueue`] owns the session's download tasks (port of the task
//! queue in `Backend.cpp`); [`Library`] owns `downloaded_modifiers.json`.

mod files;
mod library;
mod queue;

pub use files::{clean_url, correct_file_extension};
pub use library::{DeleteError, Library, RunTarget};
pub use queue::{DownloadQueue, EventSink, MAX_CONCURRENT_DOWNLOADS, QueueEvent};
