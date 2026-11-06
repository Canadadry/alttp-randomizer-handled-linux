---
title: "LADX inventory settled on real data (plan 3, Phase 2 step 3)"
description: "The static draft of notes/ladx-inventory.md guesses each generateRom call's side and group from the sources alone; Phase 2 needs them settled from LADXR's real ROM reads and writes, with every seed-dependent read covered, before cases, goldens and tables are generated."
status: needs-triage
---

## Problem Statement

The plan splits LADXR's ROM generation between precomputed patch tables (settings-only writes) and
a C port (seed- or placement-dependent writes). Phase 1 step 9 drafted `notes/ladx-inventory.md`
from the sources only: 214 rows in 6 sections, each with a guessed `Side (draft)` and
`Group (draft)` (two rows still `?`: `changeEntrances`, `addFrameCounter`). Its ROM-read columns
are only greps. Nobody has yet seen what LADXR really reads from the ROM and its parsed tables
while it generates, so:

- the guessed sides may be wrong. A row guessed `table` may write seed-dependent bytes, and then
  step 6's tables would not compose. A row guessed `C` may be settings-only;
- a seed-dependent group may decide something based on Nintendo data that `make check` cannot
  read. Phase 3 could then never claim it ROM-free, and the problem would only show up at the
  goldens or in Phase 3;
- the spoiler log, which reads the finished ROM back, has no place among the groups;
- `ROMWithTables.save`'s re-serialization has not been analysed (what C must reproduce, and what
  is copied);
- each seed-dependent group needs ROM-free evidence named for its Phase 3 sub-step, before steps
  4 to 10 depend on the inventory.

A prototype trace of the baseline (the step 1.9 tooling installed under `oracle_run.py`'s driver
with the real ROM, about 5 s) confirmed the risk. About 125,000 read records came in, about
119,000 of them from parsing. The `sprites`, `entrances`, `save`, `bosses`, `header` and
`spoiler` groups read source rooms, entities and banks in bulk. One read was untagged: the header
checksum's `for c in self.banks[0][...]` line is a loop header, which the inventory has no row
for.

## Solution

The step turns the draft into the final inventory, on evidence from real data:

1. The read trace also records **writes**, per inventory row, and the **provenance** of every read:
   untouched source bytes, bytes written earlier in the run (and by which group), or both.
   `oracle_run.py --rom` gains an opt-in flag that writes this trace into the case directory.
2. A driver runs every case of `determinism_cases.txt` with the real ROM, plus each case's
   settings under two more fixed seeds, so that seed-dependence can be observed rather than
   guessed.
3. A new checker, `read_check.py`, fails when:
   - a read is untagged;
   - a read falls in a seed-dependent group and no coverage entry of the inventory covers it;
   - a `none` or `unused` row reads or writes;
   - a `table` row writes different bytes under different seeds;
   - a `table` row reads bytes a seed-dependent group wrote;
   - the inventory's dependency and trace-summary columns disagree with the traces.
   Like the other oracle tools, it proves itself first on planted inputs.
4. The inventory's format grows:
   - final `Side` and `Group` columns, with no `?` left;
   - a `Reads (trace)` column;
   - a **Groups** table (side, settings key, depends-on, evidence);
   - a **Read coverage** table (how each seed-dependent read is covered);
   - an **Extractions** table (fixtures to produce in step 7);
   - a **Blind spots** table (the 52 buffer-protocol sites, each judged);
   - a written analysis of `ROMWithTables.save`.
   `inventory_check.py` validates the new shape.
5. Every seed-dependent group either gets an extraction (a derived fixture under the new
   strict-scope folder `tests/ladx/extract/`) or a scope cut recorded in the new file
   `notes/plan3-scope-cuts.md`, or its reads are proven to be copies, earlier writes or
   computed header bytes.

The owner then gets an inventory where each row's side is backed by real data. Steps 4 to 10 build
on it.

## User Stories

