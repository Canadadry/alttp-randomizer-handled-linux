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
4. else the **fragment rule** below, on the bytes left;
5. else a literal.

No aligned 4-byte literal of a table group occurs in the ROM. The fragment
rule's pieces are not longest matches: `gen_tables.py` never couples or
fuses units because of them.

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
inside one KS run becomes a copy of its lowest offset. Then the
**fragment rule** below, on the segment's net literal bytes; the write
identities a piece names are renamed to its copy, as KS-match does, so a
later carried copy (`save`'s packing) inherits the reference. No other ROM
matching ever applies to a C group.

**Fragment rule** (no 1–3-byte piece of a source entry stays a literal;
`golden_check.py`'s `fragments` check and `table_replay.py`'s table scan
prove it). It needs only KS, so C reproduces it from its own KS, which
must give two lookups with these exact rules:

- `entries`: the source span `(start, stop)` of every KS entry, by table
  and key (alt keys included); pointer, bank and boss bytes are not
  entries;
- `span_at(q)`: the KS span holding source offset `q` with the lowest
  start (the longest of those on a tie), or none outside KS.

A record's **reference bytes**: for a `T` record, the source bytes of the
KS entry at the same table and key (none for `texts`, an integer-pointer
entry, or a key outside KS: background entries other than 3, 4 and 0x0D
of the tiles, 3 and 4 of the attributes); for a `B` record, its indices
are cut where `span_at` of their offset changes, each piece against the
source bytes of its span, offsets outside KS skipped. **The walk**: in each
piece, each maximal run of literal bytes is walked left to right; at each
position the longest prefix of at least 1 byte that occurs in the
reference bytes becomes a copy (`@q+n`) of its lowest offset there, and
the walk goes on after it; where no byte occurs, the walk moves on one
byte. Copies merge with their neighbours by the canonical rendering; `I`
lines are computed from the literals left.

**KS**, the C-knowable source map (`oracle/known_source.py`), is the set of
source offsets whose values C parses from the user's ROM at runtime:

- every room's entity entry and object entry, alt rooms included;
- background entries 3, 4 and 0x0D of the tiles, 3 and 4 of the
  attributes;
- every room sprite data entry, and the pointer and bank bytes of every
  table `save` re-serializes;
- the zero bytes of bank 0x14 0x04E0..0x051F, and the item ids at bank 0x3E
  0x3300 plus each boss room.

KS is built from the ROM by `known_source.build`, only with the ROM present
(`golden_check.py`, `write_log.py`'s encoder, `nintendo_bytes.py`'s
`invariant` clearance, `gen_tables.py`); nothing derived from it is
committed. A span is in KS only when its parsed fields rebuild it exactly
(`known_source.py --rom` prints `NOT-REBUILT` for any other). C's reading
of those bytes from the user's ROM is verified with the real ROM by
`rom-check` ("Full-ROM check").

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
checks the files against the inventory without the ROM. Their records
follow the table groups' encoding rule, the fragment rule included
("Encoding rule"), when `gen_tables.sh` builds them with KS.

**Files.** Line-based ASCII, single spaces, one version line first.

- `order.txt` (`ladx-tables-order 1`): the inventory's sections
  (`section <file> <function>`), then every writing row in LADXR's
  statement order: `table <group> <unit> <call>` or `C <group> - <call>`,
  `<call>` the row's inventory key (its `Call` text, ` #<k>` included).
- `<group>.txt` (`ladx-tables 1`), one per `table` group: `group <group>`,
  `tables <table>...` (ROMWithTables' tables in their order, as in a write
  log), then its units.
- `<group>.txt` for a `C` group, its **constants file** (today
  `bank3e.txt`, `bosses.txt`, `endscreen.txt`, `enemizer.txt`, `goal.txt`, `items.txt` and `warp-home.txt`): the settings-only writes of the
  group's call that C cannot compute (LADXR's assembler, LADXR's binary
  files), in the same format; below.

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
group's projection equals the golden's. Before the replays it scans every
`B` and `T` record of the tables for fragments, as `golden_check.py`'s
`fragments` check does for the goldens (`BAD tables: fragments <file> line
<n>: ...`).

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

**Constants files.** A `C` group's file holds what its call writes
without the seed, as **values**: each atom is a byte (a literal, or the
source ROM's byte at a reference or keep), and the group's port writes
those bytes as computed (origin `known`) through the C-group encoder, so
a reference never carries an origin; it only keeps ROM bytes out of the
file, which is encoded by the table rule (keep, longest source match of
at least 4 bytes, the fragment rule, literal) and falls under the
`tables` scope of the Nintendo-bytes check. `oracle/gen_tables.py`'s
`CONST_GROUPS` names, per group, its call in seed-free form, its
components and the writes C makes itself. For every reachable tuple of
the components (with a witness validated by LADXR) the call runs on a
fresh parse of the ROM, after `generateRom`'s `asm-consts` statements of
the inventory evaluated in `generator.py`'s namespace, every bank and
table write recorded; a `rom.patch` skipped as already applied, an own
write a run misses, or a position written by both units is refused. The
writes equal in every run form `<group>.1` (no component), the others
`<group>.2` keyed by the components; their units match no `order.txt`
row, and C writes them, unit by unit in file order, at the group's `C`
row (`ladx_pipeline_const`, "Pipeline"). `bank3e`: `addBank3E(rom,
None, None)` keyed by `asm:HARD_MODE`, `asm:KEYSANITY_BEAKS`,
`asm:KEYSANITY_KEYS`, `asm:KEYSANITY_MAPS` and
`asm:KEYSANITY_NIGHTMARE_KEYS` (32 entries of 9,953 to 9,978 bytes of
assembled code and the handler pointer; the trampolines, bank 3 flags,
owl rupees, photo-album fix and texts 0x89, 0xC8 to 0xCB and 0xD9 in the
fixed unit); text 0xBE, the copy of 0x111, is C's own. A group whose call
draws once from a module's `random.choice` (`variants`, no component)
runs once per element of the drawn list, the `k`-th with `choice` pinned
to return the `k`-th (a run drawing other than once is refused): the
writes equal in every run form `<group>.1`, each run's others its own
unit `<group>.<k+2>`, no component, one entry. `endscreen`:
`updateEndScreen(rom)` over the global `random`, the fixed unit the
call's `rom.patch` writes, then the image of the `k`-th of LADXR's 24
sorted `patches/cats/*.bin` at unit `endscreen.<k+2>` (one 6,184-byte
`B` record at bank 0x3F 0x0400; 312,737 bytes in all). The cats are
LADXR's art, but 2bpp tiles of few values coincide with ROM runs
(measured with the ROM: 5,060 strict literal runs over the 24 files, the
longest match of more than 3 values 56 bytes at about 1.2 bits per
byte), so they reach C only through this file, under the `tables`
scope, their coincidences as references read back from the user's ROM.
A group in **calls** mode (`ConstSpec.calls`, `calls_group`) lists
`(call, own)` pairs, each run once on a fresh parse (no component), its
writes less its own positions forming its own unit `<group>.<k>` in list
order (`nothing` when empty), with no common unit. A call given as
`(call, own, keep)` keeps only its bank writes inside `keep`'s
`(offset, length)` ranges, every other write dropped; a range not
written whole, or a write straddling a range's edge, is refused. An own
position `("byte", offset)` cuts that one bank byte out of every write
covering it, splitting the write around it (`_drop_bytes`); a call
writing no such byte is refused. `goal` (265 units,
28,499 bytes): `goal.1` is `patchMaze(rom, [], [])` less room 6's three
stores (entities 6, `rooms_overworld_top` 6, its overlay at bank 0x26
0x1E0): the zeroed sign tables at bank 0x0C 0x3800..0x39FF, the egg
event's three bank 0x19 patches, entity 0x4C's bank 0x03 and 0x20
patches, the sign handler at bank 0x0C 0x0000..0x07FF and its tiles at
bank 0x3F 0x3700; `goal.2` to `goal.10` are
`setRequiredInstrumentCount(rom, c)` for `c` = -1 to 7 less text 0x1A3;
`goal.11` to `goal.265` are `setSpecificInstruments(rom, list)` less
text 0x1A3, for the mask `m` = 1 to 255 (bit `i - 1` for instrument
`i`), the list `m`'s instruments ascending. `items` (12 units, 907
bytes), each call a `locations/` class's `patch` on a fresh parse with a
placeholder item: `items.1` `BeachSword` less its item byte and room
0xF2's three stores (the like-like patches at bank 3 0x1B9C and
0x244D); `items.2` `StartItem` with the sword less its item byte (bank 5
0x0CDA); `items.3` `Chest(0x1B6)` with the sword less its chest byte
(bank 3 0x145D, the operand a placeholder); `items.4` the boomerang
trade with the hookshot less its four texts (four instructions, their
operands placeholders); `items.5` the boomerang gift less texts 0x221
and 0x225 and the item byte at bank 0x19 0x072B; `items.6` keyhole
0x2C3 less its item byte and the castle's patches (the hook: bank 2
0x2FA9, 0x3D00, 0x3FFB); `items.7` the same call less its item byte and
the hook (bank 2 0x3831, bank 0x15 0x00E9); `items.8` to `items.12`
keyholes 0x0D3, 0x0B5, 0x02B, 0x08C, 0x00E less their item byte, the
hook and the target word (each one's handler at slot 0, bank 2
0x3FEF). `bosses` (3 units, 74 bytes): `bosses.1` is
`changeMiniBosses(rom, {})`, its seven fixed patches (bank 3 0x0BE0,
bank 6 0x0533, bank 0x14 0x14AC and 0x3FE0, bank 0x36 0x14F7, 0x153B and
0x1C14); `bosses.2` is `changeBosses(rom, list(range(9)))` kept at the
genie's `ld [hl], $91` (bank 4 0x0521) and the eagle check `cp $06`
(bank 2 0x1FC8); `bosses.3` is `changeBosses(rom, [3, 6, 5, 0, 4, 2, 1,
7, 8])` kept at the fish's and the eagle's `ld hl` (bank 3 0x1A0F and
0x1A04, their operands placeholders) and the facade's three `cp $00`
(bank 4 0x121D, 0x1226, 0x127F). `enemizer` (1 unit, 19 bytes):
`enemizer.1` is `randomizeEnemies(rom, bytes(16))` kept at the colour
ghouls' two patches (`call $7F00` at bank 0x36 0x23DD, the 16-byte check
at 0x3F00), its rooms dropped. `warp-home` (1 unit, 13 records):
`warp-home.1` is `warpHome(rom)` less the five `ld a, $NN` operands of
its handler (bank 1 0x3E39 + 5k, k = 0..4: type, map, room, x, y) and
background 0x0D's tiles: the S&Q menu's patches (bank 1 0x00B7, 0x012A,
0x3E02 and the handler at 0x3E20 around its operands) and the menu
sprite (bank 0x0F 0x08E0, 32 bytes). `tables.py check`
accepts at most one such file per `C` group of the inventory (any other
name is `EXTRA`), checks its domains and duplicates, and matches its
units to no row.

**Regenerate.** `oracle/gen_tables.sh` rebuilds `data/ladx/` (coverage
runs under `tmp/ladx-tables/`), the constants files included;
`gen_tables.sh --const [--out <dir>]` rewrites only the constants files
(`gen_tables.py const`, `PYTHONHASHSEED=0`, about 70 seconds; any other
file untouched); `gen_tables.sh --check` rebuilds them with
another hash seed on a fresh clone and compares every file byte for byte.

## Table application

The library applies `data/ladx/` (above) with no allocation, no file
access and no stdio: the caller hands it the files' text.

