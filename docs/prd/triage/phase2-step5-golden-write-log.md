---
title: "LADX golden format and goldens"
description: "Every LADX case needs a committed golden: placement, spoiler, RNG trace, output SHA-1, and a write log tagged by inventory group. The log holds source references instead of ROM bytes, compares without the ROM, lets each group be claimed alone, and decodes back to the output SHA-1."
status: needs-triage
---

## Problem Statement

Phase 3 ports LADXR's seed-dependent part to C and must prove that it matches LADXR byte for byte, without the ROM, in `make check`. That needs evidence in `tests/ladx/cases/` and `tests/ladx/combos/`. Today the 116 case folders hold only `config.txt` and `seed`. `gen_golden.sh` writes its outputs (`run.txt`, `out.gbc`, `spoiler.json`) under `tmp/` only, because `out.gbc` is the ROM.

The missing piece is a committed **write log**. It must:

- record every byte LADXR writes, in LADXR's write order, each tagged with its inventory group (`notes/ladx-inventory.md`);
- encode every byte that comes from the source ROM as a source reference, never as a literal (§2);
- be produced the same way by the C side without the ROM, so that a claim compares exact encodings;
- let each group be claimed alone, edits made through `ROMWithTables` included;
- decode back to the output SHA-1 when the ROM is present;
- stay small enough to commit 116 of them (`ROMWithTables.save` alone writes about 120,000 positions per run).

The Nintendo-bytes check must print `0 literal runs` on every golden. The `computed` clearance that step 2.1 reserved (a TODO in `nintendo_bytes.py`) is built here.

## Solution

`gen_golden.sh` writes each case's golden into its own folder, lifting its `tmp/`-only rule for exactly these files. The ROM itself (`out.gbc`) and every other ROM-derived scratch file stay under `tmp/`.

A golden is:

- `run.txt`: the oracle's existing record. It holds the world setup, the pools, the placement taken right after `item_placer.run` from the logic objects, the RNG trace by stage, `rom_sha1`, `spoiler_sha1` and the outcome.
- `spoiler.json`: written on every case except `race` ones.
- The new `rom_writes.txt`.

`rom_writes.txt` is a sequence of **segments**, one per maximal run of consecutive writes by one inventory group. A segment holds:

- **bank records**: the segment's net bytes, as contiguous runs at absolute ROM offsets;
- **table records**: the segment's net `ROMWithTables` entry edits.

