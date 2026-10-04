//! [`HttpClient`] over reqwest, matching the Qt `NetworkManager` request setup:
//! browser user agent, HTTP/1.1 only, redirects followed except https→http,
//! origin-only `Referer` on downloads, a total timeout on GETs and an idle
//! timeout on downloads.

use std::io::SeekFrom;
use std::path::Path;
use std::time::Duration;

use futures::StreamExt;
use futures::future::BoxFuture;
use reqwest::header::{CONTENT_TYPE, RANGE, REFERER};
use reqwest::redirect::Policy;
use tokio::fs::{self, OpenOptions};
use tokio::io::{AsyncSeekExt, AsyncWriteExt};

use crate::{
    DEFAULT_TIMEOUT, DownloadError, DownloadOutcome, DownloadRequest, GetError, HttpClient,
    USER_AGENT, origin_referer,
};

#[derive(Clone)]
pub struct ReqwestClient {
    client: reqwest::Client,
    timeout: Duration,
}

impl ReqwestClient {
    pub fn new() -> Result<Self, reqwest::Error> {
        Self::with_timeout(DEFAULT_TIMEOUT)
    }

    pub fn with_timeout(timeout: Duration) -> Result<Self, reqwest::Error> {
        // Follow redirects, but never from https to http (Qt's NoLessSafeRedirectPolicy).
        let policy = Policy::custom(|attempt| {
            let downgrade = attempt.url().scheme() == "http"
                && attempt
                    .previous()
                    .last()
                    .is_some_and(|u| u.scheme() == "https");
            if downgrade {
                attempt.error("insecure redirect from https to http")
            } else if attempt.previous().len() >= 20 {
                attempt.error("too many redirects")
            } else {
                attempt.follow()
            }
        });
        let client = reqwest::Client::builder()
            .user_agent(USER_AGENT)
            .http1_only()
            .redirect(policy)
            // Downloads set an origin-only Referer that must survive every hop;
            // reqwest's automatic Referer would replace it with the full URL.
            .referer(false)
            .cookie_store(true)
            .connect_timeout(timeout)
            .build()?;
        Ok(Self { client, timeout })
    }
}

impl HttpClient for ReqwestClient {
    fn get(&self, url: &str) -> BoxFuture<'static, Result<Vec<u8>, GetError>> {
        let request = self.client.get(url);
        let timeout = self.timeout;
        let url = url.to_owned();
        Box::pin(async move {
            let fetch = async {
                let response = request.send().await.map_err(transport)?;
                let status = response.status();
                if !status.is_success() {
                    return Err(GetError::Status(status.as_u16()));
                }
                response
                    .bytes()
                    .await
                    .map(|b| b.to_vec())
                    .map_err(transport)
            };
            let result = tokio::time::timeout(timeout, fetch)
                .await
                .unwrap_or(Err(GetError::TimedOut));
            if let Err(err) = &result {
                tracing::debug!(url, %err, "GET failed");
            }
            result
        })
    }

    fn download(
        &self,
        request: DownloadRequest,
    ) -> BoxFuture<'static, Result<DownloadOutcome, DownloadError>> {
        let client = self.client.clone();
        let timeout = self.timeout;
        Box::pin(async move {
            let result = download(&client, timeout, &request).await;
            match &result {
                Ok(outcome) => tracing::debug!(
                    url = request.url,
                    status = outcome.status,
                    "download finished"
                ),
                Err(err) => tracing::debug!(url = request.url, %err, "download failed"),
            }
            result
        })
    }
}

fn transport(err: reqwest::Error) -> GetError {
    if err.is_timeout() {
        GetError::TimedOut
    } else {
        GetError::Transport(err.to_string())
    }
}

/// Why a transfer stopped early.
enum Interrupt {
    Canceled,
    TimedOut,
    Failed(DownloadError),
}

/// Awaits `fut`, giving up on cancellation or after `idle` without progress.
async fn guarded<T>(
    request: &DownloadRequest,
    idle: Duration,
    fut: impl Future<Output = T>,
) -> Result<T, Interrupt> {
    tokio::select! {
        biased;
        _ = request.cancel.cancelled() => Err(Interrupt::Canceled),
        result = tokio::time::timeout(idle, fut) => result.map_err(|_| Interrupt::TimedOut),
    }
}

