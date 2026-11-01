---
title: "Port the initial SRAM writes and the trailing setTotalItemCount"
description: "The C ROM writer writes neither VT's 1,280-byte initial save table nor its total item count, so a patched ROM would start every new save file from the base patch's table and no case can claim past its text."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 10, copied from `docs/plan-1.md`, as amended by plan review)

> **Port the initial SRAM writes** (`InitialSram`) and the trailing `setTotalItemCount`. Verified by every case's claim reaching the `setTotalItemCount` group, plus `test_rom_sram.c`.

The heart-beep group that follows `setTotalItemCount` belongs to step 11, not to this step.

## Facts at `93039c4` (checked)

### The target

- `tests/rando/write_groups.txt` has `group sram writeInitialSram`, then `group setTotalItemCount setTotalItemCount`, then `group heart_beep setHeartBeepSpeed`.
- Every one of the 73 goldens (50 `cases/`, 23 `combos/`) ends with the same three lines, in this order:
  - one `writeInitialSram` line: address `183000`, exactly 1,280 bytes (`0x500`);
  - one `setTotalItemCount` line: `180196 D800` (216, little-endian) in all 73;
  - `180033 <byte> setHeartBeepSpeed` (step 11).
- The `writeInitialSram` line comes right after the `writeText` line.
- **VT's calls** (`tmp/vt`, `app/World.php` `writeToRom`, after `writeCredits` and `writeText`):
  - `$rom->writeInitialSram()`, which is `write(0x183000, pack('C*', ...$this->initial_sram->getInitialSram()))`;
  - `$rom->setTotalItemCount($this->getTotalItemCount())`, which is `write(0x180196, pack('v', $count))`.
- `getInitialSram` throws when the table is not exactly `0x500` entries. No caller on this path can grow it (below).

### The non-zero bytes of the 73 goldens' `writeInitialSram` write

Decoded from the goldens. Every case has the constructor's bytes and the default equipment's:

- `20D=F0`, `20F=F0`, `379=68`, `401=FF`, `402=FF` (the constructor);
- `36C=18`, `36D=18`, `370=0A`, `371=1E` (three hearts, bomb capacity 10, arrow capacity 30).

Then, by case:

| Bytes | Cases | Setter |
|---|---|---|
| `29B=20`, `3C5=02`, `3C6=14`, `3C8=01` | 65 (every non-standard state, inverted included) | `setOpenMode` |
| none of these four | 8 (`state-standard` and the 7 `+state-standard` combos) | `setStandardMode` ORs zeros |
| `2DB=20` | 5 (`goal-fast_ganon`, `goal-ganonhunt` and its 3 combos) | `preOpenPyramid` |
| `355=01`, `379=6C` | 17 (every glitched logic, alone or in a combo) | `PegasusBoots` pre-collected |
| `359=01`, `417=01` | 1 (`weapons-assured`) | `UncleSword` pre-collected |
| `061=80`, `093=80`, `359=FF` | 1 (`weapons-swordless`) | swordless curtains and override |

No golden reaches `preOpenGanonsTower` (`2C3`), a non-zero timer, or any other starting item.

### `rom_buffers.txt`'s `sram` lines

- The oracle (`oracle_rom.php` `OracleInitialSram`) logs each outermost `InitialSram` call as `sram <method> <changes>`, in call order, mixed with the `text` and `credit` lines.
  - `<changes>` lists every byte the call changed, as `%03X=%02X` (the new value), sorted by offset, comma-separated; `-` when the call changed nothing.
  - Nested calls (`setValue` inside a setter) are not logged on their own.
  - `oracle_check_sram_overrides` dies if a public `InitialSram` method is not logged.
- The oracle's self-check (`gen_case.php`) proves that the constructor's table plus the `sram` lines rebuild the `writeInitialSram` write.
- Across the 73 goldens, the methods and their counts:
  - `setStartingTimer -` in all 73;
  - `setStartingEquipment` in all 73, four variants: `36C=18,36D=18,370=0A,371=1E` (54), with `355=01,…,379=6C` (17), with `359=01,…,417=01` (1), with `359=FF` (1);
  - `setProgressIndicator`, `setProgressFlags`, `setStartingEntrance`: `3C5=02`, `3C6=14`, `3C8=01` in 65, `-` in 8;
  - `preOpenCastleGate 29B=20` in 65;
  - `preOpenPyramid 2DB=20` in 5;
  - `setSwordlessCurtains 061=80,093=80` in 1.
