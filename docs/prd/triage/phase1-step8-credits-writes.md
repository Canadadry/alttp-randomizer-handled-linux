---
title: "Port the credits writes"
description: "The C ROM writer writes nothing for VT's writeCredits, so the two credits writes of every golden rom_writes.txt are unproven and no case can claim past its settings."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 8, copied from `docs/plan-1.md`)

> **Port the credits writes** (`Credits`). Verified by every case's claim reaching that group, plus `test_rom_credits.c`.

## Facts at `232e981` (checked)

- **The target.** In `tests/rando/write_groups.txt`, the `credits` group owns the tag `writeCredits`. Every one of the 73 goldens (50 `cases/`, 23 `combos/`) has exactly two `writeCredits` lines, right after the last settings line (`enableFastRom`) and before `writeText`:
  1. `181500`: the credits data, 892 to 918 bytes across the goldens;
  2. `076CC0`: the 17 scene pointers, 34 bytes.
- **VT's call** (`tmp/vt`, `app/Rom.php` `writeCredits`): `$data = $this->credits->getBinaryData()`, then `write(0x181500, pack('C*', ...$data['data']))`, then `write(0x76CC0, pack('S*', ...$data['pointers']))`. `S` is the machine order, little-endian on the oracle's host.
- **How the buffer is filled.** `Rom::setCredit($key, $string)` calls `Credits::updateCreditLine($key, 0, $string)`: always line 0 of the scene, always centred. Two sources call it, both before `writeCredits`:
  - **`World::writeToRom`'s loop over `$this->credits`**, before the item loop. `World::$credits` starts empty. Only `Randomizer::randomizeCredits` fills it: `castle`, `sanctuary`, `kakariko` (`"<name>'s homecoming"`), `lumberjacks`, `smithy` (`"the dwarven breadsmiths"`, only when the coin draws 1), `bridge`, `woods`, `well`. The alternatives are listed in `app/Randomizer.php` in the order `RandoCredits`' indices follow.
  - **Six `writeItem` overrides**, called in the item loop on every location, empty ones included (VT sets them to `Nothing` first). Each calls `parent::writeItem`, then `setCredit(<scene>, $this->getItemCreditsText())`:

    | Location (C id) | VT class | Scene | Default text (`Nothing` and anything unlisted) |
    |---|---|---|---|
    | King Zora (`RANDO_LOC_KING_ZORA`) | `Npc\Zora` | `zora` | `Nothing, so stupid` |
    | Potion Shop (`RANDO_LOC_POTION_SHOP`) | `Npc\Witch` | `witch` | `mushrooms go poof` |
    | Master Sword Pedestal (`RANDO_LOC_MASTER_SWORD_PEDESTAL`) | `Pedestal` | `pedestal` | `and nothing` |
    | Sick Kid (`RANDO_LOC_SICK_KID`) | `Npc\BugCatchingKid` | `kakariko2` | `nothing-having kid` |
    | Flute Spot (`RANDO_LOC_FLUTE_SPOT`) | `Dig\HauntedGrove` | `grove` | `empty boy does nothing again` |
    | Link's Uncle (`RANDO_LOC_LINKS_UNCLE`) | `Npc\Uncle` | `house` | `your uncle does nothing` |

  - The two sets of scenes don't overlap, so the call order changes no byte. Scenes neither source touches keep their default text: `desert`, `hera`, `smithy` without the coin, and every `large` and `small_alt` line.
