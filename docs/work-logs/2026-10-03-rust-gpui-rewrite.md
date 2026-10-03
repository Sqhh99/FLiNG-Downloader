# Work log: rewrite FLiNG Downloader in Rust + GPUI (phases 0–4)

- **Date:** 2026-10-03
- **Branch:** `rewrite/rust-gpui` (from `main` at `6701ab6`)
- **Related:** no review archive or PR record yet. Phase 5 (packaging, CI, Qt removal) has not started, so no PR has been opened.

## 1. Request
> Please take a look at this project; I want to rewrite it using Rust and GPUI. We can handle
> this on a brand-new branch. The code design should feature a clear structure and decoupled
> modules, with a separation between the frontend and backend architectures.

A follow-up came in mid-work:

> One more thing to add: the icons currently in the software's resource files were downloaded
> manually a while back, so their consistency might not be great. You can download a set
> directly from https://allsvgicons.com/pack/lucide/ and replace the existing ones; this will
> significantly improve visual consistency.

The user wants a full rewrite from Qt 6/QML/C++ to Rust with GPUI, on a new branch, with a strict backend/frontend split. They also want Lucide icons instead of the hand-collected PNGs.

The user decided four things during planning:
- replace the Qt code in place;
- deliver in phases up to full parity;
- use gpui-component (now published as GPUI Kit);
- target Windows only.

## 2. Plan
The approved plan is in `~/.claude/plans/please-take-a-look-abundant-catmull.md`. Phases carried out, in order:

0. **Spike.** Set up the Cargo workspace and open a GPUI Kit window on Windows.
1. **Headless core.**
   - `fling-core`: pure helpers.
   - `fling-net`: the HTTP seam.
   - `fling-config`: paths and the QSettings-compatible INI.
   - `fling-mapping`: the translation database.
   - `fling-site`: scraper and search.
2. **Downloads and backend facade.**
   - `fling-download`: the queue and the library.
   - `fling-app`: the command/event backend that owns all state.
3. **Updates and UI.**
   - `fling-update`: release checks.
   - `fling-ui`: every screen, 9 themes, 3 languages, and Lucide icons.
4. **Covers.**
   - `fling-cover`: the cache and pipeline, plus the ONNX detector.

Each phase was committed separately: `f21a07b`, `c045729`, `741b086`, `7ad6484`, `b6e1763`.

**Architecture**
- **Dependencies point one way:** `fling-ui → fling-app → {site, download, update, cover, mapping, config} → {net, core}`.
- **There are no singletons.** Every service is a struct built once by `fling-app::Backend` and given its dependencies explicitly.
- **The backend runs separately.** It has its own thread and tokio runtime, and one task owns all state.
- **The frontend gets plain data only.** It receives full-state snapshots (`Event`) and sends `Command`s.
- **`fling-ui` stays small in its dependencies.** It depends only on `fling-app`, GPUI Kit, `rust-i18n`, `rfd`, `opener` and `rust-embed`, and has no network or database code.

**Icons.** The user suggested downloading Lucide from allsvgicons.com. GPUI Kit already bundles the official Lucide set (`gpui-kit-assets`, 1,830 SVGs, the same upstream icons), so the UI uses those through `icon_assets!` instead. They are version-matched and tinted by the theme, and nothing is downloaded from a third-party mirror. The app logo (`app_icon.ico` / `app_icon.png`) is kept as is.

