---
title: "Port the text, dialog and hints writes"
description: "The C ROM writer writes nothing for VT's writeText, so the 15 KB text table write of every golden rom_writes.txt is unproven and no case can claim past its credits."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 9, copied from `docs/plan-1.md`)

> **Port the text, dialog and hints writes** (`Text`, `Dialog`), in two halves: first the encoder and default table, verified by `test_rom_text.c` against the default-table fixture; then the seed-dependent strings, verified by the logged setter calls, then by every case's claim reaching that group.

The plan's step 1.7 note adds: "`setInvertedMode`'s `text->setString` calls belong to the text group (step 9)". Step 8's PRD adds: "`Pedestal::writeItem` also calls `setText('mastersword_pedestal_translated', …)`, which this step does not port". Both are ported here.

## Facts at `0295613` (checked)

### The target

- In `tests/rando/write_groups.txt`, the `text` group owns the tag `writeText`.
- Every one of the 73 goldens (50 `cases/`, 23 `combos/`) has exactly one `writeText` line: address `0E0000`, 14,977 to 15,449 bytes. It comes right after the two `writeCredits` lines and before `writeInitialSram`.
- **VT's call** (`tmp/vt`, `app/Rom.php` `writeText`): `write(0xE0000, pack('C*', ...$this->text->getByteArray()))`.
- **`Text::getByteArray`** concatenates every entry of the table, in the table's order. It throws `Too BIG` past `0x7355` bytes (29,525). The `$pad` argument is false here.
- **The table's order** is fixed by `Text::en()`'s array literal: 408 ids, no duplicate key.
  - `setString` on an existing id replaces the value and keeps the position.
  - `setString` on an unknown id throws.
  - `setStringRaw` has no caller on this path.
- **The default table** is `en()` after `removeUnwanted()`. The `Rom` constructor runs both.
  - 8 entries are raw byte arrays (`set_cursor`, `set_cursor2`, the five `choice_*`, and the last id, `terminator`, `FF FF`).
  - Every other entry is `convertDialogCompressed(<string>)`, 9 of them with `pause` false. No entry passes `max_bytes` or `wrap`.
  - Some strings span several concatenated source lines. 93 source lines hold non-ASCII characters (`≥`, `…`, `→`, `←`, kana).
  - `removeUnwanted()` then sets about 80 listed ids to `convertDialogCompressed("{NOTEXT}", false)`.
- **The default-table fixture** `tests/rando/oracle/text_default.txt` (written by `text_default.php`): one line `<id> <HEX>` per id, in table order, 408 lines, 15,336 bytes in all. No harness reads it yet.

### The encoder (`app/Support/Dialog.php`, `convertDialogCompressed($string, $pause = true, $max_bytes = 2046, $wrap = 19)`)

- **Start:** the output starts with `FB`. The string is split on `\n`.
- **Wrapping:** each line goes through `mb_wordwrap($line, 19, "\n")` (`app/Helpers/str.php`), and the result is split again. `mb_wordwrap` takes one of two paths:
  - **Single-byte line** (`mb_strlen === strlen`): PHP's own `wordwrap` with `cut` false. It breaks at a space; a word longer than 19 stays whole. It does not trim.
  - **Multibyte line:** each line is `rtrim`med first. If it is still over 19 characters, the words (split on `' '`) are re-packed greedily: a line ends with `rtrim`, the last one with `trim`.
- **`$line_count`:** the number of lines, minus one when the last line's first byte is `{`.
- **Each line:**
  - `mb_substr($line, 0, 19)` cuts it to 19 characters. An over-long word is truncated here.
  - It is split into UTF-8 characters with `preg_split('//u')`. An empty line gives an empty array, not `false`, and is still processed.
- **A line starting with `{` is a command:**
  - `switch (trim($line))` appends the command's bytes (`{SPEED0}`, `{PAUSE3}`, `{CHOICE}`, `{NOBORDER}`, `{INTRO}`, `{IBOX}`, `{CHANGEPIC}` and the rest).
  - `{INTRO}` also sets `pad_out`.
  - `{NOTEXT}` returns `FB FE 6E 00 FE 6B 04` at once.
  - An unknown command appends nothing.
  - Every command line decrements `$line_count` and does not advance `$i`.
- **A text line:**
  - First a row marker: none for line 0, `F8` for line 1, then `F6` (scroll) when `3 <= i < count($lines)`, otherwise `F9`. That bound is the wrapped line count, not `$line_count`.
  - With `pad_out` and `i < 3`, the line is padded to 19 characters with spaces.
  - Then each character's bytes, and `i++`.
  - Then `FA` when `$pause && i % 3 == 0 && $line_count > i`.
- **The end:** the output is cut to `$max_bytes` (2,046), with no terminator added.
- **`charToHex`:**
  - a digit gives `A0 + d`; `A`–`Z` give `c - 65 + AA`; `a`–`z` give `c + 6F`;
  - then a 230-entry character table: space `FF`, `?` `C6`, `'` and `’` `9D`, `@` `FE 6A`, `>` `9B 9C`, `↑ ↓ → ←` `FD 13`–`FD 16`, the `ᚋ`–`ᚎ` variables `FE 6C 0n`, kana, and the rest;
  - anything else gives `FF`.
- **`convertDialog`** (the uncompressed variant) has no caller on the `writeToRom` path.

### Who sets a text before `writeText`

Every setter below runs before `writeText`, and none of them reads the ROM. The final table depends only on each id's last value. The four sources set disjoint ids, so their order changes no byte.

