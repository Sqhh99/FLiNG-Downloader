# PR: Keep downloads intact on failure, and encode search terms

- **Date:** 2026-10-19 / **Branch:** `fix/download-integrity` → `main` / **Base commit:** `f5dd82b` / **Related records:** [work log](../work-logs/2026-10-19-download-integrity.md)

## Related
There is no issue for this. The maintainer analyzed the download queue and the search, and reported five download defects and one search defect:

| Priority | Defect | Consequence |
|---|---|---|
| High | Existing file deleted before a re-download | The new request returns 404, and the original file is lost too. |
| High | Marked complete when the final rename fails | Reported as successful and added to the library, but the file is still `.crdownload`. |
| Medium | Non-empty HTML error pages accepted | A `200 OK` `text/html` page is saved in place of the trainer. |
| Medium | HTTP 416 treated as complete | With a 3-byte server file and a 5-byte partial file, the download reports success. |
| Medium | Race on rapid pause/resume | Two workers run for one task, and the old one's cancellation fails the new one. |
| — | Search terms not encoded | `&` or `+` in a title changes the query ("Mount & Blade II"). |

## What changed
**For users**
- **Re-downloading a trainer you already have no longer deletes it first.** The old file is replaced only when the new download has finished and been moved into place. A failed re-download leaves it untouched.
- **A download that cannot be moved into place now shows as failed, with the reason.** For example, the target is locked or is a folder. It is not added to the library. The downloaded data is kept, so **Retry** can resume from it.
- **An HTML page is never saved as a trainer.** A `text/html` reply fails with "Server returned HTML page instead of file", whatever its size or status.
- **Resuming checks the server's 416 reply.** The download counts as complete only when the server says its file is exactly as long as the partial file (`Content-Range: bytes */N`). Otherwise it starts again from the beginning.
- **Pausing and immediately resuming works.** The resumed download waits for the paused transfer to stop, then continues from its data. It no longer runs twice or ends as "failed".
- **Titles with `&`, `+`, `#`, `:` and similar characters are searched correctly.** For example "Mount & Blade II: Bannerlord".

**For developers**
- `fling-download` (`queue.rs`):
  - `prepare_start` no longer deletes the save path.
  - `complete` returns `Result`. It renames over the old file, and fails on a rename error or a missing temp file.
  - `process_next` does not start a task that still has a running worker.
  - `finish` treats `Queued` as "re-queued while winding down".
- `fling-net` (`reqwest_client.rs`):
  - `transfer` takes the resume offset and builds its own request.
  - It rejects `text/html` before writing anything.
  - It splits a 416 into `AlreadyComplete` (the `Content-Range` size matches) and `RangeRejected` (anything else). On `RangeRejected`, `download` truncates the file and retries once without `Range`.
- `fling-site` (`site.rs`): `search_url` applies `application/x-www-form-urlencoded` rules through a small `form_encode` helper, as a browser does when it submits the site's search form. `:` now goes out as `%3A`.
- `CLAUDE.md`: the `fling-net` note gives the 416 and HTML rules.

## Type of change
- [x] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [ ] Docs, CI, or build only

## How it was tested
- [x] `cargo test --workspace` passes
- [x] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

The three commands ran on the Windows toolchain, chained into one invocation that exited 0.

**New and changed tests**
- `fling-download`, three new:
  - `quick_pause_and_resume_waits_for_the_old_transfer`
  - `failed_redownload_keeps_the_existing_file`
  - `failed_rename_fails_the_task_instead_of_completing_it`
- `fling-net`:
  - New: `status_416_on_mismatched_partial_restarts_from_scratch`, which covers a 3-byte server file against a 5-byte partial, and a 416 with no size.
  - New: `html_pages_are_rejected_even_with_a_body`.
  - Changed: `status_416_on_complete_file_is_success` now sends a matching `Content-Range`.
- `fling-site`: new `search_url_encodes_query_syntax_in_titles`. The Ace Combat expectation changed from `:` to `%3A`.
- **Mutation check:** without the race fix and the rename check, the race and rename tests fail. With them, they pass.

**Live search.** The headless example (`cargo run -p fling-app --example headless -- search Mount & Blade II`) ran against flingtrainer.com in a throwaway data directory. It found the Bannerlord trainer and its 18 versions.

**Third box unticked.** The GUI was not rebuilt. Download, pause/resume and re-download were not tried in the app or against the live site. The maintainer should try them in a built app before merge:
- re-download an installed trainer while offline;
- pause and resume quickly;
- search "Mount & Blade".

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI changes include before-and-after screenshots
- [ ] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [ ] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

- **Unticked:**
  - Screenshots, strings, database and packaging: not applicable. There is no UI, `app.yml`, database or packaging change.
  - The two new task error messages are untranslated English. They are shown verbatim, like the existing ones ("Server replied: 404").
  - CONTRIBUTING.md: left for the maintainer.
- **AI disclosure:** yes. The code, tests and docs were written with Claude Code from the maintainer's defect list. They are verified by the automated tests, the mutation check and one live headless search. They were not verified interactively in the GUI.

**Open items** (pre-existing, not changed here)
- A re-downloaded `.exe` whose corrected name `X_v1.exe` already exists stays as `X_v1.zip`, because `correct_file_extension` never overwrites.
- Removing a paused task leaves its `.crdownload` file.
- A 206 reply whose `Content-Range` start differs from the resume offset is appended as-is.
