---
name: project-records
description: Write the mandatory FLiNG Downloader documentation records — a work log after any request that changes files, a code-review archive after reviewing code, and a PR record before opening a pull request. Use whenever finishing such a request, review, or PR.
---

# Project records

This repository keeps three kinds of permanent records under `docs/`. They are
mandatory, not optional extras: a decision that exists only in a conversation is lost
the moment the session ends.

## The three records

| Record | Trigger | Directory | Language |
|--------|---------|-----------|----------|
| Work log | Any request that changes files | `docs/work-logs/` | English |
| Code review | Any code review | `docs/code-reviews/` | English |
| PR record | Before opening a pull request | `docs/pull-requests/` | English |

All three are written in English. Quote the user's request in whatever language they
wrote it. The one exception is the PR record's section headings and checklist items: they
stay exactly as in `.github/pull_request_template.md` (Chinese), because the record becomes
the PR body. Work logs and PR records written before 2026-10-02 are in Chinese. Leave them
as they are; do not translate old records.

All three use the filename `YYYY-MM-DD-<topic>.md`, where `<topic>` is a short
kebab-case slug — for a PR record, the branch topic. Use the real current date, and
write dates absolutely (`2026-08-28`, never "today" or "last week").

Cross-link the records for one piece of work: the work log links to the review and the
PR record, the review's status summary links to the work log, and the PR record links to
both. Use repo-relative links (`../work-logs/...`) so they resolve on GitHub.

## Rules that apply to all three

- **Never tick a verification box for a command you did not run.** Leave it unchecked and
  say why in the surrounding prose. This is the rule most likely to be quietly broken;
  breaking it makes every other record untrustworthy.
- **State the verification status explicitly**, including "not built, not tested". The
  build is Windows-only (cargo on the MSVC toolchain, run from WSL via `cmd.exe`); when it
  could not be run, that is a fact for the record, not an excuse to omit.
- `CONTRIBUTING.md` requires the **AI-assistance disclosure** to be filled in honestly:
  which parts were AI-generated, and whether they were verified.
- Record what was **not** done or **not** covered as carefully as what was. Scope
  boundaries are the part a future reader cannot reconstruct.
- Quote the user's request verbatim rather than paraphrasing it.

## 1. Work log — `docs/work-logs/YYYY-MM-DD-<topic>.md`

Written after any request that changes files, capturing the whole request → plan →
change cycle. One file per request; append a new section to the day's file when a
request is a direct follow-up to one already logged there.

```markdown
# Work log: <one-line topic>

- **Date:** YYYY-MM-DD
- **Branch:** `<branch>` (from `main` at `<commit>`)
- **Related:** links to the review archive / PR record, if any

## 1. Request
> The request, quoted verbatim
What the request actually asked for, in a sentence or two.

## 2. Plan
The plan steps, in the order they were carried out.

## 3. Files changed
| File | Change |  ← for code changes, add a third column "Addresses"
Say why each file changed, not only what changed.

## 4. Verification
What was run, what was not, and why. If something was not run, say so plainly.

## 5. Open items
What is unfinished, not covered, or needs follow-up.
```

## 2. Code review — `docs/code-reviews/YYYY-MM-DD-<short-title>.md`

Written after reviewing code, whether or not anything is fixed as a result.

```markdown
# <Title> Code Review

- **Date:** YYYY-MM-DD
- **Reviewed at commit:** `<sha>` (branch)
- **Scope:** what was read, and how (source reading, call-path tracing, running it)
- **Not covered:** the parts deliberately left out

## Status summary
One paragraph on whether the findings are implemented, plus the findings table.

| # | Finding | Severity | Implemented |
|---|---------|----------|-------------|
| 1 | ... | Medium | ❌ No — open  /  ✅ Yes — YYYY-MM-DD |

## <n>. <Finding title>
**Location:** `file:line` · **Severity:** ... · **Implemented:** ...
What is wrong, the concrete path that reaches it, and how reachable it actually is —
state reachability honestly instead of inflating severity.
**Fix.** Added when the finding is implemented; describes the change, not the intent.

## Verified correct — do not "fix" these
Things that looked like defects but are handled properly, so nobody undoes them later.
```

The `Implemented` column is the point of the document: keep it current. When findings are
later fixed, update this file in place — do not start a second review document for the
same review. Line numbers stay as they were at review time; the header records the commit.

## 3. PR record — `docs/pull-requests/YYYY-MM-DD-<branch-topic>.md`

Written **before** opening the pull request, then used as the PR body. Follow the
sections in `.github/pull_request_template.md` exactly. Keep its headings and checklist
items verbatim, and write everything else in English:

```markdown
# PR: <title>

- **Date:** / **Branch:** `<branch>` → `main` / **Base commit:** `<sha>` / **Related records:**

## 关联
The Issue this addresses, or the problem being solved.

## 改了什么
What users or developers will notice. Do not just paste a file list.

## 怎么验证
- [ ] `cargo test --workspace`
- [ ] 本地跑过相关界面 / 下载 / 搜索路径
Leave a box unticked if the command was not run, and say why and who needs to run it,
in which environment, before merge.

## 检查项
- [ ] 已阅读 CONTRIBUTING.md
- [ ] 界面改动附了截图（不适用可删）
- [ ] 若改动了翻译库、i18n、模型或打包资源，已在上文写明
- [ ] AI 使用披露：否 / 是（说明用在哪一部分）
```

The PR body and this file should say the same thing. The PR body ends with the Claude Code
generation line; the file does not need it.

## Extending this skill

A new record type needs three things: a trigger, a directory under `docs/`, and a
template here. Add a row to the table above and a numbered section, then add the trigger
to the `Mandatory records` section of `CLAUDE.md` so it is reachable without loading this
file. Keep the naming convention (`YYYY-MM-DD-<topic>.md`) — it is what makes the
directories sortable and greppable by date.
