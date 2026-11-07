---
title: "No source fragment in C-group literals (plan 3, step 3.6.1)"
description: "The LADX write logs and patch tables still spell 1–3-byte pieces of the source ROM's table entries as literals; the oracle's encoder must reference them, and a check must prove none is left."
status: needs-triage
---

## Plan step

Plan `docs/plan-3.md`, Phase 3 (LADX library, host only), step 6, sub-step 1.
`progress.md` keeps the old title of this line ("ROM-free proof of save's
packing"), because the hook forbids renaming it. The plan text below is
authoritative.

Step 6's shared paragraph (verbatim):

> 6. **Seed-dependent writes**, in nineteen sub-steps: two that every group needs, then one per `C` group of `notes/ladx-inventory.md`'s Groups table (17), each named by its group. Each sub-step runs as its own plan step, with its own PRD and `### Step 3.6.<k>` note; step 6 itself has no PRD, and gets a one-line closing note once 6.1 to 6.19 each have theirs.
>
>    The order is not `generateRom`'s. A group comes after every `C` group it depends on, in any of three ways: it reads bytes that group wrote (the Groups table's Depends on; for `save`, which the trace cannot see reading its entry lists, every group that writes a `T` record); it overwrites bytes that group wrote, so its encoding inherits theirs (`tests/ladx/README.md`, "Encoding rule"); or it continues that group's RNG stream (`rnd`: `marin`, `hints`, `music`; the global `random`: `marin`, `endscreen`). The C pipeline runs the ported groups in LADXR's order and skips the others, so each claim is reached with the sub-steps before it. A group's claim is its projection (`tests/ladx/README.md`, "Projection and claims") of the C write log equal to the golden's on every case and combo, through step 5's e2e driver, plus the `trace` lines of `run.txt` drawn by its own functions when it draws. A group whose Evidence lists `rom`, or that 6.2 finds needs KS to encode, is claimed by a `rom-group` line, verified by `make -C tests/ladx e2e-rom` with the real ROM (summary line in the step note); every other group by a `group` line, verified by `make check`'s `e2e` and `e2e-combos`. At each sub-step both runs are green and every group claimed before it stays claimed in both. Unit tests per module run under `make test` and `make unit-arm`, on synthetic data only. A claim a Phase 2 artifact cannot reach is fixed at the oracle (§2), never cut.

Sub-step 6.1 (verbatim):

> 1. **No source fragment in C-group literals** (oracle side, with the ROM, before any C). KS-match references only runs of 4 bytes or more, so a C group's literal in an edited entry of a KS table can still carry a 1–3-byte fragment of the source entry it replaces. First, `golden_check.py` gains a check `fragments`: a literal run, of any length, of a C group's record in a KS table that occurs in the KS bytes of the entry it replaces. It is proven on a planted golden, and its count on the committed goldens is the step's red evidence. Then `write_log.py`'s encoder references every such byte. The rule is deterministic and reproducible by C from KS alone, and it is stated in `tests/ladx/README.md`'s "Encoding rule" before 6.2 ports it. The goldens are regenerated with `gen_golden.sh`, with the ROM, in their own commit (reason and counts in the message and the step note; §2). The goldens are at their budget (34.0 of 34 MiB), so any raise is a deliberate change with its reason, never a dropped check. Verified by `golden_check.py` 0 bad, with `fragments` 0 on every case and combo; `nintendo_bytes.py` `0 literal runs` on the goldens; `table_replay.py` 0 bad; `ladx-cases` green; and `make check` green with the claims of steps 4 and 5 unchanged.

Governing rule (§2): nothing in `data/ladx/` or in the LADX goldens contains
Nintendo bytes, and nothing committed holds ROM content in any other encoding
either (owner decision, commit 782e21a).

## Problem Statement

