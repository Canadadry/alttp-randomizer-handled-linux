---
name: git-commit
description: Internal — called in place by prd-autopilot and fix-bug. Runs the tests, stages only the current work, writes a conventional commit message, commits.
---

# git-commit

Always run in place by its caller, never in a sub-agent: the caller holds the context of what was built and why.

## Workflow

### 1. Inspect status
Use these scripts (separate calls) instead of ad hoc `git diff` invocations:
- `.claude/skills/git-commit/scripts/status.sh` — overview of changed files.
- `.claude/skills/git-commit/scripts/staged-diff.sh [path...]` — staged diff, rename-aware.
- `.claude/skills/git-commit/scripts/unstaged-diff.sh [path...]` — unstaged diff, rename-aware.

Never `git add .` or `git add -A`.

### 2. Run the tests
Code changed → `make test` must be green; red → stop and report, commit nothing. Only `.md` files changed → skip it. The net (`make check`) is not run here: `check-net` runs it once per PRD, before any push.

### 3. Check new comments
A comment on an added line says what the code cannot: a trap, a contract, the evidence behind a name or a flag (an address, a string, a call site). Work-log narration ("step 3", "tried X") goes in the commit message instead; remove it before staging. Never touch unchanged lines.

### 4. Select files
Only the files of this unit of work; leave the rest unstaged.

- One commit = one intention. Different concerns → separate commits.
- Never stage `.env`, secrets, generated binaries, or files unrelated to the task.
- Unsure whether a file belongs → leave it out and say so.

### 5. The gate
Present the proposal to whoever holds the gate:

```
Files: src/importer.py, tests/test_importer.py
Message: feat: import CSV files with a header row
```

- `fix-bug` run by the user → the user; wait for the go-ahead.
- Any unattended caller (`prd-autopilot`, a `skill:` step's skill, `fix-bug` under `check-net` or `plan-pilot`) → the caller accepts or refuses on its own checks; nobody waits.

### 6. Commit
On acceptance: `git add <files>`, then commit with the agreed message, plus the attribution trailers the session's instructions ask for, if any. Then push it (`CLAUDE.md` § Branches; the session's first commit creates the branch first) — unless the caller is an unattended one: `prd-autopilot`, `check-net` and `plan-pilot` push themselves, only once the net is green.

### `progress.md` at the root — every commit ticks
While it exists (a `plan-pilot` run), the hook installed by `.claude/skills/plan-pilot/scripts/progress_check.sh --install` refuses any commit that ticks no new `[x]` in it, even under `--no-verify`. So:
- The unit has its own item (a chunk, a step, a module) → stage `progress.md` with that item ticked.
- It has none (a `fix:`, a PRD removal) → fold it into `HEAD` if `HEAD` is not pushed (`git branch -r --contains HEAD` prints nothing): `git commit --amend`, this message appended to `HEAD`'s body. `HEAD` pushed → don't commit: leave it in the working tree, the next commit that ticks carries it and its message.
- A refusal is never bypassed: fix `progress.md` per the hook's message.
- New lines refused because an `[x]` follows them → append them at the end in a top-level group naming the step instead (`- [ ] Step <id> chunks` for its children, `- [ ] Added after step <id>` for new steps after it); `.claude/skills/plan-pilot/scripts/progress_tidy.py` moves such groups back under their step, in a `docs:` commit of its own that the hook accepts though it ticks nothing.
- The commit ticks the last `[ ]` → follow it with a `docs:` commit that `git rm`s `progress.md` and the finished plan file (`plan-pilot` § Find the next step).

## Commit message format

```
<type>: <short description>

<optional body: the why, the finding, what was deliberately not changed>
```

| Type | When |
|------|------|
| `feat:` | New user-visible behavior |
| `fix:` | Bug correction |
| `test:` | Adding or updating tests only |
| `refactor:` | Code restructuring, no behavior change |
| `chore:` | Tooling, deps, config, CI |
| `docs:` | Documentation only |

- Subject: 50 chars max, imperative mood ("add", not "added"), no final period, in English.
- A subject that needs "and" is two commits.