1. **`World::writeToRom`'s loop over `$this->texts`**, before the credits loop and the item loop. It calls `Rom::setText($key, $value)` with no flags, so `pause` is true. `World::setText` stores into a keyed array: a repeated key keeps its first position and takes the last value. Two methods fill that array.
   - **`Randomizer::setTexts`** (`app/Randomizer.php`):
     - `uncle_leaving_text`: an uncle alternative. `spoil.BootsLocation` is always off here (step 1.4).
     - `sahasrahla_bring_courage`: the text names PendantOfCourage's region.
     - `bomb_shop`: the text names Crystal5's then Crystal6's region.
     - `blind_by_the_light`, `kakariko_tavern_fisherman`, `ganon_fall_in`: alternatives.
     - `ganon_phase_3_alt`: a fixed text.
     - The bow branch: `ganon_phase_3_no_silvers` and `ganon_phase_3_no_silvers_alt`. A region text says `My tower?` when the region is `Ganons Tower`. On the fake branch both take the fake-silvers alternative, or `Chat said no\nto Silvers.\nIt's over Hero` under `crowd_control`.
     - `sign_ganons_tower` when `crystals.tower < 7` (true for the string `'0'`). It says `%d crystal` when the count is 1, `%d crystals` otherwise.
     - `sign_ganon`, `ganon_fall_in_alt` and, under `triforce-hunt`, `murahdahla`: per goal, with the `crystals.ganon` or `item.Goal.Required` counts. `completionist` falls through to the default.
     - `end_triforce`: `"{NOBORDER}\n"` followed by a triforce alternative.
     - Regions are named by `getRegion()->getName()`, VT's `Region::$name`. The C `RandoRegionInfo.spoiler_name` holds the same (`Dark Palace`, `Ganons Tower`).
   - **`HintService::applyHints`:**
     - **Hints off:** `sign_north_of_links_house` = `"Randomizer v31\n\n>    -veetorp"` (only `hints-off`).
     - **Hints on:** all 15 `telepathic_tile_*` ids. A location hint is `ucfirst("$item_name $location_name")`, built from `hint.item.<target raw name>` (`KeyGK` for a key under `rom.genericKeys`) and `hint.location.<location name>` (`resources/lang/en/hint.php`).
     - A key `__()` cannot find comes back as the key itself: `hint.item.<raw>`, `hint.location.<name>`.
     - A joke hint is an alternative of `strings/hint.txt`.
     - A pair pick never hints (step 1.4 note), so `LocationCollection::getHint`'s multi-item text can't be reached.
2. **`Rom::setInvertedMode`** (the settings group, `state` inverted only) calls `$this->text->setString` directly for 12 ids:
   - `sign_path_to_death_mountain`, `sign_bumper_cave`, `sign_east_of_bomb_shop`, `sign_east_of_links_house`, `dark_sanctuary_no`;
   - `kiki_leaving_screen`, `dark_sanctuary` and `dark_sanctuary_yes`, each `{NOTEXT}` with `pause` false;
   - `menu_start_2`, `menu_start_3` and `intro_main`, with `pause` false.
   - `setMysteryMasking(false)` sets nothing.
3. **`writeItem` overrides**, called in the item loop on every location (empty ones first set to `Nothing`):
   - `Drop\Ether` sets `tablet_ether_book`, `Drop\Bombos` sets `tablet_bombos_book` (`getItemText`), and `Pedestal` sets `mastersword_pedestal_translated` (`getItemPedestalText`). All three use `setText` with no flags.
   - The three switches read `$this->item`, replaced by `KeyGK` under `rom.genericKeys` when the item is a `Key`, then `getTarget()->getRawName()`. Ether's and Bombos' functions have identical source, and each has 120 `case` labels. The pedestal's has about 120.
   - The default text is `Don't waste\nyour time!`.
   - `Pedestal::setItem` stores a `MasterSword` as `L2Sword`.
   - The alternate bow `setTexts` swaps in has the raw name `ProgressiveBow`.
4. Nothing else on the path calls `setText`, `setString` or `setStringRaw`. `CustomizerController` is out of scope.

### String files

- `strings/<file>.txt` is split by `getTextArray`: `\r\n` becomes `\n`, a leading `-\n` is removed, the text is exploded on `\n-\n`, and empty pieces are filtered out.
- Every file ends with `\n-\n`, so no alternative keeps a trailing newline.
- Counts (step 1.4, `RANDO_STRINGS_*`): uncle 55, tavern_man 51, blind 77, ganon_1 31, ganon_phase_3_no_silvers 36, triforce 39, hint 37.
- `ganon_1`, `hint`, `tavern_man`, `triforce` and `uncle` hold non-ASCII characters.

### `rom_buffers.txt`

