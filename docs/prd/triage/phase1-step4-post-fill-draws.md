---
title: "Port the post-fill draws"
description: "The C generator stops drawing at the end of the fill, so every seed's texts, credits, hints, prize music, digging game, RNG block and enemy drops (everything VT draws after the fill) are unproven, and no case can claim `all`."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 4, copied from `docs/plan-1.md`)

> **Port the post-fill draws** (texts, credits, hints, `writeToRom`'s own draws) into the C generator, one stage of step 2 at a time in VT's draw order. Verified by each case's claim reaching, in turn, each ported stage it draws in (a stage with no draw in the case's attempt 0, such as `applyHints` with hints off, `writeItem` without map-on-pickup, or a `writeToRom` stage when the winning attempt is not 0, is skipped for that case), then every case moving back to `all` in `claims.txt`, with `e2e` and `e2e-combos` green.

## Facts at `6fa2338` (checked)

- `stages.txt` lists seventeen stages. The seven after `fill`, in VT's draw order, are `setTexts`, `randomizeCredits`, `applyHints` (`Location::getHint`'s draws included), `writeItem`, `setDiggingGameRng`, `writeRNGBlock` and `writePrizePacksToRom` (step 1.2's note). `RandoStage` names them the same way, and `test_generate` checks that the two tables match.
- In VT, the first three run inside `Randomizer::randomize`, after the fill and **before** `isWinnable`, so they run on every attempt that completes the fill, the losing ones included. The last four run inside `World::writeToRom`, which the oracle calls once, on the winning attempt, after `isWinnable`. Their trace lines carry the winning attempt's number, and they continue its draw count.
- Every claim in `claims.txt` is `<case> fill`. `combo_claims.txt` exists and holds no claim yet. `e2e-combos` is in the root `make check`.
- `rando_generate_until`, given a stage after the fill, stops on the first winnable attempt, and the stages after the fill make no draw. `test_generate`'s `check_stop_after_fill`, `check_baseline_complete` (805 lines) and `check_retry_identity` (the golden up to its last `fill` line) all rely on that.
- `rando_fill_rest` leaves the pools' contents unspecified. VT's `HintService`, however, reads `Randomizer::$advancement_items`: the advancement list as `prepareWorld` shuffled it, which the fill never changes.
- `RandoConfig` has no Bow overflow count (`world.c` has a `TODO` for every `item.pool` overflow count but Shield), and no `spoil.Hints`. The hints value lives only in the host's `RandoCase.hints`. The spoiler writer prints `"hints": "on"` as a constant.
- Draws per stage in the 44 goldens' attempt 0:

  | Stage | Draws |
  |---|---|
  | `setTexts` | 255, or 291 under the hard, expert and crowd_control pools |
  | `randomizeCredits` | 78 in every case |
  | `applyHints` | 176 to 341 |
  | `writeItem` | 22 under `dungeon_items` `mc`, `mcs` and `full`, none elsewhere |
  | `setDiggingGameRng` | 1 |
  | `writeRNGBlock` | 1024 |
  | `writePrizePacksToRom` | 63 |

## Step 3's outcome (hypotheses)

Step 3 ("Golden cases for the new settings and for combinations") is not done. Its PRD is `docs/prd/triage/phase1-step3-golden-combination-cases.md`, and only its chunk 1 (`e2e-combos`, `combo_claims.txt`, `gen_golden.sh` over two directories) has landed. Every decision below that depends on what step 3 leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it holds. A hypothesis that does not hold is handled by the rule given with it.

- **H-3-1**: `cases/` holds 50 single-setting cases: the 44, plus `hints-off` and `heart_beep-{off,half,quarter,double,normal}`. All 50 are claimed `fill` in `claims.txt` and are green.
- **H-3-2**: `combos/` holds the combination cases (23 are planned). Every supported one is claimed `fill` in `combo_claims.txt`, and `e2e-combos` is green.
- **H-3-3**: step 3 lists any combination the generator refuses with `RANDO_UNSUPPORTED`, and leaves it unclaimed with a comment line in `combo_claims.txt`. This step does not touch such a case: it stays unclaimed, and it is not counted in "every case moving back to `all`".
- **H-3-4**: `hints-off` is green at `fill`. Since `fill` checks the spoiler's `meta` section, step 3 made the spoiler's `hints` value follow the case. Step 3 decides how that value reaches the spoiler writer. Chunk 4 below moves it into `RandoOptions` whatever form step 3 gave it, and keeps the spoiler output unchanged.
- **H-3-5**: `hints-off`'s attempt 0 has no `applyHints` draw (H-2-8 in step 3's PRD). Up to the fill, the heart-beep cases make exactly `baseline`'s draws.
- **H-3-6**: some combination may win at an attempt other than 0, or may lose attempt 0 before the fill ends. The case reader, `run_e2e.sh` and `rando_gen` handle both cases already, through the winning-attempt retry and the refusal of a stage with no attempt-0 draw.
- **H-3-7**: step 3 changes nothing in `generate.c`'s stage machinery, in `stages.txt` or in `run_e2e.sh`. Any fix it makes is inside the pre-fill or fill modules, behind unchanged interfaces.
- **H-3-8**: `dungeon_items-full+state-{standard,inverted,retro}` and `glitches-major_glitches+dungeon_items-full` draw in `writeItem`. `state-retro` combinations turn on `rom.genericKeys`, which changes `getHint`'s item name for keys.
- **H-3-9**: the 44 existing goldens and the oracle fixtures are byte-identical to `6fa2338`'s, except for the pool fixtures' six new records.

