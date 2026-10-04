//! The session download queue. Port of the task queue in `Backend.cpp`:
//!
//! - tasks go `queued → downloading → completed | failed`, and can be paused,
//!   resumed (from paused or failed), canceled and removed;
//! - at most [`MAX_CONCURRENT_DOWNLOADS`] run at once, and two tasks that
//!   would write the same temp file never run together;
//! - data goes to `<save path>.crdownload`, which a pause keeps for resuming;
//! - on success the temp file is renamed and its extension corrected.
//!
//! Progress is coalesced: status changes are reported at once, byte counts at
//! most every 200 ms, and speeds are sampled once a second.

use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use std::time::{Duration, Instant};

use chrono::Local;
use fling_core::text::sanitize_path_component;
use fling_core::{
    DownloadTask, DownloadVersion, DownloadedModifier, ModifierInfo, TaskId, TaskStatus,
};
use fling_net::{CancellationToken, DownloadError, DownloadOutcome, DownloadRequest, HttpClient};
use parking_lot::Mutex;
use tokio::runtime::Handle;

use crate::files::{clean_url, correct_file_extension};

pub const MAX_CONCURRENT_DOWNLOADS: usize = 3;
const FLUSH_INTERVAL: Duration = Duration::from_millis(200);
const SPEED_EVERY_N_FLUSHES: u32 = 5;

#[derive(Debug, Clone, PartialEq)]
pub enum QueueEvent {
    /// The whole task list, after any change.
    Tasks(Vec<DownloadTask>),
    /// A download finished; the library should record it.
    Completed(DownloadedModifier),
}

pub type EventSink = Arc<dyn Fn(QueueEvent) + Send + Sync>;

struct Entry {
    task: DownloadTask,
    name: String,
    game_version: String,
    page_url: String,
    versions: Vec<DownloadVersion>,
    temp_path: PathBuf,
    resume_requested: bool,
    cancel: Option<CancellationToken>,
    sample_bytes: u64,
    sample_at: Instant,
}

#[derive(Default)]
struct State {
    entries: Vec<Entry>,
    next_id: u64,
    active: HashSet<TaskId>,
    dirty: bool,
    ticker_running: bool,
}

impl State {
    fn entry_mut(&mut self, id: &TaskId) -> Option<&mut Entry> {
        self.entries.iter_mut().find(|e| &e.task.id == id)
    }

    fn snapshot(&mut self) -> Vec<DownloadTask> {
        self.dirty = false;
        self.entries.iter().map(|e| e.task.clone()).collect()
    }

    fn temp_path_in_use(&self, id: &TaskId) -> bool {
        let Some(temp) = self
            .entries
            .iter()
            .find(|e| &e.task.id == id)
            .map(|e| &e.temp_path)
        else {
            return false;
        };
        self.entries
            .iter()
            .any(|e| &e.task.id != id && self.active.contains(&e.task.id) && &e.temp_path == temp)
    }
}

struct Job {
    id: TaskId,
    request: DownloadRequest,
}

struct Shared {
    http: Arc<dyn HttpClient>,
    runtime: Handle,
    sink: EventSink,
    state: Mutex<State>,
}

/// Cheap to clone; all clones share one queue.
#[derive(Clone)]
pub struct DownloadQueue {
    shared: Arc<Shared>,
}

fn temp_path_for(save_path: &Path) -> PathBuf {
    let mut os = save_path.as_os_str().to_owned();
    os.push(".crdownload");
    PathBuf::from(os)
}

impl DownloadQueue {
    /// Transfers run on `runtime`; `sink` receives every [`QueueEvent`].
    pub fn new(http: Arc<dyn HttpClient>, runtime: Handle, sink: EventSink) -> Self {
        Self {
            shared: Arc::new(Shared {
                http,
                runtime,
                sink,
                state: Mutex::new(State::default()),
            }),
        }
    }

    pub fn tasks(&self) -> Vec<DownloadTask> {
        self.shared
            .state
            .lock()
            .entries
            .iter()
            .map(|e| e.task.clone())
            .collect()
    }

    fn emit(&self, event: QueueEvent) {
        (self.shared.sink)(event);
    }

    fn emit_snapshot(&self) {
        let snapshot = self.shared.state.lock().snapshot();
        self.emit(QueueEvent::Tasks(snapshot));
    }

