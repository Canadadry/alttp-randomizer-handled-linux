---
title: "LADX case list: one case per tier-1 value, the combinations, and a fixed ok seed each"
description: "Phase 3 needs a committed, complete set of LADX cases (settings plus a seed LADXR accepts) to claim the C port against, and none exists yet."
status: needs-triage
---

## Problem Statement

Plan 3, Phase 2, Step 4, verbatim:

> 4. **Cases.** `tests/ladx/cases/` gets one case per tier-1 setting value (the other settings at LADXR's defaults), plus a baseline. `tests/ladx/combos/` gets combinations: at least every `logic` × `dungeon_keys`, every `goal` × `accessibility`, every `hardmode` × `hpmode`, and the keysanity-everything case. Each case has a fixed seed. Verified by the case list covering every tier-1 value (a script compares it with §2's list).

Today the LADX oracle only has hand-written lists for its own checks (`determinism_cases.txt`, 19 cases, and `side_seeds.txt`). Nothing says which settings the C port must be claimed on. Step 5 cannot write goldens until each case exists, and Phase 3 cannot claim the fill or the writes on every tier-1 value until each value has a case.

Three facts from the earlier steps shape this step:

- **Step 2.3 cut `goal=bingo*`** (`notes/plan3-scope-cuts.md`, `goal-bingo`). Those four values leave the tier-1 list (`tests/ladx/oracle/tier1.txt`) in this step, which changes the ROM-free fixtures generated from that list.
- **LADXR refuses or crashes on some seeds.** The fill can fail (`outcome error`; `main.py` would retry with a new seed: about 4 in 10 seeds under `itempool=pain`). `goal=maze` can raise `IndexError` in `buildMaze`, and the default text mode can raise `UnicodeEncodeError` in `reduceMessageLengths`/`getMarinStartingText` (`outcome crash`, step 1.8 and 2.2 notes). A case's seed must give `outcome ok` with the full generation and the ROM, or step 5 has no golden to write.
- **The strict Nintendo-bytes scope covers `tests/ladx/cases/` and `tests/ladx/combos/`**: a 4-byte run in any view, the hex view included. A random 16-byte seed written in hex has a measurable chance (a few tenths of a percent per seed) of matching 4 ROM bytes at one of its nibble alignments. Across about 120 seeds that is close to an even chance of at least one push refused by the gate. Settings text (`goal=instruments`) is cleared as `ladxr-public`; this was probed on 34 sample files, with 0 literal runs.

## Solution

