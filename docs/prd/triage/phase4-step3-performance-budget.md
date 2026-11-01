---
title: "Performance budget: the generator measured against 40314c3, and the Cortex-A7 estimate"
description: "Nobody has measured the finished generator against the plan's reference interleaved, attributed the post-fill growth, or re-checked the Generating screen's no-progress-indicator decision on final numbers."
status: needs-triage
---

## The step (plan 1, Phase 4 step 3, verbatim)

> 3. **Performance budget**: measure the generation time of the slowest golden case on the host at `-O2`, and estimate the time on the Cortex-A7, confirming Phase 3 step 5's progress-indicator decision (about 10 s), or reopening it if the numbers moved. Verified by: `make -C tests/rando measure` (outside `test` and `e2e`), with its numbers recorded in the step note.

This step runs **before** Phase 4 step 2: step 2 waits on the owner's `data/base.bps` (Phase 0's outcome (b)), and the plan's step 2 text has the run do step 3 and Phase 5 step 1 first. Nothing here needs `data/base.bps`, a cross-built package, the release zip or the SDL build.

## Context: facts from the plan and the code (at `de2c83f`)

- **§4, Done when:** "`e2e`'s per-case time, as measured by `make -C tests/rando measure` against Phase 1 step 1's commit on the same machine, is unchanged within ±10%, apart from the write-log diff."
- **Step 1.1:** the reference is commit `40314c3`. Each budget check re-measures it in the same session from a throwaway worktree under `tmp/`, next to the current tree, and records both sets of numbers; times from different machines are not comparable. Reference at the time: `item_placement-basic` 17.1 ms (the max), most cases 2.9–5.3 ms, 44 cases.
- **Step 1.3's lesson:** two sequential runs of both trees gave mean ratios 0.93 and 0.98, with different cases outside ±10% each time (noise on a shared machine). Only an interleaved comparison (six alternating pairs of 51 reps) settled the one case flagged twice (+1%). So later checks compare interleaved, not one run after the other.
- **Step 1.4:** the post-fill draws (`setTexts` … `writePrizePacksToRom`, about 1,700 draws per case, 805 → 2505 for baseline) made generation slower: interleaved, per-case minimum, mean ratio 1.024 on the 44 shared cases, min 0.86 (`goal-ganonhunt`), max 1.31 (`goal-triforce-hunt`), `item_placement-basic` 17.1 → 21.0 ms (+23%). That step had no budget. Its note says: "§4's ±10% … will need its own judgement".
- **Step 3.5:** slowest case `item_placement-basic` 16.2 ms median; sweep of 1024 baseline seeds, median 4.5 ms, max 7.2 ms, all won at attempt 0. Whole pipeline (`rando_assemble` on the synthetic base, 20 seeds, ad hoc): baseline 25 ms, `item_placement-basic` 35 ms. Cortex-A7 "20–50× slower" gives ~0.8–2 s, or ~2–3 s with SD I/O, under ~10 s, so **no per-stage progress indicator**, just one static Generating screen. That step also made `make measure` exit 0 on clang hosts, and left the stale "805 draws" warning and the DS-era `over` budgets as informational.
- **What `make measure` times:** its driver (`rando_measure`) times `rando_generate` only, with a counting trace hook. It times no `rando_rom_write`, no write-log formatting and no diff. So §4's "apart from the write-log diff" is outside the measured figure **by construction**, and the post-fill draws are **inside** it: they are part of `rando_generate`.
- **The like-for-like split exists in the library:** `rando_generate_until(…, RANDO_STAGE_FILL)` stops on the first winnable attempt right after `fill`. Every attempt that completes the fill then runs only the winnability check, with no `setTexts`, `randomizeCredits` or `applyHints` and no `writeToRom` stage. That is the work `rando_generate` did at `40314c3` (fill, then `isWinnable`).
- **The reference driver** at `40314c3` has the same `cases <reps> <case_dir>...` interface and the same `case …` line format (`… <median ns>` last). It reads its own tree's cases (44, without the `hints`/`heart_beep` keys).
- `measure.sh` still prints `WARNING: baseline C run … expected … 805 draws` on every run. The real count has been 2505 since step 1.4, and the golden says so.