    /// Queues `modifier`'s version `version_index` (out of range means the
    /// first) for `<download_dir>/<name>_<version>.zip`. `None` when the
    /// modifier has no versions.
    pub fn enqueue(
        &self,
        modifier: &ModifierInfo,
        version_index: usize,
        download_dir: &Path,
    ) -> Option<TaskId> {
        let version = modifier
            .versions
            .get(version_index)
            .or_else(|| modifier.versions.first())?;
        // Both halves are remote-derived; an unescaped separator in either
        // would place the file outside the download directory.
        let file_name = format!(
            "{}_{}.zip",
            sanitize_path_component(&modifier.name, "trainer"),
            sanitize_path_component(&version.label, "version"),
        );
        let save_path = download_dir.join(file_name);

        let id = {
            let mut state = self.shared.state.lock();
            state.next_id += 1;
            let id = TaskId(format!("task_{}", state.next_id));
            state.entries.push(Entry {
                task: DownloadTask {
                    id: id.clone(),
                    file_name: modifier.name.clone(),
                    status: TaskStatus::Queued,
                    bytes_received: 0,
                    bytes_total: 0,
                    speed: 0,
                    version: version.label.clone(),
                    save_path: save_path.to_string_lossy().into_owned(),
                    error_message: String::new(),
                    created_at: Local::now().naive_local(),
                },
                name: modifier.name.clone(),
                game_version: modifier.game_version.clone(),
                page_url: modifier.url.clone(),
                versions: modifier.versions.clone(),
                temp_path: temp_path_for(&save_path),
                resume_requested: false,
                cancel: None,
                sample_bytes: 0,
                sample_at: Instant::now(),
            });
            id
        };
        self.emit_snapshot();
        self.process_next();
        Some(id)
    }

    /// Queued tasks go straight to paused; a running one is stopped and keeps
    /// its temp file for resuming.
    pub fn pause(&self, id: &TaskId) {
        {
            let mut state = self.shared.state.lock();
            let Some(entry) = state.entry_mut(id) else {
                return;
            };
            match entry.task.status {
                TaskStatus::Queued => {}
                TaskStatus::Downloading => {
                    if let Some(cancel) = &entry.cancel {
                        cancel.cancel();
                    }
                }
                _ => return,
            }
            entry.task.status = TaskStatus::Paused;
            entry.resume_requested = true;
        }
        self.emit_snapshot();
    }

    /// Re-queues a paused or failed task, resuming from its temp file when
    /// one exists.
    pub fn resume(&self, id: &TaskId) {
        {
            let mut state = self.shared.state.lock();
            let Some(entry) = state.entry_mut(id) else {
                return;
            };
            if !matches!(entry.task.status, TaskStatus::Paused | TaskStatus::Failed) {
                return;
            }
            entry.resume_requested =
                entry.task.status == TaskStatus::Paused || entry.temp_path.exists();
            entry.task.status = TaskStatus::Queued;
            entry.task.error_message.clear();
        }
        self.emit_snapshot();
        self.process_next();
    }

    /// Stops a task for good and discards its temp file.
    pub fn cancel(&self, id: &TaskId) {
        let was_active = {
            let mut state = self.shared.state.lock();
            let was_active = state.active.contains(id);
            let shares_temp = state.temp_path_in_use(id);
            let Some(entry) = state.entry_mut(id) else {
                return;
            };
            if entry.task.status.is_terminal() {
                return;
            }
            entry.task.status = TaskStatus::Canceled;
            if was_active {
                if let Some(cancel) = &entry.cancel {
                    cancel.cancel();
                }
            } else if !shares_temp {
                // A duplicate of a running task shares its temp file; leave that alone.
                let _ = std::fs::remove_file(&entry.temp_path);
            }
            was_active
        };
        self.emit_snapshot();
        if !was_active {
            self.process_next();
        }
    }

    /// Drops a task from the list; refused while it is downloading.
    pub fn remove(&self, id: &TaskId) {
        {
            let mut state = self.shared.state.lock();
            let Some(index) = state.entries.iter().position(|e| &e.task.id == id) else {
                return;
            };
            if state.entries[index].task.status == TaskStatus::Downloading {
                return;
            }
            state.entries.remove(index);
        }
        self.emit_snapshot();
    }

