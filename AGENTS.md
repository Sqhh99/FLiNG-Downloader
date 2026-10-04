# Repository Guidelines

Human-facing contribution rules live in [CONTRIBUTING.md](CONTRIBUTING.md). Keep this file limited to the project map and agent-facing conventions.

## Project Structure & Module Organization

FLiNG Downloader is a Windows desktop app written in Rust (edition 2024) with a GPUI / GPUI Kit frontend. It is a Cargo workspace under `crates/`, with dependencies pointing one way: `fling-ui → fling-app → {fling-site, fling-download, fling-update, fling-cover, fling-mapping, fling-config} → {fling-net, fling-core}`.

- `fling-core`: domain types and IO-free helpers.
- `fling-net`: the `HttpClient` trait, the reqwest client, and a fake for tests.
- `fling-config`: paths and `settings.ini`.
- `fling-mapping`: the translation database and title lookup.
- `fling-site`: flingtrainer.com parsers and search.
- `fling-download`: the download queue and the library.
- `fling-update`: release checks.
- `fling-cover`: covers and the ONNX detector.
- `fling-app`: the UI-agnostic backend facade (`Command` / `Event`).
- `fling-ui`: the GPUI views. Its strings live in `crates/fling-ui/locales/app.yml` (zh-CN / en / ja).

`xtask/` holds packaging and repository tasks. `resources/` holds the bundled translation database, the ONNX model and the app icon. `tools/` holds the Inno Setup script and the `game-mappings-updater` submodule, the Python tool that builds and releases the translation database.

## Build, Test, and Development Commands

Development requires stable Rust (`x86_64-pc-windows-msvc`) and the Visual Studio 2022+ C++ build tools.

- `cargo run -p fling-ui`: build and run the app (debug).
- `cargo test --workspace`: all unit and integration tests.
- `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all`: required to be clean.
- `cargo bench -p fling-cover --bench detector`: cover-detection benchmark.
- `cargo xtask dist [--version X.Y.Z]`: release folder in `dist/FLiNG Downloader/`.
- `cargo xtask notices`: regenerate the crate list in `THIRD_PARTY_NOTICES.md` after dependency changes.

## Coding Style & Naming Conventions

Follow `rustfmt.toml` (100 columns) and keep clippy clean. Each module starts with a `//!` comment saying what it owns. Ports of Qt-era behavior name the C++ function they replace. Business logic belongs in the backend crates, never in `fling-ui`; the UI only renders `AppModel` state and sends `Command`s. Do not add singletons; pass dependencies explicitly. User-visible strings go through `tr!` with keys in `locales/app.yml`, and every key needs all three locales (a test checks this). Icons are Lucide icons registered in `crates/fling-ui/src/assets.rs` (a test checks this too).

## Testing Guidelines

Tests never touch the live network or the user's AppData. Use `fling_net::fake::FakeHttpClient`, `fling_config::AppPaths::rooted(tempdir)` and `fling_mapping::test_util`. Use descriptive snake_case test names. Every bug fix or behavior change needs focused regression coverage. Saved site pages live in `crates/fling-site/tests/fixtures/`, and cover samples in `crates/fling-cover/tests/screenshots/`.

## Commit & Pull Request Guidelines

Follow the established Conventional Commit style: `feat: add ...`, `fix(ui): restore ...`, or `chore: ...`. Keep subjects concise and imperative. Pull requests should explain the user-visible change, identify affected crates, link relevant issues, and list verification performed. Include before/after screenshots for UI changes and call out resource, database-schema, model, or translation updates explicitly.
