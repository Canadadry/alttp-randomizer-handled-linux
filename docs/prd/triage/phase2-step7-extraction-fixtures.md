---
title: "LADX extraction fixtures (plan 3, Phase 2 step 7)"
description: "Phase 3's C groups need the ROM structure the inventory's Extractions table names (rooms, entities, boss sources, backgrounds, the save layout, three source flags) as committed, ROM-free fixtures that let C rebuild the known-source map KS exactly, without holding a literal run of the ROM."
status: needs-triage
---

## Problem Statement

Plan 3, Phase 2, step 7, verbatim from `docs/plan-3.md` §3:

> 7. **Extraction fixtures.** Every extraction fixture the inventory names (the ROM-free fixtures were made in Phase 1 step 8). Verified by Phase 1 step 8's fixture generation re-run with the final `ladxr.patch` giving files byte-identical to the committed ones, with the Python version compared to the one Phase 1 recorded (any difference is regenerated in its own commit, with the reason in the step note); by the fixtures being byte-identical on two runs, and a script checking that each fixture the inventory names exists.

Phase 2 step 3 settled the inventory (`notes/ladx-inventory.md`). Several seed-dependent `C` groups (`sprites`, `bosses`, `enemizer`, `entrances`, `goal`, `title`, `warp-home`, `save`, `spoiler`, `items`, `notext`) make decisions from ROM content: room entity lists, room objects and warps, two boss tables, title and warp backgrounds, the packing of the eleven `ROMWithTables` tables, and three source bytes tested against a constant. Its Extractions table names six fixtures for them, to live under `tests/ladx/extract/` (strict scope of the Nintendo-bytes check): `room-entities`, `room-objects`, `boss-sources`, `backgrounds`, `save-layout`, `source-flags`. None exists yet.

Step 2.5 added a hard requirement on them: the goldens' C-group encoding uses a KS-match keep rule against the known-source map KS (`tests/ladx/oracle/known_source.py`, documented in `tests/ladx/README.md`), and "step 7's fixtures must let C rebuild every KS byte". Today KS is computed from the ROM only; nothing proves a ROM-free source of it exists.

From the owner's perspective: Phase 3 cannot claim any of those C groups, nor reproduce the goldens' encoding, without the ROM, until these fixtures exist, are proven complete, are reproducible, and hold no Nintendo bytes.

## Solution

A new oracle tool reads the real ROM through LADXR at the pin and writes the six fixtures into `tests/ladx/extract/`, one file per extraction the inventory names, in a purely numeric text format measured to give `0 literal runs` under the strict Nintendo-bytes scope. A driver renders them twice (setup checkout with `PYTHONHASHSEED=0`, fresh clone with `=1`) and compares both runs with each other and with the committed files, like `gen_fixtures.sh --check`.

With the ROM, a verification proves the fixtures are right and sufficient: the KS map rebuilt from the fixtures alone equals `known_source.build(rom)` offset for offset and byte for byte; the flags, boss sources and background tile maps equal what LADXR reads; the fingerprints separate every distinct suffix; and a ROM-free model of `PointerTable.store` driven only by `save-layout` reproduces LADXR's own packing of the unchanged source tables.

Without the ROM, a fast check in `make test` proves that each extraction the inventory names has its file (and no other file is there), that each file parses in the canonical format, and that the structural counts hold.

Finally, Phase 1 step 8's `gen_fixtures.sh --check` is re-run with the final `ladxr.patch` and the Python version compared with Phase 1's (3.13.18).

## User Stories