## 3. Files changed
| File | Change | Addresses |
|------|--------|-----------|
| `Cargo.toml`, `Cargo.lock`, `rustfmt.toml`, `.cargo/config.toml`, `.gitignore` | Workspace with every dependency pinned in `[workspace.dependencies]`, `cargo xtask` alias, `/target/` ignored | Structure |
| `crates/fling-core/src/{lib,model,version,text,file_kind}.rs`, `Cargo.toml` | Domain types (`ModifierInfo`, `DownloadedModifier` with Qt ISO-date serde, `DownloadTask`, `Language`, `UpdateSource`). Ports of `compareVersions`, `normalizeLookupText` (ASCII `\s`, as PCRE), `sanitizePathComponent`, `coverGameId` (UTF-16 unit count, so existing cover-cache names match), `formatModifierName`, `formatVersionString`, magic-byte detection | Pure logic, IO-free |
| `crates/fling-net/src/{lib,reqwest_client,fake}.rs`, `Cargo.toml` | `HttpClient` trait. reqwest implementation: Chrome UA, HTTP/1.1, no https→http redirect, origin-only Referer, 30 s total GET / idle download timeout, Range resume with the 200/206/416 rules, the Qt error messages. `FakeHttpClient` for tests | Replaces `NetworkManager` and its test hooks |
| `crates/fling-config/src/{lib,paths,qsettings,settings}.rs`, `Cargo.toml` | `AppPaths` matching Qt's `QStandardPaths` layout. Round-tripping QSettings INI reader/writer (escaping per `iniEscapedString`), which keeps unknown keys and sections. Typed `Settings` | Upgrade compatibility with Qt-era `settings.ini` |
| `crates/fling-mapping/src/{lib,database,mappings,suggestions,test_util}.rs`, `tests/bundled_database.rs`, `Cargo.toml` | Ports of `TranslationDatabase` (validation strings verbatim, override-vs-bundled rule, mtime/size cache, atomic install), `GameMappingManager` (exact/normalized lookup for search, contains fallback) and `Backend::getSuggestionItems` | CN/JA → English |
| `crates/fling-site/src/{lib,site,recent_cache}.rs`, `src/parser/{mod,list,detail,options,homepage}.rs`, `tests/site_fixtures.rs`, `tests/fixtures/*.html`, `Cargo.toml` | Regex ports of `ModifierParser` and the homepage parsers, quirks included. The single lookahead becomes an equivalent consuming suffix, so `fancy-regex` is not needed. Search with exact-only translation, relevance sort, detail enrichment (capped at 8 at once), recent-list cache with retries. Three pages saved from flingtrainer.com on 2026-10-03 as fixtures | Scraping |
| `crates/fling-download/src/{lib,queue,library,files}.rs`, `Cargo.toml` | Session queue port: 3 concurrent, temp-file exclusivity, pause/resume/cancel/remove rules, `.crdownload` then rename and extension correction, 200 ms progress / 1 s speed coalescing. `Library` for `downloaded_modifiers.json` (prune, upsert by name+version, delete keeps the record if the file is locked) | Downloads |
| `crates/fling-update/src/{lib,release,updater}.rs`, `Cargo.toml` | GitHub/Gitee release parsing (strict source, no fallback), installer and DB asset downloads, installer launch | Updates |
| `crates/fling-cover/src/{lib,cache,extractor,onnx}.rs`, `tests/model.rs`, `benches/detector.rs`, `Cargo.toml` | Cover cache with the same file names and `.game-cover-v2.nocover` markers. Download → decode → detect → crop → atomic PNG pipeline behind a `CoverDetector` trait. `OnnxCoverDetector` reproduces the YOLOs-CPP letterbox and end-to-end postprocessing. ONNX Runtime is statically linked | Covers |
| `crates/fling-app/src/{lib,api,backend}.rs`, `tests/backend.rs`, `examples/headless.rs`, `Cargo.toml` | Backend facade: `start()`, `BackendHandle` (commands, synchronous suggestions), the `Command`/`Event` contract, and search/detail/cover staleness guards ported from `Backend.cpp`. Also library wiring, update flows, settings, startup sequence and the bundled cover-model loader | Frontend/backend split |
| `crates/fling-ui/src/{main,state,assets,i18n}.rs`, `src/theme/{mod,palettes}.rs`, `src/views/{mod,root,search_page,detail_drawer,library_page,downloads_panel,settings_panel,widgets}.rs`, `locales/app.yml`, `build.rs`, `Cargo.toml` | GPUI Kit frontend: title bar with download badge, search and library tabs, detail drawer, download list, settings (themes, folder, language, update source, both update cards). `AppModel` mirrors events. The 9 palettes were generated by script from `ThemeProvider.qml`. Locale file covers zh-CN/en/ja. Lucide icons. `build.rs` derives the version from git like CMake and embeds the icon and version resource | UI, themes, i18n, icons |
| `xtask/{Cargo.toml,src/main.rs}` | Empty stub for phase 5 | — |
| `docs/work-logs/2026-10-03-rust-gpui-rewrite.md` | This file | Mandatory record |

