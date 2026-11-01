---
title: "Golden cases for the new settings and for combinations"
description: "Every golden case changes one setting from the baseline, so nothing proves the C generator matches VT for hints off, any heart beep, or the multi-setting combinations the app will let the user pick at once."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 3, copied from `docs/plan-1.md`)

> **Golden cases for the new settings and for combinations.**
> - Add cases for hints off and each heart-beep value. Add enough multi-setting combinations to cover what the UI allows at once: at least every state × every glitches value, dungeon_items full with every state, and triforce-hunt and ganonhunt with every state.
> - Only the single-setting cases join `e2e`. The combination cases go into a separate target, `make -C tests/rando e2e-combos`, added to the root `make check` next to `e2e` (`test` and `e2e` unchanged), so every later step's net guards the combinations.
> - List every combination where the C generator returns `RANDO_UNSUPPORTED`, and either port it or record it as a UI restriction (Phase 3 greys it out).
> - Commit the case list and goldens on their own.
>
> Verified by:
> - the goldens exist, and every new case is green or listed as unsupported; a combination that is `DIVERGENT` at its claimed stage is a bug, fixed in this step before step 4;
> - `e2e`'s time grows only by its new single-setting cases (budget: per-case time unchanged ±10%, measured with `make -C tests/rando measure` against step 1's commit as above). The per-case time is the only `measure` figure this plan gates on: the size, stack and time budgets `measure.sh` inherits from `alttp-ds` (DS targets) are informational, and an `over` on them is recorded in the step note, not a failure.

## Step 2's outcome (hypotheses checked at `81e02d3`)

This PRD was written while step 2 ("The oracle runs the post-fill code and records the write log") was still running, against its PRD (`docs/prd/triage/phase1-step2-oracle-post-fill-write-log.md`). Step 2 is now done, and its note is the plan's §6 "Step 1.2". Each hypothesis below was checked against the committed tree at `81e02d3`. Its label is kept so that the decisions that cite it still point here.