    /// Starts queued tasks, in list order, while slots are free.
    fn process_next(&self) {
        let mut jobs = Vec::new();
        let mut changed = false;
        {
            let mut state = self.shared.state.lock();
            let queued: Vec<TaskId> = state
                .entries
                .iter()
                .filter(|e| e.task.status == TaskStatus::Queued)
                .map(|e| e.task.id.clone())
                .collect();
            for id in queued {
                if state.active.len() >= MAX_CONCURRENT_DOWNLOADS {
                    break;
                }
                // The same trainer version queued twice writes one temp file;
                // the second copy waits until the first is done with it.
                if state.temp_path_in_use(&id) {
                    continue;
                }
                changed = true;
                if let Some(job) = Self::prepare_start(&mut state, &id) {
                    state.active.insert(id);
                    jobs.push(job);
                }
            }
            if !jobs.is_empty() && !state.ticker_running {
                state.ticker_running = true;
                self.spawn_ticker();
            }
        }
        if changed {
            self.emit_snapshot();
        }
        for job in jobs {
            self.spawn_job(job);
        }
    }

    /// Moves a queued task to downloading, or straight to failed when it has
    /// no usable URL.
    fn prepare_start(state: &mut State, id: &TaskId) -> Option<Job> {
        let entry = state.entry_mut(id)?;
        let resume_from = match std::fs::metadata(&entry.temp_path) {
            Ok(meta) if entry.resume_requested => meta.len(),
            _ => {
                let _ = std::fs::remove_file(&entry.temp_path);
                let _ = std::fs::remove_file(&entry.task.save_path);
                0
            }
        };

        let label = &entry.task.version;
        let raw_url = entry
            .versions
            .iter()
            .find(|v| &v.label == label)
            .or_else(|| entry.versions.first())
            .map(|v| v.url.clone())
            .filter(|u| !u.is_empty());
        let url = match raw_url.as_deref().map(clean_url) {
            None => Err("Download URL not found"),
            Some(None) => Err("Download URL is empty"),
            Some(Some(url)) => Ok(url),
        };
        let url = match url {
            Ok(url) => url,
            Err(message) => {
                entry.task.status = TaskStatus::Failed;
                entry.task.error_message = message.to_owned();
                return None;
            }
        };

        let cancel = CancellationToken::new();
        entry.cancel = Some(cancel.clone());
        entry.resume_requested = false;
        entry.task.status = TaskStatus::Downloading;
        entry.task.bytes_received = resume_from;
        entry.task.error_message.clear();
        entry.task.speed = 0;
        entry.sample_bytes = resume_from;
        entry.sample_at = Instant::now();

        Some(Job {
            id: id.clone(),
            request: DownloadRequest {
                url,
                dest: entry.temp_path.clone(),
                resume_from,
                keep_partial: true,
                cancel,
                progress: None,
            },
        })
    }

    fn spawn_job(&self, job: Job) {
        let queue = self.clone();
        let Job { id, mut request } = job;
        let progress_queue = self.clone();
        let progress_id = id.clone();
        request.progress = Some(Arc::new(move |received, total| {
            let mut state = progress_queue.shared.state.lock();
            if let Some(entry) = state.entry_mut(&progress_id) {
                entry.task.bytes_received = received;
                if total > 0 {
                    entry.task.bytes_total = total;
                }
                state.dirty = true;
            }
        }));
        let download = self.shared.http.download(request);
        self.shared.runtime.spawn(async move {
            let result = download.await;
            queue.finish(&id, result);
        });
    }

    fn finish(&self, id: &TaskId, result: Result<DownloadOutcome, DownloadError>) {
        let completed = {
            let mut state = self.shared.state.lock();
            state.active.remove(id);
            let Some(entry) = state.entry_mut(id) else {
                drop(state);
                self.process_next();
                return;
            };
            entry.cancel = None;
            entry.task.speed = 0;
            match entry.task.status {
                TaskStatus::Paused => None,
                TaskStatus::Canceled => {
                    let _ = std::fs::remove_file(&entry.temp_path);
                    None
                }
                _ => match result {
                    Ok(_) => Some(Self::complete(entry)),
                    Err(err) => {
                        entry.task.status = TaskStatus::Failed;
                        entry.task.error_message = err.to_string();
                        None
                    }
                },
            }
        };
        self.emit_snapshot();
        if let Some(record) = completed {
            self.emit(QueueEvent::Completed(record));
        }
        self.process_next();
    }

