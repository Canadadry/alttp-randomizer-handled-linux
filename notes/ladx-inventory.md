# LADX inventory of `generateRom`

Phase 2 step 3's inventory of what LADXR reads from and writes into the ROM, drafted ROM-free in
Phase 1 step 9 from the sources alone and settled on the read trace of real runs. LADXR is pinned
at `4325207e4e55076e05f209fbdf16b860a90e42ef` with `tests/ladx/oracle/ladxr.patch` applied
(order fixes in `patches/endscreen.py` and `spoilerLog.py`, neither in these sections).

Check it with `python3 -I tests/ladx/oracle/inventory_check.py` (after
`tests/ladx/oracle/setup.sh`): it extracts every statement of the six sections below from the
patched checkout and diffs them, row by row, against these tables, then checks the Groups, Read
coverage, Extractions and Blind spots tables against the rows and against
`read_trace.py --blind-spots`; `--print` writes the extracted tables with the kept columns, to
refresh this file by hand.

Columns of the six section tables: `Call` is the stripped source line (` #<k>` on the k-th
occurrence of the same text in the function, the key `read_trace.py` tags a read with).
`Settings`, `Args`, `RNG (grep)` and `Reads (grep)` are what the checker greps from the line and
its enclosing conditions (`*`: the object is passed whole; `global`: the callee's own body uses
the module `random`, one level deep). `Reads (trace)` is, per target, the provenance and range
of the row's reads in the trace (offsets and counts only, never a byte value), written by
`read_check.py --print` from `tests/ladx/oracle/inventory_trace.sh`'s traces; `-` for a row with
no read. `Notes` holds what reading the callee showed. `Side` is `table` for a fixed or
settings-only write, `C` for a seed- or placement-dependent one, `none` for a statement that
writes no ROM byte, `unused` for a path the oracle never takes (an `args` debug path or an
unsupported setting of `tier1.txt`). A row is wholly one side: a `C` row that also writes fixed
bytes keeps them on the C side, and its Notes say so. `Group` names the row's group, listed under
Groups with the same side.

Settled on the real-data run (`inventory_trace.sh`: the 19 cases of `determinism_cases.txt`,
each under its own seed and the two of `side_seeds.txt`, 57 runs): `changeEntrances` is `C`
(group `entrances`), `addFrameCounter` is `table` (group `frame-counter`), and `noText` and
`warpHome` left `options` for their own `C` groups `notext` and `warp-home` (each reads bytes a
`C` group wrote); each row's Notes give the reason, and those of the `STABLE` rows kept `C` why.
Draft groups with rows of several sides were split: `goal-fixed`,
`music-fixed`, `music-ffa`, and `locals` for the local aliases; the spoiler's file-name and
output rows moved to `output`, and the row that builds the log is `C`, group `spoiler`.