- The oracle (`oracle_rom.php` `OracleText`) logs every `setString` as `text <id> <HEX>` (the id's encoded bytes after the call), in call order, mixed with the `credit` and `sram` lines.
- Across the 73 goldens:
  - 15 ids appear in all 73 (the setTexts ids plus the tablets and the pedestal);
  - the 15 tiles appear in 72;
  - the 12 inverted ids in 8;
  - `sign_ganons_tower` in 8;
  - `murahdahla` in 4;
  - `sign_north_of_links_house` in 1.
- No harness reads these lines. The oracle's self-check proves they rebuild the `writeText` write.

### The C side

- `RandoGen.post` (`rando/include/rando_postfill.h`) holds every choice as indices:
  - `RandoTexts`: the string indices, the bow branch with `bow_locations[2]` in pop order, `silvers_location` and `fake_silvers`;
  - `RandoHints`: `on`, and per tile `NONE`/`LOCATION`/`JOKE`, a `RandoLocationHint` with the substituted item and both name indices, or a joke index.
- `rando_hint_item_alternatives` and `rando_hint_location_alternatives` (`hints.c`, replayed against `oracle/hint_tables.txt`) hold only counts. `RANDO_HINT_MISSING` marks a missing key.
- No text string, no encoder and no text id exist in `rando/`.
- `rando_rom_write`'s text slot is the empty static `write_text` in the `rando_rom` source (`TODO step 9`). Groups write through `rando_rom_emit` (private `rom_emit.h`).
- The tablets and the pedestal are each the one location of `RANDO_LOCATION_CLASS_DROP_ETHER`, `DROP_BOMBOS` and `PEDESTAL`.
- **Unit tests:** they include only `rando/include` and `rando/host`. They read goldens from `cases/<name>/` (`test_generate`), list case directories with `opendir` (`test_case_reader`), and replay oracle fixtures in `--fixtures` mode under `make logic` (`test_postfill`).
- **The fixture pattern:** `credit_tables.php` (`0295613`) boots VT, places each `Item::all` item (179) on the one location of a class, calls a private text function through a bound closure, and skips an item whose `setItem` throws. `gen_golden.sh` runs it after `hint_tables.php`.

## Step 8's outcome (hypotheses)

Step 8 (`docs/prd/triage/phase1-step8-credits-writes.md`) is not done: at `0295613` only its chunk 1, the `credit_tables.txt` fixture, is committed. Every decision below that depends on what step 8 leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it held. A hypothesis that doesn't hold is handled by the rule given with it.

- **H-8-1**: step 8 is finished: every line of `claims.txt` (50) and `combo_claims.txt` (23) is `<case> all credits`, and `make check` is green.
  - *If a case is still below `all credits`*, this step can't claim `all text` for it. It stops and reports that case instead of claiming around it, since the step text requires every case.
  - Chunks 1 to 5 need no credits claim and may go ahead.
- **H-8-2**: the credits group lives in its own private source, declared in `rom_emit.h` and called from `rando_rom_write`'s credits slot. The text slot is still the empty static `write_text` (`TODO step 9`).
  - *If step 8 moved or renamed the stub*, the text group takes whatever slot comes between `writeCredits` and `writeInitialSram`.
  - Either way, the text group gets its own source, declared in `rom_emit.h`.
- **H-8-3**: `test_rom`'s baseline expectation reads "items, then settings, then the two `writeCredits` writes", and step 8's PRD says step 9 rewrites it. This step rewrites it on purpose.
- **H-8-4**: `test_rom_credits` has its own recording writer, local to that file, and `support.*` has none.
  - *If a recorder was moved into `support.*`*, `test_rom_text` reuses it.
  - *Otherwise* it has its own copy.
- **H-8-5**: step 8 ported none of `Pedestal::writeItem`'s `setText`, and added no text string, text id or text buffer to `rando/` or `RandoGen`, as its PRD says.
  - *If it added any of these*, this step uses what is there as the single source, and does not keep a second one.
- **H-8-6**: step 8 left `RandoTexts`, `RandoHints`, `RandoLocationHint`, the `RANDO_STRINGS_*` counts and the hint count tables as they are at `0295613`.
  - *If it changed them*, this step reads what is there.
- **H-8-7**: `test_rom_credits` is in `UNIT_TESTS` and kept in `LOGIC_TESTS`, with a `--fixtures` mode replaying `credit_tables.txt`. This is the precedent `test_rom_text` follows.
- **H-8-8**: the C world still leaves an empty location empty, where VT has set it to `Nothing` (H-7-7, carried over). The tablet and pedestal texts read an empty location as `Nothing`.
- **H-8-9**: `write_groups.txt`, `run_e2e.sh`'s claim semantics, the goldens, the existing fixtures, `gen_golden.sh` (apart from step 8's committed `credit_tables.php` line) and the oracle are unchanged by the rest of step 8.
- **H-8-10**: if step 8 added a private helper that finds "the one location of a `RandoLocationClass`", or that resolves an alias target for a writer table, the text group reuses it. Otherwise it writes its own, in the same style.

## Problem Statement

The owner will play ROMs the console patches. Everything the game says has to be what alttpr.com writes for that seed:

- the uncle's parting line;
- Sahasrahla's and the bomb shop's region clues;
- the blind maiden, the tavern man and Ganon's lines;
- the silver-arrow clue;
- the Ganon's Tower and Ganon signs for the seed's crystal counts and goal;
- the Triforce ending;
- Murahdahla under triforce hunt;
- the fifteen telepathic-tile hints;
- the Ether and Bombos tablets' and the pedestal's item clues;
- under inverted, the swapped signs, menus and intro.

Today the C ROM writer writes no text. A patched ROM would keep the base patch's text table, whatever the seed. The net can't catch it either: every claim stops at `all credits`. Without a proven encoder, a single mis-wrapped line would garble a dialog box the owner can only discover in game.

## Solution

- **The encoder.** A pure C port of `Dialog::convertDialogCompressed`, with VT's `mb_wordwrap` (both of its paths) and the full character table. It turns a UTF-8 string into VT's bytes.
- **The default table.** The 408 ids in VT's order, each with its default bytes as `Text::en()` then `removeUnwanted()` leave them. The default strings are kept as source strings and encoded by the same encoder, never as a copied blob.
- **The text group.** `rando_rom`'s text group writes, through the caller's writer, VT's one `writeText` write, byte for byte, at `0E0000` with the tag `writeText`.
  - It reads the generator as const and draws nothing.
  - Each id's text is either the seed's (the setTexts strings, the hints, the inverted strings, the tablet and pedestal clues) or the default.