The write logs (`rom_writes.txt`) and the patch tables (`data/ladx/`) must not
hold any ROM content. The encoder references a byte that comes from the source
ROM when its origin is tracked, or when it is part of a match of 4 bytes or
more (KS-match for `C` groups, the longest ROM match for `table` groups). A
computed entry that rebuilds a source entry in short pieces (an entity list
with one entity changed, a sprite set re-chosen, a background patched) keeps
those pieces as literals when each is 1 to 3 bytes. The Nintendo-bytes check
looks for runs of 4 bytes or more, so it does not see them. Read together,
these pieces spell the source entries: in `data/ladx/options.txt`, 4,826 of
the 4,858 literal bytes of KS-table `T` records are such fragments.

Measured on today's 128 goldens (88 cases, 40 combos) and on `data/ladx/`
(counts only, from the ROM at `tmp/ladx/base.gbc`, KS from
`known_source.build`; no byte printed). A fragment is a piece found by the
walk defined under Implementation Decisions.

`C` groups, `T` records of KS tables (139,337 fragments, 167,604 bytes,
about 1,090 per golden; by length: 114,436 of 1 byte, 21,535 of 2, 3,366 of
3, none longer):

| group | table(s) | fragments | bytes |
|---|---|---|---|
| sprites | room_sprite_data_indoor, _overworld | 76,756 | 83,010 |
| title | background_tiles, background_attributes | 41,339 | 59,127 |
| warp-home | background_tiles | 14,592 | 15,488 |
| entrances | entities, rooms_indoor_a, rooms_overworld_top, _bottom | 4,115 | 5,896 |
| bosses | entities, rooms_indoor_a, rooms_indoor_b | 1,669 | 3,086 |
| enemizer | entities | 607 | 609 |
| items | entities | 256 | 384 |
| goal | entities | 3 | 4 |

`C` groups, `B` records at KS offsets, measured against the KS span that holds
them: `save` 29,063 fragments (33,594 bytes, mostly pointer bytes and packed
entry bytes), `sprites` 128 (128 bytes). After the `T` rule alone, `save`
still has 15,899 (16,924 bytes): the carried copies fix only part of them.

`table` groups, `T` records of KS tables: in the goldens, 11,351 fragments
(16,629 bytes: `fixed` 8,064, `options` 3,285, `goal-fixed` 2). In
`data/ladx/`, 3,795 fragments (7,285 bytes): `options.txt` 3,720 (entities
2,223, rooms_indoor_a 824, rooms_indoor_b 672, rooms_overworld_top 1),
`fixed.txt` 63, `goal-fixed.txt` 12. `table` groups have no literal `B` byte
at a KS offset, in the goldens or in the tables.

`texts` entries are not in KS (C does not parse the source texts as KS), so
their literals are out of this rule. They stay under the Nintendo-bytes check,
which has 0 literal runs today.

## Solution

At the oracle, before any C: one fragment rule, the same for `C` and `table`
groups, in `write_log.py`'s encoder. A literal piece of a record that occurs in
the source bytes of the KS entry or span it replaces becomes a copy of that
entry. The rule needs only KS, so C reproduces it in 6.2. A new `fragments`
check in `golden_check.py`, and the same scan of the patch tables in
`table_replay.py`, prove that no fragment is left. The goldens and
`data/ladx/` are regenerated with the ROM in one commit that holds only those
files. Only `rom_writes.txt` and three table files change; `run.txt`,
`spoiler.json` and every claim stay as they are.

A prototype of this rule in the scratchpad re-encoded all 128 event streams.
Every log decoded to its golden `rom_sha1`. `scrambled` was clean on
`baseline` and `sample-09`, and `nintendo_bytes.py` gave 0 literal runs on
both. The fragment scan found 0 fragments in every group. The unpatched encoder
gave the committed goldens byte for byte, so the measured change comes from
the rule alone.

Budget (limits in `ladx_cases.py`: 224 KiB = 229,376 bytes per case,
22 MiB = 23,068,672 for all `rom_writes.txt`, 34 MiB = 35,651,584 for all
golden files):

