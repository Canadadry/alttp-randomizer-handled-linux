---
title: "Port the items and locations writes"
description: "The C ROM writer writes no item byte, so the first 315 writes of every golden rom_writes.txt (the writeItem group) are unproven and no case can claim its write log at all."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 6, copied from `docs/plan-1.md`)

> **Port the items and locations writes.** Verified by every case's claim reaching that group, plus `tests/rando/unit/test_rom_items.c`.

The step 5 PRD (`docs/prd/triage/phase1-step5-rom-skeleton-write-claims.md`) hands one duty to this step: the test that nothing is written after a refused write, which has nothing to call until a group writes.

## Facts at `c347c7d` (checked)

- **The target.** In all 73 goldens (50 in `tests/rando/cases/`, 23 in `tests/rando/combos/`), the log opens with exactly 315 lines tagged `writeItem`, each a single byte. They include the prize music writes and the Hera basement write, which VT makes from inside `writeItem` overrides (baseline: `01559A 16`, `04E3BB E4`, `0155C9 16`, all tagged `writeItem`).
- **VT's walk** (`tmp/vt`, `app/World.php` `writeToRom`, not multiworld): for each region of `getRegions()`, in order, `writeItem` on every non-empty location of the region in its list order, then on every empty location, after `setItem(Nothing)`. `setText` and `setCredit` calls in that loop only fill buffers (steps 8 and 9).
- **`Location::writeItem`** (`app/Location.php`):
  1. The item is the location's item.
  2. `rom.vanillaKeys`, a `Key`-class item that is one of the region's `region_items` → `Key`, unless the location is `Secret Passage` or `Link's Uncle` and the item is `KeyH2`.
  3. `rom.vanillaBigKeys`, a `BigKey`-class region item → `BigKey` (no exception).
  4. `rom.vanillaMaps`, a `Map`-class region item → `Map`, unless the location is `Secret Passage` or `Link's Uncle` and the item is `MapH2`.
  5. `rom.vanillaCompasses`, a `Compass`-class region item → `Compass` (no exception).
  6. `rom.genericKeys`, any `Key`-class item (the result of step 2 included) → `KeyGK`.
  7. For each key of the location's address array, in insertion order, write `item_bytes[key]` at `address[key]`, one byte, skipping a key whose address or byte is null or absent.
  The class tests use the item's own class: an `ItemAlias` (`UncleSword`, `ShopKey`, `ShopArrow`) is no `Key`, but its bytes are its target's.
- **Overrides that write** (the others only call `setCredit`/`setText`):
  - `Location\Prize::writeItem` (pendants, crystals, events): after the parent, if the region has `music_addresses`, one byte at each of them, in order: under `rom.mapOnPickup` the drawn `0x11`/`0x16`, otherwise `0x11` for a `Pendant`-class item and `0x16` for anything else (an `Event` included).
  - `Location\Standing\HeraBasement::writeItem`: after the parent, `0x4E3BB` = `0xE4` if the location holds `Key` or `KeyP3` (its own item, before any substitution), otherwise `0xEB`.
- **Address arrays** are per location name, the same in every variant (the inverted regions re-add the same locations with the same addresses). Their shapes:
  - one address (most locations);
  - prizes: `[null, a1..a6]` (keys 0 to 6, key 0 null);
  - medallions: `Turtle Rock Medallion` `[null, 0x180023, t0, t1, t2]`, `Misery Mire Medallion` `[null, 0x180022, m0, m1, m2]`;
  - events (`Zelda`, `Agahnim`, `Agahnim 2`, `Ganon`, `Bomb Merchant`): `[]`, no write;
  - `Blacksmith`: `0x18002A` when `region.swordsInPool` (default true; no alttpr.com setting turns it off), else `0x3355C`.