1. As the plan pilot, I want one file per extraction named in the inventory's Extractions table under `tests/ladx/extract/`, so that every `extract:<name>` evidence of a C group points at something real.
2. As the plan pilot, I want a ROM-free check in `make test` that fails with `MISSING <name>` when an inventory extraction has no file, so that the inventory and the fixtures cannot drift apart.
3. As the plan pilot, I want the same check to fail with `EXTRA <file>` for a file in `tests/ladx/extract/` that the inventory does not name, so that stray ROM-derived files are never committed unnoticed.
4. As the plan pilot, I want the check to fail with `BADFILE <name>: <reason>` when a fixture does not parse, is not in canonical form, or breaks a structural count, so that a hand edit or a truncated file is caught without the ROM.
5. As the plan pilot, I want the check to prove itself on planted trees first (a missing, an extra, a malformed, a non-canonical fixture), so that a broken checker cannot pass silently.
6. As a Phase 3 implementer, I want `room-entities` to give, for every room 0x000..0x315, its entity entry's span and each entity's x, y and type, so that C can rebuild every entity entry and decide sprite slots, enemies and minibosses without the ROM.
7. As a Phase 3 implementer, I want `room-objects` to give, for every room and alt room (Alt06, Alt0E, Alt1B, Alt2B, Alt79, Alt8C, Alt1F5), its object entry's span, animation id, floor object and every object with the fields `known_source.fields` uses, so that `rebuild` gives the entry's bytes exactly.
8. As a Phase 3 implementer, I want `boss-sources` to give the offsets in bank 0x14 0x04E0..0x051F holding 0x00 and the item id at bank 0x3E 0x3300 plus each of the nine boss rooms, so that `bosses` can be ported.
9. As a Phase 3 implementer, I want `backgrounds` to give the spans and parsed tile maps of tile backgrounds 3, 4, 0x0D and attribute backgrounds 3, 4, so that `title` and `warp-home` can re-encode them like `BackgroundEditor`.
10. As a Phase 3 implementer, I want `save-layout` to give every re-serialized table's storage regions, every entry's source span or integer pointer, the alt pointers' spans, the de-duplication among source entries, the four values of every room sprite data entry, and a 64-bit fingerprint of every suffix of every text and background entry, so that C can reproduce `ROMWithTables.save`'s packing without parsing the ROM.
11. As a Phase 3 implementer, I want `source-flags` to give the three flags (keyhole hook not installed, witch off, boomerang trade not patched), so that `items` and `spoiler` decide like LADXR.
12. As a Phase 3 implementer, I want the fingerprint to be the first 8 bytes of SHA-512, so that the C side reuses the SHA-512 module Phase 3 step 1 builds for seeding instead of adding a new hash.
13. As a Phase 3 implementer, I want the format documented once in `tests/ladx/README.md`, so that the C reader and the Python writer share one specification.
14. As a Phase 3 implementer, I want every number in a fixture to be a decimal 0..255 and every larger quantity split into bytes in a documented order, so that a C reader needs no big-number parsing and the files keep the inventory's "at most three digits" rule.
15. As the owner, I want the fixtures to hold structure only (ids, positions, spans, counts, flags, fingerprints), never graphics, text or code bytes, so that the repository stays legal.
16. As the owner, I want the strict Nintendo-bytes check to print `0 literal runs` on `tests/ladx/extract/`, with no new clearance and no scope change, so that the rule is not weakened for these files.
17. As the plan pilot, I want the generator to run the strict Nintendo-bytes check in-process on every file before writing it and to refuse writing on any literal run, so that a bad format never reaches the working tree, let alone a push.
18. As the plan pilot, I want the generator to refuse a ROM whose SHA-1 is not `d90ac17e…` and a LADXR checkout not at `4325207e` with a `setup:` line (exit 2), so that a setup problem never looks like a bug.
19. As the plan pilot, I want a double run (setup checkout with `PYTHONHASHSEED=0`, fresh clone in another directory with `=1`) compared with each other and with the committed files, printing one `DIFFERENT`, `MISSING` or `EXTRA` line per file, else `extract: 6 files byte-identical on two runs and to tests/ladx/extract`, so that the fixtures are proven deterministic.
20. As the plan pilot, I want the double run's renders to land only under `tmp/`, and an `--out` outside `tmp/` refused, so that ROM-derived scratch never lands in the tree.
21. As the plan pilot, I want a verification with the ROM that rebuilds KS from the six fixtures alone and compares it with `known_source.build(rom)`: the same set of offsets, the same byte at each, one `BAD ks: <span label>: <detail>` line per difference, so that 2.5's requirement is proven, not assumed.
22. As the plan pilot, I want that verification to also compare each flag of `source-flags` and each value of `boss-sources` with the ROM's, and each background tile map with LADXR's `BackgroundEditor`, so that the non-KS fields are proven too.
23. As the plan pilot, I want the verification to fail when two different suffixes share a fingerprint or one suffix gets two, so that fingerprint lookups in C are exact on the source set.
24. As the plan pilot, I want a ROM-free model of `PointerTable.store` (alt pointers first, first-fit by sorted regions, suffix de-duplication for variable-size tables, whole-entry de-duplication for sprite data, the overworld leftover and the `0x0DFA` patch, the indoor table's adjusted data start) driven only by `save-layout`, checked against LADXR's own `store` of the unchanged source tables, so that `save-layout` is proven sufficient for its own group before Phase 3 depends on it.
25. As the plan pilot, I want the verification to print `ok <check>` or `BAD <check>: ...` lines and a summary `extract-verify: 6 files, <k> KS spans, <b> KS bytes rebuilt, <n> bad`, exit 1 on any bad, so that the step note can quote it.
26. As the plan pilot, I want Phase 1 step 8's `gen_fixtures.sh --check` re-run with the final `ladxr.patch`, and the Python version printed by `setup.sh` compared with the 3.13.18 Phase 1 recorded, so that the ROM-free fixtures are proven still current.
27. As the plan pilot, I want any difference from that re-run regenerated in its own commit with the reason in the step note, so that the history explains each fixture change.
28. As a maintainer, I want `CLAUDE.md` to describe `tests/ladx/extract/`, the generator, the double run, the verification and the `make test` check, so that the next session finds the tools.
29. As a maintainer, I want the inventory's Extractions table brought in line with the fixtures (save-layout holds every sprite data entry's values; Format pointing to the README), so that the inventory stays the truth.
30. As the plan pilot, I want the push gate to keep checking `tests/ladx/extract/` in its strict scope unchanged, so that the fixtures' push is held to the same rule as the check.
31. As a maintainer, I want the generator to use LADXR's own `ROMWithTables`, `RoomEditor`, `BackgroundEditor` and `PointerTable` at the pin, cross-checked against `known_source.parse`'s spans, so that the fixtures describe what LADXR actually parses and the double run on two checkouts means something.
32. As a maintainer, I want the fixtures' total size recorded in the step note, so that later budgets have a reference.

