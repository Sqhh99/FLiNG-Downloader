//! The network seam. Every crate that talks to the network takes an
//! `Arc<dyn HttpClient>`; the app wires in [`ReqwestClient`] and tests wire in
//! [`fake::FakeHttpClient`] (feature `test-util`).

use std::path::PathBuf;
use std::sync::Arc;
use std::time::Duration;

use futures::future::BoxFuture;
pub use tokio_util::sync::CancellationToken;

mod reqwest_client;
pub use reqwest_client::ReqwestClient;

#[cfg(any(test, feature = "test-util"))]
pub mod fake;

/// The browser user agent the Qt build sent; flingtrainer.com serves it normally.
pub const USER_AGENT: &str = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 \
     (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

/// Total-time limit of a GET, and idle limit of a download.
pub const DEFAULT_TIMEOUT: Duration = Duration::from_secs(30);

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum GetError {
    #[error("request timed out")]
    TimedOut,
    #[error("server replied: {0}")]
    Status(u16),
    #[error("{0}")]
    Transport(String),
}

#[derive(Debug, Clone, PartialEq, Eq, thiserror::Error)]
pub enum DownloadError {
    #[error("Cannot create file: {0}")]
    CreateFile(String),
    #[error("Failed to write downloaded data to disk: {0}")]
    Write(String),
    #[error("Server returned HTML page instead of file - download link may be invalid")]
    HtmlPage,
    #[error("Downloaded file is empty - server may have returned no content")]
    Empty,
    #[error("Server replied: {0}")]
    Status(u16),
    #[error("{0}")]
    Transport(String),
    #[error("Download timed out")]
    TimedOut,
    #[error("Operation canceled")]
    Canceled,
}

impl DownloadError {
    /// HTTP status of the failing response, if one was received.
    pub fn status(&self) -> Option<u16> {
        match self {
            Self::Status(status) => Some(*status),
            _ => None,
        }
    }
}

/// `(bytes received including the resume offset, total size or 0 when unknown)`.
pub type ProgressFn = Arc<dyn Fn(u64, u64) + Send + Sync>;

/// A file download to `dest`.
#[derive(Clone)]
pub struct DownloadRequest {
    pub url: String,
    pub dest: PathBuf,
    /// Bytes already in `dest`; when non-zero a `Range` request appends to it.
    pub resume_from: u64,
    /// Keep the partial file when the download is canceled or times out, so it
    /// can be resumed. Other failures always delete it.
    pub keep_partial: bool,
    pub cancel: CancellationToken,
    pub progress: Option<ProgressFn>,
}

impl std::fmt::Debug for DownloadRequest {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("DownloadRequest")
            .field("url", &self.url)
            .field("dest", &self.dest)
            .field("resume_from", &self.resume_from)
            .field("keep_partial", &self.keep_partial)
            .finish_non_exhaustive()
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct DownloadOutcome {
    /// Final HTTP status (200, 206, or 416 for an already-complete resume).
    pub status: u16,
}

/// HTTP operations the app needs. Futures are `'static` so callers can spawn them.
pub trait HttpClient: Send + Sync {
    /// GET `url` and return the body of a 2xx response.
    fn get(&self, url: &str) -> BoxFuture<'static, Result<Vec<u8>, GetError>>;

    /// Stream `request.url` into `request.dest`.
    fn download(
        &self,
        request: DownloadRequest,
    ) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>>;
}

/// `GET` and decode the body as UTF-8 (lossily, like `QString::fromUtf8`).
pub async fn get_text(client: &dyn HttpClient, url: &str) -> Result<String, GetError> {
    let body = client.get(url).await?;
    Ok(String::from_utf8_lossy(&body).into_owned())
}

/// `scheme://host[:port]/` of `url`, or `None` for a relative/invalid URL.
///
/// flingtrainer.com hands trainer files only to same-origin referrers. The path
/// is never included: it can carry a download token that a redirect target has
/// no business seeing.
pub fn origin_referer(url: &str) -> Option<String> {
    let parsed = reqwest::Url::parse(url).ok()?;
    let host = parsed.host_str().filter(|h| !h.is_empty())?;
    Some(match parsed.port() {
        Some(port) => format!("{}://{host}:{port}/", parsed.scheme()),
        None => format!("{}://{host}/", parsed.scheme()),
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn referer_keeps_only_the_origin() {
        assert_eq!(
            origin_referer("https://flingtrainer.com/downloads/Ekm-bdRFI0A_lCbUwTEb0g,,")
                .as_deref(),
            Some("https://flingtrainer.com/")
        );
        assert_eq!(
            origin_referer("http://localhost:8080/a/b.zip").as_deref(),
            Some("http://localhost:8080/")
        );
        assert_eq!(
            origin_referer("https://x.com:443/a").as_deref(),
            Some("https://x.com/")
        );
    }

    #[test]
    fn referer_is_none_without_scheme_and_host() {
        assert_eq!(origin_referer("/relative/path.zip"), None);
        assert_eq!(origin_referer(""), None);
    }
}