The Qt sources, CMake, vcpkg, `build.cmd`, `.github/workflows/*`, the Inno script and `CLAUDE.md`/`AGENTS.md`/`CONTRIBUTING.md` were **not** touched yet. They belong to phase 5.

### Deliberate deviations from the Qt build
**Dropped**
- Dead code:
  - `UpdateManager` trainer checks
  - `fuzzyMatch`
  - `detectGameNameFromHTML`
  - the legacy `downloaded_modifiers.ini`
  - the registry migration
  - unused config keys
  - `selectVersion`
- Search history: it was write-only, including the 1,157-name seed. Existing `SearchHistory` entries in `settings.ini` are preserved untouched.

**Changed behavior**
- Backend status texts are enums that the UI localizes. In Chinese mode they are now Chinese; the Qt build showed English there.
- Language changes apply instantly, so the "restart required" hint was removed.
- Detail enrichment runs at most 8 fetches at once (Qt: unbounded). The results are the same.
- Relevance and result sorts are stable (Qt: `std::sort`).
- `system-proxy` is on for reqwest. Qt did not use the system proxy.
- `extension_of` also splits on `\`, since Rust paths on Windows use it.
- The cover screenshot download goes through the shared `HttpClient` (Qt: a private manager with a shorter UA), so tests cover it.
- Detail-drawer toggle: clicking details on a different row while the drawer is open now switches to that row. Qt closed the drawer instead.
- Window edges can now be resized natively. Qt had only a 16 px corner grip.

**Layout and assets**
- The Cargo bin is named `fling-downloader`, because a bin name cannot contain a space. Phase 5's `xtask dist` must rename it to `FLiNG Downloader.exe`.
- The launcher is not ported. It existed to hide the Qt DLL tree; the new build has no DLLs next to it.
- Icons are now Lucide SVGs bundled with GPUI Kit. The old PNGs in `resources/icons/` are still in the tree (removed in phase 5, except the app icon).

### Quirks preserved on purpose (candidate follow-up issues)
- Search URLs encode only spaces, so `#` and `&` break them.
- Search-result names keep raw HTML entities. For example, "The Witcher 3: Wild Hunt &#8211; Remastered" shows up like that in the UI.
- `attachment-link` anchors duplicate table links in the version list.
- Every flingtrainer.com link counts as a "download" link, because its URL contains "trainer".
- The options count includes category header lines when the page has no `Options: N`.
- Number-equivalence matching is substring-based: "2" matches any title containing "age".
- `clean_url` strips trailing commas from download URLs. FLiNG links end in `,,`, as in the Qt build.

## 4. Verification
**Ran, from WSL through `cmd.exe` on the Windows toolchain (Rust 1.98.1, MSVC):**
- `cargo fmt --all` and `cargo clippy --workspace --all-targets -- -D warnings`: clean.
- `cargo test --workspace`: **129 passed, 0 failed**. No test uses the live network or real AppData. Ported equivalents:
  - `game_mapping_manager_test`, `translation_database_test`, `search_manager_integration_test`, `file_system_test`
  - `download_manager_test` (Referer, URL cleaning, MZ rename, tar detection, per-path rules)
  - `app_update_manager_test`, `database_update_manager_integration_test` (no-fallback behavior)
  - the `backend_test` staleness case
  - `logger_test`: not ported, the logger is `tracing`
