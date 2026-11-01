# Template

A project template for working with Claude Code: a set of skills that take a
change from a conversation to a pushed, verified branch — planning, PRDs,
test-driven implementation, bug fixing with proven causes, and commits.

## Quickstart

1. Create a repository from this template and clone it.
2. Open Claude Code at its root and run:

   ```
   /setup
   ```

   It asks a few questions, one at a time (the project, its stack and
   tools), then scaffolds a minimal skeleton with one passing test, wires
   `make test` and `make check`, fills `CLAUDE.md`, trims what you don't
   need, proves both targets green, commits, and removes itself.
3. Start your first piece of work, in a new session:

   ```
   /grill-me <what you want to build>
   ```

## Conventions the skills rely on

| What | Where |
|------|-------|
| `make test` — the fast loop: unit tests, on the host, in seconds | `Makefile`, `CLAUDE.md` § Checks |
| `make check` — the net: everything that must be green before a push | `Makefile`, `CLAUDE.md` § Checks |
| Project rules every change must respect | `CLAUDE.md` § Ground rules |
| Plans, PRDs | `docs/plan-<N>.md`, `docs/prd/triage/` |
| A running plan's checklist | `progress.md` at the root, guarded by a hook `plan-pilot` installs |
| Branch names: `<type>/<short-topic>` or `plan-<N>-<topic>`, never `claude/*` | `CLAUDE.md` § Branches, enforced by `.claude/hooks/no_claude_branch.sh` |
| The template's own tooling and its tests (`make test-template`) | `.claude/` only: the project's tree stays the project's |
| Throwaway files | `tmp/` (git-ignored) |

`.claude/settings.json` denies merging pull requests (merging is the
owner's job) and `AskUserQuestion` (unattended runs must never block on a
question: a `guideline` agent answers instead), and installs the pre-push
hook at the start of every session.

## Working with Claude

### Skills you run

| Skill | Use it to |
|-------|-----------|
| `/setup` | Bootstrap a new, empty project made from this template (once) |
| `/grill-me` | Get interviewed about a plan or design until every decision is made |
| `/plan-write` | Turn that conversation into `docs/plan-<N>.md` |
| `/plan-converge <plan>` | Sharpen the plan with review rounds until they come back empty |
| `/plan-pilot <plan>` | Implement the whole plan unattended, step by step, then open the PR |
| `/fix-bug <symptom>` | Fix one bug: proven cause, failing test, minimal fix, independent check |

Run `plan-converge` and `plan-pilot` each in a **new session**: the plan file
is all they know, so everything decided while grilling must be in it.

### Skills the others call

`to-prd` (one PRD per plan step), `prd-autopilot` (implements the PRD queue),
`tdd` (red-green-refactor), `check-net` (runs `make check`, drives failures
back to green), `help-me-debug` (proves a bug's cause), `git-commit`,
`plan-review`, `warn-review` (design smells at each phase end), `guideline`
(the stand-in human during unattended runs).

Follow a running plan by reading `progress.md` at the root of its branch.