If H-3-1 or H-3-2 does not hold (the cases are missing, or some are not claimed), this step runs over the cases that are claimed. The step note lists the difference. Claiming step 3's cases is not done here.

## Problem Statement

The owner will play seeds that the console generates. For a seed to be the seed alttpr.com would have made, the C generator must draw, after the fill, exactly what VT draws:
- the uncle's, blind's, tavern man's, Ganon's and the triforce room's texts;
- the progressive bow hint;
- the end credits;
- the fifteen telepathic-tile hints;
- the prize music under map-on-pickup;
- the digging game's prize count;
- the 1 KiB RNG block;
- the enemy drop table.

Today the C generator stops drawing at the end of the fill. Every one of these is missing, so none of the ROM write steps (5 to 11) has the values it needs to write. No case can claim `all`, so nothing proves that a whole seed matches VT.

Worse, VT makes three of these draws before checking winnability, on losing attempts too. A seed that needed a retry therefore already differs from VT from its second attempt on, even in the item placement the spoiler shows.

## Solution

The C generator gains the seven post-fill stages, ported one at a time in VT's draw order:
- `setTexts`, `randomizeCredits` and `applyHints` run on every attempt that completes the fill, before winnability, as in `Randomizer::randomize`;
- `writeItem`, `setDiggingGameRng`, `writeRNGBlock` and `writePrizePacksToRom` run once, on the winning attempt, as in `World::writeToRom`.

Each stage records what it chose (which text, which credit, which hint on which tile, which music byte, the digging count, the RNG block, the drops), as ids and indices. These go in one result struct that the ROM writer of steps 5 to 11 reads. This step writes no ROM byte and no string.

After each stage is ported, every case that draws in it has its claim moved to that stage, in `claims.txt` and `combo_claims.txt`. A case with no draw in that stage keeps its claim. After the last stage, every claimed case moves to `all`: full trace, `attempts` and `spoiler.json`. `e2e` and `e2e-combos` are green at every commit.

## User Stories

