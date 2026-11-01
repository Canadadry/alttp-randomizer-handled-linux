---
title: "Port the settings and flags writes"
description: "The C ROM writer writes none of the Rom setters writeToRom calls after the item loop, so the settings group of every golden rom_writes.txt (90 to 296 writes per case) is unproven and no case can claim past its items."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 7, copied from `docs/plan-1.md`)

> **Port the settings and flags writes** (the `Rom` setters `writeToRom` calls). Verified by every case's claim reaching that group, plus `test_rom_settings.c`.

## Facts at `34c6524` (checked)

- **The target.** In `tests/rando/write_groups.txt`, `settings` is the `*` group: it owns every tag not owned by `items` (`writeItem`), `credits`, `text`, `sram`, `setTotalItemCount` or `heart_beep`. In all 73 goldens (50 `cases/`, 23 `combos/`), those lines sit between the last `writeItem` line and the first `writeCredits` line. That is 90 (`goal-pedestal`) to 296 (inverted combinations) writes per case, 100 for `baseline`.
- **The tag** of a write is the first non-closure frame below `writeToRom` (step 1.2 note). So a nested setter's writes carry the outer call's name:
  - `setGameState` covers `setFixFakeWorld`, `setSewersLampCone` and the whole of `setInvertedMode`;
  - `setSwordlessMode` covers `setHammerTablet` and `setHammerBarrier`;
  - `setupCustomShops` covers each shop's `writeExtraData`;
  - `writePrizePacksToRom` covers `setOverworldDigPrizes`, `setPullTreePrizes`, `setRupeeCrabPrizes`, `setStunnedSpritePrize` and `setFishSavePrize`;
  - `setEscapeFills` (the `World` method) covers `Rom::setEscapeFills` and the three `set*SpawnRefills`;
  - the bare `$rom->write(0x45C42, …)` in `writeToRom` is tagged `writeToRom`.
- **VT's call order** (`tmp/vt`, `app/World.php` `writeToRom` from the line after the item loop to the line before `writeCredits`; `app/Rom.php` setters). Addresses are PC, multi-byte integers little-endian, and "cfg" marks a value read from the world config:

