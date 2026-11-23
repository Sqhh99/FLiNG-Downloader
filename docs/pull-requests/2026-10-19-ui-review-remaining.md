# PR: Fix the remaining UI review findings: accessibility, keyboard, Recycle Bin, clearer states and layout

- **Date:** 2026-10-19 / **Branch:** `fix/ui-review-remaining` → `fix/ui-ergonomics` (stacked on PR #54) / **Base commit:** `080b71f` / **Related records:** [code review](../code-reviews/2026-10-19-ui-ergonomics-and-defects.md) · [work log](../work-logs/2026-10-19-ui-ergonomics.md)

## Related
There is no issue for this. This PR finishes the [UI and defect review](../code-reviews/2026-10-19-ui-ergonomics-and-defects.md) of 2026-10-19. PR #54 fixed findings 1, 2, 4 and 7; this one fixes 3, 5, 6 and 8–18, and moves deleted library files to the Recycle Bin, which #54 left open.

It is stacked on PR #54 because both change the same views and the same review record. Please merge #54 first. GitHub then retargets this PR to `main`.

## What changed
**Safer and clearer**
- **Deleted trainers go to the Recycle Bin.** This uses the `trash` crate (MIT). The confirmation banner says so.
- **Missing files are explained.** Opening a library entry whose file is gone shows: ""<name>" is no longer at <path>, so it was removed from the list…". Before, the row just disappeared.
- **Empty lists say why.**
  - "No trainers found. Try the game's English title, or Show All…" when nothing matched.
  - "Couldn't reach flingtrainer.com. Check your connection…" when the site couldn't be reached.
  - "Trainers you download appear here…" for an empty library.
  - Before, all three said "No data available".
- **Dev builds** (`X.Y.Z-dev.N`) are no longer offered `X.Y.Z` as an "update", which was a downgrade.
- **The Downloads badge** counts queued downloads too, and shows "!" when a download has failed.

**Keyboard and screen readers**
- **Names:** every icon button has an accessible name, and so do the sort, version and update-source pickers. The tabs, settings sections, lists and rows have proper roles. Each row is announced with its name, date, version and option count.
- **Keyboard:** the tabs, rows, settings sections, theme swatches and the GitHub link can be reached with Tab and activated with Enter or Space.
  - Enter opens a result's details, or runs a library trainer.
  - Delete on a library row asks to delete it.
  - Ctrl+F jumps to the search box.

**Layout and wording**
- **Details drawer:**
  - **Download** is now a labeled primary button.
  - The version picker shows the part that differs ("v1.0.x Plus 44" instead of "R.I.P.-.Reincarnation.Insurance.Program…"), with the full file name on hover.
- **The drawer no longer covers the title bar or the page.** The window buttons, Downloads and Settings stay reachable, and the search page is pushed aside, so its toolbar and columns stay visible.
- **Tooltips** show the full text of trainer names, library version names, download file names and the drawer title.
- **Sort selector:** wide enough for "Recent Updates".
- **Settings, About page:** the update-source hint wraps instead of being cut off, and update dates read "2026-10-04" instead of "2026-10-04T07:00:18Z".
- **The search bar stays usable while a search runs.** A new search replaces the running one.
- **Download list:** "Remove" is now "Remove from list" (リストから削除), with a list icon instead of the trash icon used for deleting files.
- **The executable:**
  - It has a 256 px icon image (upscaled from the 200 px logo).
  - Its version information now includes company, copyright, original file name and internal name.
- **The drawer reads the cover file once per cover,** instead of on every repaint.

**For developers**
- `BackendConfig` has a new `delete_file` field. Tests pass `fling_download::delete_permanently`, so no test touches the real Recycle Bin. CLAUDE.md now mentions both test seams.
- **Events:** new `Event::SearchFailed(bool)` and `Event::LibraryFileMissing`.
- `Library::load` now takes the delete function.

## Type of change
- [x] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [ ] Docs, CI, or build only

## How it was tested
- [x] `cargo test --workspace` passes
- [x] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

**Commands:** all were run on Windows. 167 tests passed. `cargo xtask notices` added only `trash`.

**New tests**
- `opening_a_trainer_whose_file_is_gone_reports_it`
- `failed_searches_are_told_apart_from_empty_ones`
- `dev_builds_are_not_offered_their_own_base_release`
- `version_labels_keep_the_part_that_differs`
- `published_dates_drop_the_time`

**Checks without interaction**
- **Screenshots** with `FLING_DEBUG_OPEN`:
  - the search page;
  - `drawer:1` at 950 px and 800 px;
  - `library` and `library-delete:0`;
  - `settings-about`;
  - `downloads`.
- **Windows UI Automation:**
  - Before: 21 elements, 18 of them unnamed buttons, no tabs and no rows.
  - After: 40 elements, 1 unnamed (the tab-bar container), 38 keyboard-focusable.
- **REA** `inspect_pe_resources` on the rebuilt exe: five icon images including 256 px, and the new version strings.

**Why the app box is unticked:** nothing was clicked or typed. Please try these by hand before merging:
- Tab through the window, and use Enter, Space and Delete on rows, and Ctrl+F.
- Delete a library trainer, then check it is in the Recycle Bin.
- Search with the network off, to see the failure message.
- Check the badge with a queued or failed download.
- Hover over cut-off text to see the tooltips.

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI changes include before-and-after screenshots
- [x] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [x] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

- **Unticked:**
  - **CONTRIBUTING.md:** left for the maintainer.
  - **Screenshots:** before-and-after screenshots exist locally but can't be uploaded from here.
- **i18n:**
  - New keys: `search.no_results`, `search.failed`, `sort.label`, `library.file_missing`, `library.empty`, `a11y.result_row` and `a11y.library_row`.
  - Changed: `downloads.remove` and `library.confirm_delete`.
  - Removed: `table.empty`.
- **Packaging:**
  - `resources/icons/app_icon.ico` gained a 256 px image, and the Inno Setup script uses the same file.
  - New Windows-only dependency `trash`.
- **AI disclosure:** yes. The code, tests, strings and records were written with Claude Code. They were verified by the test, clippy and fmt runs, screenshots, the UI Automation tree and REA, as described above. The Chinese and Japanese strings would benefit from a native speaker's read.

**Open items**
- No arrow-key navigation between rows; rows are reached with Tab.
- At the 800 px minimum width, table headers are cut off while the drawer is open.
- The 256 px icon image is an upscale.
- `Library::load` still silently drops entries whose file vanished while the app was closed.
- The two "Auto Check" switches share one accessible name.