- **H-2-1 (corrected)**: `stages.txt` lists **seventeen** stages: the ten pre-fill stages, then `setTexts`, `randomizeCredits`, `applyHints` (`getHint`'s draws included), `writeItem`, `setDiggingGameRng`, `writeRNGBlock` and `writePrizePacksToRom`. The last one is `getDropsPool`'s `fy_shuffle` of the 63 drops. It is drawn in every case, as the last stage, and neither the plan nor step 2's PRD expected it. `writeItem` draws only under `dungeon_items` `mc`, `mcs` or `full`. `fill` owns a `section` check for every spoiler section no earlier stage checks, and `check fill attempts`. So a `fill` claim compares the trace through attempt 0's last `fill` draw, all of `spoiler.json`, and `attempts`.
- **H-2-2 (holds)**: every one of the 44 claims in `claims.txt` is `<case> fill`, and `e2e` is green.
- **H-2-3 (holds)**: a stop at `fill` or later retries up to the winning attempt. `rando_gen` then writes `trace.txt`, `attempts`, `spoiler.json` with its playthrough, and `pool.txt`.
- **H-2-4 (holds)**: `run_e2e.sh` still refuses (exit 2) a stage claim whose stage makes no draw in the case's attempt 0. A claim owes every earlier stage's checks except `pool`, which only a claim at `prepareWorld` owes. `RANDO_CASES`, `RANDO_CLAIMS`, `RANDO_STAGES` and `RANDO_GEN` still override its inputs.
- **H-2-5 (holds)**: per named case, `gen_golden.sh` writes `trace.txt`, `attempts`, `spoiler.json`, `rom_writes.txt` and `rom_buffers.txt` (through `gen_case.php` and `oracle_rom.php`). On every run it writes the logic fixtures and `text_default.txt` (`text_default.php`). It writes `pool_lists.txt` and `prepared_pools.txt` always over every case of `cases/`. `--out <dir>` puts all of these under `<dir>`, with case `<name>` at `<dir>/<name>`. Only `cases/` is read today.
- **H-2-6 (holds)**: three regeneration runs gave identical files (php 8.3.6, Composer 2.8.12, VT `219fcafd`).
- **H-2-7 (holds)**: the oracle throws in each of these cases:
  - on a draw outside `randomize` with no trace label (`vt.patch`);
  - on a `Rom::write` outside `writeToRom` with no write label (`oracle_rom.php`);
  - on a failed self-check: the log against `getWriteLog()`, or the text and SRAM setter logs against their writes.
- **H-2-8 (holds, checked in VT)**: with hints off, `HintService::applyHints` only calls `setText('sign_north_of_links_house', …)`, then skips the world. So `hints-off`'s attempt 0 has no `applyHints` draw. The heart beep makes no draw, and every `rom_writes.txt` ends with the `180033 <byte> setHeartBeepSpeed` line.
- **H-2-9 (holds)**: `dump_pools.php`'s record lists `CONFIG_KEYS`, the eleven option keys, and never `hints` or `heart_beep`. `test_generate --fixtures` replays each record, reading `cases/<name>/trace.txt`.
- **H-2-10 (holds)**: nothing in `rando/` returns `RANDO_UNSUPPORTED`. Only `rando_gen` and `rando_measure` map it to an exit status.

Facts at `81e02d3`:
- the 44 cases all use seed `1` and have `attempts` `0`;
- `test_case_reader` requires at least 44 readable cases in `cases/`;
- `measure.sh` and `run_e2e.sh` discover every directory of `cases/` that holds a `config.json`;
- the reference measure is in `tmp/measure-ref-40314c3.log`, and its numbers are in step 1's note;
- a golden case is about 72 KB of `trace.txt`, 22 KB of `spoiler.json`, 48 KB of `rom_writes.txt` and 5 KB of `rom_buffers.txt`.

## Problem Statement

The owner will pick settings on the console, several at once: an inverted seed with overworld glitches, a standard seed with full dungeon items, a triforce hunt in retro. The only proof that a seed matches alttpr.com is the golden cases. Today, every one of the 44 cases changes exactly one setting from the baseline.

So no case shows that the C generator matches VT when two settings interact, for example:
- the inverted world's glitch logic;
- the retro world's key counts under an expert pool;
- glitched logic forcing vanilla dungeon items;
- the triforce pieces in a standard escape.

These are exactly the places where a port goes wrong silently. The two settings step 2 added to the case format, hints off and the heart beep, have no case at all, so steps 4 and 11 would port them with nothing to compare against. Finally, nobody has listed which combinations the C generator cannot produce. Phase 3 needs that list to grey them out, so the app never writes a ROM that differs from VT.

## Solution

The case list gains:
- six single-setting cases: hints off, and one case per heart-beep value;
- twenty-three combination cases: every state × every glitches value, full dungeon items in every state, triforce hunt and ganonhunt in every state, plus two pairs that the C code derives together.

The VT oracle generates their goldens, which are committed alone. The single-setting cases join `e2e`. The combinations live in their own directory and run through a new `e2e-combos` target that `make check` runs, so every later step is guarded against breaking a combination.

Every new case is claimed at `fill`, the stage every case is held at until step 4. A combination that diverges is a bug, fixed in this step. A combination the C generator cannot produce (because VT itself cannot, or because it needs something the plan leaves out) makes the generator return `RANDO_UNSUPPORTED`. It is recorded as a UI restriction for Phase 3. `e2e`'s per-case time is re-measured against step 1's commit, and the step note records the case list, the unsupported list and the numbers.

## User Stories

1. As the owner, I want a golden for every state with every glitches value, so that an inverted or standard seed with glitches is proven against VT, not assumed from the open-world cases.
2. As the owner, I want a golden for full dungeon items in every state, so that keysanity in standard, inverted and retro matches alttpr.com.
3. As the owner, I want goldens for triforce hunt and ganonhunt in every state, so that the triforce-piece goals work in every world.
4. As the owner, I want a golden with hints off, so that turning hints off on the console is proven against VT once step 4 ports the hints.
5. As the owner, I want a golden for each heart-beep value, so that step 11 proves the heart-beep byte for each choice the app offers.
6. As the owner, I want the combinations guarded by `make check`, so that a later step cannot break a combination without the net catching it.
7. As the owner, I want the list of combinations the console cannot generate, so that the app greys them out instead of writing a ROM that differs from VT.
8. As the owner, I want each unsupported combination to carry its reason, so that I can judge whether the restriction is acceptable.
9. As the owner, I want the generator itself to refuse an unsupported combination, so that no caller, UI bug or saved settings file can produce a wrong ROM.
10. As a porting agent, I want the combination cases kept apart from `e2e`, so that `e2e`'s output, time and budget stay those of the single-setting matrix.
11. As a porting agent, I want `make -C tests/rando e2e-combos` to print the same `PASS`, `DIVERGENT` and `PENDING` lines as `e2e`, so that I read both the same way.
12. As a porting agent, I want a combination's case name to list the settings it changes, so that a `DIVERGENT` line tells me which options interact.
13. As a porting agent, I want every combination claimed at `fill` like the other cases, so that step 4 moves every claim, combinations included, through the post-fill stages the same way.
14. As a porting agent, I want each divergence fixed by its own `fix-bug` run and commit, so that each fix has its proof and can be reverted alone.
15. As a porting agent, I want a combination that VT itself cannot generate recorded as unsupported, not forced, so that the C port never invents behaviour VT does not have.
16. As a case author, I want combinations to use the same files as the other cases (`config.json`, `seed`, goldens), so that the oracle, the case reader and `run_e2e.sh` need no second format.
17. As a case author, I want combinations to keep the baseline for every setting they do not name, so that a case differs from its single-setting cases only in the pair it tests.
18. As a case author, I want every combination to be one alttpr.com's own UI allows, so that no golden covers a combination the user can never pick.
19. As a case author, I want `gen_golden.sh` to regenerate a combination by name like any case, so that the oracle has one entry point.
20. As a case author, I want a case name to be unique across the single-setting and combination directories, so that `gen_golden.sh <name>` and every report are unambiguous.
21. As the net, I want `make test` unchanged in speed and content, apart from reading the new cases, so that the fast loop stays fast.
22. As the net, I want `e2e`'s per-case time on the 44 existing cases unchanged within ±10% against step 1's commit, so that the plan's budget holds.
23. As the net, I want the two measurements taken in the same session on the same machine, so that the comparison is meaningful.
24. As the net, I want `measure`'s inherited DS-era budgets only recorded, so that an informational `over` does not block the step.
25. As the net, I want the goldens of the 44 existing cases byte-identical after this step, so that adding cases changes nothing already proven.
26. As the net, I want the oracle fixtures unchanged, except for the pool fixtures gaining the new single-setting cases' records, so that no seed-independent fact moves.
27. As the net, I want two `gen_golden.sh` runs over the new cases to give identical files, so that the new goldens are facts.
28. As the net, I want the case list and goldens committed alone, so that they can be inspected and reverted alone.
29. As the net, I want a unit test that fails when a required combination disappears, so that the coverage this step promises cannot erode silently.
30. As the net, I want `test_case_reader` to read every combination case, so that a malformed `config.json` there fails the fast loop.
31. As the net, I want `make check` green after every commit of this step, so that `check-net` never sees a red tree in the middle of the step.
32. As a reviewer, I want the step note to list every new case, its claim and its result, so that I can check the step without running it.
33. As a reviewer, I want the step note to list the alttpr.com UI coupling rules, so that Phase 3 has them next to the unsupported list.
34. As a reviewer, I want `CLAUDE.md` § Checks to name `e2e-combos`, so that the documented net matches the Makefile.
35. As Phase 3, I want one C predicate that says whether a set of options is supported, so that the Generate tab greys out exactly what the generator refuses (only if the list is not empty).
36. As a future agent, I want a seed changed from `1` only for a stated reason, so that no seed is picked to hide a divergence.

## Implementation Decisions

### The new single-setting cases (in `cases/`, joining `e2e`)

- Six cases, each the baseline (`config.json` with the eleven keys at the baseline values, seed `1`) plus one key:

  | Case | Extra key |
  |---|---|
  | `hints-off` | `"hints": "off"` |
  | `heart_beep-off` | `"heart_beep": "off"` |
  | `heart_beep-half` | `"heart_beep": "half"` |
  | `heart_beep-quarter` | `"heart_beep": "quarter"` |
  | `heart_beep-double` | `"heart_beep": "double"` |
  | `heart_beep-normal` | `"heart_beep": "normal"` |

- `heart_beep-half` writes the default explicitly. It checks that the explicit key is read the same way as its absence. The plan asks for "each heart-beep value", and the case is cheap.
- Up to the fill, the six cases make the same draws as `baseline` (H-2-8). Their goldens differ from `baseline`'s as follows:
  - `hints-off`: after the fill. It has no `applyHints` draw, so the later stages' draws are renumbered, and its texts and writes differ.
  - the heart-beep cases: only in `rom_writes.txt`'s last line. Today, a `fill` claim on them passes wherever `baseline` passes. They prove something from step 4 onwards (`applyHints`, skipped for `hints-off` by step 4's rule for a stage with no draw), and from step 11 onwards (the heart-beep group).