## Hypotheses about Phase 4 step 1 (SDL renderer)

Step 1's PRD is `docs/prd/triage/phase4-step1-sdl-renderer.md`. Its note (`### Step 4.1`) was recorded at `de2c83f` while this PRD was written, and all three hypotheses held there. They stay labelled because the tree may move before this step starts. This step measures the generator, so it depends on step 1 only where the two share files.

- **H-4.1-1 (the generator and the measure tooling are untouched):** step 1 changes nothing under `rando/`, nothing in the measure driver, `measure.sh` or its shim, and nothing in the `measure` target or the `MEASURE_*` variables. (`git diff 6c459cf de2c83f` over those paths is empty.) *If step 1 changed them*, this step builds on what is there and records in its note the commit it measured at.
- **H-4.1-2 (the net needs no SDL):** `make check` is still `test lint logic selftest e2e e2e-combos`, and the SDL targets stay outside it, so this step's `make check` needs no SDL package. *If an SDL target joined the net*, this step runs `make check` where the SDL packages exist, or reports the missing packages as a setup problem.
- **H-4.1-3 (shared files settled):** step 1's edits to the root `Makefile` help, `CLAUDE.md` § Checks and the `tests/rando/Makefile` `.PHONY` line are committed. This step's new help line and § Checks bullet go after them, and no step 1 line is rewritten. *If step 1's edits were still pending*, this step touches only the `measure` bullet of § Checks and appends its own lines.

## Hypotheses of this step

- **H-4.3-1 (the Cortex-A7 factor):** the Mini Flip's Cortex-A7 runs this integer, branchy, cache-friendly C between 20× and 50× slower than the measuring host, step 3.5's range. The tools encode the range as two named constants. This step does not re-derive it, and it states that the range is an assumption that the owner's device timing replaces. *If the owner's device timing (Further Notes) falls outside the projection*, the final step note records the measured figure, and the decision is re-taken on it.
- **H-4.3-2 (SD I/O allowance):** on the device, reading `base.sfc` (1 MiB) and `base.bps`, then writing the 2 MiB ROM and the record, adds at most 1.5 s (≥ 2 MB/s). This is an allowance added to the projection, not measured.
- **H-4.3-3 (the reference builds today):** a detached worktree of `40314c3` under `tmp/` builds its own `rando_measure` with today's host `gcc`, as steps 1.3 and 1.4 did. *If it does not*, that is a setup problem. The step note records it and the step stops before any budget verdict. A verdict without a same-session reference is not a verdict.
- **H-4.3-4 (attempt 0):** every e2e case and every sweep seed still wins at attempt 0, so the figures cover a single attempt. `measure` already reports `attempts` and the sweep's retry seeds. *If a retry appears*, the estimate also gives the per-attempt cost × the observed maximum attempts.

## Problem Statement

The owner will wait on a Miyoo Mini Flip for a seed to generate, staring at a static "Generating" screen with no progress indicator. That screen was chosen in Phase 3 step 5 from host numbers taken mid-plan and a rough factor, and it has not been checked since. Separately, the plan's last promise about speed (§4: e2e's per-case time unchanged within ±10% of `40314c3`) has never been checked on the finished generator. The only data so far comes from step 1.4, where two cases grew by 23% and 31% once the post-fill draws went in. Nobody has said whether that growth breaks §4, and nobody can redo the comparison without re-inventing step 1.3's interleaved method by hand. Without a repeatable, honest measurement, two things are guesses: whether the owner sees a responsive app, and whether the plan is done.

## Solution

Two measuring tools, then one recorded run:

1. **`make -C tests/rando measure`** reports the facts this step needs: the slowest golden case's generation time; the whole pipeline's time (`rando_assemble` on the synthetic base) for baseline and for that slowest case; and a Cortex-A7 projection of both with H-4.3-1's range, against the ~10 s line. Its stale baseline warning is corrected.
2. **A new `make -C tests/rando measure-compare`** creates, or reuses, a throwaway worktree of `40314c3` under `tmp/`. It builds the reference driver there, then times three things **interleaved per case** on the 44 shared e2e cases: the reference, the current full generation, and the current generation stopped after `fill` (like-for-like with the reference). It reports per-case ratios and verdicts against ±10%, and each case's post-fill share. Before measuring, it proves its own arithmetic on fake drivers, and it refuses to give a verdict when the two trees did not do the same work.
3. **The step note** records both sets of numbers. It confirms or reopens the progress-indicator decision. It states the §4 comparison and the judgement on the post-fill growth in a fixed form that never changes the ±10% figure or the "apart from the write-log diff" clause.