| encoding | all rom_writes.txt | all golden files | largest case (sample-09) |
|---|---|---|---|
| today | 21,377,797 | 34,021,731 (95.4%) | 211,047 |
| C `T` rule only | 22,755,456 | 35,399,390 | 224,099 |
| C `T` and `B` | 22,841,941 | 35,485,875 | 225,392 |
| **chosen: C and table, `T` and `B`** | **22,889,488 (99.2%)** | **35,533,422 (99.7%)** | **226,045 (98.5%)** |

The chosen rule fits every limit, so the budget is not raised. Only 118,162
bytes are left under the 34 MiB limit, so the next case added will need a
deliberate raise. `data/ladx/` grows from 534,590 to about 551,052 bytes. It
has no budget.

## User Stories

1. As the owner, I want no committed file to hold a 1–3-byte piece of a source entry as a literal, so that the repository stays legal under §2's "any encoding" rule.
2. As the owner, I want the fragment count on today's goldens recorded as the step's red evidence, so that I can see the size of the leak that was closed.
3. As the owner, I want the counts given per group and per table, and never a byte value, so that the evidence carries no ROM content.
4. As a Phase 3 implementer, I want `golden_check.py` to fail with a `fragments` problem when a golden holds a fragment, so that a regression in the encoder can't pass unseen.
5. As a Phase 3 implementer, I want the `fragments` check proven on a planted golden, so that I know it can fail.
6. As a Phase 3 implementer, I want the check to name the group, the record line and the counts, so that I can find the offending record without seeing bytes.
7. As a Phase 3 implementer, I want the fix in the oracle's encoder, before any C, so that 6.2 ports one settled rule.
8. As the 6.2 implementer, I want the rule stated in `tests/ladx/README.md`'s "Encoding rule" in terms of KS alone, so that C can reproduce it without reading the oracle's code.
9. As the 6.2 implementer, I want KS to expose its entries by table and key, and its spans by offset, so that C's KS can mirror the same interface.
10. As the 6.2 implementer, I want the pieces chosen by a fixed walk (left to right, longest first, at the lowest offset), so that the output never depends on iteration order.
11. As a reviewer, I want the encoder rule to stay reproducible without the ROM outside KS, so that `golden_check.py`'s `scrambled` check stays clean.
12. As a reviewer, I want the rule to apply to `B` records at KS offsets too, so that `save`'s packed pointer and entry bytes carry no fragment either.
13. As a reviewer, I want carried copies (write identities) to follow a referenced byte, so that `save`'s re-serialized bytes inherit the reference without extra rules.
14. As the owner, I want the patch tables in `data/ladx/` held to the same rule, so that §2's guarantee covers both kinds of ROM-derived artifact.
15. As a Phase 3 implementer, I want `table_replay.py` to report fragments in the patch tables, so that the tables can't drift back.
16. As a maintainer, I want the goldens and tables regenerated only by `gen_golden.sh` and `gen_tables.sh`, with the ROM, in one commit that holds nothing else, so that ROM-derived artifacts are never edited by hand.
17. As a maintainer, I want that commit's message to give the reason and the before and after counts, so that the change can be audited from git alone.
18. As the owner, I want the budget kept without a raise, so that growth stays a deliberate decision.
19. As a maintainer, I want `run.txt` and `spoiler.json` to stay byte-identical through the regeneration, so that the change is known to touch only the encoding.
20. As a maintainer, I want `order.txt`, the units and the entries of `data/ladx/` unchanged, so that the tables' keys and order are known to be untouched.
21. As a maintainer, I want `gen_tables.sh --check` to rebuild the new tables byte-identical on a fresh clone, so that the table encoding stays deterministic.
22. As a maintainer, I want `table_replay.py` to give 0 bad on all 128 replays, so that the tables and the goldens agree after the change.
23. As a maintainer, I want `make check` green with `claims.txt` unchanged, so that steps 4 and 5's claims (fill and the table groups) hold on the new artifacts.
24. As a maintainer, I want `nintendo_bytes.py` to give `0 literal runs` on the goldens and on `data/ladx/`, so that the larger check still passes.
25. As a maintainer, I want `ladx-cases` green, so that the budget and the golden format hold.
26. As a maintainer, I want `CLAUDE.md`'s lines for `golden_check.py` and `table_replay.py` to name the new check, so that the contract has one home in the README and one line in `CLAUDE.md`.
27. As an agent running the oracle, I want no command to print a ROM byte or the link, so that logs and transcripts stay clean.