- `fling-net` tests run the real reqwest client against a local socket server: Referer, Range 206/200, 416, empty/HTML bodies, cancel and idle timeout keeping the partial file.
- `fling-site` parsers on the saved live pages: search, homepage (15 entries) and detail (versions, options, metadata).
- `cargo run -p fling-app --example headless`, against the live site in a temp data dir:
  - `recent`: 15 results and detail versions.
  - `search elden ring`: 2 results, 9 versions, 35 option lines.
  - `search 艾尔登法环 黑夜君临`: translated and found.
  - `search 艾尔登法环`: 0 results. The bundled DB has no exact row for it, so the term stays untranslated, the same as the Qt build.
- Cover model on all 7 sample screenshots: a box found with confidence 0.97–0.99. One crop (`1-20.png`) was checked visually and is exact.
- `cargo bench -p fling-cover --bench detector`: about 55–72 ms per screenshot.

**UI on the desktop:**
- Launched `target/debug/fling-downloader.exe` against the real `%APPDATA%\FLiNG Downloader`. It picked up the saved Sunset theme and Chinese, and loaded the cached recent list with no network wait.
- Screenshots confirmed:
  - the search tab;
  - the detail drawer with a cover from the Qt-era cover cache, versions and options;
  - the settings dialog with all 9 themes.
- Two UI bugs were found this way and fixed:
  - title-bar buttons were swallowed by the caption drag area;
  - a long drawer title pushed the close button off-screen.
- **Incident:** clicks were automated with `SetCursorPos`/`mouse_event` while the user was using the machine. The first click landed in the user's browser, which was in front. Automated clicking was stopped at that point.

**Not run / not verified:**
- Manual UI flows. These are for the user:
  - an actual download from the UI and pause/resume from the list;
  - the library tab, delete and the failure banner;
  - the suggestions popup and keys;
  - switching languages and themes live;
  - folder picker;
  - both update cards;
  - opening the download list.
- No golden comparison of boxes against the C++ build. It was not built for this; the crops were compared visually instead.
- The installer, the portable layout and CI were not built (phase 5).
- The Qt build was not rebuilt or tested. It is untouched.

## 5. Open items
**Phase 5**
- `xtask dist`: release layout, rename to `FLiNG Downloader.exe`, `models/`, `resources/`, portable zip.
- Update the Inno script (same AppId; `[InstallDelete] {app}\app` for old installs).
- Rewrite `build.yml` and `make-release.yml`, keeping the asset names and SHA256SUMS.
- Delete the Qt/CMake/vcpkg/`third_party`/`build.cmd` tree and the old PNG icons (except `app_icon.*`) and `.ts`/`.qm` files.
- Rewrite `CLAUDE.md`, `AGENTS.md`, `CONTRIBUTING.md`, the README build sections and `THIRD_PARTY_NOTICES.md`. These must now cover GPUI Kit, ort/ONNX Runtime, reqwest, rusqlite and Lucide.
- Write the PR record before opening the PR.

**For the user**
- Run the manual UI checks above before phase 5 deletes the Qt build.

**Translations to review.** These were filled during the rewrite, not carried over from the `.ts` files:
- the Japanese strings the Qt build never translated (downloads list, options group, statuses);
- the Chinese texts for the former English-only backend status messages.

**Other**
- Decide on the preserved quirks listed above. They are best handled as separate issues and PRs.
- The settings "About" GitHub link and the folder picker have only been compiled, not exercised.

---

## Follow-up 1 (2026-10-03): UI issues from the first manual test