The case list is generated, not hand-written. Its sources are `tier1.txt` (§2's list in machine form, with the bingo values removed), a small hand-written combination spec, and the committed settings fixtures. A ROM-free script checks the committed case directories against that list in `make test`.

Each case directory holds only its settings (`config.txt`) and its seed (`seed`). The seed is not chosen by hand. It is the first candidate of a fixed per-case derivation that LADXR accepts (`outcome ok` with the ROM) and that the strict Nintendo-bytes check clears. A ROM-backed command picks the seeds and another re-proves the choice.

`gen_golden.sh` runs the case directories by default and fails if a case is not `ok`. `determinism.sh --full` gains a mode that double-runs every case and combo, which closes step 2's "step 4 then extends it to every case".

Expected size: 89 cases (the baseline and 88 single values) and 27 combos. The combos are 8 `logic` × `dungeon_keys`, 6 `goal` × `accessibility` (the maze cell is dropped by LADXR's own `validate`), 12 `hardmode` × `hpmode` and 1 keysanity-everything.

## User Stories

1. As the plan pilot, I want `tests/ladx/cases/` to hold one case per non-default tier-1 value plus a baseline, so that step 5 can write a golden for every value.
2. As the plan pilot, I want `tests/ladx/combos/` to hold the combinations the plan names, so that cross-setting interactions are claimed too.
3. As a Phase 3 implementer, I want each case to have a fixed seed that LADXR accepts, so that every case has a golden and a claimable placement.
4. As a Phase 3 implementer, I want a case's settings in a plain `key=value` file, so that a C host reader and a shell loop can both read them without a JSON parser.
5. As a reviewer, I want the case list derived mechanically from `tier1.txt` and one combination spec, so that adding or cutting a tier-1 value changes the cases in one place.
6. As a reviewer, I want a ROM-free check in `make test` that fails on a missing, extra or wrong case, so that the case list can never drift from §2's list unnoticed.
7. As a reviewer, I want every seed to come from a published derivation, so that nobody can hand-pick a seed to dodge a bug.
8. As a reviewer, I want a ROM-backed verify command that proves each seed is the *first* acceptable candidate, so that the derivation rule is shown to have been followed, not just matched.
9. As the owner, I want the `goal=bingo*` values gone from the cases and from `tier1.txt`, with the fixtures regenerated in their own commit and the reason stated, so that the step 2.3 scope cut is applied exactly where the cut note says.
10. As the owner, I want the regenerated fixtures to differ from the old ones only by the removed bingo entries, so that the cut provably changes nothing else.
11. As the push gate, I want every committed seed file to pass the strict Nintendo-bytes check, so that no push of the case list is refused by a coincidental 4-byte match.
12. As a Phase 3 implementer, I want a combination cell that LADXR's `validate` would rewrite to be left out and named, so that no case claims a setting LADXR does not actually run.
13. As a Phase 3 implementer, I want every cell of each required cross product accounted for (baseline, single case, combo or validate-skipped), so that "every `logic` × `dungeon_keys`" is verifiably true.
14. As a later step (Phase 2 step 6), I want to add sampled combinations by appending lines to the combination spec, so that the checker stays exact and the list keeps one source.
15. As the case runner's user, I want `gen_golden.sh` with no `--cases` to run every case and combo directory, so that one command produces every case's outputs under `tmp/`.
16. As the case runner's user, I want `gen_golden.sh` to exit non-zero when a case directory's run is not `ok`, so that a bad seed cannot pass silently.
17. As the case runner's user, I want `gen_golden.sh --cases determinism_cases.txt` to behave as before, error cases included, so that the existing determinism and inventory tools are unchanged.
18. As the plan pilot, I want `determinism.sh --full --all` to double-run every case and combo (hash seed 0 vs 1, fresh clone, tagged work directory, plain CLI), so that step 2's determinism proof covers the whole case list.
19. As a debugger, I want case names derived from their settings (`goal-open-4`, `logic-hell+dungeon_keys-keysanity`), so that a failing case name says what it runs.
20. As a debugger, I want the picker to print, per case, which candidate won and what each refused candidate's outcome was, so that the refusals are visible evidence for the C port's own refusals.
21. As the owner, I want the step note to record the refusal counts by kind (fill error, maze crash, Marin crash, Nintendo-bytes) and the cases whose seed is not candidate 0, so that the seed choice is auditable.
22. As a maintainer, I want the picker never to overwrite an existing case's seed, so that a golden's seed cannot change by accident.
23. As a maintainer, I want the checker to refuse any file other than `config.txt` and `seed` in a case directory for now, so that step 5 extends the allowed set on purpose.
24. As a maintainer, I want the checker to refuse a case name present in both `cases/` and `combos/`, so that the flat output layout under `tmp/` is unambiguous.
25. As a CI user, I want the case-list check to need no ROM, no LADXR checkout and no network, so that it belongs in `make test`.
26. As a maintainer, I want the checker to prove itself on planted trees before it checks the real one, like the repository's other checks, so that a broken checker cannot report green.
27. As a reader of `CLAUDE.md`, I want the new case layout, the spec, the checker, the picker and the new runner modes described, so that later sessions find them.

## Implementation Decisions

### Scope cut applied (goal-bingo)

- `tier1.txt`'s `goal` line changes from `all` to an explicit list without `bingo`, `bingo-double`, `bingo-triple` and `bingo-full`, in LADXR's order. The file's header comment names them as excluded by the scope cut `goal-bingo` (`notes/plan3-scope-cuts.md`).
- `gen_fixtures.sh` regenerates the fixtures, then `gen_fixtures.sh --check` must pass. Expected changes: `logic_vectors.txt` and `item_pools.txt` lose their `goal=bingo*` configurations and cases. `settings.txt`, `settings_validate.txt`, `logic_graphs.txt` and `pyrandom_vectors.txt` do not change, because they are built from all of LADXR's settings or do not depend on the tier-1 list. The commit is the cut's own, and its message states the reason (the step 2.3 cut) and that the fixture diff is deletions only, except header text if any. Any non-deletion change is investigated before committing, not accepted.
- `determinism_cases.txt` keeps its `bingo` case. It is not a tier-1 case: it is the trace evidence `inventory_trace.sh`/`read_check.py` used for the cut, and the group still exists in LADXR.

### Case directory format

- `tests/ladx/cases/<name>/` and `tests/ladx/combos/<name>/`, mirroring ALttP's `tests/alttp/cases/` and `combos/` (one directory per case, discovered by a marker file).
- `config.txt` is the marker. It holds one `key=value` line per assignment, LADXR's setting names and rendered values (a bool as `0`/`1`), in LADXR's settings order, with no other content. The baseline's file is empty. Only non-default values appear, so `""` never occurs. Chosen over JSON (ALttP's `config.json` is VT's format) because LADXR settings are flat `key=value` pairs, `oracle_run.py` takes them as arguments as they are, and the Phase 3 C reader needs no JSON parser.
- `seed` holds 32 lowercase hex digits (16 bytes, the length every oracle case uses, LADXR's seed setting left empty as on the device) and a newline.
- No other file in this step. Step 5 adds the golden files and extends the checker's allowed set.

### Names

- A single case is `<key>-<value>` (keys never contain `-`, so the first `-` splits; values may: `goal-open-4`, `goal-5-8`). A combo joins its assignments' names with `+`, in LADXR's order (`logic-hell+dungeon_keys-keysanity`). The baseline is `baseline`.
- The keysanity-everything combo is named by the same rule: `dungeon_keys-keysanity+nightmare_keys-keysanity+dungeon_beaks-keysanity+dungeon_maps-keysanity+keyholesanity-1`. There is no alias table, so the checker can derive every name from its settings.

### The expected list (the "script that compares with §2's list")

- A new oracle module, `ladx_cases.py` (stdlib only, no LADXR import for its ROM-free commands), builds the expected list. Its inputs are:
  - `tier1.txt`, read through the existing `read_tier1`;
  - the committed `settings.txt` fixture (defaults, values, order), so no LADXR checkout is needed;
  - the committed `settings_validate.txt` fixture (pair rewrites);
  - a new hand-written spec, `tests/ladx/oracle/combos.txt`.
- Singles: the baseline, then every tier-1 value that is not LADXR's default, in `tier1.txt`'s order. A single-value case whose `validate` line in the fixture changes its own assignment is an error (none does at the pin).
- `combos.txt` line kinds, with `#` comments allowed:
  - `cross <keyA> <keyB>`: every non-default tier-1 value of A × every non-default tier-1 value of B. Cells with a default on either side are covered by the baseline or a single case, and the checker reports that coverage. A cell whose pair line in `settings_validate.txt` changes either of its own assignments is skipped and reported (`skip <name>: validate sets <k=v>`): at the pin, only `goal-maze+accessibility-goal`, which `validate` turns back into `accessibility=all`.
  - `case <k=v> <k=v> ...`: one combo of at least two tier-1 non-default assignments, in any order, normalised to LADXR's order. It is refused if a key repeats or if it duplicates a cross cell.
- The initial spec: `cross logic dungeon_keys`, `cross goal accessibility`, `cross hardmode hpmode`, and the keysanity-everything `case` line (`dungeon_keys`, `nightmare_keys`, `dungeon_beaks`, `dungeon_maps` all `keysanity`, plus `keyholesanity=1`). Phase 2 step 6 appends its sampled combinations as `case` lines.

### The checker (ROM-free, in `make test`)

- `python3 -I tests/ladx/oracle/ladx_cases.py check` first proves itself on planted trees under a temporary directory: a missing case, an extra case, a wrong `config.txt` (an order swap and a wrong value), a seed not from the derivation, an uppercase seed, an extra file, and a name in both directories. Each must produce its line.
- It then compares the real `tests/ladx/cases/` and `tests/ladx/combos/` with the expected list and prints:
  - one line per mismatch: `MISSING <dir>/<name>`, `EXTRA <dir>/<name>`, `WRONG <dir>/<name>: config.txt …`, `BADSEED <dir>/<name>: …`, `STRAY <dir>/<name>/<file>` or `BOTH <name>`;
  - else one summary: `ladx-cases: <n> cases, <m> combos, <s> cells skipped by validate, every tier-1 value and cross cell covered`.
- It exits 1 on a mismatch and 2 on a malformed input.
- The seed rule checked ROM-free: the seed equals `derive(name, k)` for some `k` below the bound (64). Whether `k` is the *first* acceptable candidate needs the ROM; that is `verify`'s job.
- A new root target `ladx-cases` runs it, and `make test` depends on it once the case directories are committed (chunk 3). It runs in well under a second.

### Seed derivation and picking (with the ROM)

- `derive(name, k)` is the first 16 bytes of SHA-256 of the UTF-8 text `ladx-case <name> <k>`, for `k = 0, 1, …, 63`. It is deterministic, unique per case, and reproducible by anyone without the ROM.
- `ladx_cases.py pick [--rom <file>] [--ladxr <dir>] [<name>...]` (default: every expected case that has no directory yet). It needs `tmp/ladx/base.gbc` at SHA-1 `d90ac17e…` and the pinned, patched checkout; otherwise it prints a `setup:` line and exits 2. For each case:
  1. It checks with the live LADXR `Settings` that `validate` changes none of the case's own assignments. This covers the combos of more than two keys, which the pair fixture cannot cover. It refuses the case otherwise.
  2. For `k = 0…63`, the candidate seed is accepted only when:
     - (a) its `seed` file content clears the strict Nintendo-bytes check, through `nintendo_bytes.py`'s functions on the would-be bytes, else the refusal is `nintendo-bytes`;
     - (b) `oracle_run.py --rom` with `PYTHONHASHSEED=0`, run as `gen_golden.sh` runs it, under `tmp/ladx-cases/`, gives `outcome ok`, else the refusal is the outcome (`error <msg>` / `crash <type> <msg>`).
  3. It writes `config.txt` and `seed` for the first accepted candidate. With no acceptable candidate below 64 it writes nothing for that case and exits 1.
- It never touches an existing case directory: to re-pick, the case directory is deleted by hand, on purpose.
- It prints one line per case, `<name>: seed k=<k> (<r> refused: <outcome>; …)`, then a summary with the refusal counts by kind.
- `ladx_cases.py verify [<name>...]` (ROM, same `setup:` rules) re-runs every case's candidates `0…k`. It checks that each `j < k` is refused for a reason, that `k` is accepted, and that the stored `config.txt` matches the expected list. It prints `verified <name> k=<k>` lines, then `ladx-cases: <n> seeds verified first-ok`.
- Measured cost: about 0.8 s per candidate (12 `itempool=pain` candidates: 9.9 s, 7 ok). The whole pick, and the whole verify, are each a few minutes.

### Case runner

- `gen_golden.sh` without `--cases` runs the case directories: every case of `tests/ladx/cases/`, then of `tests/ladx/combos/`, or the named ones. It reads them through `ladx_cases.py`, which prints the same `<name> <seed> <k=v…>` lines `ladx_oracle.py cases` prints for a list file, so the run loop is shared.
- Output stays flat (`<out>/<name>/`); names are unique across both directories.
- In this mode it exits 1 after the summary if any case's outcome is not `ok` (`gen_golden: <n> cases not ok`).
- `--cases <file>` keeps today's behaviour exactly, with no ok requirement, because `determinism_cases.txt`'s `pain` case is an error on purpose.
- `--out` must still lie inside `tmp/`. Step 5 lifts this.
- `determinism.sh` passes `--cases determinism_cases.txt` explicitly, so its default behaviour is unchanged. `determinism.sh --full --all` runs the case directories instead, every case and combo through the A, B and plain runs and comparisons that `--full` already does. It ends with `determinism: <n> cases identical with the ROM (…)`. `inventory_trace.sh` is unchanged.

### Nintendo bytes

- The committed case directories pass `nintendo_bytes.py` (strict scope by path) with `0 literal runs` before the commit; the count goes in the step note. `config.txt` text is expected to clear as `ladxr-public` (probe: 34 sample files, 93 runs cleared, 0 literal). The seeds clear by construction of the picker.

### `CLAUDE.md`

It is updated in the commits that add each piece:
- the case layout (`config.txt`, `seed`);
- `combos.txt`;
- `ladx_cases.py check|pick|verify`;
- the `ladx-cases` target in `make test`;
- `gen_golden.sh`'s new default;
- `determinism.sh --full --all`;
- `tier1.txt`'s bingo exclusion.

## Implementation Chunks

1. **Apply the goal-bingo cut to the tier-1 list.** Edit `tier1.txt` (explicit `goal` values, header comment naming the cut), run `gen_fixtures.sh` then `gen_fixtures.sh --check`, confirm the fixture diff is deletions of `goal=bingo*` entries only, and run `make test`. This is its own commit, with the reason in the message. Needs the network, for setup's fresh clone in `--check`.
2. **Expected list and checker, ROM-free.** Build `ladx_cases.py`: spec reading, expansion with validate skips and coverage, the seed derivation, case-directory reading, `check` with its planted-tree self-proof, and the `<name> <seed> <k=v…>` listing. Add `combos.txt` and `test_ladx_cases.py` (picked up by `test-ladx-oracle`). Add the root `ladx-cases` target, not yet a dependency of `make test`, because no directory exists yet. Add `CLAUDE.md` text.
3. **Pick, verify and commit the cases (ROM).** Add `pick` and `verify`. Run `pick` for every case, then `verify`, then `nintendo_bytes.py` on both directories. Commit `tests/ladx/cases/` and `tests/ladx/combos/` and make `ladx-cases` a dependency of `make test`. `make check` stays green. The refusal statistics go to the step note.
4. **Runner and full double run.** Add `gen_golden.sh`'s case-directory default with its ok requirement. Add `--cases determinism_cases.txt` explicitly where `determinism.sh` and any other caller relied on the old default. Add `determinism.sh --full --all`. Run `gen_golden.sh --out tmp/…` over every case (all `ok`) and `determinism.sh --full --all` (every case identical, plain CLI equal). Run `determinism.sh --full` once more on its own list, unchanged. Update `CLAUDE.md`.

## Testing Decisions

- Good tests here check observable behaviour through each tool's command line or public functions: what list is expected for given inputs, what lines the checker prints for a given tree, and which seed is derived for a name and `k`. They do not test internal data structures.
- `test_ladx_cases.py` (unittest, stdlib, no LADXR, no ROM, run by `make test` through `test-ladx-oracle`) covers:
  - expansion from small stand-in `tier1.txt`, settings and validate fixtures: singles skip defaults, cross skips default cells, a validate-rewritten cell is skipped and reported, a `case` line is normalised to LADXR's order, a repeated key or a duplicate of a cross cell is refused;
  - names: dashes in values, `+` joins, LADXR's order;
  - the derivation: fixed vectors for two names at `k = 0` and `k = 1`, computed once with `hashlib` and written into the test;
  - the checker on planted trees: each mismatch kind gives exactly its line and exit 1, and a correct tree gives the summary and exit 0;
  - the listing format equals what `ladx_oracle.py cases` prints for the same cases;
  - the picker's candidate loop with injected stand-ins for the oracle run and the Nintendo-bytes test (no subprocess): it picks the first acceptable candidate, records refusals in order, writes nothing when all 64 are refused, and refuses an existing directory.
- The checker's built-in self-proof on planted trees runs on every `check`, as `rom_guard.py`, `nintendo_bytes.py` and `push_gate.py` do.
- `test_oracle.py`'s existing `cases` and `gen_golden.sh`-adjacent tests stay as they are. A test is added for `gen_golden.sh`'s argument handling only if the shell change touches a helper the Python tests already cover.
- ROM-backed evidence, outside `make test`, recorded in the step note:
  - `pick` and `verify` output (counts and refusal kinds);
  - `nintendo_bytes.py` on `tests/ladx/cases/` and `tests/ladx/combos/` printing `0 literal runs`;
  - `gen_golden.sh` over every case with every outcome `ok`;
  - `determinism.sh --full --all` identical on every case;
  - `determinism.sh --full` unchanged on its list;
  - `gen_fixtures.sh --check` green after the cut.
- Prior art: `tests/rom_guard.py` (self-proof on planted trees), `test_oracle.py` (`cases` parsing, stand-in settings classes), `nintendo_bytes.py`'s planted self-proof, and ALttP's `tests/alttp/cases.sh` (case discovery by marker file).
- `make check` stays green throughout. No existing expected value is edited except the fixtures the bingo cut regenerates, and those are regenerated by their generator, never by hand.

## Out of Scope

- Goldens of any kind (placement, spoiler, write log, output SHA-1, RNG trace): step 5. Here `gen_golden.sh` writes only under `tmp/`.
- Read-trace checks on every case: step 5's verification.
- Patch tables, the random-sample combos (step 6, which will append `case` lines to `combos.txt`), and extraction fixtures (step 7).
- Committing refused seeds as "LADXR refuses this seed" cases for the C port. The refusals are recorded in the step note only; Phase 3 may add such cases if its claims need them.
- Marking `goal=bingo*` unsupported in the C settings table: Phase 3 step 2.
- Any tier-2/3 value, and any change to `determinism_cases.txt`, `side_seeds.txt` or the inventory.

## Further Notes

- The push gate's strict scope already covers both case directories, so every push of this step runs the 4-byte check on them. The picker's Nintendo-bytes test makes that pass by construction rather than by luck.
- If `pick` finds no acceptable candidate below 64 for some case (unexpected: the worst measured value, `itempool=pain`, accepts about 6 in 10), that is a finding. It gets a `guideline` decision recorded in the step note: raise the bound for everyone in the derivation, or treat it as a LADXR refusal pattern. The bound is never raised for one case alone.
- Expected counts at the pin: 89 cases, 27 combos, 1 skipped cell (`goal-maze+accessibility-goal`). The checker computes them; any other count must be explained in the step note.
- The seed derivation is a rule of this repository, not of LADXR. On the device the seed is random, so the cases only need to be reproducible and auditable, which the derivation and `verify` give.