**Opening** (`ladx_tables.h`, `ladx_tables_open`): `order.txt` and the
group files as byte spans the caller owns and keeps alive; the library
indexes them into a caller-owned `LadxTables` and copies no payload. It
checks what C relies on, not everything `tables.py check` checks: the
version lines, `section`, `table` and `C` rows, each group file's `group`
and `tables` lines (every group file's identical: it is the write log's
`tables` line), `unit` lines (a component a setting key, one of the 0/1
`asm:` constants below or `count`, none repeated), `E` lines with one
value per component (`""` or a name), each entry's `B` and `T` records
(write-log v1 tokens, a known table, a canonical offset or index key) or
`nothing`, every file ending with a newline. A `table` row whose group
has no file, a unit missing from its group file, a repeated unit or
group, or a capacity exceeded is an error. An error has a status
(`ladx_tables_status_name`: `bad-version`, `bad-line`, `bad-component`,
`bad-values`, `bad-token`, `tables-differ`, `dup-group`, `dup-unit`,
`no-group-file`, `no-unit`, `overflow`, `no-entry`, `buffer`, `range`),
the caller's file name and the 1-based line; nothing is half-opened.
Capacities: 16 groups, 512 units, 512 `order.txt` rows, 2048 entries, 12
components per unit (today 8, 382, 147, 991 and 6, the constants files
included; units raised from 256 for `goal.txt`'s 265).

**Key.** From validated, supported settings and the spot count: a
setting renders `ladx_setting_value_name`'s name, `""` for the empty
string (a setting outside tier 1, `overworld` or `shopsanity`, at its
validated value); `asm:<NAME>` is `0` or `1` as `generateRom`'s
`assembler.const` rows compute it: `HARD_MODE` (`hardmode` not `none`),
`DUNGEON_CHAIN` (`overworld` `dungeonchain`), `ITEM_DIALOGS` (`textmode`
not `notextbox`), `KEYSANITY_KEYS`, `KEYSANITY_NIGHTMARE_KEYS`,
`KEYSANITY_BEAKS`, `KEYSANITY_MAPS` (`dungeon_keys`, `nightmare_keys`,
`dungeon_beaks`, `dungeon_maps` `keysanity`); `INV_SIZE` is no component.
`count` is decimal: `ladx_tables_spot_count`, the finished logic's spots
whose kind is not exactly `KeyLocation`, `addFrameCounter`'s argument.

**Walk** (`ladx_tables_walk_*`), as `tables.select`: at a `table` row
that starts its unit, a step with the group, the unit and the entry the
key selects (`ladx_tables_select`); a later row of the same unit, nothing;
at a `C` row, a step with its group and call and no entry. A unit with no
entry for the key stops the walk with `no-entry`, its unit id and the
key's values as an `E` line writes them: never a skip.

**Records** (`ladx_tables_records_*`): an entry's records in order, each
`B <offset>` or `T <table> <key>` (an index or an alt key), its payload
decoded into the caller's buffer as atoms (a source reference q as q,
keep tokens resolved to the record's own offsets; a literal b as
`-1 - b`) or an integer pointer; a payload over the buffer is `buffer`.
**Applier** (`ladx_tables_apply`): a `B` record's bytes written onto the
output ROM, each a literal or the source ROM's byte at its reference
(never the output's); an offset beyond the output or a reference beyond
the source is `range`, nothing written; a `T` record writes nothing (the
tables reach the ROM through `save`).

**Write log** (`ladx_wlog.h`): `ladx_wlog_begin` writes `ladx-rom-writes
1` and the `tables` line; each `ladx_wlog_bank`, `ladx_wlog_table` or
`ladx_wlog_pointer` record of a group merges into the open segment when
it is the same group, else closes it and opens the group's; the segment
keeps its net effect (bank bytes last write wins, a table entry replaced
whole). Closing renders `G <group>`, then `B` records over maximal runs
of contiguous offsets, then `T` records in table order, index keys
ascending before alt keys in byte order, with `write_log.render`'s
canonical tokens ("`rom_writes.txt`, version 1"); a segment with no
record is never written, and no `I` line ever. Text goes to the caller's
sink in pieces, so no buffer bounds a record. Capacities per open
segment: 262144 bank bytes, 4096 table entries, 65536 table atoms (a
replaced entry's atoms reclaimed first; today's largest projections
121,397 bank bytes (`save`), 1,035 entries and 34,094 table bytes
(`options`)); an overflow is returned by that call and every later one.
A refused record (an empty bank record, a literal over `ff`, a bad name,
table or alt key) changes nothing.

**Parsing and projection** (`ladx_wlog_project`, `ladx_wlog_groups`): a
version-1 log's text is checked with every rule of `write_log.parse`
(version and `tables` lines, `G` lines, `B` and `T` records with their
order and canonical tokens, `I` lines after every record, increasing,
each naming an earlier `B` or `T` record that is not a pointer, its
ranges canonical, not touching and each inside one literal token), with
two limits of C's: names (tables, groups, alt keys) of at most 31
characters, and 1 to 16 tables. A malformed log is `malformed` with the
1-based line at fault (0 when the text does not end with a newline),
nothing written. `ladx_wlog_groups` lists the groups, each once, in
first-write order (`write_log.groups`). `ladx_wlog_project` replays one
group's records in order into a fresh log (the same-group merge keeps
the net effect) and renders it as one segment with the header lines,
`write_log.py project`'s text; a group absent from the log gives the
two header lines; `I` lines are checked, then ignored. A table entry
over 65536 atoms is `overflow`. The token decoder (`ladx/src/wlog_token.h`,
internal) is the same for the patch tables' records.

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
(`test_sha512`, `test_sha1`, `test_rom`, `test_ks`, `test_pyrandom`, `test_rng`, `test_run_text`,
`test_settings`, `test_requirements`, `test_logic`, `test_world_setup`, `test_itempool`, `test_fill`, `test_wlog`, `test_tables`, `test_encoder`, `test_pipeline`, `test_bank3e`, `test_md5`, `test_background`, `test_title`, `test_text`, `test_marin`, `test_hints`, `test_music`, `test_endscreen`, `test_notext`, `test_room`, `test_goal`, `test_items`, `test_entrances`, `test_bosses`, `test_sprite_data`, `test_enemizer`, `test_warp_home`; `LOGIC_TESTS`, run again with `--fixtures`: `test_logic`, `test_itempool`, `test_fill`, `test_encoder`),
failing through `unit/support.h`'s `test_fail` (`<test>: FAIL
<message>`, exit 1). Every LADX build, host, lint and ARM, uses `-std=c99 -Wall -Wextra -Werror -ffp-contract=off`:
GCC fuses `a * b + c` into an FMA in its GNU modes and Onion's VFPv4 has
one, while CPython never fuses, so `uniform`'s `a + (b - a) * random()`
would differ in its last bit. Phase 4, which builds `ladx/` into the app,
keeps the flag. Library objects see only `-I ladx/include`; the host
layer `ladx/host/` (C99, stdio allowed, never built into the device's
library) sees `-I ladx/include -I ladx/host`; the tests add
`-I tests/ladx/unit`, and those linking the host layer (`HOST_TESTS`:
`test_run_text`, `test_itempool`, `test_fill`) `-I ladx/host` and `-lm`;
those linking the synthetic ROM builder `unit/synth_rom.c` (`SYNTH_TESTS`:
`test_rom`, `test_ks`, `test_pipeline`, `test_bank3e`, `test_title`, `test_marin`, `test_music`, `test_endscreen`, `test_notext`, `test_room`, `test_goal`, `test_items`, `test_entrances`, `test_bosses`, `test_enemizer`, `test_warp_home`) its object. Build output goes to `tests/ladx/build/`.

- `make -C tests/ladx test` (root `make test-ladx`, run by `make test`):
  builds `build/libladx.a` and the unit tests with host `cc`, runs each
  from `tests/ladx` (fixture paths are relative: `test_pyrandom` reads
  `oracle/pyrandom_vectors.txt`, always, with no flag; `test_settings`
  reads `oracle/settings.txt`, `oracle/settings_validate.txt`,
  `oracle/tier1.txt`, every `cases/*/config.txt` and `combos/*/config.txt`
  and `../../notes/plan3-scope-cuts.md`), then prints
  `test: <n> LADX unit tests ran`. `test_sha512` prints `test_sha512:
  FIPS 180-4 vectors, padding boundaries and streaming splits ok`;
  `test_sha1` checks `ladx_sha1.h` the same way (FIPS 180 `""`, `abc`,
  the 448-bit message and one million `a`; the padding boundaries 55,
  56, 63, 64, 119 and 120 bytes, digests from `hashlib`; every split of a
  200-byte stream; `NULL, 0`) and prints `test_sha1: FIPS 180-4 vectors,
  padding boundaries and streaming splits ok`.
  `test_rom` checks `ladx_rom.h`: the base check (`ladx_rom_check`: a
  planted 1 MiB buffer refused as wrong SHA-1, 1 MiB minus 1 byte as
  wrong size, each with its SHA-1 as the digest; the accepted SHA-1
  `d90ac17e…`); then the reader (`ladx_rom_tables_parse`, LADXR's
  `ROMWithTables` tables as `PointerTable` and each table's `_readData`
  read them) on a synthetic 1 MiB ROM that `unit/synth_rom.c` builds
  from a fixed PRNG in LADXR's layout (`romTables.py` written out in the
  builder, not taken from the library: the pointer tables, the texts'
  banks table, the seven alt pointers, entries of every kind, integer
  pointers, every byte synthetic), recording where it put each entry:
  every table's name, count, alt count and first entry, and every
  entry's key, span or integer pointer, bank, pointer place and length
  and banks-table byte equal to the builder's record, through the entry
  array and `ladx_rom_entry`; and each fault refused with its table
  (`LADX_ROM_PARSE_BAD_SIZE` for 1 MiB minus 1 byte; `PAST_BANK` for a
  text, an entity list, a room object, a room header and a background
  run reaching their bank's end; `ALT_POINTER` for `Alt8C` at `0x1234`
  and `0x8034`). It prints `test_rom: base check, the tables of a
  synthetic ROM parsed as built, faults refused (<c> checks)`.
  `test_ks` checks `ladx_ks.h` (KS, `known_source.build` ported) on the
  same builder's ROM, which plants entity lists whose `0xFF` falls on a
  pair's second byte and texts' banks bytes over `0x3F`: the span list
  (below, "KS span list") equal to the one the test writes from the
  builder's record in `known_source.build`'s order (per table the
  pointers, alt keys first, each text's banks byte after its pointer,
  then the entries of the KS keys, then the boss zero bytes and item
  ids, then the failures), with its span, byte and failure counts;
  `entries` of every key (none for an integer pointer, a key outside KS
  or a failure); `span_at` of each entry's last byte its own span; on
  hand-built maps, `span_at` with nested, duplicate and overlapping spans
  (the lowest start, the longest on a tie, nothing before `finish`),
  membership, the maximal runs, `find` (the lowest offset inside one
  run, nothing across two runs or a gap, under 4 bytes or outside KS), a
  key added twice giving its later span, and the statuses (an empty
  span, a span past the source, an add after `finish`, the span
  capacity's overflow). It prints `test_ks: the span list of a synthetic
  ROM (<s> spans, <b> bytes, <f> not rebuilt, <e> entries)` and
  `test_ks: build, span list, entries, span_at, runs, find and statuses
  (<c> checks)`.
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
  `random`, `choice`, `choices` (integer weights), `shuffle`, `randint`
  and `randrange` (start, stop, step 1) each leave the state `ladx_pyrandom` leaves and return what
  it draws, with one record holding the arguments, the result and the MT
  words drawn (the `words` counter's difference; `seed` 0 and the counter
  reset); `shuffle` records the permutation `ladx_pyrandom_shuffle` makes
  of `0..n-1` and applies it as `x[to] = before[perm[to]]` (4-byte and
  7-byte elements); the golden `baseline`'s `choices 12 ... 1 -> 1 [2]`
  and `goal-specific`'s `shuffle 8 -> 0,6,5,3,4,1,2,7 [11]` drawn from
  their seeds; `copy_state` (`random.setstate(rnd.getstate())`) a
  `getstate` record of `rnd` and a `setstate` record of `global`, both
  `[0]`, the global continuing `rnd`'s stream; an empty `choice` or
  `choices` (`IndexError`), an empty `randint` or `randrange` range (`ValueError`) and a
  list over `LADX_RNG_MAX_LIST` (256, overflow) draw nothing and record
  nothing. It prints `test_rng: seed, random, choice, choices, shuffle,
  randint, randrange and the state copy traced as ladx_pyrandom draws (<c>
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
  numbering lines from 0 across both generators; the `outcome` line
  (`ok`, read from `baseline`; `error <message>`, `crash <type>
  <message>`, `""` for an empty message; nothing for an unfinished run or
  over its buffer); the 13 `ws` lines of a hand-built `LadxWorldSetup`
  equal to `goal-maze`'s (`sign_maze`), `boss-random`'s and
  `goal-specific`'s, `goal open`, `goal_count -1` and a false flag as `0`,
  nothing past the 13th line or over its buffer; `place` lines of a
  hand-built logic (`place 0 HeartPiece 0x0A1 SWORD`, an unplaced spot
  with an empty `nameId` as `place 1 KeyLocation "" -`), nothing for a
  spot out of range or over its buffer. It prints
  `test_run_text: float.hex as CPython's, <g> golden lines rendered
  byte-identical, sink numbering ok (<c> checks)`.
  `test_world_setup` checks `ladx_randomize` up to the world setup
  (`ladx_world_setup_randomize`, `ladx/include/ladx_randomizer.h`) against
  LADXR's own results: `goal=maze` on `item_pools.txt`'s three seeds (the
  first two reach `ItemPool` with `rnd`'s MT word count at their `words_before`, 80
  and 77, and 22 signs; the third gives `crash IndexError Cannot choose
  from an empty sequence` and no world setup); two `buildMaze` vectors
  CPython ran on LADXR's `patches/maze.py` at the pin (a seed rolling
  exactly 20 for a flying sign, a seed retrying six times), their
  `sign_maze` equal; `goal=specific` with `goalcount=0` (one instrument,
  `=2`, multichest `MEDICINE`); and `goal=bingo` returning
  `LADX_RANDOMIZE_UNSUPPORTED`. It prints `test_world_setup: goal=maze's
  crash and words on item_pools.txt's seeds, buildMaze's and
  goal=specific's vectors, goal=bingo unsupported (<c> checks)`.
  `test_fill` checks LADXR's `RandomItemPlacer` (`ladx_placer_*`,
  `ladx/include/ladx_randomizer.h`) on hand-built graphs
  (`ladx_logic_builder.h`) against LADXR's own `RandomItemPlacer` run on
  the same graphs, at the pin, with `random.Random(bytes([<seed>]))`:
  every draw (`shuffle`'s length, each `choice`'s length and index) and
  the placement equal; a five-spot graph (a dungeon key placed first,
  failed placements undone and their spot re-appended at the end of the
  list); three four-spot graphs on which `canStillPlaceItemPool`'s answer
  turns on its sort (the `nameId` tie-break, the dungeon spots' score 0,
  the stable item order); a two-spot graph where every placement fails,
  LADXR's `Error` "Failed to place an item for a bunch of retries" after
  the eleventh, with its 22 draws; `AssertionError` `3 != 2` and
  "Sanity check failed: False" drawing nothing; and `boomerang=default`
  with `accessibility=all` (unsupported, `ladx_settings_supported`) giving
  `AssertionError` "Sanity check failed: False" and no placement through
  `ladx_randomize` on three seeds. It prints `test_fill: LADXR's
  placements, draws and outcomes on hand-built graphs, the boomerang
  pair's AssertionError (<c> checks)`. With `--fixtures` it then replays
  every run of `oracle/fill_outcomes.txt` (below, "Fill-outcome
  fixture"): the line's settings set and validated (support not checked:
  the boomerang pair runs), `ladx_randomize` on its seed with the
  run-text trace sink, `ladx_run_text_write_run`'s fill projection into a
  temporary file, and its outcome kind, line count and digest compared
  (`test_fill: FAIL oracle/fill_outcomes.txt line <n>: <label> k=<k>:
  ...` on the first mismatch), then prints `test_fill: <n> runs of
  oracle/fill_outcomes.txt replayed, same outcome and fill projection
  (<o> ok, <e> error, <c> crash)`; about 4 s on the host.
  `test_wlog` checks the write-log builder (`ladx_wlog.h`, "Table
  application" below) on synthetic segments, each expected text written
  once by `write_log.render` on the same net maps: touching records
  merged into one `B`, a copy on its own offsets as `=n`, continuing
  references merged, adjacent literals joined, the last write winning, a
  write back to its own source byte kept as `=1`, `T` order (table
  order, index keys ascending, alt keys after in byte order), a pointer
  entry, an overwritten entry, a group change closing the segment, a
  record longer than any internal buffer, an empty log's two header
  lines; refused records (an empty bank record, a literal over `ff`, a
  bad name, table or alt key) leaving the log unchanged with no segment;
  and each capacity's overflow (bank bytes, entries, atoms, a replaced
  entry's atoms reclaimed first) returned by every later call. Then the
  parser and projection, each expected text `write_log.py project`'s on
  the same hand-written log: two segments of one group around another's,
  a keep, a copy continued by a copy of a later segment, pointers, an
  alt key, `I` lines, and a group absent (the header alone); a
  projection projected again; an empty table entry; a 1000-byte literal
  and a log of `save`'s 121,397 net bank bytes, projected whole; the
  groups in first-write order and over the caller's array; and 53
  malformed logs, each refused by `write_log.parse` too (version, tables
  line, segments, record order, offsets, keys, every token rule, `I`
  lines), refused at the same line by projection and groups with nothing
  written. It prints `test_wlog: write_log.render's texts of synthetic
  segments, refused records and overflows; write_log.py's projections
  and groups of hand-written logs, 53 malformed logs refused (<c>
  checks)`.
  `test_tables` checks `ladx_tables.h` on synthetic tables: a walk
  interleaving `table` and `C` rows (a `C` step's call), a fused unit
  yielded once at its first row, `nothing`, `""`, booleans, `count`, a
  setting outside tier 1 at its validated value, each 0/1 `asm:` constant
  flipped by one setting, `no-entry` with its unit, values, file and
  line; open errors with their file and line (version lines, unknown or
  repeated components, a unit missing from its group file, a group with
  no file, a repeated unit or group, value counts, bad tokens, differing
  `tables` lines, misplaced lines, a file not ending with a newline, one
  unit over capacity); records decoded into atoms (`=n`, `@q+n`,
  literals, a pointer, an alt key, a payload over the caller's buffer);
  the applier (bytes copied from the source, never the output; `T`
  records and refused records write nothing); and the spot count on a
  hand-built graph with `KeyLocation` spots and an unreached one. Then
  it opens the real
  `../../data/ladx/` (`order.txt` and every other `*.txt` as a group
  file): 12 groups (`bank3e.txt`, `bosses.txt`, `endscreen.txt`, `enemizer.txt`, `goal.txt`, `items.txt` and `warp-home.txt` included), 399 units, 1008 entries, one unit keyed by `count`; and
  for every `cases/*/config.txt` and `combos/*/config.txt`, validated and
  supported, every unit without `count` selects an entry. It prints
  `test_tables: data/ladx opened (12 groups, 399 units, 1008 entries); <n>
  cases and <m> combos select <e> entries`, then `test_tables: synthetic
  walks, keys, open errors, records, applier and spot count (<c>
  checks)`.
  `test_encoder` checks the encoder (`ladx_encoder.h`, "Encoding rule"
  above) on a 4 KiB ROM from a fixed PRNG with planted regions (two
  copies for KS-match ties, a repeated run, runs of distinct values, one
  with a repeat), KS built by `ladx_ks_add`, each expected text written
  from the rule: a carried source copy; an identity inheriting a keep,
  carried by a later group; inherit-on-equal on bank bytes (a KS
  reference at its own offset and away from it, a literal, a source byte
  outside KS left a literal); an entry's prefix, middle run (at the lowest
  index of two equal runs) and suffix, and their stop at a byte outside
  KS; a segment's net effect; integer pointers; a position written back
  to its source byte as a keep; a table group's atoms passed through, its
  identities named by offset (the first naming wins) and carried, fresh
  identities for atoms given without origins; the queries (a byte's
  origin, an entry's origins, fresh identities); refused writes (past the
  ROM, a bad group or table, one segment of both sides); KS-match (the
  lowest offset of equal matches, a longer match at a higher offset, no
  match across a KS run's end); the fragment rule on an entry (its KS
  entry's bytes, the longest prefix at its lowest offset) and on bank
  bytes (pieces cut where `span_at`'s lowest-start span changes); and
  identities renamed by KS-match and by the fragment rule, carried by a
  later segment. It prints `test_encoder: the C-group encoding rule,
  KS-match and the fragment rule on hand-written streams (<c> checks)`.
  With `--fixtures` it replays `oracle/encoder_vectors.txt` ("Encoder
  fixture"): per vector, its KS from the `K` lines (`ks none`: no KS),
  its initial entries, a `C` group's events through `ladx_enc_bank`,
  `_table` and `_pointer`, a `table` group's segment through the `fixed`
  calls (its atoms decoded from the expected log's segment, its origins
  the segment's net ones from the events, entries in first-write order);
  the log must equal the expected one byte for byte (else `test_encoder:
  FAIL oracle/encoder_vectors.txt: vector <name>: got ... want ...`); it
  prints `test_encoder: 216 vectors of oracle/encoder_vectors.txt
  replayed byte for byte (16 hand-written, 200 random, 68 with ks
  none)`.
  `test_pipeline` checks `ladx_pipeline.h` ("Pipeline") on synthetic
  tables (a table group's two units around `C` rows of three groups, one
  twice, one unregistered) and two planted groups: without a ROM only the
  ROM-free group runs, once, at its first `C` row, its write between the
  table entries in `order.txt`'s order; with `unit/synth_rom.c`'s ROM
  parsed and its KS built, the group needing the ROM runs too, after it,
  receiving the parse; a group needing the ROM that has a draw-only run
  runs that run alone without the ROM, writing nothing, and its full run
  with it, never both; a unit with no entry for the key stops the walk
  as a tables error naming the unit; a group's failure stops it naming
  the group; one that sets `raised` stops it as `RAISED` with that outcome,
  `raised` cleared before each run; `ladx_pipeline_const` writes a planted constants file (a
  fixed unit with a keep, a copy and a literal, a unit keyed by `music`
  with a `T` record and a pointer) as computed literals for the key, and
  fails the group on a reference beyond the ROM, a unit with no entry or
  no file; `ladx_pipeline_const_units` writes the keyed unit alone and
  fails on units past the file's end; `ladx_pipeline_const_unit_count`
  counts a file's units, -1 without one; `ladx_pipeline_const_bank`
  reads a unit's resolved bank bytes (keep, copy and literal; inside a
  record) and gives 0 for bytes it does not all write, a unit or a group
  out of range; the library's registry is
  `bank3e`, `marin`, `hints`, `music`, `notext`, `goal`, `items`, `entrances`, `bosses`, `enemizer`, `warp-home`, `title` then `endscreen`,
  `hints` alone needing no ROM and `marin`, `music` and `endscreen` alone with a
  draw-only run. It
  prints `test_pipeline: <c> checks ok`.
  `test_bank3e` runs the library's registry on a synthetic ROM and a
  planted `bank3e.txt` (a fixed unit and one keyed by `asm:HARD_MODE`):
  the expected log on the defaults, `hardmode=hero` (the other entry, the
  other settings string) and `race=1` (no settings string): the
  constants, the seed's 16 bytes at 0x3E:0x2F00, the length and bytes of
  `ladx_settings_short_string` at 0x2F20, text 0xBE as the copy of the
  synthetic entry 0x111's span; bank3e skipped without the ROM, failing
  without its file. It prints `test_bank3e: constants, seed, settings
  string and the 0xBE copy on a synthetic ROM (<c> checks)`.
  `test_md5` checks `ladx_md5.h` against RFC 1321's seven test-suite
  strings, inputs of 0 to 129 bytes around the padding boundaries split
  at every position, and one million `a`.
  `test_background` checks `ladx_background.h` on hand-built backgrounds:
  decoding literal, repeated and vertical runs (a missing terminator or a
  cut run refused); `store`'s encoder (expectations printed by LADXR's own
  `BackgroundEditor.store` on the same tiles): repeat or literal by
  `same > different - 4 and same > 2`, both counts capped at 0x40, gaps
  splitting runs, the output cap, an empty map or a run at 0x10000
  refused; and a round trip.
  `test_title` checks `_encode`'s table, the seed number (hex, the MD5 of
  a non-hex text, bytes over 0x7F as `?`), `ladx_title_edit` on four
  synthetic backgrounds against the bytes LADXR's own `setRomInfo` stored
  on a fake ROM holding them (a hex seed, `Race`, a non-hex text), and
  the group through the library's registry on a synthetic ROM: `B 7 52`
  (`01` on `race=1`), then backgrounds 3 and 4 of the tiles and the
  attributes, each record's tokens resolved against the synthetic ROM
  equal to its entry decoded, edited and re-encoded; skipped without the
  ROM.
  `test_text` checks `ladx_text.h` (`formatText`) against bytes LADXR's
  own `formatText` printed: `{NAME}` fields and `{{ }}`, `'` and tags,
  the 16-column wrap (long and empty words, a 16-byte first word),
  newlines, `_`, `center` and `ask`; `UnicodeEncodeError` with CPython's
  message for one character and for a run, `KeyError` for a missing name;
  overflow, a positional field and a single `}` refused.
  `test_marin` checks `marin.txt`'s embedded lines (SHA-1 of their bytes
  equal to the pinned file's, `1cd50e33…`); every one of the 480 lines
  through `ladx_marin_line_text`, the global generator seeded with
  `b"ladx-marin-lines"`, hashed (`ok <len>\n<bytes>` or `raise <type>
  <message>\n` per line) equal to the digest of LADXR's own
  `getMarinStartingText` forced to each line, lines 387 and 415 raising,
  and the generator's next `random()` equal (as many words drawn);
  five hand-written lines (ordinal suffixes 1, 2, 3, 11, 12 and negative,
  `<rnd>` before `<ord>`, non-matching placeholders, `<marinLinesAmount>`,
  strip and escapes) against LADXR's output with its `open` patched; the
  group through the library's registry on a synthetic ROM with the
  baseline case's seed: one `marin` segment of 60 texts, 0x01 the
  starting text drawn from the randomizer's own generators, 0xEA to 0xEC
  and three others equal to `formatText`'s bytes; without the ROM writing
  nothing but drawing as with it (both generators equal), failing without
  the generators with or without the ROM; and the refusal seed
  ("Pipeline") as `RAISED` with LADXR's outcome, with and without the ROM.
  `test_hints` checks `ladx_hints_add` on planted spots (spot `i`: name
  id `"0x%03X" % (7i mod 13)`, so ties for the stable sort; items cycling
  over `SWORD HEART_PIECE BOW SONG2 RUPEES_20 BOWWOW ROOSTER GOLD_LEAF KEY3
  FACE_KEY MAGIC_POWDER`, hint items and others; areas cycling over
  `Planted Area`, `Sam's Cove`, `Nowhere Field`), `rnd` seeded with
  `b"ladx-hints-<k>"`: 80 spots with `k = 1` (more hint spots than texts)
  and 10 with `k = 2` (useless hints), each write hashed as `<id hex>
  <len>\n<bytes>`, equal to the digest of LADXR's own `hints.addHints` at
  the pin on the same spots and a fake ROM recording its texts, and the
  generator's next `random()` equal (as many words drawn); then the group
  through the library's registry without the ROM on the baseline case's
  seed: one `hints` segment of 43 texts by id (`marin` writing nothing),
  the six books `formatText("Read this book?", ask="YES  NO")`, every text
  the one `ladx_hints_add` gives on the placed spots after `marin`'s
  draws, both generators where those leave them; and the walk failing
  without the generators. It prints `test_hints: addHints on planted
  spots, the group on the baseline seed (<c> checks)`.
  `test_music` checks `ladx_music_randomize` with `rnd` seeded
  `b"ladx-music-<k>"`, `k` 1 to 3: the 0x120 bytes' SHA-1 equal to that
  of LADXR's own `randomizeMusic` at the pin on a fake bank 0x02, every
  byte a looping track, and the generator's next `random()` equal (as
  many words drawn); then the group through the library's registry on a
  synthetic ROM with `music=random` on the baseline case's seed, bank
  0x02's bytes made 0xFF and a planted KS of one 4-byte bank span holding
  East Tal Tal's track four times: one `music` segment whose records,
  resolved against the ROM, give the table `ladx_music_randomize` draws
  from the randomizer's own `rnd`, every byte once, each run equal to the
  KS span a copy of it (at least East Tal Tal's two rows); with `music`
  default, `off` or `shifted`, with or without the ROM, nothing written
  and nothing drawn; without the ROM nothing written but `rnd` drawn as
  with it; failing without the generators, with or without the ROM. It
  prints `test_music: randomizeMusic against LADXR, the group on a
  synthetic ROM (<c> checks)`.
  `test_endscreen` checks `ladx_endscreen_draw` against CPython 3.13
  (`random.Random(b"LADX-endscreen-test")`, six `choice` of 24, then
  `random()`), each draw one traced `choice 24` at
  `patches.endscreen:updateEndScreen`; then the group through the
  library's registry on a synthetic ROM and a planted `endscreen.txt` (a
  fixed unit, then 24 cat units, cat `k`'s bytes `k`, 0xE0 and a 4-byte
  reference): the fixed unit then exactly the drawn cat's unit, its
  reference read from the ROM as literals, the draw from the
  randomizer's own global generator; without the ROM nothing written but
  the same draw; a file of 23 or 25 cats, no file, or no generators
  (with or without the ROM) failing the group. It prints
  `test_endscreen: the drawn cat's constants on a synthetic ROM, the
  draw without it (<c> checks)`.
  `test_notext` runs the library's registry on a synthetic ROM after a
  planted table group writing text 0x10 a literal and text 0x11 an
  integer pointer: with `textmode=none`, every texts index key but
  0x217..0x21A and the integer pointers (the source's and 0x11) written
  as its last byte, a copy of its source terminator, 0x10's the planted
  literal's last byte; with any other `textmode`, or without the ROM,
  nothing written. It prints `test_notext: texts cut to their
  terminator on a synthetic ROM (<c> checks)`.
  `test_room` drives `ladx_room.h` ("Room editor") through an encoder
  over a synthetic ROM: every room loaded and stored unchanged gives its
  overlay as a keep and its entries as copies of themselves (KS's
  inherit-on-equal), the planted ones (an integer entry, the entities'
  0xFF off a pair) refused; a planted room parsed into each object kind,
  a warp's room offset by its map (colour dungeon, indoor A, indoor B at
  maps 0x06..0x19), the entity and object helpers, `updateOverlay` with
  and without the floor (an index Python refuses is `RANGE`, a negative
  one wraps), the floor's source offset carried into the overlay, alt
  rooms (`Alt06`'s overlay in bank 0x27, `Alt1F5` none), a store under
  another index with the tileset, attribset and palette bytes, an indoor
  store with its tileset alone; refusals (no 0xFE, a warp past the
  entry, bytes after the terminators, no ROM for a source entry, a field
  outside its byte, an overlay to store and none). It prints
  `test_room: RoomEditor's load, edits and store on a synthetic ROM (<c>
  checks)`.
  `test_goal` runs the library's registry on a synthetic ROM, its
  encoder without KS, after a planted table group giving every overworld
  room literal entities (room 6 two 0xDE), with a planted 265-unit
  `goal.txt` (unit 1 a code byte pair and the zeroed sign tables, unit
  `k + 1` the number `k` at bank 0x19 `4k`): the goal projection equals
  a model's for the counts 0, 4, 7 and vanilla's 8 (text, then unit `2 +
  count`, none for 8), an `open-4` seed drawing -1 (`You need -1
  instruments`, unit 1), specific goals of 1, 3, 7 and a random count
  (the text in the drawn order, unit `9 + mask`), the maze (unit 0, each
  sign's room with the sign entity first, its arrow, 0x40 for the last,
  and number, room 6 last without its 0xDE entities); `open` and
  `seashells` write nothing; a file of 264 units fails the group. It
  prints `test_goal: the goal group on a synthetic ROM and planted
  constants (<c> checks)`.
  `test_items` runs the library's registry on a synthetic ROM, its
  encoder without KS, after a planted table group (bank 2 0x2FA9 0xF0,
  the witch byte, the gift byte 0x00, room 0xF2's entities with a 0x31
  and its objects), with a planted 12-unit `items.txt` of invented
  bytes at LADXR's offsets, on an ok fill of the settings with chosen
  items placed: `CHEST_ITEMS`, `INVENTORY_MAP` and the dungeon-item
  reduction; the multichest; a dungeon chest's reduction and the pot
  chest's operand; a chest keeping another dungeon's key; the gel
  disguise and the copies of rooms 0x169, 0x166 (not reduced), 0x223 and
  0x092; the angler key's two copies, the boss item a source copy; the
  mad batter, tunic fairy and mansion bonus tables; the beach room's
  heart piece and like-like patches; the start item's sprite patch
  (none for the shield) and BowWow; the shop's items and texts; the
  witch (patched, already patched, mismatched as LADXR's
  `AssertionError`, unshuffled); the boomerang gift and trade (operands
  and texts); trade items (texts when changed, the id alone when not);
  owl statues' reduction; keyholes (unchanged: no hook; the hook once,
  each handler below the last, the target word, the castle's patches);
  `KeyError` for an item `CHEST_ITEMS` lacks, at a spot and as the
  multichest; a file without 12 units fails the group. It prints
  `test_items: setMultiChest and every location kind's patch on a
  synthetic ROM and planted constants (<c> checks)`.
  `test_bosses` drives `ladx_bosses_change` and `ladx_minibosses_change`
  on a synthetic ROM, its encoder without KS, after planted rooms (every
  boss room with a warp, stairs, lava, D1 pits, an open door and kept
  objects; rooms 0x1EF, 0x2F8, 0x22E, 0x22C; every miniboss room with two
  warp entities), planted color-dungeon warp bytes and boss items, with
  invented instruction bytes: the identity (the zero warps made 0xFF but
  the two exempt, genie and eagle check written, no room stored); the
  fish into D1 and D1's boss into D4 (event flags, the cleaned rooms, the
  stairs and 0x1EF's warp back, the flag operand, the generic map made
  `MAP1` with no origin, the second item a source copy, D4's two objects
  and four blocks); the fish from D7 and the eagle into D4 (0x22E's warps,
  the 0xDA2E operand, an item's identity carried, the stairs, tileset and
  0x2F8's warp back); the eagle into the color dungeon (its flag, map
  0xFF, the check 0xFF); the facade, the slime eel in D2 and Hot Head in
  D7 (patches, wall tiles, rows 0 and 1 cleared, D7's map bytes, rooms
  0x22E and 0x22C, the masked floor, door, stairs and warp); `KeyError
  'MAP9'` and `'STONE_BEAK9'`, `IndexError` on an empty 0x1EF, a
  malformed room and a target out of range; every miniboss special
  (warp entities kept, Ghoma's collision but in D5, Blaino's warp in D2,
  D8, the color dungeon and the moblin cave and not over a warp, Rolling
  Bones' patch, written, already applied or failing, Dodongo's floor, Cue
  Ball's platform, room 0x228 but for Grim Creeper, Hinox's entity); the
  group in the pipeline with a planted `bosses.txt` (the world setup's
  mappings, the units written, a file without 3 units failing) and the
  registry (right after `entrances`, ROM, no draw). It prints
  `test_bosses: changeBosses and changeMiniBosses on planted rooms of a
  synthetic ROM (<c> checks)`.
  `test_sprite_data` renders `ladx_sprite_data` over rooms 0 to 0x315
  and entities 0 to 255 (`%03x %02x none`, or `%03x %02x` then ` <slot>:<values>`
  per pair, values sorted, `%02x`, comma-separated; absent keys skipped),
  without and with the evil shop, and compares each text's SHA-1 and
  size with LADXR's `SPRITE_DATA` at the pin rendered the same way
  (unmutated, then with `SPRITE_DATA[0x77] = SPRITE_DATA[0x4D]`), after
  spot checks (an absent key, `None`, a lambda's `None`, the moblin
  lambda's branches, the evil shop, a two-value set). It prints
  `test_sprite_data: SPRITE_DATA equal to LADXR's over every room and
  entity, with and without the evil shop`.
  `test_enemizer` checks `ladx_enemizer_pick` on eleven planted rooms in
  one generator seeded `enemizer-test`, without and with the evil shop,
  against LADXR's own `randomizeEnemies` at the pin run with a stub
  `RoomEditor` on the same rooms (each room's new types and draw count):
  the moblin lambda, other entities' slots, a gravestone with a warp and
  under six enemy types, every slot taken (redraws until a `None` pick),
  the evil shop's 0x77, two-value sets, empty intersections in slots 0
  and 1, a room with no enemy; then `ladx_enemizer_rooms` on a synthetic
  ROM whose 256 overworld rooms are planted (rooms 0x10 and 0x20 stored
  with LADXR's picks, logic-important 0x24 and enemy-free 0x30 untouched,
  four draws; a malformed room failing); then the group in the pipeline
  with a planted `enemizer.txt` (the seed record and draws through
  `rnd`'s sink, one segment with the rooms and the unit; nothing drawn or
  written on `enemies=default`; a file without 1 unit or no `rnd`
  failing) and the registry (right after `bosses`, ROM, no draw). It
  prints `test_enemizer: randomizeEnemies' picks against LADXR on planted
  rooms, the room loop and the group on a synthetic ROM (<c> checks)`.
  `test_warp_home` checks `ladx_warp_home_target` on planted rooms (an
  overworld warp's room and target, the first warp winning; a one-way
  entrance's room, the first and last names, a room over 0x100 and
  `force_inside` giving the indoor fallback 1, 0x10, 0xA3, 0x50, 0x7F;
  D2 not one-way; room 0x100 raising LADXR's `AssemblerException`
  message, measured at the pin; no warp `IndexError`),
  `ladx_warp_home_edit` on a planted background (every moved tile, the
  third option's five tiles, nothing else added; a missing row 0x9980
  raising `KeyError` `39298`), then the group in the pipeline on a
  synthetic ROM with a planted start house, background 0x0D and
  `warp-home.txt` (the operands from the warp, the unit and the edited
  background in one segment; `overworld=dungeonchain` and
  `entranceshuffle=madness` forcing the fallback, `simple` not; no warp
  and a missing row raising; an operand not after `ld a` (0x3E) or a file
  without 1 unit failing) and the registry (between `enemizer` and
  `title`, ROM, no draw). It prints `test_warp_home: warpHome's target,
  the S&Q background and the group on a synthetic ROM (<c> checks)`.
  `test_itempool` checks the item pool (`ladx/include/ladx_itempool.h`,
  `ItemPool` and `readItemPool`) through its headers: Python's dict
  semantics on hand-made sequences (`add` keeps a key's position and
  inserts a key at zero, `remove` deletes at exactly zero and keeps a key
  below it, a deleted key re-added goes to the end, `remove` of an absent
  key inserts it negative, `set` and `del`, an item outside the table
  refused); and `itempool=pain` on seed `000000a2` as LADXR ran it
  (`oracle_run.py`): `RUPEES_20` used up, so 7 rupee picks for 5 new
  items (`__randomizeRupees`'s re-draw on weights it never updates),
  `words_after` 26 and the counts LADXR's pool holds. It prints
  `test_itempool: dict semantics and a rupee re-draw ok (<c> checks)`.
  With `--fixtures` it replays every run of `oracle/item_pools.txt`
  (settings set, validated and their short string compared, then
  `ladx_randomize`): the `words_before`, `pool`, `words_after` and
  `placer_pool` lines rendered by the host's `ladx_run_text` equal, or,
  when `readItemPool` did not return, the outcome as `crash <type>
  <message>` or `error <message>`; it fails naming the line and both
  texts, else prints `test_itempool: 267 runs of oracle/item_pools.txt
  replayed line-identical (<k> crashes)`.
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
  `tests/alttp`'s): every `ladx/src`, `ladx/host` and `ladx/host/main` source compiled with
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

## KS span list

The C-knowable source map's spans as text, offsets, lengths and labels
only, never a byte value: written by `known_source.py --spans <file>`
(`spans(ks)`) and by C's `ladx_ks_render_spans`, so that the two builds
compare as text. Line 1 is `ladx-ks-spans 1`; then one `<start> <length>
<label>` line per span (lowercase hex, no `0x`), in `known_source.build`'s
order; then one `failure <label>` line per span left out because its
fields do not rebuild it. Labels: `<table> pointer <key>`, `<table> bank
<key>`, `<table> <key>` (an entry span), `boss zero <offset>` and `boss
item <room>`, a key in hex or its alt name. On the LADX ROM both give
6105 spans, 59,971 bytes and 186 failures (all texts' banks bytes). The
list derives from the ROM: it is written only under `tmp/`, never
committed.

## Pipeline

`ladx/include/ladx_pipeline.h`: generateRom's walk of `order.txt` in the
library, shared by `ladx_gen`, step 7's `ladx_assemble` and Phase 4.
`ladx_pipeline_run` takes the opened tables, the validated settings and
spot count (the key, "Table application"), the randomizer's result, the
parsed ROM (`LadxPipelineRom`: bytes, `LadxRomTables` and KS; NULL for
none), a registry of `C` groups (`{name, needs_rom, run}`, at most 32)
and a caller-begun encoder over the same ROM. It walks `order.txt`: at
a table unit's first row, its entry's records through
`ladx_enc_fixed_bank`, `_table` and `_pointer` (no origins: fresh write
identities); at a `C` row, the registered group of the row's group,
once, at its group's first `C` row, skipped when none is registered or
when it needs the ROM, none is given and it has no draw-only run; then
`ladx_enc_end`. A group's `draw`, when set, runs instead of its `run`
when it needs the ROM and none is given: its draws alone, with their
raise, writing nothing, so every later drawing group continues `rnd`
and `global` from where LADXR has them, with or without the ROM. A group's
`run` gets the encoder, the randomizer and the parsed ROM, and returns
1 on success. Statuses: `TABLES` (a key or record error, with its
`LadxTablesError`), `ENCODER` (the encoder's status), `GROUP` (a run
returned 0, the group named), `BAD_INPUT` (over 32 groups).
`ladx_pipeline_groups` is the library's registry, in which each `C`
group's sub-step adds its group, `needs_rom` true exactly for a
`rom-group` of `claims.txt`: today `bank3e`, `marin`, `hints`, `music`, `notext`, `goal`, `items`, `entrances`, `bosses`, `enemizer`, `warp-home`, `title` and `endscreen`, in LADXR's order,
`hints` alone needing no ROM and `marin`, `music` and `endscreen` alone with a `draw`
(`ladx/include/ladx_groups.h`).

A group's `run` takes the encoder and the pipeline itself (tables,
settings, spot count, randomizer, parsed ROM, `ctx`, and the `scratch`,
`values` and `origins` buffers of one record). `rnd` and `global` are the
randomizer's own generators, which every drawing group continues (a
drawing group fails when either is NULL); the caller's trace sink stays
attached, so `generateRom`'s draws follow the fill's. A run that returns
0 with `raised` set (LADXR's exception: a `LadxOutcome` crash) ends the
walk as `RAISED`, its group in `err.group` and `err.outcome` pointing at
`raised`; any other 0 is `GROUP`, a C failure. `ladx_pipeline_const(enc,
p, group)` writes the group's constants file ("Patch tables"): every
unit's entry for the key, in file order, each record's atoms resolved to
bytes (a reference or keep from `p->rom`) and written by
`ladx_enc_bank`, `_table` or `_pointer` with no origin; it returns 0 when
the group has no file, a unit has no entry, a reference falls outside
the ROM (or there is none), or the encoder refuses.
`ladx_pipeline_const_units(enc, p, group, first, count)` writes only
units `first` to `first + count - 1` (0-based, file order) the same way,
0 when they pass the file's end; `ladx_pipeline_const_unit_count` gives
a file's unit count, -1 without one. `ladx_pipeline_const_bank(p, group,
unit, offset, out, n)` resolves the bytes unit (0-based) writes at bank
offsets `offset` to `offset + n - 1` into `out` the same way, writing
nothing: 0 unless the unit writes every one of them, for a group to
splice an operand into an assembled instruction or move code, then
write it as one event.
`ladx_enc_entry_value` gives an entry's current bytes and origins,
`ladx_enc_bank_value` a bank byte's current value and origin (0 for a
reference without the ROM),
`ladx_enc_entry_pointer` whether it is an integer pointer (1), bytes (0)
or unknown (-1), and
`ladx_randomizer_seed` the seed's first 32 bytes.

**`title`** (`setRomInfo`): bank 0 offset 7 written `0x01` on `race`,
else `0x52`; then backgrounds 3 and 4, for each the tiles then the
attributes entry, as LADXR stores them: the entry's current value
decoded by `ladx_background.h` (`BackgroundEditor`, in the pipeline's
`background` scratch), edited by `ladx_title_edit` and re-encoded by
`store`'s greedy encoder, written by `ladx_enc_table` with no origin (the
oracle's `store` builds a plain `bytearray`; the encoder's rule gives the
references). The text is the seed's upper-case hex (lines of 16 at
0x9A01 and 0x9A21; attributes 0 there), or `Race`; the guards
(0x9820/0x9840/0x9860 + 0x09..0x13) become 0x7F, 0xA0/0xA1 and 0xA2; the
pattern takes five symbols `0xA0 + 4 * (sn % 14)` and palettes `0x08 |
(sn' % 8)` from `ladx_title_seed_number`, the hex seed's value (or a
non-hex text's MD5, `ladx_md5.h`) modulo 112^5, all the pattern reads.
LADXR's version text is empty (`ladxr.patch`), so it writes nothing.
Not ported, unreachable at the pin: `int()`'s other syntax, a string
`race`, and a seed over 32 bytes (the randomizer keeps no more).

**`marin`** (`reduceMessageLengths`): text 0x01 is
`getMarinStartingText(rnd)`: `rnd.choice` of the 480 lines of
`patches/marin.txt` at the pin (embedded in the library, stage
`patches.aesthetics:getMarinStartingText`), the line `strip()`ped and
`unicode_escape` decoded (each byte over 0x7F one character), every
`<marinLinesAmount>` made `480`, then every `<rnd[a,]b>` and then every
`<ord[a,]b>` (`re.sub`'s left-to-right scan, `a` 1 when absent) drawn as
`str(random.randrange(a, b + 1))` from the global generator (stage
`utils:randomNumber`, also for ordinals), an ordinal suffixed by its last
digit (`st`, `nd`, `rd`, else `th`), then `formatText` (`ladx_text.h`:
`_NAMES` whole, ASCII, `_CHARACTERS` in order, the 16-column wrap,
`center`, `ask`). Then the 59 shortened texts in LADXR's order, each by
`formatText`, 0xEB and 0xEC as 0xEA's current value with its origins.
Every computed text is written by `ladx_enc_table` with no origin. A
formatted text holding a character over 0x7F raises `UnicodeEncodeError`
with CPython's message (lines 387 and 415), a `{NAME}` outside `_NAMES`
`KeyError`: the group sets `raised` and the run ends `RAISED`. Not
ported (unreachable at the pin, a C failure): `\x`, octal, `\u`, `\U`
and `\N` escapes, numbers over 18 digits, an empty range, `str.format`'s
positional fields, conversions and specs. The refusal seed: the first
baseline seed `derive("ladx-marin-refusal", k)` (the first 16 bytes of
SHA-256 of `ladx-marin-refusal <k>`) whose fill is ok and whose starting
text raises, found by the C port with no ROM, is `k = 166`,
`2ba680f9da28d36d2747c7017b6a6f94` (line 387); LADXR at the pin
(`oracle_run.py --rom`) gives `outcome crash UnicodeEncodeError 'ascii'
codec can't encode characters in position 54-55: ordinal not in
range(128)`, and `ladx_gen --rom`'s `run.txt` equals the oracle's but for
its `rom none` and `spoiler none` lines; `test_marin` replays it. Without
the ROM, `marin`'s `draw` runs `getMarinStartingText` alone (the 59
shortened texts draw nothing and cannot raise), so a ROM-free run raises
on the same seed with the same outcome.

**`music`** (`patches.music.randomizeMusic`, `ladx_music_randomize`), only
when `music` is `random` (else nothing drawn or written; `off` and
`shifted` are the `music-fixed` table group): for each of the 25
`_OVERWORLD_MUSIC_REGIONS` in order, `rnd.choice` of the 63
`_LOOPING_MUSIC` tracks written to every room of the region (bank 0x02
0x00..0xFF, every room once), then 32 more choices for bank 0x02
0x100..0x11F (stage `patches.music:randomizeMusic`). The 0x120 bytes
are written by one `ladx_enc_bank` with no origin: each byte is written
once, so its encoding depends only on its own offset's state, as in
LADXR's byte-by-byte order. The library holds LADXR's lists as runs of
consecutive tracks and as rectangles of the 16x16 overworld (room `16y +
x`), whose byte lists match ROM runs. It needs the ROM: its region runs repeat a track 4 or
more times, which KS-match turns into copies of KS runs (`music-random`
and `sample-05`'s goldens), so it is a `rom-group` with its trace lines.
Its `draw` makes the 57 draws without the ROM, writing nothing, so every
later draw keeps its number in `run.txt` ROM-free.

**`hints`** (`hints.addHints`, `ladx_hints_add`): the placed logic's
spots in `iteminfo_list` order, as (`name_id`, `item`, its
`checkMetadata` area), filtered to the 25 `hint_items` and stably sorted
by `name_id` (byte order, ties in list order); the 37 `hint_text_ids`
`rnd.shuffle`d (stage `hints:addHints`, as every draw of the group); for
each text id in that order, while spots remain, `rnd.randint(0, len -
1)` pops a spot and `rnd.choice` of the five templates is formatted with
`"{<ITEM>}"` and the area, else `rnd.choice` of the templates then of
the eight useless pairs (the receiver before the arguments); each text
`formatText`ted (`{<ITEM>}` resolved by `_NAMES`); then texts 0x200 to
0x20A, even ids, `formatText("Read this book?", ask="YES  NO")`. Every
text is written by `ladx_enc_table` with no origin; the segment's net
texts come out by id. It needs no ROM: its Evidence lists none and its
goldens hold no source reference, so without the ROM the encoder's
inherit-on-equal, KS-match and fragment rule have nothing to find, as
they find nothing with it. A raise of `formatText` ends the walk
`RAISED`; none is reachable on LADXR's names and areas.

**`endscreen`** (`patches.endscreen.updateEndScreen`): unit 0 of
`data/ladx/endscreen.txt` (`rom.patch`'s writes: the trampoline at bank
0 0x391D, five bank 0x17 patches, the loader at bank 0x3F 0x0200), then
`random.choice` of LADXR's 24 sorted cats from the global generator
(`ladx_endscreen_draw`, stage `patches.endscreen:updateEndScreen`), then
unit `1 + k` for the drawn index `k`, the cat's 6,184 bytes at bank 0x3F
0x0400; every byte computed, with no origin. LADXR draws after its
patches; the draw reads no ROM byte and no two writes share a position,
so the segment is the same. It needs the ROM (the file's references,
and its goldens' KS copies) and fails unless the file has 25 units; its
`draw` makes the one draw without the ROM, so a ROM-free `run.txt`
keeps the line.

**`notext`** (`patches.aesthetics.noText`), only when `textmode` is
`none` (else nothing written): for each texts index key from 0 to the
table's count less one, 0x217 to 0x21A skipped, an entry whose current
value is an integer pointer skipped (LADXR's `isinstance(..., int)` on
the current table, so a table group's pointer counts), every other
written by `ladx_enc_table` as its current value's last byte with that
byte's origin (`[-1:]`; an empty value stays empty), even when already
one byte long. The origin makes the encoding: a source terminator stays
a copy, an earlier group's computed terminator inherits its encoding. It
needs the ROM (a source terminator's value), draws nothing and has no
`draw`, so a ROM-free run skips it; it fails on an unknown entry. It
reads and overwrites the texts of `bank3e`, `marin`, `hints` and the
`fixed` table group, so it runs after them (`order.txt`'s `C notext`
row, after `options`' second `fastText`).

**`goal`** (`patches.maze.patchMaze`, `patches.goal.setSpecificInstruments`
and `setRequiredInstrumentCount`, `ladx_group_goal`), on the world
setup's goal (step 4's port), with `data/ladx/goal.txt` ("Patch
tables", 265 units): for `vanilla` and `instruments` (the drawn counts of
`open-4`, `5-8` and `goalcount=random` included) and for `open` (the
count -1: `goal=open`, or `goal=instruments` with `goalcount=random`
drawing -1, LADXR's `setRequiredInstrumentCount(rom, -1)` row, `C`
because the world setup's goal is drawn), text 0x1A3
`formatText("You need %d instruments")` (Python's `%d`, so `-1`), then,
below 8, unit `goal.<c + 3>` (`c` negative as -1); for `specific`, text
0x1A3 `formatText("You need:" + "\n{INSTRUMENT<d>}"...)` in the world
setup's drawn order, then the unit of the instruments' mask; for `maze`,
unit `goal.1`, then for each of the 22 signs in order the room `x // 10
| (y // 8) << 4` loaded ("Room editor"), the sign entity `(x % 10, y %
8, 0x4C)` inserted first, stored, bank 0x0C 0x3800 + room its arrow
(`directions[idx]`, 0x40 for the last sign) and 0x3900 + room its index,
each a byte with no origin; then room 6 loaded, its 0xDE entities
removed, stored. LADXR zeroes the sign tables, places the signs, stores
room 6, then patches; the unit's other writes share no position with the
signs and rooms, and its zeroes come before the signs in both orders, so
every event sees the same state before and the segment's net effect is
the same. `seashells` writes nothing (the `goal-fixed` row), nor does
the cut bingo (unsupported settings). Text writes have no origin.
It needs the ROM (the file's references, the rooms, the KS copies in
its goldens: `rom-group goal`), draws nothing (the world setup drew)
and has no `draw`. It fails on a file without 265 units, a malformed
room or a goal it cannot write.

**`items`** (`patches.chest.setMultiChest` and every `spot.patch(rom,
spot.item)` of `iteminfo_list`, `ladx_group_items`), with
`data/ladx/items.txt` ("Patch tables", 12 units): first the multichest
byte, bank 0x14 0x2F2 + 0x560, `CHEST_ITEMS` of the world setup's
multichest; then each spot in order by its kind, as `locations/` at the
pin. `KeyLocation` writes nothing. `Chest`: bank 0x14 room + 0x560; for
room 0x1B6 also unit 3's two bytes at bank 3 0x145D with its operand made
the id (`ld a, <id>`). `DungeonChest`: the **reduction** first (`KEYn`,
`NIGHTMARE_KEYn`, `MAPn`, `COMPASSn`, `STONE_BEAKn` with `n` the spot's
dungeon become their generic item), then `Chest`. `DroppedKey` and its
subclasses (heart pieces and containers, instruments, seashells, the
mansion, songs, toadstool, gold leaves, slime, bird and face keys,
hookshot, fishing): the reduction unless room 0x166 or 0x223, the id (a
gel disguised as 0x51 + room % 6), bank 0x3E 0x3800 + room, and for
rooms 0x169, 0x166, 0x223 and 0x092 the same byte at 0x3800 + 0x17C,
0x1FF, 0x2E8 and 0x0DC. `AnglerKey`: `DroppedKey`, then bank 0x3E
0x3800 + 0x1F8 and 0x3300 + 0x1F8 written as the current bytes of 0x0CE
with their origins (the boss item a source copy while unwritten).
`BeachSword`: `DroppedKey`, room 0xF2 loaded from the current state
("Room editor"), its 0x31 entities removed, `(5, 5, 0x35)` added,
stored, then unit 1 (the like-like patches). `StartItem`: with `bowwow`
other than `normal` the item is BowWow and text 0xC8 `Got BowWow!`;
unless the item is the shield, unit 2 (Link's sprite); then
`DroppedKey`. `KeyHole`: the id (gel disguised) at bank 0x3E 0x3B16 +
room; nothing more for its default reward; else unit 6 (the hook) when
bank 2 0x2FA9 holds 0xF0 now, then for room 0x2C3 unit 7, for 0x0D3,
0x0B5, 0x02B, 0x08C and 0x00E their unit's 12-byte handler (read at
slot 0, bank 2 0x3FEF) written at the hook's current target (bank 2
0x2FAA, little-endian) less 12, then the target less 12 written there;
another room raises `RuntimeError`. `OwlStatue`: the reduction, bank
0x3E 0x3B16 + room. `MadBatter`: bank 0x18 0x0F90 + (room & 0x0F).
`TunicFairy`: bank 0x36 0x11BF + index. `SeashellMansionBonus`: bank
0x3E 0x3E30 + index. `ShopItem` (no shopsanity in tier 1): index 0 at
bank 0x3E 0x3800 + room with text 0x030 `Deluxe {<ITEM>} 200
{RUPEES}!`, index 1 at bank 0x14 room + 0x560 with text 0x02C
`{<ITEM>} Only 980 {RUPEES}!`, both `ask="Buy  No Way"`.
`TradeSequenceItem`: the id at bank 0x3E 0x3B16 + room; when not its
default item, its default's texts (0x181 to 0x183, 0x182 the single
byte 0xFF; 0x1CA; 0x1CF; 0x173; 0x168; 0x135; 0x15D) with LADXR's
strings. `Witch`: nothing when unshuffled (one option); else
`rom.patch`'s one byte at bank 5 0x08D5 (written over 0x09, skipped
over the new byte, else LADXR's `AssertionError`). `BoomerangGuy`:
with `boomerang=trade`, unit 4's four instructions (bank 0x19 0x063F,
0x06C1, 0x06FC, 0x0710) with their operand made `INVENTORY_MAP`'s id,
texts 0x222, 0x224 to 0x226; otherwise text 0x221, unit 5 (the gift
code), the item at bank 0x19 0x072B by `rom.patch` over 0x00, text
0x225. Texts are `formatText` with LADXR's strings and no origin, every
computed byte has no origin, and an assembled patch is written as one
event, as `rom.patch` writes one slice. An item `CHEST_ITEMS` (or
`INVENTORY_MAP` for the trade) lacks raises `KeyError '<NAME>'`, and a
`formatText` raise propagates, each ending the walk `RAISED`. The
assembled patches' old bytes are not re-checked (they are LADXR's ASM,
not in the file); none fails after the groups before it on the source
ROM. It runs once, at `setMultiChest`'s row, before `entrances`' row,
where LADXR has `changeEntrances` between the multichest and the spots:
the inventory shows neither group depending on the other, they share
no position (0 shared bank offsets or table keys in the 128 goldens),
`entrances` reads only the rooms it stores and `items` edits only room
0xF2, which no entrance names, and claims are projections; the
`entrances` claim, made with `items` claimed, confirms it. It needs the
ROM (`rom-group items`), draws nothing and has no `draw`.

**`entrances`** (`patches.entrances.changeEntrances(rom,
world_setup.entrance_mapping)`, `ladx_group_entrances`), on the identity
mapping of `WorldSetup.__init__` (every key to its `:inside`, then every
`:inside` to its key, that dict order). `ladx_entrances_change` takes the
entrance table and the mapping (`2 × count` target keys, key `k` the
entrance `k`, `count + k` its `:inside`), so its unit tests drive other
mappings. `ladx_entrance_info` is `entranceInfo.py`'s `ENTRANCE_INFO` at
the pin, 104 entries in its order, with the fields the call reads (name,
room, alt room, index, target, instrument room as an index or an alt
key), checked by the SHA-1 of its rendering against LADXR's. For each
entrance in order, through the room editor on the current state (the
maze's edits included): the entrance's alt room, else its room, gives
warp `index` (`None` and `"all"` read as 0; past the last warp
`IndexError: list index out of range`), the way in, whose room must be
the target (`AssertionError: <key> != <room as %03x>`); the target room
gives the way out, the last of its warps to the entrance's room
(`AssertionError: Missing warp to outdoor on <key>` when none). The
entrance's rooms are its room then its alt room; its `:inside`'s rooms
the target then the instrument room. Then, per key in mapping order, each
of its rooms gets the change "warps to the key's vanilla room become the
target key's warp", rooms in first-insertion order, a later change to the
same room and target replacing the earlier, an index room and an alt key
never equal. Last, each room in that order is loaded, every warp whose
room has a change replaced by a copy of that warp (two doors to one house
both take the later key's warp, so identity still rewrites a second
door), and stored: objects, entities for an index room, overlay (about
220 rooms per run). A room that does not load or store fails the group
(`GROUP`); a raise ends the walk `RAISED`. At most 128 entrances, its
state on the stack (no allocation). It needs the ROM (`rom-group
entrances`), draws nothing and has no `draw`.

**`bosses`** (`patches.enemies.changeBosses(rom, world_setup.boss_mapping)`
then `changeMiniBosses(rom, world_setup.miniboss_mapping)`,
`ladx_group_bosses`), on the mappings `WorldSetup.randomize` drew in the
fill (identity unless `boss` or `miniboss` is set), with
`data/ladx/bosses.txt` ("Patch tables", 3 units). `ladx_bosses_change`
takes the instructions (`LadxBossesAsm`, read with
`ladx_pipeline_const_bank`) and the mapping, `ladx_minibosses_change`
the miniboss mapping, so unit tests drive any mapping. `changeBosses`:
each byte of bank 0x14 0x04E0..0x051F now 0x00 but 0x04F9 and 0x0512
made 0xFF; the genie patch; the eagle check `cp` with the first dungeon
mapped to 6 (0xFF when none or 8). Then per dungeon `d` whose target
differs, in order: the **fish** (target 3) from D7 makes room 0x22E's
warps type 2, map 3, room 0x1EF, (24, 16) and 0x1EF's last object a warp
back (1, 6, 0x22E, 24, 16); from another dungeon, the event flag bank
0x14 `room - 0x100` 0x2A, the cleaned boss room with stairs (4, 4, 0xBE)
and a warp (2, 3, 0x1EF, 24, 16), and 0x1EF's last object a warp back
(1, `d` or 0xFF, the room, 72, 80); then the fish's `ld hl` at bank 3
0x1A0F with the boss room's status flag (0xD800 + room, 0xDDE0 - 0x300 +
room for the color dungeon; 0xDA2E from D7), and the boss items. The
**eagle** (6): flag 0x2A, `ld hl` at bank 3 0x1A04, the cleaned room with
the stairs objects and a warp (2, 6, 0x2F8, 72, 80), tileset bank 0x20
0x2EB3 + `room - 0x100` made 0, 0x2F8's last object the warp back, the
items. Any **other**: flag 0x21, the cleaned room with the target's
entity; the slime eel (4) sets that tileset to 0x06 and, in D2, removes
(3, 3), (6, 3) and rows 0 and 1; the facade (5) writes its three `cp`
patches (a second facade's are LADXR's already-applied skip: the same
bytes). The **cleaned room** (`getCleanBossRoom`) drops warps, stairs
0xBE and lava 0x06, makes pits 0x1C, 0x1E, 0x1F and doors 0xF5 into
0x01, 0xAF, 0xB0 and 0xF1, keeps D4's first two objects and adds four
blocks, empties the entities; for D7 it first edits bank 0x14's map
bytes, room 0x22E (last object a warp to 0x223, (4, 0) removed), its
event flag 0x12E, room 0x22C ((0, 7) and (2, 7) made 0x03), then masks
the floor to its low nibble and adds a door, stairs and a warp to 0x22E.
Those edits precede loading room 0x223, which they do not touch, so the
single room scratch suffices. The **boss items**, bank 0x3E 0x3800 and
0x3300 + room, are copied to + 0x1FF (fish) or + 0x2E8 (eagle) through
`fixDungeonItem` (a generic map, compass, key, nightmare key or beak made
the dungeon's own, `KeyError '<NAME>9'` for the color dungeon), with the
read byte's origin when unchanged, none when mapped. `changeMiniBosses`:
unit `bosses.1`, then per key in dict order (0 to 7, c1, c2, moblin
cave, armos temple) its room keeps only its warp entities 0x61 and gets
the miniboss's; room 0x228 loses objects at x 3..6, y 0..2 but for Grim
Creeper; Ghoma outside D5 writes bank 3 0x034F 0xD5; Cue Ball adds its
platform; Blaino in a room without a warp adds one to its dungeon's
entrance (the start house by default); Dodongo drops floor tiles 0xDF;
Rolling Bones in D3 patches bank 3 0x0372 0x84 to 0x95 (skipped when
already 0x95, else the group fails); each room is stored. LADXR's
`NIGHTMARE_*` targets are unreachable in tier 1 and not ported. Every
computed byte, object and entity has no origin. A room that does not
load or store, or a mapping out of range, fails the group; a raise ends
the walk `RAISED`. It needs the ROM (`rom-group bosses`), draws nothing
and has no `draw`.

**`enemizer`** (`patches.enemies.randomizeEnemies(rom, seed)`,
`ladx_group_enemizer`), only when `enemies` is `overworld` (else nothing
drawn or written), with `data/ladx/enemizer.txt` ("Patch tables", 1
unit). Its generator is its own `Random(seed)`, the run's seed bytes
(`ladx_randomizer_seed`), labelled `rnd` like every generator but the
global one (the oracle's tracer), traced through `rnd`'s sink with stage
`patches.enemies:randomizeEnemies`: its `seed` record, then its draws,
between `bosses`' row and `endscreen`'s. The sprite slots come from
`ladx_sprite_data.h`, `entityData.SPRITE_DATA` at the pin (lambdas
resolved on the room number; with `evilshop` set, entity 0x77 answers as
0x4D, `addEvilShop`'s mutation, which runs earlier in `generateRom`),
checked by `test_sprite_data` against LADXR's own table. For rooms 0 to
0xFF but 0x24, 0xE2, 0xF2, 0x5A and 0x58, in order, the room is loaded
from the encoder's state ("Room editor"); `ladx_enemizer_pick` scans its
entities (an `ENEMY_TYPES` member is an enemy, any other entity with
sprite data intersects each of its slots' value sets, a slot starting
unset; an empty set stays empty), returns when there is no enemy (the
room is not stored), sets slot 3 to {0x82} when a non-warp object is a
pushable gravestone 0xC5, then for each enemy type in ascending order
draws `rnd.choice` over the 43 sorted `ENEMY_TYPES` until the pick's
data is `None` or none of its set slots is disjoint from its values,
meets the pick's slots, and rewrites every entity of that type; the room
is stored (objects, entities, overlay). Then the file's unit. LADXR's
`while True` ends: `0x1B` and `0x9B` have `None` data. Entities have no
origin. It needs the ROM (the rooms; `rom-group enemizer`) and has no
`draw`: how many draws it makes depends on every overworld room's
entities, read from the ROM, so a ROM-free run of an `enemies` case lacks
its trace lines and numbers `endscreen`'s draw lower, which only
`e2e-rom` claims. It fails on a room that does not load or store, a file
without 1 unit, or no `rnd`.

**`warp-home`** (`patches.core.warpHome(rom, force_inside)`,
`ladx_group_warp_home`), after `entrances` (the start house it stored),
with `data/ladx/warp-home.txt` ("Patch tables", 1 unit). The byte before
each of the handler's five operands must be the unit's `ld a` opcode
0x3E, else the group fails. The start house 0x2A3 is loaded from the
encoder's state ("Room editor"); `ladx_warp_home_target` takes its first
warp's room and target (type 0, map 0), or the indoor fallback (type 1,
map 0x10, room 0xA3, x 0x50, y 0x7F) when the room is one of the 15
one-way entrances' overworld rooms (`ENTRANCE_INFO`'s `d0`, `d1`, `d3`,
`d4`, `d6`, `d8`, `animal_cave`, `right_fairy`, `rooster_grave`,
`prairie_left_cave2`, `prairie_left_fairy`, `armos_fairy`,
`boomerang_cave`, `madbatter_taltal`, `forest_madbatter`), is over
0x100, or `force_inside` (`overworld=dungeonchain` or `entranceshuffle`
`chaos`, `insane` or `madness`; never in tier 1). Then the unit, the five
operands with no origin, and background 0x0D's tiles decoded from their
current value ("Pipeline"'s `p->background`, as `title`),
`ladx_warp_home_edit`ed (for columns 2..17, rows 0x9960..0x99C0 take the
rows above them, 0x98E0 moving to 0x9960; then 0xCE, 0xCF, 0xC4, 0x7F,
0x7F at 0x996A) and re-encoded with no origin. LADXR's raises are
outcomes: no warp `IndexError`, a room of exactly 0x100 the assembler's
`AssemblerException` (`([NUMBER:256:21], '8 bit value out of range')`),
an absent tile read `KeyError` with its decimal address. It needs the
ROM (`rom-group warp-home`), draws nothing and has no `draw`.

**`bank3e`** (`addBank3E`): its constants, then text 0xBE written as
entry 0x111's current value with its origins, then `seed[:0x20]` at bank
0x3E 0x2F00, then, unless `race`, the short settings string's length
byte at 0x2F20 and its bytes from 0x2F21 (over 255 bytes fails, as
LADXR's `to_bytes` raises). LADXR writes the texts first; the order does
not matter, since no two writes of the call share a position.

## Room editor

`ladx/include/ladx_room.h`: `roomEditor.py`'s `RoomEditor` at the pin,
for every `C` group that edits rooms (`goal`, `items`, `entrances`,
`bosses`, `enemizer`, `warp-home`, and later `sprites`). Caller-owned (`LadxRoom`,
about 10 KB; the pipeline holds one as `p->room`), no allocation.
`ladx_room_load(r, enc, room, alt)` reads, from the encoder's current
state, an index room (0..0x31F: objects from `rooms_overworld_top`
below 0x80, `rooms_overworld_bottom` below 0x100, `rooms_indoor_a`
below 0x200, `rooms_indoor_b` below 0x300, else `rooms_color_dungeon`,
at the index less its base; entities from `entities` at the room) or an
alt key (objects only, from the first of the top, bottom and indoor A
tables that holds it). It parses as LADXR does: the animation id, the
floor object, then objects until 0xFE (a first byte's high nibble 0x8
horizontal, 0xC vertical, each with its count; 0xE a warp of 5 bytes,
its room plus 0x300 for map 0xFF, 0x200 for maps 0x06..0x19, else 0x100
when its type is above 0; else a single object), entities in pairs until
0xFF, each entry ending at its terminator (`MALFORMED` otherwise, and for
an integer entry; `NO_ROM` for a source byte without a ROM). The overlay
is the 80 bytes at bank 0x26 `room * 80` below 0xCC, at bank 0x27
`(room - 0xCC) * 80` below 0x100, at bank 0x27 0x1040, 0x1090, 0x10E0,
0x1130, 0x1180 and 0x11D0 for `Alt06`, `Alt0E`, `Alt1B`, `Alt2B`,
`Alt79` and `Alt8C`, else none; each byte with its current value and
origin (`ladx_enc_bank_value`). The floor's and each object's type's
origins are kept, as LADXR's parsed ints carry them. The helpers are
LADXR's (insert, add, remove and test entities; test, change, remove and
move objects, a warp never matching an x, y or type; `updateOverlay`),
an overlay index Python refuses giving `RANGE`, a negative one wrapping;
`ladx_room_warp` builds an `ObjectWarp`, its room offset by its map as
LADXR's constructor does.
`ladx_room_store(r, enc, group, new_room, new_alt)` (`-1, NULL`: its own
room) writes, in LADXR's order, the objects entry (the header, each
object's export, 0xFE), for an index room below 0x100 the set tileset
(bank 0x3F 0x3F00 + room), attribset (bank 0x1A 0x2476 + room, then
0x1E76 + 2 room, low then high) and palette (bank 0x21 0x02EF + room)
bytes, above it the tileset at bank 0x20 0x2EB3 + room - 0x100, then
the entities entry (pairs, 0xFF) and the overlay where the room has one.
The entries' and extra bytes have no origin (LADXR builds plain
`bytearray`s from ints), the overlay carries its bytes' origins (LADXR
slices the tracked bank), so the encoder's carried origins and
inherit-on-equal give LADXR's tokens. A field outside its byte, or an
overlay to store and none loaded, is `RANGE`. Not ported (no tier-1
caller): `loadFromJson`, `buildObjectList`, `getTileArray`.

## Generator `ladx_gen`

`ladx/host/main/ladx_gen.c`, built `-O2` (library and host objects in
`build/gen/`) into `tests/ladx/build/ladx_gen` by `make -C tests/ladx
e2e` and `e2e-combos`.

- `ladx_gen <case_dir> <out_dir>` reads `<case_dir>/config.txt` (one
  `key=value` per line through `ladx_settings_set`, then `validate`) and
  `<case_dir>/seed` (32 lowercase hex digits, one optional newline),
  refuses unsupported settings (`ladx_settings_supported`), runs
  `ladx_randomize` (`ladx/include/ladx_randomizer.h`, `Randomizer.__init__`
  to the end of `item_placer.run`: the seeding, the `race` draw,
  `WorldSetup.randomize` for tier 1 with `buildMaze`, the global
  generator's copy, the logic built on that world setup, `ItemPool`
  with `__randomizeRupees` (`ladx/include/ladx_itempool.h`),
  `readItemPool`, the placer's `addItem` calls and `RandomItemPlacer.run`
  with `placeSpecificItem` and `__placeItem`) with the
  run-text trace sink, runs the pipeline below when `--tables` is given,
  and writes `<out_dir>/run.txt` in the oracle's
  format, already in its fill projection's form (below): `seed`,
  `settings`, the sections of the stages reached (the 13 `ws` lines once
  `WorldSetup.randomize` returned, none when it raised; `words_before`,
  `pool` and `words_after` once `ItemPool` returned, `placer_pool` once
  `readItemPool` returned, one `place` line per spot of `iteminfo_list`
  once `run` returned), the trace, then the `outcome` line when no
  `place` line was written (LADXR's error or exception; an ok run's
  `outcome ok` belongs to the whole run). The trace holds the draws of
  the ported `C` groups that ran, numbered on from the fill's; a group
  that raised (`RAISED`, "Pipeline") ends `run.txt` with its `outcome
  crash <type> <message>` line and leaves no `rom_writes.txt` (LADXR
  writes no ROM), exit 0.
- `ladx_gen --tables <dir> <case_dir> <out_dir>` does the same, then,
  after an ok fill (`ladx_randomizer_placement` not NULL), writes
  `<out_dir>/rom_writes.txt`: the tables of `<dir>` (`order.txt` and every
  other `.txt` file, read whole, 2 MiB in all at most; today 1,392,187
  bytes) opened, the key of the validated settings and the placed logic's
  spot count, the library's pipeline ("Pipeline") walks `order.txt`:
  each selected entry's records through the encoder's `fixed` calls
  under its unit's group, and at `C` rows the library's ported groups
  (without `--rom` `hints` and `marin`'s, `music`'s and `endscreen`'s draws, since `bank3e`, `marin`, `music`, `notext`, `goal`, `title` and `endscreen` need the ROM, so consecutive
  entries of one group merge into one segment across the `C` rows). The file is a valid version-1 write log, no `I`
  line; the version and `tables` lines are written even when nothing
  else is. Without `--rom` the encoder has no ROM, KS or parsed tables
  and only the groups that need no ROM run, with the draws of those that have a `draw`. An
  error outcome of the fill writes no `rom_writes.txt`. A tables open,
  key or walk error, or a log overflow, exits 2 with `ladx_gen: tables:
  <status> <file>:<line>` or `ladx_gen: tables: unit <id>: no entry for
  <values>` (or `write log overflow`), the partial `rom_writes.txt`
  removed. Opening the tables and walking them through the encoder adds
  about 12 ms per run on the host (slowest case, `sample-05`); `--rom`'s
  read, check, parse and KS about 30 ms more.
- `ladx_gen --tables <dir> --rom <file> <case_dir> <out_dir>` does the
  same with the source ROM: the file is read whole and refused unless it
  is 1 MiB with SHA-1 `d90ac17e…` (`ladx_rom_check`; else `ladx_gen:
  --rom: not the LADX English 1.0 ROM (SHA-1 <hex>)`, the whole file's
  digest, exit 3, as `rom-check`'s refused base; a missing file exits 2),
  its tables parsed (`ladx_rom_tables_parse`), KS built (`ladx_ks_build`),
  and all three given to the encoder and to every ported group, so the
  `rom-group`s run too.
- `ladx_gen --rom <file> --ks-spans <out>` checks and parses the ROM the
  same way and writes C's KS span list ("KS span list") to `<out>`,
  nothing else; exit 0, else as above.
- `ladx_gen --time <case_dir>` runs the same with no sink and prints only
  the run's elapsed microseconds (`CLOCK_MONOTONIC`), no file.
- `ladx_gen --project <rom_writes.txt> <out_dir>` reads the log whole (4
  MiB at most) and writes `<out_dir>/<group>.txt` for every group of the
  log, its projection (`ladx_wlog_project`, "Table application"), nothing
  else. A malformed log exits 2 with `ladx_gen: <file>:<line>: malformed
  write log`, nothing written; over 64 groups, or a group named `.` or
  `..`, exits 2.
- Exit 0 with the file written, whatever LADXR's outcome (it is in the
  file); 2 on a usage error, a malformed or missing input, unsupported
  settings (`ladx_gen: unsupported settings: <reason id>`) or a broken run
  (`LADX_RANDOMIZE_BROKEN`, a C bug); 1 on an I/O failure. Messages go to
  stderr, prefixed `ladx_gen: `.

`ladx_gen` writes the file with the host layer's
`ladx_run_text_write_run` (`ladx/host/ladx_run_text.h`), which
`test_fill --fixtures` uses too.

## Stage projection

A run's stage projection keeps the lines of `run.txt` a stage claims, in
file order (`run_e2e.py`'s `project`, the one definition). Stages, in
order: `init`, `ws`, `pool`, `fill`.

- Sections: `init` keeps `seed` and `settings`; `ws` adds the `ws` lines;
  `pool` adds `words_before`, `pool`, `words_after` and `placer_pool`;
  `fill` adds the `place` lines.
- Trace: the longest prefix of the `trace` lines whose stage (the 4th
  field) is in the stage's set: `init` `randomizer:Randomizer.__init__`;
  `ws` adds `worldSetup:WorldSetup.randomize` and `patches.maze:buildMaze`;
  `pool` adds `itempool:ItemPool.__randomizeRupees`; `fill` adds
  `randomizer:RandomItemPlacer.run`, `.placeSpecificItem` and
  `.__placeItem`. A trace line of the `fill` set after the `fill`
  prefix makes the file malformed.
- Outcome: the `outcome` line is kept at a stage exactly when that
  stage's completion marker is absent (`init`: no `ws` line; `ws`: no
  `words_before`; `pool`: no `placer_pool`; `fill`: no `place` line), i.e.
  LADXR stopped in that stage or before it. An ok golden's `outcome ok`
  belongs to the whole run and is never claimed by these stages.
- Everything else (`rom_sha1`, `spoiler_sha1`, `rom none`, `spoiler none`,
  the trace of `generateRom` and the spoiler) is in no stage. Any other
  line kind, a second `outcome` or a trace line with fewer than 5 fields
  is malformed.
- The projection is idempotent; the driver projects C's `run.txt` too,
  so lines C writes beyond the claimed stage never fail a case.

## E2E driver and claims

`tests/ladx/claims.txt` holds one `stage <name>` line (one of the four
stages) and zero or more `group <name>` and `rom-group <name>` lines
(blank lines and `#` comments skipped), all applying to every case of
the directory run, cases and combos alike. A group must be in the
inventory's Groups table (`--inventory`, default
`notes/ladx-inventory.md`, its Side and Evidence columns read through
`inventory_check.parse_named` only when a group is claimed), and not
`spoiler` (its claim is `spoiler.json`): a `group` line's with side
`table` or `C`, a `rom-group` line's with side `C`. A `rom-group` is a
group that needs the ROM to encode: `e2e` and `e2e-combos` skip it (the
header line names it, `, rom-groups <names> skipped`) and `e2e-rom`
("ROM-present e2e") claims it. Two refusals keep a ROM-only claim out of
a `group` line, in every mode: its group's Evidence lists `rom` (an
item exactly `rom`), or it is a `C` group and a golden of the directory
run holds a source reference (a keep or a copy, inside KS or not) in one
of its records (`run_e2e: group <g> holds source references in <case>'s
golden: it needs the ROM to encode, claim it as a rom-group`). An
unknown stage, a second `stage` line, no `stage` line, an unknown,
repeated or `spoiler` group, a group both `group` and `rom-group`, a
`rom-group` of another side, either refusal or any other line exits 2.
Claimed now: stage `fill`, groups `fixed`, `frame-counter`,
`goal-fixed`, `music-fixed` and `options` (the five table groups) and
`hints`; `rom-group bank3e`, `rom-group endscreen`, `rom-group marin`,
`rom-group music`, `rom-group notext`, `rom-group goal` and `rom-group title`.

A claimed `C` group that draws also claims its `trace` lines: those of
`run.txt` whose stage (the 4th field) is one of its LADXR frames
(`run_e2e.py`'s `GROUP_TRACE`; today `marin`:
`patches.aesthetics:getMarinStartingText` and `utils:randomNumber`;
`hints`: `hints:addHints`; `music`: `patches.music:randomizeMusic`;
`enemizer`: `patches.enemies:randomizeEnemies`;
`endscreen`: `patches.endscreen:updateEndScreen`),
whole (number, generator, arguments, result, words) and in order,
golden against C, wherever they fall in the trace. The golden's number
is first lowered by the count of its trace lines before it drawn in a
frame C does not port (in neither the fill stages nor `GROUP_TRACE`;
none at the pin since `enemizer` is ported, the rule kept for any later
one): the trace projected onto the ported frames, as C numbers it. A
draw of a ported frame is never skipped, so an extra or missing draw of
C still shifts the number. They are claimed with
the group: by `e2e` for a `group` line, by `e2e-rom` for a `rom-group`
line; a divergence is reported as section `trace <group>`, unredacted
(trace lines are draws, never ROM bytes).

`python3 -I -B tests/ladx/run_e2e.py [--cases <dir>] [--claims <file>]
[--gen <binary>] [--tables <dir>] [--inventory <file>] [<case>...]`
(defaults `tests/ladx/cases`, `tests/ladx/claims.txt`,
`tests/ladx/build/ladx_gen`, `data/ladx`, `notes/ladx-inventory.md`)
projects every golden first: its `run.txt` at the stage and, when a
group is claimed, its `rom_writes.txt` (a missing or malformed golden,
an unknown named case: exit 2; with no group claimed `rom_writes.txt` is
neither read nor required). A group's projection is
`write_log.py`'s (`parse`, then the group's segments replayed and
rendered as one segment, header lines included: "Projection and
claims"), so segmentation, other groups and `I` lines never change a
verdict; a group absent from a log projects to the two header lines. It
prints `e2e: claims stage <stage>, groups <g> ...` (`groups none` when
none), then runs `<gen> --tables <dir> <case_dir> <out_dir>` per case
(the named ones only when given), sorted by name, in parallel, each in a
fresh temporary directory, 120 s each, and prints per case `PASS <case>
(<stage> + <n> groups)` (`PASS <case> (<stage>)` with no group) or
`DIVERGENT <case>` followed by indented detail lines: for the `run.txt`
claim, then each claimed group's trace lines, then each divergent group's
write-log projection, groups in `claims.txt` order, the first
differing projection line as `<section or group>: line <n>:` (`n`
counted in the projection), `golden: <line>`, `c: <line>` (`<end>` past
the end); or `c generator exited <code>` and the first lines of its
stderr; `c wrote no run.txt`; `c run.txt malformed: <reason>`; `c wrote
no rom_writes.txt`; `c rom_writes.txt malformed: <reason>`; `c generator
timed out after <s> s`; `no C generator: <path>` (every case, when the
binary is missing). A detail line over 200 characters is cut at 200 and
ends ` ... (<k> chars)`. Then `e2e: <p> pass, <d> divergent`; exit 0, 1
when a case is divergent, 2 on a usage, claims or golden-format error.

`run_e2e.py --projections [--cases <dir>] [--gen <binary>] [<case>...]`
runs `<gen> --project <case>/rom_writes.txt <out_dir>` on every golden
of the directory (the named ones when given), in parallel, each in a
fresh temporary directory, and compares every group's file with
`write_log.py`'s `projections` of the same golden: one `PROJECTION
<case> <group>: line <n>` (the first differing line), `PROJECTION <case>
<group>: missing` or `... extra` per difference, or `PROJECTION <case>
-: <reason>` when the generator fails (exit code and first stderr line,
timeout, no generator); then `projections: <n> goldens, <g> group
projections equal to write_log.py's` (exit 0), else `projections: <n>
goldens, <d> differences from write_log.py's` (exit 1). A missing golden
`rom_writes.txt`, a malformed golden or an unknown case exits 2. Today
`projections: 88 goldens, 1413 group projections equal to write_log.py's`
and `projections: 40 goldens, 655 ...`, about 5 s and 2 s.

`run_e2e.py --ks-groups [--cases <dir>] [--rom <file>] [<case>...]`
prints, per `C` group without `rom` in its Evidence (not `spoiler`),
`ks-groups: <g>: <n> of <m> goldens hold source references, first
<case>` (with `--rom`, the ROM checked as `e2e-rom` checks it, then `
(<i> with references into KS, <o> outside)`, KS from
`known_source.build`) or `ks-groups: <g>: no golden of <m> holds a source
reference`, then `ks-groups: <k> groups without rom in their Evidence
hold source references: <names>`; exit 0, 2 on a missing or malformed
golden. On the 128 goldens: `bank3e`, `endscreen`, `header`, `marin` in
all of them, `music` in 2, `hints` in none.

`run_e2e.py --self-test` builds planted case trees from `cases/baseline`'s
golden and fake generators (Python scripts copying a planted `run.txt`
and, when planted, `rom_writes.txt`) and checks 54 verdicts. With no
group claimed (18): identical, a changed `generateRom` trace line, a
changed `rom_sha1` or `spoiler_sha1`, a claim at `ws` with a changed
`place` line, and a claim at `init` with C stopping after the seeding
(no outcome line) each `PASS`; a changed `place` line, a changed
fill-stage trace line, C's trace one line short, one extra C fill-stage
trace line, a changed `ws` line, a non-zero exit, no `run.txt`, no
generator, a claim at `init` with C's `outcome ok` and no `ws` line, and
a golden `outcome error` with no `place` against C's places each
`DIVERGENT`; a golden fill-stage line after a `generateRom` line and an
unknown claimed stage each exit 2. With groups claimed, from the
baseline's `rom_writes.txt` (15): identical logs; C's log holding only
the five table groups, merged across the absent `C` segments; a byte
changed only in an unclaimed group's segment before the claimed group's
(`frame-counter`) first segment, and one after its last; and golden `I`
lines against a C log without them, each `PASS`; a byte changed in the
last `options` segment; a literal for a reference to the same bytes (a
synthetic pair, golden `B 100 @2000+4` against C's `B 100 0a0b0c0d`,
stated to be the source bytes: the driver has no ROM); the last `fixed`
segment's last byte moved into the next segment, of another group; the
claimed `options` absent from C's log; no `rom_writes.txt` from C; and a
malformed C log, each `DIVERGENT`; `group nosuch`, a repeated group,
`group spoiler`, and a golden without `rom_writes.txt` while a group is
claimed, each exit 2. With `--projections`, on the baseline's
`rom_writes.txt` and a fake generator copying planted projection files
(3): `write_log.py`'s own projections exit 0; one projection with a
changed `G` line, and one group's file missing, each exit 1 with a
`PROJECTION planted` line. ROM-present and refusals (18), with a fake
generator writing a planted `run.txt` (the golden's unless planted) and a planted log, another one when it receives `--rom`,
and a planted span list for `--ks-spans`, a planted 11-byte ROM and its
SHA-1 injected as the accepted one, and a planted span list as the
oracle's: a `rom-group bank3e` whose last segment C changed `PASS`es
under `e2e`, its header naming it skipped; the same claimed by `e2e-rom`
(the changed log written only on `--rom`) is `DIVERGENT` after `KS
equal, 2 spans, 4 bytes, 1 not rebuilt`, its detail lines holding `
tokens)`; `rom-group marin` with C's first `marin` trace line's result
changed `PASS`es under `e2e` (skipped) and is `DIVERGENT` under
`e2e-rom` with `trace marin: line 1:`, and C's `run.txt` without the
`marin` trace lines is `DIVERGENT` under `e2e-rom` with `c: <end>`;
`rom-group endscreen` on a golden holding one more trace line of a
planted frame no table lists (`patches.planted:unported`) before its draw, later numbers
shifted, `PASS`es under `e2e-rom`, and the same with a `hints:addHints`
line instead is `DIVERGENT` with `trace endscreen: line 1:`; a different span list exits 1 with `KS DIFFERENT: line 3:
boss item 3`; a ROM of another SHA-1 and no ROM exit 2 with a `setup:
e2e-rom:` line; `group hints` (no source reference) `PASS`es; `group
hints` with a planted inventory adding `rom` to its Evidence exits 2;
`group hints` with a planted golden copying 4 source bytes in a `hints`
segment exits 2 naming the case; `rom-group fixed`, `group marin` with
`rom-group marin`, and `rom-group marin` twice each exit 2; and the KS
comparator on equal lists (no line), one span's length changed (`line
2: texts 1`) and one span missing (lines 2 to 4). It prints `e2e:
self-test, 56 planted runs as expected`, else one `e2e: self-test failed: <case> (expected <v>, got
<w>)` line each, exit 1.

`make -C tests/ladx selftest` runs the self-test; `make -C tests/ladx e2e
[CASES="<case> ..."]` builds `ladx_gen`, runs `--projections` on `cases/`,
then the driver on `cases/` with `--tables` the repository's `data/ladx`;
`e2e-combos` prints `e2e-combos: the combination cases`, then runs both on
`combos/`. The root `make selftest`, `e2e` and `e2e-combos` run
`tests/alttp`'s target, then LADX's.

## ROM-present e2e

`make -C tests/ladx e2e-rom [LADX_ROM=<file>]` (default
`tmp/ladx/base.gbc`, repository-relative; outside the net) builds
`ladx_gen`, then runs, from the repository's root, `run_e2e.py --rom
<file> --cases tests/ladx/cases`, then `run_e2e.py --rom <file> --no-ks
--cases tests/ladx/combos`, both with `--tables data/ladx`; it stops at
the first that fails, so exit 0 means KS equal and every case and combo
passing.

`run_e2e.py --rom <file> [--work <dir>] [--no-ks] [--cases <dir>]
[--claims <file>] [--gen <binary>] [--tables <dir>] [--inventory <file>]
[<case>...]`:

- Loads the claims and the goldens as `e2e` does (both refusals
  included), then reads the ROM: a missing file (`setup: e2e-rom: no ROM
  at <path> ...`) or a SHA-1 other than `d90ac17e…` (`setup: e2e-rom:
  <path> is not the LADX English 1.0 ROM (SHA-1 <hex>)`) exits 2.
- Empties the work root (`--work`, default `tmp/ladx-e2e-rom/`; with
  `--no-ks` only its `runs/<directory>/`).
- Unless `--no-ks`, compares KS: `known_source.build`'s span list ("KS
  span list") written to `<work>/ks-oracle.txt`, `<gen> --rom <file>
  --ks-spans <work>/ks-c.txt`, the two compared line by line: one `KS
  DIFFERENT: line <n>: <label>` line per differing line (the oracle's
  label, or `<end>, c has <label>` past its end), exit 1, no case run;
  else `e2e-rom: KS equal, <n> spans, <b> bytes, <f> not rebuilt` (bytes:
  distinct offsets).
- Runs `<gen> --tables <dir> --rom <file> <case_dir>
  <work>/runs/<directory>/<case>` per case, in parallel, kept there,
  claiming the stage, every `group` and every `rom-group`, with `e2e`'s
  verdict lines prefixed `e2e-rom:`. A divergent projection line shows
  only the record's head and its token count (`B <offset> (<k>
  tokens)`, `T <table> <key> (<k> tokens)`; `G`, `tables` and version
  lines whole), and a malformed C log only the parse message's line
  number, so a C bug writing ROM bytes as literals never reaches the
  terminal.

Nothing is committed: the span lists and C's runs stay under `tmp/`;
only counts, labels, offsets and digests are printed. With the ROM:
`e2e-rom: KS equal, 6105 spans, 59971 bytes, 186 not rebuilt`, then
`e2e-rom: 88 pass, 0 divergent` and `e2e-rom: 40 pass, 0 divergent`,
about 12 s.

## Fill-outcome fixture

The case seeds are `outcome ok` by construction (`ladx_cases.py pick`),
so they cannot show that C refuses the seeds LADXR refuses.
`tests/ladx/oracle/fill_outcomes.txt` records LADXR's fill projection
(stage `fill`, above) on seeds that are not case seeds. Runs, in order:
for every case and combo (`cases/` then `combos/`, in `ladx_cases.py`'s
expected order) every candidate `derive(<name>, k)` below the case's own
`k` (the candidates `pick` refused, for the Nintendo-bytes check or by
outcome); candidates `k = 0..7` of each refused single
(`boomerang-default`); then the lines of `oracle/determinism_cases.txt`
whose values are all tier-1 values (`bingo` left out, scope cut
`goal-bingo`), labelled `det-<name>`. Each runs LADXR's `Randomizer` in
`SKIP_ROM_GENERATION` mode (`oracle_run.py`'s driver; no ROM). After a
`#` header documenting it, one line per run:

    <label> <k or -> <seed hex> <ok|error|crash> <lines> <digest> [<k=v> ...]

`<digest>` is the first 16 bytes, in hex, of SHA-512 of the run's fill
projection text (`run_e2e.py`'s `project` at `fill`, lines joined by
`\n`, final newline), `<lines>` its line count; the outcome kind comes
from the run's `outcome` line. Seeds, settings and digests only, no ROM
byte: `nintendo_bytes.py` on the file (general scope) reports 0 literal
runs. Written only by its generator, never by hand:

- `python3 tests/ladx/oracle/gen_fill_outcomes.py [--ladxr <dir>] [--out
  <file>]` (no `-I` or `-E`: both drop `PYTHONHASHSEED`; the LADXR
  checkout at the pin, default `LADXR_DIR` or `tests/ladx/oracle/ladxr`,
  and python3 >= 3.11, else `setup: fill-outcomes: ...`, exit 2; no ROM,
  no network) renders in a child python3 with `PYTHONHASHSEED=0` and
  writes `<file>` (default `tests/ladx/oracle/fill_outcomes.txt`), then
  prints `fill-outcomes: <n> runs written to <file>`; about 1 minute. The
  case tree must match the expected list (`ladx_cases.py`'s comparison),
  else it exits 1 writing nothing.
- `gen_fill_outcomes.py --check [--ladxr <dir>] [--out <file>]` renders
  into `tmp/ladx-fill-outcomes/fill_outcomes.txt` with
  `PYTHONHASHSEED=1` on the same checkout and prints one `DIFFERENT line
  <n> (<label> <k>): committed <line>, regenerated <line>` line per
  differing line (exit 1), `MISSING <file>` when the committed file is
  absent, else `fill-outcomes: <n> runs byte-identical to
  tests/ladx/oracle/fill_outcomes.txt`. No fresh-clone run: the fill's
  independence from hash seed and checkout is proven by `determinism.sh`
  on the same code path.
- `test_gen_fill_outcomes.py` (`make test-ladx-oracle`) tests the line
  rendering and comparison on planted runs, and that the committed
  file's labels, `k`, seeds and settings are exactly the plan the case
  tree and inputs give, every case's candidates below its `k` and none
  of its own seed.
- C side: `test_fill --fixtures` (above, `make logic` and `make
  unit-arm`).

## Encoder fixture

`tests/ladx/oracle/encoder_vectors.txt` holds `write_log.py`'s encoder
("Encoding rule") run on synthetic event streams, the reference C's
encoder replays. Nothing in it comes from a ROM: its ROM is
`random.Random(0x1ADE0E1C).randbytes(0x1000)` with two synthetic regions
copied inside it (`rom[0xC00:0xC10] = rom[0x200:0x210]`, `rom[0x240:0x246]
= rom[0xE00:0xE06]`, for KS-match ties); each vector's KS is a
`known_source.KnownSource` built from the vector's own spans, so
`write_log.encode` runs on it unchanged (membership, `mask`, `entries`,
`span_at`). After a `#` header documenting it, the format (hex
lowercase):

    ladx-encoder-vectors 1
    tables <table>...              ROMWithTables' 11 tables, every stream's and log's order
    rom <size>                     then one `R <hex>` line per 64 bytes of the ROM
    V <name>                       one block per vector
    sides <group>=<table|C>...     its groups' sides, in first-write order
    ks none | ks <n>               no KS (the encoder runs on an empty one), or n K lines
    K <start> <length> <label>     a KS span, labels as the KS span list's ("KS span
                                   list"); a `<table> <key>` label is its key's entry
    events <n>                     n lines of a `ladx-events 1` stream after its three
                                   header lines (`I`, `B`, `T`, tab-separated, site `-`)
    log <n>                        n lines: write_log.encode's rom_writes.txt of the stream

Vectors: 16 hand-written ones, one per rule, each holding a record that
shows it (`test_gen_encoder_vectors.py` pins it): `carried-source-copy`,
`carried-identity` (a computed byte inheriting a keep, carried by a later
group), `inherit-on-equal-bank`, `table-prefix-suffix-middle`,
`table-unknowable-stop`, `ks-match-run-boundary`,
`ks-match-lowest-offset`, `ks-match-longest` (a longer match at a higher
offset wins), `fragment-table`, `fragment-bank` (pieces cut by `span_at`
over overlapping spans), `rename-after-ks-match` and
`rename-after-fragment` (a later group's carried copy inherits the
renamed reference), `keep-rewrite` (a position written back to its
source as `=1`, across segments and within one), `integer-pointer`,
`net-effect`, `table-passthrough` (a `table` group's encoding, then a
`C` group carrying its identities); then 200 `random-<nnn>` vectors drawn
from `random.Random(0x1ADE0E1D)`: 1 to 6 KS spans (entries and other
labels) or `ks none`, initial entries (source copies, computed bytes,
integer pointers, and each KS entry's source), and 2 to 10 events of a
`table` group and two `C` groups (source copies, KS bytes, copies of
earlier writes, fresh write identities, computed bytes, rewrites of an
entry with one byte changed, pointers). 216 vectors, about 195 KB;
`nintendo_bytes.py` on it (general scope) reports 0 literal runs.

- `python3 -I tests/ladx/oracle/gen_encoder_vectors.py [--out <file>]`
  (no ROM, no LADXR, no network; independent of the hash seed) writes
  the fixture (default `tests/ladx/oracle/encoder_vectors.txt`) and prints
  `encoder-vectors: <n> vectors written to <file>`; under a second.
- `gen_encoder_vectors.py --check [--out <file>]` renders it again in
  memory and prints one `DIFFERENT line <n>` line per differing line (the
  first 20) and `encoder-vectors: <k> lines differ from <file>` (exit 1),
  `MISSING <file>` (exit 1), else `encoder-vectors: <n> vectors
  byte-identical to <file>`.
- `test_gen_encoder_vectors.py` (`make test-ladx-oracle`): a planted
  render and parse round trip (sides, KS spans and entries, initial
  entries with a pointer, events, logs), a truncated fixture refused,
  `--check` reporting a changed line and a missing file; on the committed
  fixture: under 300 KB, its ROM the synthetic one, every vector's log
  equal to `write_log.encode` on its parsed stream and KS, names unique,
  the 16 hand-written vectors first, 200 random ones with `ks none` and
  KS entries among them, and each hand-written vector's pinned record.
- C side: `test_encoder --fixtures` ("Unit tests and lint", `make logic`
  and `make unit-arm`) replays every vector byte for byte.

## Measure

`make -C tests/ladx measure` (outside the net; `tests/ladx/measure.sh`)
builds `build/ladx_gen` `-O2`, runs `ladx_gen --time` (the run stopped
after the fill, no sink) 3 times on every case and combo, one after the
other, keeps each case's minimum, and writes `build/measure/report.txt`,
also printed:

- `fill.time <case> <us> us`, per case, `cases/` then `combos/`;
- `fill.time.cases <n> (each the minimum of 3 runs of ladx_gen --time,
  -O2)`, `fill.time.median <us> us`, `fill.time.max <us> us <case>`,
  `fill.time.total <us> us`;
- `fill.state.bytes <n> (sizeof LadxRandomizer)`, the device's static
  memory for this stage (a probe built with `$(CC)` against
  `ladx/include`);
- `miyoo.note ...` and `miyoo.fill.projected <lo>..<hi> ms (host x
  20..50) budget 10000[ over]`: the slowest case times
  `tests/alttp/measure.sh`'s `a7_factor_lo`/`a7_factor_hi` (read from that
  file), the same stated assumptions until a device timing replaces them;
  ` over` when the high end passes the 10 s line.

Exit 0 whatever the figures; 1 when a measurement cannot be taken (no
generator, a failing or silent `--time` run, the factors not found, the
probe not built); 2 on a usage error.

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
  groups less `spoiler`), `scrambled`, `projection`, `nintendo` and
  `fragments` (no literal piece of a record, of any length, occurs in the
  KS bytes it replaces; "Encoding rule", the fragment walk);
- `oracle/determinism.sh --full --all [<case>...]` runs every case (or
  the named ones) twice (hash seeds 0 and 1, a fresh clone, a tagged work
  root) and compares every file, `rom_writes.txt` included.
