# Work log: Download integrity fixes and search-term encoding

- **Date:** 2026-10-19
- **Branch:** `fix/download-integrity` (from `main` at `f5dd82b`)
- **Related:** [PR record](../pull-requests/2026-10-19-download-integrity.md). There is no code-review archive. The findings came from the user's own analysis, not from a review in this repository.

## 1. Request
> I have analyzed the software for several defects. Could you please fix these issues and submit a pull request (PR)?
>
> | Priority | Defect | Verified Consequence |
> |---|---|---|
> | High | Deleting existing files before re-downloading | New request returns a 404, while the original file is also lost. |
> | High | Marking as complete even when final file renaming fails | Download is reported as successful and registered in the library, but the file remains as a `.crdownload` file. |
> | Medium | Accepting non-empty HTML error pages | A `200 OK` response with `text/html` content is treated as a successful download, resulting in the HTML page being saved instead of the actual file. |
> | Medium | Treating HTTP 416 as a completed download | If the server-side file is only 3 bytes while the local residual file is 5 bytes, the process incorrectly reports success. |
> | Medium | Race condition during rapid pause/resume operations | Two workers are launched for the same task; the cancellation result from the old task causes the new task to be marked as failed. |
>
> Additionally, I discovered the following:
>
> - Incorrect encoding of search parameters: Game titles containing characters like `&` or `+` alter the query (e.g., "Mount & Blade II").

Fix all six defects, each with a regression test, and open a PR.

## 2. Plan
1. Confirm each defect in the source:
   - Defects 1, 2 and 5 are in `fling-download/src/queue.rs`.
   - Defects 3 and 4 are in `fling-net/src/reqwest_client.rs`.
   - The search encoding is in `fling-site/src/site.rs`.
2. Fix them in that order. Add a regression test for each that fails without the fix.
3. Undo the two subtlest fixes (the race and the rename check) and confirm their tests fail.
4. Run `cargo test --workspace`, clippy and `cargo fmt --all --check` on the Windows toolchain.
5. Run one live search with the headless example.
6. Write this log and the PR record, then commit, push and open the PR.

### Root causes and fixes
1. **Re-download deletes the existing file.**
   - **Cause:** `prepare_start` removed `save_path` whenever a task started without resuming. `complete` removed it again before the rename. A failed request (404) therefore left the user with neither file.
   - **Fix:** neither function deletes the file any more. `complete` renames the temp file over the old copy, so the old file is replaced only by a finished download. `std::fs::rename` replaces an existing file on Windows and Unix.
2. **Completed despite a failed rename.**
   - **Cause:** `complete` logged the rename error and went on. It reported `Completed` and emitted `QueueEvent::Completed`, so the library recorded a file that was still `.crdownload`.
   - **Fix:** `complete` now returns `Result`. A failed rename, or a missing temp file, fails the task with "Cannot move the downloaded file into place: …" / "Downloaded file is missing". The task is not added to the library. The temp file is kept, so a retry can resume from it.
3. **Non-empty HTML pages accepted.**
   - **Cause:** the client checked `Content-Type: text/html` only when the body was empty.
   - **Fix:** `transfer` now rejects any successful reply whose content type contains `text/html` (case-insensitive) with `DownloadError::HtmlPage`. It does this before writing anything, and the partial file is removed. A trainer is never served as a web page.
4. **HTTP 416 treated as complete.**
   - **Cause:** any 416 on a resume of a non-empty file counted as "already complete".
   - **Fix:** a 416 counts as complete only when `Content-Range: bytes */N` gives `N` equal to the resume offset. Otherwise the partial file is not a prefix of the server's file. This covers a longer file, or a 416 that gives no size. In that case `download` truncates the file and requests it again without `Range`, the way a 200 reply to a Range request already restarted.
5. **Rapid pause/resume race.**
   - **Cause:** `pause` cancels the token, but the worker stops asynchronously. `resume` re-queued the task at once, and `process_next` started a second worker while the first was still in `active`. Two transfers then wrote one temp file. When the old worker finished with `Canceled`, `finish` saw status `Downloading` (the new run) and marked the task `Failed`.
   - **Fix:**
     - `process_next` skips a queued task that still has a running worker.
     - `finish` treats a `Queued` status as "re-queued while winding down": it records nothing, and its own `process_next` call restarts the task from the temp file.
     - A task therefore has at most one transfer at a time.