## Implementation Decisions

- **Files.** `tests/ladx/extract/` holds exactly one file per row of the inventory's Extractions table, named `<name>.txt` (`room-entities.txt`, `room-objects.txt`, `boss-sources.txt`, `backgrounds.txt`, `save-layout.txt`, `source-flags.txt`), and nothing else: no README there, because any English prose under the strict scope hits LADX's ASCII dialogue at 4 bytes (measured: `ladx-extract 1 room-entities` alone gives 6 ROM 4-grams). The format lives in `tests/ladx/README.md` ("Extraction fixtures") and in the tool's docstring.

- **Format, numeric only (measured decision).** Lines hold only decimal digits, `=`, `r` and the newline. Every line starts with `=`; tokens are separated by `=`; the numbers inside one token are separated by `r`; each number is 0..255 in decimal without leading zeros; a quantity above 255 (an offset in a bank, a length, a room id, a text index) is split into bytes, high first; line 1 is the version line `=1`. Which token holds what (room, bank, offset, length, then one token per entity, object, region, entry, suffix fingerprint …), and the section structure of `save-layout` (sections opened by a numbered marker line, tables numbered in `ROMWithTables`' order, never by name) are documented in the README; each file is canonical (parse then render is the identity).
  Reason, measured on the real ROM during this PRD (counts only, no byte printed): the inventory's "decimal numbers of at most three digits" rule alone does not clear the strict scope's raw view. A space/comma rendering of room entities and objects gave 444 literal runs; across 70×70 separator pairs on room entities, objects, backgrounds and all 77,445 text and background suffix fingerprints (2.96 MB), a space between tokens left about 271 raw 4-gram hits whatever the inner separator, the best pairs left 2 (both across a newline), and `=` between tokens, `r` (or `S`) inside tokens and a leading `=` on every line gave 0 hits; `nintendo_bytes.py --scope strict` printed `0 literal runs` on those renderings, with nothing cleared. `r` is chosen over `X`/`x` because `0X…` could read as a `bytelist` token and over `S` for readability. This keeps the strict scope unchanged: no new scope, no clearance, no threshold (guarded by the in-process check below). If the final layout of a file still hits, the generator refuses it and the layout (not the scope) is changed; the change and its measurement go in the step note.

- **In-process Nintendo-bytes guard.** The generator renders all six files in memory and runs the Nintendo-bytes check's own per-file function under the strict scope against the ROM; any literal run aborts with the file, view, offset and length (never the bytes) and nothing is written.

- **Generator.** A new oracle module (working name `extract.py`, in `tests/ladx/oracle/`) with a `generate` command (ROM, LADXR checkout, output directory) and a shell driver (working name `gen_extract.sh [--out <dir>] [--check]`) shaped like `gen_fixtures.sh`: it runs `setup.sh`, requires `tmp/ladx/base.gbc` of SHA-1 `d90ac17e…` (else `setup:`, exit 2), writes into `tests/ladx/extract/` by default; `--check` renders A (setup checkout, `PYTHONHASHSEED=0`) and B (fresh clone under `tmp/ladx-oracle/`, `=1`) under `tmp/` and compares A, B and the committed files (`DIFFERENT`/`MISSING`/`EXTRA` lines, else `extract: 6 files byte-identical on two runs and to tests/ladx/extract`). `--out` outside `tmp/` is refused (exit 2). Python is run without `-I`/`-E` in the driver (they drop `PYTHONHASHSEED`), as in `gen_fixtures.sh`. Kept separate from `gen_fixtures.sh` because that script is ROM-free by contract.

- **Source of the data.** The generator parses the ROM with LADXR's `ROMWithTables` at the pin and reads rooms with `RoomEditor`, backgrounds with `BackgroundEditor`, regions, alt data and banks from `PointerTable`'s private state, and spans from `known_source.parse`; it asserts that LADXR's parse and `known_source.parse` agree (as `known_source.py --ladxr` does) and aborts on a mismatch.

- **KS is fixed, not widened.** The fixtures must let C rebuild exactly `known_source.build(rom)`: the KS-match rule already shaped the committed goldens, so the rebuilt set must equal it, never more or less. Spans `known_source` leaves out (`NOT-REBUILT`; today the 186 text bank bytes whose source byte holds more than the bank) stay out, and the fixtures need not describe them.

- **save-layout: every sprite data entry.** The inventory says "the four values of every room sprite data entry `updateSpriteData` leaves"; KS says "every room sprite data entry". `save-layout` holds the four values of every source entry of both sprite data tables (a superset; 211 distinct spans, 3,200 bytes) because KS requires it. The inventory row is updated to say so.

- **Fingerprints.** The first 8 bytes of SHA-512 of the suffix's bytes, written as 8 numbers; one per suffix of every source text entry and every source background entry (tiles and attributes), offset implied by its position in the entry's line. Chosen over SHA-256 or FNV to reuse Phase 3 step 1's SHA-512 module. Measured: 77,445 suffixes, about 2.8 MB of the about 3.1 MB total. Accepted as is (no budget exists for `tests/ladx/extract/`; size recorded in the step note); shrinking it would drop lookups the `save` group needs on any seed.

- **De-duplication among source entries.** Recorded explicitly per table (entry, and the earlier entry and offset it equals as a whole or as a suffix, in store order on the unchanged state), though derivable from fingerprints and KS content, because the inventory names it and it makes the store model's input direct.

- **ROM-free check (`make test`).** `extract.py check [--root <dir>] [--inventory <file>]`, stdlib only, no ROM, no LADXR: proves itself on planted trees first (`extract: self-test, <n> planted trees as expected`, else `self-test failed: <case>`, exit 1); then reads the Extractions table's names from the inventory and compares them with the files (`MISSING <name>`, `EXTRA <file>`); parses each file (`BADFILE <name>: <reason>`: version, character set, number range, canonical rendering, structural counts: 0x316 entity rooms, every room and the seven alt rooms, nine boss rooms, the five background entries, the eleven tables, 256 + 544 sprite entries, three flags); exit 1 on a finding, 2 on a malformed inventory, else `extract: 6 fixtures, <b> bytes, every inventory extraction present`. A root `Makefile` target `ladx-extract` runs it and `make test` depends on it, next to `ladx-cases` and `ladx-tables`.

- **Verification with the ROM.** `extract.py verify [--root <dir>] [--rom <file>] [--ladxr <dir>]`, outside the net (ROM and LADXR, else `setup:`, exit 2), checks in order: `parse` (the ROM-free check), `ks` (rebuild from fixtures vs `known_source.build`), `flags`, `bosses`, `backgrounds` (tile maps vs `BackgroundEditor`), `fingerprints` (equal ⇔ equal suffix, each equal to the hash of its ROM suffix), `store` (the model below vs LADXR); `ok <check>` or `BAD <check>: <detail>` lines (offsets and labels only, never byte values), then `extract-verify: 6 files, <k> KS spans, <b> KS bytes rebuilt, <n> bad`.

- **Store model.** A pure function in the same module: input = the parsed `save-layout` (plus entity/room/background content from the other fixtures and an entry list saying which entries are unchanged); output = every entry's pointer, the packed placement of every stored entry, the overworld leftover and the derived `0x0DFA` value. It follows the `ROMWithTables.save analysis` section of the inventory. `verify`'s `store` check runs LADXR's `rom.save` path's table stores on a freshly parsed, unchanged `ROMWithTables` and compares every pointer byte and every placement with the model.

- **Docs.** `tests/ladx/README.md` gains "Extraction fixtures" (format, per-file layout, the separator measurement, the KS relation, how to regenerate and verify); `CLAUDE.md`'s layout and checks describe `tests/ladx/extract/` (no longer "will hold"), `gen_extract.sh`, `extract.py check|verify` and `make ladx-extract`; the inventory's Extractions Format column points to the README.

- **Phase 1 fixtures re-run.** `gen_fixtures.sh --check` is run with the final `ladxr.patch`; the Python version from `setup.sh` is compared with Phase 1's 3.13.18 (this environment's `python3` is 3.13.18). Any `DIFFERENT` file is regenerated in its own commit with the reason in the step note; none is expected (no `ladxr.patch` change since Phase 2 step 2's title-screen fix, which `gen_fixtures.sh` does not reach).

- **Commits.** ROM-derived fixtures are committed only as the generator's output, in a commit of their own that states they come from `gen_extract.sh` at LADXR `4325207e` with the ROM SHA-1 `d90ac17e…`; never edited by hand (§2: a defect is fixed at the oracle and regenerated).

## Implementation Chunks

1. **Format and ROM-free check.** The extraction module's parse/render (canonical), the per-file structural rules, KS rebuild from parsed fixtures (pure: fixtures → `{offset: byte}`), the `check` command with its planted-tree self-test, and unit tests on synthetic fixtures. `ladx-extract` is written but not yet a dependency of `make test` (no fixture exists yet). README "Extraction fixtures" format section.
2. **Generator, fixtures and verification.** LADXR-backed `generate` with the cross-check against `known_source.parse` and the in-process strict Nintendo-bytes guard; `gen_extract.sh` with `--check`; `verify` with `parse`, `ks`, `flags`, `bosses`, `backgrounds`, `fingerprints`; the six fixtures generated and committed in their own commit; `ladx-extract` added to `make test`; `CLAUDE.md` and the inventory's Extractions table updated. Evidence: `gen_extract.sh --check`, `verify`, `nintendo_bytes.py --scope strict tests/ladx/extract` with `0 literal runs`, sizes.
3. **Store model.** The ROM-free `store` model and `verify`'s `store` check against LADXR; unit tests on synthetic tables (alt pointers first, first fit, suffix de-dup, sprite whole-entry de-dup, leftover and `0x0DFA`, indoor adjusted start). If it shows `save-layout` lacks something, the generator is fixed and the fixtures regenerated in their own commit, reason in the step note.
4. **Re-run and close.** `gen_fixtures.sh --check` with the final `ladxr.patch`, Python version compared; `make check` green; the push (through the armed gate) carries the fixtures.

## Testing Decisions

- A good test drives the module through its public commands and functions (render/parse, `check`, KS rebuild, the store model) on synthetic inputs and asserts on their outputs and printed lines, never on private helpers or intermediate structures.
- `test_extract.py` in `tests/ladx/oracle/` (picked up by `make test-ladx-oracle`'s discovery; stdlib only, no ROM, no LADXR, no network): format round trip and canonical refusal (leading zero, number above 255, a forbidden character, a missing leading `=`); `check` on planted trees (missing, extra, malformed, wrong count, a clean tree passing) and against a planted inventory; KS rebuild on synthetic room, entity, background, sprite, pointer and boss data, compared with `known_source.rebuild`/`build` on a synthetic ROM built from the same data; the store model on synthetic tables covering each rule listed in chunk 3.
- `check`'s own self-test runs inside `make test` on every call (prior art: `ladx_cases.py check`, `tables.py check`).
- With the ROM, the `verify` command is the proof (outside the net, like `golden_check.py`, `table_replay.py`, `known_source.py --ladxr`); its output goes into the step note. The double run (`gen_extract.sh --check`) mirrors `gen_fixtures.sh --check` and `gen_tables.sh --check`.
- The in-process Nintendo-bytes guard is tested with a planted synthetic ROM and a planted fixture that contains one of its 4-byte runs (prior art: `test_nintendo_bytes.py`'s planted ROMs).
- Prior art: `test_known_source.py` (synthetic ROM tables), `test_tables.py` and `test_ladx_cases.py` (planted trees, self-tests), `test_gen_tables.py` (driver logic without LADXR).

## Out of Scope

- Any C code: the reader of these fixtures, and whether C compiles them in or reads them at run time, are Phase 3's.
- Changing KS, the write-log encoding, the goldens or the patch tables; widening KS to the text bank bytes `known_source` leaves out.
- A new Nintendo-bytes scope or clearance for `tests/ladx/extract/` (step 8 runs the check over everything Phase 2 committed).
- New extractions or scope cuts: the inventory's Extractions table is taken as settled by step 2.3; only its `save-layout` row's wording and its Format column are aligned.
- The full-ROM `rom-check` target (step 9) and the Phase 3 split (step 10).
- `goal=bingo*` (cut `goal-bingo`).

## Further Notes

- For the owner to review: (a) the numeric `=`/`r` format, chosen by measurement rather than readability, to keep the strict scope at 4 bytes without a new clearance; (b) the fingerprint table's size (about 2.8 MB of the about 3.1 MB, every suffix of every text and background entry), accepted because `save` needs those lookups on every seed; (c) `save-layout` holding every sprite data entry rather than only those `updateSpriteData` leaves.
- The measurement scripts of this PRD were throwaway (scratchpad and `tmp/`, deleted); they printed counts only, never ROM bytes.
- `python3 --version` prints 3.13.18 in this environment, equal to Phase 1's record (`setup.sh` prints the same at run time).
- The KS map today: 6,105 spans, 59,971 bytes, 186 spans not rebuilt (all text bank bytes), per `known_source.py`.
