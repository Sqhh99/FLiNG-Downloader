# UI, Redundancy, and Performance Code Review

- **Date:** 2026-09-04
- **Reviewed at commit:** `743a4095ceb051555c43513f03ed0fdca1f67000` (`fix/download-review-findings`)
- **Request:** "CLAUDE.md AGENTS.md docs/code-reviews 帮我review一下项目代码，主要审查界面设计，代码冗余，性能优化"
- **Scope:** All QML pages, components, and theme definitions; the Backend call paths for selection, searching, downloading, persistence, and settings; relevant SearchManager, ModifierManager, DownloadManager, GameMappingManager, list-model, CoverExtractor, and Logger implementations. Source reading, caller searches, existing-test inspection, a read-only bundled-database query, and palette contrast calculations. The existing `resources/interface.png` was inspected as a historical design reference, not as a screenshot of this commit running.
- **Not covered:** A complete security/network audit, live website markup compatibility, installer/release workflows, vendored implementation review, all C++ modules, live desktop interaction, screen-reader testing, a full DPI/language/theme screenshot matrix, or measured CPU/GPU/frame-time benchmarks.

## Status summary

**Ten of the twelve findings were implemented on 2026-09-06** on branch
`fix/review-findings-2026-09-04`; see the [implementation work log](../work-logs/2026-09-06-review-findings-2026-09-04.md).
Finding 5 was implemented and then **reverted**: rebasing `IconButton` on
`AbstractButton` stretched every icon to its button's content area, so the title-bar
and drawer icons rendered oversized. It is open again - see the fix note below.
Finding 3 stays open by decision, not by oversight: publishing search results
progressively reshapes SearchManager's callback contract and was scoped out so it
can carry its own test pass. The "additional optimization candidates" below also
stay open - this document says to profile them first, and nothing has been measured.

Line numbers below are as they were at review time; the header pins the commit.

This request was a review; no production code or tests were changed *by it*. The design audit focused on desktop usability, state feedback, and readable controls. It did not treat stylistic preferences such as adding decorative animation or replacing system fonts as defects. See the [work log](../work-logs/2026-09-04-ui-redundancy-performance.md).

P1 means a high-priority correctness defect; P2 means a normal-priority user-visible or structural defect; P3 means a lower-priority efficiency improvement. Performance impact is not quantified unless explicitly stated.

| # | Finding | Severity | Implemented |
|---|---------|----------|-------------|
| 1 | Late detail replies can attach another game's download URLs to the current game | P1 | ✅ Yes - 2026-09-06 |
| 2 | A failed or empty detail response leaves the drawer loading indefinitely | P2 | ✅ Yes - 2026-09-06 |
| 3 | Search hides usable results until every metadata request finishes | P2 | ❌ No - deferred, see status summary |
| 4 | Two download libraries persist different paths and apply different update rules | P2 | ✅ Yes - 2026-09-06 |
| 5 | Custom actions omit keyboard activation and visible focus | P2 | ❌ No - attempted 2026-09-06, reverted |
| 6 | Accent and disabled text colors are used for readable text with insufficient contrast | P2 | ✅ Yes - 2026-09-06 |
| 7 | Empty-search keyboard and mouse paths use different homepage implementations | P2 | ✅ Yes - 2026-09-06 |
| 8 | Queued tasks expose a resume action that the backend rejects | P2 | ✅ Yes - 2026-09-06 |
| 9 | Failed file deletion still removes and persists the library entry | P2 | ✅ Yes - 2026-09-06 |
| 10 | Per-item folder actions open the current default directory instead of the item's directory | P2 | ✅ Yes - 2026-09-06 |
| 11 | New results discard the selected sort order while the selector retains it | P2 | ✅ Yes - 2026-09-06 |
| 12 | Completing one download resets the entire downloaded-list model | P3 | ✅ Yes - 2026-09-06 |

## 1. Late detail replies can attach another game's download URLs to the current game

**Location:** `src/Backend.cpp:393-408`; related `src/Backend.cpp:439-454`, `src/Backend.cpp:578-608`.
**Severity:** P1. **Implemented:** ✅ Yes - 2026-09-06.

