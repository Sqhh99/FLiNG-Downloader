# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

FLiNG Downloader is a Windows-only Rust desktop app (GPUI / GPUI Kit frontend) that
searches flingtrainer.com, downloads game trainers, and manages them locally. Chinese and
Japanese game titles are resolved to the site's canonical English titles through a
bundled SQLite translation database. Until v1.1.x it was a Qt 6 / QML app; the Rust
rewrite keeps its data formats and on-disk paths so upgrades keep users' settings,
library, cover cache and database override.

See [AGENTS.md](AGENTS.md) for the repository map and coding conventions, and
[CONTRIBUTING.md](CONTRIBUTING.md) for human-facing contribution rules.

## Build & test

Needs stable Rust (`x86_64-pc-windows-msvc`) and the Visual Studio 2022+ C++ build tools.
ONNX Runtime is downloaded by `ort` on the first build and linked statically.

```bat
cargo run -p fling-ui                   :: debug app
cargo test --workspace                  :: all tests
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
cargo bench -p fling-cover --bench detector
cargo xtask dist [--version 1.2.3]      :: dist\FLiNG Downloader\ (exe, models, resources, MSVC runtime)
cargo xtask notices                     :: regenerate the crate table in THIRD_PARTY_NOTICES.md
```

**Working from WSL:** cargo must run on the Windows toolchain. Invoke it through `cmd.exe`,
e.g. `cmd.exe /c "cd /d D:\workspace\qt-workspace\FLiNG-Downloader && cargo test --workspace"`.
No vcvars setup is needed. Editing sources from WSL is fine. A first build of `fling-ui`
takes several minutes (GPUI); `cargo test`/`clippy` do **not** rebuild the app binary, so
rebuild (`cargo build -p fling-ui`) before handing an exe to the user.

**Single test:** `cargo test -p <crate> <name filter>`, e.g. `cargo test -p fling-site parser`.

## Architecture

### Layering

`fling-ui → fling-app → {site, download, update, cover, mapping, config} → {net, core}`.
No crate below `fling-app` knows about the UI, and `fling-ui` contains no business logic.

- `fling-app` is the only contract between frontend and backend. `start(BackendConfig)`
  returns a `BackendHandle` (`send(Command)`, synchronous `suggestions()`,
  `initial_settings()`) and an `EventReceiver`. The backend runs on its own thread with
  its own tokio runtime; one task owns all state and handles commands and finished
  background work in order, so application state needs no locks. Each `Event` carries the
  full current value of one piece of state.
- Staleness guards live in `fling-app/src/backend.rs`: search request ids, detail request
  id + URL, and the cover guard (game id + screenshot URL). New async flows need their own.
- `fling-ui`: `state::AppModel` mirrors events. Views (`src/views/`) read it and send
  commands, opening files/folders on `Event::Open`. `theme/` maps the nine Qt palettes onto
  GPUI Kit's theme (`theme::palette(cx)` for the app's own tokens). `i18n.rs` provides
  `tr!`.

### Services (plain structs, built once by `fling-app`; no singletons)

- `fling-net`: all HTTP goes through `Arc<dyn HttpClient>`. `ReqwestClient` uses a Chrome
  UA, HTTP/1.1, refuses https→http redirects, and sends an origin-only Referer on
  downloads (flingtrainer answers 403 without it). GETs have a 30 s total timeout,
  downloads a 30 s idle timeout, and resume uses Range with 200/206/416 handling.
- `fling-site`: regex ports of the Qt parsers. `parser::re` keeps PCRE's ASCII `\s`/`\d`.
  Parsing quirks are preserved deliberately; see the work log before "fixing" one.
- `fling-mapping`: `translate_for_search` only accepts exact and normalized-exact
  matches, so broad Latin queries stay site searches instead of collapsing to one title.
  `localized_title` is the reverse lookup behind the trainer-name language setting:
  `fling-app/src/names.rs` turns "Elden Ring Trainer" into `display_name` "艾尔登法环"
  over `display_subtitle` "Elden Ring" (two lines in the UI) and names new download
  files "艾尔登法环 (Elden Ring)". `ModifierInfo::name`
  stays the site's English title, because cover ids, the library JSON and relevance key
  on it.
