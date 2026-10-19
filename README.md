<div align="center">
  <img src="https://github.com/user-attachments/assets/e8aceb6b-2534-4aaf-a757-020b654aa285" alt="FLiNG Downloader logo" width="120">

  <h1>FLiNG Downloader</h1>

  <p>
    Find, download and manage <a href="https://flingtrainer.com/">FLiNG</a> game trainers on Windows,<br>
    searching by game title in English, Chinese or Japanese.
  </p>

  <p>
    <a href="https://github.com/Sqhh99/FLiNG-Downloader/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/Sqhh99/FLiNG-Downloader"></a>
    <a href="https://github.com/Sqhh99/FLiNG-Downloader/releases"><img alt="Total downloads" src="https://img.shields.io/github/downloads/Sqhh99/FLiNG-Downloader/total"></a>
    <a href="LICENSE"><img alt="License: AGPL-3.0" src="https://img.shields.io/badge/license-AGPL--3.0-blue"></a>
    <img alt="Platform: Windows 10+ x64" src="https://img.shields.io/badge/platform-Windows%2010%2B%20x64-0078D6">
  </p>

  <p><b>English</b> · <a href="docs/README.zh-CN.md">简体中文</a> · <a href="docs/README.ja.md">日本語</a></p>
</div>

FLiNG Downloader is an open-source desktop client for flingtrainer.com. Search for a game, review its trainer's options and supported versions, and download it in one click. The app keeps track of everything you have downloaded. It is a native Rust app with a GPU-rendered interface, and it needs no runtime to be installed.

> [!NOTE]
> This is an independent project, **not affiliated with** FLiNG or flingtrainer.com. It does not host or redistribute trainers: it searches the public site and downloads only what you ask for. Trainers may break a game's terms of service, so use them at your own risk.