async fn remove_quietly(path: &Path) {
    let _ = fs::remove_file(path).await;
}

async fn file_len(path: &Path) -> u64 {
    fs::metadata(path).await.map(|m| m.len()).unwrap_or(0)
}

async fn download(
    client: &reqwest::Client,
    idle: Duration,
    request: &DownloadRequest,
) -> Result<DownloadOutcome, DownloadError> {
    let dest = &request.dest;
    if let Some(dir) = dest.parent() {
        let _ = fs::create_dir_all(dir).await;
    }
    // Plain write + seek rather than append mode: Windows refuses `set_len`
    // on an append-only handle, and a 200 reply to a Range request truncates.
    let mut file = OpenOptions::new()
        .create(true)
        .write(true)
        .truncate(request.resume_from == 0)
        .open(dest)
        .await
        .map_err(|e| DownloadError::CreateFile(e.to_string()))?;
    if request.resume_from > 0 {
        file.seek(SeekFrom::End(0))
            .await
            .map_err(|e| DownloadError::CreateFile(e.to_string()))?;
    }

    let mut builder = client.get(&request.url);
    if let Some(referer) = origin_referer(&request.url) {
        builder = builder.header(REFERER, referer);
    }
    if request.resume_from > 0 {
        builder = builder.header(RANGE, format!("bytes={}-", request.resume_from));
    }

    let outcome = transfer(&mut file, builder, idle, request).await;
    // Close before any removal; Windows cannot delete an open file.
    let flushed = file.flush().await;
    drop(file);

    match outcome {
        Ok(Transfer::Done {
            status,
            written,
            content_type,
        }) => {
            if let Err(err) = flushed {
                if !request.keep_partial {
                    remove_quietly(dest).await;
                }
                return Err(DownloadError::Write(err.to_string()));
            }
            if written == 0 || file_len(dest).await == 0 {
                remove_quietly(dest).await;
                let is_html = content_type.to_ascii_lowercase().contains("text/html");
                return Err(if is_html {
                    DownloadError::HtmlPage
                } else {
                    DownloadError::Empty
                });
            }
            Ok(DownloadOutcome { status })
        }
        Ok(Transfer::AlreadyComplete) => Ok(DownloadOutcome { status: 416 }),
        Err(Interrupt::Canceled) if request.keep_partial => Err(DownloadError::Canceled),
        Err(Interrupt::TimedOut) if request.keep_partial => Err(DownloadError::TimedOut),
        Err(Interrupt::Failed(DownloadError::Write(msg))) => {
            if !request.keep_partial {
                remove_quietly(dest).await;
            }
            Err(DownloadError::Write(msg))
        }
        Err(interrupt) => {
            remove_quietly(dest).await;
            Err(match interrupt {
                Interrupt::Failed(err) => err,
                Interrupt::Canceled => DownloadError::Canceled,
                Interrupt::TimedOut => DownloadError::TimedOut,
            })
        }
    }
}

enum Transfer {
    Done {
        status: u16,
        written: u64,
        content_type: String,
    },
    /// 416 on a resume of a file that already has content.
    AlreadyComplete,
}

