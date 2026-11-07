---
title: "LADX table application, claimed by write-log projections"
description: "The C port of LADXR stops after the fill: it cannot apply the committed patch tables of data/ladx/, writes no write log, and the e2e driver claims nothing of rom_writes.txt."
status: needs-triage
---

## Problem Statement

Plan 3, Phase 3, step 5, verbatim from `docs/plan-3.md` (§3, Phase 3, item 5):

> 5. **Table application.** Apply `data/ladx/` for a given settings set, in the order recorded, with no allocation. Verified by unit tests on synthetic tables, and by the write-log claims for the settings-only groups reaching every case. This first write-log step also self-proves the LADX e2e driver's claims on planted write logs: a divergence inside a claimed group prints `DIVERGENT`; one only in an unclaimed group, before or after it, does not; a literal-for-reference swap with the same bytes does; and so does a write with the wrong group tag.

Today the LADX library (`ladx/`) reaches the end of LADXR's fill: `ladx_gen <case_dir> <out_dir>` writes a `run.txt` whose `fill` projection equals the golden's on all 88 cases and 40 combos (step 3.4). Nothing in C reads `data/ladx/` (Phase 2 step 6: `order.txt` and the five `table` groups `fixed`, `frame-counter`, `goal-fixed`, `music-fixed`, `options`; 91 units, 676 entries, 534,590 bytes), nothing in C writes a `rom_writes.txt`, and `tests/ladx/run_e2e.py` compares only `run.txt`. The 128 goldens' write logs are checked by nothing on the C side. Every later step needs this: the nineteen seed-dependent sub-steps (6.1–6.19) are claimed as write-log projections through this step's driver, and the assembly pipeline (step 7) applies the tables to the real ROM.

The step's evidence is Phase 2's: the table format and its order walk (`tests/ladx/README.md`, "Patch tables"; Python reference `tests/ladx/oracle/tables.py`'s `parse` and `select`), the write-log format and projections ("`rom_writes.txt`, version 1", "Projection and claims"; reference `tests/ladx/oracle/write_log.py`'s `parse`, `render`, `project`), and `table_replay.py`, which already proved on every case and combo that walking the tables for a case's key and merging the entries segment by segment gives each table group's golden projection.

## Solution

C reads the tables (as text the caller hands it, no file access in the library), computes the key of every unit from the validated settings, the two assembler constants the tables name and the spot count, walks `order.txt` in its recorded order, and hands each selected entry's records to two consumers: a write logger that builds the canonical `rom_writes.txt` text (segments, net effect per segment, canonical tokens) and a bank applier that writes an entry's bytes onto an output ROM buffer from a source ROM buffer (used by step 7; unit-tested now). `C` rows of the walk are yielded to the caller, who writes nothing at them in this step.

`ladx_gen` gains `--tables <dir>`: after an ok fill it walks the tables and writes `<out_dir>/rom_writes.txt` holding only the five table groups' segments. `run_e2e.py` gains write-log claims: `claims.txt` lists the claimed groups next to the stage, and for each claimed group the driver compares `write_log.py`'s projection of C's log with the golden's. The driver proves itself on planted write logs before any real claim. At the end, `make -C tests/ladx e2e` and `e2e-combos` print 88 and 40 `PASS` lines claiming `fill` plus the five table groups, and no `DIVERGENT`.

## User Stories