- **Item bytes** (`app/Item.php` `Item::all`): one byte for most items; pendants seven bytes; crystals `[null, b1..b6]`; medallions `[b0, b1, t0, t1, t2, m0, m1, m2]` with string keys `t*`, `m*`; events `[null]` except `Triforce` `[0x6A]`.
- **Music addresses**: Eastern 1, Desert 4, Hera 3, Palace of Darkness 1, Swamp 1, Skull Woods 8, Thieves' Town 1, Ice 1, Misery Mire 1, Turtle Rock 4, Ganon's Tower 1 (for `Agahnim 2`). Hyrule Castle Tower has none. No inverted region overrides them.
- **The alternate bow**: `Randomizer::setTexts` replaces one ProgressiveBow location's item with `new Item\Bow('ProgressiveBow', [0x65])`, so that location writes `0x65`.
- **The C side** (step 4 is done, step 1.4 note):
  - `RandoGen` holds `world` and `post` (`RandoPostFill`, `rando/include/rando_postfill.h`) by value.
  - The post-fill result holds `texts.alternate_bow` (the swapped location, or `RANDO_LOCATION_NONE`) and `prize_music` (count 0 without map-on-pickup; otherwise the locations, in the walk's order, and each one's music byte).
  - `RandoConfig` holds `rom_vanilla_keys`, `rom_vanilla_big_keys`, `rom_vanilla_maps`, `rom_vanilla_compasses`, `rom_generic_keys`, `rom_map_on_pickup` and `region_swords_in_pool`.
  - `RandoRegionInfo.items` is the region's `region_items`.
  - The layout functions give each region's locations in VT order, and `rando_prize_music` already walks regions and locations in `writeToRom`'s order.
  - No ROM address and no item byte exists anywhere in `rando/`.
- **Golden coverage**:
  - `glitches-*` cases set the vanilla dungeon-item flags;
  - `state-retro` (and its combos) sets `genericKeys`;
  - `dungeon_items-mc`, `-mcs`, `-full` set map-on-pickup;
  - `state-inverted` has the inverted layout.
  - No case turns `swordsInPool` off: the `0x3355C` branch is proven only by the unit test, from VT's source.

## Step 5's outcome (hypotheses)

Step 5 is not done. Every decision below that depends on what it leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it holds. A hypothesis that does not hold is handled by the rule given with it.

- **H-5-1**: a public `rando_rom` module exists. Its entry point is `rando_rom_write(const RandoGen *gen, RandoHeartBeep heart_beep, const RandoRomWriter *writer)` returning `bool`. It calls seven static group functions in order (items first), all empty, and routes every write through one internal emit helper that stops at the first refused write. *If the entry point or writer differ in name or argument order*, this step keeps step 5's shape and fills its items group; nothing here depends on the exact spelling.
- **H-5-2**: the callback receives the context, the PC address, a byte pointer, a count (never 0) and a static method-tag string, and returns `bool` (false refuses). *If the tag is not passed per write*, the items group still emits `writeItem` through whatever step 5 uses to tag the log line.
- **H-5-3**: `tests/rando/write_groups.txt` exists, its first group is `items`, owning the tag `writeItem`, and `run_e2e.sh` accepts `<case> all items`, comparing the golden log's prefix up to its last `writeItem` line and printing `PASS <case> (all items)`. *If the group has another name*, the claims use that name.
- **H-5-4**: `rando_gen` writes `rom_writes.txt` through a host log writer, in the oracle's `%06X <HEX> <tag>` format, on every complete run. So the items writes reach the C log with no host change.
- **H-5-5**: no real write claim exists yet. Every line of `claims.txt` and `combo_claims.txt` is `<case> all`, and `make check` is green. *If some case is still at a stage*, it cannot take a write claim; the step stops and reports it instead of claiming around it, since the step text requires every case.
- **H-5-6**: `test_rom` exists and asserts that `rando_rom_write` on a completed baseline makes no callback call. That expectation is rewritten here on purpose (step 5 says so).
- **H-5-7**: the refusal-rule test was handed to this step and does not exist yet. *If step 5 found a way to write it*, this step keeps it and adds only the items-specific case below.
- **H-5-8**: the emit helper and group functions are static in the `rando_rom` source. *If they are*, this step moves the emit helper behind a private header under `rando/src/`, as `postfill_draw.h` is shared today, so the items group can live in its own source file. *If step 5 already shares it*, nothing moves.

## Problem Statement