The other tables: **Groups** (Side; Settings key, the settings its rows name, what a `table`
group's table is keyed on; Depends on, the groups whose writes it reads, from the trace's
provenance (written by `read_check.py --print`); Evidence, required for a `C` group: its write-log group,
its own name, then the ROM-free fixtures and `extract:<name>` extractions its Phase 3 claim
needs). **Read coverage**: how each read of a `C` group is covered (Row `*` for every row of the
group; Kind `copy`, `written`, `computed`, `table`, `extract:<name>` or `cut:<id>`).
**Extractions**: the fixtures step 7 produces under `tests/ladx/extract/` (the Nintendo-bytes
check's strict scope). **Blind spots**: every line `read_trace.py --blind-spots` lists, judged
either not a ROM read or a ROM read covered by a named row.

Decisions taken on the real data (under `guideline`):

- A read whose value decides, in a group that runs on every seed (`sprites`, `entrances`,
  `title`, `save`, `spoiler`, `warp-home`, the minibosses of `bosses`), is covered by an
  extraction, never cut. Extractions hold structure only, in decimal numbers of at most three
  digits (no hex-digit run reaches 4 bytes): ids, types, positions, offsets, lengths, counts, and
  fingerprints. Never graphics, text or code bytes, transformed or not. A value that needs those
  is a scope cut, if its group runs only under a tier-1 value that can be cut: `goal-bingo` in
  `notes/plan3-scope-cuts.md`.
- A `RoomEditor` reads the room's 80-byte overlay (banks 0x26, 0x27) whole; it is stored back
  unchanged, or not at all, by every traced group. Those reads are `copy`.
- A table entry's bytes that an earlier group assigned are `written` for the reader even when
  the assigning group re-stored source content: that group's own evidence covers what it wrote.
- An empty slice reads no byte: `read_trace.py` no longer records it (the store's de-duplication
  compares empty entries, which gave `source` reads of zero bytes).
- A `table` row that reads bytes a `C` group wrote moves to its own `C` group (`notext`,
  `warp-home`) even when its writes are `STABLE`; a `C` row whose writes are `STABLE` stays `C`
  when its group also holds seed-dependent rows or runs after them (Notes give each reason).

Transitive RNG found by reading: `patches.aesthetics.reduceMessageLengths` reaches
`utils.randomNumber`, which ignores its `rnd` argument and draws from the global `random`;
`patches.endscreen.updateEndScreen` draws from the global `random` directly. The global state
is `self.rnd`'s, copied by `random.setstate` in `Randomizer.__init__` before the logic is built,
then advanced by whatever used it before `generateRom`.

## generator.py generateRom

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `rom = ROMWithTables(open(args.input_filename, 'rb'))` | - | input_filename | - | - | - | Parses the tables listed under `romTables.py ROMWithTables.__init__`. | none | parse |
| `pymods = []` | - | - | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `spec = importlib.util.spec_from_loader(pymod, importlib.machinery.SourceFileLoader(pymod, pymod))` | - | pymod | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `module = importlib.util.module_from_spec(spec)` | - | pymod | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `spec.loader.exec_module(module)` | - | pymod | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `pymods.append(module)` | - | pymod | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `pymod.prePatch(rom)` | - | - | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |
| `patches.aesthetics.gfxMod(rom, os.path.join(os.path.dirname(__file__), "gfx", settings.gfxmod))` | gfxmod | - | - | - | - | gfxmod is unsupported (tier1.txt). | unused | gfxmod |
| `assembler.resetConsts()` | - | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("INV_SIZE", 16)` | - | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("HARD_MODE", 1 if settings.hardmode != "none" else 0)` | hardmode | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("DUNGEON_CHAIN", 1 if settings.overworld == 'dungeonchain' else 0)` | overworld | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("ITEM_DIALOGS", 1 if settings.textmode != 'notextbox' else 0)` | textmode | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("KEYSANITY_KEYS", 1 if settings.dungeon_keys == 'keysanity' else 0)` | dungeon_keys | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("KEYSANITY_NIGHTMARE_KEYS", 1 if settings.nightmare_keys == 'keysanity' else 0)` | nightmare_keys | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("KEYSANITY_BEAKS", 1 if settings.dungeon_beaks == 'keysanity' else 0)` | dungeon_beaks | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `assembler.const("KEYSANITY_MAPS", 1 if settings.dungeon_maps == 'keysanity' else 0)` | dungeon_maps | - | - | - | - | Sets an assembler constant read by later ASM patches; writes nothing itself. | none | asm-consts |
| `patches.core.cleanup(rom)` | - | - | - | - | table entities source 0x02C4..0x02C4; table rooms_indoor_b source 0x00C4..0x00C4 |  | table | fixed |
| `patches.core.fixD7exit(rom)` | - | - | - | - | bank 0x26 source 0x0460..0x04AF; table entities source 0x000E..0x000E; table rooms_overworld_top source 0x000E..0x000E |  | table | fixed |
| `patches.core.fixHealthFullCheck(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.phone.patchPhone(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.photographer.fixPhotographer(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixWrittingWrongRoomStatus(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixBossroomTopPush(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixPowderBagSprite(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixEggDeathClearingItems(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.disablePhotoPrint(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.easyColorDungeonAccess(rom)` | - | - | - | - | table entities source 0x0312..0x0312; table rooms_color_dungeon source 0x0012..0x0012 |  | table | fixed |
| `patches.owl.removeOwlEvents(rom)` | - | - | - | - | bank 0x26 source 0x0000..0x3FBF; bank 0x26 written:fixed 0x0460..0x04AF; bank 0x27 source 0x0000..0x103F; table entities source 0x0000..0x02A3; table entities written:fixed 0x000E..0x000E; table rooms_indoor_b source 0x00A3..0x00A3; table rooms_overworld_bottom source 0x0000..0x007F; table rooms_overworld_top source 0x0000..0x007F; table rooms_overworld_top written:fixed 0x000E..0x000E |  | table | fixed |
| `patches.enemies.fixArmosKnightAsMiniboss(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.enemies.enemyQoL(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.bank3e.addBank3E(rom, seed, settings)` | * | - | seed | - | table texts source 0x0111..0x0111 | Writes `seed[:0x20]` and, unless race, `settings.getShortString()` into bank 0x3E; rewrites texts 0x89, 0xC8 to 0xCB, 0xD9 and copies `rom.texts[0x111]` to 0xBE. | C | bank3e |
| `patches.bank3f.addBank3F(rom)` | - | - | - | - | bank 0x0C source 0x37C0..0x37FF; bank 0x2C source 0x0040..0x28FF; bank 0x2E source 0x1120..0x24FF; bank 0x31 source 0x1000..0x11FF; bank 0x32 source 0x1500..0x3DBF; bank 0x34 source 0x3F00..0x3F7F; bank 0x35 source 0x0F00..0x0F1F; bank 0x3F written:fixed 0x2B00..0x2B1F |  | table | fixed |
| `patches.core.removeGhost(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixMarinFollower(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixWrongWarp(rom)` | - | - | - | - | bank 0x26 source 0x0D70..0x0DBF; table entities source 0x002B..0x002B; table rooms_overworld_top source 0x002B..0x002B |  | table | fixed |
| `patches.core.alwaysAllowSecretBook(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.injectMainLoop(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.inventory.advancedInventorySubscreen(rom)` | dungeon_keys, nightmare_keys | - | - | - | bank 0x20 source 0x1994..0x19C6; table background_attributes source 0x0002..0x0002; table background_tiles source 0x0002..0x0002 |  | table | options |
| `patches.inventory.moreSlots(rom)` | - | - | - | - | bank 0x20 mixed:fixed 0x1C30..0x1C95 |  | table | fixed |
| `patches.witch.updateWitch(rom)` | witch | - | - | - | bank 0x26 source 0x1900..0x194F; table entities source 0x0050..0x0050; table rooms_overworld_top source 0x0050..0x0050 |  | table | options |
| `patches.softlock.fixAll(rom)` | - | - | - | - | bank 0x26 source 0x3250..0x329F; table entities source 0x00A1..0x02B7; table rooms_indoor_a source 0x0046..0x00F7; table rooms_indoor_b source 0x00B7..0x00B7; table rooms_overworld_bottom source 0x0021..0x0021 |  | table | fixed |
| `patches.maptweaks.tweakMap(rom)` | rooster | - | - | - | - |  | table | options |
| `patches.maptweaks.tweakBirdKeyRoom(rom)` | rooster | - | - | - | - |  | table | options |
| `patches.chest.fixChests(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.shop.fixShop(rom, allow_both=settings.shopsanity != '', shopsanity=settings.shopsanity != '')` | shopsanity | - | - | - | - | shopsanity is unsupported: always `''`, so both flags are False. | table | options |
| `patches.evilshop.addEvilShop(rom)` | evilshop | - | - | - | bank 0x20 source 0x3054..0x3054; table entities source 0x02A9..0x02A9; table rooms_indoor_b source 0x00A9..0x00A9 |  | table | options |
| `patches.rooster.patchRooster(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.trendy.fixTrendy(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.droppedKey.fixDroppedKey(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.madBatter.upgradeMadBatter(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.tunicFairy.upgradeTunicFairy(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.tarin.updateTarin(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.fishingMinigame.updateFinishingMinigame(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.health.upgradeHealthContainers(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.owl.removeDungeonOwlBeakRequirement(rom)` | dungeon_beaks | - | - | - | - |  | table | options |
| `patches.owl.upgradeDungeonOwlStatues(rom)` | owlstatues | - | - | - | - |  | table | options |
| `patches.owl.upgradeOverworldOwlStatues(rom)` | owlstatues | - | - | - | - |  | table | options |
| `patches.goldenLeaf.fixGoldenLeaf(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.heartPiece.fixHeartPiece(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.seashell.fixSeashell(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.instrument.fixInstruments(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.seashell.upgradeMansion(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.songs.upgradeMarin(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.songs.upgradeManbo(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.songs.upgradeMamu(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.tradeSequence.patchTradeSequence(rom, settings)` | tradequest, * | - | - | - | - | Passes `settings` on to `patchVarious(rom, settings)`. | table | options |
| `patches.tradeSequence.unrequiredTradeSequence(rom)` | tradequest | - | - | - | - |  | table | options |
| `patches.bowwow.fixBowwow(rom)` | - | - | - | - | table entities source 0x02E2..0x02E2; table rooms_indoor_b source 0x00E2..0x00E2 |  | table | fixed |
| `patches.follower.patchFollowerCreation(rom, bowwow_everywhere=settings.bowwow != 'normal', extra_spawn=settings.follower)` | bowwow, follower | - | - | - | - |  | table | options |
| `patches.bowwow.bowwowMapPatches(rom)` | bowwow | - | - | - | table entities source 0x0100..0x02FE; table entities written:fixed 0x0146..0x02E2; table rooms_indoor_a source 0x0000..0x00FF; table rooms_indoor_a written:fixed 0x0046..0x00FC; table rooms_indoor_b source 0x0000..0x00FE; table rooms_indoor_b written:fixed 0x0077..0x00E2 |  | table | options |
| `patches.desert.desertAccess(rom)` | - | - | - | - | bank 0x27 source 0x0F50..0x0F9F; table entities source 0x00FD..0x00FD; table rooms_overworld_bottom source 0x007D..0x007D |  | table | fixed |
| `patches.overworld.patchOverworldTilesets(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.overworld.createDungeonOnlyOverworld(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.shop.changeShopPrices(rom, 200, 500)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.dungeon.patchDungeonChain(rom, logic.world_setup)` | overworld | - | - | world_setup | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.dungeon.patchNoDungeons(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.overworld.patchOverworldTilesets(rom)` #2 | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.alttp.patch(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.overworld.patchOverworldTilesets(rom)` #3 | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `mapgen.store_map(rom, logic.world.map)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.dungeon.removeKeyDoors(rom)` | dungeon_keys | - | - | - | - |  | table | options |
| `patches.dungeon.removeNightmareKeyDoors(rom)` | nightmare_keys | - | - | - | - |  | table | options |
| `patches.reduceRNG.fixHorseHeads(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.bomb.onlyDropBombsWhenHaveBombs(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.colorBook.fixColorBook(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.aesthetics.noSwordMusic(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.aesthetics.reduceMessageLengths(rom, rnd)` | - | - | rnd | - | table texts written:marin 0x00EA..0x00EA | `rnd.choice` of a line of `patches/marin.txt` for text 0x01; its `<rnd>`/`<ord>` placeholders go through `utils.randomNumber`, which ignores its `rnd` and draws from the global `random` (seeded from `self.rnd`'s state in `Randomizer.__init__`). Reads `rom.texts[0xEA]` and others to copy them. | C | marin |
| `hints.addHints(rom, rnd, logic.iteminfo_list)` | - | - | rnd | placement | - | `rnd.shuffle`, `rnd.randint`, `rnd.choice`; reads `spot.item` and `spot.metadata.area` of the spots holding hint items; writes `rom.texts`. | C | hints |
| `patches.aesthetics.allowColorDungeonSpritesEverywhere(rom)` | - | - | - | - | bank 0x20 source 0x06AA..0x0759 |  | table | fixed |
| `patches.aesthetics.allowOverworldBackgroundTileTransitions(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.music.randomizeMusic(rom, rnd)` | music | - | rnd | - | - | `rnd.choice` over the looping tracks for every music slot. | C | music |
| `patches.music.noMusic(rom)` | music | - | - | - | - |  | table | music-fixed |
| `patches.music.shiftedMusic(rom)` | music | - | - | - | bank 0x1B source 0x09C2..0x0A55; bank 0x1E source 0x09BF..0x0A52 |  | table | music-fixed |
| `patches.music.importMusic(rom, rnd, 'ffa')` | music | - | rnd | - | - | music=ffa is unsupported (tier1.txt); reads `music/ffa` and draws from `rnd`. | unused | music-ffa |
| `patches.aesthetics.removeFlashingLights(rom)` | noflash | - | - | - | - |  | table | options |
| `patches.hardMode.oracleMode(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.hardMode.heroMode(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.hardMode.oneHitKO(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.weapons.patchSuperWeapons(rom)` | superweapons | - | - | - | table entities source 0x023D..0x023D; table rooms_indoor_b source 0x003D..0x003D |  | table | options |
| `patches.aesthetics.fastText(rom)` | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.fastText(rom)` #2 | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.noText(rom)` | textmode | - | - | - | table texts mixed:bank3e,fixed,hints,marin all; table texts source 0x0000..0x02AF; table texts written:bank3e 0x0089..0x00D9; table texts written:fixed 0x002A..0x0268; table texts written:hints 0x01B6..0x0293; table texts written:marin 0x0001..0x00F0 | Sets every text but 0x217..0x21A to its last byte, its terminator. It reads the texts that `bank3e`, `hints` and `marin` wrote, so it cannot be a table (trace: `TABLE-READS-SEED` as an `options` row); moved to its own `C` group. `STABLE` (every text a `C` group writes ends with the same terminator), kept `C` all the same: a `table` row may not read bytes a `C` group wrote. | C | notext |
| `patches.chest.shortenDungeonItemTexts(rom)` | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.fastText(rom)` #3 | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.removeDungeonTexts(rom)` | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.removeNagMessages(rom)` | nagmessages | - | - | - | - |  | table | options |
| `patches.aesthetics.slowLowHPBeep(rom)` | lowhpbeep | - | - | - | - |  | table | options |
| `patches.aesthetics.removeLowHPBeep(rom)` | hardmode, lowhpbeep | - | - | - | - |  | table | options |
| `patches.aesthetics.forceLinksPalette(rom, int(settings.linkspalette))` | linkspalette | - | - | - | - |  | table | options |
| `rom.patch(0, 0x0003, "00", "01")` | - | romdebugmode | - | - | - | `args.romdebugmode` only. | unused | debug |
| `rom.patch(4, 0x36F9, "FA4EDB", "3E0000")` | overworld, steal | - | - | - | - | The dungeonchain condition never holds (overworld unsupported). | table | options |
| `rom.patch(4, 0x36F9, "FA4EDB", "3E0100")` | overworld, steal | - | - | - | - | The dungeonchain condition never holds (overworld unsupported). | table | options |
| `rom.patch(4, 0x36F9, "FA4EDB", "3E0000")` #2 | overworld, steal | - | - | - | - | The dungeonchain condition never holds (overworld unsupported). | table | options |
| `patches.shop.preventShopSaveAndQuitTheft(rom)` | overworld, steal | - | - | - | - | The dungeonchain condition never holds (overworld unsupported). | table | options |
| `patches.shop.changeShopPrices(rom, 100, 200)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x2CB)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x29B)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x2B4)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x29C)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x29D)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x2CC)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x2E3)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.shop.createShopRoom(rom, 0x299)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `rom.texts[0x030] = utils.formatText("Only %d {RUPEES}!" % (100,), ask="Buy  No Way")` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `rom.texts[0x02C] = utils.formatText("Only %d {RUPEES}!" % (200,), ask="Buy  No Way")` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `re = roomEditor.RoomEditor(rom, 0x2A1)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `re.entities.append((1, 6, 0x80))` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `re.objects.append(roomEditor.Object(1, 6, 0xA6))` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `re.store(rom)` | shopsanity | - | - | - | - | shopsanity is unsupported (tier1.txt). | unused | shopsanity |
| `patches.health.setStartHealth(rom, 9)` | hpmode | - | - | - | - |  | table | options |
| `patches.health.setStartHealth(rom, 1)` | hpmode | - | - | - | - |  | table | options |
| `patches.health.setStartHealth(rom, 1)` #2 | hardmode, hpmode | - | - | - | - |  | table | options |
| `patches.health.setStartHealth(rom, 5)` | hardmode, hpmode | - | - | - | - |  | table | options |
| `patches.health.limitHitChallange(rom)` | hpmode | - | - | - | - |  | table | options |
| `patches.inventory.songSelectAfterOcarinaSelect(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.quickswap(rom, 1)` | quickswap | - | - | - | - |  | table | options |
| `patches.core.quickswap(rom, 0)` | quickswap | - | - | - | - |  | table | options |
| `world_setup = logic.world_setup` | - | - | - | world_setup | - | Local alias, writes nothing. | none | locals |
| `item_list = logic.iteminfo_list` | - | - | - | placement | - | Local alias, writes nothing. | none | locals |
| `patches.goal.setRaftGoal(rom)` | - | - | - | world_setup | - | Fixed for the goal setting; writes text 0x1A3 for the count. | table | goal-fixed |
| `patches.bingo.setBingoGoal(rom, world_setup.bingo_goals, world_setup.goal)` | - | - | - | world_setup | bank 0x26 written:fixed 0x01E0..0x072F; bank 0x2C source 0x0060..0x29DF; bank 0x2E source 0x07C0..0x26DF; bank 0x2F source 0x10F0..0x319F; bank 0x30 source 0x3000..0x313F; bank 0x31 source 0x0130..0x273F; bank 0x32 source 0x1460..0x3D7F; bank 0x35 source 0x0700..0x071F; bank 0x38 source 0x0080..0x00BF; bank 0x3F written:fixed 0x2920..0x295F; table background_attributes source 0x0015..0x0015; table background_tiles source 0x0015..0x0015; table entities source 0x0274..0x0274; table entities written:fixed 0x0006..0x02A3; table rooms_indoor_b source 0x0074..0x0074; table rooms_indoor_b written:fixed 0x00A3..0x00A3; table rooms_overworld_top written:fixed 0x0006..0x0016; table texts written:goal 0x00CF..0x00CF | `bingo_goals` drawn with `rnd` by `bingo.randomizeGoals` in `WorldSetup.randomize`; writes texts and rooms. | C | goal |
| `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | - | - | - | world_setup | bank 0x26 source 0x0000..0x3F1F; bank 0x26 written:fixed 0x01E0..0x077F; bank 0x26 written:goal 0x01E0..0x022F; bank 0x27 source 0x0640..0x0FEF; bank 0x27 written:fixed 0x0AA0..0x0F9F; table entities source 0x0000..0x00FE; table entities written:fixed 0x0006..0x00FD; table entities written:goal 0x0006..0x0006; table rooms_overworld_bottom source 0x0013..0x007E; table rooms_overworld_bottom written:fixed 0x006E..0x007D; table rooms_overworld_top source 0x0000..0x0059; table rooms_overworld_top written:fixed 0x0006..0x0017; table rooms_overworld_top written:goal 0x0006..0x0006 | `sign_maze` drawn by `maze.buildMaze(rnd)` in `WorldSetup.randomize`; edits rooms. | C | goal |
| `patches.goal.setSeashellGoal(rom, 20)` | - | - | - | world_setup | - | Fixed for the goal setting; writes text 0x1A3 for the count. | table | goal-fixed |
| `patches.goal.setSpecificInstruments(rom, [int(c) for c in world_setup.goal[1:]])` | - | - | - | world_setup | - | goal=specific: the instruments are `rnd.shuffle`d (and the count drawn when goalcount=random). | C | goal |
| `patches.goal.setRequiredInstrumentCount(rom, -1)` | - | - | - | world_setup | - | Fixed for the goal setting; writes text 0x1A3 for the count. | table | goal-fixed |
| `patches.goal.setRequiredInstrumentCount(rom, world_setup.goal_count)` | - | - | - | world_setup | - | goal_count is drawn with `rnd` for goalcount=random or a goal range, else the setting. `STABLE` on the traced cases, none of which draws the count (goalcount=random only with goal=specific there); kept `C` for the drawn counts. | C | goal |
| `patches.chest.setMultiChest(rom, world_setup.multichest)` | - | - | - | world_setup | - | `multichest` is always drawn with `rnd.choices` in `WorldSetup.randomize`. | C | items |
| `patches.entrances.changeEntrances(rom, world_setup.entrance_mapping)` | overworld | - | - | world_setup | bank 0x26 source 0x00F0..0x3ECF; bank 0x26 written:fixed 0x0460..0x360F; bank 0x26 written:goal 0x0500..0x383F; bank 0x26 written:options 0x1900..0x194F; bank 0x27 source 0x0000..0x121F; table entities source 0x0003..0x0301; table entities written:fixed 0x000E..0x0312; table entities written:goal 0x0010..0x02A3; table entities written:options 0x0050..0x02FE; table rooms_color_dungeon source 0x0001..0x0001; table rooms_color_dungeon written:fixed 0x0012..0x0012; table rooms_indoor_a source all; table rooms_indoor_a written:fixed 0x00F7..0x00F7; table rooms_indoor_a written:options 0x0002..0x00FE; table rooms_indoor_b source 0x000E..0x00FE; table rooms_indoor_b written:fixed 0x00A3..0x00A3; table rooms_indoor_b written:goal 0x00A3..0x00A3; table rooms_indoor_b written:options 0x000E..0x00FE; table rooms_overworld_bottom source all; table rooms_overworld_bottom written:fixed 0x001C..0x002C; table rooms_overworld_bottom written:goal 0x0013..0x0033; table rooms_overworld_top source all; table rooms_overworld_top written:fixed 0x000E..0x002B; table rooms_overworld_top written:goal 0x0010..0x0059; table rooms_overworld_top written:options 0x0050..0x0050 | entranceshuffle is unsupported, so the mapping is the identity: every entrance room is parsed and stored back with the same warps (7,768 positions written per run). Settled `C` from the draft's `?`: its writes differ between seeds under goal=maze (`maze` seeds 0 and 2), because it re-stores rooms the maze edited, so it reads bytes a `C` group wrote. | C | entrances |
| `spot.item = spot.item[1:]` | - | - | - | placement | - | Strips the `*` marker of a forced item; writes nothing to the ROM. | none | locals |
| `spot.patch(rom, spot.item)` | - | - | - | placement | bank 0x27 written:fixed 0x0BE0..0x0C2F; bank 0x27 written:goal 0x0BE0..0x0C2F; bank 0x3E source 0x33CE..0x33CE; bank 0x3E written:items 0x38CE..0x38CE; table entities written:fixed 0x00F2..0x00F2; table entities written:goal 0x00F2..0x00F2; table rooms_overworld_bottom written:fixed 0x0072..0x0072; table rooms_overworld_bottom written:goal 0x0072..0x0072 | The placement: each location writes its item. | C | items |
| `patches.enemies.changeBosses(rom, world_setup.boss_mapping)` | - | - | - | world_setup | bank 0x14 source 0x04E0..0x051F; bank 0x3E source 0x342B..0x34BC; bank 0x3E written:items 0x392B..0x39BC; table entities source 0x0106..0x02F8; table entities written:bosses 0x022E..0x022E; table entities written:entrances 0x022C..0x022C; table rooms_indoor_a source 0x0006..0x00EF; table rooms_indoor_b source 0x0023..0x00F8; table rooms_indoor_b written:bosses 0x002E..0x002E; table rooms_indoor_b written:entrances 0x002C..0x002C | Mappings drawn with `rnd` in `WorldSetup.randomize` when boss/miniboss shuffle is on, else identity; reads bank 0x14, 0x3E bytes and rooms through `RoomEditor`. | C | bosses |
| `patches.enemies.changeMiniBosses(rom, world_setup.miniboss_mapping)` | - | - | - | world_setup | table entities source 0x0111..0x030C; table entities written:options 0x0111..0x02E1; table rooms_color_dungeon source 0x0003..0x000C; table rooms_indoor_a source 0x0011..0x00C5; table rooms_indoor_a written:options 0x0011..0x00C5; table rooms_indoor_b source 0x0028..0x00E1; table rooms_indoor_b written:options 0x0028..0x00E1 | Mappings drawn with `rnd` in `WorldSetup.randomize` when boss/miniboss shuffle is on, else identity; reads bank 0x14, 0x3E bytes and rooms through `RoomEditor`. | C | bosses |
| `patches.enemies.randomizeEnemies(rom, seed)` | enemies | - | seed | - | bank 0x26 source 0x0000..0x3FBF; bank 0x26 written:entrances 0x00F0..0x3ECF; bank 0x26 written:fixed 0x01E0..0x392F; bank 0x27 source 0x00A0..0x103F; bank 0x27 written:entrances 0x0000..0x0E5F; bank 0x27 written:fixed 0x01E0..0x0F9F; table entities source 0x0000..0x00FF; table entities written:entrances 0x0003..0x00F9; table entities written:fixed 0x0006..0x00FD; table rooms_overworld_bottom source 0x0001..0x007F; table rooms_overworld_bottom written:entrances 0x0002..0x0079; table rooms_overworld_bottom written:fixed 0x0000..0x007D; table rooms_overworld_top source 0x0000..0x007F; table rooms_overworld_top written:entrances 0x0003..0x0078; table rooms_overworld_top written:fixed 0x0006..0x0064 | Draws from a private `random.Random(seed)`, not the global state; reads every overworld room through `RoomEditor`. | C | enemizer |
| `patches.core.addFrameCounter(rom, len([spot for spot in item_list if type(spot) != locations.keyLocation.KeyLocation]))` | - | romdebugmode | - | placement | bank 0x38 source 0x1100..0x17FF | Count of the non-key spots, a function of the settings only: `STABLE` (identical writes under the three seeds of every case) and it reads only source sprite tiles of bank 0x38, which it moves (`copy`). Settled `table` from the draft's `?`, keyed on the settings that add or drop a non-key location; step 6's random sample widens the key if needed. | table | frame-counter |
| `patches.core.warpHome(rom, settings.overworld == "dungeonchain" or settings.entranceshuffle in ("chaos", "insane", "madness"))  # Needs to be done after setting the start location.` | entranceshuffle, overworld | - | - | - | table background_tiles source 0x000D..0x000D; table entities written:entrances 0x02A3..0x02A3; table rooms_indoor_b written:entrances 0x00A3..0x00A3 | entranceshuffle and dungeonchain are unsupported: the flag is always False. Reads the start house's warp (room 0x2A3) after `entrances` re-stored it, so it cannot be a table (trace: `TABLE-READS-SEED` as an `options` row); moved to its own `C` group. Also re-encodes background 0x0D (the save-and-quit menu). `STABLE` (the warp `entrances` stores back is the vanilla one in tier 1), kept `C` all the same: a `table` row may not read bytes a `C` group wrote. | C | warp-home |
| `patches.titleScreen.setRomInfo(rom, binascii.hexlify(seed).decode("ascii").upper(), settings)` | * | - | seed | - | table background_attributes source 0x0003..0x0004; table background_tiles source 0x0003..0x0004 | Writes the seed hex (or `Race` plus its name) to the title screen and an md5-derived pattern of it. | C | title |
| `patches.endscreen.updateEndScreen(rom)` | - | - | global | - | - | `random.choice(cats)` from the global `random` (cat list sorted by `ladxr.patch`); copies the chosen `patches/cats/*.bin` into bank 0x3F. | C | endscreen |
| `patches.aesthetics.updateSpriteData(rom)` | - | - | - | - | bank 0x26 source 0x0000..0x3FBF; bank 0x26 written:enemizer 0x0000..0x3ECF; bank 0x26 written:entrances 0x00F0..0x3ECF; bank 0x26 written:fixed 0x01E0..0x392F; bank 0x26 written:goal 0x0000..0x3F1F; bank 0x27 source 0x00A0..0x103F; bank 0x27 written:enemizer 0x00F0..0x103F; bank 0x27 written:entrances 0x0000..0x0E5F; bank 0x27 written:fixed 0x01E0..0x0F9F; bank 0x27 written:goal 0x0640..0x0FEF; bank 0x27 written:items 0x0BE0..0x0C2F; table entities source 0x0000..0x0315; table entities written:bosses 0x0106..0x030C; table entities written:enemizer 0x0000..0x00FF; table entities written:entrances 0x0003..0x0312; table entities written:fixed 0x0006..0x02E2; table entities written:goal 0x0000..0x0274; table entities written:items 0x00F2..0x00F2; table entities written:options 0x0100..0x02F8; table rooms_color_dungeon source 0x0000..0x0015; table rooms_color_dungeon written:bosses 0x0003..0x000C; table rooms_color_dungeon written:entrances 0x0001..0x0012; table rooms_indoor_a source 0x0000..0x00FF; table rooms_indoor_a written:bosses 0x0006..0x00EF; table rooms_indoor_a written:entrances 0x0002..0x00FE; table rooms_indoor_a written:fixed 0x0046..0x00FC; table rooms_indoor_a written:options 0x0000..0x00FF; table rooms_indoor_b source 0x0000..0x00F8; table rooms_indoor_b written:bosses 0x0023..0x00F8; table rooms_indoor_b written:entrances 0x000E..0x00FE; table rooms_indoor_b written:fixed 0x0077..0x00E2; table rooms_indoor_b written:goal 0x0074..0x0074; table rooms_indoor_b written:options 0x0000..0x00F8; table rooms_overworld_bottom source 0x0001..0x007F; table rooms_overworld_bottom written:enemizer 0x0008..0x007F; table rooms_overworld_bottom written:entrances 0x0002..0x0079; table rooms_overworld_bottom written:fixed 0x0000..0x007D; table rooms_overworld_bottom written:goal 0x0016..0x007E; table rooms_overworld_bottom written:items 0x0072..0x0072; table rooms_overworld_top source 0x0000..0x007F; table rooms_overworld_top written:enemizer 0x0000..0x0077; table rooms_overworld_top written:entrances 0x0003..0x0078; table rooms_overworld_top written:fixed 0x0006..0x0064; table rooms_overworld_top written:goal 0x0000..0x0057 | Reads the entities of every room (changed by the placement, bosses and enemizer) and rewrites the room sprite data tables. | C | sprites |
| `patches.enemies.doubleTrouble(rom)` | - | doubletrouble | - | - | - | `args.doubletrouble` only. | unused | debug |
| `pymod.postPatch(rom)` | - | - | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |

## randomizer.py Randomizer.__init__

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `rom = generator.generateRom(args, settings, self.seed, self.__logic, rnd=self.rnd)` | * | input_filename, * | rnd, seed | logic | - | The section `generator.py generateRom`. | none | generate |
| `filename = args.output_filename` | - | input_filename, output_filename | - | - | - | The output file name; not ROM content. | none | output |
| `filename = "LADXR_%s.gbc" % (binascii.hexlify(self.seed).decode("ascii").upper())` | - | input_filename | seed | - | - | The output file name; not ROM content. | none | output |
| `rom.save(filename, name="LADXR")` | - | input_filename | - | - | - | The sections `romTables.py ROMWithTables.save`, `rom.py ROM.save` and `rom.py ROM.fixHeader`. | C | save |
| `log = spoilerLog.SpoilerLog(settings, args, rom)` | race, * | log_directory, spoilerformat, * | - | - | bank 0x00 written:title 0x0007..0x0007; bank 0x05 written:items 0x08D5..0x08D5; bank 0x05 written:options 0x08EF..0x08EF; bank 0x14 written:items 0x0564..0x0874; bank 0x18 written:items 0x0F90..0x0F92; bank 0x19 written:items 0x06C5..0x072B; bank 0x26 written:enemizer 0x00F0..0x3ECF; bank 0x26 written:entrances 0x00F0..0x3ECF; bank 0x27 written:enemizer 0x00F0..0x0CCF; bank 0x27 written:entrances 0x0000..0x121F; bank 0x27 written:items 0x0BE0..0x0C2F; bank 0x36 written:items 0x11BF..0x11C0; bank 0x3E written:bank3e 0x2F00..0x2F0F; bank 0x3E written:items 0x3800..0x3E31; table entities source 0x0106..0x0300; table entities written:bosses 0x0106..0x030C; table entities written:enemizer 0x0003..0x00F4; table entities written:entrances 0x0003..0x0312; table entities written:items 0x00F2..0x00F2; table entities written:options 0x0106..0x0234; table rooms_color_dungeon source 0x0000..0x0000; table rooms_color_dungeon written:bosses 0x0003..0x000C; table rooms_color_dungeon written:entrances 0x0012..0x0012; table rooms_indoor_a source 0x0006..0x00BC; table rooms_indoor_a written:bosses 0x0006..0x00C5; table rooms_indoor_a written:entrances 0x0017..0x00FE; table rooms_indoor_a written:options 0x0006..0x00BC; table rooms_indoor_b source 0x0023..0x0034; table rooms_indoor_b written:bosses 0x0023..0x00E1; table rooms_indoor_b written:entrances 0x000E..0x00FE; table rooms_indoor_b written:options 0x0023..0x0034; table rooms_overworld_bottom written:enemizer 0x0008..0x0074; table rooms_overworld_bottom written:entrances 0x0002..0x0079; table rooms_overworld_bottom written:items 0x0072..0x0072; table rooms_overworld_top source all; table rooms_overworld_top written:enemizer 0x0003..0x0077; table rooms_overworld_top written:entrances 0x0003..0x0078 | Builds the spoiler log, reading the finished ROM back (`readHexSeed`, each location's `read`, rooms); skipped for race seeds. | C | spoiler |
| `filename = "LADXR_%s_%s.json" % (datetime.now().strftime("%Y-%m-%d_%H-%M-%S"), log.seed)` | race | log_directory, spoilerformat | seed | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log_path = os.path.join(args.log_directory, filename)` | race | log_directory, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log.outputJson(log_path)` | race | log_directory, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log.output(args.spoiler_filename)` | race | log_directory, spoiler_filename, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |

## romTables.py ROMWithTables.__init__

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `super().__init__(filestream)` | - | - | - | - | - | Reads the 64 banks. | none | parse |
| `self.texts = Texts(self)` | - | - | - | - | bank 0x09 source 0x268E..0x3D41; bank 0x14 source 0x1934..0x3F47; bank 0x16 source 0x1701..0x3FA8; bank 0x1C source 0x0001..0x3EB2; bank 0x1D source 0x0000..0x3FCB | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.entities = Entities(self)` | - | - | - | - | bank 0x16 source 0x0000..0x1700 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_overworld_top = RoomsOverworldTop(self)` | - | - | - | - | bank 0x00 source 0x31CD..0x321E; bank 0x09 source 0x0000..0x268D | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_overworld_bottom = RoomsOverworldBottom(self)` | - | - | - | - | bank 0x00 source 0x31DD..0x31DE; bank 0x09 source 0x0100..0x01FF; bank 0x1A source 0x0000..0x1E75 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_indoor_a = RoomsIndoorA(self)` | - | - | - | - | bank 0x00 source 0x31A1..0x31A2; bank 0x0A source 0x0000..0x3B76 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_indoor_b = RoomsIndoorB(self)` | - | - | - | - | bank 0x0B source 0x0000..0x3E00 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_color_dungeon = RoomsColorDungeon(self)` | - | - | - | - | bank 0x0A source 0x3B77..0x3F42 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.room_sprite_data_overworld = OverworldRoomSpriteData(self)` | - | - | - | - | bank 0x20 source 0x30D3..0x363A | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.room_sprite_data_indoor = IndoorRoomSpriteData(self)` | - | - | - | - | bank 0x20 source 0x31D3..0x383E | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.background_tiles = BackgroundTilesTable(self)` | - | - | - | - | bank 0x08 source 0x0DD4..0x391F; bank 0x20 source 0x052B..0x0576 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.background_attributes = BackgroundAttributeTable(self)` | - | - | - | - | bank 0x24 source 0x1C4B..0x3CD0 | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |

## romTables.py ROMWithTables.save

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `self.texts.store(self)` | - | - | - | - | bank 0x09 written:save 0x268E..0x33A3; bank 0x14 written:save 0x1934..0x3CEA; bank 0x16 written:save 0x1701..0x2BB3; bank 0x1C written:save 0x0B2A..0x2F23; bank 0x1D written:save 0x0000..0x2C05 | Re-serializes the tables edited by the patches. | C | save |
| `self.entities.store(self)` | - | - | - | - | bank 0x16 written:save 0x0657..0x15EA | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_overworld_top.store(self)` | - | - | - | - | bank 0x09 written:save 0x1AC4..0x1B18 | Re-serializes the tables edited by the patches. `STABLE` on the traced cases; kept `C`: it packs entries the `C` groups `entrances`, `goal` (maze), `enemizer` and `items` rewrite. | C | save |
| `self.rooms_overworld_bottom.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. `STABLE` on the traced cases; kept `C`: it packs entries the `C` groups `entrances`, `goal` (maze), `enemizer` and `items` rewrite. | C | save |
| `self.rooms_indoor_a.store(self)` | - | - | - | - | bank 0x0A written:save 0x1720..0x3660 | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_indoor_b.store(self)` | - | - | - | - | bank 0x0B written:save 0x0200..0x255A | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_color_dungeon.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `leftover_storage = self.room_sprite_data_overworld.store(self)` | - | - | - | - | bank 0x20 written:save 0x33F3..0x356A | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.addStorage(leftover_storage)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.patch(0x00, 0x0DFA, ASM("ld hl, $763B"), ASM("ld hl, $%04x" % (leftover_storage[0]["start"] \| 0x4000)))` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.adjustDataStart(leftover_storage[0]["start"])` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.store(self)` | - | - | - | - | bank 0x20 written:save 0x3503..0x380A | Re-serializes the tables edited by the patches. | C | save |
| `self.background_tiles.store(self)` | - | - | - | - | bank 0x08 written:save 0x2BD4..0x2F9A | Re-serializes the tables edited by the patches. | C | save |
| `self.background_attributes.store(self)` | - | - | - | - | bank 0x24 written:save 0x218E..0x2DE4 | Re-serializes the tables edited by the patches. | C | save |
| `super().save(filename, name=name)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |

## rom.py ROM.save

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `self.fixHeader(name=name)` | - | - | - | - | - | The section `rom.py ROM.fixHeader`. | C | save |
| `f = open(file, "wb")` | - | - | - | - | - | Writes the banks to the output file. | none | output |
| `f.write(bank)` | - | - | - | - | - | Writes the banks to the output file. | none | output |
| `f.close()` | - | - | - | - | - | Writes the banks to the output file. | none | output |
| `file.write(bank)` | - | - | - | - | - | Writes the banks to the output file. | none | output |

## rom.py ROM.fixHeader

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `name = name.encode("utf-8")` | - | - | - | - | - | The title in the header (`LADXR`). | C | header |
| `name = (name + (b"\x00" * 15))[:15]` | - | - | - | - | - | The title in the header (`LADXR`). | C | header |
| `self.banks[0][0x134:0x143] = name` | - | - | - | - | - | The title in the header (`LADXR`). `STABLE`; kept `C` with the rest of `fixHeader`, which runs after `save` over the final bytes (one sub-step). | C | header |
| `checksum = 0` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `checksum -= c + 1` | - | - | - | - | bank 0x00 mixed:header 0x0134..0x014C | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14D] = checksum & 0xFF` | - | - | - | - | - | Header and global checksums over the final bytes. `STABLE` (the header checksum covers 0x134..0x14C, which only the name row writes); kept `C` with the global checksum, one sub-step. | C | header |
| `self.banks[0][0x14E] = 0` | - | - | - | - | - | Header and global checksums over the final bytes. `STABLE` (the header checksum covers 0x134..0x14C, which only the name row writes); kept `C` with the global checksum, one sub-step. | C | header |
| `self.banks[0][0x14F] = 0` | - | - | - | - | - | Header and global checksums over the final bytes. `STABLE` (the header checksum covers 0x134..0x14C, which only the name row writes); kept `C` with the global checksum, one sub-step. | C | header |
| `checksum = 0` #2 | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `checksum = (checksum + sum(bank)) & 0xFFFF` | - | - | - | - | bank 0x00 mixed:bank3e,endscreen,fixed,frame-counter,goal,header,options,save,sprites,title all; bank 0x00 mixed:bank3e,endscreen,fixed,frame-counter,header,options,save,sprites,title all; bank 0x01 mixed:fixed,goal,options,warp-home all; bank 0x01 mixed:fixed,options,warp-home all; bank 0x02 mixed:bosses,fixed,music,options all; bank 0x02 mixed:bosses,fixed,options all; bank 0x03 mixed:bank3e,bosses,fixed,goal,items,options all; bank 0x03 mixed:bank3e,bosses,fixed,items,options all; bank 0x04 mixed:bosses,fixed,goal,options all; bank 0x04 mixed:bosses,fixed,options all; bank 0x05 mixed:fixed,frame-counter,items,options all; bank 0x06 mixed:bosses,fixed,options all; bank 0x07 mixed:fixed,options all; bank 0x08..0x0B mixed:save all; bank 0x0C mixed:goal all; bank 0x0C..0x0E source all; bank 0x0D written:goal all; bank 0x0F mixed:warp-home all; bank 0x10..0x13 source all; bank 0x14 mixed:bosses,fixed,items,options,save all; bank 0x14 mixed:bosses,fixed,items,save all; bank 0x15 mixed:fixed all; bank 0x15 mixed:fixed,goal all; bank 0x15 mixed:fixed,options all; bank 0x16 mixed:save all; bank 0x17 mixed:endscreen,frame-counter all; bank 0x18..0x19 mixed:fixed,goal,items,options all; bank 0x18..0x19 mixed:fixed,items,options all; bank 0x1A mixed:save all; bank 0x1B mixed:music-fixed all; bank 0x1B source all; bank 0x1C..0x1D mixed:save all; bank 0x1E mixed:music-fixed all; bank 0x1E..0x1F source all; bank 0x20 mixed:bank3e,bosses,fixed,options,save,sprites all; bank 0x20 mixed:bank3e,fixed,goal,options,save,sprites all; bank 0x20 mixed:bank3e,fixed,options,save,sprites all; bank 0x21 mixed:fixed all; bank 0x21 mixed:fixed,goal all; bank 0x21 mixed:fixed,options all; bank 0x22..0x23 source all; bank 0x24 mixed:options,save all; bank 0x25 source all; bank 0x26 mixed:enemizer,entrances,fixed all; bank 0x26 mixed:entrances,fixed all; bank 0x26 mixed:entrances,fixed,goal all; bank 0x27 mixed:enemizer,entrances,fixed,frame-counter,items all; bank 0x27 mixed:entrances,fixed,frame-counter,goal,items all; bank 0x27 mixed:entrances,fixed,frame-counter,items all; bank 0x28 mixed:bank3e,fixed all; bank 0x29..0x2B source all; bank 0x2C mixed:fixed all; bank 0x2D..0x35 source all; bank 0x30 mixed:goal all; bank 0x36 mixed:bosses,enemizer,fixed,items,options all; bank 0x36 mixed:bosses,fixed,items,options all; bank 0x37 source all; bank 0x38 mixed:frame-counter all; bank 0x39..0x3D source all; bank 0x3E mixed:bank3e,bosses,fixed,items all; bank 0x3E mixed:bank3e,fixed,items all; bank 0x3F mixed:endscreen,fixed all; bank 0x3F mixed:endscreen,fixed,goal all | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14E] = checksum >> 8` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14F] = checksum & 0xFF` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |

## ROMWithTables.save analysis

`rom.save(filename, name="LADXR")` re-serializes the eleven parsed tables in this order, then
writes the header (`ROM.fixHeader`, group `header`) and the banks. Each table is a
`PointerTable` that kept, at parse time, every entry's bytes (or an integer pointer), its bank,
and its storage regions: the source spans of its entries, merged when contiguous, claimed whole
from the first to the last entry for the two room sprite data tables (`claim_storage_gaps`), and
grown to the end of the bank for the color dungeon rooms and the two background tables
(`expand_to_end_of_bank`).

`store` packs the alt-pointer entries first, then every entry in index order, each into the
first region of its bank with room left (first fit, regions sorted by start), and rewrites its
pointer: two bytes, little-endian, `| 0x4000`, or one index byte relative to `data_addr` for the
sprite data tables. An entry equal to one already stored in the same bank is not stored again:
for the variable-size tables (texts, entities, rooms, backgrounds) every suffix of every stored
entry is registered (aggressive de-duplication), so an entry may point inside another; for the
four-byte sprite data entries only whole entries match. `store`'s readback asserts are the only
reads the trace sees (`written:save`); its reads of its own entry list are invisible to it.

What C has to reproduce, on every seed: the whole packing of every table. Any changed entry
(hints and other texts, entities and rooms edited by `items`, `bosses`, `enemizer`, `goal`,
`entrances`, every room's sprite data from `sprites`, backgrounds 3, 4 and 0x0D from `title`
and `warp-home`) shifts what follows it, so no table's output is settings-only: the group is
`C`, its two `STABLE` rows included. The pointers and the packing are computed; an unchanged
entry's bytes are a copy of its source span (a source reference at its new offset); a changed
entry's bytes are its group's own writes. The de-duplication decides by content: for entities
and rooms the `room-entities` and `room-objects` extractions give that content in decimal; for
texts and backgrounds, whose content may not be extracted, `save-layout` gives a fingerprint of
every source suffix; for the sprite data, `sprites` computes every room's four values and
`save-layout` gives those of the entries it leaves. `save-layout` also gives the storage regions
and every entry's source span, so C never parses the ROM.

`self.patch(0x00, 0x0DFA, ...)` is derived, not read: `room_sprite_data_overworld.store` returns
its leftover storage, whose first region starts right after the last overworld sprite data entry
it packed. The patch replaces the `ld hl, $763B` that loads the indoor sprite data base (0x363B,
`data_addr` of the indoor table, `| 0x4000`) with `ld hl` of that start `| 0x4000`; the indoor
table then counts its indices from that start (`adjustDataStart`) and packs into its own regions
plus the leftover (`addStorage`). Its value is a function of the number of distinct overworld
sprite data entries, which `sprites` decides on every seed.

## Groups

| Group | Side | Settings key | Depends on | Evidence |
|---|---|---|---|---|
| asm-consts | none | dungeon_beaks, dungeon_keys, dungeon_maps, hardmode, nightmare_keys, overworld, textmode | - | - |
| bank3e | C | race, * | - | bank3e, settings.txt |
| bosses | C | boss, miniboss | entrances, items, options | bosses, pyrandom_vectors.txt, extract:room-entities, extract:room-objects, extract:boss-sources |
| debug | unused | - | - | - |
| endscreen | C | - | - | endscreen, pyrandom_vectors.txt |
| enemizer | C | enemies | entrances, fixed | enemizer, pyrandom_vectors.txt, extract:room-entities, extract:room-objects |
| entrances | C | overworld | fixed, goal, options | entrances, extract:room-entities, extract:room-objects |
| fixed | table | - | - | - |
| frame-counter | table | boomerang, dungeon_beaks, dungeon_keys, goal, nightmare_keys, owlstatues, rooster, tradequest | - | - |
| generate | none | - | - | - |
| gfxmod | unused | gfxmod | - | - |
| goal | C | goal, goalcount | fixed | goal, pyrandom_vectors.txt, extract:room-entities, extract:room-objects |
| goal-fixed | table | goal | - | - |
| header | C | - | bank3e, bosses, endscreen, enemizer, entrances, fixed, frame-counter, goal, items, music, music-fixed, options, save, sprites, title, warp-home | header |
| hints | C | - | - | hints, pyrandom_vectors.txt |
| items | C | - | fixed, goal | items |
| locals | none | - | - | - |
| marin | C | - | - | marin, pyrandom_vectors.txt |
| music | C | music | - | music, pyrandom_vectors.txt |
| music-ffa | unused | music | - | - |
| music-fixed | table | music | - | - |
| notext | C | textmode | bank3e, fixed, hints, marin | notext, extract:save-layout |
| options | table | bowwow, dungeon_beaks, dungeon_keys, evilshop, follower, hardmode, hpmode, linkspalette, lowhpbeep, nagmessages, nightmare_keys, noflash, overworld, owlstatues, quickswap, rooster, shopsanity, steal, superweapons, textmode, tradequest, witch | fixed | - |
| output | none | race | - | - |
| overworld | unused | overworld | - | - |
| parse | none | - | - | - |
| pymod | unused | - | - | - |
| save | C | - | - | save, extract:save-layout, extract:room-entities, extract:room-objects, extract:backgrounds |
| shopsanity | unused | shopsanity | - | - |
| spoiler | C | race | bank3e, bosses, enemizer, entrances, items, options, title | spoiler, extract:room-entities, extract:room-objects |
| sprites | C | - | bosses, enemizer, entrances, fixed, goal, items, options | sprites, extract:room-entities, extract:room-objects |
| title | C | race, * | - | title, extract:backgrounds |
| warp-home | C | entranceshuffle, overworld | entrances | warp-home, extract:room-objects, extract:backgrounds |

## Read coverage

| Group | Row | Target | Range | Kind | Reason |
|---|---|---|---|---|---|
| sprites | * | bank 0x26 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere; updateSpriteData never stores a room |
| sprites | * | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere; updateSpriteData never stores a room |
| sprites | * | table entities | * | extract:room-entities | RoomEditor parses the room's entity list; the entity types and positions decide: each entity type selects its sprite slots |
| sprites | * | table rooms_color_dungeon | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) on an overworld room sets slot 3 |
| sprites | * | table rooms_indoor_a | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) on an overworld room sets slot 3 |
| sprites | * | table rooms_indoor_b | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) on an overworld room sets slot 3 |
| sprites | * | table rooms_overworld_bottom | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) on an overworld room sets slot 3 |
| sprites | * | table rooms_overworld_top | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) on an overworld room sets slot 3 |
| bosses | * | bank 0x14 | 0x04E0..0x051F | extract:boss-sources | changeBosses sets to 0xFF each byte of the color dungeon warp table that is 0x00: the offsets of those zeros decide |
| bosses | * | bank 0x3E | 0x3300..0x37FF | extract:boss-sources | the source item id of a boss room's second item table, rewritten for the dungeon the boss moves to (boss shuffle only) |
| bosses | * | bank 0x3E | 0x3800..0x3FFF | written | the boss room's chest item, written by `items` |
| bosses | * | table entities | * | extract:room-entities | RoomEditor parses the room's entity list; the entity types and positions decide (miniboss rooms keep their warp entity 0x61) |
| bosses | * | table rooms_color_dungeon | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide (clean boss rooms drop warps, stairs and lava; minibosses filter by position and type) |
| bosses | * | table rooms_indoor_a | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide (clean boss rooms drop warps, stairs and lava; minibosses filter by position and type) |
| bosses | * | table rooms_indoor_b | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide (clean boss rooms drop warps, stairs and lava; minibosses filter by position and type) |
| enemizer | * | bank 0x26 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| enemizer | * | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| enemizer | * | table entities | * | extract:room-entities | RoomEditor parses the room's entity list; the entity types and positions decide: which entities are enemies and the sprite slots the others hold |
| enemizer | * | table rooms_overworld_bottom | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) fixes slot 3 |
| enemizer | * | table rooms_overworld_top | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: a pushable gravestone (0xC5) fixes slot 3 |
| entrances | * | bank 0x26 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| entrances | * | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| entrances | * | table entities | * | extract:room-entities | RoomEditor parses and stores back each entrance room's entities unchanged (identity mapping in tier 1) |
| entrances | * | table rooms_color_dungeon | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: the warps found and replaced by themselves (identity mapping in tier 1) |
| entrances | * | table rooms_indoor_a | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: the warps found and replaced by themselves (identity mapping in tier 1) |
| entrances | * | table rooms_indoor_b | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: the warps found and replaced by themselves (identity mapping in tier 1) |
| entrances | * | table rooms_overworld_bottom | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: the warps found and replaced by themselves (identity mapping in tier 1) |
| entrances | * | table rooms_overworld_top | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide: the warps found and replaced by themselves (identity mapping in tier 1) |
| goal | `patches.bingo.setBingoGoal(rom, world_setup.bingo_goals, world_setup.goal)` | * | * | cut:goal-bingo | the bingo card's tiles are source graphics shifted, flipped and recoloured per drawn goal |
| goal | `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | bank 0x26 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| goal | `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere |
| goal | `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | table entities | * | extract:room-entities | RoomEditor parses the room's entity list; the entity types and positions decide: the sign is inserted first, the egg room drops its 0xDE entities |
| goal | `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | table rooms_overworld_bottom | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide; stored back unchanged |
| goal | `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | table rooms_overworld_top | * | extract:room-objects | RoomEditor parses the room's objects; their kinds, types, positions and warps decide; stored back unchanged |
| items | * | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere (the beach room, 0xF2) |
| items | * | bank 0x3E | * | copy | AnglerKey copies the angler key's two item bytes (rooms 0x0CE to 0x1F8) |
| items | * | table entities | * | written | the beach room (0xF2) as `fixed` (or the maze) last stored it |
| items | * | table rooms_overworld_bottom | * | written | the beach room (0xF2) as `fixed` (or the maze) last stored it |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x00 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x05 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x14 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x18 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x19 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x36 | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x3E | * | written | read back from the groups that wrote it (race flag, seed, short settings, every location's item) |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x26 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere; the spoiler never stores a room |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | bank 0x27 | * | copy | RoomEditor.__init__ copies the room's 80-byte overlay; stored back unchanged when the room is stored (a source reference), else it goes nowhere; the spoiler never stores a room |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table entities | * | extract:room-entities | the boss and miniboss mappings read back from the boss rooms' entities |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table rooms_color_dungeon | * | extract:room-objects | the entrance and boss mappings read back from the rooms' warps |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table rooms_indoor_a | * | extract:room-objects | the entrance and boss mappings read back from the rooms' warps |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table rooms_indoor_b | * | extract:room-objects | the entrance and boss mappings read back from the rooms' warps |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table rooms_overworld_bottom | * | extract:room-objects | the entrance and boss mappings read back from the rooms' warps |
| spoiler | `log = spoilerLog.SpoilerLog(settings, args, rom)` | table rooms_overworld_top | * | extract:room-objects | the entrance and boss mappings read back from the rooms' warps |
| title | * | table background_attributes | * | extract:backgrounds | BackgroundEditor parses title backgrounds 3 and 4; their tile values decide its run-length re-encoding |
| title | * | table background_tiles | * | extract:backgrounds | BackgroundEditor parses title backgrounds 3 and 4; their tile values decide its run-length re-encoding |
| warp-home | * | table entities | * | written | the start house (0x2A3) as `entrances` stored it back; its warp's room and target |
| warp-home | * | table rooms_indoor_b | * | written | the start house (0x2A3) as `entrances` stored it back; its warp's room and target |
| warp-home | * | table background_tiles | * | extract:backgrounds | BackgroundEditor parses background 0x0D; its tile values decide the moved rows and the re-encoding |
| notext | * | table texts | * | copy | each text keeps only its last byte, the terminator; the save-layout extraction says which entries are pointers |
| bank3e | * | table texts | * | copy | text 0x111 copied whole to 0xBE |
| marin | * | table texts | * | written | text 0xEA, written by marin a line earlier, copied to 0xEB and 0xEC |
| header | `checksum -= c + 1` | * | * | computed | the header checksum 0x14D over 0x134..0x14C |
| header | `checksum = (checksum + sum(bank)) & 0xFFFF` | * | * | computed | the global checksum 0x14E..0x14F over every bank |
| save | * | * | * | written | PointerTable.store reads back an entry it stored this pass, to assert a de-duplicated pointer |

## Extractions

| Name | Group | Holds | Derived from | Format |
|---|---|---|---|---|
| room-entities | sprites | For every room 0x000..0x315 in the source state: the span of its entity entry (bank 0x16 offset, length) and each entity's x, y and type | `ROMWithTables.entities` of the source ROM, parsed like `RoomEditor.__init__` | one line per room: room, offset, length, then x,y,type per entity; decimal numbers of at most 3 digits |
| room-objects | sprites | For every room 0x000..0x315 and alt room (Alt06, Alt0E, Alt1B, Alt2B, Alt79, Alt8C, Alt1F5) in the source state: the span of its object entry (bank, offset, length), animation id, floor object and each object's kind (single, horizontal, vertical, warp), x, y, type and count, or a warp's type, map, room and target x, y | the five room tables of the source ROM, parsed like `RoomEditor.__init__` | one line per room: room, bank, offset, length, animation, floor, then one token group per object; decimal numbers of at most 3 digits |
| boss-sources | bosses | The offsets in bank 0x14 0x04E0..0x051F that hold 0x00, and the item id at bank 0x3E 0x3300 + each of the nine boss rooms | the source ROM's bytes at those offsets | two lines: offsets, then room and item id pairs; decimal |
| backgrounds | title | For background tables 3, 4 and 0x0D (tiles) and 3, 4 (attributes): the span of the entry and its parsed tile map, each VRAM address with its value | `ROMWithTables.background_tiles` and `background_attributes` of the source ROM, parsed like `BackgroundEditor.__init__` | one line per address run: table, index, address, values; decimal numbers of at most 3 digits |
| save-layout | save | For each table `ROMWithTables.save` re-serializes: the storage regions after parsing (bank, start, end), every entry's source span or integer pointer, the alt pointers' spans, the de-duplication among source entries (entry and suffix equal to an earlier one), the four values of every room sprite data entry `updateSpriteData` leaves, and a 64-bit fingerprint of every suffix of every source text and background entry (the tables no other extraction holds), so a written entry can be matched against them without the ROM | the parsed `PointerTable`s of the source ROM | one section per table; decimal numbers of at most 3 digits (a fingerprint as eight of them) |

## Blind spots

| File | Line | Verdict |
|---|---|---|
| dungeongen/dungeongen.py | `f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{row_width * scale * 8}" height="{rows * scale * 8}">')` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write(` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write(` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write(` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write(f'<text x="{r.x + .1}" y="{r.y + .3}" alignment-baseline="hanging" font-size="0.2">{r.type}</text>')` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write("</g>")` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| dungeongen/dungeongen.py | `f.write("</svg>")` | not a ROM read: the SVG text of a generated dungeon (debug dump) |
| gfxconv.py | `open(output_filename, "wb").write(all_tile_data + all_attr_data + all_pal_data)` | not a ROM read: the graphics converter's own output file (a tool, not `generateRom`) |
| gfxconv.py | `open(args.output_file, "wb").write(data)` | not a ROM read: the graphics converter's own output file (a tool, not `generateRom`) |
| launch_web_ui.py | `self.wfile.write(f.read())` | not a ROM read: the web UI's HTTP response |
| main.py | `f.write(";Plandomizer data\n;Items: %s\n" % (", ".join(map(lambda n: getattr(locations.items, n), filter(lambda n: not n.startswith("__"), dir(locations.items))))))` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| main.py | `f.write(";Modify the item pool:\n")` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| main.py | `f.write(";Pool:SWORD:+5\n")` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| main.py | `f.write(";Pool:RUPEES_50:-5\n")` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| main.py | `f.write(";%r\n" % (ii.metadata))` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| main.py | `f.write("Location:%s: \n" % (ii.nameId))` | not a ROM read: the empty plan template (`args.emptyplan`, never set by the oracle) |
| mapexport.py | `f.write("<img src='img/overworld.png'><br><br>")` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write(f"<img src='img/dungeon_{n}.png' map='dungeon_{n}'><br><br>")` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write(f"<img src='img/underworld_{n}.png' underworld='{n}'><br><br>")` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write("<script>var data = ")` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write(json.dumps(self.__json_data))` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write("""` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write("</script><div id='tooltip' style='position:absolute; background-color: white; padding: 4px; border: solid black 1px; white-space: nowrap'>X</div><div style='height:300px'></div>")` | not a ROM read: the HTML text of the map export |
| mapexport.py | `f.write("%s<br><img src='%s'><br><br>" % (name[5:], name[5:]))` | not a ROM read: the HTML text of the map export |
| musicData.py | `f.write("version: 1\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"pulse_instrument: vibrato: {vibrato} duty: {duty} volume: {volume} vol_change: {env_dir_inc}{env_pace} unk1: 0x{instrument[2]:02x}\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"wave_instrument: {binascii.hexlify(instrument[2]).decode('ascii')} volume: {(instrument[1] >> 5) & 0x03} effect: 0x{instrument[1] & 0x9F:02x}\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("sequence:\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("  intro\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("  main\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("pattern: intro\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("pattern: main\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"  {timestep:04} {channel_idx+1} {noteToString(channel_idx, op[0])} {length}\n")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"  {timestep:04} {channel_idx+1} ")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"={wave_instruments.index(op):02}")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"={pulse_instruments.index(op):02}")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"+U1")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"-U1")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"+U2")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"-U2")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"+U3")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"-U3")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(op[1])` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("END")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write(f"NFF {length}")` | not a ROM read: the text of a song dump file |
| musicData.py | `f.write("\n")` | not a ROM read: the text of a song dump file |
| patches/titleScreen.py | `import hashlib` | not a ROM read: an import |
| patches/titleScreen.py | `seednr = int(hashlib.md5(seed.encode('ascii', 'replace')).hexdigest(), 16)` | not a ROM read: the MD5 of the seed's text |
| rom.py | `f.write(bank)` | ROM read covered by the row `f.write(bank)` of `rom.py ROM.save` (group output): every bank written out whole |
| rom.py | `file.write(bank)` | ROM read covered by the row `file.write(bank)` of `rom.py ROM.save` (group output): every bank written out whole |
| spoilerLog.py | `logFile.write(str(self))` | not a ROM read: the spoiler text, built earlier by `log = spoilerLog.SpoilerLog(settings, args, rom)` (group spoiler) |
| spoilerLog.py | `logFile.write(jsonContent)` | not a ROM read: the spoiler text, built earlier by `log = spoilerLog.SpoilerLog(settings, args, rom)` (group spoiler) |