1. As the plan pilot, I want C to apply `data/ladx/` for any tier-1 settings set, so that step 6's sub-steps and step 7's pipeline start from LADXR's settings-only writes.
2. As the plan pilot, I want the table groups claimed on every case and combo through their write-log projections, so that table application is proven against LADXR without the ROM.
3. As the plan pilot, I want the e2e driver to prove its write-log claims on planted logs first, so that a green e2e run cannot come from a blind comparison.
4. As a later sub-step's implementer (6.3 to 6.19), I want to add a group to `claims.txt` and have it claimed on every case, so that each port's evidence is one line.
5. As a later sub-step's implementer, I want the walk to stop at each `C` row with its group and call, so that I can write my group's records at exactly LADXR's position.
6. As step 6.2's implementer, I want a write-log segment builder and canonical renderer already in the library, so that I add only the `C`-group encoder (carried origins, inherit-on-equal, KS-match) and C's own projection.
7. As step 7's implementer, I want an applier that writes an entry's bank records onto an output ROM from the source ROM, with no allocation, so that the device's pipeline uses the same walk the claims prove.
8. As the owner, I want the library to stay pure C99 with no allocation and no file access, so that the same code runs on the host, the Miyoo and qemu.
9. As the owner, I want a missing entry, an unknown component or a malformed table to be an error with its line, never a silent skip, so that a table defect is found at once and fixed at the oracle.
10. As the owner, I want the tables read as text the caller provides, so that Phase 4 can load them from the SD card through `app/sd` without the library touching files.
11. As a debugger, I want a `DIVERGENT` case to name the group and the first differing line of its projection, golden and C, so that I see which unit went wrong.
12. As a debugger, I want very long record lines shortened in the detail, so that one failure does not flood the terminal.
13. As a debugger, I want `ladx_gen`'s `rom_writes.txt` to be a valid version-1 write log that `write_log.py parse` accepts, so that I can project, diff and decode it with the oracle's tools.
14. As a debugger with the ROM, I want to decode C's table-only log onto the ROM with `write_log.py decode`, so that I can compare it with `table_replay.py`'s partial output when a claim fails.
15. As the plan pilot, I want the `fill` claim to stay in force, so that table application never hides a regression of the placement or the RNG trace.
16. As the plan pilot, I want a case whose golden write log is missing, while groups are claimed, to stop the driver with exit 2, so that a broken golden tree is a setup finding, not a pass.
17. As the plan pilot, I want C writing no `rom_writes.txt`, or a malformed one, to be `DIVERGENT` with the reason, so that a crash or an encoding bug is never a pass.
18. As the plan pilot, I want a claimed group whose C projection is empty while the golden's is not (C forgot the group) to be `DIVERGENT`, so that "nothing written" is never equal to "everything written".
19. As the plan pilot, I want differences in unclaimed groups (the seventeen `C` groups, not yet ported) to be ignored, so that the step's claim is exactly the table groups.
20. As the plan pilot, I want a literal written where the golden has a source reference to the same bytes to be `DIVERGENT`, so that the claims prove the encoding, not only the bytes.
21. As the plan pilot, I want a record tagged with the wrong group to be `DIVERGENT`, so that the inventory's grouping is part of the proof.
22. As the plan pilot, I want the segment boundaries of C's log to be free (C merges consecutive entries of one group across the `C` rows it skips), so that only the projection, not the segmentation, is claimed, as `golden_check.py`'s projection check allows.
23. As the plan pilot, I want `claims.txt` to refuse an unknown group, a repeated group or `spoiler`, so that a typo cannot claim nothing.
24. As a reader of the log, I want `ladx_gen` to emit the version line and the `tables` line even when no table group writes, so that every C log parses.
25. As the owner, I want the spot count for `frame-counter` computed from C's own logic exactly as LADXR's `addFrameCounter` argument (spots whose class is exactly `KeyLocation` left out), so that the key is never guessed.
26. As the owner, I want the assembler constants `asm:DUNGEON_CHAIN` and `asm:ITEM_DIALOGS` (and the other 0/1 constants the inventory's `asm-consts` rows set) computed from the validated settings as `generateRom` does, so that a key widened later to another constant still works.
27. As the owner, I want a setting outside tier 1 that a unit names (`overworld`, `shopsanity`) taken at its validated value, so that the key matches the tables, which hold only its default.
28. As the owner, I want the empty-string value rendered `""` in the key, as the tables write it, so that `music`'s default selects its entries.
29. As the owner, I want a fused unit (several `order.txt` rows) applied once, at its first row, so that the walk is exactly `tables.select`'s.
30. As the owner, I want an entry that reads `nothing` to write nothing and open no segment, so that a group with nothing to write leaves no empty segment.
31. As the owner, I want the library's fixed capacities stated and checked, with an overflow reported as an error, so that a regenerated table larger than today's fails loudly.
32. As the owner, I want the real `data/ladx/` opened by a unit test with its unit, entry and group counts equal to `ladx-tables`' summary, so that the capacities are proven on the shipped data.
33. As the owner, I want every case's and combo's validated settings to select an entry in every unit of the real tables in that unit test, so that a reachable key the tables lack is found without the e2e run.
34. As the owner, I want the new unit tests in `make test` and `make unit-arm`, so that the ARM build reads and applies tables the same way.
35. As the owner, I want `make lint` to build the new sources with `-pedantic-errors`, so that they stay ISO C99.
36. As the owner, I want the contract of the library's table and log behaviour, `ladx_gen --tables`, `rom_writes.txt` from C and the driver's group claims written once in `tests/ladx/README.md`, with one line in `CLAUDE.md`, so that Phase 3's one-home rule holds.
37. As the owner, I want the byte applier to copy from the source ROM, never from the output being written, so that a copy token means what `write_log.decode` means.
38. As the owner, I want table records (`T`) kept in the log but ignored by the bank applier, so that the ROM write of tables stays `save`'s job (6.17), as in `decode`.
39. As the measure step's owner, I want the cost of opening and walking the tables recorded in the step note, so that step 10 has its first figure for this part.
40. As the owner, I want `make check` green at the end with the new LADX lines, so that the net covers the step.

## Implementation Decisions

### Library: tables (`ladx/`, pure C99, no allocation, no stdio)

- New module `ladx_tables` (public header in `ladx/include/`). The caller hands it the text of `order.txt` and of each group file as NUL-free byte spans it owns and keeps alive; the library indexes them into a caller-owned struct with fixed capacities and never copies the payloads.
- Opening checks what C relies on, not everything `tables.py check` checks (that stays the oracle's): the version lines (`ladx-tables-order 1`, `ladx-tables 1`), `section`, `table` and `C` rows, each group file's `group` and `tables` lines, `unit` lines with their components, `E` lines with one value per component, record lines (`B`, `T`) with write-log v1 tokens, `nothing`. Every group file's `tables` line must be identical (it becomes the log's `tables` line). A `table` row whose group has no file, a unit missing from its group file, a duplicate unit or a component that is not a setting key, a known `asm:` constant or `count` is an open error. Errors carry a status code, the file and the 1-based line; nothing is half-opened.
- Capacities (constants in the header, each checked on open): groups, units, `order.txt` rows, components per unit; chosen with headroom over today's 5 groups, 91 units, 147 rows, at most 6 components, and stated in the README. An overflow is its own status.
- Key: built from validated settings (`ladx_settings_validate` then `ladx_settings_supported` true), plus the spot count. A setting component renders its value as `ladx_setting_value_name` gives it, the empty string as `""`. `asm:<NAME>` is `0` or `1`, computed as the inventory's `assembler.const` rows compute it from the settings: every 0/1 constant those rows set is implemented (`HARD_MODE`, `DUNGEON_CHAIN`, `ITEM_DIALOGS`, `KEYSANITY_KEYS`, `KEYSANITY_NIGHTMARE_KEYS`, `KEYSANITY_BEAKS`, `KEYSANITY_MAPS`, and any other 0/1 row of that table); a constant that is not 0/1 (`INV_SIZE`) is not a component. `count` is decimal.
- Spot count: a library function on a built logic returning the number of `iteminfo_list` spots whose kind is not exactly `LADX_SPOT_KEY_LOCATION` (`generator.py:302`). `romdebugmode` is always off, so `frame-counter` always runs.
- Walk: an iterator over `order.txt` for one key, mirroring `tables.select`: at a `table` row that starts its unit, a step carrying the group, the unit id and the selected entry; a later row of the same unit yields nothing; a `C` row yields a step carrying its group and call text (the key `table_replay.py` uses), with no entry. A unit with no entry for the key is an error naming the unit and the key, never a skip (`nothing` is an explicit entry).
- Records: an entry's records are iterated as `B <offset>` or `T <table index> <key>` (an index key or an alt-key name) with their payload decoded into atoms in a caller buffer: a source reference (absolute source offset, keep tokens resolved to the record's own offsets) or a literal byte, the representation `write_log.py` uses (a reference q as q, a literal b as -1-b); an integer-pointer `T` entry is its own payload kind. A payload longer than the caller's buffer is an error.
- Bank applier: given a `B` record's atoms, the source ROM and the output ROM (sizes passed), writes each byte (the literal, or the source byte at the reference); a reference or offset beyond either size is an error and writes nothing. `T` records are not applied (they reach the ROM through `save`, step 6.17).