- **First half.** The encoder and the default table, proven by `test_rom_text` against `text_default.txt` (all 408 ids). The text group then writes the default table.
- **Second half.** The seed-dependent strings, proven:
  - by a new oracle fixture read from VT, holding every string-file alternative, every hint name and every tablet and pedestal text, encoded;
  - then by every golden's logged `text` setter lines in `rom_buffers.txt`, compared id by id;
  - then by every claim in `claims.txt` and `combo_claims.txt` becoming `<case> all text`, so `e2e` and `e2e-combos` compare all 73 logs through the `writeText` write, byte for byte against VT.

## User Stories

1. As the owner, I want the uncle's parting line to be the one my seed drew, so that the opening matches alttpr.com.
2. As the owner, I want Sahasrahla to name the region holding the green pendant, so that his clue is true for my seed.
3. As the owner, I want the bomb shop to name the regions of crystals 5 and 6, so that I know where to go for the big bomb.
4. As the owner, I want the blind maiden, the tavern man and Ganon's opening line to be my seed's draws, so that the jokes match VT's.
5. As the owner, I want Ganon's silver-arrow line to name the region of my seed's bow or silvers ("My tower?" in Ganon's Tower), so that the clue is right.
6. As the owner of a seed with no silvers to hint, I want the fake-silvers joke, or "Chat said no to Silvers" under crowd control, so that it reads as on alttpr.com.
7. As the owner, I want the Ganon's Tower sign to give my seed's crystal requirement, singular for one crystal, so that the sign isn't wrong.
8. As the owner, I want Ganon's sign and his "you think you are ready" line to match my goal (Ganon, fast Ganon, Ganon hunt, pedestal, triforce hunt, dungeons, completionist), so that the game tells me what to do.
9. As the owner of a triforce-hunt seed, I want Murahdahla to ask for my seed's piece count, so that I know how many to collect.
10. As the owner, I want the Triforce ending text to be my seed's draw, so that the ending is VT's.
11. As the owner with hints on, I want each telepathic tile to give my seed's hint (item and location, or a joke) word for word, so that the hints are usable.
12. As the owner with hints off, I want the house sign to say "Randomizer v31", so that the ROM matches VT's hints-off output.
13. As the owner, I want the Ether and Bombos tablets and the pedestal to describe the item my seed put there, so that reading them is a real clue.
14. As the owner of a generic-keys seed, I want a key on a tablet or the pedestal to read as a generic key, so that it matches VT's substitution.
15. As the owner of an inverted seed, I want the swapped signs, the Dark Chapel menus and the inverted intro, so that the inverted world reads right.
16. As the owner, I want every other text to be VT's default, the removed texts included, so that nothing else differs from alttpr.com.
17. As the owner, I want every dialog wrapped, paused, scrolled and cut exactly as VT does, so that no box overflows or breaks mid-word differently.
18. As the owner, I want the arrows, cursor, hearts, Link's name and kana in texts encoded with VT's bytes, so that special characters show correctly.
19. As the app (Phase 2), I want the text write to land inside the 2 MiB ROM through my buffer writer, so that patching never corrupts memory.
20. As the app, I want the writer to stop at the refused text write and return false, so that a failed patch is reported and not half-applied.
21. As a porting agent, I want the text group to read the generator only as const and never touch the PRNG, so that writing a ROM can never change the seed.
22. As a porting agent, I want the encoder as a small public function (a UTF-8 string and a pause flag in, bytes out), so that it is tested directly against VT's rules.
23. As a porting agent, I want the 408 text ids as a public enum in VT's table order with their names, so that tests and fixtures address ids by name.
24. As a porting agent, I want the default strings extracted mechanically from VT's `Text::en()`, so that no default is mistyped.
25. As a porting agent, I want the string-file alternatives, hint names and tablet and pedestal tables extracted mechanically from VT, so that ~1,000 strings aren't typed by hand.
26. As a porting agent, I want the hint-name strings to be the single source the hint draws' alternative counts come from, so that strings and counts can never disagree.
27. As a porting agent, I want the text group in its own source, its tables apart from its encoder, so that steps 10 and 11 never touch it.
28. As a porting agent, I want non-ASCII characters in the C tables written as escapes, so that every cross compiler reads the same bytes.
29. As the net, I want `test_rom_text --fixtures` to compare all 408 default entries with `text_default.txt`, so that the encoder is proven on every default string, multibyte ones included.
30. As the net, I want a fixture read from VT that encodes every string-file alternative and every tablet and pedestal entry, and lists every hint name, so that strings no golden reaches are still checked against VT.
31. As the net, I want that fixture added in its own commit, with every existing golden and fixture byte-identical, so that the source of truth only grows on purpose.
32. As the net, I want every golden's `text` setter lines compared id by id with the C's seed texts, so that a wrong text is reported by its id and not as one 15 KB write.
33. As the net, I want every case and combination to claim `all text`, so that `make check` fails on any wrong text byte or order.
34. As the net, I want the trace, attempts, spoiler, items, settings and credits writes still compared before the text write, so that a text divergence is never reported on a seed that already differs.
35. As the net, I want a unit test per encoder rule (rows, scroll, pause, commands, NOTEXT, INTRO padding, both wrap paths, truncation to 19 characters, the 2,046-byte cut, the character table), so that the dev loop catches a wrong rule in milliseconds.
36. As the net, I want a unit test per selection rule (each setTexts branch, each goal, the crystal plural, hints off, location and joke hints, missing hint keys, `ucfirst`, generic keys, alias targets, empty locations, inverted), so that branches no golden reaches are still checked.
37. As the net, I want the refusal rule and the const rule tested on the text write, so that the writer contract holds.
38. As the net, I want `test_rom`'s baseline expectation rewritten on purpose, with the reason stated, so that no check is weakened silently.
39. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new tables obey the library rules.
40. As the net, I want `make test` to stay fast, with the fixture and golden replays only in `make logic`, so that the dev loop stays seconds long.
41. As `check-e2e` and `fix-bug`, I want a text divergence to show up first as a `make logic` failure naming the case, the id and both encodings, so that a fix starts from the exact string.
42. As a reviewer, I want the step note to record which H-8 hypotheses held, the claims moved, how the tables were extracted, the fixture's size and versions, and any VT quirk the goldens settled, so that I can check the step without rerunning it.

## Implementation Decisions

### Modules

- **The text module** has a new public header, `rando_text.h`. It is public because the plan verifies the encoder and the default table directly, and tests include only public headers. The interface is small and stable (shapes, not code):
  - `RandoTextId`: the 408 ids in VT's table order, named `RANDO_TEXT_<vt id>` verbatim (as `RANDO_ITEM_<raw name>` keeps VT's names), then `RANDO_TEXT_COUNT` and a `RANDO_TEXT_NONE` sentinel.
  - The id's VT name, and the reverse lookup (`RANDO_TEXT_NONE` when unknown).
  - **The encoder:** a UTF-8 string and a `pause` flag in; the bytes out into a caller buffer of `RANDO_TEXT_ENTRY_MAX` (2,046); the count returned, never 0. It is VT's `convertDialogCompressed` with its default `max_bytes` and `wrap`, the only ones this path uses. It asserts that the input is valid UTF-8 (VT's `preg_split` would silently drop an invalid line, and no VT string has one).
  - **The default:** an id's default bytes into the same kind of buffer.
  - **Second half:** the id's bytes in a generated seed's table, with a flag telling whether a seed-dependent setter set it. The generator is read as const.
  - `RANDO_TEXT_TABLE_MAX` (`0x7355`), `getByteArray`'s bound.