The owner will play ROMs the console patches. Each seed's item placement reaches the game only through the item writes: 315 bytes at the chests, NPCs, drops, prizes and medallion tablets, plus the dungeon music and the Hera basement fanfare that VT ties to them. Today the C ROM writer writes none of them. A ROM patched by it would hold the vanilla items, whatever the spoiler says. The net cannot catch this either: no case claims any write group, because the first group, items, is empty.

## Solution

- The `rando_rom` items group writes, through the caller's writer, exactly the writes VT's `writeToRom` item loop makes, in its order, each tagged `writeItem`:
  - the location walk;
  - the dungeon-item substitutions;
  - the prize bytes and their music;
  - the medallion tablets;
  - the Hera basement byte;
  - the alternate bow.
- It reads the generator as const and draws nothing: the music drawn under map-on-pickup and the alternate bow come from the post-fill result step 4 stored.
- `test_rom_items` proves each rule on hand-built worlds through a recording writer, including the refusal rule step 5 handed over.
- Every claim in `claims.txt` and `combo_claims.txt` becomes `<case> all items`, so `e2e` and `e2e-combos` compare all 73 logs up to the last `writeItem` line, byte for byte against VT.

## User Stories

1. As the owner, I want every chest, NPC, drop and standing item of my seed written into the ROM, so that the game hands me the items the spoiler lists.
2. As the owner, I want each dungeon's prize written at all six of its addresses, so that the map, the pendant/crystal display and the boss reward agree.
3. As the owner, I want the dungeon music to follow the prize (pendant or crystal) as VT sets it, so that the game sounds like an alttpr.com seed.
4. As the owner of a map-on-pickup seed, I want the music VT drew written, not the one the prize implies, so that the music does not give the prize away.
5. As the owner, I want the Turtle Rock and Misery Mire medallions written to their tablets and entrance checks, so that the right medallion opens each dungeon.
6. As the owner, I want the Hera basement's quick-key byte set as VT sets it, so that picking up a key there behaves as on alttpr.com.
7. As the owner, I want the alternate progressive bow written as `0x65` where VT put it, so that the silver-arrow hint text matches the item.
8. As the owner of a glitched seed, I want confined keys, big keys, maps and compasses written as their generic forms as VT does, so that the dungeon-item behaviour matches VT.
9. As the owner of a retro seed, I want every key written as the generic key, so that keys work in any dungeon.
10. As the owner, I want a location the pool left empty written as Nothing, so that no vanilla item leaks into the seed.
11. As the owner, I want the item at Link's Uncle written with its alias's target bytes, so that the uncle gives the progressive sword.
12. As the app (Phase 2), I want the item writes through my buffer writer to land inside the 2 MiB ROM, so that patching never corrupts memory.
13. As the app, I want the writer to stop at the first refused item write and return false, so that a failed patch is reported and not half-applied.
14. As a porting agent, I want the items group to read only the world and the post-fill result as const, so that it can never change the seed or draw.
15. As a porting agent, I want the ROM address and byte tables kept inside the ROM writer, so that the generator's world tables stay free of ROM knowledge.
16. As a porting agent, I want the items group in its own source file, so that steps 7 to 11 fill their groups without touching it.
17. As a porting agent (steps 8 and 9), I want the step note to say which `writeItem` overrides only buffer credits and texts, so that I know where those setter calls come from.
18. As a porting agent, I want the step note to say that VT's walk sets empty locations to Nothing in the world while the C writer leaves the world untouched, so that a later group reading an empty location knows to read it as Nothing.
19. As the net, I want every case and combination to claim `all items`, so that `make check` fails on any wrong item byte, address or order.
20. As the net, I want the trace, attempts and spoiler still compared before the write log, so that an item-write divergence is never reported on a seed that already differs.
21. As the net, I want a unit test per rule (walk order, empty after non-empty, each substitution and its exception, prize bytes, music both ways, medallions, Hera, Blacksmith, alias, alternate bow, events), so that the dev loop catches a wrong rule in milliseconds without a golden.
22. As the net, I want the `swordsInPool`-off Blacksmith address tested from VT's source, so that a branch no golden reaches is still proven.
23. As the net, I want the refusal rule tested on a real items write, so that the writer's contract holds from the first group that writes.
24. As the net, I want `test_rom`'s skeleton expectation rewritten on purpose, with the reason stated, so that no check is weakened silently.
25. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new tables and walk obey the library rules.
26. As the net, I want `make test` to stay fast, so that the dev loop stays seconds long.
27. As the net, I want every golden and fixture untouched, so that the source of truth does not move.
28. As `check-e2e`, I want an items divergence reported with the first differing `writeItem` line, so that `fix-bug` starts from the exact location and byte.
29. As a reviewer, I want the step note to record which H-5 hypotheses held, the claim count moved (50 + 23), and any golden the walk needed to match, so that I can check the step without rerunning it.