1. As the owner, I want the console's seed to draw its texts as alttpr.com does, so that the uncle, the blind, the tavern man and Ganon say what that seed says on alttpr.com.
2. As the owner, I want the progressive bow hint drawn as VT draws it, so that Ganon's "did you find the arrows" text points to the right place.
3. As the owner, I want the end credits drawn as VT draws them, so that the ending is that seed's ending.
4. As the owner, I want the telepathic tiles' hints drawn as VT draws them, so that each tile's hint is true for my seed.
5. As the owner, I want hints off to draw no hint, so that a hints-off seed matches alttpr.com's hints-off seed.
6. As the owner, I want the prize music under map-on-pickup drawn as VT draws it, so that a keysanity dungeon does not give its prize away through its music.
7. As the owner, I want the digging game, the RNG block and the enemy drops drawn as VT draws them, so that the game's randomness is the seed's.
8. As the owner, I want a seed that needed a retry to match VT on every attempt, so that the retry does not change which seed I get.
9. As the owner, I want every case to claim `all`, so that each golden's whole trace, its attempts and its spoiler are proven, not only its fill.
10. As a ROM-writer agent (steps 5 to 11), I want every post-fill choice recorded as ids and indices in one struct, so that I write the ROM from those values and never draw again.
11. As a ROM-writer agent, I want the struct to say which location holds VT's alternate progressive bow, so that step 6 writes its `0x65` byte where VT does.
12. As a ROM-writer agent, I want the enemy drops assigned to the world's prize packs as VT assigns them, so that step 7 reads them where the pre-fill prize packs already live.
13. As a ROM-writer agent, I want the RNG block stored as the bytes VT packs, so that a drawn 256 is already the 0 that VT writes.
14. As a ROM-writer agent, I want each hint stored as its location and its item- and location-name alternative indices, so that step 9 builds the string with no draw.
15. As a porting agent, I want one stage per chunk, in VT's draw order, so that a divergence points at a single stage.
16. As a porting agent, I want `rando_generate_until` to stop right after any post-fill stage, so that `rando_gen` and `run_e2e.sh` can claim each stage as it lands.
17. As a porting agent, I want the in-randomize stages to run on losing attempts too, so that the trace of a retried case matches VT's line for line.
18. As a porting agent, I want winnability checked after `applyHints`, as VT does, so that the stage order is VT's even where no case shows the difference yet.
19. As a porting agent, I want the advancement list kept as `prepareWorld` left it, so that the hints read the list VT reads, not what the fill left of it.
20. As a porting agent, I want the hint alternative counts checked against VT's own tables, so that a location no golden hints at still draws the right number of times on the console.
21. As a porting agent, I want each VT quirk the port reproduces (no draw for a multi-location hint pick, a deduplicated hint merge, `random()` on an empty collection) named in the contract of the function that reproduces it, so that nobody "fixes" it back.
22. As a porting agent, I want the Bow overflow count ported into the config, so that the bow hint branches as VT's does under the hard and expert pools.
23. As a porting agent, I want hints on/off to be a generator input, not only a case-file field, so that the Generate tab (Phase 3) can turn hints off.
24. As a porting agent, I want a claim moved only in the commit that makes it pass, so that `make check` is green at every commit.
25. As a porting agent, I want a case with no draw in a stage to keep its claim, so that `run_e2e.sh` never refuses a claim.
26. As the net, I want `test_generate`'s stop contracts rewritten to the new stage order deliberately, with the reason in the commit, so that no expected value changes silently.
27. As the net, I want the unit tests of each stage to check its draw count and its edge cases on hand-built worlds, so that a mistake fails `make test` before `e2e`.
28. As the net, I want `make test` to stay fast, so that the fast loop stays the fast loop.
29. As the net, I want every existing golden and fixture byte-identical after this step, so that the port is proven against goldens that did not move.
30. As the net, I want a new oracle fixture added in its own commit, with every existing golden unchanged, so that the source of truth only grows.
31. As the net, I want `rando/` to stay pure C99 with no `malloc`, so that the same code runs on the Miyoo.
32. As a reviewer, I want the step note to list, per stage, the cases that moved to it and the ones skipped and why, so that I can check the claim progression without rerunning it.
33. As a reviewer, I want the step note to record the per-case time against step 1's reference, so that the plan's final ±10% budget is known before Phase 2.
34. As a reviewer, I want the step note to say which step-3 hypotheses held, so that I know what this step was built on.

## Implementation Decisions

### The generator's stage order (`generate.c`, `rando.h`)

- One attempt runs, in this order:
  1. the ten pre-fill stages and the fill, unchanged;
  2. `setTexts`, `randomizeCredits` and `applyHints`;
  3. the winnability check;
  4. on a winning attempt, `writeItem`, `setDiggingGameRng`, `writeRNGBlock` and `writePrizePacksToRom`.

  A fill failure fails the attempt before step 2, as VT's exception does. The `RANDO_FAULT_IS_WINNABLE` fault point moves with the winnability check, after `applyHints`.
- Draw numbering: the writeToRom stages continue the winning attempt's draw count and carry its attempt number. This is what the oracle's trace does.
- `rando_generate_until`'s contract is reworded:
  - before the fill: unchanged;
  - at `fill` or later: it returns `RANDO_INCOMPLETE` on the first winnable attempt, right after stage `last`.
    - Every attempt runs the in-randomize stages up to `last`, or all three when `last` is after them.
    - The winning attempt then runs the writeToRom stages up to `last`.
    - A stage not ported yet completes with no draw.
  - `RANDO_STAGE_NONE` runs every stage and returns `RANDO_COMPLETE`.

  `rando_gen` needs no change: its `won` rule (a stop at `fill` or later) still holds.
