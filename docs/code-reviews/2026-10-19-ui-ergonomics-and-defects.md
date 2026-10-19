# UI Ergonomics and Defects Code Review

- **Date:** 2026-10-19
- **Reviewed at commit:** `449c544` (`main`, after PR #52 and PR #53 were merged). The screenshots came from a debug build of `ea99c53`. #52 and #53 did not change `fling-ui`, `fling-app`, `fling-download/src/library.rs` or `fling-update`, so they show the same UI.
- **Request:** "You can use REA to help me detect software defects and review whether the software's UI design is ergonomic."
- **Scope:**
  - **REA, static checks on `target/release/fling-downloader.exe` (FileVersion 1.2.1):**
    - `inspect_pe_resources`: icon group, version resource and manifest;
    - `inspect_rust_binary`: compiler and dependency metadata.
  - **Running the app:**
    - Built `cargo build -p fling-ui` (debug) and launched it from Windows with each `FLING_DEBUG_OPEN` value: none, `drawer:1`, `drawer:2`, `suggest:elden`, `downloads`, `settings`, `settings-download`, `settings-language` and `settings-about`.
    - Screenshots were taken with `PrintWindow`, at the default 950×650 window and again at the 800×600 minimum.
    - Queried the window's Windows UI Automation tree, which is what screen readers use.
  - **Source reading:**
    - every file under `crates/fling-ui/src/views/`, plus `state.rs` and `main.rs`;
    - the command handlers in `fling-app/src/backend.rs`;
    - `fling-download/src/library.rs` and `queue.rs` (`remove`);
    - `fling-app/src/names.rs`, `fling-site/src/parser/{mod,list}.rs`, and `fling-core/src/version.rs` with its caller in `fling-update`.
- **Not covered:**
  - **No disassembly or decompilation.** REA had no deep provider for this target: Ghidra, IDA and Hopper are not configured, and Kuna does not accept PE files.
  - **No REA UI capture.** REA's native-UI capture works only for macOS (Mach-O) apps, so the UI was observed through the Windows-side screenshots and UI Automation query above instead.
  - **No interactive input:** nothing was clicked, typed or scrolled. Findings about behavior after a click come from reading the code and were not reproduced.
  - **Only the user's current settings:** English UI, Simplified Chinese trainer names, Sunset theme. Other languages, themes and DPI scales were not looked at.
  - **Out of scope:** download and network internals, which were reviewed for PR #52; the update installer; cover detection; tests.
  - **The library page and a populated download list were not screenshotted.** The library had nothing to show without real downloads, and starting downloads was out of scope.

## Status summary
Findings 1, 2, 4 and 7 were fixed on 2026-10-19 on the same branch; see the [work log](../work-logs/2026-10-19-ui-ergonomics.md). For finding 7, deleting now asks for confirmation first, but files are still deleted outright rather than moved to the Recycle Bin (see its Fix note). The other findings are open. Findings 1–6 are defects: the code does something wrong. Findings 7–18 are about ergonomics: the code works as written, but the design makes the app harder or riskier to use. For each finding, the record says how it was established: observed in a screenshot or the UI Automation tree, or from reading the code.

| # | Finding | Severity | Implemented |
|---|---------|----------|-------------|
| 1 | Raw HTML entities (`&#8211;`) in trainer names, drawer titles and new file names | Medium | ✅ Yes — 2026-10-19 |
| 2 | The details toggle compares row numbers across different result lists | Medium | ✅ Yes — 2026-10-19 |
| 3 | Double-clicking a library entry whose file is gone silently deletes the entry | Medium | ❌ No — open |
| 4 | Installing an app update quits the app mid-download without asking | Medium | ✅ Yes — 2026-10-19 |
| 5 | Dev builds are offered their own base release as an "update" | Low | ❌ No — open |
| 6 | The drawer reads the cover file from disk on every repaint | Low | ❌ No — open |
| 7 | Library **Delete** permanently deletes the trainer with one click | High | ✅ Yes — 2026-10-19 (confirmation; no Recycle Bin) |
| 8 | Icon buttons have no accessible name; tabs and table rows are invisible to screen readers | Medium | ❌ No — open |
| 9 | The mouse is required for tabs, rows, settings sections and themes | Medium | ❌ No — open |
| 10 | Download is a small icon, and version names are cut off where they differ | Medium | ❌ No — open |
| 11 | Cut-off text with no way to read the rest | Low | ❌ No — open |
| 12 | The whole search bar is locked while a search runs, with no cancel | Low | ❌ No — open |
| 13 | "Remove" (list only) and "Delete" (file) use the same icon, and Japanese uses the same word | Low | ❌ No — open |
| 14 | Update dates are raw ISO timestamps | Low | ❌ No — open |
| 15 | Empty tables say only "No data available" | Low | ❌ No — open |
| 16 | The title-bar badge counts only running downloads | Low | ❌ No — open |
| 17 | The exe icon has no 256 px image, and the version resource is sparse | Low | ❌ No — open |
| 18 | The drawer hides the search toolbar and the table's action column | Low | ❌ No — open |

## 1. Raw HTML entities in trainer names, drawer titles and new file names
**Location:**
- `crates/fling-app/src/names.rs:50`
- `crates/fling-site/src/parser/list.rs:170`
- `crates/fling-site/src/parser/mod.rs:37`

**Severity:** Medium · **Implemented:** ✅ Yes — 2026-10-19

**Observed:** the home list shows "R.I.P. &#8211; Reincarnation Insurance…" in both the result row and the drawer title (screenshots `01-search`, `02-drawer`).

**Why it happens**
- The parser keeps the raw title in `ModifierInfo::name` on purpose. This is a Qt quirk: cover ids and the library key on the raw name.
- `names::label` is supposed to make the display name. It decodes entities only to look up a translation. When there is no translation, or the language is English, `untranslated()` returns `site_name` unchanged.
- The same label then becomes the stem of the downloaded file's name (`Command::Download` in `backend.rs`), so the file name contains `&#8211;` too.
- `decode_html_entities` also has no `&#038;`. The site uses that form in "Mount &#038; Blade", so decoding the display name alone would not fix that title.

**Fix direction:**
- Decode in the display layer only: the `untranslated` title in `names::label`.
- Add `&#038;` to the decoder's table.
- Keep `name` raw, so cover ids and library records keep matching.

**Fix.**
- `names::label` now decodes the site name it falls back to, so the list, the drawer title, the library and new download file names all show "R.I.P. - Reincarnation Insurance Program Trainer".
- `decode_html_entities` maps `&#038;` to `&`. That also helps search matching and translation lookups, which go through the same decoder.
- `ModifierInfo::name` is unchanged.
- Tests: `names::tests::english_and_misses_keep_the_site_name_without_subtitle` and `parser::tests::decodes_site_entities_only`.
- Checked in a screenshot of the list, the drawer and the library.

## 2. The details toggle compares row numbers across different result lists
**Location:** `crates/fling-ui/src/views/root.rs:84` · **Severity:** Medium · **Implemented:** ✅ Yes — 2026-10-19

`Root::drawer_row` stores which row's details are open, as a row number. It is cleared only when the drawer closes. A new search, **Show All** or a re-sort replaces `results`, and `SearchPage` resets its own `selected_row`, but `drawer_row` is left as it was. Two consequences:
- **The drawer shows the wrong trainer.** It keeps showing the previous trainer next to a list that no longer contains it.
- **The Details button can close the drawer instead.** Clicking Details (or double-clicking) on a row with the same number as the old one now closes the drawer, instead of showing that row's trainer.

**How this was established:** from the code only. It was not reproduced, because no input was sent.

**Fix direction:** clear `drawer_row` when `results_generation` changes. Alternatively, key it on the trainer URL instead of the row number.

**Fix.**
- `Root` now also stores the trainer's URL (`drawer_url`) when it opens the drawer.
- When `results_generation` changes, it moves `drawer_row` to that trainer's row in the new list. If the trainer is not in the new list, it closes the drawer.
- Empty placeholder URLs never match.
- Test: `views::root::tests::drawer_follows_its_trainer_into_a_new_list` covers the row lookup. The wiring in `Root`'s observer was not exercised, because no input can be sent to the app.

## 3. Double-clicking a library entry whose file is gone silently deletes the entry
**Location:**
- `crates/fling-download/src/library.rs:98`
- `crates/fling-app/src/backend.rs:323`

**Severity:** Medium · **Implemented:** ❌ No — open

When the file is missing, `run_target` removes the record and returns `RunTarget::Missing`. The backend then just re-emits the library. To the user, a double-click makes the row vanish, with no message saying the file was moved or deleted and no offer to download it again.

**How this was established:** from the code only.

**Fix direction:** show a banner like the one for a failed delete. Better still, keep the record, mark it "file missing", and offer Re-download or Remove.

## 4. Installing an app update quits the app mid-download without asking
**Location:**
- `crates/fling-app/src/backend.rs:777`
- `crates/fling-app/src/backend.rs:828`

**Severity:** Medium · **Implemented:** ✅ Yes — 2026-10-19

`download_update` and the installer launch never look at the download queue. 300 ms after the installer starts, `Msg::QuitAfterInstaller` quits the app, ending any trainer downloads in progress. The task list lives only in memory, so those tasks are lost; only their `.crdownload` files stay on disk.

**How this was established:** from the code only.

**Fix direction:**
- If downloads are active, say so in the update card and ask before installing.
- Or start the installer only after the queue is idle.

**Fix.**
- If a trainer download is running or queued when the installer finishes downloading, the backend keeps the installer and sets the new status `UpdateStatus::WaitingForDownloads`. The status line reads "Installer downloaded. Setup starts when your downloads finish." (zh-CN, en and ja).
- The first queue snapshot with no running or queued task starts the installer.
- While it waits, **Download and Install** is disabled, and a repeated command is ignored.
- Paused tasks do not hold the installer back.
- `BackendConfig` gained `launch_installer`, so tests never start a process.
- Test: `app_installer_waits_for_running_trainer_downloads`. A mutation check that skips the wait makes the test fail.

## 5. Dev builds are offered their own base release as an "update"
**Location:**
- `crates/fling-update/src/updater.rs:95`
- `crates/fling-core/src/version.rs:64`
- `crates/fling-ui/build.rs`

**Severity:** Low · **Implemented:** ❌ No — open

**Observed:** the About page of `v1.2.1-dev.2+gea99c53` says "New version available: 1.2.1" and offers **Download and Install** (screenshot `08-settings-about`).

**Why it happens:** `build.rs` names builds made after a tag `X.Y.Z-dev.N`. Under semver a prerelease sorts *below* `X.Y.Z`, so the comparison calls the build older than the release it came from. Installing would downgrade it.

**Who is affected:** only local and dev builds. Release builds carry the exact tag.

**Fix direction:**
- Do not offer an update when the core versions are equal and the current version is `-dev.N`.
- Or name dev builds after the next patch version.

## 6. The drawer reads the cover file from disk on every repaint
**Location:** `crates/fling-ui/src/views/detail_drawer.rs:109` · **Severity:** Low · **Implemented:** ❌ No — open

`render_cover` calls `cover_size(path)`, which opens the PNG and reads its header. `render` runs on every repaint, including each frame of the drawer, fade and row animations.

**Cost:** a 24-byte read is cheap, but it is synchronous file I/O on the UI thread.

**Fix direction:** cache the size per cover path, for example in `DetailDrawer`.

## 7. Library Delete permanently deletes the trainer with one click
**Location:**
- `crates/fling-ui/src/views/library_page.rs:89`
- `crates/fling-download/src/library.rs:111`

**Severity:** High · **Implemented:** ✅ Yes — 2026-10-19 (confirmation; no Recycle Bin)

**What happens:** the Trash button deletes the downloaded trainer from disk with `fs::remove_file`, which bypasses the Recycle Bin. There is no confirmation and no undo.

**Why it is easy to hit by accident:**
- The button is a 24 px ghost icon placed right next to **Open folder**.
- Its row also reacts to double-click, which runs the trainer.

**How this was established:** from the code only. The library page was not screenshotted.

**Fix direction:**
- Ask for confirmation, or show an undo banner.
- Move the file to the Recycle Bin instead of deleting it outright.

**Fix.**
- The Trash button now selects the row and shows a red banner: "Delete "<name>"? The trainer file is removed from disk and can't be restored." It has **Cancel** and **Delete** buttons.
- The pending delete is tracked by file path, and the row is looked up again on confirmation, so a library change cannot redirect it.
- Checked in a screenshot (`FLING_DEBUG_OPEN=library-delete:0`, a new debug-only value). The buttons were not clicked.
- **Not done: Recycle Bin.** It would need a new dependency (`trash`, or Windows shell calls). Library tests would then move files into the developer's real Recycle Bin unless that step were also injected. Left for a follow-up.

## 8. Icon buttons have no accessible name; tabs and table rows are invisible to screen readers
**Location:**
- `crates/fling-ui/src/views/widgets.rs:12` (`icon_button`)
- `crates/fling-ui/src/views/root.rs:220` (`render_tab`)
- `crates/fling-ui/src/views/widgets.rs:64` (`table_row`)

**Severity:** Medium · **Implemented:** ❌ No — open

**Observed** in the UI Automation tree of the search screen. It has 21 elements:
- 18 are buttons with an empty name: Downloads, Settings, and every row's Details button. `icon_button` sets a tooltip, but not an accessible name.
- The sort combo box has no name either.
- The tabs and the result rows (game name, version, date) are missing from the tree. A screen-reader user cannot tell what the results are.

**Fix direction:** GPUI (`gpui-pre` 0.3.7) has `aria_label` and `role` on elements, so the tooltip text could become the accessible name. It was not checked whether `gpui-component`'s `Button` passes `aria_label` through.

## 9. The mouse is required for tabs, rows, settings sections and themes
**Location:**
- `crates/fling-ui/src/views/root.rs:136`, `root.rs:220`
- `crates/fling-ui/src/views/settings_panel.rs:212`
- `crates/fling-ui/src/views/search_page.rs` (`on_key`)

**Severity:** Medium · **Implemented:** ❌ No — open

The tabs, table rows, settings sections and theme swatches are clickable `div`s with no focus handling. The only keys the app handles are:
- Esc, which closes overlays;
- up, down, Enter and Esc inside the suggestion list.

So a result cannot be selected with the arrow keys, or opened with Enter, and the tabs cannot be reached from the keyboard.

**How this was established:** from the code only.

## 10. Download is a small icon, and version names are cut off where they differ
**Location:** `crates/fling-ui/src/views/detail_drawer.rs:231`, `detail_drawer.rs:233` · **Severity:** Medium · **Implemented:** ❌ No — open

**The Download button is easy to miss.** Downloading is the drawer's main action, but its button is a ghost icon with only a tooltip, at the end of the version row.

**Version names are cut off.**
- The version picker shows raw file names, such as "R.I.P.-.Reincarnation.Insurance.Program…" and "DragonSword.Awakening.v1.0…".
- Those names are cut off at the end, which is exactly where versions differ (version range, date).
- It is worse at the 800 px minimum width, where the drawer is 320 px wide (screenshots `02-drawer` and `13-drawer-min`).

**Fix direction:**
- Make Download a labeled primary button.
- Show a shortened version label (the part after the game name), with the full name in a tooltip.

## 11. Cut-off text with no way to read the rest
**Location:**
- `crates/fling-ui/src/views/search_page.rs:409`
- `crates/fling-ui/src/views/settings_panel.rs:593`
- the drawer title, and the file names in the download list

**Severity:** Low · **Implemented:** ❌ No — open

**Observed:**
- The sort selector is a fixed 130 px and shows "Recent Upd…" in English (`01-search`).
- The update-source hint shows "Used for both software and database u…" (`08-settings-about`).
- Long drawer titles are cut off ("R.I.P. &#8211; Reincarnation Insura…").

None of these has a tooltip with the full text.

## 12. The whole search bar is locked while a search runs, with no cancel
**Location:** `crates/fling-ui/src/views/search_page.rs:390`, `:404`, `:410`, `:418` · **Severity:** Low · **Implemented:** ❌ No — open

While a search is loading, the input, **Search**, the sort selector and **Show All** are all disabled. A request may take up to the 30 s HTTP timeout. During that time the user cannot correct a typo or start a different search. The backend already throws away out-of-date answers by request id, so leaving the input enabled would be safe.

## 13. "Remove" (list only) and "Delete" (file) use the same icon, and Japanese uses the same word
**Location:**
- `crates/fling-ui/src/views/downloads_panel.rs:122`
- `crates/fling-download/src/queue.rs:287`
- `crates/fling-ui/locales/app.yml:207`

**Severity:** Low · **Implemented:** ❌ No — open

**The mismatch:**
- In the download list, `RemoveTask` takes an entry off the list and leaves the file on disk.
- It uses the same Trash icon as the library's **Delete**, which does delete the file.
- In Japanese both are labeled 削除 ("delete").

**The effect:** a user can't tell whether clearing a finished download deletes the trainer.

**Fix direction:** use an X or "clear" icon here, and "リストから削除" ("remove from list") in Japanese.

## 14. Update dates are raw ISO timestamps
**Location:** `crates/fling-ui/src/views/settings_panel.rs:419` · **Severity:** Low · **Implemented:** ❌ No — open

**Observed:** "Published: 2026-10-04T07:00:18Z" (`08-settings-about`). Everywhere else the app shows dates as `yyyy-MM-dd`.

## 15. Empty tables say only "No data available"
**Location:** `crates/fling-ui/locales/app.yml:46` (`table.empty`) · **Severity:** Low · **Implemented:** ❌ No — open

The same text is used for three different situations:
- a search with no matches;
- a failed search;
- an empty library.

None of them says what to do next, for example "No trainers match "x". Try the English title", or "Downloaded trainers appear here".

**Not checked:** whether a failed search shows anything other than this text.

## 16. The title-bar badge counts only running downloads
**Location:** `crates/fling-ui/src/state.rs:68` · **Severity:** Low · **Implemented:** ❌ No — open

`active_downloads` counts only tasks that are downloading. When a download is queued, paused or has failed, the Downloads icon shows no badge. A failure that happens while the list is closed goes unnoticed.

## 17. The exe icon has no 256 px image, and the version resource is sparse
**Location:**
- `crates/fling-ui/build.rs:50`
- `resources/icons/app_icon.ico`

**Severity:** Low · **Implemented:** ❌ No — open

REA `inspect_pe_resources` on the release exe found:
- **The icon:** it has 16, 32, 48 and 64 px images. Explorer's large and extra-large views, and the taskbar at high DPI, scale the 64 px image up.
- **The version resource:** it sets only `ProductName`, `FileDescription`, `ProductVersion` and `FileVersion`. `CompanyName`, `LegalCopyright` and `OriginalFilename` are missing, so the file's Properties → Details page is mostly empty.

## 18. The drawer hides the search toolbar and the table's action column
**Location:** `crates/fling-ui/src/views/root.rs` (drawer overlay) · **Severity:** Low · **Implemented:** ❌ No — open

**What is covered:** the drawer spans the full window height and covers up to 40% of the width. That hides:
- **Search**, the sort selector and **Show All**;
- the Option Count and Actions columns (screenshots `02-drawer` and `13-drawer-min`).

**What still works:** Enter still searches, and a double-click on a visible row still switches trainers.

**Not checked:** whether the drawer should push the content aside instead. That is a design choice; it is recorded here only.

## Verified correct — do not "fix" these
- **DPI:** the manifest declares `PerMonitorV2` DPI awareness, `asInvoker` and Windows 10 compatibility (REA, release exe).
- **Settings at the minimum size:** the 700×560 settings dialog fits inside the 800×600 minimum window (`12-settings-about-min`).
- **Suggestions:** the suggestion popup is drawn above the results table (`03-suggest`).
- **Esc order:** Esc closes settings first, then the download list, then the drawer (`root.rs` `on_key`).
- **Paused speed:** a paused task's speed is reset to 0 when its worker stops (`queue.rs`, `finish`), so the list does not show a stale speed.
- **List vs file:** **Remove** in the download list never deletes the file (`queue.rs:287`). Finding 13 is about how this is presented, not about what happens.
- **Stale detail replies:** a detail reply that arrives late cannot overwrite a newer selection, because each request has an id (`backend.rs`, `request_detail`).
