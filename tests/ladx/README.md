# LADX cases and goldens

The evidence Phase 3's C port of LADXR (pinned at `4325207e`) is checked
against, without the ROM. This file is the reference the C write logger
follows: a C run that logs its writes by these rules gives the golden's
exact text for every group it ports.

## Case folders

`cases/` holds the baseline and every non-default tier-1 value alone;
`combos/` the crosses, hand-picked combinations and random sample of
`oracle/combos.txt`. One folder per case, named from its settings
(`<key>-<value>`, joined with `+` in LADXR's order; `baseline`), except a
sample combo, `sample-<nn>` (below, "Random sample"). `oracle/ladx_cases.py check` (`make
ladx-cases`, run by `make check`) proves the list and the files below.

| File | Written by | Holds |
|---|---|---|
| `config.txt` | hand, step 2.4 | one `key=value` line per non-default setting, in LADXR's order |
| `seed` | `ladx_cases.py pick` | 32 lowercase hex digits, `derive(name, k)` |
| `run.txt` | `gen_golden.sh` | the oracle's record (below) |
| `spoiler.json` | `gen_golden.sh` | LADXR's spoiler; absent exactly when `race` is on |
| `rom_writes.txt` | `gen_golden.sh` | the write log (below) |

`run.txt` is `oracle_run.py --rom`'s output: the world setup (`ws` lines),
the pools, the placement taken right after `item_placer.run` from the logic
objects (`place` lines, `race` cases included), the RNG trace by stage
(`trace` lines, covering `generateRom` and the spoiler), then `rom_sha1`
(the output ROM), `spoiler_sha1` and `outcome ok`.

Every other output of a run (`out.gbc`, the ROM itself; `meta.txt`, which
differs between runs; the event stream `events.txt`; `invariant.txt`;
the read trace `reads.txt` and `writes.txt`) stays in the `tmp/` mirror,
default `tmp/ladx-goldens/<case>/`.

## `rom_writes.txt`, version 1

Every byte LADXR writes into the ROM after the fill, in its write order,
each tagged by its inventory group (`notes/ladx-inventory.md`), every byte
that comes from the source ROM a reference, never its value. Line-based
ASCII, lowercase hex without `0x`, single spaces, a newline after every
line.

```
ladx-rom-writes 1
tables <table>...
G <group>
B <offset> <tokens>
T <table> <key> <tokens>
...
I <line> <start>+<n>...
```

- `ladx-rom-writes 1`: the version line.
- `tables`: `ROMWithTables`' tables in their order (`texts entities
  rooms_overworld_top ... background_attributes`); `T` records sort by it.
- `G <group>`: opens a **segment**, a maximal run of consecutive writes by
  one group. Two adjacent segments never name the same group; a segment
  holds at least one record. Segments follow LADXR's write order.
- `B <offset> <tokens>`: a maximal run of the segment's net bank bytes at
  that absolute ROM offset. `B` records come first in their segment, by
  offset, never touching.
- `T <table> <key> <tokens>`: a `ROMWithTables` entry's net new value, the
  key an index in hex or an alt key (a name such as `Alt1F5`), records in
  table order, then index, then alt key. An integer-pointer entry is the
  single token `p<v>`.
- `I <line> <start>+<n>...`: after every segment, sorted by line: the `B`
  or `T` record on file line `<line>` (1-based, hex) holds invariant
  literal bytes at those payload indices (below).

Tokens of a payload:

| Token | Meaning |
|---|---|
| `=<n>` | keep: n source bytes at the record's own offsets (`B` records only) |
| `@<q>+<n>` | copy: n source bytes from absolute source offset q |
| `0a1b...` | literal bytes, an even number of hex digits |
| `p<v>` | an integer pointer, alone in its `T` record |

Lengths and offsets are hex without leading zeros. References that continue
each other are merged (`@10+4 @14+2` is `@10+6`; a copy landing on its own
offset is a keep), adjacent literals are joined, and the parser
(`write_log.parse`) refuses anything else. So **equal logs are equal
text**: a claim compares strings.

Within a segment the writes collapse to their **net effect**: the last
value of every bank position and table entry the segment wrote. A position
written back to its source value is still a record (as a keep).

## Write identities and carried origins

The oracle's event stream gives each written byte an **origin**:

- `src(q)`: an unchanged copy of source byte q, followed through bank
  slices and indices, parsed table entries, slicing, `bytes()`,
  concatenation, assignment to a bank or a table, and `PointerTable.store`'s
  moves and suffix de-duplication;
- a **write identity** `v<i>`: an unchanged copy of the computed byte that
  write `i` made;
- `known`: anything else (arithmetic, re-encoding).

A **carried origin** is the reference q for `src(q)`, or, for `v<i>`, the
encoding that byte got when it was first written. A leak in origin
tracking can only turn a reference into a literal, never the reverse; the
0-literal-run check catches it.

## Encoding rule

The rule depends on the group's side in the inventory's Groups table.

**Table groups** (side `table`: precomputed off-device, C copies their
encoding from step 6's tables, built with this same encoder). Once the
segment ends, per net byte:

1. keep, when it equals the source byte at its own offset (`B` records);
2. else its carried origin;
3. else the longest source match of at least 4 bytes among the bytes left,
   at its lowest offset;
4. else a literal.

No aligned 4-byte literal of a table group occurs in the ROM.

**C groups** (side `C`: what Phase 3 computes on the device), event by
event against the state before:

1. a carried origin;
2. else **inherit-on-equal**: a byte equal to the byte it replaces inherits
   that byte's encoding when C can know it (a literal, or a reference into
   KS below). The replaced byte is the same offset's for a bank write; for a
   table entry, the new value is aligned to the previous one by common
   prefix, then common suffix, each stopping at a byte C cannot know;
   between them, left to right, the longest run of at least 4 bytes without
   a carried origin equal to knowable bytes of the previous value, at its
   lowest index there, inherits their encoding;
3. else a literal.

Then, once the segment ends, **KS-match**: each maximal run of the
segment's net literal bytes (bank bytes at contiguous offsets; each table
entry alone) is matched left to right, longest first, against KS taken as
maximal runs of contiguous source offsets. A match of at least 4 bytes
inside one KS run becomes a copy of its lowest offset. No other ROM
matching ever applies to a C group.

**KS**, the C-knowable source map (`oracle/known_source.py`), is the set of
source offsets whose values step 7's extractions let C rebuild:

- every room's entity entry and object entry, alt rooms included
  (`room-entities`, `room-objects`);
- background entries 3, 4 and 0x0D of the tiles, 3 and 4 of the
  attributes (`backgrounds`);
- every room sprite data entry, and the pointer and bank bytes of every
  table `save` re-serializes (`save-layout`);
- the zero bytes of bank 0x14 0x04E0..0x051F, and the item ids at bank 0x3E
  0x3300 plus each boss room (`boss-sources`).

A span is in KS only when its extracted fields rebuild it exactly
(`known_source.py --rom` prints `NOT-REBUILT` for any other). **Step 7's
fixtures must let C rebuild every KS byte.**

## Reproducible without the ROM

A C group's tokens depend only on what C can know: its own computed
values, carried origins (offsets, never values) and KS bytes. The
scrambled-ROM test (`write_log.py scrambled`, `golden_check.py`'s
`scrambled` check) proves it: every ROM byte outside KS is scrambled, the
stream re-encoded with the table groups' encoding kept from the real ROM,
and every C group's projection must be unchanged.

## `I` lines

`I` lines are evidence for the Nintendo-bytes check only: decode and
projections ignore them, and **C never writes them** (a claim compares the
log without its `I` lines). A range is a maximal run of at least 4 literal
bytes of a C group's record, each the net byte of a write that
`oracle/invariant_run.py` flagged unchanged, holding a 4-byte window of the
source ROM. The flag comes from re-running that C-group call of
`generateRom` on a scrambled copy of the ROM state (bytes kept:
never-written KS bytes and bytes earlier writes computed; RNG state put
back): the byte came out the same, so it does not depend on ROM content.

## Projection and claims

A group's **projection** replays its segments in order into its own net
bank map and net table map, other groups ignored, and re-encodes them as
one canonical segment (`write_log.py project <log> <group>`). It needs no
ROM: it works on tokens. Phase 3 claims a group by comparing its
projection of the C log with the golden's, so neither other groups'
records nor how they split the group into segments can change a verdict
(`golden_check.py`'s `projection` check proves it on every golden).

## Decode

Replaying every segment's `B` records in order onto the source ROM gives
the output ROM: its SHA-1 is `run.txt`'s `rom_sha1`
(`write_log.py decode --rom tmp/ladx/base.gbc <log>`). `T` records are
evidence only: they reach the ROM through `save`'s `B` records.

## Patch tables

`data/ladx/` holds LADXR's settings-only writes, the inventory's `table`
groups (`notes/ladx-inventory.md`), as data C applies without the ROM or
LADXR's assembler. They are ROM-derived like the goldens: written only by
`oracle/gen_tables.sh` (with the ROM and LADXR at the pin), never edited
by hand. The format's module is `oracle/tables.py` (`parse`, `render`,
`select`, `check`); `tables.py check` (`make ladx-tables`, in `make test`)
checks the files against the inventory without the ROM.

**Files.** Line-based ASCII, single spaces, one version line first.

- `order.txt` (`ladx-tables-order 1`): the inventory's sections
  (`section <file> <function>`), then every writing row in LADXR's
  statement order: `table <group> <unit> <call>` or `C <group> - <call>`,
  `<call>` the row's inventory key (its `Call` text, ` #<k>` included).
- `<group>.txt` (`ladx-tables 1`), one per `table` group: `group <group>`,
  `tables <table>...` (ROMWithTables' tables in their order, as in a write
  log), then its units.

**Units.** `unit <group>.<n> <component>...`, numbered from 1 in their
first row's order. A unit is one `table` row, fused with the rows it is
coupled with in some coverage run (a source match of the encoding spanning
bytes both wrote last), transitively, with the group's rows between them;
`oracle/table_units.txt` forces a fusion. Fused rows are consecutive among
the table rows. The units with no component are the fixed layer, applied
whatever the settings.

**Keys.** A component is a setting key, `asm:<CONSTANT>` (an assembler
constant, by its value `0` or `1`, computed from the validated settings as
`generateRom` does, e.g. `asm:ITEM_DIALOGS`) or `count` (`frame-counter`
only: the number of non-key spots `addFrameCounter` receives, from the
item list). A unit's components are the union of its rows' inventory
`Settings` (else its group's `Settings key`), the settings and constants
its rows read at run time (`oracle/key_trace.py`), the components of the
units whose writes it reads or carries, and `oracle/table_keys.txt`'s
widenings. Components are ordered: settings in LADXR's order, constants by
name, then `count`.

**Entries.** One per reachable key tuple (the tier-1 product through
LADXR's `Settings.validate`), in increasing order of their values:

```
E <value>...        one value per component, `""` the empty string
B ... / T ...       the unit's net writes, write-log records (step 5's
                    tokens: `=n`, `@q+n`, hex literals, `p<v>`)
nothing             the entry writes nothing
```

`nothing` is explicit: a reachable tuple whose condition is false still
has its entry, so a missing entry is always a bug, never "skip". An
unreachable tuple has none: C validates the settings first and never looks
one up. The records are cut out of the coverage runs' encodings by
`write_log.py`'s own encoder, so a unit's tokens are the golden's.

**Order walk.** Start from the source ROM and walk `order.txt`: at a
`table` row that starts a unit (its first row), apply the unit's entry for
the key; at a later row of the same unit, nothing; at a `C` row, apply what
C writes there. The result is LADXR's output ROM.
`python3 -I oracle/table_replay.py [<case>...]` proves it on every case
folder, the `C` writes taken from the golden and placed by the mirror's
event stream: the SHA-1 equals `run.txt`'s `rom_sha1`, and each table
group's projection equals the golden's.

**Random sample.** The tables are built from coverage runs only; the case
goldens and a random sample are independent evidence. The sample is
`combos.txt`'s 12 `sample <nn> k=v ...` lines, written once by
`oracle/gen_tables.sh --sample` and never re-drawn: line `i` (1 to 12)
takes, for every setting in LADXR's order, its tier-1 value at index
SHA-256(`ladx-sample <i> <key>`), read big-endian, mod its number of
values (a setting outside `tier1.txt` stays at its default); the draw is
validated by LADXR and its non-default values written. A draw that leaves
`boomerang=default` with `accessibility=all` is drawn again with ` #<j>`
appended to every hashed text, the first `j` that is not. Each line is the
combo `combos/sample-<nn>`, seeded by `ladx_cases.py pick` and given its
goldens by `gen_golden.sh` like any case. A sample mismatch in
`table_replay.py` means a key misses a dependency: widen it in
`table_keys.txt` and regenerate the tables, never edit them.

**Regenerate.** `oracle/gen_tables.sh` rebuilds `data/ladx/` (coverage
runs under `tmp/ladx-tables/`); `gen_tables.sh --check` rebuilds them with
another hash seed on a fresh clone and compares every file byte for byte.

## Extraction fixtures

`extract/` holds the source ROM's structure that the inventory's
Extractions table names (`notes/ladx-inventory.md`), so that C rebuilds
the known-source map KS (`oracle/known_source.py`) and makes the C groups'
decisions without the ROM. There is one file per Extractions row, named
`<name>.txt`, and nothing else. A README there would put English prose
under the strict scope. The files are ROM-derived: written only by the
oracle, never edited by hand. The format's module is `oracle/extract.py`
(`parse`, `render`, `read`, `write`, `ks_spans`, `ks_rebuild`, `check`).
`extract.py check` (`make ladx-extract`) checks them against the inventory
without the ROM.

**Characters.** Only `0-9`, `=`, `r` and the newline. Every line starts
with `=`. Tokens are separated by `=` and the numbers inside a token by
`r`. Each number is 0..255 in decimal without a leading zero. A larger
quantity (an offset in a bank, a length, a room id, a pointer) is split
into two numbers, high first. Line 1 is the version line `=1`. A file is
canonical: no empty line or token, a newline at the end, and parse then
render is the identity. This rendering was chosen by measurement. With a
space between tokens, about 271 raw 4-byte runs of the ROM were left
whatever the inner separator. `=` between tokens, `r` inside them and a
leading `=` left none under the strict scope, with nothing cleared.

**Common tokens.** A *table* is numbered in `ROMWithTables`' order
(`known_source.LAYOUT`): 0 texts, 1 entities, 2 to 6 the five room tables
(overworld top, overworld bottom, indoor A, indoor B, color dungeon), 7 and
8 the overworld and indoor room sprite data, 9 the background tiles, 10
the background attributes. A *key* is `0 r hi r lo` for an index or
`1 r k` for the k-th alt pointer of the table, in `LAYOUT`'s order (table
2: Alt06, Alt0E, Alt1B, Alt2B, Alt79; table 3: Alt8C; table 4: Alt1F5). A
*span* is `known r bank r start-hi r start-lo r length-hi r length-lo`,
the start an offset in the bank; `known` is 1 when the entry's fields
rebuild its bytes exactly (`known_source.fields` and `rebuild`), then they
follow, and 0 when they do not, then nothing follows.

**`room-entities.txt`, `room-objects.txt`, `backgrounds.txt`.** One line
per entry: `=<table> r <key>`, then the span, then the fields when known.
An entry whose pointer is an integer, not a span, has the key token alone.
The entries are those KS rebuilds, in store order (alt keys first, then
indices): every entity entry 0x000..0x315 of table 1; every key of tables 2
to 6 (789 rooms and the seven alt rooms); entries 3, 4 and 0x0D of table 9
and 3, 4 of table 10. Fields, one token each:
- an entity: `x r y r type`;
- a room: `animation r floor` first, then per object `0 r x r y r type`
  (single), `1 r` or `2 r` then the four numbers `known_source.fields`
  gives for a horizontal or vertical strip, or `3 r` then the five numbers
  of a warp;
- a background: per run `address-hi r address-lo r vertical r repeat r
  amount r value...` (one value when `repeat`, else `amount + 1`).

**`boss-sources.txt`.** Line 2: `=bank r start-hi r start-lo r count`
(bank 0x14, 0x04E0, 64), then each offset of that range holding 0x00 as
`hi r lo`, increasing. Line 3: `=bank r base-hi r base-lo` (bank 0x3E,
0x3300), then per boss room, in `known_source.BOSSES` order, `room-hi r
room-lo r item`, the item id at the base plus the room.

**`source-flags.txt`.** Line 2: three tokens, each 0 or 1: the keyhole
hook not installed (bank 0x02 0x2FA9 is 0xF0), the witch off (bank 0x05
0x08EF is not 0x00), the boomerang trade not patched (bank 0x19 0x06C5 is
0x00).

**`save-layout.txt`.** One section per table, in order. A section opens
with its marker line `=<table>`, then its storage regions after parsing,
one token each `bank r start-hi r start-lo r end-hi r end-lo` (end at most
0x4000), then one line per key in store order. An entry line is the key,
then its pointer: `1 r flags r bank r start-hi r start-lo r length-hi r
length-lo` for a span or `0 r flags r bank r raw-hi r raw-lo` for an
integer pointer. `flags` bit 0: the pointer bytes are KS; bit 1 (table 0
only): the bank byte is KS. An alt entry ends there. An index entry with a
span then has its de-duplication, `0`, or `1 r index-hi r index-lo r
offset-hi r offset-lo` when its bytes equal the suffix at that offset of
an earlier index entry in the same bank (a whole entry, offset 0, for the
sprite data), in store order on the unchanged state: the entry
`PointerTable.store`'s map gives, the last earlier entry stored whole
(not itself de-duplicated) that holds those bytes. Then, by table kind:
the sprite data's four values as one token; for texts and backgrounds one
fingerprint per suffix, the suffix at offset `i` the `i`-th token, each the
first 8 bytes of SHA-512 of the suffix's bytes as eight numbers.

**KS rebuild.** `extract.py`'s `ks_spans` gives KS from the six files
alone, in `known_source.build`'s order and with its labels: the pointer
bytes (and the text bank bytes) whose flags say so, recomputed from the
spans or integer pointers; each known entry's bytes from its fields (the
sprite data from its values); the boss zeros and items. `ks_rebuild` gives
`{offset: byte}`.

**Check.** `python3 -I oracle/extract.py check [--root <dir>]
[--inventory <file>]`, stdlib only, proves itself first on planted trees
(`extract: self-test, <n> planted trees as expected`, else `extract:
self-test failed: <case>`, exit 1), then prints one `MISSING <name>` (an
Extractions name with no file), `EXTRA <file>` (a file no name gives) or
`BADFILE <name>: <reason>` line (not canonical, a structural rule broken
against `LAYOUT` and `BOSSES`: every expected key in order, fields that
rebuild their span, sprite slots, one fingerprint per suffix, a
de-duplication naming an earlier span; or a room file's span unlike
save-layout's) per finding, exit 1; else `extract: <n> fixtures, <b>
bytes, every inventory extraction present`. A malformed inventory exits 2.

**Generate and verify.** With the ROM (`tmp/ladx/base.gbc`) and LADXR at
the pin (`oracle/setup.sh`), else a `setup:` line (exit 2):

- `oracle/gen_extract.sh [--out <dir>]` writes the six files into
  `extract/` (or a directory inside `tmp/`; any other is refused, exit 2)
  through `extract.py generate`: the ROM parsed by `known_source.parse`
  and by LADXR's `ROMWithTables` (the storage regions are its
  `PointerTable`s'), cross-checked (`known_source.check_ladxr`, every
  entry's bytes, `RoomEditor`'s entities and objects, `BackgroundEditor`'s
  tile maps), rendered in memory and checked with the Nintendo-bytes
  check's `check_blob` under the strict scope; any mismatch or literal
  run writes nothing (exit 1). About 10 s.
- `oracle/gen_extract.sh --check` renders twice under `tmp/ladx-extract/`
  (A: the setup checkout, `PYTHONHASHSEED=0`; B: a fresh clone in
  `tmp/ladx-oracle/ladxr-extract-b`, `=1`) and prints one `DIFFERENT`,
  `MISSING` or `EXTRA` line per file unlike the other run or `extract/`,
  else `extract: 6 files byte-identical on two runs and to
  tests/ladx/extract`. About 25 s and the network.
- `python3 -I oracle/extract.py verify [--root <dir>]` prints `ok <check>`
  or `BAD <check>: <detail>` (labels and offsets, never a byte value) for
  `parse` (the check above), `ks` (the map rebuilt from the six files
  equal to `known_source.build(rom)`, span by span and byte by byte),
  `flags`, `bosses`, `backgrounds` (each tile map equal to
  `BackgroundEditor`'s), `fingerprints` (each the hash of its ROM
  suffix, no two suffixes sharing one) and `store` (below), then
  `extract-verify: 6 files, <k>
  KS spans, <b> KS bytes rebuilt, <n> bad` (exit 1 on any bad). About
  1 s.

**Store model.** `extract.py`'s `store_table` is LADXR's
`PointerTable.store` without the ROM: the alt pointers first, then every
entry in index order into the first region of its bank with room (regions
in `save-layout`'s order, LADXR's sorted by start), an entry whose first
suffix is already registered in its bank pointing there; every suffix of a
stored entry is registered (the last stored wins), only the whole entry for
sprite data; an integer pointer is written back with `| 0x4000`, a sprite
data pointer as its index from `data_addr`. Entries are compared by the
fingerprints of their suffixes: `save-layout`'s for texts and backgrounds,
those of the bytes the room files' fields rebuild, the sprite data values,
and for an entry the room files do not rebuild (entities 0x316..0x31F)
those of the entry its de-duplication names. `store_model` runs the eleven
tables in `ROMWithTables.save`'s order: the overworld sprite data's
leftover is added to the indoor regions (`addStorage`'s merge), its first
region's start becomes the indoor `data_addr` and the `ld hl` at 0x0DFA
(`0x21`, then that start `| 0x4000`, little-endian); an optional
`{(table, key): bytes or integer pointer}` replaces changed entries.
`verify`'s `store` check compares its placements and written bytes with
LADXR's own `save` on a fresh parse (the file write skipped).

The files are committed only as `gen_extract.sh`'s output, in a commit of
their own, never edited by hand.

## Nintendo-bytes policy

`oracle/nintendo_bytes.py` checks a case folder's `rom_writes.txt` in
scope `writes` (it must parse; literal tokens checked at 4 bytes at their
byte alignment; the rest of the text at the general thresholds), its
`run.txt` and `spoiler.json` in scope `general`, and `config.txt` and
`seed` strictly. Clearances: `trivial`, `low-info` (general only),
`ladxr-public`, `computed` (a literal run found nowhere whole in the source
bytes its group reads, from the inventory's `Reads (trace)` ranges) and
`invariant` (every byte of the run in its record's `I` ranges, and not KS
bytes). Every golden must give `0 literal runs`.

The patch tables under `data/ladx/` (format in `oracle/tables.py`) are in
scope `tables`: every file must parse with `tables.py`, the literal tokens
of their `B` and `T` records are checked at 4 bytes at their byte alignment
with neither the `computed` nor the `invariant` clearance, and the rest of
the text at the general thresholds.

## Budget

`ladx_cases.py check` fails (`BUDGET`) over: `rom_writes.txt` 224 KiB per
case, 22 MiB in all; all golden files (`run.txt`, `spoiler.json`,
`rom_writes.txt`) 34 MiB in all.

## Unit tests and lint

The C library `ladx/` (public headers `ladx/include/`, sources
`ladx/src/`; pure C99, no allocation, no platform library, nothing of
`alttp/` or `app/`) is tested by `tests/ladx/unit/test_<module>.c`, one
binary per module, listed in `tests/ladx/Makefile`'s `UNIT_TESTS`
(`test_sha512`, `test_pyrandom`, `test_rng`, `test_run_text`,
`test_settings`, `test_requirements`, `test_logic`; `LOGIC_TESTS`, run again with `--fixtures`: `test_logic`),
failing through `unit/support.h`'s `test_fail` (`<test>: FAIL
<message>`, exit 1). Every LADX build, host, lint and ARM, uses `-std=c99 -Wall -Wextra -Werror -ffp-contract=off`:
GCC fuses `a * b + c` into an FMA in its GNU modes and Onion's VFPv4 has
one, while CPython never fuses, so `uniform`'s `a + (b - a) * random()`
would differ in its last bit. Phase 4, which builds `ladx/` into the app,
keeps the flag. Library objects see only `-I ladx/include`; the host
layer `ladx/host/` (C99, stdio allowed, never built into the device's
library) sees `-I ladx/include -I ladx/host`; the tests add
`-I tests/ladx/unit`, and those linking the host layer `-I ladx/host` and
`-lm`. Build output goes to `tests/ladx/build/`.

- `make -C tests/ladx test` (root `make test-ladx`, run by `make test`):
  builds `build/libladx.a` and the unit tests with host `cc`, runs each
  from `tests/ladx` (fixture paths are relative: `test_pyrandom` reads
  `oracle/pyrandom_vectors.txt`, always, with no flag; `test_settings`
  reads `oracle/settings.txt`, `oracle/settings_validate.txt`,
  `oracle/tier1.txt`, every `cases/*/config.txt` and `combos/*/config.txt`
  and `../../notes/plan3-scope-cuts.md`), then prints
  `test: <n> LADX unit tests ran`. `test_sha512` prints `test_sha512:
  FIPS 180-4 vectors, padding boundaries and streaming splits ok`;
  `test_pyrandom` replays every fixture line and fails on an unknown kind
  or op, a malformed line, or a missing kind or op, then prints
  `test_pyrandom: <n> fixture lines ok (<kind> <count>, ...)`. Its
  hand-written `uniform(-7.3, 12.9)` vectors, from CPython, are the FMA
  proof: the fixture's `uniform` lines have `a = 0`, where a fused and an
  unfused result round alike, but 4 of these 8 draws differ when fused.
  `test_rng` checks `ladx_rng` (the traced generator: `ladx_pyrandom`
  wrapped with its name, `rnd` or `global`, and a sink receiving one
  record per call, as `ladx_oracle.TracingRandom` traces the outermost
  call) through its header: the stage, method and generator names
  (LADXR's `<module>:<qualname>`, as the goldens print them); `seed`,
  `random`, `choice`, `choices` (integer weights), `shuffle` and
  `randint` each leave the state `ladx_pyrandom` leaves and return what
  it draws, with one record holding the arguments, the result and the MT
  words drawn (the `words` counter's difference; `seed` 0 and the counter
  reset); `shuffle` records the permutation `ladx_pyrandom_shuffle` makes
  of `0..n-1` and applies it as `x[to] = before[perm[to]]` (4-byte and
  7-byte elements); the golden `baseline`'s `choices 12 ... 1 -> 1 [2]`
  and `goal-specific`'s `shuffle 8 -> 0,6,5,3,4,1,2,7 [11]` drawn from
  their seeds; `copy_state` (`random.setstate(rnd.getstate())`) a
  `getstate` record of `rnd` and a `setstate` record of `global`, both
  `[0]`, the global continuing `rnd`'s stream; an empty `choice` or
  `choices` (`IndexError`), an empty `randint` range (`ValueError`) and a
  list over `LADX_RNG_MAX_LIST` (256, overflow) draw nothing and record
  nothing. It prints `test_rng: seed, random, choice, choices, shuffle,
  randint and the state copy traced as ladx_pyrandom draws (<c>
  checks)`.
  `test_run_text` checks the host's `ladx_run_text` (`run.txt`'s line
  renderer, `oracle_run.py`'s format) through its header: `float.hex`
  equal to CPython's strings (0.5, ±0.0, the smallest subnormal,
  `1 - 2^-53`, `DBL_MIN`, `DBL_MAX`, 0.1, inf, nan); hand-built records
  rendered byte-identical to the real goldens' lines, read from the case
  folders (`baseline`'s `seed`, `settings` and trace lines 0 to 3, 16 and
  17; `race-1`'s `random - -> 0x1.1d12acb9114b1p-1 [2]`;
  `goal-specific`'s `shuffle`; a keysanity combo's `shuffle 0 -> - [0]`);
  `settings ""` for an empty short string, `seed -` for an empty seed,
  negative `randint` bounds, a `choices` with `k` 3 and 0; a record of
  256 weights and picks fitting `LADX_RUN_TEXT_MAX_LINE`, a line over its
  buffer or an unknown method rendering nothing; and the trace sink
  numbering lines from 0 across both generators. It prints
  `test_run_text: float.hex as CPython's, <g> golden lines rendered
  byte-identical, sink numbering ok (<c> checks)`.
  `test_settings` checks `ladx_settings` (LADXR's `settings.py` at the
  pin, less `seed` and `forwardfactor`) through its header only: the
  table equal to `settings.txt` in order, field by field (key, category,
  short key, type, default, aesthetic, `visible_if`, each option's value
  and short; a boolean's values `0` and `1`); the `key=value` parser's
  statuses (unknown key, unknown value, malformed; a refused assignment
  changes nothing); every `validate` line of `settings_validate.txt`
  replayed through the parser on fresh defaults, the settings validate
  changed and the short string after it compared; the unsupported values
  and the pair equal both ways to plan 3's section 2 list written out in
  the test, every `tier1.txt` value supported alone and every other value
  unsupported but the default of a setting `tier1.txt` leaves out; the
  `goal-bingo` reason id a heading of `notes/plan3-scope-cuts.md`; the
  whole-set check (defaults supported, `boomerang=default` with
  `accessibility=all` refused as `ladxr-refuses`, supported with
  `accessibility=goal`); and every case and combo `config.txt` parsed,
  unchanged by validate and supported. It prints `test_settings: table
  equal to oracle/settings.txt (<n> settings, <o> options, seed and
  forwardfactor left out)`, `test_settings: <n> validate lines replayed
  from oracle/settings_validate.txt (<r> rewritten), short strings
  equal`, `test_settings: <v> values and <p> pair unsupported, as plan 3
  section 2 lists`, `test_settings: <t> tier-1 values supported alone,
  every other non-default value unsupported` and `test_settings: <c>
  cases and <m> combos parse, survive validate and are supported`.
  `test_requirements`, synthetic only (no fixture), checks `ladx_items`
  (LADXR's item names: `locations/items.py`'s 125 in order, then the 37
  names only `logic/` uses, in first-use order, then the 5 `n = 9` names
  `ItemInfo.configure`'s `range(10)` appends, `KEY9` to `COMPASS9`;
  lookup by name, no `_USED` name) and `ladx_requirements` (LADXR's `logic/requirements.py`)
  through their headers: `AND`/`OR` folding as `__new__` (a true or false
  term, all true or all false, no term, `None` terms dropped, `AND(None)`
  an empty node that passes, an `OR` left empty an assertion that breaks
  the arena), the stored order (items, then children, duplicates kept),
  `add` (by name or by identity) and `remove` (first occurrence) seen
  through every reference to a shared node, the inventory (present after
  a zero-count add, `RUPEES_<n>` adding `n × count` to `RUPEES`), `COUNT`,
  `COUNTS` and `FOUND` thresholds, `getItems` (nothing for a satisfied
  node, a bare item always) and the node and term capacities (the first
  overflow breaks the arena, which then builds nothing). Each expectation
  cites its LADXR line; requirements are compared as `ladx_oracle`'s
  `requirement_text`. It prints `test_requirements: <n> item names;
  folding, stored order, add and remove on shared nodes, inventory, test
  and getItems ok (<c> checks)`.
  `test_logic` (linked with the fixture support `unit/logic_fixture.c`,
  the canonical text of `ladx_oracle.graph_lines`, `requirement_text` and
  `index_ranges`, and the readers of `logic_vectors.txt` and
  `logic_graphs.txt`) checks `ladx_logic` (LADXR's `logic/location.py`,
  `Logic`'s discovery, the `ItemInfo` classes of `locations/`,
  `checkMetadata.py` and `explorer.py`) through `ladx_logic.h` and
  `ladx_logic_builder.h` (the
  builder, public so that tests build synthetic graphs): `singleConnect`
  (a second edge to a target is `OR(new, old)`, `None` on either side
  wins, `False` adds nothing, `True` is `None`), `connect` (both ways, the
  back requirement the same node, `back=False` forward only), discovery
  (preorder in connection order from the start, unreached locations
  dropped, `iteminfo_list` by location then add order, edges renumbered),
  the canonical text of a hand-built graph equal to a literal, option
  appends, the explorer (a placed item picked up on visit, `_process` to
  a fixpoint, `RUPEES_<n>` folding, a zero-count item present, the
  required items of the todo list, a second visit refused), the
  capacities (the first overflow breaks the logic, which then builds and
  shows nothing), `LadxWorldSetup`'s defaults (`WorldSetup()`), the
  constructors of `locations/` (`ladx_logic_add_item_info`: each class's
  `nameId`, `KeyLocation`'s key as its name id with the `"None"` metadata
  its `__init__` reads, the data kept, the arguments a class does not
  take refused) and `configure` (`ladx_logic_configure`, once, on a
  finished logic), synthetic and citing LADXR's lines: `ItemInfo`'s
  `range(10)` appends and dungeon items under default, `keysanity` and
  `removed` dungeon items, and each override (`HeartPiece`,
  `FishingMinigame`, `Seashell`, `SeashellMansionBonus`,
  `HeartContainer`, `Witch`, `Toadstool`, `Instrument`, `KeyHole`,
  `TradeSequenceItem`, `BoomerangGuy`, `StartItem`, `ShopItem`,
  `OwlStatue`, `KeyLocation`) under the settings it reads, LADXR's errors
  and the unported shopsanity branch breaking the logic, and
  `ladx_logic_build_dungeon` (`RequirementsSettings`, then one `DungeonN`
  of `logic/dungeon<n>.py`, `DungeonColor` for `n` 0, alone, its entrance
  the start, discovery and configure): the world setup read
  (`miniboss_mapping[0]` and `boss_mapping[0]` on D1's edges,
  `miniboss_mapping["c1"]`, `["c2"]` and `boss_mapping[8]` on D0's), the
  `forced_item` `KEY3` on D3's south and west key drops and `KEY7` on
  D7's entrance key drop when `dungeon_keys` is `""` and none under
  `keysanity`, the locations outside the dungeon (D6's raft island and
  its `Chest` `0x06C`, D8's Tal Tal connector and its `HeartPiece`
  `0x000`), the pair LADXR's fill refuses built, an unsupported value
  refused (`LADX_LOGIC_UNSUPPORTED`, nothing built), an out-of-range
  dungeon and a boss mapping out of range broken, and `ladx_logic_build`
  (`Logic` of `logic/main.py`: `RequirementsSettings`, `World` of
  `logic/overworld.py` for `overworld=normal`, the nine dungeons through
  `updateIndoorLocation`, the entrance loop on the identity mapping in
  `ENTRANCE_INFO`'s order, the egg requirement, the removed dungeon
  items, discovery and configure): the start, `windfish`, `egg` and
  `nightmare` locations of the defaults, the pair LADXR's fill refuses
  built, an unsupported value refused (`LADX_LOGIC_UNSUPPORTED`, nothing
  built) and a boss mapping out of range broken. It prints
  `test_logic: synthetic graphs, renderer, explorer, constructors and
  configure ok (<c> checks)`. Then, without a flag, it rebuilds every
  `spot` line of `oracle/logic_graphs.txt` (its five graphs' locations
  chained with their dungeons, each spot from its class and the
  constructor arguments its name id gives back) and configures them under
  the graph's settings (the name before its last `/`, set on defaults,
  then validated): each spot's name id, `checkMetadata` name and area and
  options equal the line's; it prints `test_logic: <n> spot lines of <g>
  graphs of oracle/logic_graphs.txt rebuilt from class and name id: name
  id, metadata and options after configure equal`. Then it builds each
  dungeon (D0 to D8) alone under each graph's settings and `WorldSetup()`'s
  defaults and compares it with the graph (`lf_dungeon_compare`): both
  walked in preorder from the location named `D<n> Entrance`, following
  in order only the edges whose target has dungeon `n`, locations paired
  by walk position; each pair's name, spot lines (class, name id,
  options, metadata) and in-dungeon edges (target walk position,
  requirement text) equal, edges leaving the dungeon skipped (`Logic`'s
  entrance loop adds them). A difference fails with `dungeon D<n>, graph
  <name>: walk position <p> (<name>): ...` and the first differing name,
  spot or edge; it prints `test_logic: D0 to D8 built alone equal to the
  5 graphs of oracle/logic_graphs.txt (<w> dungeon walks, <l>
  locations)`. Then it builds each graph whole (`ladx_logic_build`) under
  its settings and `WorldSetup()`'s defaults: its canonical text must
  equal the graph's, line for line. A difference writes C's text to
  `build/logic/<name>.txt` (`/` in the name as `+`; `make test`, `make
  logic` and `unit-arm` create the folder) and fails with `graph <name>:
  differs from line <n> of its text; C's text in <file>, LADXR's from
  python3 tests/ladx/oracle/extract_logic.py --graph '<name>' --out
  tmp/ladx-logic tests/ladx/oracle/ladxr`, to diff the two; it prints
  `test_logic: the 5 graphs of oracle/logic_graphs.txt built whole
  (ladx_logic_build) line-identical (<n> lines)`. With `--fixtures` it then reads every configuration of
  `oracle/logic_vectors.txt`: its settings set on defaults (`""` the
  empty value) and validated, each assignment kept by validate, every
  value supported alone (`ladx_setting_value_reason`; the pair
  `boomerang=default` with `accessibility=all`, which LADXR's fill
  refuses on every seed but whose logic builds, is counted, not refused),
  and its 13 `ws` lines read into `LadxWorldSetup` (`boss_mapping`,
  `miniboss_mapping`, `goal`, `goal_count`) and the harness (`multichest`,
  `sign_maze`, `inside_to_outside`, `keep_two_way`, `one_on_one`, kept as
  text; `entrance_mapping identity`, `bingo_goals -`, `map -` and
  `dungeon_chain -` required) and rendered back unchanged, each of the
  five baselines' logic fields `WorldSetup()`'s (the dungeon claim's
  world setup); it prints `test_logic: <n> configurations of
  oracle/logic_vectors.txt read: settings kept by validate, every value
  supported alone (<p> refused only as a pair LADXR's fill refuses), <w>
  ws lines read back unchanged, the 5 baselines' world setup
  WorldSetup()'s`. For each configuration it also builds the graph
  (`ladx_logic_build` with the configuration's settings and world
  setup), renders its canonical text and compares the text's SHA-512
  (`ladx_sha512`) with the `graph` line; a difference writes C's text to
  `build/logic/<config>.txt` (`/` as `+`) and fails with `config <name>:
  graph SHA-512 <16 hex>..., fixture <16 hex>...; C's text in <file>,
  LADXR's from python3 tests/ladx/oracle/extract_logic.py --graph
  '<name>' --out tmp/ladx-logic tests/ladx/oracle/ladxr`. Then it replays
  the six `explore` lines on that graph: a new `LadxExplorer`, each
  `item=count` added in order (`ladx_explorer_add_item`), then
  `ladx_explorer_visit` of the start; every location's reached flag must
  equal the line's index ranges and `ladx_explorer_required_next`, sorted
  by name as Python's `sorted`, its required items. A difference fails
  with `config <name>: explore <k>: location <i> (<name>) reached|not
  reached, LADXR's ...` (the first differing index) or `config <name>:
  explore <k>: required item <i> is <got>, LADXR's <want>` (`(end)` past
  a list's end). It prints `test_logic: <n> configurations, graphs equal,
  <v> explore vectors equal` (425 and 2550) and the largest counts any
  configuration reached against the library's capacities: `test_logic:
  maxima over <n> configurations: <l>/<cap> locations, <e>/<cap> edges,
  <s>/<cap> spots, <o>/<cap> options appended (at most <a> on one spot,
  <w> options on one spot), <r>/<cap> requirement nodes, <t>/<cap> terms,
  <d>/<cap> explorer todo` (nodes and terms as created in the arena, the
  todo list's largest length after a visit). The replay takes about 3 s
  on the host and 25 s under `qemu-arm`, so it stays out of `make test`.
- `make -C tests/ladx logic` (run by the root `make logic`, after
  `tests/alttp`'s, part of `make check`): runs each `LOGIC_TESTS` binary
  with `--fixtures` from `tests/ladx`, then prints `logic: <n> LADX unit
  tests replayed the oracle fixtures`.
- `make -C tests/ladx lint` (run by the root `make lint`, after
  `tests/alttp`'s): every `ladx/src` and `ladx/host` source compiled with
  `-pedantic-errors` added, then `lint: <n> ladx/ sources and <h>
  ladx/host/ sources build with -pedantic-errors`.
- `make -C tests/ladx unit-arm` (run by the root `make unit-arm`, after
  `tests/alttp`'s; outside the net): `setup: qemu-arm not found on PATH
  (install qemu-user)`, exit 1, without `qemu-arm`; `onion-toolchain.sh`'s
  `setup:` line without docker or its daemon. Else it prints `unit-arm:
  building <n> LADX unit tests in build/arm/unit with Onion's toolchain`,
  compiles `ladx/src`, `ladx/host`, `unit/support.c`, `unit/logic_fixture.c` and each test (linked with `-lm`) with Onion's
  toolchain and the flags above plus `-O2 -marm -mtune=cortex-a7
  -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve -static
  -D_FILE_OFFSET_BITS=64` (as `tests/alttp`'s), checks with `file` that
  `test_pyrandom` is a 32-bit ARM EABI5 executable, prints `qemu-arm
  --version`'s first line, runs every test under `qemu-arm` from
  `tests/ladx` (the same fixture, the same output lines), then each
  `LOGIC_TESTS` binary again with `--fixtures`, and ends with `unit-arm:
  <n> LADX unit tests and <m> fixture replays passed under qemu-arm`; any
  failure exits
  non-zero.

## Full-ROM check

`make -C tests/ladx rom-check [LADX_ROM=<file>] [LADX_GEN=<binary>]
[CASES="<case> ..."]` (driver `oracle/rom_check.py`, outside the net) runs
a generator on every case and combo (or the named ones) with the base ROM
and compares the SHA-1 of each output with the `rom_sha1` line of the
case's `run.txt`. It is the whole-output proof the write-log claims do not
give. Every run first proves the driver on a synthetic 64 KiB ROM, a
planted case tree and a fake generator (`make -C tests/ladx
rom-check-selftest` runs only that; `make check` runs it too, through `make test-ladx-oracle`).

Defaults: `LADX_ROM` `tmp/ladx/base.gbc`, `LADX_GEN`
`tests/ladx/build/ladx_patch`, both repository-relative. The work
directory is `tmp/ladx-rom-check/<case>/` (`out.gbc`, deleted when `ok`,
and `gen.log`), emptied at the start.

### Generator contract

The interface Phase 3's `ladx_patch` implements; the driver and its
self-test change with it, in one commit.

- Invocation: `<gen> --base <rom> --out <file> --seed <32 lowercase hex>
  [<key>=<value> ...]`, the settings being the case's non-default
  assignments in LADXR's order, as `config.txt` holds them (none for the
  baseline). `<rom>` and `<file>` are absolute paths.
- Exit 0 with `<file>` written: a generated ROM, hashed.
- Exit 3: the base was refused (missing, or not SHA-1 `d90ac17e…`), no
  output; reported as `REFUSED`, never as a mismatch.
- Any other exit, a signal, more than 120 s, or exit 0 with no file: a
  failure (`FAILED`).
- It runs in its case's work directory, with `LADX_ROM_URL` removed from
  its environment; its stdout and stderr go to `gen.log`, never to the
  terminal.

### Output

- `rom-check: base <sha1> (the LADX English 1.0 ROM | not the LADX ROM),
  generator <path>` (a base that is not the ROM is still run: the
  generator's own check is what gets exercised);
- per case: `ok <case>`, `MISMATCH <case>: sha1 <got>, golden <want>`
  (the output kept, to diff against the mirror's `out.gbc`), `REFUSED
  <case>: base refused (exit 3)`, `FAILED <case>: <exit <n> | signal <n> |
  timeout <s> s | no output>`, `NOGOLDEN <case>: <reason>` (no `rom_sha1`,
  or an outcome other than `ok`; the generator is not run);
- `rom-check: <n> cases, <m> combos: <a> ok, <b> mismatched, <c> refused,
  <d> failed, <e> no golden`.

Exit 0 when every case is `ok`, else 1; a missing ROM or generator is a
`setup:` line and a usage error (an unknown case, a `--work` outside
`tmp/`) exits 2. Only digests are printed, never ROM bytes.

## Regenerate and verify

Goldens are ROM-derived: never edited by hand, regenerated only with the
oracle, in their own commit stating the reason. With the ROM
(`tmp/ladx/base.gbc`) and LADXR at the pin (`oracle/setup.sh`):

- `oracle/gen_golden.sh [<case>...]` writes the goldens into the case
  folders and everything else into `tmp/ladx-goldens/` (about 15 s per
  case);
- `python3 -I oracle/golden_check.py [<case>...]` checks each golden
  against its mirror: `decode`, `encode`, `tags` (every group a `table` or
  `C` group; the union over all goldens equal to the inventory's writing
  groups less `spoiler`), `scrambled`, `projection` and `nintendo`;
- `oracle/determinism.sh --full --all [<case>...]` runs every case (or
  the named ones) twice (hash seeds 0 and 1, a fresh clone, a tagged work
  root) and compares every file, `rom_writes.txt` included.
