# PR: English GitHub templates, release description and interface screenshot

- **Date:** 2026-10-03 / **Branch:** `docs/english-templates-release-notes` → `main` / **Base commit:** `74c47ef` / **Related records:** [work log](../work-logs/2026-10-03-english-templates-release-notes.md)

## Related
PR #46 (English GitHub templates) was merged into `docs/records-in-english` after that branch's own PR #45 had already landed. Its commit never reached `main`, and `main` still has the Chinese PR template. This PR re-applies it on top of the Rust rewrite, adds an English release description, and updates the README screenshot.

## What changed
**For contributors**
- **PR template.** It is in English:
  - sections Related / What changed / Type of change / How it was tested / Checklist;
  - it asks for "Related to #123", never "Fixes #123";
  - its checks name `cargo test`, clippy and fmt, and the `crates/fling-ui/locales/app.yml` strings file. #46's version still named `build.cmd` and `qsTr()`.
- **Issue forms (English).**
  - **Bug report:** version, install type, Windows version, area, interface language, steps to reproduce.
  - **Feature request.**
  - **New: "Game title not found".** For a CN/JA title that finds no trainer, or a trainer name shown in English or translated wrongly.
  - **Contact links:** antivirus FAQ, Discussions, private security advisories.
- **Discussion forms** for Q&A and Ideas.
- **Placeholders** point at v1.2.0 and database v0.0.5, and the database-update path is Settings → About.

**For users**
- **Release pages.**
  - An English download table opens each page: installer, portable zip, `SHA256SUMS.txt`. It says the installer upgrades in place and keeps settings, trainers and covers.
  - Collapsible 简体中文 and 日本語 sections follow it.
  - The generated pull-request list is grouped into New features, Bug fixes, Documentation and Other changes (`.github/release.yml`).
- **The README screenshot** (`resources/interface.png`) shows the two-line translated trainer names and the detail drawer.

**Records**
- `CLAUDE.md` and the project-records skill now ask PR records to follow the English headings.
- The PR #48 record is converted to them, with its content unchanged.

## Type of change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [x] Docs, CI, or build only

## How it was tested
- [ ] `cargo test --workspace` passes
- [ ] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

The three boxes are unticked because no Rust code changed and they were not run. What was checked instead:
- **YAML.** Every YAML file under `.github/` parses with `serde_yaml`.
- **Release intro.** Its PowerShell was run in pwsh 7 with dummy artifact names, and the generated Markdown was read. Both file names are interpolated, and the three language sections render.

The release step itself only runs on the next `v*` tag. The issue and discussion forms appear on GitHub after merge. The discussion forms need the Q&A and Ideas categories enabled.

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [x] UI changes include before-and-after screenshots
- [ ] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [x] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

**Screenshot:** the only UI-facing change is the README screenshot, `resources/interface.png`, supplied by the maintainer.

**Strings:** there are no new app strings, so the box is unticked.

**Packaging:** `resources/interface.png` is not packaged; `xtask dist` does not copy it. Nothing else under `resources/` changed.

**CONTRIBUTING.md:** left for the maintainer to tick.

**AI disclosure:** yes. The re-application and conflict resolution of #46, the form updates, the release description and the records were done with Claude Code. They were verified by the YAML parse and the PowerShell dry run above. The screenshot is the maintainer's.