- Winnability: VT's `setTexts` replaces one progressive bow with an alternate bow of the same name. The C port records that swap in the result struct and leaves the world's item unchanged. So the winnability check, the playthrough and the spoiler read the world as before. Winnability and the spoiler then match VT's, because the swapped item has the same name. `e2e`'s `fill` checks (every spoiler section, `attempts`) prove it on every case.
- The world keeps VT's other post-fill mutation that this step owns: `writePrizePacksToRom` assigns the drops into `world.prize_pack_drops`. The other writeToRom mutation, which fills empty locations with `Nothing`, is a ROM write. It belongs to step 6, not here. The spoiler must stay identical, which `all` proves.

### The result struct and the stage functions (new public header, `rando_postfill`)

- A new public header declares `RandoPostFill`, a caller-owned value struct with no pointers, which `RandoGen` holds by value (`gen->post`). Each attempt clears it before `setTexts`. It holds:
  - **texts**:
    - the alternative index picked for `uncle_leaving_text`, `blind_by_the_light`, `kakariko_tavern_fisherman`, `ganon_fall_in` and `end_triforce`;
    - the bow hint's branch: alternate bows, silvers, or fake hint. For the alternate branch, the two bow locations in VT's pop order. For the silvers branch, the silvers location. For the fake branch, its alternative index.
    - the alternate-bow location: the one whose item step 6 writes as `0x65`, or `RANDO_LOCATION_NONE`.

    The texts with no draw (the pendant and crystal locations, the tower and Ganon signs) are derived from the world by step 9, and are not stored.
  - **credits**: the index picked for each of `castle`, `sanctuary`, `kakariko`, `lumberjacks`, `bridge`, `woods` and `well`, and the smithy coin flip.
  - **hints**:
    - whether hints were on;
    - per telepathic tile, in VT's tile order: none, a location hint (location id, item-name alternative index, location-name alternative index), or a joke (its index). A multi-location hint pick never yields a hint (see the traps below), so no pick-pair kind is stored. If the goldens show otherwise, a pick-pair kind is added then.
  - **writeItem**: the music byte drawn per prize location, in VT's region order, with a count.
  - **setDiggingGameRng**: the drawn value, 1 to 30.
  - **writeRNGBlock**: 1024 bytes, each the drawn value truncated as `pack('C')` does (256 becomes 0).
