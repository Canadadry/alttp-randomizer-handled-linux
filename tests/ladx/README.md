# LADX cases and goldens

The evidence Phase 3's C port of LADXR (pinned at `4325207e`) is checked
against, without the ROM. This file is the reference the C write logger
follows: a C run that logs its writes by these rules gives the golden's
exact text for every group it ports.

## Case folders

`cases/` holds the baseline and every non-default tier-1 value alone;
`combos/` the crosses and hand-picked combinations of `oracle/combos.txt`.
One folder per case, named from its settings (`<key>-<value>`, joined with
`+` in LADXR's order; `baseline`). `oracle/ladx_cases.py check` (in
`make test`) proves the list and the files below.

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

## Budget

`ladx_cases.py check` fails (`BUDGET`) over: `rom_writes.txt` 224 KiB per
case, 22 MiB in all; all golden files (`run.txt`, `spoiler.json`,
`rom_writes.txt`) 34 MiB in all.

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
- `oracle/determinism.sh --full --all` runs every case twice (hash seeds 0
  and 1, a fresh clone, a tagged work root) and compares every file,
  `rom_writes.txt` included.
