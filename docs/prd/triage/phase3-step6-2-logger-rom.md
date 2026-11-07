---
title: "C write logger, ROM reader, KS and the ROM-present e2e (plan 3, step 3.6.2)"
description: "No LADX C group can be ported yet: C cannot read the user's ROM, build the C-knowable source map, encode a C group's writes the way the oracle does, or project a write log, and nothing runs the C generator against the goldens with the real ROM present."
status: needs-triage
---

## Plan step

Plan `docs/plan-3.md`, Phase 3 (LADX library, host only), step 6, sub-step 2.
`progress.md` keeps the old title of this line ("C write logger and KS"),
because the hook forbids renaming it. The plan text below is authoritative.

Step 6's shared paragraph (verbatim):

> 6. **Seed-dependent writes**, in nineteen sub-steps: two that every group needs, then one per `C` group of `notes/ladx-inventory.md`'s Groups table (17), each named by its group. Each sub-step runs as its own plan step, with its own PRD and `### Step 3.6.<k>` note; step 6 itself has no PRD, and gets a one-line closing note once 6.1 to 6.19 each have theirs.
>
>    The order is not `generateRom`'s. A group comes after every `C` group it depends on, in any of three ways: it reads bytes that group wrote (the Groups table's Depends on; for `save`, which the trace cannot see reading its entry lists, every group that writes a `T` record); it overwrites bytes that group wrote, so its encoding inherits theirs (`tests/ladx/README.md`, "Encoding rule"); or it continues that group's RNG stream (`rnd`: `marin`, `hints`, `music`; the global `random`: `marin`, `endscreen`). The C pipeline runs the ported groups in LADXR's order and skips the others, so each claim is reached with the sub-steps before it. A group's claim is its projection (`tests/ladx/README.md`, "Projection and claims") of the C write log equal to the golden's on every case and combo, through step 5's e2e driver, plus the `trace` lines of `run.txt` drawn by its own functions when it draws. A group whose Evidence lists `rom`, or that 6.2 finds needs KS to encode, is claimed by a `rom-group` line, verified by `make -C tests/ladx e2e-rom` with the real ROM (summary line in the step note); every other group by a `group` line, verified by `make check`'s `e2e` and `e2e-combos`. At each sub-step both runs are green and every group claimed before it stays claimed in both. Unit tests per module run under `make test` and `make unit-arm`, on synthetic data only. A claim a Phase 2 artifact cannot reach is fixed at the oracle (§2), never cut.

Sub-step 6.2 (verbatim):

> 2. **C write logger, ROM reader, KS and the ROM-present e2e.** `ladx/src/` gains four parts: the source ROM's tables parsed from caller-owned bytes as `ROMWithTables` parses them (texts, entities, the five room tables, backgrounds, room sprite data); KS built from them as `known_source.build` does; the `C`-group encoder (carried origins, inherit-on-equal, KS-match with 6.1's rule, net effect per segment, the canonical `rom_writes.txt` text without `I` lines, projection); and `ladx_gen --rom <file>`, which refuses any SHA-1 but `d90ac17e…` and, without `--rom`, runs only the groups that read no ROM. `claims.txt` gains `rom-group <name>` lines. The driver refuses (exit 2) a `group` line for a group whose inventory Evidence lists `rom`, and `e2e` and `e2e-combos` skip `rom-group` lines. A new target, `make -C tests/ladx e2e-rom [LADX_ROM=<file>]` (outside the net; a missing or wrong ROM is a `setup:` line, exit 2), works in two parts. First, it compares C's KS span list from the ROM with `known_source.build`'s: offsets and lengths only, written under `tmp/ladx-e2e-rom/`, never committed, never a byte value printed. Then it runs the driver on every case and combo with `ladx_gen --tables data/ladx --rom <file>`, claiming every `group` and `rom-group` line. Its contract goes in `tests/ladx/README.md`, with one line in `CLAUDE.md`. This step also lists, from the goldens, every group without `rom` in its Evidence whose records hold a KS reference; such a group is claimed as `rom-group` too, recorded in the step note. Verified by:
>    - unit tests of the encoder on synthetic event streams against `write_log.py`'s encoder (a ROM-free fixture the oracle writes from synthetic streams);
>    - unit tests of the parsers and KS on a synthetic ROM the test builds;
>    - C's projection of every golden equal to `write_log.py project`'s for every group;
>    - the driver's self-test gaining planted runs: a `rom-group` skipped by `e2e`, the same run claimed in `e2e-rom` mode with a fake generator, and a `group` line on a `rom` group refused;
>    - with the real ROM, `e2e-rom`'s KS comparison equal (6105 spans, 59,971 bytes) and its run passing every claim `e2e` passes.