1. As the plan pilot, I want every `generateRom` call, every call after the fill in `randomizer.py` and every `ROMWithTables` table in the inventory with a final side, so that step 6 knows exactly which calls become tables and Phase 3 knows which it ports.
2. As the plan pilot, I want no `?` side left, so that no decision is pushed onto steps 4 to 10.
3. As the plan pilot, I want `changeEntrances` (identity mapping in tier 1) settled from what it really writes across seeds, so that its side rests on data, not a guess.
4. As the plan pilot, I want `addFrameCounter` settled the same way, so that a count of non-key spots that depends only on the settings ends up in a table, and one that depends on the seed ends up in C.
5. As a Phase 3 implementer, I want each seed-dependent group to name its write-log group and the fixtures its claim needs, so that my sub-step has ROM-free evidence to be claimed against.
6. As a Phase 3 implementer, I want each group's dependencies (the groups whose writes it reads) taken from the trace, so that step 10 can order the sub-steps so each claim is reachable.
7. As a Phase 3 implementer, I want the spoiler log as its own seed-dependent group, which depends on the groups whose writes it reads back, so that it is ported after them.
8. As a Phase 3 implementer, I want `ROMWithTables.save`'s re-serialization analysed: which tables C must re-pack, which bytes are copies, and how the leftover-storage `0x0DFA` patch is derived. Then I know what the save group's sub-step has to reproduce.
9. As a Phase 3 implementer, I want every read that only moves source bytes marked `copy`, so that I know the golden's source references are enough and no ROM value is needed.
10. As a Phase 3 implementer, I want every read whose value decides an output to be covered by an extraction or a scope cut, so that I never hit a decision `make check` cannot reproduce.
11. As a Phase 3 implementer, I want the header's checksum bytes (`0x14D`, `0x14E`, `0x14F`) marked `computed`, so that the ROM-free claim checks their offsets and tag, and `rom-check` checks their values.
12. As the step 5 implementer, I want the final group names, so that every golden write carries a tag from a known set and that set equals the inventory's groups.
13. As the step 6 implementer, I want each `table` group's settings key in the inventory, so that `gen_tables.sh` knows what to key each table on (the random sample then widens it if needed).
14. As the step 7 implementer, I want the Extractions table to name each fixture, the group it serves, what it holds and how it is derived, so that I only have to produce and check it.
15. As the owner, I want every scope cut in `notes/plan3-scope-cuts.md` with its setting value, the group that forced it and the read that cannot be extracted, so that I can review each one at the PR.
16. As the owner, I want nothing derived from the ROM that holds Nintendo bytes in the inventory, the scope-cuts note or any fixture, so that the repository stays legal.
17. As the owner, I want extraction fixtures under the strict scope of the Nintendo-bytes check, so that ROM-derived data meets the same 4-byte rule as the tables and goldens.
18. As the owner, I want the ROM link never written anywhere, so that the run respects §2.
19. As the read-trace tool, I want a read on a loop or condition header attributed to the first row of the body it governs, so that a header checksum's `for` loop is tagged with its group and not reported as untagged.
20. As the read-trace tool, I want each read to carry its provenance (`source`, `written:<groups>` or `mixed:<groups>`), so that a read of bytes an earlier group wrote can be told apart from a read of Nintendo data.
21. As the read-trace tool, I want writes recorded per row (bank bytes, `ROM.patch` included, and table entry assignments), summed up as a digest of the row's net writes, so that a row's writes can be compared across seeds.
22. As a maintainer, I want the trace off by default in `oracle_run.py`, so that the double run and later golden runs keep their speed and their output.
23. As a maintainer, I want `read_check.py` to prove itself on planted traces and inventories, so that a check that never fails is caught.
24. As a maintainer, I want an untagged read to fail, so that a site the inventory misses can never go unnoticed.
25. As a maintainer, I want a read or write in a `none` row (other than the parse rows) or an `unused` row to fail, so that a mislabelled row is caught.
26. As a maintainer, I want a `table` row whose write digest differs between seeds under the same settings to fail, so that a seed-dependent write cannot slip into a table.
27. As a maintainer, I want a `table` row that reads bytes written by a seed-dependent group to fail, so that the tables cannot depend on the seed through a read.
28. As a maintainer, I want a `C` row whose writes are identical under every seed listed as `STABLE` information, so that I can consider moving it to a table, with the reason for my choice recorded.
29. As a maintainer, I want a coverage entry of kind `written` accepted only when the trace's provenance proves that every byte read was written earlier, so that `written` cannot hide a read of Nintendo data.
30. As a maintainer, I want a coverage entry of kind `cut:<id>` accepted only when the id exists in `notes/plan3-scope-cuts.md`, so that no cut is silent.
31. As a maintainer, I want a coverage entry of kind `extract:<name>` accepted only when the Extractions table lists the name, so that step 7's list is complete.
32. As a maintainer, I want `computed` accepted only on the header group's checksum bytes, so that it does not become a catch-all.
33. As a maintainer, I want rows that no case reaches listed as `UNREACHED` (information), so that steps 4 and 5 know which rows the full case list still has to exercise.
34. As a maintainer, I want the `Reads (trace)` column regenerated by `read_check.py --print` and compared on every run, so that the column stays true to the traces.
35. As a maintainer, I want the 52 buffer-protocol blind spots of `read_trace.py --blind-spots` each judged in the inventory (not a ROM read, or a ROM read covered by a named row), checked against the tool's current output, so that the reads the trace cannot see are accounted for.
36. As a maintainer, I want `inventory_check.py` to keep passing on the new format and to fail on a bad side, a group used by a row but missing from the Groups table, a seed-dependent group without evidence, or a side mismatch between a row and its group, so that the format stays consistent.
37. As a maintainer, I want `make test` no slower in any noticeable way: the new unit tests run without LADXR and without network inside `test-ladx-oracle`, and the real-data driver stays outside the net.
38. As a maintainer, I want the traces, case directories and output ROMs written only under `tmp/`, so that nothing ROM-derived is left around for the push gate to refuse.
39. As a future agent, I want `CLAUDE.md`'s oracle tooling description to name the new driver, checker and flag, so that I can re-run the step's evidence.
40. As the plan pilot, I want the error and crash cases (`pain`'s failed fill, the `goal=maze` and `UnicodeEncodeError` crashes) traced up to where they stop, so that their reads are checked too.

## Implementation Decisions

- **Guideline decisions** (this step's own questions, answered by `guideline` with the plan as input):
  1. A seed-dependent read that only copies source bytes into the output (moved or re-serialized) is covered by the golden's source-reference encoding and needs no extraction. Its coverage kind is `copy`. Step 5's "encoding reproducible without the ROM" check and Phase 3's claims prove it.
  2. The header checksum bytes `0x14D`, `0x14E` and `0x14F` are recorded as `computed` (the clearance that step 2.1's note reserved). The ROM-free claim checks their offsets and tag. Their values are proven by `rom-check` with the real ROM (Phase 3 steps 7 and 9) and by a unit test on a synthetic ROM.
  3. An extraction fixture may hold data derived from the source state (for example the entity types per room, in decimal) when it passes the **strict** scope of the Nintendo-bytes check. Extraction fixtures live in a new folder, `tests/ladx/extract/`, which is added to the check's strict path prefixes. `data/ladx/` ships on the device and stays reserved for the patch tables.
- **Read-trace module (extended, deep).** It keeps its install/records/write/uninstall interface and adds:
  - write recording: every byte write to a traced bank (slice and index assignment, so `ROM.patch` writes are included), and every assignment of a table entry through `__setitem__`;
  - per-byte provenance: a written mask per bank and a written set per table index, each tagged with the group that wrote it;
  - each read record gains a provenance field: `source`, `written:<g1,g2>` or `mixed:<g1,g2>`. A table read counts as `written` when that index was assigned since parsing;
  - per site, a net write digest (SHA-1 over the sorted final (target, offset or index, value) of the bytes and entries the site changed) and a write count.
  `ROM.patch` reads stay excluded. Parse-time reads (`ROMWithTables.__init__`) stay recorded under the `parse` group. Its self-test gains planted writes (bank, `patch`, table), one planted read of each provenance, and a planted header-line read.
- **Site resolution of header lines.** A read on a `for`/`if`/`elif`/`else`/`while`/`try`/`except` line is keyed to the first inventory row of the body it governs, using `inventory_check`'s row rules. The resolution lives in `read_check`, not in the trace, so the raw trace stays faithful. A read that still has no row is an `UNTAGGED` failure.
- **`oracle_run.py --rom` gains an opt-in `--reads`** that installs the trace with `inventory_check`'s sections and the inventory's groups. It writes `reads.txt` (aggregated: group, site, caller, channel, target, provenance, `n=` and range) and `writes.txt` (per site: count and digest) into the case directory. `run.txt`, `out.gbc` and `spoiler.json` are unchanged by the flag. The trace covers the whole run, the spoiler included. On an error or crash outcome it is still written.
- **Driver `inventory_trace.sh`** (outside the net, needs the ROM in `tmp/ladx/` and the setup checkout; otherwise a `setup:` line):
  - runs every case of `determinism_cases.txt` with `--rom --reads` into `tmp/ladx-inventory/<case>/seed-<k>/`;
  - also runs each case's settings under two more fixed seeds, listed in a new hand-written `side_seeds.txt` (two seeds, used for every case);
  - then runs `read_check.py` on the result.
  The seeds are fixed, so a re-run gives the same traces.
- **New `read_check.py`** (stdlib, `python3 -I`). Inputs are the inventory and a traces root. Every run proves itself first on planted traces and inventories. It prints one line per finding:
  - `UNTAGGED` — a read with no inventory row;
  - `UNCOVERED` — a read in a `C` group that no Read-coverage entry matches;
  - `BAD-COVER` — a coverage entry whose conditions fail;
  - `NONE-ACCESS` — a read or write in a `none` row other than the parse rows;
  - `UNUSED-REACHED` — any access in an `unused` row;
  - `SEED-VARIES` — a `table` row whose write digest differs between the seeds of one settings set;
  - `TABLE-READS-SEED` — a `table` row reading bytes written by a `C` group;
  - `DEPENDS` — a group's Depends-on column differs from the trace's provenance groups;
  - `READS-COLUMN` — a `Reads (trace)` cell differs from what the traces give.
  Information lines are `STABLE <row>` (a `C` row identical under all seeds) and `UNREACHED <row>`. The last line is `read-check: <cases> cases, <reads> reads, <groups> groups covered` or the number of findings. Exit 1 on a finding, 2 on `setup:`. `--print` writes the `Reads (trace)` cells and the Depends-on column from the traces, for refreshing the file by hand.
- **Coverage kinds** (one Read-coverage row: group, row key or `*`, target, offset or index range or `*`, kind, reason):
  - `copy`: bytes only moved; the reason says where they go;
  - `written`: accepted only if the provenance of every matched read is `written:` by listed groups;
  - `computed`: header group, checksum offsets only;
  - `extract:<name>`: the name must be in the Extractions table;
  - `cut:<id>`: the id must be a heading or entry in `notes/plan3-scope-cuts.md`;
  - `table`: a value read from bytes a `table` group wrote, accepted only with `written:` provenance from `table` groups.
  Reads in `table` groups need no coverage entry: their tables are precomputed from the real ROM.
- **Inventory format.**
  - Section tables: `Side (draft)` becomes `Side` (values `table`, `C`, `none`, `unused`; `?` no longer allowed). `Group (draft)` becomes `Group`. A new column `Reads (trace)` goes between `Reads (grep)` and `Notes`: per target, its provenance and range (offsets and counts only, never byte values), or `-`.
  - New tables, each under its own heading:
    - Groups: Group | Side | Settings key | Depends on | Evidence. Evidence is the write-log group (the group's own name) plus the existing ROM-free fixtures and extraction names it needs; it is required for `C` groups.
    - Read coverage: as above.
    - Extractions: Name | Group | Holds | Derived from | Format.
    - Blind spots: File | Line | Verdict.
  - The `ROMWithTables.save` analysis is prose under its own heading.
  - The row-to-group assignment stays per row. A row is wholly one side. A `C` row that also writes fixed bytes (for example `addBank3E`) keeps them on the C side, and its Notes say so.
  - The spoiler rows that build the log become side `C`, group `spoiler`. The file-name and output rows stay `none`.
  - The header-paragraph text is updated to describe the final columns (no longer "static draft").
- **`inventory_check.py`** keeps its grep-diff and is updated to the new columns and side set. It also validates the new tables:
  - every row group appears in Groups with the same side;
  - every `C` group has evidence;
  - every extraction named in coverage or evidence is listed;
  - every Blind-spots row matches `read_trace.py --blind-spots` output one to one (when the checkout is present).
  Its planted inventories follow the new format.
- **Nintendo-bytes check.** `tests/ladx/extract/` joins the strict prefixes, and its test plants a strict hit there. This tightens the policy and loosens nothing. The inventory, the scope-cuts note and any fixture this step commits pass the check under their scope. Results go in the step note.
- **`notes/plan3-scope-cuts.md`** is created in this step. Each cut has an id heading, the setting value moved to unsupported, the group, the read that cannot be extracted, and the reason. If there is no cut, the file says "No cut so far" so the Done-when list has a home. A cut also moves the value into `tier1.txt`'s excluded comment and the plan's unsupported list only in step 4 and Phase 3 step 2 (not here). This note is where the decision is made.
- **Decision rules applied on the data, per seed-dependent group:**
  - a read that copies → `copy`;
  - a read of earlier writes → `written` or `table`;
  - a read whose value decides → an extraction, if a derived form passes the strict check;
  - otherwise, a `cut`, only when the group is reached only under a tier-1 value that can be cut.
  Any other dead end (a decision on Nintendo data in a group that always runs) goes to `guideline` and is recorded. The expected outcomes from the prototype:
  - `header` computed;
  - `save` mostly copy, plus an analysis of the packing decisions;
  - `sprites`: an extraction of the per-room entity types of the source state;
  - `entrances` and `frame-counter`: likely table or none, decided by `SEED-VARIES` and `STABLE`;
  - `spoiler` written;
  - `bosses`, `items`, `title`, `bank3e` and `marin`: judged per read.
  These are expectations, not commitments; the data decides.
- **Nothing ROM-derived is committed except the inventory, the scope-cuts note and the checked tools.** The traces stay in `tmp/`. Every session first re-runs `fetch_rom.sh` (link from `LADX_ROM_URL`, exported from the session's scratchpad, never written to a file) and `setup.sh`, and re-arms the push gate.
- `CLAUDE.md`'s LADX oracle paragraph names `--reads`, `inventory_trace.sh`, `side_seeds.txt`, `read_check.py` and `tests/ladx/extract/`'s strict scope.

## Implementation Chunks

1. **Trace writes and provenance.** Extend the read-trace module: write recording, provenance, per-site digest, and the self-test's new planted cases. Add `oracle_run.py --rom --reads` writing `reads.txt` and `writes.txt`. Unit tests in the trace's existing test file and in `test_oracle.py` (the flag off leaves the output unchanged; the flag on writes both files), all without LADXR.
2. **Inventory format and checker.** Update `inventory_check.py` to the new columns, sides and tables (Groups, Read coverage, Extractions, Blind spots), with planted-inventory tests. Migrate `notes/ladx-inventory.md` to the new shape: columns renamed, `Reads (trace)` `-` for now, Groups table filled from the draft groups. Add `tests/ladx/extract/` to the strict scope with its planted test. `inventory_check` passes.
3. **Coverage checker and driver.** `read_check.py` with its self-proof and unit tests on planted traces (every finding and information line, header-line resolution, each coverage kind's condition), joined to `test-ladx-oracle`. `side_seeds.txt`, `inventory_trace.sh` (`setup:` lines without ROM or checkout).
4. **Inventory on real data.** Run `inventory_trace.sh` on every determinism case under three seeds. Settle every side and group, fill Reads (trace), Depends on, Evidence, Read coverage, Extractions and Blind spots. Write the `ROMWithTables.save` analysis. Create `notes/plan3-scope-cuts.md`. Update `CLAUDE.md`. Done when `read_check` prints no finding, `inventory_check` passes, the Nintendo-bytes check passes on every file the step commits, and `make check` is green.

## Testing Decisions

- A good test drives a tool through its public entry (module function or CLI) on planted inputs and checks its printed lines and exit code, never internal structures. This follows the existing oracle tests: `test_read_trace.py`, `test_inventory_check.py`, `test_nintendo_bytes.py`, `test_push_gate.py` and `test_oracle.py`.
- Tested:
  - the read-trace module (write recording, provenance of each kind, digest stable across two runs, header-line read present in the raw trace);
  - `oracle_run.py`'s flag (on a stub where LADXR is not needed, as `test_oracle.py` already does);
  - `inventory_check.py` (each new BAD case on a planted inventory);
  - `read_check.py` (every finding kind triggered by one planted trace, a clean planted set passing, `--print` round trip);
  - `nintendo_bytes.py` (a planted strict hit under `tests/ladx/extract/`).
- All of these run without LADXR, without the ROM and without network, inside `make test`'s `test-ladx-oracle`. Each tool also proves itself on every real run, as the step 1.9 tools do.
- The real-data run (chunk 4) is evidence, not a unit test. Its numbers go in the step note:
  - the read and write counts per group;
  - the findings resolved;
  - the `STABLE` and `UNREACHED` lists;
  - each side that changed from the draft, with its reason.
- `make check` stays green. No check is weakened. The planted ROM in the tests is synthetic.

## Out of Scope

- The case list (step 4), the golden format and goldens (step 5), the patch tables and their random sample (step 6), and producing the extraction fixtures (step 7: this step only names and specifies them).
- Moving a cut value to unsupported in `tier1.txt`, the settings table or the UI (step 4 and Phase 3 step 2 follow the note).
- Splitting Phase 3 step 6 (step 10), even though this step provides the depends-on graph it needs.
- Any C code, and any change to LADXR's behaviour or `ladxr.patch`, unless the trace proves an order defect (then §2's rule applies: its own commit).
- The `computed` clearance's implementation in the Nintendo-bytes check (step 5, already a TODO there).

## Further Notes

- The prototype trace (baseline, seed `000102…0f`, outcome ok, about 125,000 records, about 5 s against about 0.6 s without the trace) is why the trace is opt-in, and why header-line resolution and provenance are needed.
- The rows reached in the prototype include:
  - `fixed`/`options` rows reading rooms through `RoomEditor` (covered as tables);
  - `warpHome`, a `table` row running after the seed-dependent groups and reading rooms and backgrounds. `TABLE-READS-SEED` and `SEED-VARIES` settle it.
- Error and crash outcomes stop mid-generation. Their partial traces are checked like the others, and rows after the stop are not counted as `UNREACHED` for that case.