- **`getItemCreditsText`** (each override class, `app/Location/...`). All six read `$this->item->getTarget()`, the location's own item with an alias resolved to its target. They never read the item `Location::writeItem` substituted: its vanilla-key and generic-key substitutions only change a local. The rules:
  - first a `switch` on the target's class: `Item\Key` and `Item\BigKey` share one text, then `Item\Map`, `Item\Compass` and `Item\Egg` each have their own;
  - then a `switch` on the target's raw name;
  - then the default;
  - `Pedestal` puts a raw-name check for `BigKeyA2` (`la key of evils bane`) before the class switch;
  - PHP's `switch` takes the first matching `case`.
  - `Item::all` holds no `Egg` item, so the `Egg` branch can't be reached.
  - The alternate bow `setTexts` swaps in (`new Item\Bow('ProgressiveBow', [0x65])`) has class `Bow` and raw name `ProgressiveBow`, the same as the world's plain `ProgressiveBow`. The C world keeps the plain one (step 1.4 note), so the credits come out the same.
  - `Pedestal::setItem` stores a `MasterSword` as `L2Sword`, and the C world does the same (`rando_world_set_item`).
  - Every text is 32 bytes or less (31 and 32 at most), and is printable ASCII with only `' , . -` as punctuation. One text starts with `'` (`'splosion boy explodes again`).
- **`Credits` itself** (`app/Support/Credits.php`):
  - **16 scenes, in this order:** castle, sanctuary, kakariko, desert, hera, house, zora, witch, lumberjacks, grove, well, smithy, kakariko2, bridge, woods, pedestal.
  - **Each scene's records:** a `small` line 0 at y 19 and a `large` line at y 23, with their default text and x. Pedestal adds a `small_alt` (`sleeps again...`, x 8, y 21) between the two.
  - **`updateCreditLine`:**
    - it cuts the text to 32 bytes;
    - it sets x to `max(0, floor((32 - strlen(text)) / 2))`, the length counted on the original, mixed-case text;
    - an unknown scene or line returns false, which no caller reaches.
  - **`getBinaryData`:**
    - per scene, it pushes `count(data)` as that scene's pointer, then appends each record;
    - after the last scene it pushes one final pointer: 17 pointers in all.
  - **Header:** 4 bytes, big-endian `pack('N', (0x6000 | (y>>5<<11) | ((y&0x1F)<<5) | (x>>5<<10) | (x&0x1F)) << 16 | (len*2 - 1))`.
  - **`small` record:**
    - the header, then `convertCredits(text)` (lower-cased bytes: `a`–`z` give `c - 0x47`, `,` `34`, `'` `35`, `-` `36`, `.` `37`, space and anything else `9F`);
    - then, if the text has an apostrophe: the text with every non-`'` byte turned into a space and every `'` into `,`, trimmed, under a header at `x + strpos(<untrimmed>, ',')`, `y - 1`;
    - then, if it has a comma: the same, with `,` turned into `'`, at `y + 1`.
  - **`small_alt`:** `a`–`z` give `c - 0x29`, `.` `52`, anything else `9F`.
  - **`large`:** two lines from the same text.
    - Top line, at `y`: `a`–`z` give `c - 4`; `'` `D9`, `!` `E5`, `_` `DE`, anything else `9F`.
    - Bottom line, at `y + 1`: `a`–`z` give `c + 0x22`; `'` `EC`, `!` `F8`, `_` `F1`, anything else `9F`.
  - **Bound:** a scene is at most 108 bytes of `small`, 36 of `small_alt` and 72 of `large`. So the data is at most 2,916 bytes.
- **`rom_buffers.txt`:**
  - The goldens log each `setCredit` call as `credit <scene> 0 center <hex text>`, in call order, mixed with the `text` lines.
  - Every golden has 13 lines, or 14 when it has the coin's `smithy` line (34 cases with it, 39 without). That is the world's seven or eight, then the six overrides.
  - Across the goldens, each override scene gets 14 to 18 distinct texts. So most entries of the six override tables (about 400 `return`s) are reached by no golden.
  - No harness reads `rom_buffers.txt`. The oracle uses it for its own self-check, and a human or agent uses it to debug.