### The combination cases (in a new `combos/` directory, beside `cases/`)

- A combination case has the same files as a single-setting case: `config.json` (the eleven keys, written out in full, in the same order and layout as the existing cases), `seed` (`1`), and the oracle's goldens.
- Name: the settings it changes from the baseline, each written `<key>-<value>` as in the single-setting names, in `rando_options.h`'s key order, joined by `+`. For example: `glitches-major_glitches+state-inverted`, `dungeon_items-full+state-standard`, `goal-ganonhunt+state-retro`.
- The list, 23 cases. A pair that a single-setting case already covers is not repeated: `open` is the baseline state, so `glitches-*`, `dungeon_items-full`, `goal-triforce-hunt` and `goal-ganonhunt` are the `open` row.

  | Group | Cases | Count |
  |---|---|---|
  | state × glitches | `glitches-{overworld_glitches,hybrid_major_glitches,major_glitches,no_logic}+state-{standard,inverted,retro}` | 12 |
  | full dungeon items × state | `dungeon_items-full+state-{standard,inverted,retro}` | 3 |
  | triforce goals × state | `goal-{triforce-hunt,ganonhunt}+state-{standard,inverted,retro}` | 6 |
  | derived together in C | `glitches-major_glitches+dungeon_items-full`, `state-retro+item_pool-expert` | 2 |