- One function per stage. Each draws through the given `RandoPrng`, writes into `RandoPostFill` (or into the world, for the drops), and states its exact draw count in its contract, as `rando_prepare.h` does:
  - **set texts**: the world, the PRNG, the result;
  - **randomize credits**: the PRNG and the result (78 draws, always);
  - **apply hints**: the world, the PRNG, the advancement snapshot and its count, and the result (no draw when hints are off);
  - **prize music**: the world, the PRNG, the result (two draws per prize location of a region that has music addresses, only under `rom.mapOnPickup`; none otherwise);
  - **digging game**: one draw;
  - **RNG block**: 1024 draws;
  - **prize packs to ROM**: the world and the PRNG (one `fy_shuffle` of the drop pool, 63 draws at VT's defaults), assigning drops to the empty slots in `getEmptyDropSlots` order.

  `generate.c` labels each call with its `RandoStage` and calls nothing else after the fill. The sources may be split by stage (texts, credits, hints, writeToRom draws) behind the one header.
- The text and hint alternatives are stored as counts, not strings. Step 9 adds the strings, with the same keys and in the same order.
  - The string files have fixed counts at `219fcafd`: `uncle` 55, `tavern_man` 51, `blind` 77, `ganon_1` 31, `ganon_phase_3_no_silvers` 36, `triforce` 39 and `hint` (the jokes) 37.
  - The hint tables hold, per item raw name and per location name of `resources/lang/en/hint.php`, either "not an array" or the array's length.

### The traps the port must reproduce (from VT at `219fcafd`; the goldens are the final word)

- `Collection::random()` on an empty collection returns null **with no draw**. `randomCollection(n)` makes `min(n, count)` draws.
- `fy_shuffle` of n elements makes n draws, down from `get_random_int(0, n-1)`. `Arr::first` takes element 0 of the result.
- `setTexts`:
  - `randomCollection(2)` over the progressive bow locations is drawn unconditionally, before the branch on `item.overflow.count.Bow`. VT's default for that count is 2; the hard, expert and crowd_control pools set it to 1. This overflow count is ported into `RandoConfig` with the World constructor's per-pool values, which closes that part of `world.c`'s `TODO`.
  - The alternate bow goes to the location popped first, which is the one drawn second.
  - The fake silvers hint is drawn even under crowd_control, which then overrides its text.
- `Location::getHint`:
  - it draws for the item name only if `hint.item.<raw name of getTarget()>` is an array, and for the location name only if `hint.location.<raw name>` is an array. A missing key returns the key itself, with no draw;
  - under `rom.genericKeys`, a key's item name is `KeyGK`'s.
- `LocationCollection::getHint` looks names up with `getName()`, which carries a `:<world id>` suffix. Its translation lookups therefore miss, and make no draw.
- `HintService`'s pair picks (`["Misery Mire - Big Key Chest", …]`) filter `getLocations()` by `getName()`, which has the suffix, against bare names. As read, a pair pick matches nothing: it yields no hint, consumes no tile, and makes no draw beyond the pick shuffle. The implementer confirms this against the goldens' draw counts and records it. If the goldens disagree, the goldens win.
- `HintService`:
  - the five pick shuffles run over 10, 9, 8, 7 and 6 indices;
  - the pick tile is popped only when the hint is non-null;
  - `hintables` is the advancement snapshot filtered by item class and raw name, in snapshot order;
  - each hint item's location is drawn by `random()` over the locations holding it, minus Medallion, Fountain, Prize and Trade, in world location order. A null result is dropped after the draw count is settled;
  - the count is `get_random_int(floor((tiles - hints) / 2) - 1, tiles - hints - 1)`;
  - `randomCollection(k)->merge($hints)` rebuilds a location collection keyed by name. A hint location already drawn keeps its first position and is not added twice. `pop()` takes from the end;
  - a tile with no location left draws a joke (37 draws).
- `writeItem`: only the prize locations of regions with `music_addresses` draw, in `getRegions()` order and, within a region, in its location order. Only `rom.mapOnPickup` draws. Without it, the byte follows from the prize, with no draw, and step 6 owns it.
- `writeRNGBlock` draws `get_random_int(0, 0x100)`: 257 values packed into one byte.
- `getDropsPool`:
  - it builds the pool in `config/item.php`'s `drop` order (Heart 13, RupeeGreen 9, RupeeBlue 7, RupeeRed 6, BombRefill1 7, BombRefill4 1, BombRefill8 2, MagicRefillSmall 6, MagicRefillFull 3, ArrowRefill5 5, ArrowRefill10 3, Fairy 1, then Bee and BeeGood at 0), each count capped at 63;
  - it shuffles the whole pool, even when fewer slots are empty;
  - the NoFairy and rupee-bow byte swaps are ROM writes (step 7), not draws.

### Hints as a generator input

- `RandoOptions` gains `bool hints`, which `rando_options_init` sets to true. It is not a `RandoOptionKey`: the eleven keys stay the oracle's `CONFIG_KEYS`, and `rando_options_set` is unchanged.
- `rando_config_from_options` maps it to a new `RandoConfig` field for `spoil.Hints`.
- The case reader fills `options.hints`, and `RandoCase.hints` goes away. The spoiler writer reads the options value (H-3-4: whatever step 3 used is folded into this). The spoiler output is byte-identical.

### The advancement snapshot

- `RandoGen` keeps a copy of the advancement list as `rando_prepare_pools` leaves it, taken right after `prepareWorld`, before the fill. The fill's freedom to change the pools stays as it is. The snapshot's capacity is `RANDO_POOL_CAPACITY`.

### The oracle fixture for the hint and string tables (chunk 3)

- A new seed-independent fixture, `oracle/hint_tables.txt`, is written by a new oracle script that `gen_golden.sh` runs beside `text_default.php`. It reads VT's `resources/lang/en/hint.php` and `strings/*.txt` through VT's own code: `__()` for the hint tables, and `Randomizer::getTextArray`'s logic for the string files. Its lines:
  - `item <raw name> <n>`, with `n` 0 when the value is not an array;
  - `location <name> <n>`, likewise;
  - `strings <file> <count>`.
- It joins the net through `test_postfill --fixtures` under `make logic`. Every item and location the C tables know must match its line, and every line must name an item or location that the C tables know, or one listed as unknown to the C world with its reason.
- The commit adds the script, the `gen_golden.sh` line and the fixture, and nothing else. Run with `--out` into `tmp/` and `diff -r` against the tree: every existing golden and fixture is byte-identical. This is an oracle addition, not a regeneration.
- Without PHP in the session (a setup problem), the chunk stops and asks the owner instead of transcribing the tables unchecked.

### Claims

- After each stage's chunk, each case's claim, in `claims.txt` and `combo_claims.txt`, is the latest ported stage that has a draw in its attempt 0. A case with no draw in the new stage keeps its claim. The claims move in the commit that makes them pass.
- The last chunk replaces every claim with `all`. This covers every claimed case of both files, but not an unsupported combination (H-3-3).
- A case that diverges at a stage is fixed in that stage's chunk, through `fix-bug`, before the claim moves. A claim is never held back to hide a divergence.

### Measure and the step note

- At the end, `make -C tests/rando measure` runs in a throwaway worktree of step 1's commit (`40314c3`) and in the current tree, in the same session, with the same settings, as step 3 did.
  - Step 4's text sets no time budget, so this is recorded, not gated.
  - The plan's §4 does gate the final per-case time at ±10%. A case over budget is investigated in this step, and its cause is recorded.
  - The post-fill work, about 1,700 draws and a few location walks, is expected to cost far less than the fill's milliseconds.
- The plan's §6 gains a "Step 1.4" note:
  - which H-3 hypotheses held;
  - per stage, the cases that moved to it, and the ones skipped with their reason (no draw in attempt 0, a winning attempt other than 0, a fill lost in attempt 0);
  - the VT quirks confirmed against the goldens (the pair picks in particular);
  - the measure numbers, and any `over` on the inherited budgets, as information only.

## Implementation Chunks

1. **The post-fill skeleton and `setTexts`.**
   - The new header and `RandoPostFill`, held in `RandoGen` and cleared per attempt.
   - The new stage order, with winnability after the three in-randomize stages and the writeToRom stages on the winning attempt only (all still without draws except `setTexts`), and the reworded `rando_generate_until` contract.
   - The advancement snapshot.
   - The Bow overflow count in `RandoConfig`.
   - `setTexts` with its string-file counts.
   - `test_generate`'s stop contracts rewritten deliberately: the stop after `fill`, the stop at each post-fill stage, `check_baseline_complete`'s line count and `check_retry_identity`'s expected tail. The commit message gives the reason. Its trace buffers are raised if a retried trace no longer fits.
   - Every case moves to `setTexts`, since every case draws in it when its attempt 0 completes the fill.
2. **`randomizeCredits`.** Every case moves to `randomizeCredits`.
3. **The hint and string tables fixture.** The oracle script, the `gen_golden.sh` line and `hint_tables.txt`, in one commit, with every existing golden byte-identical (`--out` and `diff -r`). No C change and no claim moves.
4. **Hints as an input, and `getHint`.**
   - `RandoOptions.hints` and the config field.
   - The case reader and spoiler writer folded onto it (H-3-4).
   - The C hint alternative tables, checked by `test_postfill --fixtures` against `hint_tables.txt`.
   - The `Location::getHint` and `LocationCollection::getHint` equivalents, unit-tested for their draw counts (generic keys, missing keys, non-array values).

   No claim moves: nothing calls them from the generator yet.
5. **`applyHints`.** The whole `HintService::applyHints`, with its traps. Every case that draws in `applyHints` moves to it. `hints-off` keeps `randomizeCredits` (H-3-5). The pair-pick finding is recorded for the step note.
6. **`writeItem`.** The prize music under map-on-pickup. The `dungeon_items` `mc`, `mcs` and `full` cases and their combinations (H-3-8) move to `writeItem`.
7. **`setDiggingGameRng` and `writeRNGBlock`.** Both stages are one draw site each, with no world input, and are ported in one chunk. Each case whose winning attempt is 0 moves to `writeRNGBlock`.
8. **`writePrizePacksToRom`, then `all`.**
   - The drop pool and its assignment into the world's prize packs.
   - Each eligible case moves to `writePrizePacksToRom`, and `make check` runs green.
   - In the same chunk, a second commit replaces every claim of both files with `all`. A case that is not green at `all` is fixed through `fix-bug` first.
9. **Measure and the step note.** Both measure runs, as above, and §6 "Step 1.4".

## Testing Decisions

- A good test drives a public interface and checks what it returns or records: a stage function's draw count through a counting PRNG observer, the result struct's fields, `rando_generate_until`'s status and trace, and `run_e2e.sh`'s lines. No test inspects static helpers.
- **`test_postfill`** (new, registered in `UNIT_TESTS` and in `LOGIC_TESTS` for its `--fixtures` mode). Prior art: `test_prepare`, which drives stage functions on worlds built with `rando_world_init` and hand placements, and states exact draw counts.
  - Credits make exactly 78 draws, and each recorded index is element 0 of VT's shuffle for a known PRNG state.
  - `setTexts` makes 255 draws on a world with two progressive bows and the default overflow count. It makes 291 with an overflow count of 1 and no silvers, and records the alternate-bow location as the bow drawn second.
  - Hints off makes no draw and records no tile.
  - `random()` on an empty set makes no draw.
  - Under generic keys, a key's item name is `KeyGK`'s.
  - A hint location drawn twice appears once, at its first position.
  - The prize-music function makes no draw without map-on-pickup, and two per eligible prize with it.
  - The RNG block stores a drawn 256 as 0.
  - The drop pool has 63 entries in `config/item.php` order before the shuffle, and fills exactly the empty slots.
  - `--fixtures`: the C hint and string tables against `hint_tables.txt`.
- **`test_generate`** (prior art: `check_golden_stop`, `check_baseline_fill`, `check_retry_identity`, `check_stop_on_winning_attempt`):
  - for each ported stage, a stop there on `baseline` reproduces the golden's first lines, up to that stage's last draw, and the golden's next line belongs to a later stage;
  - `dungeon_items-full` covers the `writeItem` stop, and `hints-off` (H-3-1) the `applyHints` skip;
  - an `IS_WINNABLE`-fault retry shows attempt 0 carrying the in-randomize stages and no writeToRom stage;
  - a complete `baseline` run equals the whole golden trace.

  These rewrites are the deliberate change of this step, each one stated in its commit.
- **`test_case_reader`**: reading `hints` into `options.hints`, with the default on and `off` read.
- **`e2e` and `e2e-combos`** are the step's acceptance test: every claim moves as described, and every case ends at `all`, green.
- **`lint`** covers every new `rando/` source. `selftest` is unchanged: it does not depend on which stages a real case reaches.
- `make test` stays fast. The new tests run on hand-built worlds and on a few golden traces already read today.

## Out of Scope

- Any ROM write: the `rando_rom` module, the write callback and the write-log claims (step 5); items and locations, including writing the alternate bow and the `Nothing` fill (step 6); settings and drop bytes (step 7); credits (step 8); the text encoder, the strings and the hint text (step 9); SRAM (step 10); the checksum and heart beep (step 11).
- The text and hint strings themselves. Only their counts and the chosen indices are part of this step.
- `check-e2e`'s handling of write logs and of `e2e-combos` (step 5). Running `check-e2e` over every case (step 12). The qemu-arm run of the post-fill stages (step 12, the end of Phase 2).
- Claiming step 3's cases, if step 3 left any unclaimed (H-3-1, H-3-2). Changing step 3's unsupported list (H-3-3).
- A UI row for hints (Phase 3 step 4). `rando_options_set` learning a `hints` key.
- Any `item.pool` overflow count other than Bow, beyond what `setTexts` needs.
- Multiworld, custom drop counts (`drop.count.*`) and `spoil.BootsLocation` on: none is reachable from the matrix. They are ported only as far as VT's defaults need.

## Further Notes

- **Why winnability moves.** All 44 existing cases win at attempt 0, so no golden yet shows the difference. Still, VT's losing attempts make the three in-randomize stages' draws, and an `all` claim on a retried case compares them. Porting the order now keeps the generator VT's, rather than waiting for a combination case to expose it (H-3-6).
- **Why indices, not strings.** Step 9 owns the strings, and the plan keeps one step per concern. The struct is the contract between step 4 and steps 5 to 11. Its field list may grow there, but this step decides what is drawn.
- **Size.** `RandoPostFill` adds about 1.1 KiB to `RandoGen`, most of it the RNG block. The hint tables add a few hundred bytes of counts. The inherited DS size budget is informational (step 1's note).
- **No `guideline` agent was consulted.** Every point was settled from the plan, VT at `219fcafd` (read in `tmp/vt`), the code at `6fa2338` and steps 1.1 and 1.2's notes, and each decision is recorded above. Chunks 5 and 8 may consult one if a divergence needs more than one `fix-bug` cycle.
