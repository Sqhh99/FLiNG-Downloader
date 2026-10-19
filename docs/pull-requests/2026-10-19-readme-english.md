# PR: English README, antivirus FAQ, security policy and contributing guide

- **Date:** 2026-10-19 / **Branch:** `docs/readme-english` → `main` / **Base commit:** `f5dd82b` / **Related records:** [work log](../work-logs/2026-10-19-readme-english.md)

## Related
There is no issue for this. The maintainer asked to "improve the formatting and description of the README, and ensure that the primary language used in the README is English", and then asked for the antivirus FAQ, SECURITY.md and CONTRIBUTING.md to be translated as well.

## What changed
**The README is now English-first and easier to scan**
- **Header:** a centered logo, a one-line description, badges (latest release, downloads, license, platform) and an English · 简体中文 · 日本語 switcher.
- **Top of the page:** the "not affiliated with FLiNG" disclaimer is a note at the top, followed by a table of contents.
- **User sections first:**
  - Features, grouped and checked against the code.
  - Download and install, with the real asset names, a PowerShell checksum example, the antivirus tip, and a note that 1.1.x (Qt) users keep their data.
  - A four-step *Using the app* guide, with where to report a missing game title.
  - Privacy and security, with the actual storage paths.
- **Developer sections:** *Building from source* follows. Project structure, tests, the translation database and CI/releases are collapsible sections.
- **Fixes:**
  - The links under `docs/` used `../../releases`-style paths that resolved wrongly; they are now absolute.
  - The bench command now has `--bench detector`.
  - The test-directory list now includes `fling-cover/tests`.
  - The asset names are real; the README used to say "`setup.exe`".

**Other languages**
- The Chinese README moved to `docs/README.zh-CN.md`, and `docs/README.ja.md` was rewritten to the same structure. Both use the app's own UI labels in their language.
- `docs/README.en.md` now points to the root README, so old links still work.

**The antivirus FAQ, security policy and contributing guide are English**
- `docs/ANTIVIRUS_FAQ.md`, `SECURITY.md` and `CONTRIBUTING.md` keep their paths. Every existing link keeps working: the GitHub security and contributing tabs, the issue templates, the PR template, and the FAQ link in each release description.
- The Chinese originals are kept as `docs/ANTIVIRUS_FAQ.zh-CN.md`, `docs/SECURITY.zh-CN.md` and `docs/CONTRIBUTING.zh-CN.md`. Every file has an English · 简体中文 switcher.
- **Corrections, in both languages:**
  - The FAQ no longer says checksums start "with new releases".
  - Its network-use line now includes update checks.
  - It sends questions to Discussions and bugs to Issues, as the issue templates do.
  - SECURITY.md names **Settings → About** as the place to read the version.
- Each README and the Chinese docs index link to the copy in their own language.

## Type of change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [x] Docs, CI, or build only

## How it was tested
- [ ] `cargo test --workspace` passes
- [ ] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

**Unticked:** no code changed, so cargo was not run. The README's descriptions of the app were checked against the source instead: release workflow asset names, `app.yml` labels in all three languages, `AppPaths`, and the empty-search behavior in `search_page.rs`.

**What was checked**
- **Links:** a script checked every relative link, `src` and anchor in the 11 Markdown files involved, and found 0 problems. It also found one anchor in `docs/README.md` that was already broken; it is fixed.
- **Rendering:** `gh api markdown` (GFM, repository context) rendered the three READMEs. Each produced its tables, its NOTE and TIP alerts, and four `<details>` blocks.
- **Language:** the English files contain no Chinese except the 简体中文 switcher link and the README's two feature examples (生化危机 / バイオハザード, 艾尔登法环 (Elden Ring)).

**Not checked:** the pages were not viewed in a browser, so badges and layout were not checked visually. The GitHub security and contributing tabs should be glanced at after merge.

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI changes include before-and-after screenshots
- [ ] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [ ] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

- **Unticked:**
  - Screenshots, strings, database and packaging: not applicable. This changes only Markdown, and the README screenshot is unchanged.
  - CONTRIBUTING.md: left for the maintainer.
- **AI disclosure:** yes. All text, including the Chinese and Japanese versions and the translations, was written with Claude Code. Facts were checked against the source, and links were checked by script. A native-speaker read of the Chinese and Japanese text is still worthwhile.

**Open items**
- `docs/README.md`, the docs index, is still Chinese only.
- There is no Japanese FAQ, security policy or contributing guide. The Japanese README links to the English ones and labels them （英語）.
