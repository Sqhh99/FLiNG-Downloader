# PR: Find trainers for partial Chinese and Japanese queries

- **Date:** 2026-10-03 / **Branch:** `fix/cjk-search` → `main` / **Base commit:** `62f2e2d` / **Related records:** [work log](../work-logs/2026-10-03-cjk-search.md)

## Related
There is no issue for this. The maintainer reported it after v1.2.0: "the current search bar seems to support only English queries. Searches using Chinese or Japanese yield no matches."

**Cause.** Search translated input only when it was an **exact** database title, and users rarely type one:
- The database often has no row for the base game. For example, there is no plain 艾尔登法环, only 艾尔登法环 黑夜君临 and 艾尔登法环 黄金树幽影.
- Titles carry suffixes, such as 生化危机4 重制版.

On a miss, the Chinese or Japanese text went to flingtrainer.com, which indexes only English, so the search returned 0 results. The suggestion dropdown already worked; typing a query and pressing Enter did not.

## What changed
**For users.** Pressing Enter on a Chinese or Japanese query now finds trainers whenever part of a database title matches:
- **One game matches:** its English title is searched (生化危机4 → "Resident Evil 4").
- **Several games match:** the English words their titles start with are searched, in one request (生化危机 / バイオハザード → "Resident Evil", 艾尔登法环 / エルデンリング → "Elden Ring").
- **No useful shared words:** up to five titles are searched, and the results are combined without duplicates.
- **The query contains a whole title:** that game is found (艾尔登法环 黑夜君临 修改器 → "Elden Ring Nightreign").
- **English queries behave exactly as before.** A broad Latin query such as "ace combat" is still searched as typed.

**For developers**
- `fling-mapping`: new `GameMappings::search_terms(input) -> Vec<String>`.
  - Exact and normalized-exact matches win.
  - Chinese or Japanese input is then matched as a substring of the normalized Chinese and Japanese titles. Titles starting with the query rank first.
  - Several matches collapse to their shared leading English words. These must include a word of 3+ characters that isn't "the", "a", "an", "of" or "and"; otherwise the result is up to `MAX_SEARCH_TITLES` (5) titles.
  - When nothing contains the query, the longest title the query contains is used.
- `fling-site`: `SiteClient::search` requests every term concurrently and sorts each page by relevance to its own term.
  - It concatenates the pages in term order, dropping rows whose URL an earlier term already returned.
  - It enriches the merged list once.
  - It fails only when every request fails.
  - With a single term, behavior is unchanged.
- `CLAUDE.md`: the `fling-mapping` note now describes `search_terms` instead of exact-only matching.

## Type of change
- [x] Bug fix
- [ ] New feature
- [ ] Performance or refactoring (no behavior change intended)
- [ ] Docs, CI, or build only

## How it was tested
- [x] `cargo test --workspace` passes
- [x] `cargo clippy --workspace --all-targets -- -D warnings` and `cargo fmt --all --check` pass
- [ ] Tried the affected flow in the app (search / download / covers / library / settings — say which)

The three commands ran on the Windows toolchain, chained into one invocation that exited 0.

**New tests**
- Six in `fling-mapping`: exact match wins; partial Chinese and Japanese input; ranking and the generic-prefix rule; a title inside a longer query; Latin input untouched; the five-title cap.
- Three in `fling-site`: a partial query searches "Resident Evil"; two titles are merged without duplicates; one failed request keeps the other's results, and all failing is an error.

**Live search.** The headless example (`cargo run -p fling-app --example headless -- search <term>`) ran the real backend against the live site and the bundled database v0.0.5, in a throwaway data directory.

| Query | Before | After |
|-------|--------|-------|
| 艾尔登法环 | 0 | 2 (Elden Ring Shadow of the Erdtree, Nightreign) |
| エルデンリング | 0 | 2 |
| 生化危机 | 0 | 8 (Resident Evil Requiem, 4, Village, …) |
| バイオハザード | 0 | 8 |
| 黑神话 | 0 | 1 (Black Myth: Wukong) |
| 生化危机4 / 赛博朋克 / 巫师3 / 空洞骑士 | not run | 6 / 1 / 2 / 2, the expected games |

**Third box unticked.** The GUI was not rebuilt, and the search bar was not used interactively. It sends the same `Command::Search` as the headless run, but the maintainer should still press Enter on a Chinese query in the built app.

## Checklist
- [ ] I have read [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [ ] UI changes include before-and-after screenshots
- [ ] New user-facing strings are in `crates/fling-ui/locales/app.yml` with zh-CN, en and ja values
- [ ] Changes to the translation database, i18n files, ONNX model, or packaging layout are called out above
- [x] AI disclosure: No / Yes (say which parts, and whether you verified them)

- **Unticked:**
  - Screenshots, strings, database and packaging: not applicable. There is no UI, string, database or packaging change.
  - CONTRIBUTING.md: left for the maintainer.
- **AI disclosure:** yes. The diagnosis, code, tests and docs were written with Claude Code. They are verified by the automated tests and the live headless searches above, but not interactively in the GUI.

**Open items**
- A game missing from the translation database is still searched as typed and finds nothing.
- 生化危机4 searches "Resident Evil 4", but the site's fuzzy filter also returns other Resident Evil trainers, and the app's sort decides the order. English searches behave the same way.
