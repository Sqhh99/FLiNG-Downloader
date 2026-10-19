# Work log: English-first README with a clearer layout

- **Date:** 2026-10-19
- **Branch:** `docs/readme-english` (from `origin/main` at `f5dd82b`). This is separate from `fix/download-integrity` (PR #52), so each PR does one thing.
- **Related:** [PR record](../pull-requests/2026-10-19-readme-english.md). There is no code review.

## 1. Request
> Please help me improve the formatting and description of the README, and ensure that the primary language used in the README is English.

Make the root README English, and improve its structure and wording.

## 2. Plan
1. Read the three READMEs (root Chinese, `docs/README.en.md`, `docs/README.ja.md`) and every file that links to them.
2. Check each claim against the source, so the new text describes the app as it is:
   - release asset names and checksums (`make-release.yml`);
   - UI labels in all three languages (`app.yml`);
   - AppData paths (`fling-config/src/paths.rs`);
   - empty-search behavior (`search_page.rs`: an empty box sends `FetchRecent`);
   - test directories, update sources and issue templates.
3. Make the root `README.md` English and restructure it. Move the Chinese text to `docs/README.zh-CN.md`, and rewrite `docs/README.ja.md` to the same structure.
4. Keep `docs/README.en.md` as a pointer, so existing links keep working.
5. Fix the links in the docs index.
6. Check every relative link and anchor with a script, and render the three READMEs with GitHub's Markdown API.

### What changed in the README
- **Header:** a centered logo, title and one-line description, plus badges for the latest release, total downloads, license and platform. The language switcher is now English · 简体中文 · 日本語.
- **Opening:** a short paragraph saying what the app is. The "not affiliated" disclaimer moved up as a GitHub `[!NOTE]` alert, and a table of contents was added.
- **User sections first:**
  - **Features**, grouped under bold leads. New details that were checked in the code: partial CJK search, option hotkeys, three concurrent downloads with resume, extension correction, and the library actions.
  - **Download and install:**
    - The real asset names, replacing "`setup.exe`".
    - A PowerShell checksum example, replacing "(shipped starting with new releases)", which is now always true.
    - An antivirus `[!TIP]`.
    - A note on upgrading from the Qt version.
  - **Using the app**, in four steps, plus where to report a missing game title (the `game-title.yml` issue form).
  - **Privacy and security**, with the actual storage paths.
- **Developer sections:** **Building from source**, with clone, commands, and the correct `--bench detector`. Its project structure, tests, translation database and CI/release subsections are now collapsible `<details>` blocks, so they no longer push the user sections down.
- **Links:**
  - Links now use absolute GitHub URLs. The old `../../releases`-style links resolved wrongly from the files under `docs/`.
  - Links to Chinese-only documents are marked "(in Chinese)": the antivirus FAQ, SECURITY.md and CONTRIBUTING.md.
- **Corrected claims:**
  - The integration-test list now includes `crates/fling-cover/tests`.
  - The bench command is now `cargo bench -p fling-cover --bench detector`.
  - Version examples use `1.2.3` throughout.

## 3. Files changed
| File | Change |
|------|--------|
| `README.md` | Rewritten in English with the structure above. It was the Chinese README. |
| `docs/README.zh-CN.md` | New. The Simplified Chinese README, a translation of the new English one, using the app's zh-CN UI labels. |
| `docs/README.ja.md` | Rewritten to the same structure, using the app's ja UI labels. |
| `docs/README.en.md` | Now a pointer to the root README. Kept so existing links to it still work. |
| `docs/README.md` | The docs index. Its links point to the new README files. The download row's anchor `#快速开始` no longer existed, and the antivirus-FAQ anchor was already stale; both were fixed. "Qt" in the third-party line became "GPUI". |
| `docs/work-logs/2026-10-19-readme-english.md` | This log. |

## 4. Verification
- **Links:** a Python script checked every relative link, `src` and anchor in the five Markdown files. Everything resolved after one fix: the stale antivirus-FAQ anchor in `docs/README.md`.
- **Rendering:** `gh api markdown` (GFM, repository context) rendered `README.md`, `docs/README.zh-CN.md` and `docs/README.ja.md`. Each produced its 2 tables, the NOTE and TIP alerts, and 4 `<details>` blocks.
  - I did not view the rendered pages in a browser, so badges and layout were not checked visually.
- **Root README language:** it is English throughout. The only Chinese or Japanese text is the language switcher and two examples (生化危机 / バイオハザード, 艾尔登法环 (Elden Ring)), which show the feature itself.
- No code changed, so cargo was not run.

## 5. Open items
- ~~`docs/ANTIVIRUS_FAQ.md`, `SECURITY.md`, `CONTRIBUTING.md` and the docs index `docs/README.md` are still Chinese only. They are linked with "(in Chinese)". Translating them was not part of this request.~~ The first three were translated in the follow-up below. The docs index is still Chinese.
- The README screenshot (`resources/interface.png`) was not changed.
- ~~Nothing is committed yet.~~ Committed and opened as a PR in the follow-up below.

---

# Follow-up: English antivirus FAQ, security policy and contributing guide

## 1. Request
> The other three documents were also translated, and a new PR was opened.

This answers the question "should I translate those three documents too?", together with the earlier offer to commit and open a PR. I checked the working tree first: all three files were still in Chinese, and the only open PR was #52. So I read the message as: translate the antivirus FAQ, SECURITY.md and CONTRIBUTING.md too, then open a new PR for this branch.

## 2. Plan
1. Make the English text the file at the **original path**, so every existing link keeps working:
   - GitHub's security-policy and contributing tabs;
   - `.github/ISSUE_TEMPLATE/config.yml` and `bug.yml`;
   - the FAQ link written into every release description by `make-release.yml`;
   - the PR template's CONTRIBUTING checkbox.
2. Keep the Chinese originals as `docs/*.zh-CN.md`, the same pattern as `docs/README.zh-CN.md`. Give every file an English · 简体中文 switcher.
3. Correct, in both languages, the places where the documents disagreed with the repository.
4. Point each README and the docs index at the copy in its own language, remove the "(in Chinese)" labels, and recheck every link.

### Corrections made while translating
- **FAQ, verification step:** "`SHA256SUMS.txt` is published starting with new releases" was dropped. Every release attaches it now.
- **FAQ, network use:** "only to search and download" now also lists update checks, as SECURITY.md already did.
- **FAQ, help links:** "General questions → Issues" now sends questions to Discussions and bugs to Issues, matching `.github/ISSUE_TEMPLATE/config.yml`.
- **FAQ, date:** "Last updated" moved from 2026-03-01 to 2026-10-19.
- **SECURITY, version:** "the version on the `关于` page" now says **Settings → About**, the label the app actually shows.
- **CONTRIBUTING:** no content changes. Its "common commands" link points to the README's *Building from source* section.

## 3. Files changed
| File | Change |
|------|--------|
| `docs/ANTIVIRUS_FAQ.md` | Now English, with the corrections above. |
| `docs/ANTIVIRUS_FAQ.zh-CN.md` | The Chinese original (`git mv`), with a language switcher and the same corrections. |
| `SECURITY.md` | Now English. |
| `docs/SECURITY.zh-CN.md` | The Chinese original, with a switcher and the Settings → About wording. |
| `CONTRIBUTING.md` | Now English. |
| `docs/CONTRIBUTING.zh-CN.md` | The Chinese original, with a switcher. Its relative links were fixed for the `docs/` location, and its README link points to the Chinese README. |
| `README.md` | Removed the three "(in Chinese)" labels. |
| `docs/README.zh-CN.md` | Links to the Chinese FAQ, security policy and contributing guide. |
| `docs/README.ja.md` | The three labels now say （英語）, not （中国語）. |
| `docs/README.md` | The Chinese docs index links to the `.zh-CN.md` copies, and its anchors still resolve. The CRLF line endings were kept. |
| `docs/pull-requests/2026-10-19-readme-english.md` | The PR record. |
| `docs/work-logs/2026-10-19-readme-english.md` | This follow-up. |

## 4. Verification
- **Links:** the link-and-anchor script was extended to all 11 Markdown files involved and found 0 problems.
- **Language:** a CJK search found no Chinese in `README.md`, `SECURITY.md`, `CONTRIBUTING.md` or `docs/ANTIVIRUS_FAQ.md`, apart from the 简体中文 switcher link and the README's feature examples.
- **Not checked:** the rendered pages were not viewed in a browser, and the GitHub security and contributing tabs were not checked after the push.

## 5. Open items
- `docs/README.md`, the docs index, is still Chinese only.
- There is no Japanese FAQ, security policy or contributing guide. The Japanese README links to the English ones and labels them （英語）.