async fn transfer(
    file: &mut fs::File,
    builder: reqwest::RequestBuilder,
    idle: Duration,
    request: &DownloadRequest,
) -> Result<Transfer, Interrupt> {
    let response = guarded(request, idle, builder.send())
        .await?
        .map_err(|e| Interrupt::Failed(DownloadError::Transport(e.to_string())))?;
    let status = response.status().as_u16();

    if !response.status().is_success() {
        if status == 416 && file_len(&request.dest).await > 0 {
            return Ok(Transfer::AlreadyComplete);
        }
        return Err(Interrupt::Failed(DownloadError::Status(status)));
    }

    // A 200 to a Range request means the server ignored it: start over.
    let mut offset = request.resume_from;
    if request.resume_from > 0 && status == 200 {
        let restart = async {
            file.set_len(0).await?;
            file.seek(SeekFrom::Start(0)).await.map(|_| ())
        };
        restart
            .await
            .map_err(|e| Interrupt::Failed(DownloadError::Write(e.to_string())))?;
        offset = 0;
    }

    let content_type = response
        .headers()
        .get(CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or_default()
        .to_owned();
    let total = response
        .content_length()
        .filter(|&n| n > 0)
        .map_or(0, |n| offset + n);
    let report = |received: u64| {
        if let Some(progress) = &request.progress {
            progress(offset + received, total);
        }
    };

    let mut stream = response.bytes_stream();
    let mut written = 0u64;
    report(0);
    while let Some(chunk) = guarded(request, idle, stream.next()).await? {
        let chunk =
            chunk.map_err(|e| Interrupt::Failed(DownloadError::Transport(e.to_string())))?;
        file.write_all(&chunk)
            .await
            .map_err(|e| Interrupt::Failed(DownloadError::Write(e.to_string())))?;
        written += chunk.len() as u64;
        report(written);
    }
    Ok(Transfer::Done {
        status,
        written,
        content_type,
    })
}

#[cfg(test)]
mod tests {
    //! Exercises the real client against a tiny local HTTP/1.1 server.

    use std::sync::Arc;
    use std::sync::atomic::{AtomicU64, Ordering};

    use tokio::io::{AsyncReadExt, AsyncWriteExt};
    use tokio::net::TcpListener;
    use tokio_util::sync::CancellationToken;

    use super::*;

    /// Serves one canned response per connection; returns the base URL and
    /// the raw requests it received.
    async fn serve(responses: Vec<Vec<u8>>) -> (String, Arc<parking_lot::Mutex<Vec<String>>>) {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let seen = Arc::new(parking_lot::Mutex::new(Vec::new()));
        let log = seen.clone();
        tokio::spawn(async move {
            for response in responses {
                let (mut socket, _) = listener.accept().await.unwrap();
                let mut buf = vec![0u8; 8192];
                let n = socket.read(&mut buf).await.unwrap();
                log.lock()
                    .push(String::from_utf8_lossy(&buf[..n]).into_owned());
                socket.write_all(&response).await.unwrap();
                let _ = socket.shutdown().await;
            }
        });
        (format!("http://{addr}"), seen)
    }

    fn http(status: &str, headers: &str, body: &[u8]) -> Vec<u8> {
        let mut out = format!(
            "HTTP/1.1 {status}\r\nContent-Length: {}\r\nConnection: close\r\n{headers}\r\n",
            body.len()
        )
        .into_bytes();
        out.extend_from_slice(body);
        out
    }

    fn request(url: String, dest: std::path::PathBuf, resume_from: u64) -> DownloadRequest {
        DownloadRequest {
            url,
            dest,
            resume_from,
            keep_partial: true,
            cancel: CancellationToken::new(),
            progress: None,
        }
    }

    #[tokio::test]
    async fn get_returns_body_and_rejects_errors() {
        let (base, seen) = serve(vec![
            http("200 OK", "", b"hello"),
            http("404 Not Found", "", b"nope"),
        ])
        .await;
        let client = ReqwestClient::new().unwrap();
        assert_eq!(client.get(&format!("{base}/a")).await.unwrap(), b"hello");
        assert_eq!(
            client.get(&format!("{base}/b")).await,
            Err(GetError::Status(404))
        );
        assert!(seen.lock()[0].contains("Chrome/120.0.0.0"));
    }

    #[tokio::test]
    async fn download_sends_origin_referer_and_reports_progress() {
        let (base, seen) = serve(vec![http(
            "200 OK",
            "Content-Type: application/zip\r\n",
            b"PK\x03\x04data",
        )])
        .await;
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("sub/file.zip");
        let last = Arc::new(AtomicU64::new(0));
        let mut req = request(format!("{base}/downloads/token,,"), dest.clone(), 0);
        let progress_seen = last.clone();
        req.progress = Some(Arc::new(move |received, total| {
            assert_eq!(total, 8);
            progress_seen.store(received, Ordering::SeqCst);
        }));
        let outcome = ReqwestClient::new().unwrap().download(req).await.unwrap();
        assert_eq!(outcome.status, 200);
        assert_eq!(std::fs::read(&dest).unwrap(), b"PK\x03\x04data");
        assert_eq!(last.load(Ordering::SeqCst), 8);
        let raw = seen.lock()[0].to_lowercase();
        assert!(
            raw.contains(&format!("referer: {}/\r\n", base.to_lowercase())),
            "{raw}"
        );
    }

    #[tokio::test]
    async fn resume_appends_on_206_and_restarts_on_200() {
        let (base, seen) = serve(vec![
            http("206 Partial Content", "", b"world"),
            http("200 OK", "", b"full body"),
        ])
        .await;
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("f.bin");
        std::fs::write(&dest, b"hello ").unwrap();
        let client = ReqwestClient::new().unwrap();

        client
            .download(request(format!("{base}/a"), dest.clone(), 6))
            .await
            .unwrap();
        assert_eq!(std::fs::read(&dest).unwrap(), b"hello world");
        assert!(seen.lock()[0].to_lowercase().contains("range: bytes=6-"));

        client
            .download(request(format!("{base}/a"), dest.clone(), 11))
            .await
            .unwrap();
        assert_eq!(std::fs::read(&dest).unwrap(), b"full body");
    }

    #[tokio::test]
    async fn status_416_on_complete_file_is_success() {
        let (base, _) = serve(vec![http("416 Range Not Satisfiable", "", b"")]).await;
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("f.bin");
        std::fs::write(&dest, b"done").unwrap();
        let outcome = ReqwestClient::new()
            .unwrap()
            .download(request(format!("{base}/a"), dest.clone(), 4))
            .await;
        assert_eq!(outcome, Ok(DownloadOutcome { status: 416 }));
        assert_eq!(std::fs::read(&dest).unwrap(), b"done");
    }

    #[tokio::test]
    async fn empty_and_error_responses_remove_the_file() {
        let (base, _) = serve(vec![
            http("200 OK", "Content-Type: text/html; charset=UTF-8\r\n", b""),
            http("200 OK", "", b""),
            http("403 Forbidden", "", b"denied"),
        ])
        .await;
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("f.bin");
        let client = ReqwestClient::new().unwrap();
        for expected in [
            DownloadError::HtmlPage,
            DownloadError::Empty,
            DownloadError::Status(403),
        ] {
            let result = client
                .download(request(format!("{base}/a"), dest.clone(), 0))
                .await;
            assert_eq!(result, Err(expected));
            assert!(!dest.exists());
        }
    }

    #[tokio::test]
    async fn cancel_keeps_partial_file_when_requested() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        tokio::spawn(async move {
            let (mut socket, _) = listener.accept().await.unwrap();
            let mut buf = [0u8; 4096];
            let _ = socket.read(&mut buf).await;
            socket
                .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\npartial")
                .await
                .unwrap();
            tokio::time::sleep(Duration::from_secs(10)).await;
        });
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("f.bin");
        let req = request(format!("http://{addr}/a"), dest.clone(), 0);
        let cancel = req.cancel.clone();
        let progress_cancel = cancel.clone();
        let mut req = req;
        req.progress = Some(Arc::new(move |received, _| {
            if received > 0 {
                progress_cancel.cancel();
            }
        }));
        let result = ReqwestClient::new().unwrap().download(req).await;
        assert_eq!(result, Err(DownloadError::Canceled));
        assert_eq!(std::fs::read(&dest).unwrap(), b"partial");
    }

    #[tokio::test]
    async fn idle_timeout_aborts_stalled_download() {
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        tokio::spawn(async move {
            let (mut socket, _) = listener.accept().await.unwrap();
            let mut buf = [0u8; 4096];
            let _ = socket.read(&mut buf).await;
            socket
                .write_all(b"HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\nab")
                .await
                .unwrap();
            tokio::time::sleep(Duration::from_secs(10)).await;
        });
        let dir = tempfile::tempdir().unwrap();
        let dest = dir.path().join("f.bin");
        let client = ReqwestClient::with_timeout(Duration::from_millis(300)).unwrap();
        let result = client
            .download(request(format!("http://{addr}/a"), dest.clone(), 0))
            .await;
        assert_eq!(result, Err(DownloadError::TimedOut));
        assert_eq!(std::fs::read(&dest).unwrap(), b"ab");
    }
}
