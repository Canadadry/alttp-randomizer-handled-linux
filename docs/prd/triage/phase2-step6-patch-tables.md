---
title: "LADX patch tables"
description: "The C side must apply LADXR's settings-only writes without the ROM and without LADXR's assembler. Precompute them once, with the ROM, into committed patch tables under data/ladx/, keyed by what each write depends on, in the order the C side must apply them. Prove that they compose into every golden's output SHA-1 and into a fresh random sample."
status: needs-triage
---

## Step (verbatim from `docs/plan-3.md`, §3 Phase 2 item 6)

6. **Patch tables.** `tests/ladx/oracle/gen_tables.sh` writes `data/ladx/`:
   - one table per settings-only patch group of the inventory, keyed by the setting values it depends on;
   - its layer of fixed patches applied whatever the settings;
   - the order the C side must apply them in.

   Verified by:
   - a host replay, Python on the oracle side: applying the tables to the real ROM, plus each case's seed-dependent writes taken from its golden write log, gives exactly the case's output SHA-1, for every case and combo. This is the proof that the tables compose.
   - a random sample: LADXR run live on a fixed, recorded sample of random tier-1 setting combinations, each with a fixed seed (sample size and generator in the step note); for each, the tables chosen by their keys plus that run's own seed-dependent writes give LADXR's output SHA-1. A mismatch means a table key misses a dependency (the assembler's global constants, `generator.py:80-89`, and cross-conditions such as `hardmode == 'ohko'` make this likely): widen the key, regenerate, and repeat until the whole sample passes. Each sample combination is then committed as a combo case in `tests/ladx/combos/` with its golden, under step 5's checks, so Phase 3 claims it too;
   - the Nintendo-bytes check printing `0 literal runs` for `data/ladx/`.
   - the total size of `data/ladx/` in the step note.

## Problem Statement

Phase 3 ports only LADXR's seed-dependent part to C (§2, "Hybrid port"). Everything else that `generateRom` writes must reach the device as data. That covers the assembled ASM patches and the fixed `rom.patch` calls each setting value turns on. The C side applies this data, with no ROM in the repository, no assembler and no allocation (Phase 3 step 5).

Phase 2 step 3's inventory (`notes/ladx-inventory.md`) has five `table` groups: `fixed`, `options`, `frame-counter`, `goal-fixed` and `music-fixed`. Step 5's write log (`tests/ladx/README.md`) already encodes their writes in every golden, and says that "C copies their encoding from step 6's tables, built with this same encoder". Nothing produces those tables yet.

Reading the 116 goldens and their `tmp/ladx-goldens/` event streams (throwaway analysis while this PRD was written) showed that a naive table, one blob per group keyed by the Groups table's `Settings key`, would be wrong in three ways:

1. **Hidden dependencies.** The `fixed` group's key is `-`. Yet 12 of its rows (`fixChests`, `patchRooster`, `fixDroppedKey`, `updateTarin`, `upgradeHealthContainers`, `fixGoldenLeaf`, `fixHeartPiece`, `fixSeashell`, `fixInstruments`, `upgradeMarin`, `upgradeManbo`, `upgradeMamu`) write different bytes under `textmode=notextbox`. They read the assembler constant `ITEM_DIALOGS` (`generator.py:80-89`), which no grep of the call line shows.
2. **Interleaving.** Table-group writes are not one block. In the baseline, `fixed` alone has 7 segments, with `options`, `bank3e`, `marin` and `hints` between them. Where a C group overwrites a table byte, or a table byte lands on a C group's write, the order decides the output.
3. **Encoding coupling.** Table-group atoms are chosen per golden segment. A source match can span two rows. This happens in 5 golden segments: `slowLowHPBeep` and `removeLowHPBeep` under `hardmode=ohko` share one `@q+6` copy. Three bytes are also rewritten by a second row of the same segment. If each row were encoded alone, its tokens would differ from the golden's, and Phase 3 step 5's projection claims would fail on correct C code.

The `frame-counter` group is also keyed by 8 settings in the inventory. Their product is about 31,000 combinations. Its writes depend on one value only, the non-key spot count passed to `addFrameCounter`, which shows up as three digit bytes.

## Solution

`tests/ladx/oracle/gen_tables.sh` (with the ROM and the LADXR checkout at the pin, else a `setup:` line, exit 2) writes `data/ladx/`. The C side applies these files in Phase 3; this step produces and proves them.

**Files and format.**

- `data/ladx/order.txt` lists every writing row of the inventory's sections in LADXR's statement order. A row is a `table` or `C` row (`generateRom`, `Randomizer.__init__` after the fill, `ROMWithTables.save`, `ROM.save`, `ROM.fixHeader`). Each line gives the row's side, its group and its inventory key (the `Call` text, ` #<k>` included). A `table` row also gives the id of the unit it belongs to.
- `data/ladx/<group>.txt` is one file per `table` group of the inventory. Each holds that group's **units**. Every unit has its **key components**, then one **entry** per reachable key tuple. An entry is a key tuple followed by the unit's net writes as write-log records (`B` and `T` records, step 5's tokens `=n`, `@q+n`, hex literals, `p<v>`), or the word for "writes nothing".
- The **fixed layer** is the units whose key is empty: one entry each, applied whatever the settings.

**Units.**

- A unit is one `table` row by default.
- Rows are **coupled** when, in any coverage run, a reference token in a golden-encoded segment spans bytes that both rows wrote last, or one row rewrites a byte the other wrote in the same segment.
- Coupled rows are fused into one unit, transitively. A fused unit's key is the union of its rows' keys. Its entries hold the segment-level encoding of the rows together, so its tokens are the golden encoder's.
- A hand-written input, `tests/ladx/oracle/table_units.txt`, can force a fusion with its reason. It starts empty.

**Keys.** A unit's key components are the union of:

- the row's `Settings` column in the inventory; when that column is `-`, its group's `Settings key` (`goal-fixed` gives `goal`, `music-fixed` gives `music`). `frame-counter` is the exception (last item);
- the settings the row's call reads, traced at run time (attribute reads on LADXR's settings object while the row runs);
- the assembler constants its assembly reads, traced at the assembler's constant lookup. They are keyed by the constant's value (`ITEM_DIALOGS=0`), which C computes exactly as `generateRom` does;
- the key components of every unit whose writes it reads or carries (read-trace provenance `written:`/`mixed:`, and carried write identities);
- `tests/ladx/oracle/table_keys.txt`, a hand-written widening (`<row> <component>` with its reason). It starts empty. A widening is how a sample mismatch is fixed (below);
- for `frame-counter`, `count=<n>`, the non-key spot count `addFrameCounter` receives, in place of the group's 8 settings.

A unit whose writes depend on a `C` group's writes is a `DEPENDS-C <unit> <group>` failure. That means its inventory side is wrong, and it is fixed at the inventory (§2).

**Domains and coverage.**

- A component's domain is its setting's tier-1 values (`tier1.txt`, cut values excluded), a constant's reachable values, or the reachable counts.
- A unit's reachable tuples are the image, under its key, of every combination of the underlying settings' tier-1 values after LADXR's live `Settings.validate`.
- The counts are computed ROM-free by LADXR's logic (the driver stopped after the item pool, about 17 ms per combination; about 9 minutes over `frame-counter`'s 8 settings).
- Each reachable tuple gets a witness assignment: the defaults overridden by it, else the first enumerated combination that gives it.
- Witnesses are merged greedily, and deterministically, into the fewest **coverage runs**.
- Each run is a full LADXR generation, with the ROM, events and read trace on, and no invariant re-run. Its seed is `derive("ladx-tables <run>", k)` for the first `k < 64` with outcome `ok` (the cases' pick rule).
- A tuple with no `ok` run is `UNREACHED <unit> <tuple>` (exit 1).

**Building.**

- Every coverage run's stream is encoded with `write_log.py`'s own encoder, exactly as the goldens are.
- Each unit's entry is the unit's net records, cut out of its golden-encoded segment.
- Two runs giving one tuple different contents (values or tokens) is `CONFLICT <unit> <tuple>` (exit 1).
- The tables are built from the coverage runs only. The case goldens and the sample are never inputs, so they remain independent evidence.

**Verification.** `python3 -I tests/ladx/oracle/table_replay.py [--root <dir>] [--mirror <dir>] [--rom <file>] [--tables <dir>] [<case>...]`, with the ROM, checks every case and combo:

- **SHA-1.** Start from the ROM. Walk `order.txt`. At a `table` row that starts a unit, apply the unit's entry for the case's key. At a `C` row, apply the golden's `C` segments that the row wrote, placed by the mirror's event stream (segments align one-to-one with the golden's). The result's SHA-1 must equal `run.txt`'s `rom_sha1`.
- **Projection.** Each table group's projection of the replayed log (`write_log.py project`) must equal the golden's, so Phase 3 step 5's claims hold on tables C copies.
- Output: one `ok <case>` or `BAD <case>: sha1|projection <group>|key <unit>: ...` line each, then `table-replay: <n> cases, <b> bad`. Exit 1 on a bad case.

**Random sample.**

- `gen_tables.sh --sample` writes 12 lines, `sample <nn> k=v ...`, into `tests/ladx/oracle/combos.txt`, once. They are never re-drawn.
- Line `i` picks, for every tier-1 setting in LADXR's order, the value at index `SHA-256("ladx-sample <i> <key>") mod <number of values>`. Only non-default values are written, as validate returns them.
- A draw that validate rewrites into a refused pair (`boomerang=default` with `accessibility=all`) is re-drawn with the suffix ` #<j>`, the first `j` that is not refused.
- `ladx_cases.py` names these combos `sample-<nn>` (a setting-joined name would exceed a path component). It picks their seeds by the existing rule, and `gen_golden.sh` writes their goldens.
- `table_replay.py` must pass on all 12.
- On a mismatch: find the missing dependency, widen it in `table_keys.txt`, regenerate, and repeat.
- The sample combos then live under every step-5 check (`golden_check.py`, `determinism.sh --full` on them, `ladx-cases` with its budget).

**Nintendo bytes.**

- `data/ladx/` files get the scope `tables` (guideline-style decision below).
- A file must parse with the tables parser, else one `tables ... unparsed` literal.
- Its literal tokens are checked at 4 bytes at their byte alignment (view `tables`). The rest of its text is checked at the general thresholds.
- `nintendo_bytes.py data/ladx/` must print `0 literal runs`. The push gate inherits the scope through `check_blob`.

**Make test.** `python3 -I tests/ladx/oracle/tables.py check` (no ROM, no LADXR, no network) runs in `make test` as `ladx-tables`. It first proves itself on planted trees. Then it checks:

- every `data/ladx/` file parses;
- `order.txt`'s rows equal the inventory's `table` and `C` rows, in its section order, with their groups and sides;
- every `table` group of the inventory has its file, and no other file exists;
- every `table` row is in exactly one unit, and fused rows are consecutive among the table rows;
- no unit has a duplicate tuple, and every tuple's components are the unit's own, with values from `tier1.txt` (constants 0/1, counts decimal).

Output: one `MISSING`, `EXTRA`, `BADROW`, `DUP` or `BADKEY` line per mismatch (exit 1), else `ladx-tables: <u> units in <g> groups, <e> entries, <s> bytes`. Coverage completeness against reachability needs LADXR and is proven by `gen_tables.sh` itself, not here.

**Determinism.** `gen_tables.sh --check` rebuilds the tables into `tmp/ladx-tables/` with `PYTHONHASHSEED=1` on a fresh clone. It prints `DIFFERENT`, `MISSING` or `EXTRA` per file unlike the committed `data/ladx/`, else `tables: <n> files byte-identical to data/ladx`.

## User Stories

1. As the Phase 3 C porter, I want every settings-only write of LADXR as data in `data/ladx/`, so that the device never needs LADXR's assembler or the ROM's patched code.
2. As the Phase 3 C porter, I want one file per inventory `table` group, so that Phase 3 step 5's write-log claims map one file to one claimed group.
3. As the Phase 3 C porter, I want each entry's records in step 5's exact token language, so that the C logger copies tokens and its projection equals the golden's.
4. As the Phase 3 C porter, I want `order.txt` to interleave table units with `C` rows, so that overlapping writes land in LADXR's order.
5. As the Phase 3 C porter, I want keys stated as setting values, assembler-constant values or the spot count, so that C can compute every key from validated settings and its own item list.
6. As the Phase 3 C porter, I want an explicit "writes nothing" entry for every reachable tuple where a unit's condition is false, so that a missing entry always means a bug, never "skip".
7. As the Phase 3 C porter, I want the fixed layer to be the units with an empty key, so that it is applied without any lookup.
8. As the owner, I want the tables proven against every one of the 116 goldens by SHA-1, so that I know they compose with the seed-dependent writes.
9. As the owner, I want the tables proven against a random sample they were not built from, so that a missing key dependency cannot hide behind the hand-picked cases.
10. As the owner, I want the sample's generator and size recorded, so that the sample can be re-derived and audited.
11. As the owner, I want each sample combo committed as a case with its golden, so that Phase 3 claims it like any other case.
12. As the owner, I want `0 literal runs` from the Nintendo-bytes check on `data/ladx/`, so that the repository stays legal.
13. As the owner, I want the total size of `data/ladx/` in the step note, so that I can judge what the zip ships in Phase 4.
14. As a maintainer, I want hidden dependencies (assembler constants, settings read inside a callee, earlier table writes) traced rather than guessed, so that keys are right the first time and the sample only confirms them.
15. As a maintainer, I want coupled rows fused automatically from the observed encodings, so that a cross-row source match never makes C's tokens differ from the golden's.
16. As a maintainer, I want `CONFLICT`, `UNREACHED` and `DEPENDS-C` failures named per unit and tuple, so that a wrong key, an impossible tuple or a mis-sided row is found at once.
17. As a maintainer, I want widenings and forced fusions in two small hand-written oracle inputs with reasons, so that every fix lives at the oracle (§2), never in a hand-edited table.
18. As a maintainer, I want `gen_tables.sh --check` to rebuild byte-identical tables under another hash seed and a fresh clone, so that the tables are as deterministic as the fixtures.
19. As a maintainer, I want `make test` to check the tables' structure against the inventory without the ROM, so that an inventory edit that makes the tables stale fails fast.
20. As a maintainer, I want `table_replay.py` to say which group or unit diverged on which case, so that a failure leads straight to its unit.
21. As a maintainer, I want the replay to take `C` writes from the committed goldens and the mirror's event streams, so that it checks the tables, not a fresh LADXR run.
22. As a maintainer, I want the tables built only from coverage runs, so that the 116 goldens and the sample stay independent evidence.
23. As a maintainer, I want the spot-count domain computed ROM-free from LADXR's logic, so that `frame-counter` gets one entry per reachable count instead of about 31,000.
24. As the push gate, I want `data/ladx/` checked in a format-aware scope, so that table files pass on content, not on coincidences of their record syntax with ROM text.
25. As a reviewer, I want the table format documented in `tests/ladx/README.md`, so that Phase 3 has one reference for both the write log and the tables.
26. As a reviewer, I want `CLAUDE.md` to describe `gen_tables.sh`, `table_replay.py`, `tables.py check`, the `tables` scope and `sample-<nn>` combos, so that later sessions find them.
27. As the plan-pilot run, I want the step note to list the coverage-run count, the sample, any widening or fusion, the Nintendo result and the size, so that Phase 3 step 5's PRD starts from facts.

## Implementation Decisions

- **Unit granularity: row, fused on coupling.** The golden encoder chooses table-group atoms per segment, and segment boundaries move with the settings (an `options` row that writes nothing merges two `fixed` segments). Rows are the finest unit whose atoms are stable.
  - Coupling is detected from the coverage runs' encodings and fused transitively, and the replay's projection check proves the result on every golden.
  - Rejected: one unit per group (segment boundaries vary); encoding each row alone (5 golden segments span rows); storing whole segments (their row set varies with settings).
- **Keys are traced, then proven.**
  - Declared settings (the inventory) plus run-time reads cover what grep cannot see: `*` arguments such as `patchTradeSequence(rom, settings)`, and assembler constants.
  - Constants are keyed by their value, not by the four or three setting values behind them, which keeps `options`' products small.
  - `frame-counter` is keyed by `count`, the one value its writes depend on. The deviation from "keyed by setting values" is deliberate: the count is a function of the settings, and C computes it from the item list Phase 3 step 4 builds.
- **Encoding is the golden's.** Entries are cut from the coverage runs' streams encoded by `write_log.py`'s own encoder. There is no second encoder, so tables and goldens cannot drift apart.
- **Order.** `order.txt` holds rows, not segments: the statement order is fixed, while segments are not. A fused unit is applied at its first row. If a fusion spans a `C` row that writes in some run, it is a `FUSE-ACROSS <unit> <row>` failure, settled then (forced fusion or a widened key).
- **Reachability.**
  - Tuples come from the product of the underlying tier-1 values after live `Settings.validate`, with cut values excluded.
  - LADXR's refused pair `boomerang=default` with `accessibility=all` is reached through its covering combo's companion setting (`accessibility=goal`).
  - Unreachable tuples get no entry. C applies validate first (Phase 3 step 2), so it never looks one up.
- **Coverage runs** use the case seed rule's derivation with their own label, so their seeds are reproducible and are not case seeds. Runs go under `tmp/ladx-tables/` only. `gen_tables.sh` refuses any `--out` outside `tmp/` (as `gen_golden.sh` does), except its default target `data/ladx/`. `oracle_run.py` may gain an option that skips the invariant re-run, for speed; the outputs it does write stay unchanged.
- **Nintendo-bytes scope `tables` for `data/ladx/`.** This is a deliberate, recorded change of step 2.1's strict rule for this path, following step 2.5's precedent for the case folders' `rom_writes.txt`.
  - Measured: table-group records under the strict scope gave 107 literal runs on 4 goldens' worth of records (~100 KB). 104 were raw-view coincidences of the record syntax (`T texts 7d`) with ROM text, 3 were hex-view runs at the other nibble alignment. None was ROM content carried as a literal.
  - The table encoder's rule 3 already guarantees that no table-group literal of 4 bytes or more at byte alignment occurs in the ROM.
  - The `tables` scope keeps the strict 4-byte threshold on every literal token, at the alignment its bytes really have. The `computed` and `invariant` clearances do not apply (table groups need neither), and the rest of the text is checked at the general thresholds.
  - Rejected: a decimal or binary format. Decimal byte lists hit ROM digit text such as `200 `, and binary hits structured ROM data.
- **Sample: 12 combos, `sample-<nn>`.** The budget leaves room for about 18 more cases: `rom_writes.txt` 19.1 of 22 MiB, all golden files 30.7 of 34 MiB. 12 random combos fit, at up to about 200 KB of write log and 300 KB of golden files each, without raising the budget. `combos.txt` gains the line kind `sample <nn> k=v ...`. `ladx_cases.py` (check, list, pick, verify and their self-proofs) accepts it, with the folder named `sample-<nn>`.
- **`tables.py`** is the format's module: `parse`, `render` (canonical, so parse then render is the identity), `select(tables, settings_key_values)` giving the ordered records to apply, and `check`. `gen_tables` and `table_replay` share it, and `nintendo_bytes.py` uses its parser for the `tables` scope.
- **README.** A "Patch tables" section in `tests/ladx/README.md` covers the files, the units, the keys, the entry grammar, the order walk and the "writes nothing" entry. `data/ladx/` holds only the table files, since prose there would fall under the `tables` scope.
- **Fixes stay at the oracle (§2).** A wrong key, fusion or side is fixed in `table_keys.txt`, `table_units.txt` or the inventory, never in `data/ladx/`. The tables are then regenerated in their own commit, with the reason.

## Implementation Chunks

1. **Table format, structural check and Nintendo scope (ROM-free).**
   - `tables.py`: parse, render, select and `check`, with its planted-tree self-proof.
   - `test_tables.py` in `test-ladx-oracle`.
   - The `tables` scope in `nintendo_bytes.py`, with planted files: a byte-aligned 4-byte literal refused, syntax coincidences passing, an unparsable file refused. Its self-test count is updated.
   - One planted table push in `push_gate.py --self-test`.
   - `ladx-tables` is not yet wired into `make test`, because no `data/ladx/` exists.
2. **Units, keys and coverage planning (ROM-free logic, planted inputs).**
   - Coupling and fusion from encoded streams.
   - Key derivation: inventory columns, traced settings and constant reads, provenance, `table_keys.txt` and `table_units.txt`.
   - Domain and reachability enumeration (validate and the spot count through LADXR's driver).
   - The deterministic witness merge into coverage runs.
   - The settings-read and constant-read tracing hooks with their planted self-proof (as `read_trace.py` proves its channels).
   - Unit-tested on planted streams and inventories.
3. **`gen_tables.sh` with the ROM: build and determinism.**
   - Runs the coverage runs, encodes them, cuts the entries, reports `CONFLICT`, `UNREACHED`, `DEPENDS-C` and `FUSE-ACROSS`, and writes `data/ladx/`.
   - Adds `--check`.
   - Commits `data/ladx/` in its own commit, after `nintendo_bytes.py data/ladx/` prints `0 literal runs`.
   - Wires `ladx-tables` into `make test`.
4. **Host replay.**
   - `table_replay.py` with its planted self-proof: a flipped table byte gives `BAD sha1`, a token swap with the same bytes gives `BAD projection`, and a wrong key gives `BAD key`.
   - Run on all 116 cases and combos until 0 bad. Any fix goes to chunk 2's inputs, followed by a regeneration.
5. **Random sample.**
   - `--sample` writes the 12 `sample` lines, and `ladx_cases.py` supports them (self-proof updated).
   - `pick` seeds them, `gen_golden.sh` writes their goldens, and `golden_check.py`, `determinism.sh --full` on them, and `ladx-cases` with its budget pass.
   - `table_replay.py` passes on them, after widening and regenerating if needed.
   - Final runs: `nintendo_bytes.py` on `data/ladx/`, `tests/ladx/README.md` and `CLAUDE.md` updates, and the step note facts (sample generator and size, coverage-run count and time, widenings and fusions, `data/ladx/` total size, Nintendo result).

## Testing Decisions

- A good test drives a tool through its public interface (CLI or module functions) on planted inputs and checks the printed lines and exit code. It never inspects private helpers. Each tool proves itself on planted inputs before it trusts real data, as every Phase 2 tool does.
- **ROM-free tests in `make test` (`test-ladx-oracle`):**
  - `test_tables.py`: parse/render round trip, canonical-form refusals, `select` order across interleaved `C` rows, and each `check` failure kind on a planted tree.
  - `test_gen_tables.py`: coupling detection on planted segments (a cross-row copy token, a rewritten byte), key union rules, constant- and count-keyed components, reachability under a planted validate, the witness merge being deterministic and minimal on a planted case, `CONFLICT`/`UNREACHED`/`DEPENDS-C`/`FUSE-ACROSS` reporting.
  - `test_table_replay.py`: replay on a synthetic ROM with planted tables, goldens and event streams; the three `BAD` kinds.
  - Extended `test_nintendo_bytes.py` and `test_push_gate.py` for the `tables` scope; extended `test_ladx_cases.py` for `sample` lines.
- **`ladx-tables`** in `make test` checks the committed `data/ladx/` structurally against the inventory.
- **With the ROM, outside the net:**
  - `gen_tables.sh` (its failures are the build's own checks);
  - `gen_tables.sh --check` (determinism);
  - `table_replay.py` on every case, combo and sample combo;
  - `nintendo_bytes.py data/ladx/`;
  - step 5's `golden_check.py` and `determinism.sh --full` on the sample combos.
- Prior art: `ladx_cases.py check` and its planted trees; `golden_check.py` (per-case `ok`/`BAD` lines with the ROM); `gen_fixtures.sh --check` (two-render determinism); `write_log.py` (encoder, `project`, `decode`); `read_trace.py` (planted-channel self-proof for the new tracing hooks).

## Out of Scope

- The C application of the tables (Phase 3 step 5) and shipping `data/ladx/` in the zip (Phase 4 step 4).
- Extraction fixtures for `C` groups (step 7), the repository-wide Nintendo-bytes sweep (step 8), `rom-check` (step 9) and the Phase 3 step 6 split (step 10).
- Changing the write-log format or the encoding rule of step 5. The tables reuse both as they are.
- Changing any inventory side, unless a `DEPENDS-C` failure proves it wrong. Such a change is then a separate commit, with its reason and the inventory and read checks re-run.
- Regenerating any existing golden. Only the 12 new sample combos get goldens.
- Raising the golden size budget.

## Further Notes

- Throwaway analyses for this PRD are under `tmp/prd6/` and the session scratchpad. Their figures:
  - `fixed`'s 12 `notextbox`-dependent rows;
  - the 5 cross-row copy tokens;
  - the 945 `fixed` and 600 `options` segments over 116 goldens;
  - 107 strict-scope literal runs on table-group records;
  - 17 ms per pool-only LADXR run.
- If the coverage runs turn out much slower than about 5 s each, record the time and the run count in the step note. Do not cut coverage.
- The sample is drawn once and committed. Re-drawing it to get green would be weakening a check.
