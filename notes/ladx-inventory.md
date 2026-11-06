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
`read_check.py --print`; `-` for a row with no read, and for every row until the real-data run
fills the column. `Notes` holds what reading the callee showed. `Side` is `table` for a fixed or
settings-only write, `C` for a seed- or placement-dependent one, `none` for a statement that
writes no ROM byte, `unused` for a path the oracle never takes (an `args` debug path or an
unsupported setting of `tier1.txt`). A row is wholly one side: a `C` row that also writes fixed
bytes keeps them on the C side, and its Notes say so. `Group` names the row's group, listed under
Groups with the same side.

The draft's two `?` rows (`changeEntrances`, `addFrameCounter`) are `C` provisionally, until the
real-data run settles them. Draft groups with rows of several sides were split: `goal-fixed`,
`music-fixed`, `music-ffa`, and `locals` for the local aliases; the spoiler's file-name and
output rows moved to `output`, and the row that builds the log is `C`, group `spoiler`.

The other tables: **Groups** (Side; Settings key, the settings its rows name, what a `table`
group's table is keyed on; Depends on, the groups whose writes it reads, from the trace's
provenance, `-` until the real-data run; Evidence, required for a `C` group: its write-log group,
its own name, then the ROM-free fixtures and `extract:<name>` extractions its Phase 3 claim
needs). **Read coverage**: how each read of a `C` group is covered (Row `*` for every row of the
group; Kind `copy`, `written`, `computed`, `table`, `extract:<name>` or `cut:<id>`).
**Extractions**: the fixtures step 7 produces under `tests/ladx/extract/` (the Nintendo-bytes
check's strict scope). **Blind spots**: every line `read_trace.py --blind-spots` lists, judged
either not a ROM read or a ROM read covered by a named row.

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
| `patches.core.cleanup(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixD7exit(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixHealthFullCheck(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.phone.patchPhone(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.photographer.fixPhotographer(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixWrittingWrongRoomStatus(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixBossroomTopPush(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.bugfixPowderBagSprite(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixEggDeathClearingItems(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.disablePhotoPrint(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.easyColorDungeonAccess(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.owl.removeOwlEvents(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.enemies.fixArmosKnightAsMiniboss(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.enemies.enemyQoL(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.bank3e.addBank3E(rom, seed, settings)` | * | - | seed | - | - | Writes `seed[:0x20]` and, unless race, `settings.getShortString()` into bank 0x3E; rewrites texts 0x89, 0xC8 to 0xCB, 0xD9 and copies `rom.texts[0x111]` to 0xBE. | C | bank3e |
| `patches.bank3f.addBank3F(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.removeGhost(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixMarinFollower(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.fixWrongWarp(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.alwaysAllowSecretBook(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.core.injectMainLoop(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.inventory.advancedInventorySubscreen(rom)` | dungeon_keys, nightmare_keys | - | - | - | - |  | table | options |
| `patches.inventory.moreSlots(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.witch.updateWitch(rom)` | witch | - | - | - | - |  | table | options |
| `patches.softlock.fixAll(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.maptweaks.tweakMap(rom)` | rooster | - | - | - | - |  | table | options |
| `patches.maptweaks.tweakBirdKeyRoom(rom)` | rooster | - | - | - | - |  | table | options |
| `patches.chest.fixChests(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.shop.fixShop(rom, allow_both=settings.shopsanity != '', shopsanity=settings.shopsanity != '')` | shopsanity | - | - | - | - | shopsanity is unsupported: always `''`, so both flags are False. | table | options |
| `patches.evilshop.addEvilShop(rom)` | evilshop | - | - | - | - |  | table | options |
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
| `patches.bowwow.fixBowwow(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.follower.patchFollowerCreation(rom, bowwow_everywhere=settings.bowwow != 'normal', extra_spawn=settings.follower)` | bowwow, follower | - | - | - | - |  | table | options |
| `patches.bowwow.bowwowMapPatches(rom)` | bowwow | - | - | - | - |  | table | options |
| `patches.desert.desertAccess(rom)` | - | - | - | - | - |  | table | fixed |
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
| `patches.aesthetics.reduceMessageLengths(rom, rnd)` | - | - | rnd | - | - | `rnd.choice` of a line of `patches/marin.txt` for text 0x01; its `<rnd>`/`<ord>` placeholders go through `utils.randomNumber`, which ignores its `rnd` and draws from the global `random` (seeded from `self.rnd`'s state in `Randomizer.__init__`). Reads `rom.texts[0xEA]` and others to copy them. | C | marin |
| `hints.addHints(rom, rnd, logic.iteminfo_list)` | - | - | rnd | placement | - | `rnd.shuffle`, `rnd.randint`, `rnd.choice`; reads `spot.item` and `spot.metadata.area` of the spots holding hint items; writes `rom.texts`. | C | hints |
| `patches.aesthetics.allowColorDungeonSpritesEverywhere(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.aesthetics.allowOverworldBackgroundTileTransitions(rom)` | overworld | - | - | - | - | Alternative overworlds are unsupported (tier1.txt). | unused | overworld |
| `patches.music.randomizeMusic(rom, rnd)` | music | - | rnd | - | - | `rnd.choice` over the looping tracks for every music slot. | C | music |
| `patches.music.noMusic(rom)` | music | - | - | - | - |  | table | music-fixed |
| `patches.music.shiftedMusic(rom)` | music | - | - | - | - |  | table | music-fixed |
| `patches.music.importMusic(rom, rnd, 'ffa')` | music | - | rnd | - | - | music=ffa is unsupported (tier1.txt); reads `music/ffa` and draws from `rnd`. | unused | music-ffa |
| `patches.aesthetics.removeFlashingLights(rom)` | noflash | - | - | - | - |  | table | options |
| `patches.hardMode.oracleMode(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.hardMode.heroMode(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.hardMode.oneHitKO(rom)` | hardmode | - | - | - | - |  | table | options |
| `patches.weapons.patchSuperWeapons(rom)` | superweapons | - | - | - | - |  | table | options |
| `patches.aesthetics.fastText(rom)` | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.fastText(rom)` #2 | textmode | - | - | - | - |  | table | options |
| `patches.aesthetics.noText(rom)` | textmode | - | - | - | - |  | table | options |
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
| `patches.bingo.setBingoGoal(rom, world_setup.bingo_goals, world_setup.goal)` | - | - | - | world_setup | - | `bingo_goals` drawn with `rnd` by `bingo.randomizeGoals` in `WorldSetup.randomize`; writes texts and rooms. | C | goal |
| `patches.maze.patchMaze(rom, world_setup.sign_maze[0], world_setup.sign_maze[1])` | - | - | - | world_setup | - | `sign_maze` drawn by `maze.buildMaze(rnd)` in `WorldSetup.randomize`; edits rooms. | C | goal |
| `patches.goal.setSeashellGoal(rom, 20)` | - | - | - | world_setup | - | Fixed for the goal setting; writes text 0x1A3 for the count. | table | goal-fixed |
| `patches.goal.setSpecificInstruments(rom, [int(c) for c in world_setup.goal[1:]])` | - | - | - | world_setup | - | goal=specific: the instruments are `rnd.shuffle`d (and the count drawn when goalcount=random). | C | goal |
| `patches.goal.setRequiredInstrumentCount(rom, -1)` | - | - | - | world_setup | - | Fixed for the goal setting; writes text 0x1A3 for the count. | table | goal-fixed |
| `patches.goal.setRequiredInstrumentCount(rom, world_setup.goal_count)` | - | - | - | world_setup | - | goal_count is drawn with `rnd` for goalcount=random or a goal range, else the setting. | C | goal |
| `patches.chest.setMultiChest(rom, world_setup.multichest)` | - | - | - | world_setup | - | `multichest` is always drawn with `rnd.choices` in `WorldSetup.randomize`. | C | items |
| `patches.entrances.changeEntrances(rom, world_setup.entrance_mapping)` | overworld | - | - | world_setup | - | entranceshuffle is unsupported, so the mapping should be the identity; Phase 2 checks whether this writes anything then. Side provisional (draft `?`), to settle from the real-data run (`SEED-VARIES`, `STABLE`). | C | entrances |
| `spot.item = spot.item[1:]` | - | - | - | placement | - | Strips the `*` marker of a forced item; writes nothing to the ROM. | none | locals |
| `spot.patch(rom, spot.item)` | - | - | - | placement | - | The placement: each location writes its item. | C | items |
| `patches.enemies.changeBosses(rom, world_setup.boss_mapping)` | - | - | - | world_setup | - | Mappings drawn with `rnd` in `WorldSetup.randomize` when boss/miniboss shuffle is on, else identity; reads bank 0x14, 0x3E bytes and rooms through `RoomEditor`. | C | bosses |
| `patches.enemies.changeMiniBosses(rom, world_setup.miniboss_mapping)` | - | - | - | world_setup | - | Mappings drawn with `rnd` in `WorldSetup.randomize` when boss/miniboss shuffle is on, else identity; reads bank 0x14, 0x3E bytes and rooms through `RoomEditor`. | C | bosses |
| `patches.enemies.randomizeEnemies(rom, seed)` | enemies | - | seed | - | - | Draws from a private `random.Random(seed)`, not the global state; reads every overworld room through `RoomEditor`. | C | enemizer |
| `patches.core.addFrameCounter(rom, len([spot for spot in item_list if type(spot) != locations.keyLocation.KeyLocation]))` | - | romdebugmode | - | placement | - | Count of the non-key spots: a function of the settings, not of the seed, to be confirmed; reads banks 0x38 sprite data. Side provisional (draft `?`), to settle from the real-data run (`SEED-VARIES`, `STABLE`). | C | frame-counter |
| `patches.core.warpHome(rom, settings.overworld == "dungeonchain" or settings.entranceshuffle in ("chaos", "insane", "madness"))  # Needs to be done after setting the start location.` | entranceshuffle, overworld | - | - | - | - | entranceshuffle and dungeonchain are unsupported: the flag is always False. | table | options |
| `patches.titleScreen.setRomInfo(rom, binascii.hexlify(seed).decode("ascii").upper(), settings)` | * | - | seed | - | - | Writes the seed hex (or `Race` plus its name) to the title screen and an md5-derived pattern of it. | C | title |
| `patches.endscreen.updateEndScreen(rom)` | - | - | global | - | - | `random.choice(cats)` from the global `random` (cat list sorted by `ladxr.patch`); copies the chosen `patches/cats/*.bin` into bank 0x3F. | C | endscreen |
| `patches.aesthetics.updateSpriteData(rom)` | - | - | - | - | - | Reads the entities of every room (changed by the placement, bosses and enemizer) and rewrites the room sprite data tables. | C | sprites |
| `patches.enemies.doubleTrouble(rom)` | - | doubletrouble | - | - | - | `args.doubletrouble` only. | unused | debug |
| `pymod.postPatch(rom)` | - | - | - | - | - | `args.pymod` only; the oracle passes none. | unused | pymod |

## randomizer.py Randomizer.__init__

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `rom = generator.generateRom(args, settings, self.seed, self.__logic, rnd=self.rnd)` | * | input_filename, * | rnd, seed | logic | - | The section `generator.py generateRom`. | none | generate |
| `filename = args.output_filename` | - | input_filename, output_filename | - | - | - | The output file name; not ROM content. | none | output |
| `filename = "LADXR_%s.gbc" % (binascii.hexlify(self.seed).decode("ascii").upper())` | - | input_filename | seed | - | - | The output file name; not ROM content. | none | output |
| `rom.save(filename, name="LADXR")` | - | input_filename | - | - | - | The sections `romTables.py ROMWithTables.save`, `rom.py ROM.save` and `rom.py ROM.fixHeader`. | C | save |
| `log = spoilerLog.SpoilerLog(settings, args, rom)` | race, * | log_directory, spoilerformat, * | - | - | - | Builds the spoiler log, reading the finished ROM back (`readHexSeed`, each location's `read`, rooms); skipped for race seeds. | C | spoiler |
| `filename = "LADXR_%s_%s.json" % (datetime.now().strftime("%Y-%m-%d_%H-%M-%S"), log.seed)` | race | log_directory, spoilerformat | seed | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log_path = os.path.join(args.log_directory, filename)` | race | log_directory, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log.outputJson(log_path)` | race | log_directory, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |
| `log.output(args.spoiler_filename)` | race | log_directory, spoiler_filename, spoilerformat | - | - | - | The spoiler log, outside the ROM; skipped for race seeds. | none | output |

## romTables.py ROMWithTables.__init__

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `super().__init__(filestream)` | - | - | - | - | - | Reads the 64 banks. | none | parse |
| `self.texts = Texts(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.entities = Entities(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_overworld_top = RoomsOverworldTop(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_overworld_bottom = RoomsOverworldBottom(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_indoor_a = RoomsIndoorA(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_indoor_b = RoomsIndoorB(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.rooms_color_dungeon = RoomsColorDungeon(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.room_sprite_data_overworld = OverworldRoomSpriteData(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.room_sprite_data_indoor = IndoorRoomSpriteData(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.background_tiles = BackgroundTilesTable(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |
| `self.background_attributes = BackgroundAttributeTable(self)` | - | - | - | - | - | Parses one table from the ROM; re-serialized by `ROMWithTables.save`. | none | parse |

## romTables.py ROMWithTables.save

| Call | Settings | Args | RNG (grep) | Reads (grep) | Reads (trace) | Notes | Side | Group |
|---|---|---|---|---|---|---|---|---|
| `self.texts.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.entities.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_overworld_top.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_overworld_bottom.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_indoor_a.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_indoor_b.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.rooms_color_dungeon.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `leftover_storage = self.room_sprite_data_overworld.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.addStorage(leftover_storage)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.patch(0x00, 0x0DFA, ASM("ld hl, $763B"), ASM("ld hl, $%04x" % (leftover_storage[0]["start"] \| 0x4000)))` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.adjustDataStart(leftover_storage[0]["start"])` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.room_sprite_data_indoor.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.background_tiles.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
| `self.background_attributes.store(self)` | - | - | - | - | - | Re-serializes the tables edited by the patches. | C | save |
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
| `self.banks[0][0x134:0x143] = name` | - | - | - | - | - | The title in the header (`LADXR`). | C | header |
| `checksum = 0` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `checksum -= c + 1` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14D] = checksum & 0xFF` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14E] = 0` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14F] = 0` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `checksum = 0` #2 | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `checksum = (checksum + sum(bank)) & 0xFFFF` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14E] = checksum >> 8` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |
| `self.banks[0][0x14F] = checksum & 0xFF` | - | - | - | - | - | Header and global checksums over the final bytes. | C | header |

## Groups

| Group | Side | Settings key | Depends on | Evidence |
|---|---|---|---|---|
| asm-consts | none | dungeon_beaks, dungeon_keys, dungeon_maps, hardmode, nightmare_keys, overworld, textmode | - | - |
| bank3e | C | - | - | bank3e |
| bosses | C | - | - | bosses |
| debug | unused | - | - | - |
| endscreen | C | - | - | endscreen |
| enemizer | C | enemies | - | enemizer |
| entrances | C | overworld | - | entrances |
| fixed | table | - | - | - |
| frame-counter | C | - | - | frame-counter |
| generate | none | - | - | - |
| gfxmod | unused | gfxmod | - | - |
| goal | C | - | - | goal |
| goal-fixed | table | goal | - | - |
| header | C | - | - | header |
| hints | C | - | - | hints |
| items | C | - | - | items |
| locals | none | - | - | - |
| marin | C | - | - | marin |
| music | C | music | - | music |
| music-ffa | unused | music | - | - |
| music-fixed | table | music | - | - |
| options | table | bowwow, dungeon_beaks, dungeon_keys, entranceshuffle, evilshop, follower, hardmode, hpmode, linkspalette, lowhpbeep, nagmessages, nightmare_keys, noflash, overworld, owlstatues, quickswap, rooster, shopsanity, steal, superweapons, textmode, tradequest, witch | - | - |
| output | none | race | - | - |
| overworld | unused | overworld | - | - |
| parse | none | - | - | - |
| pymod | unused | - | - | - |
| save | C | - | - | save |
| shopsanity | unused | shopsanity | - | - |
| spoiler | C | race | - | spoiler |
| sprites | C | - | - | sprites |
| title | C | - | - | title |

## Read coverage

| Group | Row | Target | Range | Kind | Reason |
|---|---|---|---|---|---|

## Extractions

| Name | Group | Holds | Derived from | Format |
|---|---|---|---|---|

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
