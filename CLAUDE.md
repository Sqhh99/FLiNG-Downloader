# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

FLiNG Downloader is a Windows-only Qt 6 / QML desktop app (C++17) that searches
flingtrainer.com, downloads game trainers, and manages them locally. Chinese and
Japanese game titles are resolved to the site's canonical English titles through a
bundled SQLite translation database.

See [AGENTS.md](AGENTS.md) for the repository map and coding conventions, and
[CONTRIBUTING.md](CONTRIBUTING.md) for human-facing contribution rules.

## Build & test

Everything goes through `build.cmd` (a Windows batch script wrapping CMake presets +
Ninja). It requires Visual Studio 2022, CMake ≥ 3.25, Ninja, Qt 6, vcpkg, `VCPKG_ROOT`,
and `CMAKE_PREFIX_PATH` when Qt is not at the hard-coded default (`D:/Qt/6.10.0/msvc2022_64`).

```bat
build.cmd                  :: Release  -> build\ninja-release\
build.cmd debug            :: Debug    -> build\ninja-debug\
build.cmd run debug        :: build + launch
build.cmd tests            :: configure -DFLING_BUILD_TESTS=ON, build, run via ctest
build.cmd benchmark --filter CoverExtractor/.*
build.cmd i18n             :: regenerate .ts + .qm
build.cmd i18n check       :: verify translations are in sync (local check, not a CI gate)
build.cmd rebuild / clean
```

**Working from WSL:** the batch script cannot run under bash. Invoke it through
`cmd.exe /c` from the Windows path, e.g.
`cmd.exe /c build.cmd tests`.
Reading/editing sources from WSL is fine; only building and running need Windows.

**Running a single test:** ctest registers one aggregate test, so filter at the GoogleTest
level instead of through ctest:

```bat
"build\ninja-release\FLiNG Downloader Tests.exe" --gtest_filter=BackendTest.*
```

The test binary lands in the *root* of the build dir (`RUNTIME_OUTPUT_DIRECTORY`), next to
the `onnxruntime.dll` and `models/` copied there by a POST_BUILD step — running it from
elsewhere breaks `CoverExtractor`, which loads both via `applicationDirPath()`.

## Architecture

### Layering

QML owns all presentation; C++ owns all logic. There is exactly one bridge object.

- `src/main.cpp` — installs `Logger`, applies the saved language, initializes
  `GameMappingManager`, constructs `Backend`, injects it into QML via
  `engine.setInitialProperties({{"backend", ...}})`, and loads `qrc:/qml/Main.qml`.
  `initialTheme` and a `Log` facade go in as context properties.
- `src/Backend.{h,cpp}` — the single `QML_ELEMENT QML_SINGLETON` bridge. Every
  `Q_PROPERTY` / `Q_INVOKABLE` QML touches lives here; it owns the two list models,
  the download-task queue, and the update-manager instances. New user-visible features
  almost always mean a new property/invokable here plus wiring into a manager.
- `qml/` — `Main.qml` (frameless window) hosts `pages/` (SearchPage, DownloadedPage) built
  from `components/`. `themes/ThemeProvider.qml` is a QML singleton holding all nine theme
  palettes; C++ `ThemeManager` only persists the selected index, it does not style anything.

The QML module is declared with `qt_add_qml_module` in the root `CMakeLists.txt` — new
`.qml` files and resources must be added to its `QML_FILES` / `RESOURCES` lists or they
won't exist at runtime.

### Managers (all singletons, `getInstance()`)

Business logic sits in independent, callback-driven singletons under `src/`; `Backend`
orchestrates them and none of them know about QML.

- `NetworkManager` — search, download, and update traffic all goes through
  `sendGetRequest` / `downloadFile` / `downloadFileWithStatus`. The one exception is
  `CoverExtractor`, which owns a private `QNetworkAccessManager` and calls `get()`
  directly, so it bypasses the test hooks below.
- `SearchManager` → `ModifierParser` (pugixml) — scrapes `flingtrainer.com/?s=` and the
  homepage, parses HTML into `ModifierInfo`, sorts by relevance, and back-fills missing
  option counts/game versions from detail pages.
- `GameMappingManager` / `TranslationDatabase` — CN/JA → canonical English resolution.
  `translateToEnglishForSearch()` deliberately only accepts exact and normalized-exact
  matches so broad Latin queries stay site searches instead of collapsing to one title.
- `DownloadManager` / `ModifierManager` / `FileSystem` — download queue, `.crdownload`
  temp files, resume, archive/executable detection and renaming, downloaded-list persistence.
- `AppUpdateManager` / `DatabaseUpdateManager` — GitHub or Gitee release checks (the
  configured source is used *strictly*, with no cross-fallback) plus installer/DB download.