Governing rules (§2 and Phase 3's introduction): nothing committed holds ROM
content in any encoding (owner decision, commit 782e21a); `make check` never
reads the ROM; unit tests use synthetic tables and ROMs only, and nothing
derived from the ROM's content is committed as a test input or a reference;
a new LADX tool or target states its full contract in one place,
`tests/ladx/README.md`, with one line in `CLAUDE.md`.

## Problem Statement

The next seventeen sub-steps each port one `C` group of `generateRom` and
claim its projection of the C write log against the golden's. None of them
can start today, because four foundations are missing on the C side:

- **C cannot read the user's ROM.** Most `C` groups edit the tables
  `ROMWithTables` parses (texts, entities, rooms, backgrounds, room sprite
  data), and `save` re-packs all eleven of them. Since the owner's decision
  after step 3.5, no extraction of ROM content may be committed, so C must
  parse those tables at runtime from the user's own bytes, exactly as LADXR
  does, including LADXR's integer pointers and alt keys.
- **C cannot build KS.** The C-group encoding rule (inherit-on-equal,
  KS-match, the fragment rule of 6.1) depends on the C-knowable source map,
  built today only by the oracle's `known_source.build`. C needs the same
  map, the same `entries` and `span_at` lookups, byte for byte, or every
  KS-matched reference diverges.
- **C cannot encode a `C` group's writes.** `ladx_wlog` renders net bank
  atoms and table atoms that the caller already encoded; that is enough for
  the five table groups, whose atoms come pre-encoded from `data/ladx/`, but
  a `C` group produces values with origins (a source copy, a copy of an
  earlier computed byte, a fresh computed byte), and their encoding depends on
  the whole run's state: what each position and entry holds and how it was
  encoded, which write identities were renamed by KS-match, and the segment's
  net effect. Nor can C project a write log: the driver projects through
  `write_log.py`, and the plan asks for C's own projection, proven equal on
  every golden.
- **Nothing verifies C with the ROM present.** `make check`'s `e2e` and
  `e2e-combos` run without the ROM, so a group whose encoding needs the ROM
  (Evidence `rom`, or KS references in its goldens) cannot be claimed there,
  and nothing compares C's KS with the oracle's. The driver also accepts a
  `group` claim on a group that can only be checked with the ROM.

Measured now on the 128 committed goldens (88 cases, 40 combos), with the
ROM at `tmp/ladx/base.gbc` (counts only, no byte printed):

- `known_source.build`: 6105 spans, 59,971 bytes, 186 spans not rebuilt.
- `C` groups whose Evidence does not list `rom` but whose golden records
  hold source references: `bank3e` (128 of 128 cases: `B` copies into KS,
  and `T texts` copies outside KS, the carried copy of text 0x111's source
  span), `endscreen` (128: `B` copies into KS), `header` (128: `B` copies
  into KS), `marin` (128: `T texts` copies into KS, KS-matched), `music` (2
  of 2 cases where it writes: `B` copies into KS). `hints` holds none on
  these goldens.
- Largest net sizes per group across all goldens, which size C's buffers:
  `save` 121,397 net bank bytes in one segment; `options` 1,035 entries and
  34,094 entry bytes; at most 38 segments per log. One event stream
  (`sample-05`) writes 161,470 bank bytes and 63,623 entry bytes, with
  95,592 distinct write identities; the parsed tables hold 3,160 entries,
  134,022 bytes.
- `write_log.py projections` over every golden: 2,068 group projections,
  about 14 s.

## Solution

C gains everything a `C` group's port needs, and a ROM-present e2e verifies
it, with the existing claims unchanged:

- A **SHA-1** module and a **base check**: a ROM is accepted only when it is
  1 MiB and its SHA-1 is `d90ac17e9bf17b6c61624ad9f05447bdb5efc01a`.
- A **ROM reader** parsing the eleven `ROMWithTables` tables from
  caller-owned bytes as LADXR's `PointerTable` does: every entry's absolute
  source span or integer pointer, its bank, its pointer bytes' place, alt
  keys included.
- **KS** built from the parsed tables as `known_source.build` builds it, with
  its span list (labels as the oracle's), membership, `entries`, `span_at`
  and the maximal contiguous runs KS-match searches.
- The **C-group encoder**: events with Python's origin model in, the
  canonical write log out through `ladx_wlog`, every rule of
  `tests/ladx/README.md`'s "Encoding rule" for `C` groups; `table` groups'
  pre-encoded atoms passed through so the state stays right.
- A **write-log parser and projection** in C, proven equal to
  `write_log.py`'s on every group of every golden, as part of `e2e` and
  `e2e-combos`.
- A **pipeline** in the library that walks `order.txt`, applies the table
  entries through the encoder and dispatches `C` rows to the ported groups,
  running a group that needs the ROM only when a parsed ROM is given (no
  group is ported yet; the dispatch is proven with planted groups).
- `ladx_gen --rom <file>` (refusing any other ROM), and
  `ladx_gen --rom <file> --ks-spans <file>` to write C's KS span list.
- The driver: `rom-group` lines, two refusals of `group` lines that only the
  ROM can verify, a `--rom` mode, and a KS comparison; a new target
  `make -C tests/ladx e2e-rom` outside the net.

The owner, after this step, has one command that, with the ROM in
`tmp/ladx/`, shows C's KS equal to the oracle's and every claim passing with
the ROM read at runtime, and the next sub-step (6.3 `bank3e`) only has to
write the group itself.

## User Stories

1. As a plan-pilot agent porting a `C` group, I want C to parse the user's ROM into the eleven `ROMWithTables` tables, so that my group reads and edits the same entries LADXR edits without any ROM content committed.
2. As a plan-pilot agent, I want every parsed entry to carry its absolute source span, so that an unchanged entry's bytes are encoded as a reference to their source offset.
3. As a plan-pilot agent, I want integer-pointer entries kept as LADXR keeps them (a pointer outside 0x4000..0x7FFF, no data read), so that `T` records render `p<v>` as the goldens do.
4. As a plan-pilot agent, I want the alt keys (`Alt06`, `Alt0E`, `Alt1B`, `Alt2B`, `Alt79`, `Alt8C`, `Alt1F5`) parsed with their pointer places, so that rooms reached through alt pointers are editable and re-packable.
5. As the `save` port (6.17), I want each entry's bank and pointer-byte place recorded at parse time, so that the storage regions and the pointer rewrite can be derived from the parse without re-reading the ROM.
6. As the owner, I want the ROM reader to refuse a ROM that is not 1 MiB and to report an entry that runs past its bank as an error status, so that a malformed ROM never reads out of bounds on the device.
7. As a developer, I want SHA-1 in the LADX library tested against FIPS 180 vectors, so that the base check (and step 7's `ladx_assemble`, and Phase 4's base check) rests on a proven digest.
8. As the owner, I want `ladx_gen --rom` to refuse any file whose SHA-1 is not the LADX English 1.0 ROM's, printing only the digest, so that a wrong base is a clear refusal and never a silent divergence.
9. As a plan-pilot agent, I want KS built in C exactly as `known_source.build` builds it (the pointer bytes, bank bytes, rebuildable entries of the KS tables and keys, the boss zero bytes and item ids, in the oracle's order), so that every KS-matched reference C writes is the golden's.
10. As a plan-pilot agent, I want C's KS to expose membership, `entries` by table and key, `span_at` with the lowest-start rule (longest on a tie), and the maximal contiguous runs, so that the fragment rule and KS-match are ported against the same lookups the oracle uses.
11. As the owner, I want `make -C tests/ladx e2e-rom` to compare C's KS span list with the oracle's on the real ROM (offsets, lengths and labels, written only under `tmp/ladx-e2e-rom/`, counts printed), so that C's reading of the ROM is proven before any group depends on it.
12. As the owner, I want that comparison to print only counts and line numbers with labels, never an offset's byte, so that nothing of the ROM reaches the terminal or a log.
13. As a plan-pilot agent, I want a C encoder that takes each write with its per-byte origins (a source offset, a write identity, or none) and encodes it by the C-group rule event by event, so that my group's code only states what it writes and where each byte came from.
14. As a plan-pilot agent, I want inherit-on-equal for bank writes and for table entries (common prefix, common suffix, then the longest middle run of at least 4 knowable bytes), so that a rewritten entry keeps the encoding the golden gives it.
15. As a plan-pilot agent, I want KS-match at the end of each `C` segment (longest first, at least 4 bytes, inside one KS run, lowest offset) and then 6.1's fragment rule, with the write identities they name renamed, so that later carried copies inherit the reference exactly as in the oracle.
16. As a plan-pilot agent, I want the encoder to keep each segment's net effect and render it through `ladx_wlog`, so that the text is canonical and `equal logs are equal text` holds for C.
17. As a plan-pilot agent, I want the encoder to tell me the carried origin of the byte currently at an output position and of each byte of a table entry, so that a group copying an unchanged byte from the working ROM carries the origin LADXR's trace would give it.
18. As a plan-pilot agent, I want `table` groups' pre-encoded atoms passed through the encoder (updating the state and giving their computed bytes write identities), so that a `C` group that later overwrites or copies a table group's bytes inherits the right encoding.
19. As the owner, I want the encoder proven against `write_log.py`'s own encoder on synthetic event streams, through a fixture the oracle writes without the ROM, so that the port is checked rule by rule with no ROM content committed.
20. As the owner, I want that fixture regenerated byte-identical by its `--check`, so that it stays a faithful render of the oracle's encoder.
21. As the owner, I want the fixture to cover each rule by a named hand-written stream (carried source copy, carried identity, inherit-on-equal bank, table prefix, suffix and middle, unknowable stop, KS-match across a KS run boundary, lowest-offset tie, fragment rule on `T` and `B`, renaming after KS-match then a carried copy in a later segment, a position rewritten to its source value as a keep, an integer pointer, net effect within a segment, a table group's passthrough) plus random streams, so that both the intended cases and the unexpected interactions are compared.
22. As a plan-pilot agent, I want C to parse a version-1 write log and project one group as `write_log.py project` does, so that C's own projection exists for later checks and for debugging a single group.
23. As the owner, I want `e2e` and `e2e-combos` to compare C's projection of every golden with `write_log.py`'s for every group, so that the C projection is proven on all 2,068 real projections and stays proven.
24. As a plan-pilot agent, I want the write-log builder's capacities raised to hold `save`'s 121,397-byte segment, so that the largest group projects and later encodes without overflow.
25. As a plan-pilot agent, I want a pipeline in the library that walks `order.txt`, applies the table entries through the encoder and calls the ported `C` groups at their rows in LADXR's order, so that each sub-step only registers its group.
26. As the owner, I want `ladx_gen` without `--rom` to run only the groups that read no ROM, and with `--rom` every ported group, so that `make check` stays ROM-free and `e2e-rom` covers the rest.
27. As the owner, I want the existing five table-group claims and the fill claim to pass unchanged on every case and combo after the encoder is wired in, with and without `--rom`, so that the new plumbing changes no output.
28. As the owner, I want `claims.txt` to accept `rom-group <name>` lines (a `C` group, not `spoiler`), skipped by `e2e` and `e2e-combos` and claimed by `e2e-rom`, so that ROM-dependent groups are claimed where they can be verified.
29. As the owner, I want the driver to refuse (exit 2) a `group` line on a group whose inventory Evidence lists `rom`, so that no ROM-dependent claim is ever silently checked without the ROM.
30. As the owner, I want the driver to also refuse (exit 2) a `group` line on a `C` group whose goldens hold any source reference, naming the group and a case, so that the "needs the ROM to encode" rule is enforced from the committed goldens alone, in the net.
31. As the owner, I want `e2e-rom` in its `--rom` mode never to print a literal from a projection line (only the record's head and its token count), so that a C bug writing ROM bytes as literals cannot leak them to the terminal.
32. As the owner, I want the driver's self-test to prove a `rom-group` skipped by `e2e`, the same run divergent when claimed in `--rom` mode with a fake generator (which receives `--rom`), both `group` refusals, the KS comparator on planted span files, and the projection comparison on a planted wrong projection, so that the new paths are proven without the ROM.
33. As the owner, I want a missing ROM or a ROM of another SHA-1 to make `e2e-rom` print a `setup:` line and exit 2, so that an absent ROM is a setup problem, not a bug.
34. As the owner, I want the step note to list the groups found holding source references with their counts, and `e2e-rom`'s summary lines, so that later sub-steps know which groups are `rom-group`s before porting them.
35. As a future reader, I want the contracts of `e2e-rom`, the new `ladx_gen` flags, the driver's new modes and the encoder fixture in `tests/ladx/README.md`, and one line per tool in `CLAUDE.md`, so that each contract lives in one place.
36. As the owner, I want every new unit test to run under `make test` and `make unit-arm`, and the fixture replay under `make logic` and `make unit-arm`, so that the code is proven on the host and on the Cortex-A7 build.
37. As the owner, I want `make lint` to build every new source with `-pedantic-errors`, so that the library stays strict C99.
38. As the owner, I want the encoder fixture checked by the Nintendo-bytes check with `0 literal runs` before it is pushed, so that a synthetic fixture never coincides with ROM content.

## Implementation Decisions

### Library modules (pure C99, no allocation, `ladx/include` only)

- **SHA-1** (`ladx_sha1.h`): one-shot and incremental digest, like the
  existing SHA-512 module's interface. Placed in `ladx/`, not a `common/`
  library: `alttp/` has no SHA-1 user, so §2's "two real users" rule does not
  hold.
- **ROM reader** (`ladx_rom.h`): `ladx_rom_check(bytes, len)` returns ok,
  wrong size (not exactly 0x100000 bytes) or wrong SHA-1, with the digest
  for the caller to print. `ladx_rom_tables_parse(bytes, len, out)` fills a
  caller-owned `LadxRomTables`: for each of the eleven tables in
  `ROMWithTables`' order (the write log's `tables` line), each entry by key
  (index, then alt keys in LADXR's dict order): either a span (absolute
  start and stop) or an integer pointer, its bank, and the absolute place
  and length (2 bytes, or 1 for the sprite data tables) of its pointer, plus
  the bank byte's place for `texts`. The layout constants are LADXR's
  (`romTables.py`, the same as `known_source.LAYOUT`); they are code
  constants, not ROM content. The entry end follows each table's
  `_readData` (texts until 0xFE or 0xFF; entities until 0xFF; rooms by the
  object-size rule after the 2-byte header until 0xFE; backgrounds by
  `BackgroundTable`'s own `_readData`; sprite data a fixed 4 bytes). Error
  statuses: bad size, an entry reaching the end of its bank, an alt pointer
  outside 0x4000..0x7FFF (LADXR's assert). Storage regions (merging,
  `claim_storage_gaps`, `expand_to_end_of_bank`) are **not** computed here:
  only `save` uses them, its claim under `e2e-rom` verifies them, and every
  input they need (spans and banks) is in the parse. Decided here because
  nothing in 6.2 could verify regions with the real ROM short of adding an
  LADXR dependency to `e2e-rom`.
- **KS** (`ladx_ks.h`): `ladx_ks_build(rom, len, tables, ks)` ports
  `known_source.build` line for line: per table, pointer spans (alt first,
  then by index) and bank spans as in `_pointers`, kept only when the
  pointer bytes rebuild; then each entry of a KS key kept only when
  `fields` parses it and `rebuild` gives its bytes back (ported literally,
  not reasoned equivalent), else recorded as a failure label; then the boss
  zero bytes and item ids. Span order and labels are the oracle's
  (`texts pointer 1a`, `entities 2c4`, `boss zero 50513`, ...) so the span
  lists compare as text. `ladx_ks_add` (public) builds a synthetic KS for
  tests and the fixture. Lookups: `ladx_ks_contains(q)`,
  `ladx_ks_entry(table, key)`, `ladx_ks_span_at(q)` (lowest start, longest
  on a tie), the maximal contiguous runs, and an index of every 4-byte
  window inside a run, sorted by offset, for KS-match (built once per KS;
  candidates are extended within their run, a strictly longer match
  replaces the best, so the longest wins at its lowest offset, as
  `_KnownRuns.match`'s binary search gives). Capacities: 8,192 spans,
  131,072 KS bytes (today 6,105 and 59,971); overflow is a status.
- **Encoder** (`ladx_encoder.h`): a caller-owned `LadxEncoder`, initialised
  with the source ROM (or none), KS (or none), the parsed tables (or none)
  and the `LadxWlog` it renders into. Origins are `write_log.py`'s model,
  per byte: `q >= 0` an unchanged copy of source byte q; `-1` (KNOWN) no
  carried origin; `<= -2` the write identity `-2 - i`. A byte whose identity
  is met for the first time gets the atom the rules give and names it;
  later carries of that identity take the named atom, renamed when
  KS-match or the fragment rule turns it into a copy. Calls:
  - `C` writes: `ladx_enc_bank(group, offset, values, origins, n)`,
    `ladx_enc_table(group, table, key, values, origins, n)`,
    `ladx_enc_pointer(group, table, key, value)`; encoded event by event
    against the state before (carried origin, else inherit-on-equal, else a
    literal); at the segment's end KS-match then the fragment rule on its
    net literals, renaming the identities each changed byte names.
  - `table` writes: `ladx_enc_fixed_bank/table/pointer(group, ..., atoms,
    origins or NULL, n)`: the atoms are taken as given (C copies the tables'
    encoding); at the segment's end the state takes the net atoms, and the
    origins of the net bytes are named, as `_table_segment` with `fixed`
    does. With NULL origins (the real tables, which carry none), each net
    computed byte gets a fresh identity the encoder allocates, so a later
    carried copy can name it.
  - Queries for group code: the carried origin of the byte now at an output
    position, and of each byte of an entry's current value; a fresh-identity
    allocator.
  - `ladx_enc_end` closes the last segment and the log.
  - A segment changes when the group changes, exactly as the event stream's
    grouping; a group's side is the caller's choice of call, so the encoder
    needs no inventory.
  - Initial entries: from the parsed tables, each entry's atoms its source
    span's offsets (an integer pointer stays one); with no ROM, no initial
    entries (a previous value unknown, as `old = []`). Without a ROM, KS is
    empty and no reference is knowable, so the rules give what the oracle
    gives for a group whose bytes do not depend on the ROM.
  - State: one atom per ROM position (4 MiB for the 1 MiB ROM), the named
    atom per identity (cap 262,144; 95,592 seen), an entry atom pool (cap
    524,288 atoms), the open segment's net maps (as `ladx_wlog`'s). About
    12 MiB in all with the log: on the device the app owns it statically
    (Phase 4); recorded so step 10's measure sees it.
- **Write log** (`ladx_wlog.h`, extended): `LADX_WLOG_BANK_MAX` raised from
  32,768 to 262,144 (save's 121,397). `ladx_wlog_project(text, len, group,
  log, sink, ctx)`: parses a version-1 log (version and `tables` lines,
  `G`, `B`, `T`, `I` lines, every token rule of `write_log.parse`;
  `I` lines checked for shape and skipped) and feeds only `group`'s records,
  in order, into a fresh log: `ladx_wlog`'s same-group merge already keeps
  the net effect, so the projection is the group's records re-rendered as
  one segment, header lines included; a group absent from the log gives the
  two header lines. `ladx_wlog_groups` lists a log's groups in first-write
  order. The token decoder is shared with the tables reader through an
  internal header, not duplicated.
- **Pipeline** (`ladx_pipeline.h`): moves the walk now in `ladx_gen` into
  the library. Given the opened tables, the key, the encoder, an optional
  parsed ROM and a registry of `C` groups (`{name, needs_rom, run}`, run
  receiving the encoder, the randomizer's result and the parsed ROM), it
  walks `order.txt`: a unit's entry through the encoder's `fixed` calls at
  its first row; at a `C` row, the registered group of that row's group,
  once, at its first row, skipped when it needs the ROM and none is given
  or when no group is registered. The library's registry is empty in 6.2;
  each later sub-step adds its group with `needs_rom` true exactly when it
  is a `rom-group`. Decided as the library's job (not `ladx_gen`'s) because
  step 7's `ladx_assemble` and Phase 4 run the same walk.

### Host generator `ladx_gen`

- `--rom <file>` (with `--tables`): reads the file whole into a static
  1 MiB buffer, `ladx_rom_check`s it (a wrong size or SHA-1:
  `ladx_gen: --rom: not the LADX English 1.0 ROM (SHA-1 <hex>)`, exit 3,
  matching `rom-check`'s refused-base exit), parses the tables, builds KS
  and runs the pipeline with them. Without `--rom`, the pipeline gets no
  ROM: only groups with `needs_rom` false run. Today both give the same
  `rom_writes.txt` (no `C` group is ported).
- `--rom <file> --ks-spans <out>` writes C's KS span list and exits:
  `ladx-ks-spans 1`, then `<start> <length> <label>` per span (hex, the
  build's order), then `failure <label>` per span left out. Written only
  where told; the driver puts it under `tmp/ladx-e2e-rom/`.
- `--project <rom_writes.txt> <out_dir>` writes `<out_dir>/<group>.txt`,
  each group's projection, for every group of the log.
- Every route of the encoder is used even without `--rom`: table records go
  through `ladx_enc_fixed_*`, so the e2e run proves the passthrough.

### Oracle side (Python)

- `known_source.py` gains `spans(ks)`, the same span-list text C writes, and
  a `--spans <file>` option; unit-tested in `test_known_source.py` on its
  synthetic ROM.
- A fixture generator `gen_encoder_vectors.py` writes
  `tests/ladx/oracle/encoder_vectors.txt` (`--check` re-renders and
  compares, byte-identical; no ROM, no LADXR): a synthetic ROM of a few KiB
  drawn from a fixed-seed `random.Random` (no byte from the ROM), then
  vectors, each: a name, the sides, a synthetic KS (spans with optional
  `(table, key)` entries, or `none`), the table order, initial entries,
  events in the `ladx-events 1` line format, and the expected log text
  from `write_log.encode` with that KS object (it accepts any object with
  `mask`, `entries` and `span_at`). The hand-written vectors cover user
  story 21's list; about 200 random vectors draw small streams over the
  synthetic ROM and KS with repeats, copies, overwrites and both sides.
  The `#` header describes the format, as the other fixtures do. The
  vectors flagged `ks none` exercise the rules without KS-match or
  fragments. Unit-tested by a `test_gen_encoder_vectors.py` in
  `make test-ladx-oracle` (planted render round trip and `--check`
  mismatch). Under `tests/ladx/oracle/`, the Nintendo-bytes check's general
  scope; it is run once with the ROM and its `0 literal runs` goes in the
  step note.

### Driver `run_e2e.py`

- `claims.txt` gains `rom-group <name>`: the group must be side `C` in the
  inventory and not `spoiler`; a group both `group` and `rom-group`, a
  repeated one, or another side exits 2. No `rom-group` line is added in
  6.2: no `C` group is ported yet.
- Two refusals of a `group` line (exit 2, in every mode): its group's
  inventory Evidence lists `rom` (the Evidence column read from the Groups
  table, as `sides_of` reads Side); or it is a `C` group and a golden of the
  directory run holds a source reference (a keep or a copy) in one of its
  records, named with the first such case. The second goes beyond the
  plan's "KS reference" on purpose: a copy outside KS (bank3e's carried
  copy of text 0x111) also needs the ROM, since C learns that offset only
  by parsing the user's pointer tables; and the check needs no ROM, so it
  holds in the net, not only under `e2e-rom`.
- `e2e` and `e2e-combos` skip `rom-group` lines (the header line names them
  as skipped).
- `--rom <file>` mode: checks the file (missing or another SHA-1:
  `setup: e2e-rom: ...`, exit 2), then compares KS: runs
  `known_source.build` and writes `tmp/ladx-e2e-rom/ks-oracle.txt`, runs
  `<gen> --rom <file> --ks-spans tmp/ladx-e2e-rom/ks-c.txt`, compares the
  files line by line and prints `e2e-rom: KS equal, <n> spans, <b> bytes,
  <f> not rebuilt` or `KS DIFFERENT: line <n>: <label>` lines (labels only)
  and exits 1; then runs every case with `--tables <dir> --rom <file>`,
  claiming the stage, every `group` and every `rom-group` line. In this
  mode a divergent projection line prints only the record's head (`G`,
  `B <offset>`, `T <table> <key>`) and its token count, never its tokens.
  The directory work root `tmp/ladx-e2e-rom/` is emptied at the start.
- `--projections` mode: for each golden of the directory, runs
  `<gen> --project` and compares every group's file with
  `write_log.projections`; prints `PROJECTION <case> <group>: line <n>`
  per difference, then `projections: <n> goldens, <g> group projections
  equal to write_log.py's`; exit 1 on a difference. The tests/ladx `e2e`
  and `e2e-combos` targets run it on their directory before the claims
  (about 7 s each, C plus Python; ROM-free, so it belongs in the net).
- `--ks-groups` (with `--rom` not needed): prints, per `C` group without
  `rom` in its Evidence, the cases whose goldens hold source references
  (keeps and copies, inside and outside KS when a ROM is given, else
  total), the source of the step note's list.
- Self-test additions (counted into its total): a `rom-group` line with a
  divergent C log in that group passes under `e2e`; the same run under
  `--rom` with a planted ROM (the SHA-1 check injectable for the test only)
  and a fake generator that records receiving `--rom` diverges; a `group`
  line on a planted `rom`-evidence group exits 2; a `group` line on a `C`
  group whose planted golden holds a copy exits 2; `rom-group` on a
  `table` group exits 2; the KS comparator on equal, one changed and one
  missing planted span line; a planted wrong projection prints
  `PROJECTION`.

### Target and docs

- `make -C tests/ladx e2e-rom [LADX_ROM=<file>]` (default
  `tmp/ladx/base.gbc`, repository-relative, run from the root like
  `rom-check`; outside the net): builds `ladx_gen`, then
  `run_e2e.py --rom` on `cases`, then on `combos` (the KS comparison once,
  on the first), exit 0 only when KS is equal and every case passes.
- `tests/ladx/README.md`: a new section "ROM-present e2e" (the target, the
  KS comparison, the modes, the output rules), the "Generator `ladx_gen`"
  flags, "E2E driver and claims" (`rom-group`, refusals, `--projections`,
  `--ks-groups`, the self-test count), "Unit tests and lint" (new tests and
  modules), and an "Encoder fixture" section. `CLAUDE.md`: one line for
  `e2e-rom` (outside the net, with a pointer), the new library modules in
  the `ladx/` layout entry, the fixture and its generator in the oracle
  entry, `--projections` under `e2e`.

### Decisions taken without a guideline agent (the caller asked for none)

- `rom-group` scope widened to any source reference (above).
- Storage regions deferred to 6.17 (above).
- Projection check in the net as part of `e2e`/`e2e-combos`, not a new root
  target: it reads only committed goldens and costs seconds.
- The fixture replay runs under `make logic` (the `--fixtures` convention
  of `test_fill`), the hand-written encoder tests under `make test`.
- `ladx_gen --rom` exits 3 on a refused base, as `rom-check`'s contract.
- No `rom-group` line is added now; the step note carries the list.

## Implementation Chunks

Each chunk is one TDD cycle, committed with `make check` green.

1. **SHA-1 and the base check.** `ladx_sha1.h` and `ladx_rom_check`.
   Claim: `test_sha1` passes the FIPS 180 vectors ("abc", the 448-bit
   message, one million `a`, and an empty message) and a planted 1 MiB
   buffer is refused as wrong SHA-1, a 1 MiB minus 1 one as wrong size;
   `make test`, `make lint` green.
2. **ROM reader on a synthetic ROM.** `ladx_rom_tables_parse`, and a test
   helper that builds a well-formed 1 MiB LADX layout from a fixed PRNG
   (pointer tables, banks table, alt pointers, entries of every kind,
   integer pointers, every byte synthetic) and records where it put each
   entry. Claim: `test_rom` gets every entry's span, bank and pointer place
   equal to the builder's record, integer pointers kept, alt keys in order,
   and each error status on a planted fault (short ROM, an entry running to
   the bank's end, an alt pointer out of range).
3. **KS in C, and the oracle's span list.** `ladx_ks_build`, lookups and
   the 4-byte window index; `known_source.py --spans`. Claim: `test_ks` on
   the chunk-2 ROM with planted non-rebuildable entries (an entity list
   whose 0xFF falls on a pair's second byte, a background with a short
   value run, a pointer whose bytes do not rebuild) gets the expected span
   list text, failures, `entries`, `span_at` (overlapping spans: lowest
   start, longest on a tie) and runs; `test_known_source.py` checks the
   `--spans` text on its synthetic ROM; the same layout's expected text is
   written by both sides' tests from the builder, not from each other.
4. **Write-log parser and projection in C.** `ladx_wlog_project`,
   `ladx_wlog_groups`, the capacity raise, the shared token decoder,
   `ladx_gen --project`, `run_e2e.py --projections`, wired into the
   `e2e` and `e2e-combos` targets. Claim: `test_wlog` projects hand-written
   logs (two segments of one group around another's, a keep, a copy
   continuing a copy, a pointer, an `I` line, a group absent) to the
   expected text and refuses each malformed form; the self-test's planted
   wrong projection is reported; `make check` prints `projections: 88
   goldens, ...` and `projections: 40 goldens, ...` with no difference.
5. **Encoder fixture from the oracle.** `gen_encoder_vectors.py`,
   `encoder_vectors.txt`, its unit test, the README section. Claim:
   `--check` byte-identical, `make test-ladx-oracle` green, the fixture
   under 300 KB, and `nintendo_bytes.py` on it with the ROM:
   `0 literal runs` (step note).
6. **Encoder core.** Origins, identities and naming, inherit-on-equal for
   bank and table writes (prefix, suffix, middle), net effect per segment,
   integer pointers, the `fixed` passthrough, the queries, rendering through
   `ladx_wlog`. Claim: `test_encoder`'s hand-written cases pass under
   `make test`, and `test_encoder --fixtures` reproduces every `ks none`
   vector byte for byte (the others reported as not yet covered only in
   this chunk's run, never skipped by a committed flag).
7. **KS-match and the fragment rule.** End-of-segment KS-match through the
   window index, the fragment rule on `T` entries and `B` pieces cut by
   `span_at`, identity renaming. Claim: `test_encoder --fixtures` reproduces
   every vector of the fixture under `make logic` and `make unit-arm`;
   `test_encoder` gains hand-written KS-match ties and run-boundary cases.
8. **Pipeline and `ladx_gen --rom`.** `ladx_pipeline.h` with its group
   registry, the walk moved out of `ladx_gen`, table entries through the
   encoder, `--rom` and `--ks-spans`. Claim: `test_pipeline` on synthetic
   tables with planted groups (a ROM-free group runs with and without a
   ROM; a `needs_rom` group runs only with a parsed synthetic ROM; groups
   run at their first `C` row, in order); `ladx_gen --rom` on a planted
   file exits 3 with the digest line; `make check` green with every claim
   unchanged (88 cases, 40 combos at `fill` plus the five table groups).
9. **Driver, `e2e-rom` and the real-ROM run.** `rom-group`, both refusals,
   `--rom`, the KS comparison, `--ks-groups`, the self-test's planted runs,
   the Makefile target, README and `CLAUDE.md`. Claim: the self-test passes
   with its new count; `make check` green; with `tmp/ladx/base.gbc`,
   `make -C tests/ladx e2e-rom` prints `KS equal, 6105 spans, 59971 bytes,
   186 not rebuilt` and passes every case and combo; `--ks-groups` lists
   `bank3e`, `endscreen`, `header`, `marin`, `music` (the step note records
   the list, its counts and both summary lines).

## Testing Decisions

- A good test drives a module through its public header only and checks
  what a caller sees: parsed spans, span-list text, encoded log text,
  projected text, statuses. No test reads a struct's private fields.
- Every C test input is synthetic: the ROMs are built by the test from a
  fixed PRNG, the KS from those ROMs or from explicit spans, the event
  streams from the oracle-written synthetic fixture. Nothing derived from
  the real ROM is committed as an input or an expected value. The goldens,
  already committed and checked byte-free, are used only by the driver's
  `--projections` comparison, never as a unit-test input.
- Expected values come from an independent source: the builder's record of
  where it placed each entry (parser, KS), FIPS vectors (SHA-1),
  `write_log.py`'s encoder (encoder fixture) and `write_log.projections`
  (projection), never from the C code under test.
- Modules tested: SHA-1 (`test_sha1`), ROM reader (`test_rom`), KS
  (`test_ks`), encoder (`test_encoder`, plus `--fixtures`), write-log
  parse and projection (`test_wlog`, extended), pipeline (`test_pipeline`).
  New tests join `UNIT_TESTS`; `test_encoder` joins `LOGIC_TESTS`; the
  synthetic ROM builder joins the support objects of the host and ARM
  builds. Python: `test_known_source.py` (spans text),
  `test_gen_encoder_vectors.py`, `run_e2e.py --self-test`.
- Prior art: `test_tables` and `test_wlog` (synthetic tables and canonical
  text), `test_fill --fixtures` with `gen_fill_outcomes.py --check`
  (oracle-written fixture replayed by C), `test_known_source.py`'s
  synthetic ROM with a planted layout, `run_e2e.py --self-test`'s planted
  case trees and fake generators, `rom_check.py`'s self-test for a planted
  ROM and setup lines.
- Mutation sanity: before each chunk's commit, a few planted mutations of
  the new rules (the inherit-on-equal suffix off by one, KS-match keeping
  the first rather than lowest-offset longest match, `span_at` taking the
  highest start, the projection dropping a keep) are each shown to fail a
  test, as step 3.5 did.

## Out of Scope

- Porting any `C` group (6.3 onwards), and adding any `rom-group` line to
  `claims.txt`.
- Storage regions and `PointerTable.store` (6.17), the header checksums
  (6.18), the spoiler (6.19).
- The bank applier's wiring and `ladx_patch` (step 7), the synthetic-ROM
  regression reference and `e2e-synthetic` (step 7).
- Porting `write_log.py`'s table-group encoding rule (longest ROM match
  over the whole ROM): table groups stay precomputed in `data/ladx/`.
- `I` lines: C never writes them.
- Any change to the goldens, `data/ladx/` or the inventory.

## Further Notes

- The ROM never leaves `tmp/`: `e2e-rom` writes its span lists and C's runs
  only under `tmp/ladx-e2e-rom/` and prints counts, labels and digests. The
  link `LADX_ROM_URL` is never read by any of this step's tools.
- The groups found holding source references (bank3e, endscreen, header,
  marin, music) will be `rom-group`s when ported; `hints` holds none on
  today's goldens and keeps a `group` line unless its goldens change. The
  net refusal in the driver makes a wrong choice fail at once.
- Step 10's measure should include the encoder's KS index build and
  `ladx_rom_tables_parse`; both are linear in the ROM and KS.
- Re-encoding: step 3.6.1's note observed that re-encoding the recorded
  event streams would be faster than a full regeneration. Not in scope
  here, but the fixture generator's reuse of `write_log.encode` on a
  synthetic KS makes such a tool simple later.
