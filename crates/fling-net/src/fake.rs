//! An in-memory [`HttpClient`] for tests: no sockets, no real network.

use std::collections::HashMap;
use std::sync::Arc;

use futures::future::BoxFuture;
use parking_lot::Mutex;

use crate::{DownloadError, DownloadOutcome, DownloadRequest, GetError, HttpClient};

type GetResult = Result<Vec<u8>, GetError>;
type GetHandler = dyn Fn(&str) -> Option<Result<Vec<u8>, GetError>> + Send + Sync;
type DownloadHandler = dyn Fn(DownloadRequest) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>>
    + Send
    + Sync;

/// Answers GETs from a URL → body table (or a handler), and downloads by
/// writing canned bytes. Unmatched GETs fail with 404; every request is logged.
#[derive(Default, Clone)]
pub struct FakeHttpClient {
    pages: Arc<Mutex<HashMap<String, GetResult>>>,
    get_handler: Arc<Mutex<Option<Arc<GetHandler>>>>,
    files: Arc<Mutex<HashMap<String, Vec<u8>>>>,
    download_handler: Arc<Mutex<Option<Arc<DownloadHandler>>>>,
    gets: Arc<Mutex<Vec<String>>>,
    downloads: Arc<Mutex<Vec<DownloadRequest>>>,
}

impl FakeHttpClient {
    pub fn new() -> Self {
        Self::default()
    }

    /// Serve `body` for GET `url`.
    pub fn page(&self, url: &str, body: impl Into<Vec<u8>>) -> &Self {
        self.pages.lock().insert(url.to_owned(), Ok(body.into()));
        self
    }

    /// Fail GET `url` with `error`.
    pub fn page_error(&self, url: &str, error: GetError) -> &Self {
        self.pages.lock().insert(url.to_owned(), Err(error));
        self
    }

    /// Consulted before the page table; return `None` to fall through.
    pub fn on_get(
        &self,
        handler: impl Fn(&str) -> Option<Result<Vec<u8>, GetError>> + Send + Sync + 'static,
    ) {
        *self.get_handler.lock() = Some(Arc::new(handler));
    }

    /// Serve `body` as the file behind download `url`.
    pub fn file(&self, url: &str, body: impl Into<Vec<u8>>) -> &Self {
        self.files.lock().insert(url.to_owned(), body.into());
        self
    }

    /// Take over downloads entirely (for pause/cancel/failure scenarios).
    pub fn on_download(
        &self,
        handler: impl Fn(DownloadRequest) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>>
        + Send
        + Sync
        + 'static,
    ) {
        *self.download_handler.lock() = Some(Arc::new(handler));
    }

    /// Every GET URL requested so far, in order.
    pub fn requested_gets(&self) -> Vec<String> {
        self.gets.lock().clone()
    }

    /// Every download requested so far, in order.
    pub fn requested_downloads(&self) -> Vec<DownloadRequest> {
        self.downloads.lock().clone()
    }
}

impl HttpClient for FakeHttpClient {
    fn get(&self, url: &str) -> BoxFuture<'static, Result<Vec<u8>, GetError>> {
        self.gets.lock().push(url.to_owned());
        let handler = self.get_handler.lock().clone();
        let result = handler
            .and_then(|h| h(url))
            .or_else(|| self.pages.lock().get(url).cloned())
            .unwrap_or(Err(GetError::Status(404)));
        Box::pin(async move { result })
    }

    fn download(
        &self,
        request: DownloadRequest,
    ) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>> {
        self.downloads.lock().push(request.clone());
        if let Some(handler) = self.download_handler.lock().clone() {
            return handler(request);
        }
        let body = self.files.lock().get(&request.url).cloned();
        Box::pin(async move {
            let Some(body) = body else {
                return Err(DownloadError::Status(404));
            };
            if let Some(dir) = request.dest.parent() {
                let _ = std::fs::create_dir_all(dir);
            }
            std::fs::write(&request.dest, &body)
                .map_err(|e| DownloadError::Write(e.to_string()))?;
            if let Some(progress) = &request.progress {
                progress(body.len() as u64, body.len() as u64);
            }
            Ok(DownloadOutcome { status: 200 })
        })
    }
}
