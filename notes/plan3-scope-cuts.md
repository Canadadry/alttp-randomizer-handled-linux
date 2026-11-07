# Plan 3 scope cuts

Every tier-1 value of plan 3 that moves to "unsupported" because it is too costly to port, with
its reason. The owner reviews each one at the plan's PR. The cut value moves to `tier1.txt`'s
excluded comment and the plan's unsupported list in Phase 2 step 4 and Phase 3 step 2, not
before. A cut never hides a group that runs on every seed; such a group reads the user's ROM at
runtime (coverage `rom`).

Each cut: its id as a heading (the `cut:<id>` of `notes/ladx-inventory.md`'s Read coverage), the
setting values moved to unsupported, the inventory group and row that forced it, the read that
forced it, and the reason.

## goal-bingo

- Values: `goal=bingo`, `goal=bingo-double`, `goal=bingo-triple`, `goal=bingo-full`.
- Group: `goal`, row `patches.bingo.setBingoGoal(rom, world_setup.bingo_goals, world_setup.goal)`
  of `generator.py generateRom`, reached only under these four values.
- Read: the bingo card. For each of the 25 goals drawn with `rnd` (`bingo.randomizeGoals`),
  `TileInfo.get` reads four 16-byte tiles of source graphics (banks 0x2C to 0x3F) and, for the
  item goals, shifts, mirrors or recolours them bit by bit before writing them to bank 0x30. The
  output bytes are new values computed from Nintendo graphics: not a copy a source reference can
  encode, and no derived form holds them without holding the graphics themselves. The card also
  re-encodes background 0x15 from its source tile values.
- Reason: the group's claim needs the transformed tiles of every goal a seed can draw, which only
  the ROM's graphics give. Phase 2 step 3 trace: `bingo` case, three seeds, reads of banks 0x26,
  0x2C, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x35, 0x38, 0x3F and of the entities, room, text and
  background tables. Decided under `guideline` when structure could be extracted in decimal
  but graphics, text and code bytes could not; the owner has since removed every extraction
  (nothing derived from ROM content is committed), the cut stands.
