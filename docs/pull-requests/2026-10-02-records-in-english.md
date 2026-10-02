# PR: write work logs and PR records in English

- **Date:** 2026-10-02
- **Branch:** `docs/records-in-english` → `main`
- **Base commit:** `a5f1737`
- **Related records:** [Work log](../work-logs/2026-10-02-records-in-english.md)

## 关联

No Issue. The maintainer asked for work logs and PR records to be written in English, the
same as code-review records already are.

## 改了什么

- `agents/skills/project-records/SKILL.md`: all three record types are now English. The
  work-log template and the prose of the PR-record template are translated.
- `CLAUDE.md`: the `Mandatory records` table matches the skill.
- Two edge cases are spelled out:
  - The user's request is still quoted verbatim, in whatever language it was written.
  - A PR record keeps the section headings and checklist items of
    `.github/pull_request_template.md` (关联 / 改了什么 / 怎么验证 / 检查项) verbatim,
    because the record becomes the PR body. Everything else is English.
- Records written before 2026-10-02 stay in Chinese and are not translated.
- This PR's own work log and record are the first ones written under the new rule.

## 怎么验证

- [ ] `build.cmd tests`
- [ ] 本地跑过相关界面 / 下载 / 搜索路径

Neither box applies: this is a docs-only change with no code, QML, or build changes, so
nothing was built or run. The two tables were checked by reading them against each other
and against `.github/pull_request_template.md`.

## 检查项

- [x] 已阅读 [CONTRIBUTING.md](https://github.com/Sqhh99/FLiNG-Downloader/blob/main/CONTRIBUTING.md)
- [x] 若改动了翻译库、i18n、模型或打包资源，已在上文写明 (none changed)
- [x] AI 使用披露：是 — the edits and both records were written by Claude Code at the
  maintainer's request.