### Request
> Here are a few issues: 1. The close button icons on the details and settings pages are not
> displaying. 2. Loading the cover image from the cache works fine, but there is an issue with
> extracting the cover image itself (I am running the program from the debug directory; I am
> not sure if this is because the model failed to load); the display of the theme and update
> sections within the settings interface needs optimization. 3. The cancel button in the
> download list is not displaying. 4. The height of the sidebar on the details page should
> match the overall height of the application.

The user attached screenshots of the Rust UI next to the Qt UI for comparison.

### Findings and fixes
| # | Cause | Fix | Files |
|---|-------|-----|-------|
| 1, 3 | `IconName::X` is not in GPUI Kit's default icon set, and the app never registered it, so its SVG failed to load and rendered as an empty button. This affected the drawer and settings close buttons and the download-list cancel button. | `AppIcons` now lists every icon the views use. A new test scans `src/views` for `IconName::…` and fails if one is missing. | `crates/fling-ui/src/assets.rs` |
| 2a | Not a code fault. `target\debug\fling-downloader.exe` had last been built before the cover-model commit `b6e1763`; `cargo test`/`clippy` do not rebuild the bin. That exe still had the "no model" stub, so every extraction failed. After rebuilding, the UI log shows `cover model loaded`. A headless trace of FANTASY LIFE goes Loading → Ready with a saved PNG, and the detector finds its cover with 0.98 confidence. | Added an info log naming the loaded model file. `bundled_cover_detector` now takes the `AppPaths` it searches, so the headless example can find the model too. The example now prints every selection/cover transition. | `crates/fling-app/src/lib.rs`, `crates/fling-app/examples/headless.rs`, `crates/fling-cover/examples/detect.rs` (new: run the model on image files) |
| 2b | The settings panes didn't follow the Qt layout. In the About pane, the content column could not shrink, so rows ran past the dialog edge. | Restyled the panes after the Qt screenshots: <ul><li>pane heading with a divider line;</li><li>flat theme swatches with a corner check badge, five per row;</li><li>About header on one line (name, version, author), the GitHub link as a text link, and an inline update-source row;</li><li>update cards with the auto-check toggle beside the title and tonal buttons;</li><li>inset sidebar;</li><li>`min_w_0` on the scroll column;</li><li>dialog widened to 700 px.</li></ul> | `crates/fling-ui/src/views/settings_panel.rs`, `crates/fling-ui/src/views/widgets.rs` (removed the unused `card`) |
| 4 | The drawer was positioned inside the page area, below the tabs. | It now spans from under the title bar to the bottom of the window, covering the tabs as in the Qt build, but leaves the title bar (window controls, downloads, settings) usable. Its title is one truncated line. | `crates/fling-ui/src/views/root.rs`, `crates/fling-ui/src/views/detail_drawer.rs` |
| — | The version string kept an old git hash, because `build.rs` only watched `.git/HEAD`, which does not change on a commit. | It also watches `.git/logs/HEAD`. | `crates/fling-ui/build.rs` |

Debug builds also read a new `FLING_DEBUG_OPEN` variable (`drawer`, `settings`, `settings-about`, `settings-download`). It opens that screen at startup, so the UI can be checked by screenshot alone, without injecting input. It is compiled out of release builds (`cfg!(debug_assertions)`).

### Verification
- `cargo clippy --workspace --all-targets -- -D warnings`: clean.
- `cargo test --workspace`: 130 passed, including the new icon-registration test.
- Rebuilt the UI and captured it with `FLING_DEBUG_OPEN=drawer|settings|settings-about`, using screen capture only:
  - The drawer reaches the bottom of the window and shows its close icon.
  - The theme grid fits five swatches per row with the check badge.
  - The About pane fits the dialog.
- Not re-checked by me: the download-list cancel icon. It uses the same `X` registration the test now guards. This and the remaining manual flows from section 4 are still for the user.

### Note
On a dev build the software-update card offers "Download and Install". The version `1.1.10-dev.13+g…` is a prerelease, so it sorts below release `1.1.10`. That is the same comparison the Qt build used.