## Implementation Decisions

### Where the items group lives

- A new source file in `rando/src/` holds the items group and its private tables. It is called from the `rando_rom` entry point's items slot (H-5-1) and writes through the shared emit helper (H-5-8). It is not a public API: the public interface stays `rando_rom_write` alone, and the header's contract gains no function.
- It reads `gen->world` (variant, config, location items) and `gen->post` (alternate bow, prize music), both const, and never `gen->prng`.

### The tables (private, in that source)

- **Slots.** VT's array keys are modelled as thirteen fixed slots: `0` to `6`, then `t0`, `t1`, `t2`, `m0`, `m1`, `m2`.
- **Item bytes.** One entry per `RandoItemId` in VT's `Item::all` order (aliases excluded: an alias writes its `alias_target`'s bytes). Each entry gives, per slot, a byte or "absent", so that VT's `null` (a crystal's slot 0, an event's) and a missing key are both "absent". Built from `app/Item.php` at `219fcafd`.
- **Location addresses.** One entry per `RandoLocationId`: an ordered list of (slot, PC address) pairs, in VT's insertion order, with the null addresses left out (they are skipped anyway). Events have an empty list. `Blacksmith` has two alternatives, chosen by `region_swords_in_pool`.
- **Music addresses.** One list per `RandoRegionId`, in VT's order; empty for every region without `music_addresses`.
- The order of a write within a location is the location list's order. A pair is skipped when the item has no byte in that slot (a pendant's slot 0, a one-byte item in a medallion's `t*` slots).

### The walk

- The regions are walked in `RandoRegionId` order, and each region's locations come from the variant's layout (`rando_layout_region_location`), the same walk `rando_prize_music` makes. Per region: first every location holding an item, in list order, then every empty one, written as `Nothing`.
- An empty prize or event location is unreachable: VT's `Prize::setItem(Nothing)` throws. The writer asserts against it; it does not write.
- The world is not modified: an empty location is read as `Nothing` for its write only.

### One location's write

1. **Item.**
   - If the location is `post.texts.alternate_bow`, its bytes are the alternate bow's (`0x65` in slot 0), and no substitution applies (it is a `Bow`).
   - Otherwise the item is the world's item, empty read as `Nothing`.
2. **Substitutions**, in VT's order. They test the item id's own class (`RandoItemInfo.item_class`, so an alias is never a key) and membership in the location's region `items` list:
   - vanilla keys, with the `Secret Passage`/`Link's Uncle` + `KeyH2` exception;
   - vanilla big keys;
   - vanilla maps, with the `Secret Passage`/`Link's Uncle` + `MapH2` exception;
   - vanilla compasses;
   - generic keys, last, on the result.
   The region whose list is read is the location's region in the variant's layout (`rando_layout_location_region`), as `$this->region` is in VT.
3. **Bytes.** An alias is replaced by its target. Then one `writeItem` write of one byte per (slot, address) pair whose slot the item has.
4. **Prize music.** For a prize-class location (pendant, crystal, event) whose region has music addresses:
   - with `rom_map_on_pickup`, the byte is the entry of `post.prize_music` for that location;
   - otherwise it is `0x11` for a pendant-class item and `0x16` for any other.
   It is written at each music address, in order, after the location's own bytes. Under map-on-pickup, a prize location with no entry in `post.prize_music` is a broken precondition: assert.