- `ConfigManager` — `QSettings`-backed; theme, language, download path, update prefs.
- `CoverExtractor` — downloads the trainer screenshot and crops the game cover with a
  YOLO ONNX model via OpenCV + header-only YOLOs-CPP; runs off the GUI thread and caches
  results on disk. The staleness guard for switching modifiers mid-flight is
  `Backend::m_coverRequestId`, not part of `CoverExtractor` — new callers do not inherit it.
- `Logger` — installs a Qt message handler; use the `LOG_DEBUG()/LOG_WARN()` macros in
  C++ and `Log.debug(...)` in QML rather than `qDebug()`/`console.log()`.

### Translation database

`resources/fling_translations.db` ships with the app, and updates are written to an
AppData override copy. `TranslationDatabase::resolveDatabasePath()` validates both
(required: `metadata.release_tag` and the `games.english`, `games.normalized_english`,
`games.chinese_simplified`, `games.japanese` columns; `metadata.schema_version` is
optional but rejected when present and not `1`) and picks the newer valid `release_tag` — an override older than the bundled copy is ignored. Changing
this schema means changing the separate `game-mappings-updater` release repo too.

### Packaging

Two executables: `FLiNG Downloader.exe` (the Qt app) and `FLiNG Launcher.exe`, a tiny
`/MT`, Qt-free shim that starts `app\FLiNG Downloader.exe` — release layouts put the real
app in an `app\` subdirectory behind the launcher.

## Testing

`tests/` is compiled from the same `TESTABLE_PROJECT_SOURCES` as the app (everything
except `main.cpp`), so tests link the production singletons directly. New test files must
be added to `TEST_SOURCES` in `tests/CMakeLists.txt`.

Never hit the live network or the user's real settings from a test. `tests/fixtures/test_support.h`
provides the seams — note they cover `NetworkManager` only, so a `CoverExtractor` test would
still reach the real network through its private manager:

- `ScopedNetworkHooks` — installs `NetworkManager::setGetRequestHandlerForTesting` /
  `setDownloadRequestHandlerForTesting` and resets them on destruction.
- `ScopedConfigState` — snapshots and restores `ConfigManager` state.
- `createTranslationDatabase(...)` — builds throwaway SQLite fixtures, including a
  deliberately malformed variant for schema-validation tests.

`tests/unit/` holds isolated behavior, `tests/integration/` the network/database
workflows, `tests/performance/` Google Benchmark cases (built separately with
`-DFLING_BUILD_BENCHMARKS=ON`; only `CoverExtractor` is covered, over the sample images
in `tests/resources/fling_trainer_screenshot/`).

## Mandatory records

Three documentation steps are mandatory, not optional extras. Each writes a
`YYYY-MM-DD-<topic>.md` file under its own `docs/` subdirectory:

| Trigger | Record | Language |
|---------|--------|----------|
| Any request that changes files | `docs/work-logs/` — request (quoted), plan, every file touched and why, verification, what was left open | English |
| Any code review | `docs/code-reviews/` — commit reviewed, scope *and* what was not covered, findings table whose last column says whether each finding is implemented | English |
| Before opening a pull request | `docs/pull-requests/` — written first, then used as the PR body, following `.github/pull_request_template.md` (关联 / 改了什么 / 怎么验证 / 检查项) | English, with the template's headings and checklist kept verbatim |

Records written before 2026-10-02 are in Chinese. Leave them as they are.

**Read [`agents/skills/project-records/SKILL.md`](agents/skills/project-records/SKILL.md)
before writing any of the three** — it holds the templates, the cross-linking rules, and
how to add a fourth record type. The table above is the trigger list; the skill is the
how.

Never tick a verification box for a command you did not run — say so and leave it
unchecked. `CONTRIBUTING.md` requires the AI-assistance disclosure to be filled in
honestly.

## Gotchas

- The app version is derived from `git describe --tags` at configure time and injected as
  `FLING_APP_VERSION`; override with `build.cmd release --app-version 1.2.3`. Pushing a
  `v*` tag triggers the release workflow (a tag containing `-` becomes a GitHub pre-release).
- vcpkg installs into `third_party/` (`VCPKG_INSTALLED_DIR`) with the
  `x64-windows-static-md` triplet — static libs, dynamic CRT, to match Qt's prebuilt DLLs.
  `third_party/YOLOs-CPP` is vendored; ONNX Runtime is fetched by `cmake/FetchONNXRuntime.cmake`.
- User-facing strings need `qsTr()` in QML / `tr()` in C++, followed by `build.cmd i18n`
  to regenerate `.ts`/`.qm`. `build.cmd i18n check` verifies they are in sync; CI
  regenerates them before building and prints the diff for information only.
- vcpkg port versions come from `vcpkg.json`'s `builtin-baseline` — do not `git pull` the
  vcpkg clone to fix a dependency problem.
- `agents/prompts/knowledge_base.md` is stale WebRTC boilerplate and does not apply here.