### Library: write log (`ladx/`, pure C99, no allocation)

- New module `ladx_wlog`: the write-log builder that every later sub-step extends. It holds the current segment's net effect in caller-owned fixed-capacity storage: bank writes as (offset, atom) with last-write-wins, table entries as (table, key) with whole-entry replacement, pointers included. Capacities stated in the header, with headroom over the measured maxima (a table group's net projection: 9,953 bank bytes for `fixed`, 1,035 table entries and 34,094 table bytes for `options`); overflow is an error status, after which the log refuses further input.
- Output goes to a caller-supplied sink (a function and its context), line by line as text, so no line buffer bounds a record's length.
- `begin` writes `ladx-rom-writes 1` and the `tables` line. A record of group g appended while the open segment is g merges into it; a different group closes the open segment and opens g's. Closing renders `G <group>`, then the `B` records over maximal runs of contiguous offsets in offset order, then the `T` records in table order, index keys before alt keys, index keys ascending, alt keys in byte order. A segment with no record is never emitted.
- Tokens are canonical exactly as `write_log._tokens`: `=n` when atoms continue the record's own offsets, `@q+n` for continuing references elsewhere, joined literal hex, lowercase hex without leading zeros, pointer `p<v>` alone. So C's text for a segment equals `write_log.render` of the same net maps, byte for byte.
- No `I` lines (C never writes them, README "`I` lines").
- What step 6.2 adds, and is out of this step: the `C`-group encoder (carried origins, inherit-on-equal, KS-match against KS), any whole-ROM state those rules need, the six fixture readers, KS, and C's own projection. In this step every record C logs comes verbatim from a table entry, already encoded by Phase 2's encoder, and the projection is computed by the driver with `write_log.py`.