5. **Hera basement.** After the bytes of a `STANDING_HERA_BASEMENT`-class location: `0x4E3BB` = `0xE4` when its world item (before substitution) is `Key` or `KeyP3`, otherwise `0xEB`.
- Every write uses the tag `writeItem`. A refused write ends the group and the whole `rando_rom_write` (the emit helper's rule, H-5-1).

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all items` (H-5-3). This is the step's proof; nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle.
- A case that stays `DIVERGENT` at `all items` is a bug in this step, driven to green by `check-e2e`'s loop (its write-log ranking from step 5) before the step ends. A divergence that comes from the C world (a location or region order the layout gets wrong) is fixed in the layout, with its own unit test, not worked around in the writer.

### Not done here

- No `measure` run: step 6's text sets no budget. The 315 single-byte callbacks per case are negligible against the generation.
- `e2e-arm` is not run here: step 12 and the end of Phase 2 run it again (step 1.1). If it runs anyway, the result goes into the note.

### Step note

The plan's §6 gains a "Step 1.6" note:
- which H-5 hypotheses held, and how any that did not was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all items`);
- any VT quirk the goldens settled, as earlier notes do;
- for steps 8 and 9: the `writeItem` overrides that only buffer (`Uncle`, `Witch`, `Zora`, `BugCatchingKid`, `HauntedGrove` and `Pedestal` credits; `Pedestal`, `Ether` and `Bombos` texts), called from the items walk in its order;
- that VT's walk leaves Nothing in empty locations while the C world keeps them empty.

### No `guideline` agent

None was consulted: every point was settled from the plan, VT at `219fcafd` (`tmp/vt`), the code and the goldens at `c347c7d`, step 5's PRD and steps 1.1 to 1.4's notes.

## Implementation Chunks

1. **Tables and the plain walk.**
   - The items source, its item-byte, location-address and music tables.
   - The walk (regions, non-empty then empty as Nothing), one-byte locations, aliases, events writing nothing, prize bytes with their unmapped music, medallions and Blacksmith.
   - The emit-helper move if H-5-8 needs it.
   - `test_rom_items` registered in `UNIT_TESTS` with those cases, plus the refusal-rule test, and `test_rom`'s skeleton expectation rewritten.
   - `make check` green (no claim moves yet).
2. **The rules that depend on settings and post-fill.**
   - The vanilla and generic substitutions with their exceptions, map-on-pickup music from `post.prize_music`, the Hera basement byte and the alternate bow, each with its `test_rom_items` case.
   - `make check` green.
3. **Claims and the note.**
   - Every claim of both files moves to `all items`; `e2e` and `e2e-combos` are driven to green, each divergence fixed with its own unit test first.
   - The §6 "Step 1.6" note.

## Testing Decisions