Each payload is a list of tokens: *keep* (source bytes at the record's own offsets), *copy* (source bytes from another offset) and *literal* (hex).

Which bytes become references is decided by a documented **encoding rule**:

- **Table groups:** every reference the ROM allows, because their encoding is precomputed off-device and C copies it (step 6's tables use the same encoder).
- **C groups:** only references the C side can know without the ROM:
  - bytes LADXR moves unchanged from the source, found by *origin tracking* in the oracle;
  - recomputed bytes equal to what they replace where C can know the replaced value from step 7's extractions (*inherit-on-equal*).
  - Every other byte is a literal.

A **scrambled-ROM test** proves that C groups' encodings do not depend on ROM content C cannot know.

The Nintendo-bytes check:

- gains the `computed` clearance, settled per run (guideline decision below);
- checks `rom_writes.txt` with a format-aware view that reads payload bytes at their true alignment;
- checks `run.txt` and `spoiler.json` in the general scope.

`tests/ladx/README.md` documents:

- the format;
- the "comparable without the ROM" property;
- the encoding rule;
- how to project one group;
- the size budget.

## User Stories

1. As the Phase 3 implementer, I want every case and combo folder to hold its golden, so that every C claim has committed evidence.
2. As the Phase 3 implementer, I want the placement in `run.txt`, recorded after the fill from the logic objects, so that the fill is claimed before any ROM write, `race` cases included.
3. As the Phase 3 implementer, I want the RNG trace by stage in the golden, so that I can claim the fill stage by stage, like ALttP's `trace.txt`.
4. As the Phase 3 implementer, I want the spoiler JSON in every non-race golden, so that the spoiler group has its own evidence.
5. As the Phase 3 implementer, I want the output ROM's SHA-1 in every golden, so that `rom-check` (step 9) has its expected value.
6. As the Phase 3 implementer, I want every written byte in a write log, in LADXR's write order, so that the C pipeline's log compares record for record.
7. As the Phase 3 implementer, I want every record tagged with its inventory group, so that a divergence names its group.
8. As the Phase 3 implementer, I want `ROMWithTables` entry edits (texts, entities, rooms, sprite data, backgrounds) logged as table records in the group that made them, so that `hints`, `marin` or `entrances` are claimed on their own edits, not only through `save`'s re-serialization.
9. As the Phase 3 implementer, I want to project one group's net effect out of a golden, so that I can claim one group while the others are still unported.
10. As the Phase 3 implementer, I want a group's projection to be unaffected by changes in another group's records, so that a claim never fails because of a group it does not claim.
11. As the Phase 3 implementer, I want a group's projection not to depend on how other groups' writes split it into segments, so that a group C has not ported yet (one that writes nothing) does not change the comparison.
12. As the Phase 3 implementer, I want the encoding rule written down in `tests/ladx/README.md`, so that my C logger produces the golden's exact tokens without the ROM.
13. As the Phase 3 implementer, I want bytes LADXR moves unchanged from the source encoded as references at their source offsets, so that C can emit them symbolically, never knowing their value.
14. As the Phase 3 implementer, I want recomputed room, entity, sprite-data, background and pointer bytes that equal what they replace to inherit the replaced bytes' encoding, so that re-encoding unchanged structure never produces Nintendo literals.
15. As the Phase 3 implementer, I want that inheritance limited to values I can know from the extraction fixtures, so that I never need the ROM to decide it.
16. As the Phase 3 implementer, I want table groups' encodings fixed by the oracle, so that step 6's tables and the goldens agree token for token.
17. As the owner, I want no golden to contain a ROM byte as a literal, so that the repository stays legal (§2).
18. As the owner, I want the Nintendo-bytes check to print `0 literal runs` on every golden, with every clearance counted, so that nothing is waved through silently.
19. As the owner, I want the `computed` clearance to clear only literals whose bytes the writing group never read from the ROM, so that a copy leaked into a literal can never be cleared.
20. As the owner, I want the goldens' total size measured, budgeted and checked, so that 116 cases do not bloat the repository.
21. As the run's agent, I want decoding each golden against the real ROM to give its `rom_sha1`, so that the log is proven complete and correct.
22. As the run's agent, I want two `gen_golden.sh` runs (hash seeds 0 and 1, a fresh clone) to give byte-identical goldens, so that the goldens are deterministic.
23. As the run's agent, I want every record to carry a tag, and the union of tags over all goldens to equal the inventory's writing groups, so that the inventory and the log agree.
24. As the run's agent, I want step 3's read-trace check to pass on all 116 cases, so that the inventory covers every case, not only the 19 traced in step 3.
25. As the run's agent, I want a scrambled-ROM test proving that C groups' encodings do not change when every ROM byte C cannot know is scrambled, so that "reproducible without the ROM" is shown, not assumed.
26. As the run's agent, I want the encoder separated from the LADXR run (events recorded once, encoded after), so that the scrambled-ROM test and the SHA-1 decode reuse one run.
27. As the run's agent, I want a case whose golden cannot reach 0 literal runs only because of a seed-dependent coincidence to get its seed re-picked by step 2.4's rule, with the refusal recorded, so that the fix happens at the oracle (§2).
28. As the run's agent, I want a systematic encoder failure to stop the run, not be hidden by re-picks, so that a leak in origin tracking gets fixed.
29. As a future session, I want `tests/ladx/README.md` to explain how to regenerate, check and read a golden, so that I can work without reading this PRD.
30. As a future session, I want `CLAUDE.md` to describe the goldens, the new scripts and the changed policy, so that the checks are discoverable.
31. As the owner, I want `make test` to stay fast and ROM-free while checking that every case folder holds its golden and that the budget holds, so that a missing golden is caught without the ROM.
32. As the owner, I want the push gate to accept the goldens' push under the same policy, with its time recorded, so that later pushes are not slowed by surprise.

## Implementation Decisions

### Golden files

Each case folder gets three generated files, all from one `gen_golden.sh` run:

- `run.txt`: today's `oracle_run.py --rom` output, unchanged in format.
- `spoiler.json`: absent on `race` cases.
- `rom_writes.txt`.

`config.txt` and `seed` stay as step 2.4 wrote them, unless a case is re-picked (see Seed re-pick). `meta.txt` (differs between runs), `out.gbc` (the ROM), the event stream and `reads.txt`/`writes.txt` stay under `tmp/`.

### `gen_golden.sh`

- Default `--out` becomes the case folders.
- The `tmp/` refusal stays for every other `--out`.
- With the folders as `--out`, it writes only the three golden files into each case folder.
- `out.gbc`, the event stream and the read trace go to a `tmp/` mirror (default `tmp/ladx-goldens/<case>/`).
- Every case runs with the read trace on (`--reads`), because tags and read sets come from it. This costs about 8 s per case instead of 0.6 s (measured), so about 15 min per full run.

### Event stream (oracle side, `tmp/` only)

`read_trace.py` gains an ordered record of every write: group, site, target (bank or table), positions and the values written. Each written byte carries its **origin**: `src(q)` (an unchanged copy of source byte `q`) or `known`.

Origin tracking:

- Every ROM position starts as `src(p)`.
- Bank slices read through the traced bank come back as a byte type carrying per-byte origins. Index reads come back as an int type carrying one.
- Parsed table entries keep their origins.
- Slicing, `bytes()`, concatenation, assignment to a bank or a table, and `PointerTable.store`'s moves and suffix de-duplication preserve origins.
- Any arithmetic or re-encoding yields `known`.
- A leak can only turn a reference into a literal, never the reverse. The 0-literal-run check and the scrambled-ROM test catch it, and the fix goes into the oracle.
- The stream (values, origins, groups, order) is written under `tmp/`.

### Encoder

A new oracle module (working name `write_log.py`) turns an event stream plus a ROM into `rom_writes.txt`. It also decodes a log against a ROM, projects a group, and runs its own self-test.

### `rom_writes.txt` format, version 1

Line-based ASCII, lowercase hex, no `0x` prefixes. It starts with a version line. `G <group>` opens a segment. Then come records:

- `B <offset> <tokens>`: a contiguous run of net bank bytes at that absolute ROM offset. Records are sorted by offset within the segment.
- `T <table> <key> <tokens>`: a table entry's net new value, keyed by its index or alt key. An integer-pointer entry is written as a pointer token. Records are sorted by `ROMWithTables`' table order, then by key.

Tokens are separated by single spaces:

- `=<n>`: keep, n source bytes at the record's own offsets (bank records only);
- `@<q>+<n>`: copy n source bytes from absolute offset q;
- a run of literal hex.

Adjacent references that continue each other are merged, adjacent literals are joined, and lengths are in hex. Group segments follow LADXR's write order. Within a segment, writes collapse to their net effect, which replays identically at every segment boundary.

### Group projection

A group's projection replays its segments in order into its own net bank map and net table map, ignoring every other group. It then re-encodes those maps as canonical records. Claims (Phase 3) compare projections, so segmentation and other groups never affect a group's verdict. The projection needs no ROM: it works on tokens.

### Decode

Replay every segment's bank records in order onto the source ROM. Table records are evidence only, because they reach the ROM through `save`'s bank records. The result's SHA-1 must equal `run.txt`'s `rom_sha1`.

### Encoding rule

**Table groups** (inventory side `table`). The encoder takes, at each byte:

1. a keep run, if the byte equals the source at the same offset;
2. else its `src(q)` origin, if the event stream gives one;
3. else the longest ROM match of at least 4 bytes, at the lowest offset;
4. else a literal byte.

This leaves no aligned 4-byte literal that occurs in the ROM. Step 6 builds its tables with this same encoder.

**C groups.** For each byte:

- `src(q)` bytes become references: keep when q is the destination, copy otherwise.
- `known` bytes go through **inherit-on-equal**: a byte equal to the byte it replaces inherits that byte's encoding, if C can know the replaced value.
  - "Can know" means the replaced byte is a literal, or `src(q)` with q in the **C-knowable source map** (KS).
  - For bank writes, the replaced byte is at the same offset.
  - For table entries, the new value is aligned to the previous value by common prefix, then common suffix.
- Every other `known` byte is a literal.
- No arbitrary-ROM matching is ever applied to a C group.

**KS** is the set of source offsets whose values step 7's extractions let C rebuild:

- every room's entity entry and object entry, alt rooms included (`room-entities`, `room-objects`);
- the `boss-sources` bytes;
- background entries 3, 4 and 0x0D (`backgrounds`);
- the pointer and bank tables of every table `save` re-serializes, and the room sprite data entries (`save-layout`).

KS is built from the source parse in a small oracle module, restricted to those fields. A test proves that each KS span is rebuilt exactly from them (LADXR's own editors round-trip unchanged source rooms; pointers recompute from spans). The README states this as a requirement step 7's fixtures must meet.

### Scrambled-ROM test

Build ROM′: every byte outside KS replaced by a fixed scramble. Re-encode the same event stream with ROM′. Every C group's projection must be identical to the one from the real ROM. Table groups may change. This is the "reproducible without the ROM" verification. Any difference is printed per group and fails the test.

### Nintendo-bytes policy for case folders

Guideline decision, recorded here and in the step note. Inside `tests/ladx/cases|combos/<case>/`:

- `run.txt` and `spoiler.json` are checked in the general scope. They hold LADXR names, decimal numbers and digests, never a ROM byte. Measured: 2 to 7 raw strict hits per case from ASCII identifiers whatever the separator, and 3 spoiler hits.
- `rom_writes.txt` gets a new format-aware view, `writes`:
  - the file must parse, else it fails as a literal;
  - literal tokens are decoded at their true byte alignment and checked at 4 bytes;
  - the rest of its text (offsets, tags, reference tokens) is checked at the general thresholds.
- `seed`, `config.txt`, every other strict path and the seed pick of `ladx_cases.py` keep step 2.1's policy unchanged.

The check's self-test gains planted files for each of these rules.

### `computed` clearance

Guideline decision, refining step 2.1's group-level rule, whose rationale it keeps. Measured: `items` and `goal` hint texts, and `bank3e`'s assembled code, coincide with ROM content, but each group also reads ROM elsewhere, so the group-level rule cannot clear them.

In the `writes` view, a 4-byte literal run is cleared `computed` iff:

- it lies inside one literal token of one group g; and
- its bytes occur nowhere in the source bytes g reads.

g's read set comes from the inventory's `Reads (trace)` ranges for g's rows:

- bank ranges are taken as source bytes;
- table ranges resolve to their entries' source spans through the ROM parse the check already has the ROM for;
- `computed`-kind checksum reads are excluded;
- `written` and `mixed` ranges are followed to the writer groups' read sets.

Over-approximation only shrinks clearances. Every clearance is counted and listed with `--verbose`. The TODO in `_clearance` is replaced by this.

### Inventory refresh

`inventory_trace.sh` and `read_check.py` run over all 116 cases, not only `determinism_cases.txt`. `Reads (trace)` is regenerated with `read_check.py --print`, and `inventory_check.py` stays green. Any new finding is fixed at the oracle or the inventory before goldens are committed. Without the refresh the clearance's read sets would be incomplete.

### Seed re-pick

`ladx_cases.py pick` and `verify` gain one refusal reason: `golden-bytes`, the candidate's golden does not pass the Nintendo-bytes check.

- Seeds that change are re-derived by the existing rule (candidate k, first passing k < 64), in their own commit.
- The step note lists each refused candidate with the group, file and token of its literal run.
- If more than 10 cases need a re-pick, or one group causes more than half the refusals, the run stops for a fix in the encoder or origin tracking. Re-picks never cover a systematic failure.

### Size budget

| What | Limit |
|---|---|
| `rom_writes.txt`, per case | 192 KB |
| `rom_writes.txt`, all cases | 12 MB |
| All golden files (`run.txt`, `spoiler.json`, `rom_writes.txt`) | 25 MB |

Measured today: `run.txt` 6.5 MB and `spoiler.json` 5.1 MB over the 116 cases. A prototype of the format gave about 74 KB of bank records on the baseline, plus table records.

What keeps it small:

- net effect per segment;
- one record per contiguous run;
- the group named once per segment;
- merged references and keep tokens (overlays, unchanged pointers, unchanged entries moved by `save`);
- short hex fields.

`ladx_cases.py check` (already in `make test`, ROM-free) gains:

- every case folder holds `run.txt` and `rom_writes.txt`, and `spoiler.json` exactly when `race` is off;
- `rom_writes.txt` parses and carries its version line;
- the budget holds, with the totals printed.

### Tags

A new verification fails on:

- any write whose group is `-`;
- any tag outside the inventory's groups with side `table` or `C`;
- a union over all goldens that differs from that set minus `spoiler` (which writes no ROM byte; its evidence is `spoiler.json`).

### `tests/ladx/README.md` (new)

It documents:

- the case folder layout;
- each golden file;
- the write-log grammar;
- write order and segments;
- projection;
- decode;
- the encoding rule with KS and inherit-on-equal;
- the scrambled-ROM property;
- the Nintendo-bytes views and clearances that apply;
- the budget;
- how to regenerate (`gen_golden.sh` with the ROM, own commit, reason) and verify.

`CLAUDE.md` gets the matching lines.

### Determinism

`determinism.sh --full --all` already compares every file the runs write. It now also compares `rom_writes.txt`, with the second run's goldens written to a `tmp/` tree through the existing `--out` path.

### Golden regeneration

Goldens are ROM-derived artifacts: never edited by hand, regenerated only with the oracle in their own commit with the reason (§2). This step generates them for the first time.

## Implementation Chunks

1. **Write-event recording with origins.** Extend `read_trace.py`: an ordered write record with values, and origin tracking for bank bytes, ints, table entries and `PointerTable.store`. Self-tested on the synthetic banks with planted copies (slice, index, table entry copy, suffix de-dup, concatenation) and planted computed writes. `oracle_run.py --rom --reads` writes the event stream under `tmp/`.
2. **Encoder, format, decode, projection.** Build `write_log.py`: the v1 grammar, segments, net effect, table- and C-group rules, inherit-on-equal, the KS builder and its round-trip test, decode, projection, and the scrambled-ROM comparison. Unit tests on synthetic ROMs and event streams, ROM-free, in `make test`.
3. **Nintendo-bytes policy.** Add the `writes` view, the general scope for `run.txt` and `spoiler.json` inside case folders, and the per-run `computed` clearance from the inventory's read ranges. Self-test planted files for every rule, including a cleared computed run, a run in g's read set that is not cleared, a misaligned nibble that is not flagged, and an unparseable `rom_writes.txt`. The push gate picks up the same policy; its tests still pass.
4. **Inventory refresh over every case.** Run `inventory_trace.sh` over all cases and refresh `Reads (trace)`. `read_check.py` and `inventory_check.py` must be green on all 116 cases.
5. **`gen_golden.sh` into the case folders.** Default output to the folders, `tmp/` mirror, `--reads` on, the encoder run after each case. A verifier script (working name `golden_check.py`, with ROM) checks, per case, the decode SHA-1, tags, scrambled-ROM equality, projection independence on a planted foreign-group change, and the Nintendo-bytes result. `ladx_cases.py check` gains the folder, format and budget checks.
6. **Generate and settle.** Generate all goldens. Re-pick seeds where only a seed-dependent coincidence blocks 0 literal runs (`golden-bytes`), in their own commit. Run the double run and every verification, then commit the goldens in their own commit. Write `tests/ladx/README.md`, update `CLAUDE.md`, and write the step note.

## Testing Decisions

- Good tests check behaviour through each tool's public entry point: a planted event stream gives the expected `rom_writes.txt`; a planted golden decodes to the expected bytes; a planted file gives the expected check lines. They never inspect internals.
- **Origin tracking** (`test_read_trace.py`): planted copies through every preserved path come out `src(q)` with the right q; arithmetic gives `known`; table-entry copies and `store` moves keep origins. Uses the real `rom.ROM` and `pointerTable.PointerTable` from the checkout, as today's self-test does.
- **Encoder** (new `test_write_log.py`, ROM-free, `make test`):
  - net effect within segments;
  - merging of records and references;
  - keep, copy and literal tokens;
  - table records, including pointer entries and alt keys;
  - the table-group greedy rule (no aligned 4-byte literal found in a synthetic ROM);
  - C-group rule: no arbitrary match; inherit-on-equal by prefix/suffix on entries and by same offset on banks; no inheritance outside KS;
  - decode round trip;
  - projection independence from other groups and from segmentation;
  - the scrambled-ROM property on a synthetic case where it holds, and a planted C-group encoding that depends on a non-KS byte, which it must flag;
  - grammar errors.
- **Nintendo-bytes** (`test_nintendo_bytes.py`): the new planted cases. The existing 34 cases are unchanged except where the policy table is restated.
- **Cases** (`test_ladx_cases.py`): planted trees missing a golden file, with a spoiler on a race case, over budget, or with a malformed `rom_writes.txt`; plus the `golden-bytes` refusal reason with a stub check.
- **With the ROM** (step-note evidence, not `make check`):
  - `golden_check.py` on all 116 cases: decode SHA-1, tags, scrambled-ROM, projection independence, and `0 literal runs`;
  - `determinism.sh --full --all` all `same`;
  - `read_check.py` green over all cases;
  - totals against the budget.
- **Prior art:** ALttP's `run_e2e.sh` and `write_groups.txt` claim a write-log prefix per group; this step replaces prefixes with projections. Also `test_read_trace.py`'s planted banks, and `nintendo_bytes.py`'s planted self-test.

## Out of Scope

- The patch tables in `data/ladx/` (step 6). Step 6 must use this encoder for table groups.
- The extraction fixture files themselves (step 7). This step only builds KS from the source parse and states what step 7's fixtures must let C rebuild.
- The LADX e2e driver and its claims (Phase 3 step 4 onward), and the `rom-check` target (step 9).
- Changing LADXR's spoiler format, or any `ladxr.patch` change not forced by a determinism or encoding defect.
- Reworking the general scope, or the strict scope of `data/ladx/` and `tests/ladx/extract/`.

## Further Notes

Measured on the baseline (`tmp/prd5/`, ROM-derived, throwaway):

- **Event volume:** 11,350 write events.
- **Writes by group:**

  | Group | Writes | Positions |
  |---|---|---|
  | `save` | 7,815 | 120,906 |
  | `entrances` | 92 | 7,360 (all overlays rewritten unchanged) |
  | `bank3e` | — | 10,805 |
  | `endscreen` | — | 6,361 |
  | `fixed` | — | 10,231 |

- **Table-entry edits:** 1,400 (`sprites` 789, `entrances` 407).
- **Coincidences with ROM content in C groups:** after keep, entry equality and prefix/suffix anchoring, some literal runs still coincide with ROM content: `bank3e` 392, `hints` 74, `marin` 53, `items` 30, `title` 113, `sprites` 61, `warp-home` 23, `entrances` 14, `bosses` 6, `goal` 3.
  - In `hints`, `marin`, `items`, `goal` and `bank3e`, most are computed texts and code. The per-run clearance is for those.
  - The rest are re-encoded structure that origin tracking plus inherit-on-equal must turn into references; chunk 6 records what remains.
- **Run time:** about 7.6 s per traced run. The Nintendo-bytes check runs at about 2.5 s per case's goldens, so a full check takes about 5 min. Record the push gate's time on the goldens push in the step note.
- **Pick-rule history:** step 2.4's seeds were screened on the seed file alone. The `golden-bytes` refusal extends that rule as a fix at the oracle.
- **Change to step 2.1's policy:** both guideline decisions above (case-folder scopes with the `writes` view, and the per-run `computed` clearance) change step 2.1's calibrated policy. They go in the step note and the plan's PR for the owner's review.