### Host: `ladx_gen`

- New option `--tables <dir>` (the directory holding `order.txt` and the group files; the host reads them into static buffers sized for the files with headroom, a larger file an error). Without it, `ladx_gen` behaves exactly as today (no `rom_writes.txt`), so `--time` and measure are unchanged.
- After `ladx_randomize` returns ok with a placement (the run LADXR would hand to `generateRom`), it builds the key (validated settings, `asm:` constants, spot count from the placed logic), walks the tables, logs each entry's records into `ladx_wlog` under the unit's group, skips the `C` rows, ends the log and writes `<out_dir>/rom_writes.txt` through a file sink. On an error outcome of the fill, no `rom_writes.txt` (LADXR writes no ROM).
- A table open, key or walk error, or a log overflow, is a broken run: exit 2 with `ladx_gen: tables: <status> <file>:<line>` or `ladx_gen: tables: unit <id>: no entry for <values>` on stderr. Exit codes otherwise as today.
- `tests/ladx/Makefile`'s `e2e` and `e2e-combos` pass the repository's `data/ladx` to the driver, which passes it to the generator.

### E2E driver (`tests/ladx/run_e2e.py`, `python3 -I -B`)

- `claims.txt` keeps its one `stage` line and gains zero or more `group <name>` lines. A group must be in the inventory's Groups table (`write_log.sides_of` on `notes/ladx-inventory.md`, new option `--inventory`) with side `table` or `C`, and not `spoiler` (its claim is `spoiler.json`, step 6.19); unknown, repeated or `spoiler` exits 2. This step's `claims.txt`: `stage fill`, then `group fixed`, `group frame-counter`, `group goal-fixed`, `group music-fixed`, `group options`.
- The driver imports `tests/ladx/oracle/write_log.py` (standard library only) and uses its `parse` and `projections`; the projection stays defined once, in Python.
- New option `--tables <dir>` (default the repository's `data/ladx`); the generator is invoked as `<gen> --tables <dir> <case_dir> <out_dir>`.
- With at least one group claimed: every golden's `rom_writes.txt` is read and parsed before any run (missing or malformed: exit 2, as for `run.txt`); per case, C's `run.txt` claim first, then for each claimed group in `claims.txt` order its projection of C's log against the golden's. C writing no `rom_writes.txt`, or one `write_log.parse` refuses, is `DIVERGENT` (`c wrote no rom_writes.txt`, `c rom_writes.txt malformed: <reason>`). With no group claimed, `rom_writes.txt` is neither read nor required (today's behaviour).
- Verdict lines: the run prints `e2e: claims stage <stage>, groups <g> ...` first (`groups none` when none); a passing case `PASS <case> (<stage> + <n> groups)` (`PASS <case> (<stage>)` when none); a divergent case one `DIVERGENT <case>` followed, for the `run.txt` claim and for each divergent group, by `<section or group>: line <n>:`, `golden: <line>`, `c: <line>` (`n` counted in the projection's text, header lines included; `<end>` past the end). A detail line over 200 characters is cut at 200 and ends ` ... (<k> chars)`. Summary and exit codes unchanged.
- Self-test: the 18 existing planted runs keep their verdicts (their claims name no group). New planted runs, built from `cases/baseline`'s golden `run.txt` and `rom_writes.txt` (each fake generator copies both planted files), claim groups and check: identical logs `PASS`; C's log holding only the claimed table groups' records, segmented differently from the golden (merged across the absent `C` segments) `PASS`; a changed byte inside a claimed group's record `DIVERGENT`; a change only in an unclaimed group's segment before the claimed group's first segment `PASS`, and one after its last segment `PASS`; a literal-for-reference swap with the same bytes `DIVERGENT` (a synthetic pair of logs, golden `@q+4` against C's four literal bytes, stated to be the source bytes at q: the driver has no ROM, which is the point); a claimed group's record moved into a segment tagged with another group `DIVERGENT`; a claimed group absent from C's log `DIVERGENT`; no `rom_writes.txt` from C `DIVERGENT`; a malformed C log `DIVERGENT`; golden `I` lines against a C log without them `PASS`; `group nosuch`, a repeated group and `group spoiler` each exit 2; a golden missing `rom_writes.txt` while groups are claimed exits 2. The count printed in `e2e: self-test, <n> planted runs as expected` grows accordingly.

### Documentation

- `tests/ladx/README.md`: a new section "Table application" (the library's walk, key, components, errors and capacities; the applier; the log builder's segment and canonical-token rules, by reference to "`rom_writes.txt`, version 1"), "Generator `ladx_gen`" gains `--tables` and `rom_writes.txt`, "E2E driver and claims" gains the group lines, the detail format and the new planted runs, "Unit tests and lint" gains the new tests. `CLAUDE.md`: the `ladx/` layout line names the new sources; one line pointing at the README for the group claims. No docstring restates the contract.

### Decisions taken without the owner (each with its reason)

- **The driver, not C, projects.** Step 3.4 chose a Python driver so steps 5–6 reuse `write_log.py`; C's projection belongs to 6.2, whose claim is C's projection equal to `write_log.py project`'s. Doing it here would duplicate 6.2.
- **C merges entries across skipped `C` rows.** Segmentation is not claimed (projections are), and `table_replay.py` merges the same way; a later sub-step that logs at a `C` row splits the segment naturally.
- **The library takes text, not files.** The library has no platform library; Phase 4 reads the files through `app/sd`, the host through `ladx_gen`.
- **Every 0/1 assembler constant of the inventory is implemented, not only the two the tables name today.** Step 2.6 widens keys by regeneration; C should not need a code change for a constant the inventory already defines. Each is unit-tested.
- **The bank applier is built and unit-tested now, not in step 7.** The step's text is "apply `data/ladx/`"; the log proves the selection and the encoding, the applier the bytes, and step 7 only wires it.
- **The tables are opened whole on every `ladx_gen` run.** 534 KB of text indexed once is cheap on the host; its time is measured and recorded in the step note for step 10.

## Implementation Chunks

1. **Write-log builder.** `ladx_wlog`: header, segments with net effect, canonical `B` and `T` rendering, merging of consecutive same-group records, sink, capacities. Claim: `test_wlog` under `make test` and `make unit-arm`: synthetic segments render byte-identical to expected texts (each expected text produced once by `write_log.render` on the same net maps): touching records merging into one `B`, a copy landing on its own offset as `=n`, continuing references merged, adjacent literals joined, last write wins inside a segment, a write back to its own source byte kept as `=1`, `T` order (table order, index keys ascending, alt keys after), a pointer entry, an overwritten `T` entry, a group change closing the segment, a same-group record merging, no empty segment, an empty log's two header lines; an overflow returning its status and refusing later input. `make lint` builds it.
2. **Tables reader, key, walk and applier.** `ladx_tables` and the spot count. Claim: `test_tables` under `make test` and `make unit-arm` on synthetic tables (the shapes of `table_replay.py`'s planted ones and more): a walk interleaving `table` and `C` rows in order, a fused unit applied once at its first row, `nothing`, `""` and boolean values, an `asm:` component (each 0/1 constant on a settings pair that flips it), `count`, an out-of-tier-1 setting at its validated default; errors with their file and line (bad version line, unknown component, unit missing from its group file, duplicate unit, wrong value count on an `E` line, bad token, differing `tables` lines, a capacity overflow, no entry for a key); records decoded into atoms (`=n`, `@q+n`, literals, pointer, alt key); the applier writing literal and copied bytes onto a synthetic output from a synthetic source (copy from source, not output), refusing out-of-range offsets; the spot count on a hand-built logic with `KeyLocation` spots. On the real `data/ladx/`: it opens with 5 groups, 91 units and 676 entries, and the validated settings of every case and combo select an entry in every unit without a `count` component.
3. **Driver write-log claims and self-proof.** `run_e2e.py`'s `group` lines, `--inventory`, `--tables`, projections through `write_log.py`, detail lines, and the new planted runs; `claims.txt` still `stage fill` alone; the Makefile passes `--tables`; `ladx_gen` accepts `--tables <dir>` and does not use it yet. Claim: `make -C tests/ladx selftest` prints `e2e: self-test, <n> planted runs as expected` with every listed planted verdict; `make -C tests/ladx e2e` and `e2e-combos` still print 88 and 40 `PASS ... (fill)`; three hand-made mutations of the driver (comparing the whole log's text instead of the group's projection, merging every group into one projection, passing a case when C's log is missing) each fail the self-test (recorded in the step note).
4. **`ladx_gen` writes the table groups; the claim reaches every case.** The host walk into `ladx_wlog` and the file sink, the error exits, `claims.txt` with the five `group` lines, README and `CLAUDE.md`. Claim: `make -C tests/ladx e2e` and `e2e-combos` print 88 and 40 `PASS <case> (fill + 5 groups)` and no `DIVERGENT`; `write_log.py parse` accepts every C log; `make check` green with the LADX lines; the time of opening and walking the tables on the host (slowest case) in the step note.

## Testing Decisions

- A good test drives the public headers only (`ladx_tables.h`, `ladx_wlog.h`, the logic builder for the spot count) and checks observable output: rendered text, selected entries, decoded atoms, written bytes, status codes. No test reaches into the index or the segment storage.
- Expected log texts in `test_wlog` come from the reference encoder's renderer, not from C, so the C renderer is checked against `write_log.py`, not against itself.
- `test_tables` reads the real `data/ladx/` and the case `config.txt` files by relative path from `tests/ladx` (as `test_settings` reads the fixtures), always, with no flag: the files are small and parsing is fast, so it stays in `make test`.
- Both tests are added to `UNIT_TESTS`; neither needs the host layer, so neither is in `HOST_TESTS`; both run under `make unit-arm`.
- The e2e claim (every case and combo, five groups) is the integration proof of key, walk, merge and rendering together; `run_e2e.py --self-test` proves the claim can fail in each listed way.
- Prior art: `test_settings` (fixture files read by relative path, every case `config.txt`), `test_run_text` (C text rendered byte-identical to the oracle's), `run_e2e.py --self-test` (planted trees and fake generators), `table_replay.py`'s self-test (planted tables and logs).
- Mutation check by hand, recorded in the step note: in `ladx_tables`, first-row-only application, the `""` rendering, one `asm:` constant's polarity; in `ladx_wlog`, keep-versus-copy, record merging, `T` ordering. Each must fail a unit test or the e2e claim.

## Out of Scope

- Every `C` group's writes and claim (steps 6.3 to 6.19), the `C`-group encoder, KS and the extraction fixture readers (6.2), and the proof of `save`'s packing (6.1).
- Writing `T` entries into the ROM (`save`, 6.17) and the header checksums (6.18).
- `ladx_patch`, the source ROM check, writing a ROM file and `rom-check` against the C generator (step 7).
- C's own projection of a log (6.2).
- Loading the tables on the device from the SD card and shipping `data/ladx/` in the package (Phase 4).
- Extending `make -C tests/ladx measure` beyond the fill (step 10).
- Any change to `data/ladx/`, the goldens or the oracle scripts' behaviour: a defect proven to be in a table is fixed at the oracle and regenerated (§2), in its own commit.

## Further Notes

- `table_replay.py` already proved, on all 128 folders, that the walk and the per-segment merge give every table group's golden projection; a C divergence is therefore a C bug unless proven otherwise, and the oracle fix path (§2) applies only to a proven table defect.
- Today's figures the capacities are checked against: 5 groups, 91 units, 676 entries, 147 `order.txt` rows, at most 6 components per unit; per-case table-group projections at most 15,150 bank bytes and 1,174 table entries in all (`sample-04`).
- The unit tests' assumption that every case's validated settings select an entry everywhere is the C mirror of `table_replay.py`'s `key` check; a failure there means C's key differs from `gen_tables.py`'s model (value rendering or a constant), not a missing table entry.
