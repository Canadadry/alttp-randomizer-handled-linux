---
name: check-net
description: Internal — called by prd-autopilot after each PRD, and by plan-pilot after a "skill:" step. Runs make check and drives every failure back to green through fix-bug, one at a time.
---

# check-net

`make test` is the dev loop: unit tests, on the host, seconds. The net is `make check`, run once, at the end — here (`CLAUDE.md` § Checks). It must be green. A red `make check` is always a bug.

Never weaken a check to get green: no skipped or deleted test, no edited expected value or snapshot, no lowered threshold, no rule added to an ignore or allow list, no check removed from `make check`. Those change only in the step that makes the deliberate change, with its reason — never here, nor in the `fix-bug` driven from here.

Every bug found by a slow check that the fast loop could have caught leaves behind a unit test that catches it in `make test`.

## Before the loop: the environment

`make check` may need things beyond the host (`CLAUDE.md` § Checks names them: a service, a container, a dependency). If one is missing, set it up before anything else. A missing environment is not a failure: never send it to `fix-bug`. A target that fails naming a missing input or service is this case; any other failure is a bug.

## Rule #1 — never wait on the human

No one answers during this loop. A `QUESTION:` from any sub-agent goes to a fresh `guideline` agent, never to the user. The only way back to the human is the stop after 3 failed attempts (step 5).

## Rule #2 — you are the human at fix-bug's commit gate

`fix-bug` stops once, at its `git-commit` proposal, for a human go-ahead. Here, you give or refuse it, after running the checks of step 4 yourself. Never accept on the strength of an agent's report.

## Loop

### 1. Run the net
Run `make check` in full; when it is made of several targets, run each one even after the first is red (`make -k check`), so that every failure is known. Exit 0 → report `NET-GREEN:` with the list of `fix:` commits made during this run, and stop.

### 2. Split the red into failures
One failure = one failing test, one failing check item (a lint error, a type error, a failing scenario), or one target's tool error. Keep the full output of each.

Order: the cheapest to diagnose first — a tool error, then the fast loop's failures, then the slower checks in the order `make check` runs them. Handle one alone; the others wait for the next run of the net, since a fix often repairs more than one.

### 3. Invoke fix-bug on it
Invoke the `fix-bug` skill in place. Its argument is a bug report you write, containing:

- **Symptom**, verbatim: the failing target's output for this failure, nothing paraphrased.
- **Reproduction**: the narrowest command that reproduces it (the failing target, or a single test), and `make check` for the commit gate.
- **Constraints on the fix**, to be carried into every stage:
  - The bug gets a RED **unit test run by `make test`**, on an input the test builds itself, fast and without external services — unless the failing check is itself the most direct proof (a lint or type error), in which case that check going green is the proof.
  - The fix turns `make test` and `make check` green.
  - Never weaken a check to hide the failure (see the top of this skill).
- **Already eliminated**, on a retry (step 5): every hypothesis the previous attempts disproved, and why their commit was refused.

### 4. The commit gate — check, then accept or refuse
When `fix-bug` proposes its commit, check each point yourself, with commands you run:

1. The fix adds (or extends) a test that `make test` runs, unless step 3 waived it.
2. That test **fails on the pre-fix code**: in a throwaway `git worktree` of `HEAD` under `tmp/`, apply only the diff's test changes and run it — it must fail. Then it passes on the working tree.
3. `make test` and `make check` are green, and the failure you handed over no longer appears.
4. The diff weakens no check (see the top of this skill), unless the bug report said that was the cause.

All hold → accept: `fix-bug` commits (folded into `HEAD` when `progress.md` exists, see `git-commit`). Remove the worktree.
Any one fails → refuse, with the reason. Nothing is committed; restore the working tree to `HEAD` before the next attempt.

### 5. Count the attempt
An attempt fails when you refuse the commit, or when `fix-bug` stops on its own (cause unproven after two debug agents, verification failing twice).

- Fewer than 3 failed attempts on this failure → back to step 3 with the same failure, the bug report extended with what was eliminated and why the last attempt failed.
- 3 failed attempts → stop. Report `NET-FAILED:` to the user: the failure, the three attempts, what each eliminated, and which observation is missing. This is the only moment this skill hands back to the human.

### 6. Loop
After an accepted commit, back to step 1: `make check` from the top.

## Output

- `NET-GREEN:` + the `fix:` commits made, or folded into `HEAD` (possibly none).
- `NET-FAILED:` + the report of step 5. The working tree is back at `HEAD`, nothing half-fixed is left behind.

Never push: the caller decides.
