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
Capacities: 16 groups, 256 units, 512 `order.txt` rows, 2048 entries, 12
components per unit (today 5, 91, 147, 676 and 6).

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
segment: 32768 bank bytes, 4096 table entries, 65536 table atoms (a
replaced entry's atoms reclaimed first; today's largest table-group
projections 9,953 bank bytes, 1,035 entries and 34,094 table bytes); an
overflow is returned by that call and every later one. A refused record
(an empty bank record, a literal over `ff`, a bad name, table or alt key)
changes nothing.

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
`test_settings`, `test_requirements`, `test_logic`, `test_world_setup`, `test_itempool`, `test_fill`, `test_wlog`, `test_tables`; `LOGIC_TESTS`, run again with `--fixtures`: `test_logic`, `test_itempool`, `test_fill`),
failing through `unit/support.h`'s `test_fail` (`<test>: FAIL
<message>`, exit 1). Every LADX build, host, lint and ARM, uses `-std=c99 -Wall -Wextra -Werror -ffp-contract=off`:
GCC fuses `a * b + c` into an FMA in its GNU modes and Onion's VFPv4 has
one, while CPython never fuses, so `uniform`'s `a + (b - a) * random()`
would differ in its last bit. Phase 4, which builds `ladx/` into the app,
keeps the flag. Library objects see only `-I ladx/include`; the host
layer `ladx/host/` (C99, stdio allowed, never built into the device's
library) sees `-I ladx/include -I ladx/host`; the tests add
`-I tests/ladx/unit`, and those linking the host layer (`HOST_TESTS`:
`test_run_text`, `test_itempool`, `test_fill`) `-I ladx/host` and `-lm`. Build output goes to `tests/ladx/build/`.

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
  entry's atoms reclaimed first) returned by every later call. It prints
  `test_wlog: write_log.render's texts of synthetic segments, refused
  records and overflows (<c> checks)`.
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
  file): 5 groups, 91 units, 676 entries, one unit keyed by `count`; and
  for every `cases/*/config.txt` and `combos/*/config.txt`, validated and
  supported, every unit without `count` selects an entry. It prints
  `test_tables: data/ladx opened (5 groups, 91 units, 676 entries); <n>
  cases and <m> combos select <e> entries`, then `test_tables: synthetic
  walks, keys, open errors, records, applier and spot count (<c>
  checks)`.
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
  run-text trace sink, and writes `<out_dir>/run.txt` in the oracle's
  format, already in its fill projection's form (below): `seed`,
  `settings`, the sections of the stages reached (the 13 `ws` lines once
  `WorldSetup.randomize` returned, none when it raised; `words_before`,
  `pool` and `words_after` once `ItemPool` returned, `placer_pool` once
  `readItemPool` returned, one `place` line per spot of `iteminfo_list`
  once `run` returned), the trace, then the `outcome` line when no
  `place` line was written (LADXR's error or exception; an ok run's
  `outcome ok` belongs to the whole run).
- `ladx_gen --tables <dir> <case_dir> <out_dir>` does the same, then,
  after an ok fill (`ladx_randomizer_placement` not NULL), writes
  `<out_dir>/rom_writes.txt`: the tables of `<dir>` (`order.txt` and every
  other `.txt` file, read whole, 2 MiB in all at most; today 534,590
  bytes) opened, the key of the validated settings and the placed logic's
  spot count, the walk ("Table application") logging each selected
  entry's records into `ladx_wlog` under its unit's group, `C` rows
  skipped (nothing written at them yet), so consecutive entries of one
  group merge into one segment across them. The file is a valid version-1
  write log holding only the table groups' segments, no `I` line; the
  version and `tables` lines are written even when nothing else is. An
  error outcome of the fill writes no `rom_writes.txt`. A tables open,
  key or walk error, or a log overflow, exits 2 with `ladx_gen: tables:
  <status> <file>:<line>` or `ladx_gen: tables: unit <id>: no entry for
  <values>` (or `write log overflow`), the partial `rom_writes.txt`
  removed. Opening and walking the tables and writing the log adds about
  9 ms per run on the host (slowest case, `sample-05`).
- `ladx_gen --time <case_dir>` runs the same with no sink and prints only
  the run's elapsed microseconds (`CLOCK_MONOTONIC`), no file.
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
stages) and zero or more `group <name>` lines (blank lines and `#`
comments skipped), all applying to every case of the directory run,
cases and combos alike. A group must be in the inventory's Groups table
(`--inventory`, default `notes/ladx-inventory.md`, read through
`write_log.py`'s `sides_of` only when a group is claimed) with side
`table` or `C`, and not `spoiler` (its claim is `spoiler.json`). An
unknown stage, a second `stage` line, no `stage` line, an unknown,
repeated or `spoiler` group or any other line exits 2. Claimed now:
stage `fill`, groups `fixed`, `frame-counter`, `goal-fixed`,
`music-fixed` and `options` (the five table groups).

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
claim and then each divergent group in `claims.txt` order, the first
differing projection line as `<section or group>: line <n>:` (`n`
counted in the projection), `golden: <line>`, `c: <line>` (`<end>` past
the end); or `c generator exited <code>` and the first lines of its
stderr; `c wrote no run.txt`; `c run.txt malformed: <reason>`; `c wrote
no rom_writes.txt`; `c rom_writes.txt malformed: <reason>`; `c generator
timed out after <s> s`; `no C generator: <path>` (every case, when the
binary is missing). A detail line over 200 characters is cut at 200 and
ends ` ... (<k> chars)`. Then `e2e: <p> pass, <d> divergent`; exit 0, 1
when a case is divergent, 2 on a usage, claims or golden-format error.

`run_e2e.py --self-test` builds planted case trees from `cases/baseline`'s
golden and fake generators (Python scripts copying a planted `run.txt`
and, when planted, `rom_writes.txt`) and checks 33 verdicts. With no
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
claimed, each exit 2. It prints `e2e: self-test, 33 planted runs as
expected`, else one `e2e: self-test failed: <case> (expected <v>, got
<w>)` line each, exit 1.

`make -C tests/ladx selftest` runs the self-test; `make -C tests/ladx e2e
[CASES="<case> ..."]` builds `ladx_gen` and runs the driver on `cases/`
with `--tables` the repository's `data/ladx`;
`e2e-combos` prints `e2e-combos: the combination cases`, then runs it on
`combos/`. The root `make selftest`, `e2e` and `e2e-combos` run
`tests/alttp`'s target, then LADX's.

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
  groups less `spoiler`), `scrambled`, `projection` and `nintendo`;
- `oracle/determinism.sh --full --all [<case>...]` runs every case (or
  the named ones) twice (hash seeds 0 and 1, a fresh clone, a tagged work
  root) and compares every file, `rom_writes.txt` included.
