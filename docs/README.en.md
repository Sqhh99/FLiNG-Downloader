<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="Logo" width="120">
</div>

# FLiNG Downloader

[简体中文](../README.md) | [English](./README.en.md) | [日本語](./README.ja.md)

---

## Features

- Native GPU-rendered UI (Rust + GPUI) with 9 themes (Light, Dark, Ocean, Sunset, Forest, Lavender, Rose, Midnight, Mocha)
- Search and suggestions backed by a local SQLite translation database
- Chinese and Japanese game titles can be remapped to the canonical English title used by FLiNG
- Trainer names can be shown in Chinese or Japanese (e.g. "艾尔登法环 (Elden Ring)"), and new downloads are named the same way
- One-click download and categorized trainer management
- Real-time download progress, pause/resume, and downloaded item management
- Built-in languages: Chinese, English, Japanese (switch instantly)
- Game covers cropped automatically from trainer screenshots by a local ONNX model
- Application update detection and installer download
- Independent translation database updates, with automatic selection between the bundled copy and the newer AppData override

## Screenshot

![Interface](../resources/interface.png)

## Requirements

- Windows 10 (1903) or later, 64-bit
- No runtimes to install: the portable zip and the installer ship everything the app needs (ONNX Runtime is built into the executable).

## Relationship to FLiNG

This is an independent open-source downloader. It is **not affiliated with** the official FLiNG Trainer project. The app does not host or redistribute trainer binaries; it only searches public sources and downloads what you ask for. Game trainers may violate a game's terms of use — that risk is yours.

## Quick Start

- Download the latest build from [Releases](../../releases)
- Portable zip: extract and run `FLiNG Downloader.exe`
- Installer: run `setup.exe`
- Verify the files against `SHA256SUMS.txt` (shipped starting with new releases)
- Windows Defender false positives: [antivirus FAQ](./ANTIVIRUS_FAQ.md)

## Development & Build (Windows)

You need stable Rust (`x86_64-pc-windows-msvc`) and the Visual Studio 2022+ C++ build tools. No Qt, CMake or vcpkg; ONNX Runtime is downloaded and statically linked on the first build.

```bat
cargo run -p fling-ui                 :: build and run (debug)
cargo test --workspace                :: all tests
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
cargo bench -p fling-cover            :: cover detection benchmark
cargo xtask dist [--version 1.2.0]    :: release folder in dist\FLiNG Downloader\
cargo xtask notices                   :: regenerate the crate list in THIRD_PARTY_NOTICES.md
```

### Project Structure

A Cargo workspace with the frontend and backend separated: the UI talks to the backend only through commands and events.

| Crate | Responsibility |
|---|---|
| `fling-core` | Domain types and pure helpers (version compare, title normalization, file-type sniffing) |
| `fling-net` | HTTP abstraction and the reqwest client (resume, Referer, timeouts), plus a fake for tests |
| `fling-config` | AppData paths and the QSettings-compatible `settings.ini` |
| `fling-mapping` | Translation database, CN/JA → English title mapping, search suggestions |
| `fling-site` | flingtrainer.com parsing, search, recently updated list |
| `fling-download` | Download queue (pause / resume / 3 concurrent) and the downloaded list |
| `fling-update` | GitHub / Gitee app and database updates |
| `fling-cover` | Cover cache and ONNX cover detection |
| `fling-app` | Backend facade: owns every service, exposes `Command` / `Event` |
| `fling-ui` | The GPUI frontend (`FLiNG Downloader.exe`) |
| `xtask` | Packaging and repository tasks |

### Testing

- Every crate has unit tests; `fling-app/tests`, `fling-site/tests` and `fling-mapping/tests` hold integration tests
- Tests never hit the live network or the real AppData (fake HTTP client, temporary directories)
- `fling-site/tests/fixtures/` holds saved site pages; `fling-cover/tests/screenshots/` holds cover-detection samples

## Translation Database

- The application uses an external SQLite database: `fling_translations.db`
- Release packages include a bundled copy under the app's `resources/` directory
- After a database update is downloaded, it is written to the AppData override location; the app compares the bundled and override versions and uses the newer valid copy
- A valid database must include:
  - `metadata.release_tag`
  - `metadata.schema_version` (optional; must be `1` when present)
  - `games.english`
  - `games.normalized_english`
  - `games.chinese_simplified`
  - `games.japanese`

## CI / Release

- `build.yml` runs format, clippy, test and packaging steps
- `make-release.yml` produces installer and portable packages and includes `fling_translations.db` plus `SHA256SUMS.txt`
- Both packages contain the app, the cover model (`models/`), the bundled database (`resources/`) and the MSVC runtime

### Version Release

- Pushing a tag that starts with `v` automatically triggers `make-release.yml` and creates a GitHub Release
- Stable release example:

```bash
git tag v1.2.0
git push origin v1.2.0
```

- Pre-release example:

```bash
git tag v1.2.0-beta.1
git push origin v1.2.0-beta.1
```

- Fixed rule:
  - tags without `-`: stable release
  - tags containing `-`: GitHub pre-release

## Security & Privacy

- Windows Defender may report a false positive (for example `Win32/Wacapew.C!ml`); this is a false alarm.
- No personal data is collected; network access is used only for search, download, and update checks; local config and database override files are stored locally only.

## License & Feedback

- License: GNU AGPL v3, see [LICENSE](../LICENSE)
- Third-party notices: [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)
- Bugs and agreed features: [Issues](../../issues)
- Questions and ideas: [Discussions](../../discussions)
- Contributing: [CONTRIBUTING.md](../CONTRIBUTING.md)
- Security: [SECURITY.md](../SECURITY.md)