6. **Search terms not encoded.**
   - **Cause:** `search_url` replaced only spaces with `+`, so `Mount & Blade II` sent `?s=Mount+&+Blade+II`. The site read `s=Mount ` plus a stray parameter. `+` became a space, and `#` cut the URL.
   - **Fix:** a `form_encode` helper applies `application/x-www-form-urlencoded` rules, as a browser does when it submits the site's search form. Spaces become `+`, and every byte except `A-Z a-z 0-9 * - . _` becomes `%XX`. This also changes `:` to `%3A`, so the existing Ace Combat test expectation was updated.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-download/src/queue.rs` | `prepare_start` no longer deletes `save_path`. `complete` returns `Result`, renames over the old copy without deleting it first, and fails on a rename error or a missing temp file. `process_next` skips tasks with a running worker. `finish` handles `Queued` and routes `complete` errors to `Failed`. The module doc is updated. Three tests were added: `quick_pause_and_resume_waits_for_the_old_transfer`, `failed_redownload_keeps_the_existing_file` and `failed_rename_fails_the_task_instead_of_completing_it`. | Defects 1, 2, 5 |
| `crates/fling-net/src/reqwest_client.rs` | `transfer` builds its own request from a `resume_from` argument. It rejects `text/html` replies, and it returns `AlreadyComplete` only on a matching `Content-Range: bytes */N`, otherwise the new `RangeRejected`. `download` restarts from scratch on `RangeRejected`. A shared `truncate` helper was added. The `Done` variant no longer carries the content type, which is now checked earlier. The 416 test now sends `Content-Range`. Two tests were added: `status_416_on_mismatched_partial_restarts_from_scratch` and `html_pages_are_rejected_even_with_a_body`. | Defects 3, 4 |
| `crates/fling-site/src/site.rs` | `search_url` form-encodes the term with the new `form_encode` helper. The Ace Combat expectation was updated (`:` → `%3A`). `search_url_encodes_query_syntax_in_titles` was added, covering `&`, `:`, `+`, `#`, `=`, `%` and CJK. | Search encoding |
| `CLAUDE.md` | The `fling-net` note now gives the 416 rule and says an HTML reply is never saved. | Keeps the agent guide accurate |
| `docs/work-logs/2026-10-19-download-integrity.md` | This log. | Mandatory record |
| `docs/pull-requests/2026-10-19-download-integrity.md` | The PR record. | Mandatory record |

## 4. Verification
All commands ran on the Windows toolchain, from WSL through `cmd.exe`.
- `cargo test -p fling-net -p fling-download -p fling-site`: all passed (11 + 20 + 37 unit tests, plus 3 site fixture tests).
- **Mutation check:**
  - I removed the `active.contains` skip and the rename-error return from `queue.rs`. `quick_pause_and_resume_waits_for_the_old_transfer` and `failed_rename_fails_the_task_instead_of_completing_it` then both failed.
  - I restored the file from a copy afterwards; `git diff --stat` matched the earlier diff.
  - The other new tests were not mutation-checked. Each one asserts the exact behavior the old code got wrong: a 404 deleting the old file, a 416 without a matching size, an HTML body, an unencoded `&`.
- **Full run:** `cargo test --workspace`, then `cargo clippy --workspace --all-targets -- -D warnings`, then `cargo fmt --all --check`, chained in one invocation. It exited 0.
  - The `CLAUDE.md` edit was made after this run. It is Markdown only and not covered by fmt.
- **Live search:** `cargo run -p fling-app --example headless -- search Mount & Blade II` ran the real backend against flingtrainer.com in a throwaway data directory. It returned 1 result, "Mount & Blade II: Bannerlord Trainer", with its 18 download versions.
  - I tried a quick `curl` comparison of the old URL, `?s=Mount+&+Blade+II`. It was inconclusive: sidebar links appear on both pages, and searching "Mount" alone also finds Bannerlord. So no before/after result count is claimed.
- **Not run:**
  - The GUI was not rebuilt (`cargo build -p fling-ui`).
  - No download, pause/resume or re-download was tried in the app.
  - The download fixes were not exercised against the live site.

## 5. Open items
- **Pre-existing, not changed:** when a re-download's content is an `.exe` and the old corrected `X_v1.exe` exists, `correct_file_extension` does not overwrite it. The new file stays as `X_v1.zip`. This was the behavior before this change too.
- **Pre-existing, not changed:** removing a paused task leaves its `.crdownload` file behind.
- **Pre-existing, not changed:** the headless output shows the title as "Mount &#038; Blade II". The list parser leaves that entity undecoded. CLAUDE.md says parser quirks are kept on purpose, so it was left alone.
- A 206 reply whose `Content-Range` start differs from the resume offset is still appended as-is. That case was not in the request.
- An HTML reply on a resume now deletes the partial file, as other non-cancel failures already did. The next attempt therefore starts from zero.
