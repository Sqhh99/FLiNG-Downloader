# Work log: UI review with REA, and fixes for four of its findings

- **Date:** 2026-10-19
- **Branch:** `fix/ui-ergonomics` (from `origin/main` at `449c544`, after PR #52 and PR #53 were merged)
- **Related:** [code review](../code-reviews/2026-10-19-ui-ergonomics-and-defects.md) · [PR record](../pull-requests/2026-10-19-ui-ergonomics.md)

## 1. Request
> You can use REA to help me detect software defects and review whether the software's UI design is ergonomic.

The first request was the review itself. It is archived as the code review linked above, with 6 defects and 12 ergonomics findings. The review proposed fixing findings 7, 1, 2 and 4 first and opening a PR. The maintainer answered:

> ok

So this log covers fixing those four findings on this branch and opening a pull request.

## 2. Plan
1. **#1, entities in names.** Decode the site name in the display layer (`names::label`) and leave `ModifierInfo::name` raw, because cover ids and the library key on it. Add the `&#038;` entity to the decoder.
2. **#2, stale drawer row.** Have `Root` remember the open trainer's URL, and move the drawer to that trainer's row in each new result list, or close it if the trainer is not there.
3. **#4, update during downloads.** Hold the downloaded installer while a trainer download is running or queued. Start it from the first queue snapshot with none. Add a launcher seam, so the backend test never starts a process.
4. **#7, one-click delete.** Ask for confirmation in an inline banner before deleting. Track the pending delete by file path, not row. Add a debug-only `FLING_DEBUG_OPEN` value so the banner can be screenshotted without clicking.
5. Run the tests, clippy, fmt and a mutation check. Rebuild the debug app and screenshot the changed screens.
6. Update the review's Implemented column, then write the PR record and open the PR.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-app/src/names.rs` | The `untranslated` label now decodes entities. The test's English expectation changed to `Tom & Jerry Trainer`, and the R.I.P. title was added as a case. | #1 |
| `crates/fling-site/src/parser/mod.rs` | `&#038;` → `&` in `decode_html_entities`, with a test. | #1 |
| `crates/fling-ui/src/views/root.rs` | Adds `drawer_url` and `seen_generation`. `open_drawer` and `close_drawer` replace the scattered `drawer_row` writes. The new `row_of` finds the trainer in a new list, and has a unit test. The `library` debug values open the Library tab. | #2, #7 (debug) |
| `crates/fling-app/src/api.rs` | New `UpdateStatus::WaitingForDownloads`. | #4 |
| `crates/fling-app/src/lib.rs` | New `InstallerLauncher` type and `BackendConfig::launch_installer`. `for_app` passes `fling_update::launch_installer`. | #4 |
| `crates/fling-app/src/backend.rs` | Adds `pending_installer`, `transfers_running`, `start_installer`. `download_done` waits when transfers are running, the queue snapshot handler starts the held installer, and `download_update` ignores repeats while one is held. | #4 |
| `crates/fling-app/tests/backend.rs` | `GatedClient` can now also gate downloads. The harness records launched installers instead of running them. New test `app_installer_waits_for_running_trainer_downloads`. | #4 |
| `crates/fling-ui/src/i18n.rs` | Status text for `WaitingForDownloads`. | #4 |
| `crates/fling-ui/src/views/settings_panel.rs` | **Download and Install** is disabled while the installer waits. | #4 |
| `crates/fling-ui/src/views/library_page.rs` | Trash now sets `pending_delete`, and a red banner offers **Cancel** / **Delete**. The row is resolved by file path when the delete is confirmed. Handles the `library-delete[:row]` debug value. | #7 |
| `crates/fling-ui/locales/app.yml` | New keys `common.cancel`, `library.confirm_delete` and `update.waiting_for_downloads`, each in zh-CN, en and ja. | #4, #7 |
| `CLAUDE.md` | Lists the new `library` and `library-delete[:row]` debug values. | #7 |
| `docs/code-reviews/2026-10-19-ui-ergonomics-and-defects.md` | The review. Findings 1, 2, 4 and 7 are marked implemented, with Fix notes. | — |

## 4. Verification
All cargo commands were run on Windows through `cmd.exe`.
- **Tests:** `cargo test --workspace` passed, 162 tests. That includes the new `app_installer_waits_for_running_trainer_downloads` and `drawer_follows_its_trainer_into_a_new_list`, the changed `english_and_misses_keep_the_site_name_without_subtitle` and `decodes_site_entities_only`, and `every_locale_has_every_key`.
- **Lint and format:** `cargo clippy --workspace --all-targets -- -D warnings` was clean, and `cargo fmt --all --check` passed.
- **Mutation check:** with the wait skipped (`if false && transfers_running(..)`), the installer test failed with a timeout at `backend.rs:147`. The source was restored afterwards.
- **App build and screenshots:** `cargo build -p fling-ui` (debug), then screenshots with `FLING_DEBUG_OPEN`:
  - none: the list shows "R.I.P. - Reincarnation Insurance Progra…";
  - `drawer:1`: the drawer title is decoded;
  - `library`: the library entry is decoded;
  - `library-delete:0`: the confirmation banner shows with Cancel and Delete.

  The app read the maintainer's real settings and library, read-only. Nothing was clicked, so nothing was deleted.
- **Not run:**
  - Clicking Cancel or Delete in the banner.
  - A real app update while a download runs. Only the backend test covers it.
  - Re-searching and re-sorting with the drawer open. Only `row_of` has a test; the wiring in `Root` does not.

## 5. Open items
- **#7 is only half done:** deleting is confirmed, but the file is not moved to the Recycle Bin. That needs a new dependency and a test seam (see the review).
- **Existing files keep their names:** files downloaded before this change keep any entity text in their names. Only new downloads get decoded names.
- **Other findings:** 3, 5, 6 and 8–18 in the review are still open.