- Call order in a golden: `preOpenPyramid`, `preOpenCastleGate`, `setProgressIndicator`, `setProgressFlags`, `setStartingEntrance`, `setSwordlessCurtains`, `setStartingEquipment`, `setStartingTimer`.
- No harness reads these lines.

### VT's `InitialSram` (`app/Support/InitialSram.php`)

- **Constructor:** `0x500` zeros, then `0x20D = 0xF0`, `0x20F = 0xF0` (room data: Kakariko bomb hut and brewery), `0x379 = 0x68` (ability flags), `0x401 = 0xFF`, `0x402 = 0xFF`.
- **`setValue($idx, $val)`** ORs `$val` into the byte. It throws when `$idx > 0x500` (so `0x500` itself would append a byte) or when `$val` is outside 0–255. No call on this path reaches either.
- **Setters that only OR** (through `setValue`):
  - `preOpenCastleGate`: `0x29B |= 0x20` (`OVERWORLD_DATA + 0x1B`);
  - `preOpenGanonsTower`: `0x2C3 |= 0x20` (`+ 0x43`);
  - `preOpenPyramid`: `0x2DB |= 0x20` (`+ 0x5B`);
  - `setProgressIndicator(x)`: `0x3C5 |= x`; `setProgressFlags(x)`: `0x3C6 |= x`; `setStartingEntrance(x)`: `0x3C8 |= x`;
  - `setSwordlessCurtains`: `0x061 |= 0x80`, `0x093 |= 0x80`.
- **`setStartingTimer($seconds)`** assigns `pack('V', $seconds * 60)` to `0x454`–`0x457`.
- **`setStartingEquipment($items, $config)`**, in this order:
  1. `0x36C = 0x18` and `0x36D = 0x18` when `$items->heartCount(0) < 1` (`heartCount` counts `Item\Upgrade\Health` items, a `PieceOfHeart` as 0.25).
  2. For each item, in the collection's insertion order with duplicates (`values()`), a `switch` on `$item->getTarget()->getRawName()`: about 120 `case` labels, each assigning, OR-ing, or adding with a cap. The labels the generation path can reach:
     - `BossHeartContainer` (with `HeartContainer`, `HeartContainerNoAnimation`): `0x36C` and `0x36D` each `min(+0x08, 0xA0)`;
     - `BombUpgrade10`: bomb capacity `+= 10`; `ArrowUpgrade10`: arrow capacity `+= 10` (local counters);
     - `PegasusBoots`: `0x355 = 0x01`, `0x379 |= 0x04`;
     - `ProgressiveSword` (the target of `UncleSword`): `0x359 = min(0x359 + 1, 4)`;
     - `RescueZelda` (an event): no `case`, nothing.
  3. Rupees (a local counter, 0 here): `0x360 = 0x362 = low byte`, `0x361 = 0x363 = high byte`.
  4. Counters: `0x476 = count_set_bits(0x37A)` (crystals), `0x429 = count_set_bits(0x374)` (pendants), `0x417 = 0x359`, `0x422 = 0x35A`, `0x46E = 0x35B`.
  5. `setValue(0x370, bomb capacity)`, `setValue(0x371, arrow capacity)`: OR, not assign.
  6. When `$config['mode.weapons'] == 'swordless'`: `0x359 = 0xFF`, `0x417 = 0x00`.
- A trap in the switch: the label `'Progressivebow'` (lower-case `b`) never matches `ProgressiveBow`, and its body's `min(...)` result is discarded. Unreachable here (no bow is ever pre-collected).
- `preOpenAgaCurtains`, `preOpenSkullWoodsCurtains`, `setInstantPostAga` and `getValue` have no caller on the `writeToRom` path.

### Who calls `InitialSram` before `writeInitialSram`

All of them run in the settings part of `writeToRom` (the C settings group, step 7), which writes their ROM bytes but not their SRAM effects. In VT's call order:

1. **The goal switch** (`World.php`): `fast_ganon` and `ganonhunt` call `preOpenPyramid()`.
2. **`Rom::setGameState($state)`** (`World.php` passes `mode.state`):
   - `open` and `retro` → `setOpenMode(true)` → `preOpenCastleGate()`, `setProgressIndicator(0x02)`, `setProgressFlags(0x14)`, `setStartingEntrance(0x01)`;
   - `inverted` → `setInvertedMode(true)`, which calls `setOpenMode(true)`;
   - `standard` → `setStandardMode()` → the three setters with `0x00`, which change nothing.