## Implementation Decisions

### Definition of a fragment (the check)

- A fragment is a piece of a record's literal bytes, found by this walk. Take
  each maximal run of literal bytes in the record. Walk it left to right. At
  each position, the piece is the longest prefix of at least 1 byte that occurs
  in the reference bytes, and the walk goes on after it. Where no byte
  matches, the walk moves on one byte.
- Reference bytes of a `T` record: the source bytes of the KS entry at the
  same table and key (alt keys included). This applies only when that source
  entry is a span in KS (`known_source.build` rebuilt it). Integer-pointer
  entries, `texts`, and keys outside a table's known set (background entries
  other than 3, 4 and 0x0D of the tiles, 3 and 4 of the attributes) have no
  reference bytes.
- Reference bytes of a `B` record: each maximal literal run is first cut at
  the boundaries of KS spans, and offsets outside KS are skipped. Each piece is
  then matched against the source bytes of the KS span that holds it. When
  spans overlap, the one with the lowest start is used. Today 605 duplicate
  pairs are identical spans (shared, de-duplicated entries), and 2 are nested
  background entries (3 inside 4).
- **Scope (decision): every group, `C` and `table`, `T` and `B` records.** The
  plan names `C` groups' records in KS tables. Measurement shows the same
  leak in the `table` groups (11,351 fragments in the goldens, 3,795 in
  `data/ladx/`; `options.txt`'s entity literals are almost all fragments) and
  in `save`'s `B` records (29,063). §2 covers `data/ladx/` and the goldens
  alike, with no exception by side or by record kind. The wider scope costs
  134,032 bytes over the `C` `T` rule alone and fits the budget. C pays
  nothing for the `table` side: it copies table encodings verbatim. So the
  rule is not cut by side.

### `known_source.py` (KS)

- `KnownSource` gains two lookups, filled by `build`:
  - `entries`: `{(table, key): (start, stop)}` of every entry span added to
    KS (entries only; pointer bytes, bank bytes and boss bytes are not
    entries).
  - `span_at(q)`: the `(start, stop)` of the KS span holding source offset
    `q`, with the lowest start, or `None` outside KS.
- Nothing else in KS changes: same spans, same bytes, same `NOT-REBUILT`
  output. Nothing derived from it is committed or printed.

### `write_log.py` (the finder and the encoder rule)

- A public finder: for a parsed log (or one record's kind, table, key or
  offset, and atoms), with the source ROM and a `KnownSource`, it returns each
  fragment's record and position (record line, start index, length) as the
  walk above defines it. It returns counts and positions only. It is written
  separately from the encoder's pass, so the check stays independent of the
  fix.
- **Encoder rule, `C` groups** (new last step, after KS-match, once the
  segment ends): on the segment's net atoms, the walk is run over every `T`
  entry that has reference bytes and over every `B` run at KS offsets. Each
  piece becomes a copy of its lowest offset in the reference bytes. The
  write identities the piece names are renamed, as KS-match does, so a later
  carried copy (`save`'s packing) inherits the reference. Measured: this
  removes every `save` fragment.
- **Encoder rule, `table` groups** (new step 4, between the longest ROM match
  of at least 4 bytes and the literal): the same walk on the same records.
  These pieces are not longest-match matches: they are not added to the
  `matches` list `gen_tables.py` uses for couplings, so no unit is fused
  because of them.
- With a `known` that has no `entries` or `span_at` (a plain container, as in
  planted tests and `gen_tables.py`'s default `known=()`), the step does
  nothing. `gen_tables.sh` already passes `known_source.build(rom)`.
- The rule depends only on the computed values, carried origins and KS bytes
  and spans, so it can be reproduced by C from KS alone. `scrambled` proves
  this: KS bytes are kept, and every reference lies inside a KS entry or span.
- The format does not change (still version 1). A piece is an ordinary
  `@<q>+<n>` token, merged with its neighbours by the canonical rendering.
  `I` lines are recomputed from the remaining literals.

### `golden_check.py`

- New check `fragments`, run per case on `rom_writes.txt` with the ROM and
  KS. Each group with a fragment gives one `BAD <case>: fragments: <group>
  <n> fragments, <b> bytes, first at line <l>` line (no byte value).
- The summary becomes `golden-check: <n> cases, <b> bad, <k> literal runs,
  <f> fragments`, where `<f>` counts fragments over every case checked.

### `table_replay.py`

- Before the replays, a fragment scan over every `B` and `T` record of the
  tables under `--tables`. Each record with a fragment gives one `BAD tables:
  fragments <file> line <n>: <k> fragments, <b> bytes` line, and makes the
  run fail (exit 1).
- The self-test gains one planted table holding a fragment, so it reads
  `table-replay: self-test, 5 planted replays as expected`.
- The summary becomes `table-replay: <n> cases, <b> bad, <f> table
  fragments`.

### Docs

- `tests/ladx/README.md`:
  - "Encoding rule" gains the walk, its reference bytes and where it sits in
    each side's sequence. The text is written to be ported by 6.2 and states
    that C needs `entries` and `span_at` of its KS.
  - "Regenerate and verify" names the new check.
  - "Patch tables" says that the tables follow the same rule.
- `write_log.py`'s docstring rule is updated to match.
- `CLAUDE.md`'s `golden_check.py` and `table_replay.py` lines name the
  `fragments` check and the new summary fields.

### Regeneration (decision: one commit for both artifact sets)

- Run with the ROM and LADXR at the pin: `gen_tables.sh` (about 41 min), then
  `gen_golden.sh` on every case and combo.
- Both go into one commit that holds only regenerated files. That commit
  touches only `rom_writes.txt` under the case folders and
  `data/ladx/{fixed,goal-fixed,options}.txt`.
- Reason for one commit: step 5's e2e claims compare C's encoding of
  `data/ladx/` with the goldens' table-group projections. A commit with only
  one side regenerated would fail `make check`.
- The commit message gives the reason (§2, owner decision 782e21a) and the
  before and after counts: fragments per group, bytes, and the budget figures.
- **Budget (decision): no raise.** The prototype lands at 35,533,422 of
  35,651,584 bytes. If the real regeneration goes over a limit, the only
  allowed answer is a deliberate raise of that limit to the next whole MiB
  (all golden files to 35 MiB, or `rom_writes.txt` to 23 MiB), in its own
  commit before the regeneration, with the measured sizes as its reason.
  Dropping or narrowing the rule is not allowed.

## Implementation Chunks

1. **KS lookups, the fragment finder and the checks.**
   - Add `entries` and `span_at` to `KnownSource`.
   - Add the finder to `write_log.py`.
   - Add the `fragments` check to `golden_check.py` and the table scan to
     `table_replay.py`, each with its planted proof.
   - Update the README, `CLAUDE.md` and the docstrings for the checks.
   - Then run `golden_check.py` and `table_replay.py` on today's goldens and
     tables with the ROM. They must report `fragments` on all 128 goldens and
     on the three table files. These counts are the red evidence. Take them
     from the summary and the `BAD` lines, compare them with the figures
     above, and record them for the step note.
2. **The encoder rule.**
   - Add the new step to `write_log.py` for both sides, with the README's
     "Encoding rule" and the docstring.
   - Add unit tests on synthetic ROMs and KS.
   - Until the next chunk lands, `golden_check.py` reports `encode` and
     `fragments` on the old goldens. This is expected and outside the net.
     `make check` must stay green: none of these tools needs the ROM there.
3. **Regeneration (its own commit, ROM-derived files only).**
   - Run `gen_tables.sh`, then `gen_golden.sh` on every case and combo.
   - Verify everything listed under Testing Decisions.
   - Commit with the counts in the message.

## Testing Decisions

- Good tests check external behaviour only: a synthetic ROM, KS and event
  stream go in, and the log text or the check lines come out. They never
  check the encoder's internals. Every test runs on synthetic data, without
  the ROM, LADXR or the network, under `make test-ladx-oracle` (in
  `make check`).
- `test_known_source.py`:
  - On the planted layout, `entries` holds every rebuilt entry and none of the
    failed ones.
  - `span_at` gives the lowest-start span for nested and duplicate spans, and
    `None` outside KS.
- `test_write_log.py`:
  - The finder: a planted 1-, 2- and 3-byte piece of the replaced entry is
    found. A literal that is not in the entry is not, even when it is
    elsewhere in the ROM. A `texts` entry and a key outside KS are never
    scanned. A `B` run crossing a span boundary is cut there.
  - The encoder, `C` side: a `T` fragment becomes a copy at the lowest offset
    in the entry. A `B` fragment at a KS offset becomes a copy. A later
    carried copy of the renamed write identity inherits the reference. A
    plain-container `known` leaves the encoding as before. `scrambled` stays
    clean on a planted case that uses the rule.
  - The encoder, `table` side: the piece is a reference, and it does not
    appear in `table_segments`' match list.
  - Determinism: when two equal pieces are in the entry, the lowest offset is
    chosen.
- `test_golden_check.py`: a planted golden with a fragment gives a
  `fragments` problem naming its group, and the clean planted golden still
  gives none.
- `test_table_replay.py` and the self-test: a planted table holding a fragment
  is reported, and the clean planted tables are not.
- Prior art:
  - `test_write_log.py`'s planted encode and scramble cases.
  - `test_golden_check.py`'s planted case folders.
  - `table_replay.py`'s four planted replays.
  - `test_known_source.py`'s planted layout.
- Verification with the ROM, outside the net, after chunk 3 (counts only):
  - `golden_check.py`: 128 cases, 0 bad, 0 literal runs, 0 fragments.
  - `nintendo_bytes.py` on `tests/ladx/cases`, `tests/ladx/combos` and
    `data/ladx`: `0 literal runs`.
  - `table_replay.py`: 128 cases, 0 bad, 0 table fragments.
  - `gen_tables.sh --check`: byte-identical. It needs the network; a blocked
    host is a setup problem to report, not a pass.
  - `ladx-cases` green: the budget lines.
  - `ladx-tables` green, with the same units and entries as before (91 and
    676).
  - `git diff --stat` shows only `rom_writes.txt` files and the three table
    files changed, and `run.txt`, `spoiler.json`, `order.txt` and
    `claims.txt` untouched.
  - `make check` green.

## Out of Scope

- Any C code: the C encoder, KS and the ROM reader belong to 6.2.
- `texts` entries: they are not in KS, and their literals stay under the
  Nintendo-bytes check.
- Changes to the `nintendo_bytes.py` thresholds and to the push gate. The
  gate keeps its literal-run policy, and fragments are guarded by
  `golden_check.py` and `table_replay.py`, which need the ROM.
- `determinism.sh --full --all`: the encoder is a pure function of the event
  stream, which does not change. The stream's determinism was proven in Phase
  2.
- Raising any budget, unless the contingency above is triggered.
- Renaming the `progress.md` line.

## Further Notes

- The prototype's counts come from a scratchpad re-implementation, with no
  file in the repository changed. The final counts come from the implemented
  check and go into the commit message and the step note.
- The headroom left under the all-goldens limit is 118,162 bytes, less than one
  golden's `rom_writes.txt` on average (about 167 KB). Any new case or combo will need a
  raise first.
- 6.2's KS port must provide `entries` and `span_at` with the same
  lowest-start rule. The README's "Encoding rule" is the contract it ports.