- **What a good test is here.** It builds a `RandoGen` by hand (`rando_config_init`, `rando_world_init`, `rando_postfill_clear`, then the location items and flags the case needs), calls `rando_rom_write` with a recording writer, and checks the recorded writes: address, bytes, count, tag and order. It never inspects a table or a static function. It never reads a golden log: the goldens are the e2e net's job, and a golden line can exceed `FIXTURE_LINE_MAX`. Prior art: `test_postfill` for hand-built Open worlds (`pool_world`, `hints_world`); `test_rom` (step 5) for the recording writer.
- **`test_rom_items`** (new, in `UNIT_TESTS`; not in `LOGIC_TESTS`: it has no fixture). Expected bytes and addresses are read from VT's source at `219fcafd`, never from the C tables. Cases:
  - a chest location's item lands at its address with tag `writeItem`, one byte;
  - region order and list order: two locations of different regions come out in `RandoRegionId` order; within a region, an empty location is written as `Nothing` (`0x5A`) after every non-empty one, even when it comes earlier in the list;
  - an alias (`UncleSword` at `Link's Uncle`) writes its target's byte (`0x5E`);
  - an event location (`Agahnim`) writes nothing; `Agahnim 2`'s event writes `0x16` at `0x155C9`;
  - a pendant prize: six writes at keys 1 to 6 in order, then `0x11` at each of Desert's four music addresses; a crystal prize: its six bytes, then `0x16`;
  - map-on-pickup: the music byte is the `post.prize_music` entry, not the prize's, for a pendant given `0x16`;
  - each medallion location: four writes in VT's order (`0x180023`, `t0`, `t1`, `t2`; `0x180022`, `m0`, `m1`, `m2`) for one medallion;
  - Hera basement: its own byte, then `0x4E3BB` = `0xE4` with `KeyP3` and `0xEB` with another item; under vanilla keys the `KeyP3` is written as `Key` and `0x4E3BB` is still `0xE4`;
  - Blacksmith at `0x18002A` by default and at `0x3355C` with `region_swords_in_pool` off;
  - vanilla keys: a region key is written as `Key` (`0x24`); a key of another region is not; `KeyH2` at `Secret Passage` and at `Link's Uncle` stays `KeyH2`, and at another escape location becomes `Key`; the same three cases for maps and `MapH2`; big keys and compasses with no exception;
  - generic keys: a key becomes `KeyGK` (`0xAF`), also when vanilla keys turned it into `Key` first; a big key is untouched;
  - the alternate bow: the `post.texts.alternate_bow` location writes `0x65`, another ProgressiveBow location `0x64`;
  - **the refusal rule** (handed over by step 5): a writer refusing the third write gets exactly three calls, and `rando_rom_write` returns false;
  - `rando_rom_write` leaves the generator's bytes unchanged (compared with a copy taken before), with an empty location in the world.
- **`test_rom`** (step 5): its "no callback call on baseline" expectation is rewritten on purpose, because the items group now writes. The new expectation: on a completed baseline, every call is tagged `writeItem` and lies inside 2 MiB, and the buffer writer accepts the whole run into a 2 MiB buffer. Step 7 rewrites it again when settings write.
- **`e2e` and `e2e-combos`**: the step's proof, all 73 claims at `all items`, green.
- **`lint`** builds the new source with `-std=c99 -pedantic-errors`.
- `make test` stays fast: one new binary of hand-built worlds, with no generation.

## Out of Scope

- Every later write group: settings and flags (step 7, including shops, prize packs, the RNG block and the digging game, which VT writes after the item loop), credits (step 8), text, dialog and hints (step 9, including the `Pedestal`, `Ether` and `Bombos` texts the items walk buffers), initial SRAM and `setTotalItemCount` (step 10), checksum and heart beep (step 11).
- The `setCredit`/`setText` calls of the `writeItem` overrides: they reach the ROM only through `writeCredits` and `writeText`, so they belong to steps 8 and 9; this step makes no buffer.
- `multiworld` and `override_patch` (entrance shuffle): out of scope in plan §2.
- Any change to the goldens, the oracle, `stages.txt`, `write_groups.txt` or the claim semantics.
- The writes outside the log (step 2's note) and the Phase 2 pipeline.
- Running `check-e2e` over the whole case list (step 12) and `e2e-arm` (step 12, Phase 2).

## Further Notes

- **Why the tables live in the ROM writer, not in `RandoLocationInfo`.** The generator never needs a ROM address. Keeping the addresses and bytes beside the only code that reads them leaves the world tables, and every test that builds a world, unchanged.
- **Why slots, not VT's keys.** Thirteen fixed slots cover every key VT uses (`0` to `6`, `t0` to `m2`) with no strings and no allocation. Ordered (slot, address) lists keep VT's insertion order, which is the write order.
- **Why read the alternate bow and the music from `post`.** VT changes the world (`setItem` of a new bow) and draws inside `writeItem`. Step 4 moved both decisions to the post-fill result, so the ROM writer stays a const reader that cannot change the seed.
- **The `KeyH2`/`MapH2` exception** compares items loosely in PHP (`!=` on objects of one class compares their properties). Only the item that is `KeyH2` (or `MapH2`) itself matches; in C that is an id comparison.
- **Which region a location belongs to.** Inverted moves `Ganon` into North East Light World's list while North East Dark World keeps it as its prize (step 1.4). The walk follows the region lists, not the prize pointers, as VT's `getLocations()` does; the goldens of `state-inverted` and its combos confirm it in chunk 3.