3. **`Rom::setSwordlessMode($enable)`** with `mode.weapons === 'swordless'`: `setSwordlessCurtains()` when true.
4. **`setStartingEquipment($this->pre_collected_items, $this->config)`**.
5. **`setStartingTimer($this->config('rom.timerStart', 0) ?: 0)`**: `rom.timerStart` is never set on this path, so 0, which assigns zeros.
6. **`preOpenGanonsTower()`** when `$this->config('crystals.tower') === 0`. The web form and the oracle pass `'0'..'7'` as strings, so a fixed `crystals_tower-0` never matches (step 1.2 note). Only `'random'` gives an int (`get_random_int(0, 7)` in `RandomizerController::prepSeed` and `oracle_case.php`), so the call happens exactly when the tower count is random **and** the draw is 0. `crystals_tower-random`'s draw is not 0.

The ORed offsets (steps 1–3, 6) and the offsets `setStartingEquipment` and `setStartingTimer` assign are disjoint, so the final table does not depend on the order of these calls.

### The pre-collected items on this path (`app/Randomizer.php`)

- The constructor sets the default 7 when the world has none: 3 × `BossHeartContainer`, `BombUpgrade10`, 3 × `ArrowUpgrade10`.
- `prepareWorld` adds `PegasusBoots` under `MajorGlitches`, `HybridMajorGlitches`, `OverworldGlitches` and `NoLogic`, then `RescueZelda` when the state is not `standard`.
- Under `mode.weapons` `assured`, the uncle's sword (`UncleSword`, target `ProgressiveSword`) is pre-collected.
- Nothing else adds a pre-collected item outside `CustomizerController` (out of scope).
- **The C side** mirrors this: `rando_prepare_start_equipment`, `prepare.c`'s `PegasusBoots` and `RescueZelda`, and the assured uncle's sword, all through `rando_world_add_pre_collected`. `RandoWorld` keeps the insertion order (`rando_world_pre_collected_count`, `rando_world_pre_collected_item`). `UncleSword`'s C alias target is fixed to `ProgressiveSword` (`prepare.c` asserts it).

### VT's `World::getTotalItemCount`