Open game A, close its drawer before its detail request completes, and open game B. The callback captures only `this` and writes the returned versions, options, and screenshot into whichever `m_selectedModifier` is current. If A completes after B, the drawer retains B's name but receives A's download links. Starting a download then combines B's identity/save filename with A's versions.

The cover callback's `gameId != m_coverRequestId` check does not prevent this: the stale detail callback first changes B's screenshot URL, and `extractCover()` derives its ID from B's current name. That extraction can consequently pass the cover guard and cache A's cover under B.

**Fix.** Detail requests now carry a generation id, checked together with the requested URL before any state is touched; a stale reply is deleted and dropped. The previous row's versions and options are cleared when the request goes out, so nothing on screen is attributed to the wrong game while loading (`src/Backend.cpp`, `requestSelectedModifierDetail()`).

**Recommendation:** Give detail selection a monotonically increasing request generation; capture it and the requested URL, and reject outdated detail replies before any state mutation or cover work. Dispose of the returned object even when ignoring it. Clear selection-specific option state at request start.

**Regression coverage:** Use deferred network hooks for A and B, complete B before A, then assert B's version URLs, options, and download target stay associated with B. Also cover A-B-A navigation.

## 2. A failed or empty detail response leaves the drawer loading indefinitely

**Location:** `qml/components/DetailDrawer.qml:24-26`, `qml/components/DetailDrawer.qml:50-106`; `src/ModifierManager.cpp:72-78`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

The drawer defines loading as a nonempty game name with zero versions, and displays all content, including its close button, only when versions exist. On network failure `getModifierDetail()` returns a non-null, empty `ModifierInfo`; Backend's null-result branch is therefore not the failure path used here. The request has ended, but the drawer continues to show its loading overlay. A successfully fetched page with no downloadable versions has the same result.

**Fix.** `ModifierManager::getModifierDetail()` reports the outcome explicitly instead of synthesising an empty `ModifierInfo` on failure, and Backend exposes a `detailState` property - loading / ready / empty / error. The drawer drives its overlay off that, keeps a close button inside the overlay, offers a retry on error, and still renders the metadata of a page that parsed but offers no downloads.

**Recommendation:** Expose explicit detail loading/error/empty states from C++; stop loading on every terminal path and keep close/retry controls available. A valid detail page without downloadable versions should still show its metadata.

**Regression coverage:** Failed GET and a successful page without version links both leave loading, show an appropriate explanation, and allow closing/retrying.

## 3. Search hides usable results until every metadata request finishes

**Location:** `src/SearchManager.cpp:103-121`, `src/SearchManager.cpp:584-664`; `qml/pages/SearchPage.qml:311`, `qml/pages/SearchPage.qml:472-481`.
**Severity:** P2. **Implemented:** ❌ No - deferred.

If even one result lacks its option count or game version, the initial parsed list is withheld. Enrichment launches detail requests for incomplete rows and only invokes the result callback when the pending count reaches zero. Backend keeps `searchLoading` true throughout, so QML masks the table and disables search actions. A single slow detail request therefore prevents interaction with every otherwise usable result; the default network timeout is 30 seconds (`src/NetworkManager.cpp:13`). This is avoidable waiting, not evidence of a blocked GUI thread.

Enrichment only retains two fields from each detail response. Opening a result later downloads its detail page again through `ModifierManager::getModifierDetail()`.

**Recommendation:** Publish the initial list immediately, show unknown metadata explicitly, and update individual rows as bounded enrichment requests finish. Keep the search generation guard for those updates. Share a detail cache when the full parsed result is available.

**Regression coverage:** Defer one enrichment callback while allowing the rest to complete; initial results must already be visible and selectable. Complete an older search's enrichment after a newer search and assert it cannot update the new rows.

## 4. Two download libraries persist different paths and apply different update rules

**Location:** `src/ModifierManager.cpp:96-103`, `src/ModifierManager.cpp:295-318`; `src/Backend.cpp:1080-1124`, `src/Backend.cpp:1276-1301`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