- **Private sources in `rando/src/`:**
  - the encoder (wrapping, line layout, commands, character table);
  - the generated default table: per id, either a source string with its `pause` flag, or raw bytes. `removeUnwanted`'s ids are folded in as `{NOTEXT}` with `pause` false, a comment naming VT's method;
  - the seed selection: setTexts, hints, inverted, the writeItem overrides;
  - the string tables behind a private header: the seven string files, the hint item and location names, and the tablet and pedestal tables;
  - the text group, declared in `rom_emit.h` and called from `rando_rom_write`'s text slot (H-8-2).
- No other public API changes. `rando_rom.h`'s contract already covers the group.

### The encoder

- It ports `convertDialogCompressed` line by line, keeping each trap in the facts:
  - wrapping happens per source line;
  - `$line_count` drops one when the last line starts with `{`;
  - a command line advances no row;
  - `{NOTEXT}` returns at once;
  - the scroll test uses the wrapped line count;
  - `{INTRO}` padding applies to the first three text rows only;
  - the `FA` pause rule;
  - each line is cut to 19 characters after wrapping;
  - an empty line still gets its row marker;
  - the output is cut to 2,046 bytes.
- **Wrapping takes two paths**, chosen per line as `mb_wordwrap` does:
  - a single-byte line ports PHP 8.3's `wordwrap` (single-character break, no cut) literally, with no trimming;
  - a multibyte line ports `mb_wordwrap`'s own loop, `rtrim`, `trim` and character counts included.
- **Characters** are counted as UTF-8 code points.
- **`charToHex`**: digits, `A`–`Z` and `a`–`z` by arithmetic first, then the character table, then `FF`. The table is extracted mechanically from `Dialog::$characters` and keyed by code point.
- The encoder works in a caller buffer. Every append is bounds-checked against `RANDO_TEXT_ENTRY_MAX`, and anything past the cut is dropped, as `array_slice` does.
  - VT's "command overflowed byte length" exception can't be reached by any string on this path. It is asserted, not reproduced.

### The default table

- It is extracted from VT at `219fcafd` by a throwaway script in `tmp/`, never from a golden or from the fixture. The script boots VT and records `en()`'s entries:
  - a `Text` subclass whose converter records each `convertDialogCompressed` call's string and flags, in call order;
  - the raw arrays as they are;
  - `removeUnwanted`'s list applied.
- 408 ids have no duplicate key, so call order maps to id order. The script checks that the recorded count is 408.
- The step note records how the table was extracted.
- Non-ASCII bytes are written as `\x` escapes. A literal is split after an escape when the next character is a hex digit.

### The text group

- It assembles the table into a function-local buffer of `RANDO_TEXT_TABLE_MAX` bytes: for each id in order, its seed bytes or its default bytes. It then makes one `rando_rom_emit` call, `0E0000`, tagged `writeText`.
- The table can't pass `0x7355` on this path: the goldens peak at 15,449 bytes, and the seed texts are short. VT's `Too BIG` is asserted, not reproduced.
- **The stack frame** is about 30 KB. The library keeps no mutable static state, so the buffer is not static. The host and Onion's Linux give a thread 8 MiB of stack. `measure`'s DS-era 8 KiB stack figure is informational (step 1.1 note), and the measure driver doesn't call `rando_rom_write`. The step note records the frame size.
- **First half:** every id writes its default bytes.
- **Second half:** the seed's text where one is set, the default otherwise.

### Seed texts (second half)