- `count` of the world's locations that are not `instanceof` `Location\Prize` (crystals, pendants and events included), `Location\Fountain`, `Location\Medallion` or `Location\Trade`.
- It depends on the world's location set only, never on items. Every golden, of every state (open, standard, inverted, retro), has 216.
- The C has `rando_location_info(id)->location_class` and `rando_location_is_a` (VT's `instanceof` between location classes).

### The C side

- `rando_rom_write` (`rando/src/rom.c`) calls, after the text slot, the two empty statics `write_initial_sram` and `write_total_item_count` (`TODO step 10`), then `write_heart_beep` (`TODO step 11`). Groups write through `rando_rom_emit` (private `rom_emit.h`), which stops at the first refused write.
- `rando_rom.h`'s contract already lists the initial SRAM and `setTotalItemCount` groups. No public API needs a change.
- `RandoConfig` (`rando_config.h`) has `mode_state`, `mode_weapons`, `goal`, `logic`, `rom_rupee_bow` and the resolved `crystals_tower`. `RandoGen.options.crystals_tower` (`RandoCrystals`) is `RANDO_CRYSTALS_RANDOM` when the count was drawn.
- `RandoConfig` has no timer key. Step 1.7's precedent: a single-valued key is a named constant.
- The library keeps no mutable static state; steps 6–9 build their buffers on the stack.
- `tests/rando/Makefile`: `UNIT_TESTS` lists the unit binaries; `LOGIC_TESTS` is `UNIT_TESTS` minus those without a `--fixtures` mode.

## Step 9's outcome (hypotheses)

Step 9 (`docs/prd/triage/phase1-step9-text-dialog-hints-writes.md`) is not done at `93039c4`: no text code is committed, and every claim reads `<case> all credits`. Every decision below that depends on what step 9 leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it held. A hypothesis that doesn't hold is handled by the rule given with it.

- **H-9-1**: step 9 is finished: every line of `claims.txt` (50) and `combo_claims.txt` (23) is `<case> all text`, and `make check` is green.
  - *If a case is still below `all text`*, this step can't claim `all setTotalItemCount` for it. It stops and reports that case instead of claiming around it, since the step text requires every case.
  - Chunks 1 and 2 need no text claim and may go ahead.
- **H-9-2**: the text group lives in its own private source, declared in `rom_emit.h` and called from `rando_rom_write`'s text slot. The SRAM and total-item-count slots are still the empty statics `write_initial_sram` and `write_total_item_count` (`TODO step 10`), between the text slot and `write_heart_beep`.
  - *If step 9 moved or renamed them*, the two groups take whatever slots come between `writeText` and the heart beep, in that order.
  - Either way, the SRAM group gets its own private source, declared in `rom_emit.h`.
- **H-9-3**: `test_rom`'s baseline expectation reads "items, settings, two `writeCredits`, then one `writeText`", and step 9's PRD says step 10 rewrites it. This step rewrites it on purpose.
- **H-9-4**: the test-side helpers stayed local: `test_rom_text` has its own recording writer and its own walk over `cases/` and `combos/` reading `rom_buffers.txt`, and `support.*` has neither.
  - *If step 9 moved a recorder or a case walk into `support.*`*, `test_rom_sram` reuses it.
  - *Otherwise* it has its own copies, in the style of `test_rom_text`.
- **H-9-5**: step 9 added no SRAM table, no `InitialSram` logic, no total-item count and no pre-collected-item processing to `rando/` or `RandoGen`.
  - *If it added any of these*, this step uses what is there as the single source, and does not keep a second one.
- **H-9-6**: step 9 left unchanged the APIs this step reads: the pre-collected order (`rando_world_pre_collected_count`, `rando_world_pre_collected_item`), `RandoConfig`'s `mode_state`, `mode_weapons`, `goal`, `crystals_tower`, `RandoGen.options.crystals_tower`, `rando_location_info` and `rando_location_is_a`.
  - *If it changed them*, this step reads what is there.
- **H-9-7**: `test_rom_text` is in `UNIT_TESTS` and kept in `LOGIC_TESTS`, with a `--fixtures` mode that walks the goldens (step 9 PRD). This is the precedent `test_rom_sram`'s golden replay follows.
- **H-9-8**: `write_groups.txt`, `run_e2e.sh`'s claim semantics, the goldens, the existing fixtures, `gen_golden.sh` and the oracle are unchanged by step 9, apart from its new `text_tables.txt` fixture and `text_tables.php` line.
- **H-9-9**: step 9's text group still reads `crystals_tower` without touching `RandoGen.options`, so nothing in the tree yet distinguishes a drawn tower count from a fixed one; this step is the first to need it.
  - *If step 9 added a helper for "the tower count was drawn"*, this step reuses it.

## Problem Statement

The owner will play ROMs the console patches. When a new save file is created, the game copies its starting state from the ROM's initial SRAM table: the open castle gate and Link's starting point, the three hearts and the starting capacities, the boots of a glitched seed, the uncle's sword under assured weapons, the swordless curtains, the pre-opened pyramid of fast Ganon and Ganon hunt. The HUD's item counter reads the total item count.

Today the C ROM writer writes neither. A patched ROM would keep the base patch's table: an open-mode seed would start as standard, a glitched seed without its boots, and a swordless seed behind closed curtains, unwinnable as alttpr.com intends it. The net can't catch it: every claim stops before these two writes.

## Solution

- **The SRAM group.** `rando_rom`'s initial SRAM group builds VT's `0x500`-byte table from the generator alone and writes it once, at `183000`, tagged `writeInitialSram`:
  - the constructor's bytes;
  - the open-mode bytes (open, retro, inverted), none for standard;
  - the pyramid under fast Ganon and Ganon hunt;
  - the swordless curtains;
  - the starting equipment from the pre-collected items, in their order, with VT's counters and the swordless override;
  - the zero timer;
  - Ganon's Tower pre-opened when the tower count was drawn at random as 0.
- **The total item count.** The next group writes VT's count of collectable locations as two little-endian bytes at `180196`, tagged `setTotalItemCount`.
- Both read the generator as const and draw nothing.
- **Proof:**
  - `test_rom_sram`, one unit case per rule, expected bytes computed by hand from VT's source;
  - its `--fixtures` mode, in `make logic`, compares each golden's `writeInitialSram` write with the C table offset by offset and names the golden `sram` setter line behind each differing byte;
  - then every claim in `claims.txt` and `combo_claims.txt` becomes `<case> all setTotalItemCount`, so `e2e` and `e2e-combos` compare all 73 logs byte for byte up to, and including, the total item count.

## User Stories

1. As the owner of an open seed, I want a new save to start with the castle gate open and Link in his house, so that the game begins as on alttpr.com.
2. As the owner of a retro or inverted seed, I want the same open start, so that these states begin as VT sets them.
3. As the owner of a standard seed, I want the escape sequence intact, with none of the open-mode bytes, so that standard plays as standard.
4. As the owner, I want three hearts, bomb capacity 10 and arrow capacity 30 at the start, so that the starting equipment matches VT's defaults.
5. As the owner of a glitched seed (overworld glitches, major glitches, hybrid, no logic), I want to start with the Pegasus Boots, ability flag included, so that the seed's logic is playable.
6. As the owner of an assured-weapons seed, I want to start with the uncle's sword, so that the first sword is in hand.
7. As the owner of a swordless seed, I want the Agahnim and Skull Woods curtains open and no sword, so that the swordless seed can be finished.
8. As the owner of a fast Ganon or Ganon hunt seed, I want the pyramid hole pre-opened, so that Ganon is reachable as alttpr.com intends.
9. As the owner of a seed whose random tower count came out 0, I want Ganon's Tower pre-opened, so that the tower is open as on alttpr.com.
10. As the owner of a seed whose tower count was chosen as 0 in the menu, I want the tower left as VT leaves it (not pre-opened, VT's string-compare quirk), so that the ROM matches VT.
11. As the owner, I want the HUD's total item count to be VT's 216, so that the item counter reads right.
12. As the owner, I want every other byte of the save table to be VT's, so that nothing else differs from alttpr.com.
13. As the app (Phase 2), I want both writes to land inside the 2 MiB ROM through my buffer writer, so that patching never corrupts memory.
14. As the app, I want the writer to stop at a refused SRAM or count write and return false, so that a failed patch is reported and not half-applied.
15. As a porting agent, I want both groups to read the generator only as const and never touch the PRNG, so that writing a ROM can never change the seed.
16. As a porting agent, I want the SRAM table computed from the config and the pre-collected items alone, never from state the settings group leaves behind, so that groups stay independent.
17. As a porting agent, I want the starting-equipment rules ported for exactly the items this generator can pre-collect, with an assertion on any other, so that no untestable dead switch enters the library and a future new pre-collected item fails loudly.
18. As a porting agent, I want VT's "drawn tower count of 0" rule decided from the options and the resolved count, so that the `=== 0` quirk is reproduced in both directions.
19. As a porting agent, I want the total item count computed from the location classes, never a hard-coded 216, so that it stays VT's rule.
20. As a porting agent, I want the SRAM group in its own private source, so that step 11 never touches it.
21. As the net, I want a unit test per SRAM rule (constructor, each state, each goal, swordless, each pre-collected item, the heart guard, the counters, the OR on capacities, the timer, both tower branches), so that rules no golden reaches are still checked.
22. As the net, I want unit tests of the count on every state and of its independence from items, so that the count's rule is pinned.
23. As the net, I want the refusal rule and the const rule tested on both writes, so that the writer contract holds.
24. As the net, I want `test_rom_sram --fixtures` to compare every golden's SRAM table with the C's, naming the offset, both bytes and the golden setter, so that a wrong byte is reported by its cause and not as one 1,280-byte write.
25. As the net, I want every case and combination to claim `all setTotalItemCount`, so that `make check` fails on any wrong SRAM byte or count.
26. As the net, I want everything before these writes still compared, so that an SRAM divergence is never reported on a seed that already differs.
27. As the net, I want `test_rom`'s baseline expectation rewritten on purpose, with the reason stated, so that no check is weakened silently.
28. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new source obeys the library rules.
29. As the net, I want `make test` to stay fast, with the golden replay only in `make logic`, so that the dev loop stays seconds long.
30. As a reviewer, I want the step note to record which H-9 hypotheses held, the claims moved, the reachable pre-collected items and any VT quirk settled, so that I can check the step without rerunning it.

## Implementation Decisions

### Modules

- **No public API change.** Both groups sit behind `rando_rom_write`, whose contract already names them. Tests go through `rando_rom_write` with a recording writer, and decode the one `0x500`-byte write (as `test_rom_credits` does). A public SRAM interface would buy nothing: the table is one write whose offsets are its own key, unlike step 9's 408 concatenated ids.
- **A private source in `rando/src/`** holds the SRAM group: the table build and the one `writeInitialSram` emit. It is declared in `rom_emit.h` and called from `rando_rom_write`'s SRAM slot (H-9-2).
- **The total item count** is small enough to live with it in the same source, also declared in `rom_emit.h`, and called from the `setTotalItemCount` slot. The two empty statics go away.

### The SRAM table

- Built in a function-local `uint8_t[0x500]`, never static (the library has no mutable static state). A named constant holds `0x500` (VT's `SRAM_SIZE`).
- One private helper ORs a value into a byte, as VT's `setValue`; it asserts the index is below `0x500` and the value fits a byte (VT's exceptions, unreachable here).
- **The order of VT's calls** is kept, though the facts show the result is order-independent; a trap comment says so, because `setStartingEquipment` assigns where the others OR:
  1. the constructor's five bytes;
  2. `preOpenPyramid` when `goal` is `FAST_GANON` or `GANONHUNT`;
  3. the open-mode four (`0x29B |= 0x20`, `0x3C5 |= 0x02`, `0x3C6 |= 0x14`, `0x3C8 |= 0x01`) when `mode_state` is open, retro or inverted; nothing for standard (VT ORs zeros);
  4. the curtains when `mode_weapons` is swordless;
  5. the starting equipment (below);
  6. the timer: `0x454`–`0x457` assigned from `seconds * 60` little-endian, with `seconds` a named constant 0 (VT's `rom.timerStart`, never set here);
  7. `preOpenGanonsTower` when `options.crystals_tower == RANDO_CRYSTALS_RANDOM` **and** `config.crystals_tower == 0` (H-9-9). A trap comment states VT's `=== 0` on the string the menu passes.
- The SRAM group decides each rule from `gen->world.config` and `gen->options` itself. It does not call into the settings group, which keeps writing the ROM side of `setOpenMode` and the others.

### The starting equipment

- It iterates the pre-collected items in insertion order (`rando_world_pre_collected_item`), reading an alias as its target, as VT's `getTarget()->getRawName()`.
- **The heart guard first:** when the pre-collected `Health` count (`BossHeartContainer`, `HeartContainer`, `HeartContainerNoAnimation` as 1, `PieceOfHeart` as 0.25) is below 1, `0x36C = 0x36D = 0x18`.
- **The per-item rules ported** are exactly the labels the generation path reaches (facts):
  - `BossHeartContainer`, `HeartContainer`, `HeartContainerNoAnimation`: `0x36C` and `0x36D` `min(+0x08, 0xA0)`;
  - `BombUpgrade10`, `ArrowUpgrade10`: the local capacities `+= 10`;
  - `PegasusBoots`: `0x355 = 0x01`, `0x379 |= 0x04`;
  - `ProgressiveSword`: `0x359 = min(0x359 + 1, 4)`;
  - `RescueZelda`, and any other event: nothing, as VT's switch has no label for it.
- **Any other pre-collected item** fails an assertion naming VT's `setStartingEquipment` switch. Reason, recorded here: no path but `CustomizerController` (out of scope) can pre-collect one, so the other ~110 labels would be dead code no golden, fixture or caller could check; the assertion makes a future change loud instead of silently wrong. VT is frozen, so the list cannot drift.
- **Then, on every path:** the rupee bytes from a zero counter (`0x360`–`0x363`), the four counters (`0x476`, `0x429` by bit count, `0x417`, `0x422`, `0x46E` copied), the capacities ORed into `0x370` and `0x371`, then the swordless override (`0x359 = 0xFF`, `0x417 = 0x00`).
- `rom_rupee_bow` is read by no reachable label, so this step does not read it.

### The total item count

- Counted over every location id: those whose class is not `rando_location_is_a` `PRIZE`, `FOUNTAIN`, `MEDALLION` or `TRADE`. Computed at write time from the location table, never a literal 216.
- Written as `count & 0xFF`, `count >> 8` at `180196`, tagged `setTotalItemCount`; asserted to fit 16 bits.
- If the C location set differs from VT's on some state, the goldens of that state show it at the claim (every state has golden cases).

### The golden replay

- `test_rom_sram --fixtures` walks every case directory of `cases/` and `combos/` (H-9-4). For each one it:
  - reads the case (`rando_case_read`), generates it, and records the C `writeInitialSram` and `setTotalItemCount` writes;
  - takes the golden `rom_writes.txt`'s `writeInitialSram` and `setTotalItemCount` lines;
  - compares the two tables byte for byte and the two counts.
- A failure names the case and, per differing offset, the C byte, the golden byte, and the golden `rom_buffers.txt` `sram` line(s) whose change list holds that offset (the constructor when none does).
- It also fails when a golden `sram` line names a method the C does not port (the 8 the goldens hold, plus `preOpenGanonsTower`), so a new setter in a regenerated golden is noticed.
- **Cost:** about 73 generations, so `make logic` only, never `make test`.

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all setTotalItemCount` (H-9-1). That claims the whole log but the heart-beep line.
- Nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle (H-9-8). This step adds no fixture: the goldens hold every reachable SRAM byte, and the unreached rules are unit-tested from VT's source.
- A case that is `DIVERGENT` at `all setTotalItemCount` is a bug in this step. `check-e2e`'s loop drives it to green before the step ends, each fix behind its own failing unit test first. The golden replay in `make logic` runs before `e2e` in `make check`, so it reports the failing offset first.

### Not done here

- No `measure` run: step 10 sets no budget.
- `e2e-arm` is not run (step 12 and the end of Phase 2 run it). If it runs anyway, the result goes into the note.
- No change to `check-e2e`: the per-offset failure already comes out of `make logic`.

### Step note

The plan's §6 gains a "Step 1.10" note with:

- which H-9 hypotheses held, and how any that didn't was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all setTotalItemCount`);
- the pre-collected items the starting equipment ports, and why the rest is an assertion;
- the C total item count per state (216 expected);
- any VT quirk settled. Expected: `setStandardMode` ORing zeros; `preOpenGanonsTower` only for a drawn 0; the capacities ORed, not assigned; the `Progressivebow` label that never matches (unreachable).

### No `guideline` agent

None was consulted. Every point was settled from:

- the plan;
- VT at `219fcafd` (`tmp/vt`: `InitialSram.php`, `Rom.php`, `World.php`, `Randomizer.php`, `ItemCollection.php`, `RandomizerController.php`);
- the code, the goldens and the oracle at `93039c4`;
- step 9's PRD;
- steps 1.1 to 1.8's notes.

## Implementation Chunks

1. **The SRAM group.**
   - The private source with the table build and the `writeInitialSram` emit, wired into the SRAM slot.
   - `test_rom_sram` registered in `UNIT_TESTS`, with the SRAM unit cases (Testing Decisions).
   - `test_rom`'s expectation rewritten for the new write (H-9-3).
   - `make check` green, no claim moved.
2. **The total item count.**
   - The count and its `setTotalItemCount` emit, wired into its slot.
   - The count's unit cases in `test_rom_sram`; `test_rom`'s expectation extended to the count.
   - `make check` green, no claim moved.
3. **The golden replay, the claims and the note.**
   - `test_rom_sram` joins `LOGIC_TESTS` with its `--fixtures` golden replay.
   - Every claim of both files moves to `all setTotalItemCount`. `e2e` and `e2e-combos` are driven to green, each divergence fixed behind its own unit test.
   - The §6 "Step 1.10" note.

## Testing Decisions

- **What a good test is here.**
  - It goes through the public interface only: `rando_rom_write` with a recording writer. It never inspects a static function.
  - Expected bytes come from VT's rules at `219fcafd`, written by hand in the test as `offset=value` pairs over the constructor's defaults, never from the C code. Golden input is read only in `--fixtures` mode.
  - Cases start from one `rando_generate` of the baseline options (seed 1), made once per binary. They then edit a copy through public fields and APIs:
    - `world.config` fields (`mode_state`, `mode_weapons`, `goal`, `crystals_tower`) and `options.crystals_tower`;
    - the pre-collected items (cleared with `rando_items_clear` and the order count, refilled with `rando_world_add_pre_collected`);
    - `rando_world_set_item` for the count's independence from items.
  - Prior art:
    - `test_rom`, `test_rom_items`, `test_rom_settings` and `test_rom_credits` for the recording writer and the edited baseline;
    - `test_rom_text --fixtures` (H-9-7) and `test_generate` for walking the case directories.
- **`test_rom_sram`** (new, in `UNIT_TESTS`, joining `LOGIC_TESTS` in chunk 3). Cases:
  - **Shape:**
    - exactly one `writeInitialSram` call, at `183000`, 1,280 bytes, after the `writeText` call;
    - exactly one `setTotalItemCount` call, at `180196`, 2 bytes, right after it, and nothing after it but a heart-beep tag;
    - both inside 2 MiB.
  - **Baseline:** the whole table equals the constructor's five bytes, the open-mode four, and `36C=18`, `36D=18`, `370=0A`, `371=1E`, every other byte 0.
  - **States:** open, retro and inverted give the open-mode four; standard gives none of them.
  - **Goals:** `FAST_GANON` and `GANONHUNT` give `2DB=20`; each other goal does not.
  - **Swordless:** `061=80`, `093=80`, `359=FF`, `417=00`, even with a sword pre-collected (the override wins).
  - **Starting equipment:**
    - `PegasusBoots` gives `355=01` and `379=6C`;
    - one `UncleSword` gives `359=01`, `417=01`; two `ProgressiveSword`s give `02`; five give `04` (the cap);
    - no heart pre-collected gives `36C=36D=18`; one `BossHeartContainer` gives `08` (no guard); four give `20`;
    - two `BombUpgrade10`s give `370=14`, and three `ArrowUpgrade10`s `371=1E` (VT ORs them into a zero byte, so no case can tell OR from assignment: the port keeps VT's OR);
    - `RescueZelda` alone changes nothing but the heart guard;
    - the counters: `417` follows `359`, `422` and `46E` stay 0, `429` and `476` stay 0.
  - **Timer:** `454`–`457` are 0.
  - **Ganon's Tower:** options random with a resolved 0 gives `2C3=20`; options fixed `0` gives none; options random with a resolved 3 gives none.
  - **Count:** 216 (`D8 00`) on open, standard, inverted and retro worlds; unchanged after moving an item between locations.
  - **Refusal:** a writer refusing `writeInitialSram` gets no later call, and `rando_rom_write` returns false; the same for `setTotalItemCount`.
  - **Const:** the generator's bytes are unchanged after a write.
  - **`--fixtures`:** the golden replay (Implementation Decisions).
- **`test_rom`**: its baseline expectation is rewritten on purpose, because the two groups now write (H-9-3). It becomes: items, settings, two `writeCredits`, one `writeText`, one `writeInitialSram`, one `setTotalItemCount`, in that order, all inside 2 MiB, accepted by the buffer writer. Step 11 rewrites it again.
- **`e2e` and `e2e-combos`**: the step's proof is all 73 claims at `all setTotalItemCount`, green.
- **`lint`** builds the new source with `-std=c99 -pedantic-errors`.
- **Speed:**
  - `make test` gains one binary: one baseline generation and a few dozen `rando_rom_write` calls.
  - The golden replay (73 generations) runs only in `make logic`.

## Out of Scope

- **The heart beep** (`setHeartBeepSpeed`, the last golden line) and the checksum: step 11. So are the writes step 1.2's note marks "reproduced in C".
- **`setStartingEquipment`'s labels for items this generator never pre-collects** (bottles, bows, keys, maps, pendants, crystals, rupees and the rest): asserted, not ported.
- **`InitialSram` methods with no caller on this path:** `preOpenAgaCurtains`, `preOpenSkullWoodsCurtains`, `setInstantPostAga`, `getValue`.
- **Settings VT never sets here:** a non-zero `rom.timerStart`, `rom.timerMode`, customizer starting items.
- **Oracle changes:** any change to the goldens, the fixtures, `stages.txt`, `write_groups.txt` or the claim semantics. This step adds no fixture.
- Running `check-e2e` over the whole case list, and `e2e-arm` (step 12).

## Further Notes

- **Why no new fixture**, when steps 8 and 9 added one? Their tables held hundreds of strings no golden reached. Here every rule the generator can reach is a handful of bytes, read off VT's source and checked by hand in a unit case, and the goldens already hold every reachable combination but the drawn-0 tower and the multi-item caps, both unit-tested.
- **Why derive the drawn tower count from the options**, not add a flag to the config? `RandoGen.options` already says whether the count was drawn, and `RandoConfig` mirrors VT's config keys, which hold an int or a string, not a flag. The rule reads both at the one place VT's `===` matters.
- **Where a divergence is likely:**
  - the order of the pre-collected items under assured and glitched logics (it matters only for capped adds);
  - the swordless override after the counters (`417` must end 0);
  - `setStandardMode` writing nothing;
  - the count on inverted, whose region set differs from open's.

  The goldens of `weapons-assured`, `weapons-swordless`, `state-standard`, `state-inverted` and the glitched cases settle these in chunk 3.