The active download path passes a `.crdownload` destination through ModifierManager. DownloadManager intentionally leaves that suffix unchanged (`src/DownloadManager.cpp:54-70`). Before calling Backend's completion handler, ModifierManager adds that temporary path to its private library and synchronously writes `downloaded_modifiers.ini`. Backend then renames the file, corrects its extension, appends another library record, and writes `downloaded_modifiers.json`. The INI entry now names a file that has moved.

The policies also differ: ModifierManager upserts by name/version, while Backend always appends. Re-downloading the same version can produce two visible rows pointing at the same final path; deleting one removes the shared file while the other row remains. UI deletion only updates Backend's model/list and JSON, leaving the manager's in-memory library unsynchronized.

**Fix.** The redundant write is removed: `ModifierManager::downloadModifier()` no longer records the `.crdownload` path, leaving Backend as the single writer. Nothing read the INI library - Backend uses ModifierManager only for `getModifierDetail` and `downloadModifier`, and no caller anywhere touches `getDownloadedModifiers()` - so no migration was involved and the file, its loader and `addDownloadedModifier()` stay in place. Backend's append became an upsert keyed by name+version.

**Recommendation:** Have one owner finalize the file and then upsert a single authoritative library record using its actual path. Derive the QML model from that library. Plan any INI/JSON migration explicitly; preserve existing records and do not simply delete a legacy store without a migration decision.

**Regression coverage:** Successful download, extension correction, repeated download, deletion, and restart should all agree on one valid final path and one logical library entry.

## 5. Custom actions omit keyboard activation and visible focus

**Location:** `qml/components/IconButton.qml:9-58`; `qml/components/DetailDrawer.qml:280-309`; `qml/pages/DownloadedPage.qml:142-199`; `qml/components/SettingsDialog.qml:130-228`.
**Severity:** P2. **Implemented:** ❌ No - attempted 2026-09-06, reverted.

These actions are rectangles with MouseAreas, without a tab-focus policy, keyboard activation, or an accessible button name/role. Keyboard users cannot reach the title-bar settings/download actions, the drawer download action, the downloaded-row actions, or settings navigation through an equivalent control path. The reusable IconButton also exposes `iconColor` and `iconHoverColor`, but neither property affects its Image.

Separately, StyledButton, StyledSwitch, and StyledComboBox replace their visuals without drawing a `visualFocus` state. They retain control behavior, but keyboard focus is not represented by these custom backgrounds.

**Attempted and reverted (2026-09-06).** `IconButton` was rebased on `AbstractButton` for keyboard activation and `visualFocus`, with `Accessible.role`/`name`, the inline copies in DetailDrawer and DownloadedPage folded into it, the four settings nav rows extracted into a `SettingsNavItem`, and `visualFocus` added to StyledButton/Switch/ComboBox. **This broke every icon in the app:** a `Control` stretches its `contentItem` to the content area, so the Image scaled up to fill the button instead of staying at `iconSize`, and the title-bar and drawer icons rendered oversized. Reverted whole.

Whoever retries this: the accessibility approach is sound, the sizing is not. Constrain the Image inside the contentItem - wrap it in an `Item` and give the Image a fixed `width`/`height` of `iconSize` centred in it, rather than letting `fillMode` scale it - and verify against a running build before committing, on the title bar as well as the list rows.

