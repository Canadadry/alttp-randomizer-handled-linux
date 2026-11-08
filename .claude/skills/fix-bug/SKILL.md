---
name: fix-bug
description: Fixes one reported bug end to end — proven cause, failing test, minimal fix, independent check, commit. Also called unattended by check-net and plan-pilot.
---

# fix-bug

Drives one reported bug to a committed fix through four fresh, memoryless agents. Each stage hands the next one **evidence**, never an opinion: the debug agent hands over a proof, the implementer hands over a failing-then-passing test, the verifier hands over its own independent reproduction.

**Argument**: the bug report — free text, or a path to a file holding it.

One invocation = one bug. It is not a loop over a backlog; to chain bugs, invoke it again.

## Two modes

**Called by the user.** No report given → ask for the symptom and how they hit it. Then the run is unattended until stage 4, where `git-commit`'s proposal waits for the user's go-ahead. Those are the only two moments this skill talks to a human.

**Called by `check-net` or `plan-pilot`.** Nobody is there. The caller writes the report, and the caller is the human at the commit gate: it checks the fix itself and accepts or refuses.

## Rule #1 — the sub-agents never wait on a human

Any `QUESTION:` from a sub-agent is answered by a fresh `guideline` agent, relayed back — never by pausing for the user.

This bites hardest on stage 1: tell the debug agent that it runs every test itself, in its own environment, so it designs tests it can run rather than tests that need the reporter's machine or production.

Be honest about what this costs: a bug may have been observed in production or on the reporter's machine, while this pipeline proves causes in a session environment. A cause proven here is proven *about the code*, not about that environment. When the debug agent can only reach an environment-dependent hypothesis, that is an `UNPROVEN:` (see stage 1's gate) — not a green light.

## Rule #2 — no fix before a proven cause

Stage 2 never starts on a plausible cause. The gate after stage 1 is the whole point of this pipeline; waved through, this is just "guess a fix" with extra agents.

## Constraints every sub-agent inherits

Put these in each sub-agent prompt — a fresh agent knows none of them:

