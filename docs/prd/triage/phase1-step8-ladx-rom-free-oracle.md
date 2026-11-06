---
title: "LADX oracle, ROM-free: setup, fixtures and determinism"
description: "The C port of LADXR has no reference values yet, and nobody has shown that LADXR's own output is stable across hash seeds and checkouts."
status: needs-triage
---

## Plan step (verbatim, `docs/plan-3.md`, Phase 1, step 8)

> 8. **ROM-free oracle work.** LADXR runs its logic and fill without a ROM (its own CI uses `SKIP_ROM_GENERATION`, and `randomizer.py` calls `generateRom` only after the fill). This step:
>    - writes `tests/ladx/oracle/setup.sh`, which clones LADXR at `4325207e…` into `tests/ladx/oracle/ladxr/` (git-ignored) and records the Python version in the step note;
>    - produces the ROM-free fixtures: CPython RNG values for every method LADXR calls, with several seeds; LADXR's logic and location tables for the logic unit tests (like ALttP's `extract_logic`); the item pool per setting; and LADXR's settings and values list, with its own rules on incompatible combinations;
>    - runs the baseline and a handful of settings in `SKIP_ROM_GENERATION` mode twice, with `PYTHONHASHSEED=0` and `=1`, comparing the placement and the RNG trace; the second run uses a fresh clone in another directory, so directory-listing order varies too. Any difference is fixed in `ladxr.patch` (§2), in its own commit, with each change explained;
>    - audits the frozen sources for `os.listdir`, `glob` and `os.walk`: each one on the tier-1 path (for example `patches/endscreen.py`'s unsorted `cats` list, fed to `random.choice`) is made order-stable in `ladxr.patch`, in its own commit, or justified in the step note. The same audit covers iteration over sets, or dicts keyed by objects, that hash by identity (no `__hash__`, so `PYTHONHASHSEED` does not vary their order) wherever the order reaches the RNG, the placement, a ROM write or the spoiler JSON; the known instance is `spoilerLog.py`'s `accessibleItems`, built from `Explorer`'s visited set. Phase 2 step 2's double run then re-checks the patched paths.
>
>    Verified by the fixtures being byte-identical on two runs, and by the double run matching.

The §2 rules that apply: "Python's RNG, exactly" (MT19937, seeding from bytes version 2 with SHA-512, every method LADXR calls, values CPython printed, committed fixtures needing no ROM); "Determinism is proven, not assumed" (`ladxr.patch` fixes order as `vt.patch` does for VT); "Settings tier 1"; "Code layout" (`tests/ladx/` with `unit/`, `cases/`, `oracle/`); "No Nintendo ROM in the repository or in CI".

## Problem Statement

Plan 3 ports LADXR's seed-dependent half to C99 and proves it byte for byte against LADXR, as the ALttP port was proven against VT. Phase 3 starts with Python's RNG, the settings table, the logic and the item pool, and each of those steps is "verified by unit tests against Phase 1 step 8's fixtures". Today there is no `tests/ladx/` at all: no pinned LADXR checkout, no reference values for CPython's `random.Random`, no machine-readable copy of LADXR's settings, logic graph or item pool, and nothing to write them with.

Worse, the oracle itself is unproven. LADXR is Python: its set iteration depends on `PYTHONHASHSEED` for strings and on memory addresses for objects without `__hash__`, and a few patches pick files from an unsorted `os.listdir`. If any of that reaches the RNG, the placement, a ROM write or the spoiler, two runs of the "same" seed differ, and every golden made from it in Phase 2 would be a coin toss. That has to be shown before any golden is generated, and it can be shown now, without the ROM, on the logic and fill half (`SKIP_ROM_GENERATION`), plus a source audit for the ROM half.

## Solution

A new test tree `tests/ladx/`, mirroring `tests/alttp/`, starts with its oracle folder, `tests/ladx/oracle/`:

- **`setup.sh`** clones LADXR at `4325207e4e55076e05f209fbdf16b860a90e42ef` into `tests/ladx/oracle/ladxr/` (git-ignored), applies `ladxr.patch` when it exists, and prints one line with the Python version and the checked-out commit. It is idempotent; `--fresh` starts from nothing; `LADXR_DIR` puts the checkout elsewhere (the double run's second clone uses it).
- **`gen_fixtures.sh`** runs every fixture generator against that checkout and writes six committed text fixtures into `tests/ladx/oracle/`: CPython RNG vectors, LADXR's settings list, its settings-validation vectors, its logic graphs, its logic vectors and its item pools. `--out <dir>` writes elsewhere; `--check` generates twice (`PYTHONHASHSEED=0` on the setup checkout, `PYTHONHASHSEED=1` on a fresh clone in another directory), requires the two runs to be byte-identical and both byte-identical to the committed files, and prints one summary line.
- **`determinism.sh`** runs the baseline and a handful of settings cases through LADXR in `SKIP_ROM_GENERATION` mode, twice, under the same two conditions, recording for each the world setup, the item pool, the placement and the full RNG trace, and prints one line per case and one summary line; any difference fails it.
- **`audit_order.py`** lists every directory-listing call and every set construction or set-returning call in the frozen (patched) sources and fails unless each one has a verdict in the committed `order_audit.txt`: patched in `ladxr.patch`, unreached on the tier-1 path, or order-free.
- **`ladxr.patch`** holds the order fixes the audit and the double run require, each in its own commit with its reason; at least the two the plan already names (`endscreen.py`'s `cats`, `spoilerLog.py`'s `accessibleItems`).

None of this needs the ROM, and none of it joins `make check`: the oracle needs the network (GitHub) and runs only when the fixtures are regenerated, like ALttP's `gen_golden.sh`. The pure helpers the scripts share are unit-tested in `make test`, in well under a second, with no LADXR and no network.

## User Stories

1. As the Phase 3 implementer of `pyrandom.c`, I want committed CPython values for every `random.Random` method LADXR calls, so that I can test the C RNG without Python.
2. As that implementer, I want the vectors to start from byte seeds of several lengths (empty, one byte, LADXR's usual 16 bytes, an ASCII user seed, 64 bytes and longer than SHA-512's block), so that the SHA-512 seeding path is pinned in every branch.
3. As that implementer, I want the raw 32-bit MT outputs after seeding, so that I can find a seeding bug before a method bug.
4. As that implementer, I want `random()` values given exactly (as hexadecimal floats C99's `strtod` reads), so that no decimal rounding hides a one-ulp error.
5. As that implementer, I want vectors for `randrange` and `randint` over the ranges LADXR uses (one value, powers of two, a negative lower bound like `randint(-1, 8)`, ranges just above a power of two), so that the rejection loop of `_randbelow` is covered.
6. As that implementer, I want `choice`, `shuffle`, weighted `choices` and `uniform` vectors given as indexes and exact values, so that I can test them without LADXR's objects.
7. As that implementer, I want mixed scripts of calls on one generator, so that I know each method consumes exactly CPython's number of MT words.
8. As the owner, I want the oracle to refuse to trace a `random.Random` method the vectors do not cover, so that no LADXR call can slip past the C port's tests.
9. As the Phase 3 implementer of the settings table, I want LADXR's settings in a fixture (key, category, short key, type, default, label, aesthetic flag, visibility condition, and every option with its short form and label), so that the C table is checked against LADXR's names, not my copy of them.
10. As that implementer, I want LADXR's `Settings.validate` rules as input/output vectors, so that the automatic adjustments of incompatible combinations are ported and checked rather than guessed.
11. As that implementer, I want each validation vector to give LADXR's short settings string after validation, so that the short string (which ends up in the ROM and spoiler) is pinned too.
12. As the Phase 3 implementer of the logic, I want the full logic graph of the baseline for each `logic` value, with locations in LADXR's discovery order, their item spots and options, and every connection's requirement, so that I can diff my graph against LADXR's line by line.
13. As that implementer, I want, for every tier-1 setting value under every `logic` value, the world setup used and a digest of LADXR's graph, so that every setting that touches the logic is covered without committing hundreds of full graphs.
14. As that implementer, I want explorer vectors (an inventory, the locations reached, the items required next), so that requirement evaluation and the explorer are tested on real LADXR answers.
15. As that implementer, I want the generator to write the full graph of any one configuration into a directory I choose, so that a digest mismatch can be turned into a line diff.
16. As the Phase 3 implementer of the item pool, I want the item pool LADXR builds for every tier-1 setting value, under several seeds, in LADXR's insertion order with counts, so that the pool and its order are claimed exactly.
17. As that implementer, I want each pool to record how many MT words had been drawn before the pool was built and after, so that my C pool can start from the right RNG state and prove it consumes the same draws, without porting the world setup first.
18. As that implementer, I want the pool after `readItemPool`'s adjustments (forced items, single-option spots) too, so that the fill's input is pinned before the fill.
19. As the owner, I want LADXR pinned at one commit and fetched by a script, never vendored, so that the repository stays small and the oracle's source is unambiguous.
20. As the owner, I want `ladxr.patch` to be the only change to LADXR, applied by `setup.sh`, so that every deviation from upstream is visible and explained.
21. As an agent, I want `setup.sh` to print the Python version and LADXR's commit, so that the step note can record the version Phase 2 step 7 compares against.
22. As an agent, I want a warning when the Python minor version differs from the one the fixtures were made with, so that a regeneration on another Python is noticed, not silently committed.
23. As an agent, I want a `setup:` line, not a failure that looks like a bug, when git, the network or a recent enough Python is missing.
24. As the owner, I want the fixtures proven byte-identical on two runs, with different hash seeds and checkout directories, so that a fixture is a fact about LADXR, not about one run.
25. As the owner, I want the double run to compare placement and the full RNG trace, so that a hash-order difference that the RNG happens to absorb is still caught.
26. As the owner, I want the double run's second clone to be fresh and in another directory, so that directory-listing order and memory layout vary too.
27. As the owner, I want proof that the two runs really used different hash seeds, so that a "match" cannot come from a variable that never reached Python.
28. As an agent, I want one line per case (`same` or `DIFFERENT` with the first differing line) and one summary line, so that I can read the result at a glance.
29. As the owner, I want every `os.listdir`, `os.walk`, `os.scandir`, `glob`, `iterdir` and set in LADXR's sources to carry a recorded verdict, so that the order audit can be re-run in Phase 2 and stays honest when the patch grows.
30. As the owner, I want the audit to fail on a new unaudited site and on a stale verdict, so that the table never drifts from the sources.
31. As the owner, I want `endscreen.py`'s cat picture and `spoilerLog.py`'s accessible-item order made stable in `ladxr.patch` now, so that Phase 2's goldens never depend on a filesystem or on addresses.
32. As the owner, I want each `ladxr.patch` change in its own commit with its reason, so that the oracle's deviations from upstream are reviewable one by one.
33. As an agent, I want LADXR run from a throwaway working directory with bytecode writing off, so that stray files (`cave.svg`, `__pycache__`) never land in the repository or change the clone's listing.
34. As an agent, I want the oracle's shared helpers (RNG tracer, requirement serializer, line encoders, comparator, audit matcher) unit-tested in `make test` without LADXR, so that the tools are trusted before they produce a fixture.
35. As the owner, I want `make test` to stay a few seconds with no network, so that the new tests do not slow the fast loop.
36. As a future Phase 2 session, I want the double-run driver's output (world setup, pool, placement, trace by stage) in a form the golden format can extend, so that Phase 2 step 2 reuses it rather than rewrites it.
37. As the owner, I want every fixture to describe its own format in `#` header lines, as ALttP's fixtures do, so that a reader needs no other document.
38. As the owner, I want nothing in the fixtures to come from a ROM, so that they are committable and CI-safe under the ROM guard.
39. As a later agent, I want `CLAUDE.md` to describe `tests/ladx/oracle/`, its three scripts and that they are outside the net, so that I know how to regenerate the fixtures and why `make check` does not.
40. As the owner, I want the tier-1 setting list the generators use committed once, in a file derived from §2, so that the fixtures, and Phase 2's case-list check, read one list.

## Implementation Decisions

### Layout

- `tests/ladx/` is created with only `oracle/` in this step. `unit/`, `cases/`, `combos/`, its Makefile and README come with the phases that need them (Phase 3 step 1, Phase 2 steps 4–5).
- The committed fixtures live in `tests/ladx/oracle/`, next to their generators, as ALttP's do in `tests/alttp/oracle/`. Six generated files: `pyrandom_vectors.txt`, `settings.txt`, `settings_validate.txt`, `logic_graphs.txt`, `logic_vectors.txt`, `item_pools.txt`. Three hand-written inputs: `tier1.txt`, `determinism_cases.txt`, `order_audit.txt`.
- The LADXR checkout is `tests/ladx/oracle/ladxr/`, git-ignored (a `.gitignore` line next to the existing `tests/alttp/oracle/vt/` one). Throwaway outputs (double-run outputs, the second clone, `--check` renders, `--graph` dumps) go under the repository's `tmp/ladx-oracle/`.

### `setup.sh`

- POSIX `sh`, `set -eu`, `LC_ALL=C`, structured like `gen_golden.sh`'s VT checkout block: `LADXR_URL=https://github.com/daid/LADXR.git`, `LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef`, `PYTHON_EXPECTED` set to the minor version found when the fixtures are first generated (3.13 in the Claude Code container); `git init` + `fetch --depth 1 <pin>` + forced checkout + `reset --hard` + `clean -fdx`, then `git apply ladxr.patch` when the file exists.
- Flags: `--fresh` (delete and re-clone). Environment: `LADXR_DIR` (checkout path, default `tests/ladx/oracle/ladxr`).
- Output: `setup: ladxr <commit>, python <x.y.z>[, ladxr.patch applied]`, plus a warning line on stderr when Python's `x.y` differs from `PYTHON_EXPECTED`. A missing `git`, a failed fetch (network) or Python older than 3.11 prints a `setup:` line and exits 2.
- Python ≥ 3.11 is required by the oracle scripts (they read `co_qualname` for trace stages); the helpers' module itself stays importable on older Python so that `make test` runs everywhere.

### Running LADXR

- One shared helper module, `ladx_oracle.py`, used by every generator and the driver. It inserts the checkout first on `sys.path`, `chdir`s into a fresh temporary directory under `tmp/ladx-oracle/`, and imports LADXR's modules (`settings`, `randomizer`, `logic.main`, `worldSetup`, `itempool`, `explorer`). LADXR's code is not modified for instrumentation: the oracle wraps functions from outside (monkeypatching), so `ladxr.patch` only ever holds determinism fixes.
- Every oracle invocation runs `python3` **without `-I` or `-E`** (both drop `PYTHONHASHSEED`, which would make the double run vacuous), with `PYTHONDONTWRITEBYTECODE=1` and `PYTHONHASHSEED` set explicitly (0 by default).
- `make test`'s run of the helper unit tests uses `python3 -I` like `test_package.py`, loading the module by path with `importlib`.

### RNG tracing

- `TracingRandom`, a subclass of `random.Random`, replaces `random.Random` for the run (so `randomizer.py`'s `random.Random(self.seed)` and `enemies.py`'s get it), and the module-level functions of `random` (`random.choice`, `random.setstate`, …) are rebound to one `TracingRandom` instance marked `global`.
- It overrides **both** `random` and `getrandbits` (counting MT words: 2 per `random()`, ⌈k/32⌉ per `getrandbits(k)`). Trap recorded in a comment: CPython's `Random.__init_subclass__` switches a subclass that overrides only `random` to `_randbelow_without_getrandbits`, which silently changes every `randrange`/`choice`/`shuffle`. A unit test proves the traced and the plain generator give identical streams.
- Covered public methods: `seed`, `random`, `getrandbits`, `randrange`, `randint`, `choice`, `choices`, `shuffle`, `uniform`, `getstate`, `setstate` — the inventory of the pinned sources (`rnd.`/`random.` calls: `choice` 47, `randint` 29, `randrange` 14, `shuffle` 8, `random` 5, `choices` 5, `uniform` 1, `getstate`/`setstate` 1 each). Any other public method of `random.Random` (`sample`, `gauss`, `randbytes`, `triangular`, the `*variate` family) raises `oracle: RNG method <name> is not covered by pyrandom_vectors.txt`.
- One trace line per outermost public call (nested calls only add their words): `<n> <gen> <stage> <method> <args> -> <result> [<words>]`, where `<gen>` is `rnd` (a generator made by LADXR) or `global`, `<stage>` is `<module>:<co_qualname>` of the nearest calling frame inside the LADXR checkout, `<args>`/`<result>` are rendered canonically: integers in decimal, floats as `float.hex`, `choice`/`choices` as the chosen index and the sequence length, `shuffle` as the length and the resulting permutation of indexes, `seed` as hex bytes, `getstate`/`setstate` as `-`.

### Fixture formats (all)

- UTF-8 text, LF line endings, one record per line, `#` header lines describing the format and naming the generator. No timestamp, no path and no Python version inside a fixture (they would make two equal runs differ); the version goes in the step note.
- Fields separated by a single tab where a field may contain spaces (labels); by single spaces otherwise. An empty string is written `""`. Generators assert that no key, value, short form or item name contains a tab, a newline or a `"`, and that item names match `[A-Z0-9_]+`.
- Seeds are lower-case hex bytes, `-` for the empty seed.

### `pyrandom_vectors.txt`

- Generated by plain CPython `random.Random` (no LADXR needed; still run by `gen_fixtures.sh`).
- Seeds: `-`, `00`, `01`, `000102…0f` (16 bytes), `ff`×16, the ASCII bytes of `LADXR`, 64 bytes, 300 bytes.
- Records, each for every seed: `words <seed> <first 16 getrandbits(32)>`; `random <seed> <first 8 random() as float.hex>`; `getrandbits <seed> <k> <first 4>` for k in 1, 31, 32, 33, 64; `randrange <seed> <start> <stop> <first 8>` and `randint <seed> <a> <b> <first 8>` over ranges 1, 2, 3, 100, 2³¹, 2³²+1 and `-1..8`; `choice <seed> <len> <first 8 indexes>` for lengths 1, 2, 7, 64, 300; `shuffle <seed> <n> <permutation>` for n 0, 1, 2, 9, 64; `choices <seed> <weights, comma-separated> <first 8 indexes>` for LADXR's integer weight lists (`MULTI_CHEST_WEIGHTS`'s shape, `10,5,10,10,1`, uneven rupee counts); `uniform <seed> <a> <b> <first 8 as float.hex>` for `0 100`; and `script <seed> <op;op;…> -> <results>`, three mixed sequences per seed interleaving every method, ending with the word count consumed.
- The race mode's extra `random()` call needs no vector of its own: it is `random`'s first value.

### `tier1.txt`

- Hand-written from §2, one line per tier-1 setting: `<key> all` or `<key> <value> <value> …` (e.g. `music "" random off shifted`). `forwardfactor` and `seed` are not listed. Every generator that enumerates tier-1 values reads it and fails if a key or value does not exist in LADXR's `Settings`.

### `settings.txt` and `settings_validate.txt`

- `settings.txt`: `setting <key>\t<category>\t<short>\t<bool|str|float>\t<default>\t<label>\t<aesthetic 0|1>\t<visible_if, comma-separated, or ->`, then one `option <key>\t<value>\t<short>\t<label>\t<meta or ->` per option in LADXR's order (the label's `|meta` split as `toJson` does). Every setting LADXR defines, tier-1 or not, `gfxmod` included (its options are LADXR's own sorted `gfx/` names); the C table applies tiers.
- `settings_validate.txt`: the settings `Settings.validate` reads or writes are found by recording attribute access on a `Settings` while `validate` runs over the single-value inputs (union, then fixed in the file's header). Vectors: every value of every setting alone, then every pair of values of two different settings from that set; each line `validate <k=v,…> -> <changed k=v,… or => >\t<short string after>`. LADXR's warning prints are captured and dropped.

### `logic_graphs.txt` and `logic_vectors.txt`

- A configuration is a settings set plus a world setup. Configurations: for each `logic` value, the baseline and every non-default tier-1 value of every tier-1 setting, one at a time (about 5 × 100). The world setup is `WorldSetup().randomize(settings, random.Random(000102…0f))`, as `randomizer.py` does, and is written out in full so that the C logic test takes it as input and does not depend on the C world setup.
- Canonical graph text: `loc <i> <dungeon or -> <name or ->` in `Logic.location_list` order; `spot <loc> <class> <nameId> <options, comma-separated>\t<metadata name>\t<metadata area>` in `iteminfo_list` order (options after `configure`); `edge <from> <to> <req>` in each location's `connections` order. Requirements as S-expressions keeping LADXR's stored order: `-` (None), `T`/`F`, a bare item name, `and(…)`/`or(…)` (strings first, then children, as LADXR stores them), `count(ITEM,n)`, `counts(n,A,B,…)`, `found(ITEM,n)`. Unknown requirement types fail the generator.
- `logic_graphs.txt`: the full canonical text of the baseline of each `logic` value (five graphs, about 1 MB in all, in line with ALttP's row files).
- `logic_vectors.txt`: per configuration, `config <name> <k=v,…>`, the world setup as `ws <field> <value>` lines (every attribute of `WorldSetup`, rendered canonically; entrance mapping asserted to be the identity on tier 1 and written as `identity`), `graph <SHA-512 hex of its canonical text>`, then explorer vectors `explore <inventory item=count,… or -> -> <reached location indexes as ranges>\t<required-next items, sorted>`: the empty inventory, the full item pool of that configuration, and four partial inventories drawn by the generator from that pool with its own fixed `random.Random`, written explicitly.
- SHA-512 because Phase 3 step 1 builds it in `ladx/` before the logic step needs it (no `common/` needed). `extract_logic.py --graph <config> --out <dir>` writes any configuration's full canonical text for a line diff.

### `item_pools.txt`

- Cases: the baseline and every non-default tier-1 value, one at a time, each under three fixed seeds. Each case runs the real `Randomizer` in `SKIP_ROM_GENERATION` mode through the driver below, which records at the hooks: `case <name> <seed> <short settings string>`, `words_before <n>` (MT words drawn when `ItemPool` is entered), `pool <item>=<count> …` (`toDict()` in insertion order, zero counts kept), `words_after <n>`, and `placer_pool <item>=<count> …` (what `readItemPool` returns). A case that raises LADXR's `Error` records `error <message>`; the generator fails if a baseline case does.

### Double-run driver and `determinism.sh`

- `oracle_run.py <ladxr dir> <seed hex> [k=v …]` builds `Settings`, calls `validate`, and constructs `randomizer.Randomizer` with `input_filename="SKIP_ROM_GENERATION"`, no plan, `spoilerformat="none"`. Hooks: `ItemPool` construction and `readItemPool` (pools and word counts), `RandomItemPlacer.run` (placement right after it returns: `place <i> <class> <nameId> <item>` over `iteminfo_list`), the world setup after `randomize`. Output file sections in order: `seed`, `settings` (short string), `ws …`, `pool …`, `placer_pool …`, `place …`, `trace …`, `outcome ok|error <msg>`. A separate meta file holds `PYTHONHASHSEED`, `hash("ladxr")`, the checkout path and the Python version, and is not compared.
- `determinism_cases.txt`: about ten cases, each `<name> <seed hex> <k=v …>`: baseline; `race=1`; `accessibility=goal`; `logic=hell dungeon_keys=keysanity nightmare_keys=keysanity`; `boss=random miniboss=random`; `goal=bingo`; `goal=maze`; `goal=specific goalcount=random`; `itempool=casual`; `itempool=pain`; `owlstatues=both dungeon_beaks=keysanity`; `hpmode=5hit heartpiece=1`.
- `determinism.sh` runs every case with run A = `PYTHONHASHSEED=0` on `tests/ladx/oracle/ladxr`, run B = `PYTHONHASHSEED=1` on a fresh `git clone` from `LADXR_URL` into a new directory under `tmp/ladx-oracle/` (patched by `setup.sh` with `LADXR_DIR`). Output: `same <case>` or `DIFFERENT <case>: line <n>: <A line> | <B line>`, then `determinism: <n> cases identical (PYTHONHASHSEED 0 vs 1, <dir A> vs <dir B>)` or exit 1. It first proves itself: the comparator on two planted output files differing on one trace line must print `DIFFERENT`; then, after the real runs, it requires each case's two meta files to show different `hash("ladxr")` values and different checkout paths, else it fails with `determinism: the two runs did not vary`.

### `gen_fixtures.sh`

- `gen_fixtures.sh [--out <dir>] [--check]`. Without `--out`, writes the six fixtures into `tests/ladx/oracle/`. `--check` renders into two directories under `tmp/ladx-oracle/` (A: setup checkout, hash seed 0; B: fresh clone, hash seed 1), then prints `DIFFERENT <file>` for each file where A ≠ B or A ≠ the committed one (`MISSING`/`EXTRA` likewise), else `fixtures: 6 files byte-identical on two runs and to tests/ladx/oracle`.

### Order audit

- `audit_order.py <ladxr dir>` scans every `.py` of the checkout except `www/` for: `os.listdir`, `os.scandir`, `os.walk`, `glob`, `iterdir`, a call `set(`/`frozenset(` not preceded by `.` or a word character, `for … in {`, and `getAccessableLocations(` (the explorer's visited set). Each hit is keyed by `<file>\t<stripped line text>` (not the line number, which `ladxr.patch` shifts).
- `order_audit.txt`: `<file>\t<stripped line text>\t<patched|unreached|order-free>\t<reason>`. `unreached` cites the setting that gates it outside tier 1 (e.g. `patches/music.py` `importMusic`: only `music=ffa`); `order-free` says why order cannot matter (membership only, a boolean result, a later `sort`).
- Output: `UNAUDITED <file>:<line> <text>` per hit without an entry, `STALE <file> <text>` per entry without a hit, else `order audit: <n> sites, all audited`. It first proves itself on a planted source tree (an unaudited `os.listdir` caught, a stale entry caught, an audited site passed).
- Set literals used only for membership (`x in {…}`) are outside the mechanical scan; the step note says so and records the manual review of the tier-1 path (`randomizer.py`, `itempool.py`, `worldSetup.py`, `logic/`, `locations/`, `explorer.py`, `spoilerLog.py`, `generator.py` and the patches it calls), including dicts keyed by objects.

### `ladxr.patch`

- Made by `git diff` in the checkout against the pin, applied by `setup.sh` with `git apply`. Each fix is its own commit, its reason in the commit message and in the step note; a code comment only where it carries a trap.
- Known fixes, committed before the fixtures that could depend on them: `patches/endscreen.py` takes `sorted(…)` of the `cats` listing before `random.choice`; `spoilerLog.py` builds `accessibleItems` by walking `self.logic.location_list` and keeping the reached locations, instead of iterating the explorer's set. Further fixes only if the audit or the double run demands them; if one changes the fill path after the fixtures exist, the fixtures are regenerated in that same fix's follow-up commit.

### Wiring and docs

- The helper unit tests (`tests/ladx/oracle/test_oracle.py`) join `make test` through a root `Makefile` target `test-ladx-oracle`, added to `test`'s prerequisites, with a `help` line. No oracle script joins `make test` or `make check`.
- `.gitignore`: `tests/ladx/oracle/ladxr/`.
- `CLAUDE.md`: the Layout section gains `tests/ladx/oracle/`; the Checks section describes `setup.sh`, `gen_fixtures.sh [--check]`, `determinism.sh` and `audit_order.py` as outside the net (network, LADXR), and `test-ladx-oracle` inside `make test`; the "Not in the net" list names them. The "No Nintendo ROM" ground rule's rewrite for the temporary link is not this step's.
- Step note: Python version, LADXR commit, the `--check` and `determinism.sh` summary lines, the audit's site count with each `patched` and the tier-1-path `unreached`/`order-free` verdicts summarised, the manual set-literal review, and the fixture sizes.

### Hypotheses about steps 6 and 7 (not done, run before this one)

- **H-6-1**: Step 6 (game-choice screen) changes only `app/`, `tests/alttp/` (unit tests, snapshots, Makefile, SDL goldens) and possibly `CLAUDE.md`'s UI text; it creates nothing under `tests/ladx/` and does not touch `.gitignore`.
- **H-6-2**: Step 6 does not change the root `Makefile`'s `test` line; if it did, adding `test-ladx-oracle` is a one-word merge.
- Step 6 landed while this PRD was written (`9ddcdc9`, note `3061764`): it touches `CLAUDE.md` and `tests/alttp/Makefile` but not the root `Makefile`, `.gitignore` or `tests/ladx/`, so H-6-1 and H-6-2 hold.
- **H-7-1**: Step 7 adds `tests/rom_guard.py` and a root `rom-guard` target to `test` (`test: test-rando test-template rom-guard`), and `*.gb`/`*.gbc` to `.gitignore`. This step appends `test-ladx-oracle` to that same line and a different `.gitignore` line; both merges are mechanical.
- **H-7-2**: The ROM guard checks only tracked files by extension and by the two ROM digests. This step tracks only `.txt`, `.sh`, `.py` and `.patch` files and no ROM content; the git-ignored LADXR checkout (which holds `.bin` art, no `.gb`/`.gbc`) is untracked, so the guard stays green.
- **H-7-3**: The guard hashes every tracked blob; the ~2 MB of fixtures added here keep it well under a second.
- **H-7-4**: Step 7 edits `CLAUDE.md`'s `make test` bullet and the "No Nintendo ROM" ground rule; this step adds a Layout entry and separate Checks bullets, so the edits do not overlap.

## Implementation Chunks

1. **Oracle scaffolding and RNG vectors.** `tests/ladx/oracle/` with `setup.sh`, the `.gitignore` line, `ladx_oracle.py` (path/chdir/env handling, `TracingRandom`, line encoders), `test_oracle.py` wired into `make test`, and `pyrandom_vectors.txt` via `gen_fixtures.sh` (with `--out` and `--check` working for this one file). Done when `make test` is green and `gen_fixtures.sh --check` reports the file identical on two runs.
2. **Order audit and the known `ladxr.patch` fixes.** `audit_order.py`, its self-proof, `order_audit.txt` covering every hit; then the `endscreen.py` fix and the `spoilerLog.py` fix, each in its own commit; `setup.sh` applying the patch. Done when the audit prints `all audited` on the patched checkout.
3. **Settings fixtures.** `tier1.txt`, `settings.txt`, `settings_validate.txt`. Done when `gen_fixtures.sh --check` covers three files.
4. **Driver, item pools and the double run.** `oracle_run.py` with its hooks, `item_pools.txt`, `determinism_cases.txt`, `determinism.sh` with its self-proof and the hash-seed proof. Any difference found is fixed in `ladxr.patch` in its own commit (with the audit table updated and the affected fixtures regenerated). Done when `determinism.sh` prints its summary and `--check` covers four files.
5. **Logic fixtures and docs.** `extract_logic.py` (with `--graph`), `logic_graphs.txt`, `logic_vectors.txt`, `CLAUDE.md`, the step note. Done when `gen_fixtures.sh --check` prints the six-file summary, `determinism.sh` and the audit pass, and `make check` is green.

## Testing Decisions

- **A good test** checks what a tool does from the outside: a generator's output lines for a given input, the driver's trace for a known seed, the comparator's verdict on two files, the audit's verdict on a source tree. Internals (which hook fires first, how a line is assembled) are not asserted.
- **`test_oracle.py`** (Python `unittest`, standard library only, no LADXR, no network, under a second, run with `python3 -I` in `make test`; prior art `tests/alttp/test_package.py`, loading its module by path):
  - `TracingRandom` and a plain `random.Random` with the same byte seed give identical outputs for every covered method over a long mixed sequence (guards the `__init_subclass__` trap); word counts match CPython's consumption (checked by re-seeding and skipping that many `getrandbits(32)`); an uncovered method (`sample`) raises the documented error; the trace renders `choice`/`shuffle`/`choices` as indexes and floats as `float.hex`.
  - The requirement serializer on stand-in classes named `AND`, `OR`, `COUNT`, `COUNTS`, `FOUND` with the same private attribute names, plus `None`, `True`, `False` and strings; an unknown type is refused.
  - The field encoders: `""` for empty, tab/newline/quote refused, item-name pattern enforced.
  - The determinism comparator: identical files → `same`; one differing trace line → `DIFFERENT` with that line number.
  - The audit matcher on a planted tree in a temp directory: unaudited, stale and audited sites.
- **Scripts' self-proofs** (prior art: `goldens.sh --compare`, `no_sdl.sh`): `determinism.sh` and `audit_order.py` prove themselves on planted inputs on every run before the real run.
- **Acceptance** (outside the net, in the step note): `setup.sh` output; `gen_fixtures.sh --check` summary; `determinism.sh` summary; `audit_order.py` summary; `make test` and `make check` green.
- **Spot checks** recorded in the step note: one `pyrandom_vectors.txt` line recomputed by hand in a Python shell; the baseline `item_pools.txt` total equal to the number of placeable spots in the baseline driver output; the `logic_graphs.txt` normal baseline's location count equal to `len(Logic(...).location_list)`.
- Nothing existing is weakened; no ALttP fixture or golden is touched.

## Out of Scope

- The ROM, `fetch_rom.sh`, the Nintendo-bytes check, the read trace, the push gate and the inventory draft: step 9 and Phase 2.
- `gen_golden.sh`, the LADX cases and combos, the golden format and `tests/ladx/README.md`: Phase 2 steps 2, 4 and 5. The driver's output is their starting point, not their final format.
- Any C code, `tests/ladx/Makefile`, `make logic`/`unit-arm`/`lint` for LADX: Phase 3.
- Double-running the ROM-writing half; the audit's patched ROM-side paths are re-checked by Phase 2 step 2's full double run.
- Fixtures for tier 2/3 values (other overworlds, entrance shuffle, shopsanity, `gfxmod`, `music=ffa`), except where a fixture is LADXR's whole list by nature (`settings.txt`, `settings_validate.txt`, `order_audit.txt`).
- `ForwardItemPlacer` (`forwardfactor` > 0): not ported; its RNG calls are covered by the vectors anyway because they use the same methods.

## Further Notes

- LADXR at the pin imports only the standard library on the `SKIP_ROM_GENERATION` path (`PIL` is imported inside the `gfxmod` and overworld-export functions only), so the oracle needs no `pip install`.
- In `SKIP_ROM_GENERATION` mode `randomizer.py` would reference an undefined `rom` if a spoiler were requested; the driver always passes `spoilerformat="none"` and no log directory.
- CPython hashes objects without `__hash__` by address, so `PYTHONHASHSEED` does not move them; only a fresh process, a different checkout and the audit can show an address-order dependence, which is why the double run uses a fresh clone and the audit covers `getAccessableLocations`.
- The fixtures' expected total size is about 2–3 MB (mostly `logic_graphs.txt` and `settings_validate.txt`), below ALttP's 5.4 MB of oracle fixtures; the step note records the real sizes.
- The study clone used to write this PRD (`tmp/ladxr-study/`, read-only) is throwaway and not referenced by any script.
