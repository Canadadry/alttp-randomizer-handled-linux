# Notes for Claude sessions

<One paragraph: what this project is, and where to start reading.>

(Template not set up yet: run `/setup`.)

## Checks

- **`make test` is the fast loop**: unit tests, on the host, in seconds, with
  no network and no external service. Never make it slower: anything slow or
  needing a service is a separate target, called from `make check`.
- **`make check` is the net**: everything that must be green before a push.
  A change that keeps it green is correct; one that breaks it is a bug,
  whatever the unit tests say. A target failing because its environment is
  missing (a service down, a dependency not installed) is a setup problem,
  not a bug.
- Never weaken a check to get green: no skipped or deleted test, no edited
  expected value, no lowered threshold, unless that is the deliberate change,
  stated with its reason.
- Throwaway files go in `tmp/` (git-ignored).

## Ground rules

<Project-specific rules every change must respect, one bullet each, with the
goal each one serves. `warn-review` flags every violation; `guideline` decides
by them. Delete this section if there are none.>

## Language

Everything written in the repository is in English: code, comments, notes,
skills, commit messages, PR titles and descriptions, plans, PRDs. The
conversation with the owner can be in French.

## What we don't do

- **Never merge a pull request**, in any way (GitHub tool, `gh`, auto-merge).
  Merging is the owner's job; denied in `.claude/settings.json`.

## Plans

Plans live in `docs/plan-<N>.md` and are run with the skills of `.claude/skills/`:
`grill-me` → `plan-write` → `plan-converge` → `plan-pilot`. A finished plan is
deleted (git is the history); findings go to `notes/`, not to the plan.
`progress.md` at the root is a running plan's checklist, guarded by the git
hook `plan-pilot` installs (`.claude/skills/plan-pilot/scripts/progress_check.sh --install`).

## Branches

- **Never work on or push a `claude/*` branch.** A session starts on one; at
  its first commit it creates a branch named after the work, then commits and
  pushes there as it goes (`git push -u origin <branch>`).
- Name: `<type>/<short-topic>`, the type of the first commit (`feat/import-csv`,
  `fix/empty-header`, `chore/ci`); `plan-<N>-<topic>` for a session that
  starts by writing a plan.
- The pre-push hook `.claude/hooks/no_claude_branch.sh` refuses any push to
  a `claude/*` branch; the `SessionStart` hook of `.claude/settings.json`
  installs it.
- No pull request unless the owner asks for one; `plan-pilot` opens its own
  at the end of a finished plan.