- **No access to production or to the reporter's machine.** Reproduction runs in the session's environment: the build, the tests, the checks, throwaway programs.
- **Only the orchestrator touches git.** Sub-agents never commit, stage, stash, branch or push.
- **The suites are `make test` (the fast loop) and `make check` (the net, `CLAUDE.md` § Checks; a failure naming a missing service or input is setup, not the bug)** — prefer the Makefile targets over invoking a runner directly.
- **The reference is the expected behavior**: the tests, the documentation, the PRD or plan that specified it, and the repository's own conventions, cited from a real site in the tree.
- Throwaway repro programs go in `tmp/`, never in the source or test directories.
- **The game's test directory** (`tests/alttp` or `tests/ladx`; the report names it, else the failing paths do): its `make -C <game> test` is that game's dev loop, and the RED unit test goes where it runs it, `<game>/unit/test_<module>.c` in that Makefile's `UNIT_TESTS` (`check-e2e`, "Where the RED test goes").
- **LADX, whenever the bug touches `ladx/` or `tests/ladx/`** (`docs/plan-3.md` §2, `CLAUDE.md` § Ground rules):
  - Never print a ROM byte: with the ROM (`tmp/ladx/base.gbc`) read, programs print offsets, lengths, counts and digests only, and C's outputs from a `--rom` run are compared through the e2e driver or by digest, never shown. Never write or echo `LADX_ROM_URL`.
  - RED tests use synthetic data only (hand-built states, `tests/ladx/unit/synth_rom.c`'s synthetic ROM): never a golden, a table of `data/ladx/`, the ROM or anything derived from it.
  - A cause proven to sit in a ROM-derived artifact (a golden, a table of `data/ladx/`, an inventory Side, Group or coverage) is fixed at the oracle (its scripts or `ladxr.patch`, RED test a `tests/ladx/oracle/test_*.py`), the artifact regenerated with the ROM in its own commit; never edited by hand, never cut.
  - `make -C tests/ladx e2e-rom` and `rom-check` need the ROM; without it their `setup:` line is not the bug.

## Pipeline

### 1. Prove the cause — fresh `help-me-debug` agent

Spawn a fresh, memoryless agent running the `help-me-debug` skill. Hand it the bug report **verbatim** — never your paraphrase, and never your own early theory. A summary is exactly where an unproven hypothesis quietly becomes a premise.

Its mandate: understand and prove, **change nothing** outside `tmp/`. It may build, run, instrument throwaway copies in `tmp/`, and read anything.

Reporting contract:
- `PROVEN:` followed by the bug document from `help-me-debug`'s own template (Summary / Reproduction / Established cause / Scope / Inputs for prioritization / Unverified leads).
- `UNPROVEN:` followed by what was eliminated, what remains open, and exactly which observation is missing.
- `QUESTION:` for anything blocking, resolved via a `guideline` agent (Rule #1).

**Gate — read the proof, don't skim it.** Every claim about the cause must carry a file excerpt with path and line numbers, the output of a command actually run, or sourced documentation with proof of applicability. "Consistent with", "would explain" and "most likely" are not proofs.

- Proof holds → stage 2.
- `UNPROVEN:`, or a "proof" that is really a chain of reasoning → spawn **one** more debug agent, handed everything already eliminated so it doesn't retrace it. Still unproven after that → stop and report back what is known and which observation is missing. Stopping here is a legitimate outcome of this skill, not a failure of it.

### 2. Fix it — fresh `tdd` agent

Spawn a fresh, memoryless agent running the `tdd` skill. Hand it the stage-1 bug document **verbatim** as its context, plus the constraints above.

Its mandate, in this order:

1. **RED first**: a test that fails *because of this bug*, exercising the real path through the public interface — never one that asserts the shape of the fix. Drive it through the module's entry point on a small input the test builds itself, never on production data or an external service. A bug that a check other than a test reports (a lint or type error) needs no new test: that check going green is the proof. Never weaken a check to get green (`CLAUDE.md` § Checks).
2. **GREEN**: the minimal fix the proven cause justifies. Nothing adjacent, no cleanup, no refactor of neighbouring code.
3. `make test` fully green.

It updates `README.md` only if user-visible behaviour changed. It never touches git — stage 4 owns that.

Reporting contract: `DONE:` plus what the test asserts, which files and lines changed, and why that is the minimal fix — or `QUESTION:` (Rule #1). `DONE:` means implemented, **not** verified.

### 3. Verify independently — fresh agent, no skill

Spawn a third fresh, memoryless agent. It is not handed the implementer's reasoning as truth; it re-establishes the facts itself:

1. **Prove the new test actually detects the bug**: rebuild a throwaway copy of the pre-fix code under `tmp/` (or revert the fix in a scratch copy) and show the new test failing against it, then passing against the tree. A test that passes both ways proves nothing — this is the most common way a fix pipeline fools itself.
2. Run `make test` and `make check` in full; for LADX with the ROM present, also `make -C tests/ladx e2e-rom` and `make -C tests/ladx rom-check`.
3. Read the diff: it must touch only what the proven cause justifies, and nothing else.

Reporting contract: `VERIFIED:` plus the evidence for each point, or `FAILED:` plus what broke.

On `FAILED:`, route by kind. A mechanical failure (test doesn't compile, wrong path, suite breakage) goes back to stage 2 with the detail. A fix that doesn't actually remove the symptom means the cause wasn't the cause: back to **stage 1**, not stage 2, handed the verifier's evidence. After one such round trip, stop and report back rather than spinning.

### 4. Commit the fix

Invoke the `git-commit` skill **yourself, in place** — never in a sub-agent. Its proposal goes to whoever holds the gate (see Two modes): the user, or your caller.

The commit message is where the finding is recorded — no journal file, no prose in the code. Write it in English, strictly factual, from the stage-1 bug document and the stage-3 evidence:

- the symptom as reported, and what surfaced it (`make check`, a unit test, a review, a user);
- **the hypothesis that was disproved**, when there was one — as valuable as the cause itself, since it's what spares the next reader the same detour;
- the proven cause, with file/function references;
- the fix retained, and what was deliberately *not* changed.

Write it yourself rather than delegating: you hold both the proof and the verification, a fresh agent would hold neither.

What the commit message must NOT absorb, because it belongs in the working tree instead:

- a trap a future reader could reintroduce → an `assert` carrying a message at the exact spot, or, when the invariant isn't checkable at runtime, a comment of at most 40 words there.

Scope the commit to the code fix and its test (`fix:`). With `progress.md` at the root, `git-commit` folds it into `HEAD` instead (§ `progress.md`): the message above goes into `HEAD`'s body.

## Stopping conditions

Stop and report back to your caller when the cause can't be proven after two debug agents, when a `FAILED:` verification survives one round trip, or when the commit proposal is refused. Never widen the fix, never skip or weaken a test to get green, and never commit on an unproven cause.