- **Single source, no order.** Each seed-dependent id's final string is computed from `gen` alone. The four sources set disjoint ids, so call order changes no byte (facts). A comment states that VT's `World::$texts` keeps a key's first position, which is why table order alone decides the bytes.
- **setTexts:**
  - the five alternatives are taken by their `post.texts` index;
  - the regions come from `rando_layout_location_region` and `RandoRegionInfo.spoiler_name`;
  - `PendantOfCourage`, `Crystal5` and `Crystal6` are each found as the first location holding the item, in `getLocations()` order (as `getLocationsWithItem()->first()`);
  - the bow branch follows `post.texts.bow_hint`: on `ALTERNATE`, `bow_locations[0]` gives `ganon_phase_3_no_silvers` and `[1]` gives `_alt` (VT's pop order); the `Ganons Tower` region gives `My tower?`;
  - `crowd_control` overrides the fake-silvers alternative;
  - `sign_ganons_tower` is set when `crystals_tower < 7`;
  - the goal switch is ported with `completionist`'s fall-through, and counts come from `RandoConfig` with `%d`.
- **Hints:**
  - **Hints off:** the `sign_north_of_links_house` text.
  - **On, a `LOCATION` tile:** `ucfirst` (ASCII `a`–`z` only) of the item name, a space, then the location name.
    - Each name is the indexed alternative, or the plain string, from `hint.php`.
    - A missing key gives the literal `hint.item.<raw name>` or `hint.location.<location name>`.
    - The item is `RandoLocationHint.item`, already substituted (`KeyGK`, alias target).
  - **A `JOKE` tile:** the indexed `hint.txt` alternative.
  - **A `NONE` tile:** asserted absent when hints are on.
- **Inverted:** the 12 `setInvertedMode` strings with their flags, when `mode_state` is inverted. The text group decides this from the config; it doesn't call into the settings group.
- **writeItem overrides:**
  - the location of each override is the one location of its class (H-8-10), and an empty location reads as `Nothing` (H-8-8);
  - a key under `rom_generic_keys` reads as `KeyGK`, then any alias as its target;
  - then the per-item table entry, else the default;
  - the tables give each item VT's first matching `case`. Later duplicate labels are dead, and the extraction drops them;
  - Ether and Bombos have identical source, but each gets its own table, both checked by the fixture. Sharing them would hide a future VT divergence, and VT is frozen anyway.
- **The string tables** are extracted from VT by a throwaway script in `tmp/`:
  - the seven string files through VT's own `getTextArray`;
  - the hint names from `hint.php`;
  - the three switches by evaluating each private function on every `Item::all` item, the way `credit_tables.php` does.
- **Hint name counts.** The hint-name tables become the source of `rando_hint_item_alternatives` and `rando_hint_location_alternatives`: the counts are derived from the string tables, and the old count tables go away. `test_postfill --fixtures` must stay green on `hint_tables.txt`. This is a refactor with the same results. The `RANDO_STRINGS_*` macros stay, and each string array is sized by its macro with a check that every slot is filled.

### The fixture (second half, first chunk)

- **The script:** a new oracle script, `text_tables.php` (in the style of `credit_tables.php`), writes `oracle/text_tables.txt`. `gen_golden.sh` runs it after `credit_tables.php`, with and without `--out`, and its header comment names it.
- **Its records:**
  - `pick <text id> <file> <index> <HEX>`: the id's bytes after `setString(<id>, <alternative>)` on a fresh table, for each alternative of each string file, in `getTextArray` order:
    - `uncle` → `uncle_leaving_text`, `blind` → `blind_by_the_light`, `tavern_man` → `kakariko_tavern_fisherman`, `ganon_1` → `ganon_fall_in`, `ganon_phase_3_no_silvers` → `ganon_phase_3_no_silvers`;
    - `triforce` → `end_triforce` with the `"{NOBORDER}\n"` prefix;
    - `hint` → `telepathic_tile_eastern_palace`.
  - `item <text id> <raw name> <HEX>`: for each of `tablet_ether_book`, `tablet_bombos_book` and `mastersword_pedestal_translated`, and each `Item::all` item in its order:
    - the item is placed on the one location of `Drop\Ether`, `Drop\Bombos` or `Pedestal` with VT's `setItem`, with `rom.genericKeys` off;
    - the class's private text function is called through a bound closure, then `setString`;
    - an item whose `setItem` throws gets no line.
  - `hint_item <raw name> <index> <HEX of the UTF-8 name>` and `hint_location <index> <HEX of the UTF-8 name> <location name>`, the location name last because it holds spaces: one line per alternative of `hint.php`, index 0 for a plain string, no line for an absent key. These are raw strings, not encodings: a hint's encoding depends on both names, and the goldens and encoder tests cover that.
- **Its commit:** it lands in its own commit (`test:`). The commit message records php, Composer and the VT commit.
  - **The proof** is a `gen_golden.sh --out tmp/<dir>` run over every case: every committed golden and fixture is byte-identical, the new file excepted. Two runs give an identical new fixture.
  - No golden line is copied into `rando/` or into a test expectation.

### The golden comparison

- `test_rom_text --fixtures` also walks every case directory of `cases/` and `combos/`. For each one it:
  - reads the case (`rando_case_read`) and generates it;
  - takes the golden `rom_buffers.txt`'s `text` lines, the last line per id;
  - checks that the set of ids equals the set the C marks as seed-set;
  - checks that each id's bytes are equal.
- A failure names the case, the id, and both encodings.
- **Cost:** about 73 generations, so it runs in `make logic` only, never in `make test`.
- **Chunk 4** compares only the ids chunk 4 ports, listed in the test. Chunk 5 replaces the list with every id and the set equality. The final form compares everything; the list is a build order, not a weakened check, and is gone when the step ends.

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all text` (H-8-1).
- Nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle, apart from the new fixture and its `gen_golden.sh` line.
- A case that is `DIVERGENT` at `all text` is a bug in this step. `check-e2e`'s loop drives it to green before the step ends, each fix behind its own failing unit test first. The golden comparison in `make logic` runs before `e2e` in `make check`, so it reports the failing id first.

### Not done here

- No `measure` run: step 9's text sets no budget.
- `e2e-arm` is not run (step 12 and the end of Phase 2 run it). If it runs anyway, the result goes into the note.
- No change to `check-e2e`: the per-id failure already comes out of `make logic`.

### Step note

The plan's §6 gains a "Step 1.9" note with:

- which H-8 hypotheses held, and how any that didn't was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all text`);
- how the default table and the string tables were extracted, and the fixture's size, record counts and versions;
- the text group's stack frame size;
- any VT quirk the goldens or the fixture settled. Expected:
  - PHP's `wordwrap` versus `mb_wordwrap` per line;
  - the empty-line row marker;
  - the 19-character cut after wrapping;
  - the scroll bound using the wrapped count;
  - the missing hint keys printed literally;
  - `'0' < 7` setting `sign_ganons_tower`;
  - the dead duplicate `case` labels.