- **The C side:**
  - `RandoGen.post.credits` (`RandoCredits`, `rando/include/rando_postfill.h`) holds `randomizeCredits`' seven indices and the `smithy_breadsmiths` coin. `RANDO_CREDITS_*` gives each pick's alternative count. No credit string exists in `rando/`.
  - `rando_rom_write`'s credits slot is the empty static `write_credits` in the `rando_rom` source (`TODO step 8`).
  - Groups write through `rando_rom_emit` (private `rom_emit.h`).
  - The six override locations are told apart by `RandoLocationClass` (`NPC_ZORA`, `NPC_WITCH`, `PEDESTAL`, `NPC_BUG_CATCHING_KID`, `DIG_HAUNTED_GROVE`, `NPC_UNCLE`), each held by exactly one location.
  - `RandoItemInfo` gives each item its raw name, its class and an alias's target.
  - The oracle fixtures include the precedent of a table read from VT: `oracle/hint_tables.txt`. `hint_tables.php` writes it, `gen_golden.sh` runs it, and `test_postfill --fixtures` replays it in `make logic`.

## Step 7's outcome (hypotheses)

Step 7 (`docs/prd/triage/phase1-step7-settings-flags-writes.md`) is not done. Every decision below that depends on what it leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it holds. A hypothesis that does not hold is handled by the rule given with it.

- **H-7-1**: step 7 is finished: every line of `claims.txt` (50) and `combo_claims.txt` (23) is `<case> all settings`, and `make check` is green. *If a case is still below `all settings`*, this step cannot claim `all credits` for it. It stops and reports that case rather than claim around it, since the step text requires every case.
- **H-7-2**: the settings group lives in its own private source, called from `rando_rom_write`'s settings slot through `rom_emit.h`. The credits slot is still the empty static `write_credits` stub. *If step 7 moved or renamed the stub*, the credits group takes whatever slot calls `writeCredits`' place. Either way, the credits group gets its own source, declared in `rom_emit.h` the way the items group is.
- **H-7-3**: `test_rom`'s baseline expectation reads "the items writes, then settings writes ending with `enableFastRom`, all inside 2 MiB, accepted by the buffer writer". This step rewrites it on purpose (step 7's PRD says step 8 does).
- **H-7-4**: `test_rom_settings` has its own recording writer, local to that file, and `support.*` has none. *If step 7 moved a recorder into `support.*`*, `test_rom_credits` reuses it. *Otherwise* it has its own copy.
- **H-7-5**: step 7 leaves `RandoCredits`, `rando_randomize_credits` and `RANDO_CREDITS_*` as they are at `232e981`. *If it changed them*, this step reads what is there and does not duplicate it.
- **H-7-6**: the settings group asserts preconditions a hand-built world may lack (for example, every prize-pack drop assigned). This hypothesis needs no check: it only explains a choice. `test_rom_credits` therefore starts from a generated world and edits it, rather than building one from nothing (Testing Decisions).
- **H-7-7**: the C world still leaves an empty location empty, where VT has set it to `Nothing` (H-6-4 carried over). The credits group reads an empty override location as `Nothing`.
- **H-7-8**: step 7 adds no text, credits or SRAM buffer to `RandoGen`, as its PRD's Out of Scope says. *If it added a credits buffer*, this step uses it as the single source for the scene texts, and does not keep a second one.
- **H-7-9**: `write_groups.txt`, `run_e2e.sh`'s claim semantics, the goldens, the existing fixtures, `gen_golden.sh` and the oracle are unchanged by step 7.

## Problem Statement

The owner will play ROMs the console patches. At the end of a seed, the ending credits show what alttpr.com shows:

- the joke scene titles the seed drew (castle, sanctuary, Sahasrahla's homecoming, lumberjacks, smithy, bridge, woods, well);
- six titles that name the item the seed put at King Zora, the Potion Shop, the Pedestal, the Sick Kid, the Flute Spot and Link's Uncle.

Today the C ROM writer writes no credits. A patched ROM would end on the base patch's credits data, whatever the seed. The net can't catch it either: every claim stops at `all settings`.

## Solution

- The `rando_rom` credits group writes, through the caller's writer, VT's two `writeCredits` writes, byte for byte, with the tag `writeCredits`:
  - `181500`: the data;
  - `076CC0`: the 17 pointers.
- It reads the generator as const and draws nothing. The scene texts come from three places: VT's defaults, `post.credits` (`randomizeCredits`' picks), and the items at the six override locations (`getItemCreditsText`).
- The encoder is a direct port of `Credits`: centring, the three record types, the apostrophe and comma records, and the headers.
- `test_rom_credits` proves the encoding and each selection rule on edited generators. In `make logic`, it also replays a new oracle fixture, read from VT, that encodes every override scene for every VT item and every `randomizeCredits` alternative. It proves the ~400 table texts no golden reaches.
- Every claim in `claims.txt` and `combo_claims.txt` becomes `<case> all credits`. `e2e` and `e2e-combos` then compare all 73 logs through the pointer write, byte for byte against VT.

## User Stories