    /// Renames the temp file into place, fixes its extension and builds the
    /// library record.
    fn complete(entry: &mut Entry) -> DownloadedModifier {
        let save_path = PathBuf::from(&entry.task.save_path);
        if entry.temp_path.exists() {
            let _ = std::fs::remove_file(&save_path);
            if let Err(err) = std::fs::rename(&entry.temp_path, &save_path) {
                tracing::warn!(?save_path, %err, "failed to move finished download into place");
            }
        }
        let final_path = correct_file_extension(&save_path);
        let size = std::fs::metadata(&final_path).map(|m| m.len()).unwrap_or(0);

        let task = &mut entry.task;
        task.status = TaskStatus::Completed;
        task.bytes_received = size;
        if task.bytes_total == 0 {
            task.bytes_total = size;
        }
        task.save_path = final_path.to_string_lossy().into_owned();

        DownloadedModifier {
            name: entry.name.clone(),
            version: task.version.clone(),
            game_version: entry.game_version.clone(),
            download_date: Some(Local::now().naive_local()),
            file_path: task.save_path.clone(),
            url: entry.page_url.clone(),
        }
    }

    /// Flushes coalesced progress every 200 ms and samples speeds every
    /// second, until no task is downloading.
    fn spawn_ticker(&self) {
        let queue = self.clone();
        self.shared.runtime.spawn(async move {
            let mut tick: u32 = 0;
            loop {
                tokio::time::sleep(FLUSH_INTERVAL).await;
                tick = tick.wrapping_add(1);
                let snapshot = {
                    let mut state = queue.shared.state.lock();
                    if tick.is_multiple_of(SPEED_EVERY_N_FLUSHES) {
                        sample_speeds(&mut state);
                    }
                    if state.active.is_empty() {
                        state.ticker_running = false;
                        let flush = state.dirty.then(|| state.snapshot());
                        drop(state);
                        if let Some(snapshot) = flush {
                            queue.emit(QueueEvent::Tasks(snapshot));
                        }
                        return;
                    }
                    state.dirty.then(|| state.snapshot())
                };
                if let Some(snapshot) = snapshot {
                    queue.emit(QueueEvent::Tasks(snapshot));
                }
            }
        });
    }
}

fn sample_speeds(state: &mut State) {
    let now = Instant::now();
    let active = state.active.clone();
    for entry in state
        .entries
        .iter_mut()
        .filter(|e| active.contains(&e.task.id))
    {
        let elapsed = now.duration_since(entry.sample_at).as_millis() as u64;
        let delta = entry.task.bytes_received.saturating_sub(entry.sample_bytes);
        if let Some(speed) = (delta * 1000).checked_div(elapsed) {
            entry.task.speed = speed;
        }
        entry.sample_bytes = entry.task.bytes_received;
        entry.sample_at = now;
    }
    state.dirty = true;
}

#[cfg(test)]
mod tests {
    use std::time::Duration;

    use fling_net::fake::FakeHttpClient;
    use futures::FutureExt;

    use super::*;

    struct Harness {
        _dir: tempfile::TempDir,
        dir: PathBuf,
        fake: FakeHttpClient,
        queue: DownloadQueue,
        events: Arc<Mutex<Vec<QueueEvent>>>,
    }

    fn harness() -> Harness {
        let tmp = tempfile::tempdir().unwrap();
        let dir = tmp.path().to_path_buf();
        let fake = FakeHttpClient::new();
        let events = Arc::new(Mutex::new(Vec::new()));
        let sink_events = events.clone();
        let queue = DownloadQueue::new(
            Arc::new(fake.clone()),
            Handle::current(),
            Arc::new(move |event| sink_events.lock().push(event)),
        );
        Harness {
            _dir: tmp,
            dir,
            fake,
            queue,
            events,
        }
    }

    fn modifier(name: &str, urls: &[(&str, &str)]) -> ModifierInfo {
        ModifierInfo {
            name: name.into(),
            game_version: "v1.0+".into(),
            url: format!("https://fling.test/{name}/"),
            versions: urls
                .iter()
                .map(|(label, url)| DownloadVersion {
                    label: (*label).into(),
                    url: (*url).into(),
                })
                .collect(),
            ..ModifierInfo::default()
        }
    }