![FLiNG Downloader showing search results and a trainer's details](resources/interface.png)

## Contents

- [Features](#features)
- [Download and install](#download-and-install)
- [Using the app](#using-the-app)
- [Privacy and security](#privacy-and-security)
- [Building from source](#building-from-source)
- [Getting help and contributing](#getting-help-and-contributing)
- [License](#license)

## Features

- **Search in your language.** Type a game title in English, Chinese or Japanese. A bundled title database maps Chinese and Japanese titles to the English titles flingtrainer.com uses. Partial titles work too: 生化危机 or バイオハザード finds the Resident Evil trainers. Suggestions appear as you type.
- **Trainer details at a glance.** See every option with its hotkey, the supported game versions, the last update date and all downloadable versions. A local ONNX model crops the game's cover from the trainer screenshot.
- **Download manager.** Run up to three downloads at once, then pause, resume, retry or cancel them. Interrupted downloads continue where they stopped. Each file gets the right extension (`.zip`, `.exe`, `.rar`, …) based on its contents.
- **Library.** The *Downloaded Mods* tab lists every trainer you have downloaded. Open one, show it in its folder or delete it from there.
- **Localized trainer names.** Show trainer names in Chinese or Japanese, for example "艾尔登法环 (Elden Ring)". New downloads are saved under the same name.
- **Your interface, your way.** Switch the interface between English, Simplified Chinese and Japanese instantly. Pick from nine themes: Light, Dark, Ocean, Sunset, Forest, Lavender, Rose, Midnight and Mocha.
- **Updates built in.** Check for new app versions and title-database updates from GitHub or Gitee, then install them from Settings.

## Download and install

Download the latest version from the [Releases page](https://github.com/Sqhh99/FLiNG-Downloader/releases/latest):

| File | What it is |
|------|------------|
| `FLiNG-Downloader-v<version>-win-x64-setup.exe` | **Installer.** Installs over an older version and keeps your settings, downloaded trainers and covers. |
| `FLiNG-Downloader-v<version>-win-x64-portable.zip` | **Portable.** Unzip it anywhere and run `FLiNG Downloader.exe`. |
| `SHA256SUMS.txt` | SHA-256 checksums of both files. |

**Requirements:** Windows 10 version 1903 or later, 64-bit. Nothing else needs to be installed: ONNX Runtime is built into the executable, and the packages include the MSVC runtime.

**Verify your download** against `SHA256SUMS.txt`, for example in PowerShell:

```powershell
Get-FileHash .\FLiNG-Downloader-v<version>-win-x64-setup.exe -Algorithm SHA256
```

> [!TIP]
> Windows Defender or another antivirus may flag the app by mistake (for example as `Win32/Wacapew.C!ml`). The [antivirus FAQ](docs/ANTIVIRUS_FAQ.md) explains why and what to do. Download the app only from this repository's Releases page.

**Upgrading from 1.1.x or earlier** (the Qt version): the app reads the same files, so your settings, library, cover cache and title database carry over.

## Using the app

1. **Search.** Type a game title and press Enter, or pick a suggestion. To see recently updated trainers, click **Show All**, or press Enter with the box empty.
2. **Choose a trainer.** Click a result to open its details: options, supported versions and cover.
3. **Download.** Choose a version and click **Download**. Progress appears in the **Download List** in the title bar.
4. **Run it.** Open the trainer from the **Downloaded Mods** tab.

Settings lets you set the download directory (your Downloads folder by default), the interface and trainer-name languages, the theme and the update source.

**A game can't be found, or its name is translated wrongly?** First update the title database: **Settings → About → Translation Database Updates → Check Database Updates**. If the problem remains, report it with the [*Game title not found* form](https://github.com/Sqhh99/FLiNG-Downloader/issues/new?template=game-title.yml).

## Privacy and security

- The app collects no personal data and has no telemetry. It connects to the network only to search flingtrainer.com, download trainers and covers, and check GitHub or Gitee for updates.
- Everything is stored on your computer:
  - settings: `%APPDATA%\FLiNG Downloader\config`;
  - library and downloaded title database: `%APPDATA%\FLiNG Downloader\data`;
  - cover cache: `%LOCALAPPDATA%\FLiNG Downloader\cache`.
- To report a vulnerability, see [SECURITY.md](SECURITY.md). Please don't open a public issue for it.

## Building from source

You need stable Rust (`x86_64-pc-windows-msvc`) and the Visual Studio 2022 (or later) C++ build tools. Qt, CMake and vcpkg are not needed. ONNX Runtime is downloaded and linked statically on the first build.

```bat
git clone --recurse-submodules https://github.com/Sqhh99/FLiNG-Downloader.git
cd FLiNG-Downloader

cargo run -p fling-ui                   :: build and run (debug)
cargo test --workspace                  :: all tests
cargo clippy --workspace --all-targets -- -D warnings
cargo fmt --all
cargo bench -p fling-cover --bench detector   :: cover-detection benchmark
cargo xtask dist [--version 1.2.3]      :: release folder in dist\FLiNG Downloader\
cargo xtask notices                     :: regenerate the crate list in THIRD_PARTY_NOTICES.md
```

The first build of `fling-ui` takes several minutes.

<details>
<summary><b>Project structure</b></summary>

The project is a Cargo workspace with the interface and the backend kept apart. The UI talks to the backend only through `Command`s and `Event`s.

| Crate | Responsibility |
|-------|----------------|
| `fling-core` | Domain types and pure helpers: version comparison, title normalization, file-type detection |
| `fling-net` | The HTTP abstraction and the reqwest client (resume, Referer, timeouts), plus a fake for tests |
| `fling-config` | AppData paths and the QSettings-compatible `settings.ini` |
| `fling-mapping` | The translation database, Chinese/Japanese → English title mapping, search suggestions |
| `fling-site` | flingtrainer.com page parsing, search and the recently updated list |
| `fling-download` | The download queue (pause, resume, three at once) and the library |
| `fling-update` | App and database updates from GitHub or Gitee |
| `fling-cover` | The cover cache and ONNX cover detection |
| `fling-app` | The backend facade: owns every service and exposes `Command` / `Event` |
| `fling-ui` | The GPUI interface (`FLiNG Downloader.exe`) |
| `xtask` | Packaging and repository tasks |

</details>

<details>
<summary><b>Tests</b></summary>

- Every crate has unit tests. Integration tests live in `crates/fling-app/tests`, `crates/fling-site/tests`, `crates/fling-mapping/tests` and `crates/fling-cover/tests`.
- Tests never touch the live network or your real AppData: they use a fake HTTP client and temporary directories.
- `crates/fling-site/tests/fixtures/` holds saved site pages, and `crates/fling-cover/tests/screenshots/` holds cover-detection samples.

</details>

<details>
<summary><b>Translation database</b></summary>

- The app reads game titles from a SQLite database, `fling_translations.db`. A copy ships in the app's `resources/` folder.
- Database updates are saved to `%APPDATA%\FLiNG Downloader\data`. The app compares that copy with the bundled one and uses the newer valid one.
- The database is built and released by [game-mappings-updater](https://github.com/Sqhh99/game-mappings-updater), included as a submodule at `tools/game-mappings-updater` (fetch it with `git submodule update --init`).
- A valid database has these columns:
  - `metadata.release_tag`
  - `metadata.schema_version` (optional; must be `1` when present)
  - `games.english`, `games.normalized_english`, `games.chinese_simplified`, `games.japanese`

</details>

<details>
<summary><b>CI and releases</b></summary>

- `build.yml` checks formatting, runs clippy and the tests, and builds the packages.
- `make-release.yml` builds the installer and the portable zip. It attaches them to the GitHub Release with `fling_translations.db` and `SHA256SUMS.txt`.
- Both packages contain the app, the cover model (`models/`), the bundled database (`resources/`) and the MSVC runtime.
- Pushing a tag that starts with `v` triggers `make-release.yml`. A tag without `-` makes a stable release; a tag with `-` makes a GitHub pre-release:

  ```bash
  git tag v1.2.3 && git push origin v1.2.3                 # stable
  git tag v1.2.3-beta.1 && git push origin v1.2.3-beta.1   # pre-release
  ```

</details>

## Getting help and contributing

- **Questions and ideas:** [Discussions](https://github.com/Sqhh99/FLiNG-Downloader/discussions)
- **Bugs and agreed features:** [Issues](https://github.com/Sqhh99/FLiNG-Downloader/issues). For a new feature, start a discussion first.
- **Contributing code:** read [CONTRIBUTING.md](CONTRIBUTING.md) first.
- **Security vulnerabilities:** report privately through a [Security Advisory](https://github.com/Sqhh99/FLiNG-Downloader/security/advisories/new); see [SECURITY.md](SECURITY.md).

## License

FLiNG Downloader is licensed under the [GNU AGPL v3](LICENSE). Third-party components and their licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