### No `guideline` agent

None was consulted. Every point was settled from:

- the plan;
- VT at `219fcafd` (`tmp/vt`);
- the code, the goldens and the fixtures at `0295613`;
- step 8's PRD;
- steps 1.1 to 1.7's notes.

## Implementation Chunks

**First half: the encoder and the default table.**

1. **The encoder.**
   - `rando_text.h` with the encoder only (and `RANDO_TEXT_ENTRY_MAX`), and the encoder source with its character table.
   - `test_rom_text` registered in `UNIT_TESTS`, with one case per encoder rule (Testing Decisions).
   - `make check` green.
2. **The default table and the text group.**
   - The `RandoTextId` enum, the names and lookup, the generated default table, and the default function.
   - The text group wired into the text slot, writing the default table as one `writeText` write.
   - `test_rom_text` joins `LOGIC_TESTS`: `--fixtures` replays all 408 lines of `text_default.txt`, and the group's shape, refusal and const cases are added.
   - `test_rom`'s expectation rewritten (H-8-3).
   - `make check` green, no claim moved.

**Second half: the seed-dependent strings.**

3. **The `text_tables.txt` fixture.**
   - `text_tables.php`, its `gen_golden.sh` line, and the generated fixture, in their own commit.
   - The `--out` byte-identity proof, and two identical runs.
   - No C change; `make check` green.
4. **setTexts, hints off and inverted.**
   - The string tables for the seven files, and the seed function.
   - The setTexts ids, `sign_north_of_links_house`, and the 12 inverted ids.
   - Unit cases per branch.
   - `--fixtures` replays the `pick` records, and compares those ids against every golden's `text` lines.
   - `make check` green.
5. **Hints and the writeItem overrides.**
   - The hint name tables, with the counts derived from them (`test_postfill --fixtures` still green).
   - The tile texts, and the tablet and pedestal tables and selection.
   - Unit cases per rule.
   - `--fixtures` replays the `hint_item`, `hint_location` and `item` records, and the golden comparison now covers every id with set equality.
   - `make check` green.
6. **Claims and the note.**
   - Every claim of both files moves to `all text`. `e2e` and `e2e-combos` are driven to green, each divergence fixed behind its own unit test.
   - The §6 "Step 1.9" note.

## Testing Decisions

- **What a good test is here.**
  - It goes through the public interface only: the encoder, the default function, the seed function, and `rando_rom_write` with a recording writer. It never inspects a table or a static function.
  - Expected bytes come from VT's rules at `219fcafd`, computed by hand in the test, never from the C tables. Golden-like input is read only in `--fixtures` mode: `text_default.txt`, `text_tables.txt`, and the goldens' `rom_buffers.txt`.
  - Seed cases start from one `rando_generate` of the baseline options (seed 1), made once per binary. They then edit a copy through public APIs:
    - `post.texts` and `post.hints` fields;
    - `rando_world_set_item` on the tablets, the pedestal and the prize locations;
    - config fields (goal, crystals, item pool, generic keys, state).
  - Prior art:
    - `test_rom`, `test_rom_items` and `test_rom_credits` for the recording writer and the edited baseline;
    - `test_postfill --fixtures` and `test_rom_credits --fixtures` for replaying a fixture;
    - `test_generate` and `test_case_reader` for reading case directories.
