# PR: Confirm library deletes, decode trainer names, keep the drawer on its trainer, and let downloads finish before updating

- **Date:** 2026-10-19 / **Branch:** `fix/ui-ergonomics` → `main` / **Base commit:** `449c544` / **Related records:** [code review](../code-reviews/2026-10-19-ui-ergonomics-and-defects.md) · [work log](../work-logs/2026-10-19-ui-ergonomics.md)

## Related
There is no issue for this. A [UI and defect review](../code-reviews/2026-10-19-ui-ergonomics-and-defects.md) on 2026-10-19 found 6 defects and 12 ergonomics problems. This PR fixes the four the maintainer chose to fix first: findings 7, 1, 2 and 4. The review record is included, with its Implemented column updated.

## What changed
- **Deleting from the library asks first (finding 7).**
  - Before, the trash icon deleted the trainer file at once.
  - Now it shows a red banner, "Delete "<name>"? The trainer file is removed from disk and can't be restored.", with **Cancel** and **Delete**.
  - The delete is tracked by file, so a library change cannot redirect it to another row.
  - Files are still deleted outright, not moved to the Recycle Bin.
- **Trainer names no longer show HTML codes (finding 1).**
  - "R.I.P. &#8211; Reincarnation Insurance Program" now reads "R.I.P. - Reincarnation Insurance Program". The fix covers the results, the details drawer, the library and the names of new downloads.
  - `&#038;` ("Mount &#038; Blade") is now decoded too.
  - The site name stored for covers and the library is unchanged.
- **The details drawer follows its trainer (finding 2).**
  - **The bug:** after a new search, **Show All** or a re-sort, the drawer kept the old row number. Clicking Details on that row number could close the drawer instead of opening the new trainer.
  - **Now:** the drawer moves to its trainer's new row, or closes if the trainer is not in the new list.
- **App updates wait for trainer downloads (finding 4).**
  - **The bug:** **Download and Install** quit the app 0.3 s after starting the installer, ending any running downloads.
  - **Now:** if a trainer download is running or queued, the installer is kept, and the card says "Installer downloaded. Setup starts when your downloads finish." Setup starts once the queue has nothing running or queued.
  - Paused downloads do not hold it back.
  - While it waits, the install button is disabled.
- **For developers:**
  - `BackendConfig` has a new `launch_installer` field. Tests pass one that records the path instead of running it.
  - New debug-only values `FLING_DEBUG_OPEN=library` and `library-delete[:row]` (listed in CLAUDE.md).

## Type of change
- [x] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [ ] Docs, CI, or build only

## How it was tested
- [x] `cargo test --workspace` passes
- [x] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

**Commands:** all three were run on Windows. 162 tests passed.

**New or changed tests**
- `app_installer_waits_for_running_trainer_downloads`: holds a trainer download open, then checks:
  - the status is `WaitingForDownloads` and nothing has launched;
  - a second install request downloads nothing new;
  - the installer launches exactly once, after the trainer finishes, followed by `Quit`.

  With the wait skipped as a mutation check, this test fails.
- `drawer_follows_its_trainer_into_a_new_list`.
- `english_and_misses_keep_the_site_name_without_subtitle` (changed: English names are now decoded).
- `decodes_site_entities_only` (adds `&#038;`).

**Why the app box is unticked:** the debug app was built and screenshotted with `FLING_DEBUG_OPEN`, but nothing was clicked.
- **Screenshots:**
  - the results list, `drawer:1` and `library`, showing decoded names;
  - `library-delete:0`, showing the confirmation banner.
- **Still needs trying by hand before merge:**
  - Cancel and Delete in the banner;
  - re-search and re-sort with the drawer open;
  - an update started while a download runs.

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI changes include before-and-after screenshots
- [x] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [x] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

- **Unticked:**
  - **CONTRIBUTING.md:** left for the maintainer.
  - **Screenshots:** before-and-after screenshots exist locally (the review's screenshots and the ones listed above) but are not attached; GitHub images can't be uploaded from here.
- **i18n:** new keys `common.cancel`, `library.confirm_delete` and `update.waiting_for_downloads`, each in zh-CN, en and ja.
- **AI disclosure:** yes. The review, the code, the tests and the records were written with Claude Code. They were verified by the test, clippy and fmt runs, a mutation check and screenshots, as described above. The Chinese and Japanese strings would benefit from a native speaker's read.

**Open items**
- Recycle Bin for library deletes.
- Files downloaded before this change keep any entity text in their names.
- Findings 3, 5, 6 and 8–18 in the review are still open.