**Recommendation:** Base shared actions on Button/ToolButton, supply a textual accessible name, and style hover, press, disabled, and keyboard focus consistently. Apply the shared component to the remaining inline actions. Add explicit keyboard row activation where the table needs it. Qt documents [keyboard-generated button clicks and accessible button text](https://doc.qt.io/qt-6/qml-qtquick-controls-abstractbutton.html) and the [visualFocus property](https://doc.qt.io/qt-6/qml-qtquick-controls-control.html#visualFocus-prop).

**Regression coverage:** A Qt Quick interaction test should navigate settings, activate a result/download, and pause/resume a task using only keys; inspect accessible names and visible focus. Native screen-reader behavior still needs a Windows check.

## 6. Accent and disabled text colors are used for readable text with insufficient contrast

**Location:** `qml/Main.qml:98-108`; `qml/themes/ThemeProvider.qml:20-31`, `qml/themes/ThemeProvider.qml:143-154`; `qml/components/StyledTextField.qml:23`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

The selected tab uses the theme's accent directly as 14-pixel text on `surfaceColor`. Calculations from the source palette, using linearized sRGB relative luminance, produce:

| Usage | Foreground | Background | Contrast |
|-------|------------|------------|----------|
| Light selected tab | `#2196F3` | `#FFFFFF` | 3.12:1 |
| Ocean selected tab | `#00BCD4` | `#FFFFFF` | 2.30:1 |
| Sunset selected tab | `#FF7043` | `#FFFDE7` | 2.67:1 |
| Light search placeholder | `#BDBDBD` | `#FFFFFF` | 1.88:1 |

For comparison, [W3C's normal-text readability criterion is 4.5:1](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html). It is used here as a design benchmark, not as a claim about legal obligations for this desktop app. The search placeholder and explanatory settings text are not disabled controls merely because their token is named `textDisabled`.

**Fix.** Two tokens were added to ThemeProvider - `primaryTextColor` and `textMuted` - computed per theme against that theme's `surfaceColor` with the formula used in this finding, each adjusted until it clears 4.5:1. Accent *fills* are unchanged. `textDisabled` stays on the genuinely disabled labels. These are palette calculations; how they look on screen still needs eyes on each theme.

**Recommendation:** Separate accent fills from accessible accent text and distinguish muted explanatory text from disabled-control text. Calculate contrast for actual foreground/background pairs across themes; the current weighted-brightness helper is not a contrast-ratio calculation. Keep focus/selection indicators distinct from text color.

**Verification:** The values above were calculated during this review. They are palette measurements, not a screenshot or font-rendering assessment.

## 7. Empty-search keyboard and mouse paths use different homepage implementations

**Location:** `qml/pages/SearchPage.qml:99-120`, `qml/pages/SearchPage.qml:173-178`; `src/SearchManager.cpp:62-64`, `src/SearchManager.cpp:273-338`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

With an empty input, the search button emits `refreshRequested()` and uses the recent-list path with retries/cache fallback. Enter instead emits `searchRequested("")`, which invokes `loadFeaturedModifiers()`. That separate homepage implementation looks for different markup, has no recent-list cache fallback, and appends article records after a generic parse. Its fallback article records explicitly use the current date, `Latest`, and an option count of `10` rather than extracted values (`src/SearchManager.cpp:319-323`). With matching markup, entries can also duplicate the generic parser's results.

**Fix.** SearchPage has one `submitSearch()`, called by Return, keypad Enter and the search button, so every empty query takes the recent-list path with its cache fallback. `loadFeaturedModifiers()` is left in place but is no longer reachable from the UI; removing it and consolidating the two homepage parsers is a separate decision.

**Recommendation:** Centralize submission/trim logic and use the recent-list implementation for every empty query. Consolidate homepage parsing, preserving unknown fields as unknown. Keep nonempty query behavior intact.

**Regression coverage:** Empty and whitespace-only inputs through Enter, keypad Enter, and the search button must follow the same data path, including offline cached results. A shared homepage fixture must not invent counts or duplicate URLs.

## 8. Queued tasks expose a resume action that the backend rejects

**Location:** `qml/components/DownloadListPopup.qml:223-240`; `src/Backend.cpp:492-494`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

While one download is active, queue another. Its button displays the continue icon/tooltip and calls `resumeDownload()`, but Backend accepts only `paused` or `failed`, so the action returns immediately. Backend already supports pausing a queued task (`src/Backend.cpp:465-470`), but the UI provides no way to reach that behavior.

**Fix.** Queued rows show the pause action and call `pauseDownload()`, which has always accepted `queued`. The dead resume path for queued tasks is gone.

**Recommendation:** Present queued tasks with the existing pause action, or explicitly define a separate start/prioritize action if that behavior is intended. Keep labels, visibility, and accepted state transitions together.

**Regression coverage:** Queue a second task, activate its button, and assert it transitions to paused and is not started when the first task finishes.

## 9. Failed file deletion still removes and persists the library entry

**Location:** `src/Backend.cpp:654-663`; UI entry point `qml/pages/DownloadedPage.qml:195-197`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

If removal fails, for example because Windows has a conflicting open file handle or the directory denies deletion, `QFile::remove()` returns false. Backend ignores that result and removes the row and persisted entry anyway. The UI consequently presents a successful deletion while leaving the file behind. Successful deletion is also immediate and permanent through the small row icon, with no confirmation or undo route.

**Fix.** `deleteModifier()` returns bool, keeps the model, the list and the JSON untouched when `QFile::remove()` fails, and emits `deleteFailed(name, filePath)`; DownloadedPage shows an inline banner. A record whose file is already missing is still removable. Confirmation or undo for accidental clicks remains a separate product decision and was not added.

**Recommendation:** On a removal error, keep the entry and display the error. Make the difference between deleting the file and removing a record explicit. For accidental-click recovery, choose either a confirmation flow or a recoverable deletion mechanism as a product decision.

**Regression coverage:** A controlled deletion failure leaves both the model and persisted record unchanged and surfaces an error; a genuinely missing file can still have its stale record removed.

## 10. Per-item folder actions open the current default directory instead of the item's directory

**Location:** `qml/Main.qml:199-200`, `qml/Main.qml:280-281`; `src/Backend.cpp:621-624`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

Download into directory A, change the default to B, and click the folder icon on the old library row or completed task. Both handlers discard the item index and call `openDownloadFolder()`, which reads the current configured directory B. Existing records already contain the correct file/save path, but it is not used.

**Fix.** Adds `openModifierFolder(index)` and `openTaskFolder(taskId)`, both resolving the item's own parent directory and falling back to the configured directory only when the item has no usable path. `openDownloadFolder()` stays for the title-bar action. Selecting the file in Explorer was not added.

**Recommendation:** Pass an item identity/path to an item-specific backend action, resolve its parent directory, and optionally select the file in Explorer. Keep a separate action for opening the current default directory.

**Regression coverage:** After changing the default download directory, folder actions for old items still resolve directory A.

## 11. New results discard the selected sort order while the selector retains it

**Location:** `qml/pages/SearchPage.qml:183-190`; `src/Backend.cpp:337-360`, `src/Backend.cpp:1215-1223`.
**Severity:** P2. **Implemented:** ✅ Yes - 2026-09-06.

Choose name or option-count sorting, then search or refresh. `setSortOrder()` only sorts the current snapshot; Backend stores no selected sort mode, and `finishSearchRequest()` installs the new list directly. The combo box still displays the user's prior sort mode. SearchManager's relevance sorting can also conflict with the selector's default recent-update label.

**Fix.** The mode is stored in `m_sortOrder` and the switch is extracted into `applySortOrder()`, called both when the user picks an order and when new results arrive. Consolidating the three sorting APIs across Backend, SearchManager and ModifierManager was not attempted.

**Recommendation:** Persist the selected sort mode in one model/proxy layer and apply it when results or sortable roles change. If relevance sorting is intentional for search, expose that mode in the UI. Avoid retaining three separate sorting APIs across Backend, SearchManager, and ModifierManager.

**Regression coverage:** Choose a sort order, replace results and update enriched fields, then verify both actual row order and the selector stay consistent.

## 12. Completing one download resets the entire downloaded-list model

**Location:** `src/Backend.cpp:1122-1123`; `src/DownloadedModifierModel.cpp:60-73`.
**Severity:** P3. **Implemented:** ✅ Yes - 2026-09-06.

The completion path appends one item and calls `setModifiers()`, which emits a model reset. `DownloadedModifierModel::addModifier()` already supports a row insertion. Rebuilding the view for an append creates unnecessary model/delegate work and invalidates selection/model indexes, especially when the library is open during multiple download completions. The exact scroll/selection behavior has not been reproduced in a running UI.

**Fix.** The completion path inserts one row through the existing `addModifier()`, or updates one through a new `updateModifier()` emitting `dataChanged`. `setModifiers()` is reserved for wholesale loads.

**Recommendation:** After choosing the authoritative library owner in finding 4, emit insertion or per-row updates for an upsert; reserve resets for wholesale replacements. Do not optimize by adding another independent list copy.

**Regression coverage:** Completing a new download emits a row insertion rather than modelReset; replacing an existing version emits an update and retains the selected item identity.

## Additional optimization and design candidates

These are bounded follow-up opportunities, not measured performance regressions:

- **Autocomplete:** `SearchPage.qml:138-155` synchronously calls `Backend::getSuggestionItems()` on each focused text change. The latter scans multilingual strings until eight matches, or the entire mapping list on a miss (`Backend.cpp:1373-1437`). The bundled database contains 1,157 records, measured with a read-only SQL query, so a claim of severe current latency would be unsupported. Profile missed queries and input-method use first; consider a short debounce, skipping active composition, and avoiding per-keystroke synchronous debug output (`Logger.cpp:69-75`). Normalized mapping strings are already precomputed.
- **Startup duplication:** `main.cpp:101` loads GameMappingManager's mappings, then Backend loads and normalizes another database snapshot (`Backend.cpp:1305-1334`). Share an immutable record snapshot while preserving the intentionally different exact-translation and substring-suggestion semantics. No startup duration or memory saving was measured.
- **Unused history path:** `SearchManager.cpp:38-51` seeds history from all Chinese names, performs repeated linear `contains()` checks, and saves it synchronously; the normal 20-item cap is only applied on later user searches. `getSearchHistory()` has no caller in the inspected application or tests. Remove this unused initialization if history is not a product feature, or cap and consume it deliberately.
- **Task progress:** A 200 ms timer already throttles updates, and the popup uses an integer model to retain delegates. However, `Backend::downloadTasks()` still assembles every task map and Main recomputes the active count on each notification. Profile long task histories before replacing this with a role-based task model and per-row dataChanged notifications. Do not claim that every progress update recreates the delegates.
- **Obsolete cover work:** The stale-cover guard filters completed results, but does not cancel obsolete downloads or prevent their queued decoding/inference. Shared inference is serialized by a mutex. Repeated navigation before uncached covers finish can waste work and delay the current cover; consider a bounded queue and checks before expensive work while retaining detector synchronization. No inference latency was measured here.
- **Reusable visual pieces:** The two loading spinners (`SearchPage.qml:487-513`, `DetailDrawer.qml:62-89`), Main's tab visuals, inline row actions, and the app/database update-card structures repeat behavior and styling. Extract small components where shared behavior prevents drift; do not turn SettingsDialog into an overly generic schema renderer. Write-only selection fields (`m_selectedIndex`, `m_selectedVersionIndex`), `settingsApplied`, unused icon-color properties, and uncalled sort helpers are candidates for removal after checking public consumers.
- **Layout validation:** At the 800-pixel minimum window width, the drawer is only 320 pixels wide. Its fixed cover and unbounded metadata RowLayouts (`DetailDrawer.qml:138-243`) leave little room for long version strings or translated labels. Validate 800x600, 950x650, long real titles/versions, and Chinese/English/Japanese; allow metadata wrapping and provide a way to inspect elided titles/version choices. The whole drawer has no scrolling fallback; only the options group scrolls. These layout risks need live screenshots before prescribing exact sizes.

## Verified correct - do not "fix" these

- Search-generation checks already prevent older whole-list responses from replacing a newer search (`Backend.cpp:1217-1219`). Preserve that logic when adding progressive updates.
- Cover image decoding and inference already run through QtConcurrent; QImage crosses the worker boundary and QPixmap conversion occurs on the GUI side (`CoverExtractor.cpp:90-106`). This is not a synchronous-inference-on-the-GUI defect.
- The shared detector is initialized once and inference is protected by a mutex (`CoverExtractor.cpp:29-55`, `CoverExtractor.cpp:169-176`). Removing that lock is not a safe performance optimization.
- The cover completion guard remains useful. Finding 1 identifies an earlier, separate detail-reply race; it does not negate that guard's narrower protection.
- The two main tables already use ListView, not a Repeater that constructs every row (`StyledTable.qml:140`).
- Download progress notifications are already throttled and the popup retains rows at a stable count (`Backend.cpp:204-212`, `DownloadListPopup.qml:98-112`).
- The settings About content already scrolls and reserves room for its scrollbar (`SettingsDialog.qml:529-555`).
- Earlier network/download review findings are not relisted as new defects. This review does not certify all of their regression coverage.

## Verification and remaining coverage

- Source/caller inspection and the historical screenshot inspection completed.
- Read-only SQLite query completed: 1,157 game records, 1,136 distinct Chinese-name values. These counts describe the reviewed bundled file only.
- Source-palette contrast calculations completed; selected examples appear in finding 6.
- Windows **`build.cmd tests` passed** after loading the Visual Studio developer environment and prepending `D:\Qt\6.10.0\msvc2022_64\bin` to the process PATH. CTest passed its one aggregate target; `build/ninja-release/Testing/Temporary/LastTest.log` confirms **29 GoogleTest cases from 9 suites passed**, with an aggregate test duration of 0.86 seconds. The test target was compiled from the current production C++ sources; the production GUI executable/QML module was not rebuilt or launched by this test command.
- Earlier attempts failed before tests ran: sandboxed WSL interoperability (`UtilBindVsockAnyPort: socket failed 1`), missing MSVC standard-library include paths in a plain command prompt, and Windows-path quoting when launching the developer environment. The successful retry used the short Visual Studio path. The build regenerated ignored dependency/build artifacts; no dependency manifest or tracked third-party source was changed. CMake still reported the QTP0004 and unused-toolchain-variable warnings.
- Document local links, referenced line bounds, finding numbering, and trailing whitespace were checked. `git diff --check` passed; separate checks covered the new, untracked documents.
- No live UI interaction, QML lint, Qt Quick regression tests, or performance benchmarks were run. Existing Backend unit tests cover suggestion localization; the listed selection/state/UI regressions are not covered by those tests. Passing the existing C++ tests does not establish that those findings are fixed.
- **AI assistance:** Source analysis and this report were generated by Codex. Claims were checked against the inspected source, the explicitly reported calculations, and linked primary documentation; unexecuted interaction tests are proposed coverage, not passing results.
- **AI assistance (implementation):** The 2026-09-06 fixes below, their unit tests, and the implementation record were produced by Claude Code. Each finding was re-verified against the source before being changed; none of the resulting code was compiled or run by it.

## Implementation record - 2026-09-06

Ten findings were implemented on branch `fix/review-findings-2026-09-04`, one commit
each; an eleventh (finding 5) was implemented and then reverted in the same series. Full detail in the [work log](../work-logs/2026-09-06-review-findings-2026-09-04.md).

| Finding | Main files touched |
|---------|--------------------|
| 1, 2 | `src/Backend.{h,cpp}`, `src/ModifierManager.{h,cpp}`, `qml/components/DetailDrawer.qml`, `qml/Main.qml` |
| 4, 12 | `src/Backend.cpp`, `src/ModifierManager.cpp`, `src/DownloadedModifierModel.{h,cpp}` |
| 6 | `qml/themes/ThemeProvider.qml` and its readable-text call sites |
| 7 | `qml/pages/SearchPage.qml` |
| 8 | `qml/components/DownloadListPopup.qml` |
| 9 | `src/Backend.{h,cpp}`, `qml/pages/DownloadedPage.qml`, `qml/Main.qml` |
| 10, 11 | `src/Backend.{h,cpp}`, `qml/Main.qml` |

Two claims in this review were checked and turned out to overstate the work needed:
`getModifierDetail()` has exactly one caller, so its contract could change freely; and
ModifierManager's INI library is never read by the app, so finding 4 needed the redundant
write removed rather than the re-architecture and INI/JSON migration recommended above.

**Verification of the fixes:** not built and not tested by the author of the changes -
`build.cmd` needs Windows and WSL interop could not launch `cmd.exe` in that environment.
New unit tests cover findings 1, 2 and 12 only; findings 4, 9, 10 and 11 and every QML
change are unverified by automated tests - finding 5 is exactly what that gap cost, and the repository has no Qt Quick test harness.
The build and the manual pass are the maintainer's.