- **`test_rom_text`** (new, in `UNIT_TESTS`, kept in `LOGIC_TESTS`). Cases:
  - **Encoder, layout:**
    - one line gives `FB` then its characters;
    - two and three lines give `F8` and `F9`;
    - a fourth line gives `F6`;
    - `FA` after the third line only when more text lines follow, and never with `pause` false;
    - a trailing `{…}` line doesn't count for the pause;
    - an empty line still gets its row marker.
  - **Encoder, commands:**
    - each command's bytes;
    - an unknown command gives nothing and no row;
    - `{NOTEXT}` returns its seven bytes whatever follows;
    - `{INTRO}` pads only the first three rows to 19 characters.
  - **Encoder, wrapping:**
    - a single-byte line breaks at the last space within 19;
    - a 25-character word stays whole, then is cut to 19 characters;
    - no trim on the single-byte path;
    - a multibyte line (with `≥` or `…`) is `rtrim`med and re-packed by character count.
  - **Encoder, characters:**
    - digits, capitals, lower case;
    - `'` and `’`, `@` (`FE 6A`), `>` (`9B 9C`), the arrows, a heart, a variable, a kana;
    - an unknown character gives `FF`.
  - **Encoder, the cut:** a long input is cut to exactly 2,046 bytes.
  - **Default table** (`--fixtures`):
    - every one of the 408 lines of `text_default.txt` equals the C default of the id of that name, in that order;
    - the C has exactly 408 ids;
    - the names round-trip.
  - **Group shape:**
    - exactly one `writeText` call, after both `writeCredits` calls and before any later group's tag;
    - at `0E0000`;
    - its bytes equal the concatenation of the per-id bytes in id order;
    - it lies inside 2 MiB.
  - **Refusal:** a writer refusing the `writeText` call gets no later call, and `rando_rom_write` returns false.
  - **Const:** the generator's bytes are unchanged.
  - **setTexts:**
    - first and last index of each alternative;
    - the pendant and crystal regions;
    - the bow branch (both locations, `Ganons Tower` → `My tower?`);
    - the silvers branch;
    - the fake branch and `crowd_control`;
    - `sign_ganons_tower` absent at 7, `crystal` at 1, `crystals` at 0 and 6;
    - each goal's `sign_ganon` and `ganon_fall_in_alt`, including the `ganon` versus default sentence and the singular;
    - `murahdahla` with the required count.
  - **Hints:**
    - hints off sets only `sign_north_of_links_house`, and no tile;
    - a `LOCATION` tile with an array name at each index;
    - a plain name;
    - a missing item key and a missing location key printed literally;
    - `ucfirst` on a lower-case name;
    - a `JOKE` tile at the first and last index.
  - **Overrides**, one location at a time:
    - empty is the same as `Nothing`, which gives the default;
    - a table entry;
    - `MasterSword` on the pedestal reads as `L2Sword`;
    - an alias gives its target;
    - a dungeon key under generic keys gives `KeyGK`'s text, and without them its own;
    - Ether and Bombos each follow their own location.
  - **Inverted:** the 12 ids are set only under inverted, `{NOTEXT}` ids with their seven bytes.
  - **Seed flag:** an id no source sets is unflagged and keeps its default bytes.
  - **`--fixtures`, the tables:**
    - every `pick` and `item` record equals the C bytes for that id and choice;
    - every `hint_item` and `hint_location` record equals the C name;
    - a raw or location name the C doesn't know fails the test, unless the step note lists it as unreachable with its reason;
    - the C has no hint name the fixture lacks.
  - **`--fixtures`, the goldens:** the comparison over every case (Implementation Decisions).
- **`test_rom`**: its baseline expectation is rewritten on purpose, because the text group now writes (H-8-3). It becomes: items, settings, two `writeCredits`, then one `writeText`, in that order, all inside 2 MiB, accepted by the buffer writer. Step 10 rewrites it again.
- **`test_postfill`**: unchanged. Its `--fixtures` run on `hint_tables.txt` must stay green after the counts come from the string tables.
- **`e2e` and `e2e-combos`**: the step's proof is all 73 claims at `all text`, green.
- **`lint`** builds the new sources with `-std=c99 -pedantic-errors`.
- **Speed:**
  - `make test` gains one binary: one baseline generation, a few dozen encoder calls, and a few dozen `rando_rom_write` calls.
  - The fixture and golden replays (408 defaults, about 1,000 table records, 73 generations) run only in `make logic`.

## Out of Scope

- **The later write groups:** initial SRAM and `setTotalItemCount` (step 10); checksum and heart beep (step 11).
- **Draws:** `setTexts`' and `applyHints`' draws, which step 4 already ported, and any change to `RandoTexts`' or `RandoHints`' shape (H-8-6).
- **Texts outside the frozen settings:**
  - `spoil.BootsLocation`'s uncle and sign texts (always off);
  - `setMysteryMasking` (spoilers `mystery`);
  - `CustomizerController` texts;
  - multiworld;
  - `LocationCollection::getHint`'s multi-item text (it can't be reached);
  - `Dialog::convertDialog` (no caller);
  - `Text::getByteArray`'s padding (`$pad` false).
- **Translations** other than `en`.
- **Oracle changes:** any change to the goldens, the existing fixtures, `stages.txt`, `write_groups.txt` or the claim semantics. The one new fixture is the only oracle change.
- Running `check-e2e` over the whole case list, and `e2e-arm` (step 12).

## Further Notes

- **Why a public text module, when credits stayed private?**
  - The plan verifies "the encoder and default table" against a per-id fixture, but the group writes all 408 entries as one opaque concatenation.
  - Slicing that write back into ids would need the very boundaries under test.
  - A small public interface (encode, default, seed bytes, ids) is a deep module: it hides about 1,500 strings and the whole `Dialog` behind four functions, and it gives `fix-bug` a per-id view.
- **Why keep default strings and not their bytes?** One encoder for every entry keeps the bytes a consequence of VT's text. Then `text_default.txt` proves the encoder on 400 real strings, including both wrap paths, the commands and the kana.
- **Why a second fixture, when the goldens cover the texts?**
  - The 73 goldens reach a few dozen of the 326 string-file alternatives and of the roughly 360 tablet and pedestal entries.
  - A table extracted and tested from the same extraction proves nothing about a wrong first `case` or a lost character.
  - Step 1.4 (`hint_tables.txt`) and step 8 (`credit_tables.txt`) set the precedent.
- **Where a divergence is likely:**
  - PHP `wordwrap`'s exact break rule;
  - the multibyte `rtrim`;
  - the `$line_count` versus `count($lines)` bounds;
  - the 19-character cut;
  - a missing hint key;
  - `ucfirst`;
  - generic keys on a tablet;
  - the inverted intro (long, `pause` false, with `{PAUSE…}` and `{CHANGEPIC}`).

  The goldens of `state-inverted`, `hints-off`, `state-retro` (generic keys), the triforce-hunt cases and the `crystals_tower-*` cases settle these in chunk 6.