## User Stories

1. As the owner, I want the time a seed takes on my Mini Flip estimated from the finished generator, so that I know whether the static Generating screen is acceptable.
2. As the owner, I want the estimate to cover the whole generation (patching, hashing, generating, writing the ROM image) and not only the item fill, so that it matches what I actually wait for.
3. As the owner, I want the SD-card part of the wait stated as an explicit allowance, so that I can see which part is measured and which is assumed.
4. As the owner, I want the progress-indicator decision either confirmed with numbers or reopened, so that I don't get a frozen-looking screen for longer than about 10 s.
5. As the owner, I want a request in the note to time one real generation on the device once `data/base.bps` exists, so that the estimate is checked against reality.
6. As the owner, I want §4's ±10% comparison reported exactly as §4 states it, case by case, so that I can judge the plan's completion myself.
7. As the owner, I want any case outside ±10% named with its ratio, and never hidden by a re-run or a changed threshold, so that I can trust the verdict.
8. As the owner, I want the growth from the post-fill draws separated from any other change, so that I can tell VT work we chose to add from a regression.
9. As the owner, I want the decision on whether §4 accepts the post-fill growth left to me and listed for the final step note, so that the agent does not quietly redefine "done".
10. As the plan-pilot agent, I want one command that creates the reference worktree, builds both drivers and prints the comparison, so that §4's check at the end of the plan is the same procedure as this step's.
11. As the plan-pilot agent, I want the comparison interleaved per case (reference and current alternated with a rotating order), so that machine drift and neighbours on a shared host hit both trees equally (step 1.3's lesson).
12. As the plan-pilot agent, I want each tree's per-case figure to be the minimum of several per-run medians, so that a single interference spike cannot fail or pass a case.
13. As the plan-pilot agent, I want cases flagged outside ±10% re-measured in a longer interleaved pass, with both passes printed, so that noise is settled the way step 1.3 settled it, openly.
14. As the plan-pilot agent, I want the tool to check that the current tree's stop-after-fill run draws exactly as many numbers as the reference, at the same winning attempt, so that the like-for-like comparison is proven like for like.
15. As the plan-pilot agent, I want the tool to check that the current full run's draw count equals the current golden's, so that a timing never comes from a run that diverged.
16. As the plan-pilot agent, I want the tool to exit 1 with the case named when either check fails, so that no verdict is printed on mismatched work.
17. As the plan-pilot agent, I want the tool to prove its pairing, statistic, ratio and verdict arithmetic on planted fake drivers before every real run, so that a broken report fails loudly and does not mislead.
18. As the plan-pilot agent, I want the tool to record both commits, the compiler version and the CPU, so that the two sets of numbers carry their provenance.
19. As the plan-pilot agent, I want the e2e cases that have no reference (the six added in step 1.3) listed as "new, no reference" and not compared, so that the 44-case basis is explicit.
20. As the plan-pilot agent, I want `measure-compare` outside `test`, `e2e` and `check`, so that the fast loop and the net stay as fast as they are.
21. As the plan-pilot agent, I want a missing reference build to print a `setup:` line, so that it reads as a setup problem and not as a bug (CLAUDE.md).
22. As the plan-pilot agent, I want `make measure`'s baseline warning to compare against the golden's own draw count, so that the report stops printing a false warning on every run.
23. As the plan-pilot agent, I want `make measure` to self-check the stop-after-fill mode on baseline (its draws equal the golden's draws through `fill`), so that the new driver mode is proven by the tool that uses it.
24. As the plan-pilot agent, I want `make measure` to self-check the pipeline mode (status ok, one 2 MiB image out), so that a pipeline time never comes from a failed assembly.
25. As a maintainer, I want the Cortex-A7 factor range and the SD allowance as named constants beside the DS-era ones, so that a better device figure changes one line.
26. As a maintainer, I want `CLAUDE.md` § Checks to name `measure-compare` and what it compares, so that the next budget check finds it.
27. As a maintainer, I want the synthetic-base limitation stated in the report (the synthetic BPS is far smaller than the real one, so its apply time is a lower bound), so that the pipeline figure is not over-trusted.
28. As a maintainer, I want the reference worktree reused when it already exists at the right commit and refused when it sits at another commit or is dirty, so that a stale tree never becomes the reference.
29. As a future reader of the plan, I want the judgement written as attribution plus an owner decision, not as a reinterpretation of §4, so that the plan's budgets mean what they say.
30. As the owner, I want a like-for-like regression (the pre-post-fill work slower than ±10%) treated as a bug, so that real slowdowns get fixed rather than explained away.

## Implementation Decisions

### The measure driver (`rando_measure`) gains two modes, the existing output unchanged

- **`cases-until <stage> <reps> <case_dir>...`**: the same `case …` line as `cases`, field for field, with the generation call replaced by `rando_generate_until(…, <stage>)`. `<stage>` is a stage name from `rando_stage_name`, and an unknown name is a usage error (exit 2). The status field reads `incomplete` for a stop. The `cases` and `sweep` modes and their line formats are unchanged, so `measure.sh` §1–7 and the reference driver stay comparable.
- **`assemble <reps> <case_dir>...`**: per case, times `rando_assemble` on the synthetic 1 MiB source and synthetic base patch (the unit tests' synthetic-base builder, linked into the driver), with the request's digests set to the synthetic ones, the case's options, seed and attempt cap, no trace, and an output callback that only counts bytes. One line per case: `assemble <name> <status> <bytes out> <median ns>`. Any status other than ok, or an output that is not exactly one 2 MiB image, exits 1 with the case named. Warm-up and median work as in `cases`. The `RandoAssembleWork` buffer is static, like `gen`.

### `measure.sh` (`make measure`)

- **Baseline warning:** the expected count is the baseline golden's own total draws (already parsed into the golden table), not the constant 805. Reason: the constant has been stale since step 1.4, and the warning's intent was always "the C run equals the golden". This corrects an informational line. It changes no gate.
- **Stop-after-fill self-check:** a `cases-until fill 1` run on baseline must give status `incomplete`, attempt 0, and a draw count equal to the sum of the golden's per-stage draws up to and including `fill` (805). Otherwise the tool dies. Since H-4.1-1 holds, this is the only new check.
- **New section "8. Pipeline":** `assemble` (reps `MEASURE_REPS`) on baseline and on §6's `max-case`. Lines: `host-O2.assemble.<case>.time <us> us median`, plus `host-O2.assemble.note synthetic base patch: its apply time is a lower bound`.
- **New section "9. Device projection":** the named constants `a7_factor_lo=20`, `a7_factor_hi=50` (H-4.3-1), `sd_allowance_ms=1500` (H-4.3-2) and `device_line_ms=10000`. Lines:
  - `miyoo.time.projected.generate <lo> <hi> ms (host x 20..50)` for §6's max-case;
  - `miyoo.time.projected.pipeline <lo> <hi> ms (host x 20..50)` for the same case's assemble time;
  - `miyoo.time.projected.wait <hi + allowance> ms budget 10000[ over]`.
  These lines are informational like every other `measure` budget (step 1.3: the per-case time is the only gated figure), and `measure` still exits 0 whatever the figures.
- The DS-era budgets and `ds.time.projected.*` lines stay unchanged, so reports stay comparable with step 1.1's reference log.

### `measure_compare.sh` and `make -C tests/rando measure-compare`

- **Make target** (outside `test`, `e2e`, `check`; in `.PHONY`; a root `make measure-compare` pass-through with one help line):
  - `REF_COMMIT ?= 40314c3`, `REF_DIR ?= <repo root>/tmp/measure-ref-<REF_COMMIT>`.
  - If `REF_DIR` is missing, `git worktree add --detach $(REF_DIR) $(REF_COMMIT)`.
  - Then `make -C $(REF_DIR)/tests/rando build/measure/rando_measure` (only the reference driver: the reference's own `measure.sh` would fail on a clang host, and step 3.5 fixed that only in the current tree).
  - Then the current `$(MEASURE_BIN)`, then the script.
  - A failed worktree or reference build prints `setup: cannot build the reference driver at <REF_COMMIT> (<reason>)` and exits 1 (H-4.3-3).
- **Inputs (environment):**
  - `REF_DIR`;
  - `COMPARE_PASSES` (default 6), `COMPARE_REPS` (default 21), `COMPARE_RECHECK_REPS` (default 51);
  - test hooks `COMPARE_REF_DRIVER`, `COMPARE_CUR_DRIVER`, `COMPARE_REF_CASES`, `COMPARE_CUR_CASES` (directories). The defaults are the two real drivers and the two trees' `tests/rando/cases`.
- **Reference guard:** `REF_DIR`'s `HEAD` must resolve to `REF_COMMIT`, and its tree must be clean. Otherwise exit 1, `reference worktree is not a clean <REF_COMMIT>`.
- **Shared cases:** the names present in both case directories, in the current tree's order. Names only in the current tree are printed as `compare.new <names> (no reference)`.
- **Interleaving:** for pass p = 1..P and each shared case, run the three measurements `ref` (reference driver `cases R <ref dir>`), `full` (current `cases R <cur dir>`) and `fill` (current `cases-until fill R <cur dir>`). Each invocation measures one case. The order rotates with p (`ref full fill`, `full fill ref`, `fill ref full`, …), so no measurement always runs first.
- **Statistic:** for each case and measurement, the figure is the **minimum over the P passes of the driver's median**. Ratios: `full/ref`, `fill/ref`. Post-fill share: `full − fill` in µs, and as a percentage of `full`.
- **Verdict per ratio:** `within` when 0.90 ≤ r ≤ 1.10, `over` when r > 1.10, `under` when r < 0.90. The ±10% is two-sided, as §4 writes "unchanged". The tool never drops a case and never widens the band.
- **Recheck:** every case with any `over` or `under` gets a second interleaved pass set (P passes, `COMPARE_RECHECK_REPS` reps) of all three measurements. Its figures are printed as `recheck` lines, and **the case's final verdict is the recheck's**. The first-pass lines stay in the report. Recheck happens once, never repeated until green.
- **Work checks** (per case, every pass; any failure exits 1 naming the case and the fields):
  - the `ref` status is `complete`, the `full` status `complete`, the `fill` status `incomplete`;
  - all three report the same winning attempt;
  - `ref` draws equal `fill` draws (the like-for-like proof);
  - `full` draws equal the current golden's total draws for the case (read the way `measure.sh` reads them).
- **Report** (stdout and `build/measure/compare.txt`):
  - provenance lines: `compare.ref <commit> <cc --version first line>`; `compare.current <commit>[ dirty]`; `compare.cpu <model>`; `compare.passes <P> reps <R> recheck-reps <R2>`;
  - per case: `compare.<case>.ref <us> us`, `compare.<case>.full <us> us ratio <r> <verdict>`, `compare.<case>.fill <us> us ratio <r> <verdict>`, `compare.<case>.post-fill <us> us share <pct>%`, and for a recheck the same three lines with `.recheck` after the case name;
  - summaries, on final verdicts: `compare.full.summary cases <n> mean <r> min <r> <case> max <r> <case> within <n> over <n> under <n>` and the same for `compare.fill.summary`; then `compare.full.over <cases or none>`, `compare.full.under …`, `compare.fill.over …`, `compare.fill.under …`.
- **Exit:** 0 whatever the figures; 1 when a measurement cannot be taken (missing driver, a work-check failure, a failed self-check); 2 on a usage error.
- **Self-check, before every real run** (as `no_sdl.sh` proves itself on a planted case): fake drivers (small scripts written to a temporary directory) print `case` lines with planted medians that vary by call. The script runs its own comparison on them and must find:
  - a case at 1.05 → `within`;
  - a case at 1.20 → `over` that the recheck turns into `within`;
  - a case at 0.80 → `under`;
  - the minimum-of-medians figure, not the last or the mean;
  - the rotation of the order;
  - a run with planted mismatched draws → exit 1.
  If any expectation fails: `measure-compare: self-check failed: <what>`, exit 1, nothing measured.

### How the step note records §4's comparison (fixed form)

The step note (written by `plan-pilot` from this step's output) holds, in this order:

1. **Provenance:** both commits, CPU, compiler, `COMPARE_*` values, and the date. Also the `make measure` report's §6, §7, §8 and §9 lines for the current tree, saved to `tmp/` with the path given.
2. **§4 as written:** "`full` vs `40314c3`, interleaved, 44 shared cases: within N, over N, under N; mean r (min r case, max r case)". It gives every `over` and `under` case with its first-pass and recheck ratios, and lists the six new cases as having no reference. Verbatim from the tool's summary lines, never rounded into a better verdict.
3. **Like for like:** the same line for `fill` vs `40314c3`, plus "draw counts identical on all 44 (the tool's work check)".
4. **Post-fill share:** for each `full` case that is `over`, its `post-fill` µs and share.
5. **Judgement**, one of three outcomes, picked mechanically from 2–3:
   - (a) **No `full` case is `over`:** "§4's per-case budget is met." Any `under` is listed as a speed-up, with no further claim.
   - (b) **Some `full` cases are `over`, and no `fill` case is `over`:** "§4's per-case budget is **not met as written** on <cases>. The work that existed at `40314c3` is unchanged within ±10% (like-for-like column). The excess is the post-fill draws (<µs, share> per case), which Phase 1 steps 1.2 and 1.4 added on purpose: they are VT's own work in `Randomizer::randomize` and `writeToRom`, needed for the byte-for-byte match that is goal 1's proof. They are neither the write log nor its diff, so §4's exception does not cover them. §4's ±10% figure and its exception are left unchanged. Whether §4 accepts this deliberate growth is **the owner's decision**, listed for the final step note with these figures." The same sentence goes to `notes/` as a finding (one bullet in a new `notes/phase4-performance.md`), so that the plan's final note and the owner find it.
   - (c) **Some `fill` case is `over` after its recheck:** a like-for-like regression. That is a bug: the step is not done. It goes to `fix-bug` with the case, both ratios and the `measure-compare` command as reproduction. After the fix, `measure-compare` runs again and the note records both runs.
   No outcome edits §4, the ±10% band, a case list, a statistic or a default after seeing the figures. A changed default (passes, reps) applies to the whole run and is stated with its reason before the run.

### How the step note records the Cortex-A7 estimate and the decision

- The slowest golden case (§6's `max-case`), its host `-O2` generation median, its pipeline median, and the §9 projections. The sweep median, max and attempt histogram from §7 (H-4.3-4).
- **Decision rule, stated before the numbers are read:** if `miyoo.time.projected.wait` (upper factor plus SD allowance) is ≤ 10 000 ms, Phase 3 step 5's decision ("no per-stage progress indicator, one static Generating screen") is **confirmed**. If it is above, the decision is **reopened**: the note says so with the figures, `notes/phase4-performance.md` records it, and the indicator is left to a plan amendment (out of scope here). This step changes no UI.
- The step 3.5 baseline next to it ("16.2 ms, ~0.8–2 s, ~2–3 s with SD I/O"), so that "if the numbers moved" can be read off directly.

### Documentation

- `CLAUDE.md` § Checks: the `measure` bullet keeps its text and gains the pipeline and device-projection sections. A new bullet: "`make -C tests/rando measure-compare`, outside the net, times the current generator against a throwaway worktree of `40314c3` (`tmp/measure-ref-40314c3`), interleaved per case, full and stopped after `fill`, with ±10% verdicts; a reference that does not build is a `setup:` line."

## Implementation Chunks

1. **Driver modes and `make measure` sections.** `cases-until`, `assemble` (the driver now links the synthetic-base builder), the corrected baseline warning, the stop-after-fill and pipeline self-checks, §8 and §9. Done when `make -C tests/rando measure` exits 0 with the new sections, the false warning is gone and both self-checks pass. `make check` green.
2. **`measure-compare`.** The script with its self-check, the Makefile target and worktree handling, the root pass-through and help line, and `CLAUDE.md`. Done when `make -C tests/rando measure-compare` runs end to end on this machine, a planted failure of each self-check expectation makes it exit 1 (checked once by hand, then reverted), a dirty or wrong-commit `REF_DIR` is refused, and `make check` is green. Needs 1 (`cases-until`).
3. **The measurement and its record.** On one machine, in one session, back to back: `make -C tests/rando measure` (`MEASURE_REPS=21`), then `make -C tests/rando measure-compare` with its defaults. Save both reports under `tmp/`. Write `notes/phase4-performance.md` only if judgement (b) or a reopening applies, and hand the note's content (the fixed form above) to `plan-pilot`. Judgement (c) sends the regression to `fix-bug` before this chunk ends. No code in this chunk unless (c).

Chunk 1 needs nothing new, 2 needs 1, and 3 needs 2.

## Testing Decisions

- A good test here checks what the tools report and refuse, through their command lines and output lines. It does not check internal variables. The figures themselves are not testable (they depend on the machine), and no test asserts a time.
- **Driver modes:** tested by `make measure`'s own self-checks on real goldens. The stop-after-fill draw count must equal the golden's draws through `fill`, and the pipeline must return ok with one 2 MiB image. This follows the existing golden-parser self-check in `measure.sh`, which dies on a mismatch. No unit test is added under `tests/rando/unit/`: the driver is measurement tooling, not library code. A unit test would add a pthread and timing binary to `make test`.
- **`measure_compare.sh`:** tested by its self-check on planted fake drivers, run before every real measurement (prior art: `tests/rando/no_sdl.sh` and `no_literals.py`, which prove themselves on planted sources). It is kept out of `make test` on purpose, because the net does not run `measure-compare` and CLAUDE.md's `make test` list stays as it is. Chunk 2 also checks once by hand that each planted expectation, when broken, fails the self-check.
- **Not tested, on purpose:** the Cortex-A7 factor and the SD allowance (H-4.3-1, H-4.3-2). They are stated assumptions, and the owner's device timing replaces them.
- `make test`, `e2e`, `e2e-combos` and the rest of `make check` are unchanged and must stay green. No golden, claim, case or expected value changes.

## Out of Scope

- Any optimisation of the generator. Judgement (b) leaves the decision to the owner, and (c) goes through `fix-bug`, not this step.
- A progress indicator or any other UI change (a reopening is recorded, not implemented).
- Device or emulator runs (CLAUDE.md: never launch the game or an emulator). `qemu-arm` timing is not used as a device figure.
- `data/base.bps`, the cross-built package, the release zip, Phase 4 step 2's gate and its `e2e-arm` reuse.
- The `e2e-combos` cases (§4 budgets `e2e`'s cases), and the six step 1.3 e2e cases, which have no reference.
- The DS-era size, stack and time budgets in `measure.sh` (informational since step 1.3), kept as they are.
- Making CI run `measure` or `measure-compare` (times on CI runners are not comparable with the reference machine).
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): the fixed form above (provenance, §4 as written, like for like, post-fill share, judgement (a), (b) or (c)); the Cortex-A7 estimate and "confirmed" or "reopened"; the outcome of H-4.1-1 … H-4.1-3 and H-4.3-1 … H-4.3-4. Also a request to the owner, to answer in the final step note once `data/base.bps` exists and Phase 4 step 2's package is on the card: time one generation on the Mini Flip with the slowest case's settings (`item_placement-basic`, unless §6 names another), from pressing Generate to the success screen. That figure replaces H-4.3-1 and H-4.3-2.
- **For the final step note:** §4's per-case item is decided on this step's report, or on a re-run of `make measure-compare` at the end of the plan, if any generator code changed after this step. Under judgement (b), the owner's answer on accepting the post-fill growth goes there.
- Why the like-for-like column is fair: `rando_generate_until(…, fill)` skips exactly the three pre-winnability post-fill stages and the `writeToRom` stages. It still runs the winnability check, which `40314c3` also ran, and the work check proves equal draws. What remains between `fill` and `ref` is any change to code that existed at `40314c3`.
- Why the minimum of medians: on a shared host, interference only ever adds time, so the smallest per-run median is the closest to the code's own cost. Using it for both trees keeps the ratio fair. Steps 1.3 and 1.4 used the same idea.
- The tool's runtime at its defaults is about 44 cases × 6 passes × 3 measurements × 21 reps, a few ms each: roughly two minutes, plus rechecks. It is acceptable outside the net.