| # | Call (tag) | Writes |
|---|---|---|
| 1 | `setEscapeFills` (World, `mode.state` standard only) | See "Escape fills" below. Zero or five writes. |
| 2 | `setGoalRequiredCount` | `180167` u16 = `item.Goal.Required` (null → 0) |
| 3 | `setGoalIcon('triforce')` | `180165` = `0E 28` |
| 4 | `setCaneOfByrnaSpikeCaveUsage` | `18016B` = `04 02 01` |
| 5 | `setCapeSpikeCaveUsage` | `18016E` = `04 08 10` |
| 6 | `setByrnaCaveSpikeDamage(0x08)` | `180195` = `08` |
| 7 | (`writeToRom`) | `045C42` = `04 02 01` |
| 8 | `setCapeRegularMagicUsage` | `03ADA7` = cfg normal, half, quarter (`04 08 10`; hard/expert functionality `02 04 08`) |
| 9 | `setCaneOfByrnaInvulnerability` | `18004F` = `01` (hard/expert `00`) |
| 10 | `setPowderedSpriteFairyPrize` | `036DD0` = `E3` (hard/expert `D8`) |
| 11 | `setBottleFills` | `180084` = health, magic (`A0 80`; hard `38 40`; expert `20 20`) |
| 12 | `setCatchableFairies` | `034FD6` = `F0` on, `80` off (off for hard/expert) |
| 13 | `setCatchableBees` | `0F5D73`, then `0F5F10`, each `F0` (true on every reachable path) |
| 14 | `setStunItems` | `180180` = `03` (hard `02`, expert `00`) |
| 15 | `setSilversOnlyAtGanon` | `180181` = `00` (hard/expert `01`) |
| 16 | `setRupoorValue(0)` | `180036` = `00 00` |
| 17 | `setGanonAgahnimRng('table')` | `180086` = `00` |
| 18 | `setTowerCrystalRequirement` | `18019A` = `crystals.tower` clamped to 0..7 |
| 19 | `setGanonCrystalRequirement` | `1801A6` = `crystals.ganon` clamped to 0..7 |
| 20 | `setGenericKeys` | `180172` = `rom.genericKeys` |
| 21 | `setupCustomShops` | See "Shops" below. |
| 22 | `setRupeeArrow(rom.rupeeBow)` | eight writes: `030052`, `0301FC`, `0ECB4E` (4 bytes), `0F0D96` (4), `180175`, `180176` (u16), `180178` (u16), `00EDA5` (2) |
| 23 | `setWishingWellChests(true)` | `00E9AE` = `14 01`, `00E9CF` = `14 01`, `01F714` = the enabled base64 room (205 bytes) |
| 24 | `setWishingWellUpgrade(false)` | `0348DB` = `2A`, `0348EB` = `05` |
| 25 | `setHyliaFairyShop(true)` | `01F810` = `1A 1E 01 1A 1E 01` |
| 26 | `setRestrictFairyPonds(true)` | `18017E` = `01` |
| 27 | `setLimitProgressiveSword`, `…Shield`, `…Armor`, `setLimitBottle`, `setLimitProgressiveBow` | `180090`, `180092`, `180094`, `180096`, `180098`: each the cfg overflow count, then the first byte of the cfg replacement item (`TwentyRupees2` = `47`) |
| 28 | `setSilversEquip('collection')` | `180182` = `01` |
| 29 | `setSubstitutions` | `184000`, 28 bytes: `12 01 35 FF 51 06 52 FF 53 06 54 FF 58 01 <rupeeBow ? 36 : 43> FF 3E <BossHeartContainer count> <its replacement's byte> FF 17 <PieceOfHeart count> <its replacement's byte> FF FF FF FF FF` |
| 30 | goal switch | triforce-hunt: `enableTriforceTurnIn` `180194` = `01`, then `setGanonInvincible` `1801A8` = `01`; pedestal `01`; dungeons `02`; ganonhunt `05`; fast_ganon `04`; completionist `0B`; ganon (default) `04` |
| 31 | `rom.mapOnPickup` only | `setMapRevealSahasrahla` `18017A` u16 = the map reveal of the region holding `PendantOfCourage`; `setMapRevealBombShop` `18017C` u16 = the reveal of `Crystal5`'s region OR `Crystal6`'s region |
| 32 | `setMapMode` | `18003B` = `rom.mapOnPickup` |
| 33 | `setCompassMode(rom.dungeonCount)` | `18003C` = `01` for `pickup` (mc, mcs, full), `00` for `off` |
| 34 | `setCompassCountTotals()` | `187000` = the 16 default bytes `08 08 06 06 02 0A 0E 08 08 08 06 08 0C 1B 00 00` |
| 35 | `setFreeItemTextMode` | `18016A` = cfg `rom.freeItemText` |
| 36 | `setFreeItemMenu` | `180045` = cfg `rom.freeItemMenu` |
| 37 | `setDiggingGameRng` | `180020`, then `0EFD95`, each the drawn count |
| 38 | `writeRNGBlock` | `178000`, the 1024 drawn bytes |
| 39 | `writePrizePacksToRom` | See "Prize packs" below. |
| 40 | `setPyramidFairyChests(region.swordsInPool)` | `01FC16` = `B1 C6 F9 C9 C6 F9` (off: `A8 B8 3D D0 B8 3D`) |
| 41 | `setSmithyQuickItemGive(region.swordsInPool)` | `180029` = `01` (off `00`) |
| 42 | `setGameState(mode.state)` | `180174` = `00` (`setFixFakeWorld(false)`), then: open and retro `180038` = `00`; standard `180038` = `01`; inverted `180038` = `00`, then `setInvertedMode`'s 192 writes in source order, with `setFixFakeWorld(true)` (`180174` = `01`) at its place among them (195 lines in all) |
| 43 | `setSwordlessMode(weapons swordless)` | `18003F`, `180041`, `180044` = `01` when swordless, else `00`; then `18005D` = `00` |
| 44 | `setLockAgahnimDoorInEscape` (not inverted) | `180169` = `01` for NoGlitches, `00` for OverworldGlitches, HybridMajorGlitches, MajorGlitches and NoLogic |
| 45 | `removeUnclesSword` (unless Link's Uncle holds an `Item\Sword`) | ten 6-byte writes from `06D263` to `06D323` |
| 46 | `removeUnclesShield` | seven 6-byte writes from `06D253` to `06D313`. VT's condition (`!instanceof Shield \|\| !hasItem(L1SwordAndShield)`) is true for every item, so it always writes (511 = 73 × 7 lines). |
| 47 | `setBallNChainDungeon(0x02)` | `186FFF` = `02` |
| 48 | `setCapacityUpgradeFills` | `180080` = `32 32 46 46` |
| 49 | `setClockMode('off')` | `180190` = `00 00 00`, with no compass override |
| 50 | `setBlueClock`, `setRedClock`, `setGreenClock` (0) | `180204`, `180200`, `180208`, each 4 × `00` |
| 51 | logic switch | HybridMajorGlitches, MajorGlitches, NoLogic: `1800A1` `00`, `1800A2` `00`, `180042` `01`, `1800A3` `00`, `180210` `01`, `180212` `60`, `180358` `01`, `1800A4` `00`, `0159A8` `02`. OverworldGlitches: `1800A2` `00`, `180042` `01`, `1800A3` `00`, `180210` `02`, `180212` `40`, `180358` `01`, `1800A4` `00`, `0159A8` `02`. NoGlitches: `180042` `01`, `1800A3` `01`, `180358` `00`, `1800A4` `01`, `0159A8` `04`. |
| 52 | `enableHudItemCounter` | `180039` = `00` when the goal is triforce-hunt or ganonhunt or `item.Goal.Required > 0`; otherwise `01` for completionist, `00` else |
| 53 | `setGameType('item')` | `180211` = `04` |
| 54 | `setMysteryMasking(false)` | no write (`spoilers` is `on`) |
| 55 | `setPseudoBoots(false)` | `18008E` = `00` |
| 56 | `enableFastRom(true)` | `187032` = `01` |

- **Escape fills** (`World::setEscapeFills`). `uncle_items` holds Link's Uncle's item. If it cannot `canKillEscapeThings` (read with `ignoreCanKillEscapeThings` forced false), the pre-collected items are merged in. Then the first branch that holds wins:
  1. `hasSword()` or `Hammer`: `18004E` `00`, `180183` `2C 01` (300), `180185` `00 00 00`, `180188` `00 00 00`, `18018B` `00 00 00`.
  2. `FireRod`, `CaneOfSomaria`, or `CaneOfByrna` with `enemizer.enemyHealth` default: flags `04`, then `80 00 00`, `20 00 00`, `20 00 00`.
  3. `canShootArrows` (level 1): flags `01`, then `00 00 46`, `00 00 0A`, `00 00 0A`.
  4. `TenBombs`, or logic not NoLogic: flags `02`, then `00 32 00`, `00 03 00`, `00 03 00`.
  5. Otherwise: no write. `glitches-no_logic+state-standard` shows it.

  `rom.EscapeAssist` is off, so no branch writes `18004D`. The goldens reach branch 1 (2 combinations) and branch 2 (5); branches 3, 4 and 5 are proven only by the unit test.
- **Shops** (`Rom::setupCustomShops` over `World::getShops()`):
  - It keeps the active shops, in getShops order. Each one gets an id from 0 up; the last gets `0xFF` instead.
  - For each shop, first its `writeExtraData`: the one-byte writes a `TakeAny` carries, at `0xDBBAE`–`0xDBBF3`; the other shops carry none.
  - Then 8 shop bytes: the id, the room id (u16), the door id, `00`, `(config & 0xFC) + inventory size`, the shopkeeper and the SRAM offset. The offset grows by 1 for a `TakeAny` and by the inventory size otherwise, and may not exceed 36.
  - Then, per inventory record in insertion order, 8 item bytes: the id, the first byte of the item (an alias gives its target's, `ShopArrow` → `Arrow`, `ShopKey` → `KeyGK`), the price (u16), the max, the replacement id (`FF`), and the replacement price (u16 `0`).
  - Then one write of all shop bytes at `184800`, and one write of all item bytes plus 8 × `FF` at `184900`.
  - Facts the C world lacks:
    - every shop's config byte, default shopkeeper, room id, door id and TakeAny extra write (VT's region constructors; Inverted reuses Standard's shops);
    - `max` is 7 for every record of `Capacity Upgrade` and 0 for every other record (every `addInventory` into it passes 7, and no other call passes a max);
    - the shopkeeper is `0xE2` (old man) for every active `TakeAny`, because the only path that activates one, retro's `takeAnys`, also calls `setShopkeeper('old_man')`;
    - no record has a replacement;
    - a cleared `Capacity Upgrade` (hard and expert pools) stays active with an empty inventory.
  - Insertion order is ascending slot order on every path (each `addInventory` either fills a fresh shop from slot 0 up or overwrites an existing slot in place).
  - The goldens cover open (80 + 240 bytes), retro (5 extra writes, 120 + 304 bytes), inverted (BluePotion 160) and the hard, expert and crowd_control pools.
- **Prize packs** (`World::writePrizePacksToRom`):
  - The drops step 4 assigned are read in `getAllDrops()` order: packs 0 to 6, pull, crab, stun, fish; 63 bytes in all. Each byte is the drop's sprite byte, `0xD7 + drop id`.
  - Under `rom.NoFarieDrops` (hard and expert functionality), `E0` becomes `DF` and `E3` becomes `D8`. Under `rom.rupeeBow`, `E1` becomes `DA` and `E2` becomes `DB`, and `setOverworldDigPrizes` first writes its 64 fixed bytes at `180100`.
  - Then: `037A78` gets bytes 0–55, `0EFBD4` gets 56–58, `0329C8` gets 59, `0329C4` gets 60, `037993` gets 61, `0E82CC` gets 62.
  - VT does this with `str_replace` on an int array. That turns every value into a decimal string, but all drop bytes are three digits, so the replacements are whole-value.
- **Map reveal** (`Region::$map_reveal`): Eastern `0x2000`, Desert `0x1000`, Hera `0x0020`, Palace of Darkness `0x0200`, Swamp `0x0400`, Skull Woods `0x0080`, Thieves' Town `0x0010`, Ice `0x0040`, Misery Mire `0x0100`, Turtle Rock `0x0008`; every other region `0x0000`. `getLocationsWithItem(...)->first()` walks `World::getLocations()` order. The goldens cover it with the three `dungeon_items` cases and their combinations (e.g. `dungeon_items-mc`: `0004`, `4020`).
- **Config keys the group reads that `RandoConfig` lacks.** These are the keys some reachable World constructor branch sets; `derive_config`'s TODO lists most of them:
  - `rom.CapeMagicUsage.{Normal,Half,Quarter}`, `rom.CaneOfByrnaInvulnerability`, `rom.PowderedSpriteFairyPrize`, `rom.BottleFill.Health`, `rom.StunItems`, `rom.SilversOnlyAtGanon` and `rom.NoFarieDrops` (item.functionality hard and expert);
  - `item.overflow.count.{Sword,Armor,BossHeartContainer,PieceOfHeart}` (item.pool hard, expert, crowd_control): Sword 3/2/4, Armor 0/0/1, BossHeartContainer 6/2/1, PieceOfHeart 16/8/20, against the defaults 4, 2, 10, 24;
  - `rom.freeItemText` and `rom.freeItemMenu`: dungeonItems full `|= 0x18`/`0x02`, mcs `|= 0x11`/`0x01`, mc `|= 0x16`/`0x0C`, cumulative from full down, plus `0x10` on the menu under glitched logic;
  - `rom.dungeonCount` (`pickup` for mc, mcs, full).

  Every other key the group reads has one value on every alttpr.com path, its `config()` default: `rom.CatchableBees`, `item.overflow.count.Bottle`, the `item.overflow.replacement.*` other than Shield, `item.Goal.Icon`, `item.value.*`, `rom.GanonAgRNG`, `rom.timerMode`, `rom.hudItemCounter`, `rom.EscapeRefills.*`, `rom.EscapeAssist`, `rom.logicMode`, `spoilers`, `pseudoboots` and `fastrom`.
- **The C side** at `34c6524`:
  - `RandoGen.post` holds `digging_game` and `rng_block` (a drawn 256 stored as 0, as VT's `pack('C')`).
  - `RandoWorld` holds the prize-pack drops (assigned on the winning attempt), the shops (`shop_items`, `shop_prices`, `shop_active`; `RandoShopInfo.kind` tells Shop, TakeAny and Upgrade apart), the location items and the pre-collected items.
  - `rando_items_has_sword`, `rando_items_can_kill_escape_things` and `rando_items_can_shoot_arrows` port the `ItemCollection` predicates the escape fills read.
  - `RandoItemInfo.item_class` gives the item's own class: `UncleSword` is an `ALIAS`, not a `SWORD`.
  - No ROM address exists in `rando/` outside step 6's items source (H-6-3).
- **Side effects that are not writes.** Some setters of this group only fill buffers: `setOpenMode`/`setStandardMode`'s initial SRAM calls, the goal switch's `preOpenPyramid`, `setSwordlessMode`'s `setSwordlessCurtains`, `setStartingEquipment`, `setStartingTimer`, `preOpenGanonsTower`, and `setMysteryMasking`'s text when on. They reach the ROM only through `writeInitialSram` and `writeText`, so they belong to steps 9 and 10.

## Step 6's outcome (hypotheses)

Step 6 (`docs/prd/triage/phase1-step6-items-locations-writes.md`) is not done. Every decision below that depends on what it leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it holds. A hypothesis that does not hold is handled by the rule given with it.

- **H-6-1**: step 6 is finished. The items group writes every `writeItem` line, every line of `claims.txt` (50) and `combo_claims.txt` (23) is `<case> all items`, and `make check` is green. *If a case is still below `all items`*, this step cannot claim `all settings` for it: the step stops and reports it, rather than claiming around it, since the step text requires every case.
- **H-6-2**: the emit helper (and its output state) is shared through a private header under `rando/src/`, and the items group lives in its own source file, called from `rando_rom_write`'s items slot. *If emit is still static in the `rando_rom` source*, this step moves it behind such a header, as step 6 planned. Either way the settings group lives in its own source.
- **H-6-3**: step 6's item-byte table (one entry per `RandoItemId`, aliases resolved to their target) is private to the items source. *If step 6 exposes an item's bytes through the private header*, the settings group uses that. *Otherwise* this step adds one private accessor to that header, "the first byte of an item id, an alias read as its target", served by the same table. There is never a second item-byte table.
- **H-6-4**: the items group leaves the world untouched: an empty location stays empty, where VT has set it to `Nothing`. The settings group therefore reads an empty Link's Uncle as `Nothing`. That means no Sword, so `removeUnclesSword` writes, and the escape fills' `uncle_items` holds `Nothing`.
- **H-6-5**: `test_rom`'s baseline expectation reads "every call is tagged `writeItem`, inside 2 MiB, and the buffer writer accepts the whole run". This step rewrites it on purpose (step 6 says so).
- **H-6-6**: `test_rom_items` has its own recording writer (address, bytes, count and tag per call), local to that file, and `support.h` has none. *If step 6 moved a recorder into `support.*`*, `test_rom_settings` reuses it. *Otherwise* it has its own copy.
- **H-6-7**: step 6 added no `RandoConfig` field and left `derive_config`'s TODOs as they are. *If it added any field this step needs*, the field is kept and not duplicated.
- **H-6-8**: `write_groups.txt`, `run_e2e.sh`'s claim semantics, the goldens and the oracle are unchanged by step 6.

## Problem Statement

The owner will play ROMs the console patches. Everything that makes a seed's ROM behave like its settings goes through this group:

- the goal and Ganon's vulnerability, and the crystal requirements;
- the item functionality (cape, byrna, bottles, fairies, stun, silvers), and the progressive item limits and substitutions;
- the shops, the enemy drops, the digging game and the RNG table;
- the game state (standard escape, open, inverted world), swordless mode, the glitch-logic switches;
- the map and compass modes of the dungeon-item settings.

Today the C ROM writer writes none of it. A patched ROM would run the base patch's defaults whatever the user chose, and an inverted seed would start in the Light World. The net cannot catch it either: every claim stops at `all items`.

## Solution

- The `rando_rom` settings group writes, through the caller's writer, exactly the writes `writeToRom` makes between the item loop and `writeCredits`. It follows VT's order and VT's tags, for every alttpr.com setting the app offers.
- It reads the generator as const and draws nothing. The digging count and the RNG block come from the post-fill result, the drops and shops from the world, everything else from the world config.
- The few config keys alttpr.com settings change and the C config does not yet hold are derived where the World constructor derives them.
- `test_rom_settings` proves each rule on hand-built generators through a recording writer. That includes the branches no golden reaches (three of the five escape-fill outcomes, `region.swordsInPool` off, the crystal clamp).
- Every claim in `claims.txt` and `combo_claims.txt` becomes `<case> all settings`. `e2e` and `e2e-combos` then compare all 73 logs through the last settings write, byte for byte against VT.

## User Stories

1. As the owner, I want Ganon's vulnerability written for my goal (ganon, fast ganon, dungeons, pedestal, triforce hunt, ganonhunt, completionist), so that the game ends the way the goal says.
2. As the owner of a triforce-hunt or ganonhunt seed, I want the required piece count, the turn-in and the HUD written, so that Murahdahla and the counter work.
3. As the owner of a completionist seed, I want the item counter on the HUD, so that I can track my collection rate.
4. As the owner, I want the Ganon's Tower and Ganon crystal requirements written, random draws included, so that the doors open at the counts the spoiler shows.
5. As the owner of a hard or expert item-functionality seed, I want the cape, byrna, bottle, fairy, stun and silver-arrow behaviour of that setting, so that the difficulty is real.
6. As the owner of a hard or expert functionality seed, I want fairies and full magic refills removed from enemy drops, so that the drops match VT.
7. As the owner of a hard, expert or crowd-control item-pool seed, I want the sword, shield, armor, bottle and bow limits and the heart substitutions of that pool, so that surplus items turn into rupees as on alttpr.com.
8. As the owner, I want every active shop written with its inventory, prices and SRAM slot, so that shops sell what the seed decided.
9. As the owner of a retro seed, I want the take-any caves, the rupee bow, generic keys, shop arrows and keys, and the dig prizes written, so that retro plays as retro.
10. As the owner of an inverted seed, I want the inverted world flags written, so that the game starts in the Dark World with the inverted map.
11. As the owner of a standard seed, I want the escape fills chosen from my uncle's weapon, so that I can clear the escape with what I was given.
12. As the owner of a standard seed whose uncle gives no usable weapon under no-logic, I want no escape fill written, as VT does, so that the ROM matches VT exactly.
13. As the owner of an open or retro seed, I want the sewers lamp cone off and the fake-world fix off, so that the state matches VT's open mode.
14. As the owner of a swordless seed, I want the hammer Ganon, swordless medallions and hammer tablet flags, so that the seed is beatable without a sword.
15. As the owner of a glitched seed, I want the swamp water, death, save-and-quit, seed type, warning, accidental-glitch, SQEG and Zelda mirror flags of my logic, so that the glitches the logic expects work.
16. As the owner of a no-glitch seed, I want Agahnim's door locked during the escape and the SQEG and mirror fixes on, so that I cannot break the seed by accident.
17. As the owner of a map/compass/full dungeon-items seed, I want map mode, compass counting, the free-item text and menu, and the map reveals of Sahasrahla and the bomb shop written, so that the map shows prizes only when VT shows them.
18. As the owner, I want the enemy prize packs, pull trees, crab, stun and fish prizes written as drawn, so that drops match the seed.
19. As the owner, I want the digging game's prize count and the 1024-byte RNG table written as drawn, so that the game's randomness matches VT's seed.
20. As the owner, I want Link's Uncle's sword and shield removed unless he really gives a sword, so that I start with what the spoiler says.
21. As the owner, I want every fixed flag of `writeToRom` (wishing well, Hylia fairy shop, fairy ponds, silver equip, ball-and-chain, capacity fills, clocks, game type, pseudoboots, fast ROM) written with VT's values, so that nothing differs from an alttpr.com ROM.
22. As the app (Phase 2), I want the settings writes through my buffer writer to land inside the 2 MiB ROM, so that patching never corrupts memory.
23. As the app, I want the writer to stop at the first refused settings write and return false, so that a failed patch is reported and not half-applied.
24. As a porting agent, I want the settings group to read the generator only as const and never touch the PRNG, so that writing a ROM can never change the seed.
25. As a porting agent, I want the shop, map-reveal and fixed-blob ROM tables kept inside the ROM writer, so that the generator's world tables stay free of ROM knowledge.
26. As a porting agent, I want the settings group in its own source, with its large fixed data apart from its logic, so that steps 8 to 11 never touch it and the logic stays readable.
27. As a porting agent, I want the World constructor's functionality, overflow, free-item and dungeon-count keys derived in the C config, so that the writer reads config the way VT does instead of re-deriving settings.
28. As a porting agent (step 10), I want the step note to list the SRAM and text buffer calls this group's setters make, so that step 10 knows where they come from.
29. As the net, I want every case and combination to claim `all settings`, so that `make check` fails on any wrong settings byte, address, tag or order.
30. As the net, I want the trace, attempts, spoiler and items writes still compared before the settings writes, so that a settings divergence is never reported on a seed that already differs.
31. As the net, I want a unit test per rule and per branch, including the ones no golden reaches, so that the dev loop catches a wrong rule in milliseconds without a golden.
32. As the net, I want the refusal rule tested on a settings write, so that the contract holds across group boundaries.
33. As the net, I want `test_rom`'s baseline expectation rewritten on purpose, with the reason stated, so that no check is weakened silently.
34. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new tables obey the library rules.
35. As the net, I want `make test` to stay fast and every golden and fixture untouched, so that the dev loop stays seconds long and the source of truth does not move.
36. As `check-e2e`, I want a settings divergence reported with the first differing write, so that `fix-bug` starts from the exact setter.
37. As a reviewer, I want the step note to record which H-6 hypotheses held, the claims moved and any VT quirk the goldens settled, so that I can check the step without rerunning it.

## Implementation Decisions

### Where the settings group lives

- A new private source in `rando/src/` holds the settings group. `rando_rom_write`'s settings slot calls it, after the items group, and it writes through the shared emit helper (H-6-2), so a refused write ends the whole `rando_rom_write`.
- A second private source holds the group's large fixed data, behind a private header: `setInvertedMode`'s write list, the enabled wishing-well room, the `removeUncles*` rows and retro's dig prizes. The logic source keeps only small constants.
- No public API changes. `rando_rom.h`'s contract already covers the group.
- It reads `gen->world` (config, variant, location items, pre-collected items, shops, drops) and `gen->post` (`digging_game`, `rng_block`), both const, and never `gen->prng`.

### Config (the World constructor's derived keys)

- `RandoConfig` gains the keys listed in the facts, with `rando_config_init` giving VT's `config()` defaults:
  - the eight item-functionality keys, including `rom_no_farie_drops`;
  - the four overflow counts (Sword, Armor, BossHeartContainer, PieceOfHeart);
  - `rom_free_item_text` and `rom_free_item_menu`;
  - `rom_dungeon_count_pickup`, a bool, since `pickup` and `off` are the only values any path sets.
- `derive_config` sets them as the World constructor does, in its order, for the hard and expert functionality, the hard, expert and crowd_control pools, the three dungeon-item levels and glitched logic. The functionality and overflow TODOs go away, rewritten to name only what stays unported (superexpert, not an alttpr.com value).
- Keys that have one value on every alttpr.com path stay out of the struct. The writer writes them as named constants equal to VT's default, and each one is listed in the step note. That covers `rom.CatchableBees`, the Bottle count, the replacements other than Shield, and the other keys listed in the facts. The Shield replacement uses the existing field.
- The generator reads none of the new fields, so traces, attempts and spoilers do not move.

### The group's walk

- One function writes the 56 rows of the facts table in order, each with its VT tag (a static string). It branches only where VT branches: state, goal, logic, `rom.mapOnPickup`, `rom.rupeeBow`, `rom.NoFarieDrops`, swordless, `region.swordsInPool`, and the uncle's item.
- **Escape fills**:
  - Written only when `mode_state` is standard.
  - `uncle_items` is a `RandoItems` holding Link's Uncle's item, empty read as `Nothing` (H-6-4).
  - The `canKillEscapeThings` test reads a copy of the world config with `ignore_can_kill_escape_things` false. When it fails, the world's pre-collected items are merged in.
  - The four branches use the existing item predicates, in VT's order, and `Hammer`, `FireRod`, `CaneOfSomaria`, `CaneOfByrna` and `TenBombs` are counted by id. An alias counts under its own id, as VT's collection counts it under its own name.
- **Shops**:
  - A private ROM table per `RandoShopId` holds the config byte, default shopkeeper, room id, door id and optional extra write, from VT's region constructors at `219fcafd`.
  - The walk counts the active shops first, so that it can give the last one `0xFF`.
  - The shopkeeper is `0xE2` for an active TakeAny, otherwise the table's.
  - The inventory is the shop's filled slots in ascending slot order, and the inventory size is their count.
  - `max` is 7 for the Upgrade-kind shop, 0 elsewhere.
  - The item byte comes from H-6-3's accessor.
  - The shop and item bytes are assembled in two fixed caller-stack buffers sized for 41 shops of 3 slots plus the terminator, then emitted as two writes. The SRAM offset is asserted at most 36.
- **Prize packs**:
  - Every pack slot is read with `rando_world_drop` in `RandoPrizePack` order and asserted non-`NONE`, then mapped to `0xD7 + id`.
  - The replacements apply as whole values: `NoFarieDrops` first, then `rupeeBow`.
  - The dig-prize write precedes the pack write under `rupeeBow`.
- **Map reveal**:
  - A private per-`RandoRegionId` reveal table (VT's ten values, 0 elsewhere).
  - The region holding an item is that of the first location, in `rando_layout_location` order, whose world item is that item, via `rando_layout_location_region`.
  - Asserted found under `rom.mapOnPickup`.
- **Game state**: the inverted list keeps VT's source order, its duplicate duck write included, with PC addresses (`snes_to_pc` applied when the table is made).
- **Uncle**:
  - `removeUnclesSword` is skipped only when Link's Uncle's item has class `SWORD`. `UncleSword` (an alias) and an empty location both write.
  - `removeUnclesShield` always writes. A comment states why: VT's condition holds for every item.
- **Crystal requirements**: clamped to 0..7 before the byte, as VT's `max(min(...))`.

### How the fixed data is made

- The fixed blobs come from VT's source at `219fcafd`, never from a golden: the inverted list, the wishing-well room (the base64 string decoded), the uncle rows, the dig prizes, the shop table and the substitution prefix.
- They may be extracted mechanically by a throwaway script in `tmp/`, for example decoding the base64 or converting `snes_to_pc` addresses. The step note records how they were made. The goldens then check them through `e2e`.
- No golden line is copied into `rando/` or into a test expectation.

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all settings` (H-6-1). Nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle.
- A case that is `DIVERGENT` at `all settings` is a bug in this step. It is driven to green by `check-e2e`'s loop before the step ends, each fix behind its own failing unit test first.
- A divergence that traces back to the C world (a shop's state, a drop, an uncle item) is fixed where that state is made, with its own unit test, not worked around in the writer.

### Not done here

- No `measure` run: step 7's text sets no budget.
- `e2e-arm` is not run (step 12 and the end of Phase 2 run it). If it runs anyway, the result goes into the note.

### Step note

The plan's §6 gains a "Step 1.7" note with:
- which H-6 hypotheses held, and how any that did not was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all settings`);
- the config keys added and the ones written as constants;
- how the fixed data was extracted;
- any VT quirk the goldens settled, as earlier notes do (expected: the always-true uncle-shield condition, `UncleSword` failing `instanceof Sword`, no escape fill under no-logic);
- for step 10, the SRAM and text buffer calls this group's setters make, in order (facts, "Side effects that are not writes"), and the remark that the C config keeps `crystals_tower` as an int, while VT's `crystals.tower === 0` is false for the form's string `'0'` (step 1.2) but true for a random draw of 0.

### No `guideline` agent

None was consulted. Every point was settled from the plan, VT at `219fcafd` (`tmp/vt`), the code and the goldens at `34c6524`, step 6's PRD and steps 1.1 to 1.5's notes.

## Implementation Chunks

1. **The config keys.**
   - `RandoConfig`'s new fields, their `rando_config_init` defaults and `derive_config`'s derivation, each with a `test_world` case that builds a world from options and reads the derived config (functionality hard and expert, each pool, each dungeon-item level, glitched logic on the free-item menu).
   - `make check` green, with no trace or spoiler change.
2. **The settings source and the config-only rows.**
   - The settings source wired into the settings slot (and the emit move if H-6-2 needs it).
   - Every row whose value depends only on config and post: rows 2 to 20, 22 to 30, 32 to 38, 40, 41, 43, 44, 47 to 56, and row 42 for open, retro and standard.
   - `test_rom_settings` registered in `UNIT_TESTS` (not `LOGIC_TESTS`), with those rows' cases and the refusal test.
   - `test_rom`'s expectation rewritten.
   - `make check` green (no claim moves yet).
3. **The world-dependent rows.**
   - Shops (row 21, with the shop ROM table and the item-byte accessor of H-6-3), prize packs (39), escape fills (1), map reveal (31) and the uncle removals (45, 46), each with its `test_rom_settings` cases.
   - `make check` green.
4. **Inverted.**
   - The fixed-data source with `setInvertedMode`'s list, and row 42's inverted branch, with its test.
   - `make check` green.
5. **Claims and the note.**
   - Every claim of both files moves to `all settings`. `e2e` and `e2e-combos` are driven to green, each divergence fixed behind its own unit test.
   - The §6 "Step 1.7" note.

## Testing Decisions

- **What a good test is here.**
  - It builds a `RandoGen` by hand (`rando_config_init`, then the config fields the case needs, `rando_world_init` with the variant, `rando_postfill_clear`, then location items, pre-collected items, shops and drops through the public world API), calls `rando_rom_write` with a recording writer, and checks the recorded settings writes: address, bytes, count, tag and order.
  - The settings writes are the recorded calls whose tag belongs to no other group of `write_groups.txt` (the items tag and the five later groups' tags). Steps 8 to 11 adding writes therefore never breaks these tests.
  - It never inspects a table or a static function, and never reads a golden.
  - Expected bytes and addresses come from VT's source at `219fcafd`, never from the C tables.
  - Prior art: `test_rom_items` (step 6) and `test_rom` for the recording writer, and `test_postfill` for hand-built worlds.
- **`test_world`** gains the derived-config cases of chunk 1. They go through `rando_world_init` and read the public config fields.
- **`test_rom_settings`** (new, in `UNIT_TESTS`; not in `LOGIC_TESTS`, since it has no fixture). Cases:
  - **Order and tags**: on an open, NoGlitches, ganon, normal world, the settings writes come out exactly as rows 2 to 56 give them, the first `180167` `setGoalRequiredCount` and the last `187032` `enableFastRom`, with `045C42` tagged `writeToRom`;
  - **goal**: each goal's `1801A8` byte; triforce-hunt's `180194` before it; `180167` = `14 00` for triforce-hunt and `28 00` for ganonhunt; the HUD byte for completionist, triforce-hunt and ganon;
  - **crystals**: a value from 0 to 7 written as is, and an out-of-range config clamped (VT's `max(min())`);
  - **functionality**: hard's and expert's rows 8 to 15 bytes, read from a world built with that option;
  - **pool**: hard's limits and substitutions; `rom.rupeeBow`'s `36` in the substitutions;
  - **dungeon items**: map mode, compass mode and free-item bytes for mc and full; full plus major glitches gives menu `1F`;
  - **map reveal**: PendantOfCourage at Swamp's prize, Crystal5 at Eastern's and Crystal6 at Ice's gives `18017A` = `00 04` and `18017C` = `40 20`; nothing written without map-on-pickup;
  - **shops**:
    - open's default shops (each active shop's 8 bytes, the last id `FF`, the Capacity Upgrade `max` 7, the 8-byte terminator);
    - retro, with chosen take-anys: each TakeAny's extra write before `184800`, shopkeeper `E2`, SRAM offset +1;
    - a cleared Capacity Upgrade: active, inventory size 0;
    - `ShopArrow` written as `Arrow`'s byte and `ShopKey` as `KeyGK`'s;
  - **prize packs**:
    - the 63 drop bytes split across the six writes;
    - `NoFarieDrops` turning a Fairy (`E3`) into `D8` and a full magic (`E0`) into `DF`;
    - `rupeeBow` turning `E1`/`E2` into `DA`/`DB`, with `180100` written first;
  - **escape fills**: each of the five outcomes on a standard world:
    - a sword, then the Hammer, gives branch 1;
    - the FireRod gives branch 2, and so does CaneOfByrna at default enemy health;
    - a Bow with arrows gives branch 3;
    - TenBombs under NoLogic, and an empty-handed uncle under NoGlitches, give branch 4;
    - an empty-handed uncle under NoLogic writes nothing;
    - an uncle with no killing item gets a pre-collected sword, so branch 1 applies;
    - no escape write on an open world;
  - **uncle**:
    - `ProgressiveSword` at Link's Uncle: no `removeUnclesSword`;
    - `UncleSword`, `FiveRupees` and an empty uncle: its ten rows;
    - `removeUnclesShield`'s seven rows in every case;
  - **swords in pool off**: `01FC16` = `A8 B8 3D D0 B8 3D` and `180029` = `00`;
  - **game state**: standard's `180038` = `01`; inverted's first rows and its `180174` = `01` in place; no `180169` on inverted;
  - **logic**: each of the three logic branches' rows, and `180169` = `00` for overworld glitches;
  - **swordless**: `18003F`, `180041`, `180044` = `01`, `18005D` = `00`;
  - **post**: `180020` and `0EFD95` carry `post.digging_game`, and `178000` carries `post.rng_block` byte for byte;
  - **the refusal rule**: a writer refusing the first settings write gets no further call, and `rando_rom_write` returns false;
  - **const**: `rando_rom_write` leaves the generator's bytes unchanged.
- **`test_rom`**: its baseline expectation is rewritten on purpose, because the settings group now writes. The new expectation: on a completed baseline, every call lies inside 2 MiB, the calls are the items writes followed by settings writes ending with `enableFastRom`, and the buffer writer accepts the whole run into a 2 MiB buffer. Step 8 rewrites it again.
- **`e2e` and `e2e-combos`**: the step's proof is all 73 claims at `all settings`, green.
- **`lint`** builds the new sources with `-std=c99 -pedantic-errors`.
- `make test` stays fast: one new binary of hand-built worlds, plus a few `test_world` cases, with no generation beyond `test_rom`'s existing baseline.

## Out of Scope

- The later write groups: credits (step 8), text, dialog and hints (step 9), initial SRAM and `setTotalItemCount` (step 10), checksum and heart beep (step 11).
- The SRAM and text buffer calls this group's setters make (`setOpenMode`'s and `setStandardMode`'s SRAM, `preOpenPyramid`, `setSwordlessCurtains`, `setStartingEquipment`, `setStartingTimer`, `preOpenGanonsTower`, `setMysteryMasking`'s text): they reach the ROM in steps 9 and 10, and this step makes no buffer.
- The writes outside the log that step 1.2's note marks "reproduced in C, step 11" (quickswap, music volume, reduce flashing, menu speed, heart color, Link sprite, checksum), and those it leaves out (`setSeedString`, `setStartScreenHash`, tournament, Enemizer).
- Config values no alttpr.com setting reaches: timer modes, the star goal icon, `GanonAgRNG` none, pseudoboots, mystery masking, escape assist, superexpert, custom overflow replacements and `override_patch` (entrance shuffle). The writer handles only the values the app can produce, as named constants where there is one value.
- `multiworld`.
- Any change to the goldens, the oracle, `stages.txt`, `write_groups.txt` or the claim semantics.
- Running `check-e2e` over the whole case list and `e2e-arm` (step 12).

## Further Notes

- **Why derive config instead of reading options in the writer.** VT's setters read `config()`, and `derive_config` is already where the C port reproduces the World constructor. Putting the remaining derived keys there keeps one place that maps settings to config, and keeps the writer a plain translation of `writeToRom`.
- **Why the shop ROM data is a writer table, not `RandoShopInfo`.** The generator never needs a room id, door id or shopkeeper. Keeping them beside the only code that reads them leaves the world tables, and every test that builds a world, unchanged (as step 6 does for item bytes and addresses).
- **Why the shopkeeper and `max` are derived, not stored.** In VT, only the take-any activation sets a shopkeeper, and only `Capacity Upgrade` gets a `max`. Deriving them from the shop's kind and state keeps `RandoWorld` unchanged. A future path that breaks either rule would show as an `e2e` divergence on `setupCustomShops`.
- **Tags matter.** The e2e prefix compares the whole line, tag included. A nested setter's writes carry the outer call's name, so a C port that tags `setFixFakeWorld` or `setPullTreePrizes` by its own name diverges even with the right bytes.
- **Where a divergence is likely.**
  - The inverted list: 195 lines, one duplicate.
  - The shop bytes: order of active shops, the last-id `FF`, empty Capacity Upgrade.
  - The retro prize-pack replacements.
  - The escape-fill branches for the uncle's weapon.

  The goldens of `state-inverted`, `state-retro`, `item_pool-hard` and the standard combinations settle each of these in chunk 5.