    fn status_of(queue: &DownloadQueue, id: &TaskId) -> TaskStatus {
        queue
            .tasks()
            .into_iter()
            .find(|t| &t.id == id)
            .unwrap()
            .status
    }

    async fn wait_for(mut condition: impl FnMut() -> bool) {
        for _ in 0..200 {
            if condition() {
                return;
            }
            tokio::time::sleep(Duration::from_millis(10)).await;
        }
        panic!("condition not reached");
    }

    /// Downloads that write `partial` then wait until canceled.
    fn hanging_downloads(fake: &FakeHttpClient, partial: &'static [u8]) {
        fake.on_download(move |request| {
            async move {
                let existing = std::fs::read(&request.dest).unwrap_or_default();
                let mut data = if request.resume_from > 0 {
                    existing
                } else {
                    Vec::new()
                };
                data.extend_from_slice(partial);
                std::fs::write(&request.dest, &data).unwrap();
                request.cancel.cancelled().await;
                Err(DownloadError::Canceled)
            }
            .boxed()
        });
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn completes_renames_and_reports() {
        let h = harness();
        h.fake.file(
            "https://fling.test/downloads/a",
            b"MZ\x90\x00payload".to_vec(),
        );
        let m = modifier(
            "Elden Ring",
            &[("v1.02", "https://fling.test/downloads/a,,")],
        );
        let id = h.queue.enqueue(&m, 0, &h.dir).unwrap();
        assert_eq!(id.0, "task_1");

        wait_for(|| status_of(&h.queue, &id) == TaskStatus::Completed).await;
        let task = h.queue.tasks().remove(0);
        let expected = h.dir.join("Elden Ring_v1.02.exe");
        assert_eq!(PathBuf::from(&task.save_path), expected);
        assert!(expected.exists());
        assert!(!h.dir.join("Elden Ring_v1.02.zip.crdownload").exists());
        assert_eq!(task.bytes_received, 11);

        // The trailing commas were cleaned off the URL.
        assert_eq!(
            h.fake.requested_downloads()[0].url,
            "https://fling.test/downloads/a"
        );
        let completed: Vec<_> = h
            .events
            .lock()
            .iter()
            .filter_map(|e| match e {
                QueueEvent::Completed(record) => Some(record.clone()),
                _ => None,
            })
            .collect();
        assert_eq!(completed.len(), 1);
        assert_eq!(completed[0].name, "Elden Ring");
        assert_eq!(completed[0].version, "v1.02");
        assert_eq!(completed[0].game_version, "v1.0+");
        assert_eq!(completed[0].url, "https://fling.test/Elden Ring/");
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn runs_at_most_three_and_serializes_shared_temp_files() {
        let h = harness();
        hanging_downloads(&h.fake, b"");
        let ids: Vec<_> = ["A", "B", "C", "D"]
            .iter()
            .map(|name| {
                h.queue
                    .enqueue(&modifier(name, &[("v1", "https://x/1")]), 0, &h.dir)
                    .unwrap()
            })
            .collect();
        wait_for(|| h.fake.requested_downloads().len() == 3).await;
        let statuses: Vec<_> = ids.iter().map(|id| status_of(&h.queue, id)).collect();
        assert_eq!(
            statuses,
            [
                TaskStatus::Downloading,
                TaskStatus::Downloading,
                TaskStatus::Downloading,
                TaskStatus::Queued
            ]
        );

        // Cancel one running task: the queued one takes the slot.
        h.queue.cancel(&ids[0]);
        wait_for(|| status_of(&h.queue, &ids[3]) == TaskStatus::Downloading).await;
        assert_eq!(status_of(&h.queue, &ids[0]), TaskStatus::Canceled);

        // A duplicate of a running task waits for the shared temp file.
        let dup = h
            .queue
            .enqueue(&modifier("B", &[("v1", "https://x/1")]), 0, &h.dir)
            .unwrap();
        h.queue.cancel(&ids[2]);
        wait_for(|| {
            h.queue
                .tasks()
                .iter()
                .filter(|t| t.status == TaskStatus::Downloading)
                .count()
                == 2
        })
        .await;
        tokio::time::sleep(Duration::from_millis(50)).await;
        assert_eq!(
            status_of(&h.queue, &dup),
            TaskStatus::Queued,
            "a slot is free but B holds the temp file"
        );

        h.queue.cancel(&ids[1]);
        wait_for(|| status_of(&h.queue, &dup) == TaskStatus::Downloading).await;
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn pause_keeps_partial_and_resume_continues_from_it() {
        let h = harness();
        hanging_downloads(&h.fake, b"half");
        let id = h
            .queue
            .enqueue(&modifier("A", &[("v1", "https://x/1")]), 0, &h.dir)
            .unwrap();
        let temp = h.dir.join("A_v1.zip.crdownload");
        wait_for(|| temp.exists()).await;

        h.queue.pause(&id);
        assert_eq!(status_of(&h.queue, &id), TaskStatus::Paused);
        tokio::time::sleep(Duration::from_millis(50)).await;
        assert_eq!(
            status_of(&h.queue, &id),
            TaskStatus::Paused,
            "finishing a paused transfer keeps it paused"
        );
        assert_eq!(std::fs::read(&temp).unwrap(), b"half");

        h.queue.resume(&id);
        wait_for(|| h.fake.requested_downloads().len() == 2).await;
        assert_eq!(h.fake.requested_downloads()[1].resume_from, 4);
        wait_for(|| std::fs::read(&temp).unwrap() == b"halfhalf").await;
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn queued_pause_cancel_and_remove_rules() {
        let h = harness();
        hanging_downloads(&h.fake, b"");
        let ids: Vec<_> = (0..4)
            .map(|i| {
                h.queue
                    .enqueue(
                        &modifier(&format!("G{i}"), &[("v1", "https://x/1")]),
                        0,
                        &h.dir,
                    )
                    .unwrap()
            })
            .collect();
        let queued = &ids[3];
        h.queue.pause(queued);
        assert_eq!(status_of(&h.queue, queued), TaskStatus::Paused);

        h.queue.remove(&ids[0]);
        assert_eq!(
            h.queue.tasks().len(),
            4,
            "a downloading task cannot be removed"
        );
        h.queue.cancel(queued);
        assert_eq!(status_of(&h.queue, queued), TaskStatus::Canceled);
        h.queue.cancel(queued);
        h.queue.remove(queued);
        assert_eq!(h.queue.tasks().len(), 3);
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn failures_report_and_can_be_retried() {
        let h = harness();
        let id = h
            .queue
            .enqueue(&modifier("A", &[("v1", "https://x/missing")]), 0, &h.dir)
            .unwrap();
        wait_for(|| status_of(&h.queue, &id) == TaskStatus::Failed).await;
        assert_eq!(h.queue.tasks()[0].error_message, "Server replied: 404");

        h.fake.file("https://x/missing", b"PK\x03\x04".to_vec());
        h.queue.resume(&id);
        wait_for(|| status_of(&h.queue, &id) == TaskStatus::Completed).await;
        assert!(h.dir.join("A_v1.zip").exists());
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn invalid_urls_fail_without_a_request() {
        let h = harness();
        let id = h
            .queue
            .enqueue(&modifier("A", &[("v1", "/relative")]), 0, &h.dir)
            .unwrap();
        assert_eq!(status_of(&h.queue, &id), TaskStatus::Failed);
        assert_eq!(h.queue.tasks()[0].error_message, "Download URL is empty");
        assert!(h.fake.requested_downloads().is_empty());
        assert!(h.queue.enqueue(&modifier("B", &[]), 0, &h.dir).is_none());
    }

    #[tokio::test(flavor = "multi_thread")]
    async fn names_are_sanitized_and_index_falls_back_to_first() {
        let h = harness();
        hanging_downloads(&h.fake, b"");
        let m = modifier(
            "Bad/Name:",
            &[("v1|beta", "https://x/1"), ("v2", "https://x/2")],
        );
        h.queue.enqueue(&m, 9, &h.dir).unwrap();
        let task = h.queue.tasks().remove(0);
        assert_eq!(
            PathBuf::from(task.save_path),
            h.dir.join("Bad_Name__v1_beta.zip")
        );
        assert_eq!(task.version, "v1|beta");
    }
}
