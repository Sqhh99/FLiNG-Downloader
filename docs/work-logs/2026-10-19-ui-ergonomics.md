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

---

# Follow-up: the remaining review findings

- **Branch:** `fix/ui-review-remaining`, stacked on `fix/ui-ergonomics` (PR #54). Both change the same views and the same review record.
- **Related:** [PR record](../pull-requests/2026-10-19-ui-review-remaining.md)

## 1. Request
> Please help me continue to fix the remaining issues

The remaining findings were 3, 5, 6 and 8–18, plus the Recycle Bin part of 7 that the first PR left open.

## 2. Plan
1. **Backend** (3, 5, 7, 15): report missing library files; stop offering dev builds their base release; delete library files to the Recycle Bin through an injectable function; tell failed searches apart from empty ones.
2. **Accessibility and keyboard** (8, 9): accessible names on icon buttons and pickers; roles for tabs, lists, rows and swatches; Tab stops with focus styles; Enter/Space/Delete on rows; Ctrl+F.
3. **Layout and wording** (10–16, 18): a labeled Download button and short version labels; tooltips for cut-off text; a search bar that stays usable during a search; a list-remove icon; short dates; empty-state messages; a badge that counts queued downloads and shows failures; a drawer below the title bar that pushes the page aside.
4. **Executable** (17): a 256 px icon image and fuller version information.
5. Run the tests, clippy, fmt and `cargo xtask notices`. Screenshot the changed screens, re-query UI Automation, and inspect the rebuilt exe with REA.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `crates/fling-download/src/library.rs`, `lib.rs`, `Cargo.toml` | `Library::load` takes a `DeleteFile`. New `move_to_recycle_bin` (via `trash`) and `delete_permanently`. `RunTarget::Missing` carries the removed record. | 7, 3 |
| `Cargo.toml`, `Cargo.lock`, `THIRD_PARTY_NOTICES.md` | Workspace dependency `trash` 5.2.9, MIT, Windows only, with multithreaded COM because deletes run on tokio workers. Regenerated notices; `trash` is the only new entry. | 7 |
| `crates/fling-app/src/lib.rs`, `backend.rs`, `api.rs` | `BackendConfig::delete_file`. `SearchDone.failed` and `Event::SearchFailed`. `Event::LibraryFileMissing`. | 7, 15, 3 |
| `crates/fling-app/tests/backend.rs` | The harness passes `delete_permanently`. New tests `opening_a_trainer_whose_file_is_gone_reports_it` and `failed_searches_are_told_apart_from_empty_ones`. | 3, 15 |
| `crates/fling-core/src/version.rs`, `crates/fling-update/src/updater.rs` | `is_update`, with a test; the updater uses it. | 5 |
| `crates/fling-ui/src/state.rs` | `LibraryNotice` (delete failed / file missing) replaces `delete_error`. Adds `search_failed`, `pending_downloads` and `failed_downloads`. | 3, 15, 16 |
| `crates/fling-ui/src/views/widgets.rs` | Accessible icon buttons. `text_tooltip` and `tooltip_cell`. Focusable list-item rows with a name. `trainer_full_name`. `empty_table` takes a message. | 8, 9, 11, 15 |
| `crates/fling-ui/src/views/root.rs` | Accessible, focusable tabs. Ctrl+F. The new badge. The drawer sits below the title bar and pushes the page aside. | 8, 9, 16, 18 |
| `crates/fling-ui/src/views/search_page.rs` | No lock while loading. Wider, labeled sort selector. Row names and keyboard opening. Failure and no-match empty states. `focus_input`. | 8, 9, 11, 12, 15 |
| `crates/fling-ui/src/views/library_page.rs` | Notice banner for both kinds. Row names. Enter runs the trainer and Delete asks. Version tooltip. Empty-library text. | 3, 8, 9, 11, 15 |
| `crates/fling-ui/src/views/detail_drawer.rs` | Short version labels, with a test. Labeled primary Download. Picker and title tooltips. Cached cover size. No drag area in the header. | 6, 10, 11, 18 |
| `crates/fling-ui/src/views/downloads_panel.rs`, `assets.rs` | `ListX` remove icon, registered in `AppIcons`. File-name tooltip. | 13, 11 |
| `crates/fling-ui/src/views/settings_panel.rs`, `i18n.rs` | Accessible, focusable sections, swatches and link. Wrapped source hint. `format_published`, with a test. | 8, 9, 11, 14 |
| `crates/fling-ui/locales/app.yml` | New `search.no_results`, `search.failed`, `sort.label`, `library.file_missing`, `library.empty`, `a11y.result_row` and `a11y.library_row`. Changed `downloads.remove` and `library.confirm_delete`. Removed `table.empty`. All entries in zh-CN, en and ja. | 3, 7, 8, 13, 15 |
| `resources/icons/app_icon.ico` | Adds a 256 px PNG image, upscaled from `app_icon.png`. | 17 |
| `crates/fling-ui/build.rs` | `CompanyName`, `LegalCopyright`, `OriginalFilename` and `InternalName`. | 17 |
| `CLAUDE.md` | Notes the Recycle Bin and the two `BackendConfig` test seams. | 7 |
| `docs/code-reviews/2026-10-19-ui-ergonomics-and-defects.md` | All findings marked implemented, with Fix notes. | — |

## 4. Verification
- **Commands** (Windows through `cmd.exe`):
  - `cargo test --workspace`: 167 tests passed.
  - `cargo clippy --workspace --all-targets -- -D warnings`: clean.
  - `cargo fmt --all --check`: passed.
  - `cargo xtask notices`: only `trash` was added.
- **Screenshots** of the rebuilt debug app via `FLING_DEBUG_OPEN`. The app read the maintainer's settings and library read-only, and nothing was clicked.
  - Search page: "Recent Updates" shown in full.
  - `drawer:1` at 950 px and 800 px: the drawer sits below the title bar, the page is pushed aside, the label reads "v1.0.x Plus 44", and **Download** is a primary button.
  - `library` and `library-delete:0`: the banner mentions the Recycle Bin.
  - `settings-about`: the dev build is "latest", dates are trimmed to the day, and the hint wraps.
  - `downloads`.
- **UI Automation:** 40 elements (21 before), 1 unnamed (18 before), 38 keyboard-focusable. Tabs, list items with full names, and a labeled sort combo box are present.
- **REA** `inspect_pe_resources` on the rebuilt exe: five icon images including 256 px, and the new version-resource strings.
- **Not checked:**
  - Any keyboard or mouse interaction: Tab order, Enter/Space/Delete on rows, Ctrl+F, tooltips appearing, Cancel/Delete in the banner.
  - The actual move to the Recycle Bin.
  - The drawer re-search wiring.
  - The failed-search message on screen (only the backend event is tested).
  - The badge with queued or failed tasks.

## 5. Open items
- Arrow-key navigation between rows is not implemented; rows are reached with Tab.
- With the drawer open at the 800 px minimum width, table headers are cut off.
- The 256 px icon image is an upscale; a drawn 256 px icon would be sharper.
- `Library::load` still silently drops entries whose file vanished while the app was closed.
- The two "Auto Check" switches have the same accessible name.