- `fling-download`: at most 3 transfers at once; tasks sharing a temp file never run
  together. Data goes to `.crdownload`, then the file is renamed and its extension
  corrected by magic bytes. The library is `downloaded_modifiers.json`.
- `fling-update`: GitHub or Gitee is used **strictly**, with no cross-fallback. App
  asset: `FLiNG-Downloader-v{ver}-win-x64-setup.exe`; DB asset: `fling_translations.db`.
- `fling-cover`: cache in `<LocalAppData>/FLiNG Downloader/cache/covers` (`<id>.png`,
  `<id>.game-cover-v2.nocover`). `OnnxCoverDetector` reproduces the YOLOs-CPP
  letterbox/end-to-end pipeline. Behind the `onnx` feature (default on).
- `fling-config`: `AppPaths` match Qt's `QStandardPaths` layout. `settings.ini` is read and
  written by a round-tripping QSettings-compatible INI implementation that keeps unknown
  keys.

### Translation database

`resources/fling_translations.db` ships with the app; updates are written to an AppData
override copy. `TranslationDatabase` validates both (required: `metadata.release_tag` and
the `games.english`, `games.normalized_english`, `games.chinese_simplified`,
`games.japanese` columns; `metadata.schema_version` is optional but rejected when present
and not `1`) and picks the newer valid `release_tag` — an override older than the bundled
copy is ignored. The database is built and released by the separate
`game-mappings-updater` repo, checked out as a submodule at `tools/game-mappings-updater`
(Python/uv; `git submodule update --init` to fetch it). Changing this schema means changing
that repo too. To refresh the bundled copy, replace `resources/fling_translations.db` with
the asset of its latest release.

### Packaging

`cargo xtask dist` produces the portable layout: `FLiNG Downloader.exe` (the Cargo bin is
`fling-downloader`), `models/game-cover-v2.onnx`, `resources/fling_translations.db`, the
four MSVC runtime DLLs, `LICENSE` and `THIRD_PARTY_NOTICES.md`. The Inno Setup script
(`tools/FLiNG Downloader-Setup.iss`) keeps the Qt-era AppId and removes the old `app\`
folder on upgrade.

## Testing

Never hit the live network or the user's real settings from a test. The seams:

- `fling_net::fake::FakeHttpClient` (feature `test-util`): canned pages/files, request log,
  custom handlers.
- `fling_config::AppPaths::rooted(tempdir)`: every path under a temp dir.
- `fling_mapping::test_util::create_database(...)`: throwaway SQLite fixtures, including a
  deliberately malformed variant.
- `crates/fling-app/tests/backend.rs` drives the whole backend through `Command`/`Event`.

UI checks: debug builds read `FLING_DEBUG_OPEN` (`drawer[:row]`, `settings`,
`settings-about`, `settings-download`, `settings-language`, `suggest:<text>`, `downloads`) to open a screen at startup, so
layouts can be verified by screenshot without injecting input. Interactive checks are the
user's.

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

- The app version comes from `git describe --tags` in `crates/fling-ui/build.rs`
  (`X.Y.Z` or `X.Y.Z-dev.N+gHASH`). Override it with the `FLING_APP_VERSION` env var or
  `cargo xtask dist --version`. Pushing a `v*` tag triggers the release workflow (a tag
  containing `-` becomes a GitHub pre-release).
- User-facing strings need a key in `crates/fling-ui/locales/app.yml` with zh-CN, en and
  ja values (a test fails otherwise). Values starting with `%` must be quoted in YAML.
- Lucide icons must be listed in `icon_assets!` in `crates/fling-ui/src/assets.rs`; GPUI
  Kit's default set lacks some (e.g. `X`), and a missing one renders as an empty button
  (a test scans the views).
- In GPUI, later siblings paint over earlier ones. Popups that must overlap following
  content need `deferred(...)`, and buttons inside a caption drag area need `.occlude()`.
- In `fling-ui` tests, do not `use super::*`: it imports GPUI's `test` macro, which
  shadows `#[test]`.
- `agents/prompts/knowledge_base.md` is stale WebRTC boilerplate and does not apply here.