- Why the two extras (the plan's "at least" leaves room; these are the only extras):
  - `glitches-major_glitches+dungeon_items-full`: the world derives `rom.vanilla*` from "no wild items and glitched logic". No case has both at once, so the branch where glitched logic meets wild items is untested.
  - `state-retro+item_pool-expert`: the retro key and rupee counts branch on the hard and expert pools. Only `state-retro` with the normal pool is tested today.
- The `dungeon_items-full+state-*` cases and `glitches-major_glitches+dungeon_items-full` are also the only cases outside the open world that draw in `writeItem` (H-2-1). Step 4 ports that stage, and these cases guard it.
- Every combination respects alttpr.com's UI coupling rules (VT `resources/js/store/modules/randomizer.js` at `219fcafd`): crowd control forces advanced placement; the `dungeons` goal forces Ganon's crystals to 7; `completionist` forces `locations` accessibility. The other coupling rules concern entrance shuffle, which is out of scope. None of the 23 cases touches these settings, so all are reachable on alttpr.com. The step note copies these rules for Phase 3.
- Seed: `1`, as for every case. A combination whose attempt 0 makes no `fill` draw cannot be claimed at `fill` (H-2-4). This happens only when VT's attempt 0 fails before the fill, in `fillPrizes`. For such a case, its seed becomes the smallest seed `≥ 2` whose attempt 0 reaches the fill, and the step note says so. A seed is never changed for any other reason, and never to hide a divergence.

### `gen_golden.sh` reaches the combinations

- A case name names a directory of `cases/` or of `combos/`. A name present in both is an error (exit 2, `gen_golden: case in both cases/ and combos/: <name>`), and so is a name in neither (today's `no such case`).
- With no case named, it regenerates every case of both directories.
- Without `--out`, a combination's goldens are written back into `combos/<name>/`. With `--out <dir>`, they go to `<dir>/combos/<name>`, while single-setting cases stay at `<dir>/<name>` (H-2-5). A `diff -r` of `<dir>` against `tests/rando` then compares like with like.
- `dump_pools.php` still runs over `cases/` only. The pool fixtures stay the single-setting matrix's, which `test_generate --fixtures` replays from `cases/<name>/trace.txt` (H-2-9). Combinations are proven by `e2e-combos`, not by `logic`.
- `discover_cases` is reused unchanged, once per directory. Its usage comment does not change.

### `e2e-combos`

- `tests/rando/Makefile` gains `e2e-combos`, which depends on the same generator as `e2e`. It runs `run_e2e.sh` with `RANDO_CASES` set to `combos/` and `RANDO_CLAIMS` set to a new claims file, `combo_claims.txt`. Both overrides already exist, so `run_e2e.sh` itself is unchanged.
  - The target first prints one line, `e2e-combos: the combination cases`, so that the two `e2e:` summaries in `make check`'s output can be told apart.
  - With no combination directory, it prints `e2e: no cases` and passes. This is the state at chunk 1.
- `combo_claims.txt` has the same format and header comment as `claims.txt`, and is the only claims file for `combos/`. A combination that is not claimed is `PENDING`, which does not fail.
- The root `Makefile` gains `e2e-combos` (delegating to `tests/rando`), lists it in `help`, and puts it in `check` right after `e2e`: `check: test lint logic selftest e2e e2e-combos`. `test` and `e2e` are unchanged.
- `CLAUDE.md` § Checks: the `make check` line names `e2e-combos` ("every combination case diffed against its golden, kept out of `e2e`").
- `measure` and `e2e-arm` keep reading `cases/` only. `measure` thus times the single-setting matrix, which is what the budget covers. The qemu run of the combinations belongs to step 12 and the end of Phase 2.

### Claims

- `claims.txt` gains `<case> fill` for each of the six single-setting cases.
- `combo_claims.txt` holds `<case> fill` for every combination that is green.
- A claim is added only when its case is green, in the commit that makes it green, so that `make check` stays green at every commit.
- An unsupported combination is not claimed. A comment line in `combo_claims.txt` names it and points to the step note.

### Divergences and `RANDO_UNSUPPORTED`

- Expected starting point: no combination returns `RANDO_UNSUPPORTED` (H-2-10). The list is "none" unless this step adds one.
- A combination that is `DIVERGENT` under its `fill` claim is a C bug. It goes through one `fix-bug` run, one at a time, and the earliest differing stage is fixed first. That run commits the fix, its failing unit test, and the claim line.
- A combination is recorded as **unsupported** instead of fixed only in one of these two cases:
  - (a) The oracle cannot generate it: VT throws inside `randomize`, or gives up after the watchdog. alttpr.com would fail too, so the app must not offer it. Its case directory is not committed (with no golden, every full `gen_golden.sh` run would die on it); the coverage guard names it as an explicit exception.
  - (b) Matching VT needs a subsystem the plan puts out of scope (§2 Out of scope).

  Anything else, however large, is ported. If porting turns out to need more than one `fix-bug` cycle can hold, the implementer asks a `guideline` agent, giving it the plan file and the divergence. They record its decision in the step note.
- If at least one combination is unsupported, the generator refuses it itself:
  - `rando_options.h` gains one predicate, "these options are supported";
  - `rando_generate_until` returns `RANDO_UNSUPPORTED` before any draw when it says no;
  - `rando_gen` already maps that status to exit 4;
  - Phase 3's Generate tab will call the same predicate.

  The predicate's contract names each unsupported combination. If the list is empty, no predicate is added.
- A combination for which the oracle dies because of the oracle itself (H-2-7: an unlabelled draw, an untagged write, a failed self-check) is not unsupported. It is an oracle gap. The fix goes in `vt.patch` or the oracle's PHP, in its own commit, before the goldens commit. That commit must leave every committed golden byte-identical (checked with a `--out` run and `diff -r`). Otherwise it is a regeneration in its own right, which needs its own commit and stated reason, and the step stops to record it in the note.

### The coverage guard

- `test_case_reader` gains one check that reads every `config.json` of `cases/` and `combos/`. It fails, naming the missing pair, unless:
  - every (state, glitches) pair is present;
  - `dungeon_items` `full` is present with every state;
  - `goal` `triforce-hunt` and `goal` `ganonhunt` are each present with every state.

  Each of these pairs must come from a case that changes nothing else from the baseline but `hints`/`heart_beep`. An unsupported combination of case (b) still counts, since its directory stays; one of case (a) has no directory and is listed in the test as a named exception, with a pointer to the step note.
- The same test reads every combination case (`rando_case_read`) and fails on the first unreadable one. It fails if a name appears in both directories, and requires at least 50 single-setting cases and 23 combination cases, minus the named case (a) exceptions. Raising the lower bound from 44 to 50 is deliberate: the matrix grew.

### Measure and the step note

- The budget check follows the plan's method. A throwaway worktree of step 1's commit (`40314c3`) is made under `tmp/`, and `make -C tests/rando measure` runs there and in the current tree, in the same session, one after the other, with the same `MEASURE_REPS` and `MEASURE_SWEEP`.
- The gate: for each of the 44 cases common to both, the current host `-O2` median is within ±10% of the reference median measured in this session.
  - If a case is outside, both trees are measured once more, and the second pair decides. A case still outside is investigated as a regression, not waived.
  - The six new cases' times are recorded with no gate. They are expected to equal `baseline`'s within noise.
- The plan's §6 gains a "Step 1.3" note:
  - the new cases and combinations, with each one's claim and result;
  - every seed other than `1`, with its reason;
  - the unsupported list (expected: none, because no C code returns `RANDO_UNSUPPORTED`), each entry with its reason and its UI restriction;
  - the alttpr.com UI coupling rules above, for Phase 3 step 4;
  - the PHP, composer and VT versions of the golden run, and the reproducibility evidence;
  - both measure runs (reference and current), per-case medians, and any `over` on the inherited budgets, recorded as informational.

## Implementation Chunks

1. **`e2e-combos` and `gen_golden.sh` over two directories.**
   - `combo_claims.txt` with its header and no claim.
   - The `e2e-combos` target in `tests/rando/Makefile` and the root `Makefile` (`check`, `help`).
   - `CLAUDE.md` § Checks.
   - `gen_golden.sh` resolving names in `cases/` and `combos/`, with the `--out <dir>/combos/` layout and the duplicate-name error.

   No case is added. `make check` is green, and `e2e-combos` prints `e2e: no cases`.
2. **The case list and its goldens, committed alone.** Write the 29 `config.json` and `seed` files.
   - Run `gen_golden.sh --out tmp/<a>` and `--out tmp/<b>` over every case (`VT_DIR` under `tmp/`); `diff -r` must be empty.
   - Diff `tmp/<a>` against the committed tree. Each of the 44 existing cases' files is byte-identical; every oracle fixture is identical, except that `pool_lists.txt` and `prepared_pools.txt` gain exactly the six new records.
   - A case the oracle dies on follows the rules above: an oracle gap gets its own commit first; a VT failure is recorded unsupported, case (a).
   - Install the goldens: the new case directories and the two pool fixtures, and nothing else, in one commit.

   The new cases are not claimed yet, so they are `PENDING`, and `make check` is green (`logic` replays the six new pool records, which are `baseline`'s pools).
3. **Claims, reader checks and the green combinations.**
   - Claim the six single-setting cases at `fill`, and every combination that passes in `combo_claims.txt`.
   - Add the `test_case_reader` checks (combination cases read, unique names, lower bounds, coverage guard).

   `make check` is green. Every `DIVERGENT` combination is listed, ranked by its earliest differing stage, for chunk 4.
4. **Fix each divergent combination.** One `fix-bug` run per combination, earliest stage first. Each run commits the fix, its unit test, and the claim line.
   - If a combination is recorded unsupported, case (b): add the predicate, its unit test, the `RANDO_UNSUPPORTED` return, and the comment line in `combo_claims.txt`, in one commit.
   - If no combination diverged in chunk 3, this chunk is empty and is recorded as such.
5. **Measure and the step note.** Run both measures as above, apply the ±10% gate, and write §6 "Step 1.3".

## Testing Decisions

- A good test drives the public interface and checks observable output. Here that means:
  - `run_e2e.sh`'s lines and exit status through the two make targets;
  - `rando_case_read`'s result on the committed files;
  - `rando_generate_until`'s status.

  No test inspects generator internals.
- **`test_case_reader`** (prior art: `check_every_case`, which already walks `cases/`): the combination walk, the unique-name check, the raised lower bounds, and the coverage guard. Its failure messages name the directory, the case or the missing pair. It runs in `make test`. Reading about 75 small files keeps the fast loop fast.
- **Divergence fixes** (chunk 4) each come with the unit test `fix-bug` requires, in the module the fix touches (prior art: `test_world`, `test_prepare`, `test_filler`, `test_generate`'s fixture-driven checks). Each is also proven by its case moving to `PASS` under `e2e-combos`.
- **The unsupported predicate**, only if it is added: a `test_options` (or `test_generate`) test.
  - Every recorded combination is refused, and `rando_generate_until` returns `RANDO_UNSUPPORTED` with no trace callback made.
  - The baseline and every claimed case are accepted.
- **`e2e-combos`** has no selftest of its own. It is `run_e2e.sh` with two overrides that `selftest.sh` already exercises (`RANDO_CASES`, `RANDO_CLAIMS`). Its proof is the target's output: no cases at chunk 1, then `PASS` lines.
- **The oracle and `gen_golden.sh`** stay outside the net (CLAUDE.md). The goldens are proven by two runs giving identical files, by the unchanged 44 cases and fixtures, and by the oracle's self-checks (H-2-7).
- `lint` covers any `rando/` change (the predicate, the fixes).

## Out of Scope

- Porting the post-fill draws or any ROM write (steps 4 and 6 to 11). Every new case is held at `fill`, like the others.
- Write-log claims and `check-e2e`'s handling of `e2e-combos` (step 5). The run of `check-e2e` over `e2e-combos` (step 12).
- Running the combinations under qemu-arm (step 12, the end of Phase 2).
- Making hints a twelfth `RandoOptions` setting; the Generate tab and its greying out (Phase 3 step 4).
- Combinations involving entrance shuffle, Enemizer, door shuffle, custom or tournament settings (§2 Out of scope).
- A pairwise or exhaustive matrix over all eleven settings: the plan sets the floor, and the two extras are justified by code that derives settings together.
- Measuring the combination cases: the budget covers `e2e`'s single-setting matrix only.

## Further Notes

- **Size.** Each case adds about 150 KB: a 72 KB trace, a 22 KB spoiler, a 48 KB `rom_writes.txt` and a 5 KB `rom_buffers.txt` (the sizes at `81e02d3`). The 29 new cases therefore add about 4.3 MB of goldens. This is accepted: the goldens are the source of truth.
- **For step 4.** Step 4's "every case moving back to `all`" covers `combo_claims.txt` too. A combination whose winning attempt is not 0 skips the `writeToRom` stages, as the plan's step 4 rule says; since `81e02d3`, that includes `writePrizePacksToRom`.
- **For step 5.** `check-e2e` must learn `e2e-combos` (the plan's step 5 says so). `combo_claims.txt` is the second claims file it ranks.
- **For Phase 3 step 4.** The greyed-out list is the union of two sets: this step's unsupported list (with the predicate, if any), and alttpr.com's coupling rules, which the Generate tab should apply as VT's UI does (forcing the coupled value rather than refusing).
- No `guideline` agent was consulted. Every open point was settled from the plan, VT at `219fcafd`, the code at `81e02d3`, and step 2's note, and each is recorded above. Chunk 4 may consult one, as stated.
