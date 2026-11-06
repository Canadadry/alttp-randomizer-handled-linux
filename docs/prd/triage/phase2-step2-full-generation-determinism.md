---
title: "Plan 3 step 2.2: oracle case runner and full-generation determinism"
description: "LADXR's double run has only been proven without the ROM, and nothing runs a case through the whole generator yet, so the ROM-writing half of every future golden rests on an unchecked assumption that its output depends only on settings and seed."
status: needs-triage
---

## Problem Statement

Plan 3 (`docs/plan-3.md`) proves a LADX seed right by matching LADXR's output byte for byte. That proof is only worth something if LADXR's output is a function of *settings + seed + ROM* and nothing else.

Phase 1 step 8 proved this for half of the generator. `oracle_run.py` drives LADXR in `SKIP_ROM_GENERATION` mode, and `determinism.sh` runs the 12 cases of `determinism_cases.txt` twice, with `PYTHONHASHSEED=0` on the setup checkout and `=1` on a fresh clone, and compares the world setup, item pool, placement and RNG trace. Two order fixes landed in `ladxr.patch`. Nothing after the fill was covered. `generator.generateRom` (about 300 lines of patch calls, the enemizer with its own `random.Random(seed)`, hints, music, the end-screen cat, the title screen, `ROMWithTables.save`'s re-serialization) and the JSON spoiler, which `spoilerLog` builds by reading the finished ROM back, have never been run twice and compared.

While this PRD was being written, a full LADXR run was made on the real ROM (`tmp/ladx/base.gbc`). Five settings/seed pairs gave identical ROMs and spoilers under `PYTHONHASHSEED=0` and `=1`. Two pairs were refused by LADXR ("Specified seed does not produce a valid result"). One generation took about 0.6 s. Reading the frozen sources for environment reads found one that the double run as planned cannot see:

- `patches/titleScreen.py`'s `setRomInfo` runs `git describe --tags --dirty=-D` **in the current working directory** and writes the answer onto the title screen. Today it gets an empty string by luck: the oracle's working directory is under this repository's `tmp/`, and this repository has no tag. The day the owner tags a release, every LADX output changes. A checkout with tags would write the tag and `-D`, because `ladxr.patch` makes the LADXR checkout dirty. Both of the planned runs work inside this repository, so they see the same answer and still compare equal.

The plan also says this step introduces `gen_golden.sh`, LADX's case runner: settings plus a seed as hex bytes, run with the ROM. Step 5 defines the golden format (placement, spoiler, tagged write log with source references, output SHA-1, RNG trace by stage) and its "0 literal runs" rule. Until then, anything derived from the ROM must stay under `tmp/`. This step therefore has to decide what the runner writes now, and where, without pre-empting step 5.

## Solution

1. Re-run `setup.sh`, as the Phase 2 preamble requires, and confirm the ROM's SHA-1 with `fetch_rom.sh`.
2. Add a **full-generation mode** to the existing driver. The driver runs `Randomizer` with the real ROM, the same hooks and RNG tracer as the ROM-free mode, and a working directory under `tmp/`. It writes the output ROM, the JSON spoiler, and the familiar text record (seed, settings, world setup, pools, placement, the full RNG trace now including `generateRom`'s draws, the output and spoiler SHA-1s, and the outcome).
3. Add a **plain mode**: LADXR's own `main.main`, with no hook and no tracer. The only change is that `os.urandom` hands LADXR the case's seed. This mode proves that the hooked driver writes exactly what LADXR's own CLI writes.
4. Write **`tests/ladx/oracle/gen_golden.sh`**, the case runner. It reads cases from a case list (name, seed hex, `k=v` settings), runs each case in its own Python process through the full-generation mode, and writes one directory per case under an output directory that must be inside the repository's `tmp/`.
5. Extend **`determinism.sh` with `--full`**. It runs `gen_golden.sh` twice: run A on the setup checkout with `PYTHONHASHSEED=0`; run B on a fresh clone with `=1`, from a working directory inside a throwaway git repository that holds a tag. It then runs the plain mode once per case and compares every file of every case, never printing ROM bytes. It first proves its comparator on planted runs.
6. Extend the **order audit to environment reads**: subprocess, environment variables, the clock, the working directory, `os.urandom`, platform and version queries. Each site on the tier-1 path is made environment-free in `ladxr.patch` (its own commit, explained) or justified in the audit table.
7. Fix `setRomInfo`'s version string in `ladxr.patch`: the constant empty string. This is the value every oracle run so far has produced, and it is what the C port will write. The fix is shown red first, by run B's tagged working directory, then green.
8. Run the double run on the extended case list: the 12 existing cases plus a handful whose settings drive `generateRom`'s RNG and setting-dependent writes. Record the result in the step note.

## User Stories

1. As the owner, I want every case run twice with the ROM, with `PYTHONHASHSEED` 0 and 1 and the second run on a fresh clone, so that the ROM-writing half of LADXR is proven deterministic before any golden is generated from it.
2. As the owner, I want the comparison to cover the output ROM byte for byte, the JSON spoiler byte for byte, and the full RNG trace, so that a difference anywhere in generation is caught, not only in the placement.
3. As the owner, I want the second run's working directory to be a throwaway git repository with a tag, so that any read of the working directory's git state shows up as a difference instead of hiding behind this repository having no tag.
4. As the owner, I want `setRomInfo`'s `git describe` replaced in `ladxr.patch` by a fixed version string, so that tagging this repository, or running LADXR from a tagged checkout, can never change a golden.
5. As the owner, I want that fixed string to be the empty string, so that the outputs measured in step 2.1 stay valid and the C port writes a known constant.
6. As the owner, I want the version fix in its own commit, with the cause and the red-then-green evidence in the commit message, so that `ladxr.patch` stays reviewable fix by fix, as §2 requires.
7. As the owner, I want every environment read of LADXR's sources (subprocess, environment variables, clock, working directory, `os.urandom`, platform and Python version) listed and given a verdict, so that the next hidden dependency is found by an audit, not by luck.
8. As the owner, I want the audit to fail on a new environment-read site without a verdict, and on a stale verdict, in the same way it already does for order-sensitive sites, so that the audit stays true at the pin.
9. As the owner, I want proof that the hooked driver writes exactly the ROM and spoiler LADXR's own CLI writes for the same case, so that the tracer and hooks cannot have changed what the goldens will record.
10. As the owner, I want that proof run on every case of the double run, so that a hook that only perturbs some settings is caught too.
11. As the owner, I want the plain run to change only `os.urandom`, so that it follows LADXR's own path for a seed nobody typed: the device's path.
12. As the owner, I want a case's seed passed as bytes with LADXR's `seed` setting left empty, as on LADXR's site and on the device, so that the short-settings string written into the ROM matches what a player's generation writes.
13. As the owner, I want case seeds to be 16 bytes, as LADXR's `os.urandom(16)`, so that the cases exercise the seed length the device will use.
14. As the owner, I want every ROM-derived output (ROMs, spoilers, records with traces) written only under the repository's `tmp/`, so that nothing derived from the ROM is committed before step 5 defines a golden format that passes the Nintendo-bytes check.
15. As the owner, I want `gen_golden.sh` to refuse an output directory outside `tmp/`, so that the rule is enforced by the tool and not by memory.
16. As the owner, I want the comparison reports to give file names, sizes, differing-byte counts and offsets, never byte values, so that a failure report or step note cannot leak ROM bytes.
17. As the owner, I want the double run to check that the working tree has no new untracked file outside the git-ignored paths after the runs, so that a stray `LADXR_<seed>.*` file is caught here, before the push gate refuses a push for it.
18. As the owner, I want the double run to fail unless the two runs really differed in hash seed, checkout and working-directory git state, so that "identical" cannot come from two identical environments.
19. As the owner, I want the full mode to prove its comparator on planted runs (a flipped ROM byte, a different spoiler, a missing file, an extra file, a different trace line, an outcome mismatch with the plain run) before it runs LADXR, so that a broken comparator cannot report "identical".
20. As the owner, I want the ROM-free double run kept as it is, so that the step 1.8 evidence remains reproducible without the ROM.
21. As the run's agent, I want `gen_golden.sh` to take the LADXR checkout, the hash seed, the ROM, the working-directory root and the case names as options, so that the double run, step 4's cases and step 5's goldens all drive the same runner.
22. As the run's agent, I want `gen_golden.sh` to refuse a ROM whose SHA-1 is not `d90ac17e…` or that is missing, with a `setup:` line, so that a bad ROM is a setup problem and never a silently wrong output.
23. As the run's agent, I want a missing or unpinned LADXR checkout reported as a `setup:` line, so that the environment is fixed rather than the code.
24. As the run's agent, I want each case run in its own Python process, so that module-level state LADXR leaves behind in one generation cannot change the next case's output.
25. As the run's agent, I want LADXR's refusals recorded as outcomes (`error` for a failed fill, `crash` for an exception such as `goal=maze`'s `IndexError`), with no ROM and no spoiler written, so that these cases are double-run too and the C port knows which refusals to reproduce.
26. As the run's agent, I want the plain mode to report a second `os.urandom` call as `outcome retry` instead of letting LADXR retry 100 times, so that a refused seed costs one generation, and the driver's `error` matches the plain run's `retry`.
27. As the run's agent, I want the trace to fail loudly on an RNG method the tracer does not cover, as it already does, so that a method `generateRom` uses and the C port would miss is found here; the fix then extends the RNG vectors in its own commit.
28. As the run's agent, I want one line per case (`same <case>` or `DIFFERENT <case>: …`) and one summary line, in the style of the ROM-free run, so that a failure is easy to rank and to quote in the step note.
29. As the run's agent, I want the case list to stay one hand-written file read by both modes, extended with cases that drive `generateRom`'s RNG and setting-dependent writes, so that the "handful of settings cases" is explicit and the ROM-free run covers them too.
30. As the step-4 author, I want the case-list line format (name, 16-byte seed hex, settings in order) kept unchanged, so that step 4 can reuse it inside its case directories.
31. As the step-5 author, I want each case's run directory to hold the output ROM, the spoiler, the record with the placement and the full RNG trace by stage, and the SHA-1s, so that step 5 only adds the tagged write log, the source-reference encoding and the golden layout.
32. As the step-3 author, I want the full RNG trace to tag `generateRom`'s draws with their LADXR stage, so that the inventory can name which group draws from which generator, the enemizer's own generator included.
33. As a Phase 3 porter, I want the title screen's version string pinned to a known constant, so that the C port writes it without a ROM read or a guess.
34. As a future session, I want `CLAUDE.md` to describe `gen_golden.sh`, `determinism.sh --full`, the plain-equivalence check, the environment audit and `ladxr.patch`'s new kind of fix, so that the tools can be used without this PRD.
35. As the owner, I want the step note to record the case count, the run time, the Python version, every `ladxr.patch` change with its evidence, and any RNG method or environment read found, so that I can review the step at the PR.
36. As the owner, I want `make test` and `make check` unchanged in what they need (no ROM, no LADXR, no network), with only the new pure helpers unit-tested there, so that the net stays fast and ROM-free.

## Implementation Decisions

### What this step outputs, and what step 5 adds

- **Now (all under `tmp/`, never committed):** per case, a directory `<out>/<case>/` holding:
  - `run.txt`: the existing `oracle_run.py` sections in order (`seed`, `settings`, `ws`, `words_before`, `pool`, `words_after`, `placer_pool`, `place` (taken right after `RandomItemPlacer.run`, before `generateRom`), `trace` (every RNG call of the whole generation, spoiler included, each tagged with its `<module>:<qualname>` stage)), followed by `rom_sha1 <hex>` or `rom none`, `spoiler_sha1 <hex>` or `spoiler none`, and `outcome …`.
  - `out.gbc`: the output ROM, when the outcome is `ok`.
  - `spoiler.json`: LADXR's JSON spoiler, when the outcome is `ok` and the case is not `race`.
  - `meta.txt`: what must differ between runs and is never compared: `PYTHONHASHSEED`, `hash("ladxr")`, the checkout's path, the Python version, and the working directory's `git describe --tags` answer (or `-`).
- **Step 5 adds:** the committed golden layout in `tests/ladx/cases/`, the write log tagged by inventory group with its source-reference encoding, the trace and placement as golden files, writing goldens back into case directories (lifting the `tmp/`-only refusal at that point), `tests/ladx/README.md`, and the Nintendo-bytes check over goldens. No golden format is invented here, and nothing in `tests/ladx/cases/` is created.

### The driver (`oracle_run.py`)

- It gains `--rom <file>`, which turns on full generation. The `Randomizer` gets an `argparse.Namespace` carrying every attribute of LADXR's `main.py` parser at its default, except: `input_filename` (the ROM), `output_filename` (an absolute path for `out.gbc`), `spoilerformat` (`json`), `spoiler_filename` (an absolute path for `spoiler.json`) and `log_directory` (`None`). The implementer reads the attribute list from `main.py` at the pin. The plain-equivalence check catches a wrong default.
- The case's seed is passed as bytes (`seed=`), and LADXR's `seed` setting stays empty. This is LADXR's path when no seed is typed: `settings.seed` empty, `os.urandom(16)` replaced by the case's bytes. It is also the device's path, and it is what `oracle_run.py` already does in ROM-free mode. Running `main.py -s seed=<hex>` instead would put `<` and the seed into the short-settings string that `addBank3E` writes. That ROM differs and is not what a player gets, so it is not a case.
- The ROM is read only through LADXR (`ROMWithTables`). The driver itself never reads or prints ROM bytes.
- It gains `--plain`, which uses no `Driver`, no hook and no tracer. It chdirs into a throwaway directory under the work root, puts the checkout on `sys.path`, replaces `os.urandom` with a function that returns the case's bytes on its first call and raises a private exception on any later call, and calls `main.main([...])` with the ROM, `-o`, `--spoilerformat json`, `--spoilerfilename` and one `-s k=v` per setting. It writes the same `out.gbc` and `spoiler.json` files and a short record: `rom_sha1`/`rom none`, `spoiler_sha1`/`spoiler none`, and `outcome ok | retry | exit <code> | crash <type> <message>`. A second `os.urandom` call means LADXR's fill failed and `main.py` would retry with a new seed, so it maps to the driver's `outcome error`.
- Outcome equivalence between plain and driver: `ok` ↔ `ok` with identical `out.gbc` and `spoiler.json` (or both without a spoiler); `retry` ↔ `error`; `crash T` ↔ `crash T`. Anything else is a `DIFFERENT <case> (plain): …` line.
- `--meta` gains the working directory's `git describe --tags` answer, read by the driver with `git` before LADXR runs, so the double run can require that it differed.
- In both modes, `ladx_oracle.enter_checkout` picks the working directory under a work root given by the caller (default `tmp/ladx-oracle/`). LADXR's stray files land there, never in the repository.

### `gen_golden.sh`, the case runner

- Usage: `gen_golden.sh --out <dir> [--cases <file>] [--ladxr <dir>] [--hashseed <n>] [--rom <file>] [--work <dir>] [<case>...]`. Defaults: the case list is `determinism_cases.txt`, the checkout is `tests/ladx/oracle/ladxr`, the hash seed is 0, the ROM is `tmp/ladx/base.gbc`, and the work root is `tmp/ladx-oracle/`. With no case named, it runs every case of the list. An unknown name is a usage error.
- `--out` is required and must resolve inside the repository's `tmp/`. Otherwise the runner refuses with exit 2 and writes nothing. This is the step-2 rule that ROM-derived outputs stay under `tmp/`. Step 5 changes the rule when it defines the golden format.
- Before any case runs, it checks the ROM's SHA-1 against `d90ac17e…`, reading the file with Python's `hashlib` and printing only the SHA-1. A missing or wrong ROM gives `setup: …` and exit 2. A missing checkout, or a checkout not at the pin, also gives a `setup:` line, matching what `setup.sh` and `nintendo_bytes.py` do.
- It runs each case in its own `python3 oracle_run.py --rom …` process, without `-I` or `-E` (either one drops `PYTHONHASHSEED`), with `PYTHONHASHSEED` set and bytecode writing off.
- It prints one line per case, `<case>: outcome <ok|error|crash>`, then `gen_golden: <n> cases written to <dir relative to the repository>`.
- The case list is parsed in Python (`ladx_oracle.py`), not by shell `read`, so that `gen_golden.sh` and `determinism.sh` share one parser. The parser refuses a duplicate name, a seed that is not even-length hex, and a malformed line.
- It never calls the plain mode, and it never compares anything: it is the primitive that step 5's "two `gen_golden.sh` runs" verification is built on.

### `determinism.sh --full`

- Without arguments, the script behaves as today (ROM-free). The full mode, `--full`, does the following:
  1. **Self-proof** on planted case directories, before LADXR runs. The comparator must report each of these with the expected exact line, and report `same` on an identical pair:
     - one flipped byte in `out.gbc` (reported with its offset and differing count);
     - a different `spoiler.json` (reported by offset);
     - `spoiler.json` missing on one side;
     - an extra file on one side;
     - one differing `trace` line in `run.txt`;
     - a plain-run outcome `retry` against a driver `ok`.
  2. **Setup.** `setup.sh` on the default checkout, and `setup.sh --fresh` with `LADXR_DIR` set to the B clone, as today.
  3. **Tagged work root for B.** A throwaway git repository under `tmp/ladx-oracle/`, with one empty commit and one tag, is created fresh on each run. It is B's work root, so B's LADXR process runs inside a tagged repository while A's runs inside this one.
  4. **Untracked-file snapshot.** The list of untracked, non-ignored files (`git status --porcelain --untracked-files=all`) is taken before the runs.
  5. **Run A:** `gen_golden.sh --hashseed 0` on the default checkout into `tmp/ladx-oracle/full/a`. **Run B:** `gen_golden.sh --hashseed 1 --ladxr <B clone> --work <tagged root>` into `…/full/b`. **Plain run:** one per case, with the default checkout and hash seed 0, into `…/full/plain`.
  6. **Compare.** For each case: A against B, every file, and plain against A, by the outcome-equivalence rule. Each case prints `same <case>`, or one `DIFFERENT <case>: <file>: …` or `DIFFERENT <case> (plain): …` line per mismatching file.
  7. **Varied check.** It fails unless every case's two `meta.txt` files differ in `hash("ladxr")`, the checkout and the working directory's `git describe` answer.
  8. **Stray check.** It fails if the untracked, non-ignored file list grew, with one `STRAY <path>` line per new file.
  9. **Summary:** `determinism: <n> cases identical with the ROM (PYTHONHASHSEED 0 vs 1, <A checkout> vs <B checkout>, untagged vs tagged work directory), driver equal to LADXR's CLI`.
- The comparison helpers are pure functions in `ladx_oracle.py`, reached by new subcommands of its `main`:
  - `compare-bytes <name> <label> <a> <b>`: `same`, or `DIFFERENT <name>: <label>: sizes <a> vs <b>` or `DIFFERENT <name>: <label>: <k> bytes differ, first at 0x<offset>`. Never a byte value.
  - `compare-dirs <name> <a dir> <b dir>`: the file sets first (`only in A: <file>` / `only in B: <file>`), then `run.txt` through the existing `compare_runs`, and every other file through `compare-bytes`.
  - `compare-plain <name> <driver dir> <plain dir>`: the outcome-equivalence rule.
- The `run.txt` line comparison may print trace lines. Trace fields are lengths, indexes, RNG results and stage names, never ROM bytes. `run.txt` stays under `tmp/` in any case.

### Environment audit and `ladxr.patch`

- `ladx_oracle.AUDIT_SITE` is widened, or a second pattern is added, to environment reads: `subprocess`, `os.environ`, `os.getenv`, `datetime`, `time.`, `os.urandom`, `os.getcwd`, `platform.`, `sys.version`, `sys.platform`, `uuid`, `socket`. `audit_order.py` reports them in the same `UNAUDITED` / `STALE` format, and its self-proof gains one planted environment site.
- `order_audit.txt` gains one verdict per new site. A new verdict, `inert`, means that the value read cannot reach the RNG, the placement, a ROM write or the spoiler (for example `randomizer.py`'s `datetime`, used only with `--logdirectory`, which the oracle never passes, and `main.py`'s `--timeout` thread). `patched` and `unreached` keep their meaning. The table's header comment and `CLAUDE.md`'s description say that `ladxr.patch` now holds order fixes and environment fixes.
- **`setRomInfo`'s version:** `ladxr.patch` replaces the `subprocess.run(['git', 'describe', …])` block with `version = ""`. It is an empty string, not the pin's upstream describe, for three reasons:
  - it is what every oracle run has produced so far, including step 2.1's calibration output SHA-1s;
  - it is what a tagless checkout gives;
  - the C side then writes nothing for the version.
  The audit marks the site `patched`. The commit message quotes the red run, `DIFFERENT <case>: out.gbc: …` from the tagged B work root, and the green run.
- Any other difference the full double run finds is fixed the same way: in `ladxr.patch`, one commit per fix, the cause and evidence in the message, the site marked `patched` in the audit when it is a site. The ROM-free double run and `gen_fixtures.sh --check` are re-run after each `ladxr.patch` change, and must still pass byte-identical. A fixture change would be its own regeneration commit with the reason.
- An RNG method that `generateRom` calls and the tracer does not cover stops the run with the tracer's `OracleError`. The fix extends `COVERED`, `TracingRandom` and `gen_pyrandom.py`'s vectors, in its own commit. The audit at this PRD's writing found only `choice`, `randint`, `randrange` and `shuffle` on `generateRom`'s path, all covered.

### The case list

- `determinism_cases.txt` keeps its format and its 12 cases. It gains cases whose settings drive `generateRom`'s RNG or setting-dependent writes, each with a 16-byte seed:
  - `enemies` at every non-default value;
  - `music=random` and `music=shifted`;
  - `textmode=none` (the `reduceMessageLengths` path);
  - `hardmode=ohko`;
  - `superweapons=1`;
  - `evilshop=1`;
  - `bowwow` at a non-default value;
  - `quickswap` at a non-default value;
  - `linkspalette` at a non-default value.
  
  Several of these may share one case where the settings are independent, so that the list stays a handful (about 8 new cases).
- Each new seed is chosen so that LADXR places its items (outcome `ok`), except where an error path is wanted (`pain` already covers one). Both modes read the extended list, and the ROM-free double run must also pass on it.

### Unchanged

- `make test`, `make check`, `make lint` and the CI workflow are unchanged, except that `test-ladx-oracle` picks up the new unit tests automatically.
- The push gate is unchanged. The implementer does not push. The orchestrator pushes with `LADX_ROM_URL` exported, the link obtained only with the command given to the session, and never written anywhere.

## Implementation Chunks

1. **Pure helpers, with unit tests:**
   - the case-list parser;
   - `compare_bytes`, `compare_dirs` and `compare_plain`;
   - the path guard "inside the repository's `tmp/`";
   - the widened audit pattern and the `inert` verdict.

   Tested in `test_oracle.py`, with no LADXR, no ROM and no network. `audit_order.py`'s self-proof gains the planted environment site. `order_audit.txt` gains a verdict for every new site except `setRomInfo`'s `subprocess` line, which gets `patched` in chunk 4's commit together with the fix. Until then `audit_order.py` (outside the net) prints exactly one `UNAUDITED patches/titleScreen.py:…` line, which is expected and recorded.
2. **Driver modes and `gen_golden.sh`:**
   - `oracle_run.py --rom` and `--plain`, and `--meta`'s describe field;
   - `gen_golden.sh` with its `tmp/` guard, ROM SHA-1 check and `setup:` lines.

   Verified by running the baseline and one `race` case with the ROM into `tmp/`; by the refusal of an `--out` outside `tmp/` and of a planted wrong ROM (a 1 MiB file of zeros under `tmp/`); and by `git status` unchanged.
3. **`determinism.sh --full`:** self-proof, tagged B work root, plain run, varied check and stray check. Run on the extended case list. The expected first result is `DIFFERENT` on every `ok` case's `out.gbc`, from `setRomInfo`'s version under B's tag. This is the red evidence; record it.
4. **`ladxr.patch` fixes:** `setRomInfo`'s version first, in its own commit, with the audit verdict. Then any further difference the full run shows, one commit each. Re-run `determinism.sh` (ROM-free), `determinism.sh --full` and `gen_fixtures.sh --check`, all green. Then update `CLAUDE.md`.

## Testing Decisions

- A good test here checks what a tool prints and returns on inputs it does not control: planted directories, planted files, planted case lists. It does not check how the tool is built. The comparison helpers are tested only through their verdict strings and exit codes, as `compare_runs` and `runs_varied` already are in `test_oracle.py`.
- **Unit-tested in `make test`** (`test_oracle.py`, `python3 -I`, standard library only):
  - **Case-list parser:** valid lines, comments and blank lines, a duplicate name, odd-length hex, a non-hex seed, name selection, an unknown name.
  - **`compare_bytes`:** equal; different size; one flipped byte (count 1, offset given); several differences (count and first offset); and the property that no byte value appears in the verdict, checked by planting distinctive byte values and asserting their hex is absent.
  - **`compare_dirs`:** file only in A, file only in B, `run.txt` difference through `compare_runs`, binary difference, all equal.
  - **`compare_plain`:** each equivalence pair and each mismatch.
  - **The `tmp/` path guard:** inside, outside, `..` escapes, a symlink pointing outside.
  - **The widened audit:** a planted environment site is `UNAUDITED`, the `inert` verdict is accepted, and a stale environment entry is `STALE`.
  - **`runs_varied`:** with the new describe key.
- **Self-proved at every run, outside the net:** `determinism.sh --full` (planted directories, before any LADXR run), as `determinism.sh` already does for traces, and `audit_order.py` (planted tree).
- **Not unit-tested:** `gen_golden.sh` and the driver's `--rom` and `--plain` modes need LADXR and the ROM. They are verified by the double run itself, and by the plain-equivalence check, which is their real test: any wrong `Namespace` default, perturbing hook or tracer side effect appears as a `(plain)` difference.
- **Prior art:**
  - `test_oracle.py` (helpers tested through strings);
  - `determinism.sh`'s planted-trace self-proof;
  - `audit_order.py`'s planted tree;
  - ALttP's `tests/alttp/oracle/gen_golden.sh`, for the runner's option shape (`--out`, case names) and its "never called by make" rule.
- **Step-note evidence:**
  - the `determinism.sh --full` summary line, with the case count and wall time;
  - the red line before the version fix;
  - `audit_order.py`'s summary;
  - `gen_fixtures.sh --check` and the ROM-free `determinism.sh` still green;
  - the Python version;
  - `make check` green.

## Out of Scope

- The golden format, the write log, group tags, source-reference encoding, `tests/ladx/README.md`, and writing anything into `tests/ladx/cases/` or `tests/ladx/combos/` (step 5).
- The full case and combo lists covering every tier-1 value (step 4). This step's double run covers the baseline and a handful of settings cases only.
- The inventory, the read trace on real runs, and the decision of each call's side (step 3). This step only makes the stage-tagged trace available.
- Patch tables (step 6), extraction fixtures (step 7), `rom-check` (step 9).
- Changing the Nintendo-bytes check or the push gate.
- Any C code.
- Wiring any of this into `make test`, `make check` or CI (the ROM never enters the net).

## Further Notes

- Measured while writing this PRD, in a throwaway `tmp/` directory with LADXR's plain CLI and `-s seed=<hex>`:
  - one full generation takes about 0.6 s;
  - five settings sets gave identical ROM and spoiler SHA-1s under hash seeds 0 and 1 (baseline-like seed `03`, hell + keysanity, casual + `music=random` + `owlstatues=both`);
  - two were refused by LADXR for their 1-byte seed (`goal=bingo` with random bosses, and `race`).

  The full mode's cost is roughly 3 runs × about 20 cases × 1 s, plus one clone, which is well under the time `gen_fixtures.sh` takes.
- `randomizeEnemies` builds its own `random.Random(seed)`. Under the tracer it is traced with generator name `rnd`, like the main generator, and told apart by its stage (`patches.enemies:randomizeEnemies`). Step 3's inventory and step 5's trace format should name it explicitly. Recording this is enough here.
- `setRomInfo` stays a seed-dependent writer (it writes the seed's hex, or `Race`, on the title screen). Pinning the version only removes its environment input. Step 3's inventory row for it should say "version: the constant empty string (`ladxr.patch`)".
- A refused 1-byte seed (step 2.1's `01`) and the `goal=maze` crash are LADXR behaviours the C port must reproduce as refusals. Recording them as outcomes, never as failures of the runner, keeps them visible.
- The ROM link is used only by the orchestrator's push. No script in this step reads `LADX_ROM_URL`: the ROM is already in `tmp/ladx/base.gbc`, and `fetch_rom.sh` re-checks it at session start.