1. As the owner, I want the ending credits to show the scene titles my seed drew, so that the ending is the alttpr.com one.
2. As the owner, I want Sahasrahla's homecoming scene to show the drawn name with its apostrophe drawn above the line, so that it reads as VT shows it.
3. As the owner, I want the smithy scene to say "the dwarven breadsmiths" only when the seed's coin said so, so that the joke appears as often as on alttpr.com.
4. As the owner, I want the wishing-well scene's punctuation (commas, apostrophes, full stops) drawn in place, so that "Venus, I'm your fire" isn't garbled.
5. As the owner, I want the Zora, witch, pedestal, sick-kid, grove and uncle scenes to name the item the seed put there, so that the credits tell me what I found.
6. As the owner, I want a location left empty to show its "nothing" credit, so that the scene reads as VT's.
7. As the owner of a dungeon-item seed, I want a key, big key, map or compass at an override location to show that kind's text, whatever the dungeon, so that it matches VT's class switch.
8. As the owner of a retro seed, I want a shop key or another alias to show its target's text, so that the alias is invisible in the credits as in VT.
9. As the owner of a seed with Ganon's Tower's big key on the pedestal, I want "la key of evils bane", so that VT's special case shows.
10. As the owner, I want the scenes no rule touches (desert, Hera, the large titles, the pedestal's "sleeps again...") written with VT's default text, so that nothing differs.
11. As the owner, I want each title centred the way VT centres it, so that no line is shifted.
12. As the app (Phase 2), I want the credits writes to land inside the 2 MiB ROM through my buffer writer, so that patching never corrupts memory.
13. As the app, I want the writer to stop at the first refused credits write and return false, so that a failed patch is reported and not half-applied.
14. As a porting agent, I want the credits group to read the generator only as const and never touch the PRNG, so that writing a ROM can never change the seed.
15. As a porting agent, I want the credit strings kept inside the ROM writer, beside the encoder, so that the generator keeps storing only indices.
16. As a porting agent, I want the credits group in its own source, with its text tables apart from its encoder, so that steps 9 to 11 never touch it and the logic stays readable.
17. As a porting agent, I want one encoder used for every scene, defaults included, so that the default bytes follow from VT's text and not from a copied blob.
18. As a porting agent, I want the six override tables extracted mechanically from VT's source, so that no text is mistyped.
19. As the net, I want a fixture read from VT that encodes every override scene for every item and every `randomizeCredits` alternative, so that table entries no golden reaches are still checked against VT.
20. As the net, I want that fixture added in its own commit, with every existing golden and fixture byte-identical, so that the source of truth only grows on purpose.
21. As the net, I want every case and combination to claim `all credits`, so that `make check` fails on any wrong credits byte, pointer or order.
22. As the net, I want the trace, attempts, spoiler, items and settings writes still compared before the credits writes, so that a credits divergence is never reported on a seed that already differs.
23. As the net, I want a unit test per encoding rule and per selection rule, so that the dev loop catches a wrong rule in milliseconds without a golden.
24. As the net, I want the refusal rule tested on both credits writes, so that the contract holds across the group's two writes.
25. As the net, I want `test_rom`'s baseline expectation rewritten on purpose, with the reason stated, so that no check is weakened silently.
26. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new tables obey the library rules.
27. As the net, I want `make test` to stay fast, so that the dev loop stays seconds long.
28. As `check-e2e`, I want a credits divergence reported with the first differing write, and the golden's `credit` buffer lines as the expected scene texts, so that `fix-bug` starts from the exact scene.
29. As a reviewer, I want the step note to record which H-7 hypotheses held, the claims moved, how the tables were extracted and any VT quirk the goldens settled, so that I can check the step without rerunning it.

## Implementation Decisions

### Where the credits group lives

- A new private source in `rando/src/` holds the credits group: the scene list, the encoder and the selection.
  - `rom_emit.h` declares its entry point, and `rando_rom_write`'s credits slot calls it, after the settings group (H-7-2).
  - It writes through `rando_rom_emit`, so a refused write ends the whole `rando_rom_write`.
- A second private source holds the group's text tables, behind a private header:
  - `randomizeCredits`' seven alternative lists;
  - the six `getItemCreditsText` tables, one text per `RandoItemId`, plus each table's default and its class texts.
- No public API changes. `rando_rom.h`'s contract already covers the group.
- It reads `gen->world` (the six override locations' items) and `gen->post.credits`, both const, and never `gen->prng`.

### Scenes and texts

- The 16 scenes are a static table in VT's order. Each scene has its records (type, x, y, default text), with pedestal's `small_alt` between its two lines.
- Line 0 (the `small` record) of each scene starts at its default. Then:
  - `castle`, `sanctuary`, `lumberjacks`, `bridge`, `woods` and `well` take the alternative at their `post.credits` index;
  - `kakariko` takes the indexed name followed by `'s homecoming`;
  - `smithy` takes `the dwarven breadsmiths` only when `smithy_breadsmiths` is set;
  - each override scene takes its location's text.
- A text set this way is centred as `updateCreditLine` does. It is cut to 32 bytes, and x is computed on the original case. A default text keeps its own x.
- Each alternative list has exactly `RANDO_CREDITS_<pick>` entries. The table's size is that macro, and a test reads the last index of each pick. An index at or above the count is asserted.

### Override selection (`getItemCreditsText`)

- The location of each override scene is the one location of its `RandoLocationClass`, read with `rando_world_item`. An empty location reads as `Nothing` (H-7-7).
- The item is resolved through `alias_target` when its class is `ALIAS`. That is VT's `getTarget()`. The substitutions of `Location::writeItem` (vanilla and generic keys) don't apply.
- The order is VT's:
  - `Pedestal` only: a target of `BigKeyA2` gives `la key of evils bane`;
  - a target of class `KEY` or `BIG_KEY` gives the scene's key text, `MAP` its map text, `COMPASS` its compass text;
  - otherwise the per-item table entry;
  - an item with no entry gets the scene's default.
- `Egg` has no C class or item and is left out. A comment states that `Item::all` has no `Egg`.
- The per-item tables give each item VT's first matching `case`. Duplicate `case` labels later in a PHP `switch` are dead, and the extraction drops them.

### Encoding

- The data is put together in a fixed, function-local buffer of 2,916 bytes (the bound in the facts, as a named constant), with an assert that it never overflows. The 17 pointers go in a 34-byte buffer, little-endian. Then the group makes two `rando_rom_emit` calls, `181500` then `076CC0`, each tagged `writeCredits`.
- The header, the `small`, `small_alt` and `large` converters and the apostrophe and comma records port `Credits` literally. That covers lower-casing before conversion, `strpos` taken on the untrimmed mask, and `trim` of spaces.
- An empty text, which no path makes, is asserted against: VT would pack a negative length.

### How the tables are made

- The tables come from VT's source at `219fcafd`, never from a golden:
  - the six `getItemCreditsText` switches;
  - `randomizeCredits`' lists;
  - the `Credits` scene defaults.
- A throwaway script in `tmp/` may extract them mechanically (parsing the PHP `case`/`return` lines). The step note records how. No golden line is copied into `rando/` or into a test expectation.
- The new fixture (below) then checks every table entry against VT.

### The fixture

- **The script:** a new oracle script, `credit_tables.php` (in the style of `hint_tables.php`), writes `oracle/credit_tables.txt`. `gen_golden.sh` runs it next to `hint_tables.php`, with and without `--out`.
- **Its records**, each line's hex being that scene's segment of `getBinaryData()['data']` (the bytes from the scene's pointer to the next) on a fresh `Credits`:
  - `default <scene> <hex>`: the 16 scenes, untouched.
  - `pick <scene> <index> <hex>`: after `updateCreditLine(<scene>, 0, <text>)` for each `randomizeCredits` alternative, in VT's list order. `kakariko`'s text is `"<name>'s homecoming"`, and `smithy` has the one index 0 (the breadsmiths).
  - `item <scene> <raw name> <hex>`: for each of the six override scenes and each item of `Item::all` in its order, aliases included. The script places the item on a world's location of the scene's class with VT's `setItem` (so `Pedestal`'s `MasterSword` → `L2Sword` applies), calls the class's private `getItemCreditsText` (through a bound closure), then `updateCreditLine`. If VT's `setItem` throws, the item gets no line.
- The fixture lands in its own commit (`test:`). The commit message records php, Composer and the VT commit. Its proof is a `gen_golden.sh --out tmp/<dir>` run on every case, whose files are byte-identical to every committed golden and fixture, the new one excepted. Two runs give an identical new fixture.
- The fixture is seed-independent, so `make logic` replays it (`test_rom_credits --fixtures`). `test_rom_credits` therefore joins `LOGIC_TESTS`, unlike `test_rom_items`.

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all credits` (H-7-1). Nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle, apart from the new fixture.
- A case that is `DIVERGENT` at `all credits` is a bug in this step. `check-e2e`'s loop drives it to green before the step ends, each fix behind its own failing unit test first.
- To debug, the golden's last `credit <scene>` line per scene in `rom_buffers.txt` gives the expected text. The first differing pointer in the `076CC0` write tells which scene's segment differs.

### Not done here

- No `measure` run: step 8's text sets no budget.
- `e2e-arm` is not run (step 12 and the end of Phase 2 run it). If it runs anyway, the result goes into the note.

### Step note

The plan's §6 gains a "Step 1.8" note with:

- which H-7 hypotheses held, and how any that did not was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all credits`);
- how the tables were extracted, and the fixture's size, record counts and versions;
- any VT quirk the goldens or the fixture settled. Expected: the override texts read the unsubstituted item; the `Egg` branch can't be reached; dead duplicate `case` labels; `Pedestal`'s `BigKeyA2` before the class switch;
- for step 9: `Pedestal::writeItem` also calls `setText('mastersword_pedestal_translated', …)`, which this step does not port.

### No `guideline` agent

None was consulted. Every point was settled from the plan, VT at `219fcafd` (`tmp/vt`), the code and the goldens at `232e981`, step 7's PRD and steps 1.1 to 1.6's notes.

## Implementation Chunks

1. **The fixture.**
   - `credit_tables.php`, its line in `gen_golden.sh`, and the generated `oracle/credit_tables.txt`, in their own commit.
   - The `--out` byte-identity proof against every existing golden and fixture, and two identical runs.
   - No C change; `make check` green.
2. **The encoder, the scenes and `randomizeCredits`' picks.**
   - The credits source wired into the credits slot, with the scene table, the encoder, the seven alternative lists and the coin.
   - The override scenes keep their default text for now (the default scene text, not the item's).
   - `test_rom_credits` registered in `UNIT_TESTS` (and so in `LOGIC_TESTS`). It covers the two writes, the tags, the pointers, the encoding rules, each pick (first and last index), the coin, the refusal rule and const. Its `--fixtures` mode replays the `default` and `pick` records.
   - `test_rom`'s expectation rewritten (H-7-3).
   - `make check` green (no claim moves yet).
3. **The six override tables.**
   - The tables source and the selection: the location by class, empty read as `Nothing`, alias target, `BigKeyA2`, the class texts, the per-item entry, the default.
   - `test_rom_credits` cases for each rule, and its `--fixtures` mode replaying every `item` record.
   - `make check` green.
4. **Claims and the note.**
   - Every claim of both files moves to `all credits`. `e2e` and `e2e-combos` are driven to green, each divergence fixed behind its own unit test.
   - The §6 "Step 1.8" note.

## Testing Decisions

- **What a good test is here.**
  - It starts from one `rando_generate` of the baseline options (seed 1), made once per test binary. It then edits a copy of the generator through public APIs only:
    - `rando_world_set_item` on the six override locations, `RANDO_ITEM_NONE` for an empty one;
    - the fields of `post.credits`.
  - It calls `rando_rom_write` with a recording writer and checks the recorded `writeCredits` calls: address, bytes, count, order. A generated world keeps the settings group's preconditions (H-7-6).
  - It never inspects a table or a static function. The only golden-like input it reads is the fixture, in `--fixtures` mode.
  - Expected bytes come from VT's source at `219fcafd`, computed by hand in the test, never from the C tables.
  - It finds a scene's segment by slicing the data write with the pointer write, in VT's scene order (hard-coded in the test from `Credits.php`).
  - Prior art: `test_rom` and `test_rom_items` for the recording writer and the baseline generation; `test_postfill --fixtures` for replaying `hint_tables.txt`.
- **`test_rom_credits`** (new, in `UNIT_TESTS`, kept in `LOGIC_TESTS`). Cases:
  - **Shape**: exactly two `writeCredits` calls, after every settings write and before any later group's tag. The first is at `181500`, the second at `076CC0` with 34 bytes. Pointer 0 is 0, the pointers never go down, and the last pointer equals the data length.
  - **Header and centring**:
    - a 14-byte text (`the two towers`) at x 9, y 19 gives the header `62 69 00 1B`;
    - an odd length rounds x down;
    - a 32-byte text has x 0;
    - `Your ad here` is centred on its own length and encoded lower-case.
  - **`small` conversion**: letters, space, `,` `'` `-` `.`, and an unknown byte giving `9F`.
  - **Apostrophe record**: `kakariko` with a name gives the extra `,` record at y 18 and x + the apostrophe's index. A text starting with `'` (`'splosion boy explodes again` at the grove) gives it at x + 0.
  - **Comma record**: `Venus, I'm your fire` gives both the y-18 and the y-20 records. `Yeah, baby, she's got it` gives a comma record whose spaces between the two commas are `9F`.
  - **Defaults**: `desert`'s segment, with its `small` and `large` records; the `large` top and bottom conversions; pedestal's `small_alt` (`sleeps again...`, `.` as `52`); `smithy` unchanged without the coin.
  - **Picks**: index 0 and the last index of each of the seven picks. `smithy_breadsmiths` set changes only the smithy segment.
  - **Overrides**, one location at a time:
    - an empty location and `Nothing` give the same default;
    - a raw-name entry (`Hammer` at the Potion Shop);
    - a `Key`-class and a `BigKey`-class item give the scene's key text, a map its map text, a compass its compass text;
    - `UncleSword` at Link's Uncle gives `ProgressiveSword`'s text, and `ShopKey` gives the key text;
    - `BigKeyA2` at the Pedestal gives `la key of evils bane`, while another big key gives the class text;
    - `MasterSword` placed on the Pedestal gives `L2Sword`'s text;
    - under `rom.genericKeys`, a dungeon key still gives the key text by its own class (no substitution).
  - **Isolation**: changing one override location changes only its scene's segment and the later pointers.
  - **The refusal rule**:
    - a writer refusing the `181500` write gets no `076CC0` call, and `rando_rom_write` returns false;
    - a writer refusing the `076CC0` write gets no later call and the same false.
  - **Const**: `rando_rom_write` leaves the generator's bytes unchanged.
  - **`--fixtures`**: every `default`, `pick` and `item` record of `credit_tables.txt`.
    - Each `item` raw name is resolved with `rando_item_find`. A name C doesn't know fails the test, unless the step note lists it as unreachable with its reason.
    - The record's segment is compared with the C segment of the matching scene.
- **`test_rom`**: its baseline expectation is rewritten on purpose, because the credits group now writes (H-7-3). The new expectation:
  - on a completed baseline, every call lies inside 2 MiB;
  - the calls are the items writes, then the settings writes, then the two `writeCredits` writes, in that order;
  - the buffer writer accepts the whole run into a 2 MiB buffer.

  Step 9 rewrites it again.
- **`e2e` and `e2e-combos`**: the step's proof is all 73 claims at `all credits`, green.
- **`lint`** builds the new sources with `-std=c99 -pedantic-errors`.
- **Speed:** `make test` stays fast: one new binary, with one baseline generation and a few dozen `rando_rom_write` calls. The fixture replay (about 1,100 `rando_rom_write` calls on an edited baseline) runs only in `make logic`.

## Out of Scope

- The later write groups: text, dialog and hints (step 9), including `Pedestal::writeItem`'s `setText`; initial SRAM and `setTotalItemCount` (step 10); checksum and heart beep (step 11).
- `randomizeCredits`' draws, which step 4 already ported, and any change to `RandoCredits`' shape (H-7-5).
- Credits for `multiworld`, which skips the item loop and so the overrides.
- Any change to the goldens, the existing fixtures, `stages.txt`, `write_groups.txt` or the claim semantics. The one new fixture is the only oracle change.
- A harness check of `rom_buffers.txt`'s `credit` lines: the write log already proves their effect byte for byte.
- Running `check-e2e` over the whole case list and `e2e-arm` (step 12).

## Further Notes

- **Why a fixture, when the goldens cover the credits?** The goldens reach 14 to 18 texts per override scene, out of about 70 each. The rest of the ~400 strings would be proven only by a unit test written from the same extraction, which proves nothing about a mistyped string or a wrong first `case`. Step 1.4 set the precedent of a table read from VT (`hint_tables.txt`), in its own commit, replayed in `make logic`.
- **Why scene segments in the fixture, not texts?** Segments let the test go through the public `rando_rom_write`, so no private accessor of the C text is needed. They also check the encoder on every text, not only the centring and conversion cases the unit tests pick.
- **Why encode the defaults instead of storing their bytes?** One encoder for every record keeps the bytes a consequence of VT's text. The `default` fixture records then prove the encoder on `large` and `small_alt`, which no setter ever changes.
- **Why text tables in the writer, not in `postfill`?** The generator needs only the counts (`RANDO_CREDITS_*`) to draw, and it keeps indices. The strings exist only to be encoded, as step 6 keeps item bytes beside the items writer.
- **Where a divergence is likely.**
  - The apostrophe and comma records' x (an untrimmed `strpos`).
  - The `well` alternatives' punctuation.
  - An override reading the substituted item instead of its own.
  - An alias not resolved to its target.
  - The goldens of `state-retro` (shop keys, generic keys), the `dungeon_items` cases (keys and maps at override locations) and the coin cases settle these in chunk 4.
