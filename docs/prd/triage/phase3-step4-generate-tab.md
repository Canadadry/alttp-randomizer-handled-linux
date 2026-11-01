---
title: "Generate tab state machine"
description: "The app has no Generate screen: the player cannot see or change a single setting with the d-pad, the settings list is written out four times in the code, and nothing remembers the last settings used."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 4, copied from `docs/plan-1.md`)

> 4. **Generate tab state machine.**
>    - First, the settings list becomes one library table (every RandoOptionKey, then hints, then heart_beep, each with its key name and value names, owned outside rando_rom.h); name.c, record.c, case_reader.c and patch_cli.c iterate it instead of their own lists, and the uint32 parser record.c and patch_cli.c take from rando_case.h moves with it. Verified by test_name, test_record, test_case_reader, test_patch_cli, e2e and e2e-combos green and unchanged.
>    - Every setting of §2 is a row, one per table entry; left and right change the value, and up and down move.
>    - Unsupported combinations from Phase 1 step 3 are greyed out with a reason.
>    - The rows include the spoiler option (default off), the heart beep option, and the Generate action.
>    - The last settings are saved and restored through rando_record's settings writer and reader, not a new parser.
>
>    Verified by key-sequence snapshot tests, including restoring from a saved settings file.

Context the step relies on, from the plan and the repository at HEAD:

- §2 *Generation settings*: the eleven `RandoOptionKey` settings plus the hints toggle. *Cosmetic options*: only the heart beep (off, half, quarter, double, normal). *The seed is always random*: no seed entry; the app remembers the last settings used. *Spoiler*: an option of the Generate screen, off by default; it only decides whether the history record keeps the spoiler.
- §2 *UI*: two tabs, Generate and History, in the style of Onion's menus; every string looked up through the translation table. *UI architecture*: a pure C state machine with no SDL, a key in, a screen description out; tests play a key sequence and compare the text screen with a snapshot.
- Goal 2 and `CLAUDE.md`: controller-first, Onion's menu conventions, never a PC-style panel.
- `docs/onion.md` §5: Onion's Tweaks convention (Up/Down move, Left/Right change a value, A activates, B goes back and quits at the top, Menu released quits, Y resets the item, Select shows help) and Package Manager convention (L1/R1 change tab with wrap-around, L2/R2 page, B back, Start applies). keymon reads the input device without grabbing it, so the app receives every key, including those of keymon's combinations: Select + Start + L1/R1 (CPU clock), Start + L2/R2 (audio boost), Select + L2/R2 (brightness), Menu + A / Menu + B held (recorder, blue light), Menu + Power and Menu + volume. Power and the volume keys belong to keymon. `docs/onion.md` §10 records for this step: "follows the Tweaks and Package Manager keys and leaves keymon's combinations alone".
- Step 1.3's note: **no combination returns `RANDO_UNSUPPORTED`**, so nothing is greyed out for that reason today; the mechanism must exist all the same. At HEAD, `RANDO_UNSUPPORTED` is declared in the library's result enum and handled by the pipeline and `rando_gen`, but no generator code returns it.
- Step 3.2 (done): the UI core is `app/core/`, built with only the library's public headers and itself on its include path; the screen model (`UiScreen`) holds borrowed, already-resolved `const char *` texts, up to 64 rows of kind setting/action/text with a `disabled` flag, a cursor, a scroll, an info or error message and a footer; `ui_scroll_follow` keeps a cursor on a page; the default grid is 40 × 15, its list page is `lines − 4` = 11 rows; a setting row draws the label on the left and the value right-aligned, `< value >` on the cursor row; snapshots are hand-written files in the unit tests' snapshot directory, with no update mode. `app/sd/` holds `record.c` and `seed_save.c` (stdio allowed there, not in the core).
- At HEAD the settings list is spelled in four places: the library's option name arrays plus `rando_hints_name` and `rando_heart_beep_name` (the latter in `rando_rom.h`); `record.c`'s bytewise-sorted `settings_keys`; `case_reader.c`'s `CaseKey` list for hints and heart_beep; `patch_cli.c`'s `SETTING_HINTS` / `SETTING_HEART_BEEP` / `SETTING_COUNT`. `rando_case_parse_uint32` (`^[0-9]{1,10}\n?$`, ≤ 2^32 − 1, error "`<file>: not a decimal uint32 on one line`") lives in the host case reader and is used by `record.c` and `patch_cli.c`. `notes/phase2-warn-review.md` folds the settings list, the uint32 parser's home and `RandoHeartBeep`'s header into this step, and says a third JSON parser is prevented by saving settings through the record.

## Hypotheses about Phase 3 step 3 (not done when this PRD was written)

Phase 3 step 3 (string table, PRD `docs/prd/triage/phase3-step3-string-table.md`) is not done. Each assumption about its outcome is labelled; its consequence when it fails is stated.

- **H-3.3-1** (id list): the string ids are one X-macro list in a UI-core header giving both the id enum `UiStrId` and each id's arity (0 or 1 placeholder); id 0 is the reserved `UI_STR_NONE` (empty English text, never shown); the enum ends with `UI_STR_COUNT`; names read `UI_STR_<AREA>_<NAME>`. Adding a text is one id line plus one English-table line (designated initializer). *If it fails* (another id mechanism), this step's ids are added the way step 3 adds ids; every decision below that names an id keeps its meaning, only the spelling changes.
- **H-3.3-2** (lookup): `ui_str(table, id)` never returns NULL, falls back to English for a NULL entry, treats a NULL table as English, and returns `""` for `UI_STR_NONE`; the table is handed around as a `const` table pointer. `ui_str_fill(pattern, arg, buf, cap)` replaces the one `{}` of a pattern into a caller buffer, always NUL-terminated, true when it fit. *If it fails*, the Generate machine calls whatever lookup and fill step 3 provides; the machine still owns the buffer of every filled text.
- **H-3.3-3** (table check): `ui_strings_check` refuses, for the English table, a missing entry, an empty text (except `UI_STR_NONE`), a byte outside printable ASCII, a wrong placeholder count, a stray brace and a text longer than the default list field (38 characters, `{}` counting as zero); `test_ui_strings` asserts the English table passes it. *If it holds*, every text this step adds is checked by step 3's test with no new code. *If it fails* (no length or byte rule), this step's own map test (below) checks length and bytes of the texts it adds.
- **H-3.3-4** (the literal check): `make test` runs a script that fails on any string literal with a letter in a UI-core source, exempting only the English table file, ignoring comments, character constants, `#include` lines and printf conversions. *If it holds*, the Generate machine, the input filter and the setting-to-id map hold no letter in any literal (the map holds ids, never text), and the English table is where every word of this step goes. *If it fails* (check not yet wired), this step writes its core code to the same rule anyway, so the check passes once it lands.
- **H-3.3-5** (seed ids): `UI_STR_TAB_GENERATE` ("Generate") and `UI_STR_TAB_HISTORY` ("History") exist. *If they hold*, the Generate screen's tab bar uses them. *If they fail*, this step adds them with those texts.
- **H-3.3-6** (enum-to-id pattern): a table mapping another enum's values to string ids is a `const UiStrId` array sized by that enum's count, and its test walks every value and fails on an entry still equal to `UI_STR_NONE`. *If it holds*, the setting-to-id map below follows it. *If it fails*, the map follows the rule written below, which is self-contained.
- **H-3.3-7** (test wiring): new unit tests live in the unit test directory, are registered in `UNIT_TESTS`, listed in the `LOGIC_TESTS` filter-out list when they have no fixture mode, fail through `test_fail`; every `.c` file of `app/core/` and `app/sd/` is picked up by wildcard into the app archive, `make lint` and `ARM_UNIT_SRC` (true at HEAD for step 2's files). *If it fails*, this step registers its files the way step 3 did.

## Problem Statement

The player has a console and a d-pad, and no screen to choose the settings of the seed they want: there is no Generate tab, so none of §2's thirteen settings, the spoiler option or the Generate action can be seen or changed. Each start would also forget what the player chose last time, although §2 promises the app remembers it.

Behind it, the agent has no single place that says what the settings are. Their names and values are spelled in four places (the library, the history record, the case reader, the patch CLI), with three different orders, and `RandoHeartBeep` sits in the ROM writer's header. A Generate tab built on top of that would add a fifth list, and a setting added or renamed would have to be found in five places, one of them on screen. And the player-facing words are not decided anywhere: the library's names (`fast_ganon`, `crowd_control`) are file keys, not English, and nothing in `make test` would catch the tab showing them raw.

## Solution

**One settings table in the library.** A new library module lists the thirteen settings in a fixed order — the eleven `RandoOptionKey` settings in enum order, then hints, then heart beep — each with its key name, its value count, its value names and its default, and gets and sets any setting of a `RandoOptions` plus a `RandoHeartBeep` by index. `RandoHeartBeep` and its names move there. The record, the ROM naming code, the case reader and the patch CLI walk this table instead of their own lists; the uint32 parser moves into the same module. Every existing test passes unchanged, so the files, codes and messages are byte for byte what they were. The same module holds the list of unsupported setting pairs: empty today, as step 1.3 found.

**A Generate tab driven like an Onion menu.** The tab shows one row per table entry, then a "Spoiler in History" row (off by default), then the "Generate Seed" action, under the tab bar (Generate active, History beside it), with a title and a one-line legend. Up and Down move the cursor (wrapping at both ends), Left and Right change the value (wrapping), A changes it forward too, Y puts the row back to its default, L2 and R2 jump a page, Start generates from anywhere, A on the action row generates. B and Menu ask to quit, L1 and R1 ask to change tab: the tab reports them to its caller instead of acting, so the app shell (later steps) decides. Every label and every value shown is a string-table text: the player reads "Fast Ganon", not `fast_ganon`.

**Keymon's combinations left alone.** An input filter in the core turns raw button presses and releases into the buttons a screen acts on. Start, Select and Menu act only when released alone; any button pressed while one of them is held is swallowed. So Start + L2 raises the audio boost without generating a seed, and Select + Start + R1 changes the CPU clock without touching a setting.

**Greyed out with a reason.** When the current settings hold an unsupported pair from the library's list, both rows are greyed, the Generate action is greyed, the cursor on a greyed row shows "Not supported with <other setting>", and Generate refuses with that message instead of starting. Their values still change, so the player can leave the combination. With today's empty list nothing is ever greyed; tests inject a list to prove the mechanism.

**The last settings remembered.** The history record's settings object becomes a public writer and reader of its own; the app's SD layer saves the thirteen settings to one file in the app folder with that writer, and loads them with that reader at start. A missing file gives the defaults silently; an unreadable one gives the defaults and a one-line notice on the first screen. The spoiler option is not remembered: it starts off at every start.

## User Stories

1. As the player, I want a Generate tab that opens with every setting listed, so that I see at once what seed I am about to make.
2. As the player, I want each setting on its own row with its current value at the right, so that the screen reads like Onion's own menus.
3. As the player, I want Up and Down to move between rows, so that I reach any setting with the d-pad.
4. As the player, I want Down on the last row to wrap to the first and Up on the first to wrap to the last, so that the Generate action is one press away from the top.
5. As the player, I want Left and Right to change the value of the row under the cursor, so that I never open a sub-menu.
6. As the player, I want values to wrap at both ends, so that one direction reaches every value.
7. As the player, I want A on a setting row to change it like Right, so that the main button is never dead.
8. As the player, I want the selected row to show `< value >`, so that I know Left and Right will change it.
9. As the player, I want the list to scroll when the cursor leaves the page, with marks for rows above and below, so that I know the list goes on.
10. As the player, I want L2 and R2 to jump a page, as in Onion's Package Manager, so that long lists are quick to cross.
11. As the player, I want Y to put the row under the cursor back to its default, as in Onion's Tweaks, so that I can undo a change without remembering the default.
12. As the player, I want Start to generate from any row, as Onion's Package Manager applies with Start, so that I do not scroll to the end each time.
13. As the player, I want A on the "Generate Seed" row to generate, so that the action is also a normal list item.
14. As the player, I want B and Menu to quit, as in Onion's apps, so that leaving works the way I expect.
15. As the player, I want L1 and R1 to switch to the History tab, so that tabs work as in the Package Manager.
16. As the player, I want to see both tabs at the top with Generate highlighted, so that I know where I am.
17. As the player, I want every label and value in plain English ("Fast Ganon", "Keysanity", "Half Speed"), so that I understand them without knowing alttpr.com's file keys.
18. As the player, I want the labels and values close to alttpr.com's wording, so that guides and videos still apply.
19. As the player, I want every label and value to fit on its row without being cut, so that nothing reads as `Dungeon I~`.
20. As the player, I want a one-line legend at the bottom, so that I learn the buttons on the first screen.
21. As the player, I want a "Spoiler in History" row that is off by default, so that I am not spoiled unless I ask.
22. As the player, I want the spoiler to be off again at each start, so that I never keep spoilers by accident because of a past choice.
23. As the player, I want a "Heart Beep" row with Off, Half, Quarter, Double and Normal speed, defaulting to Half as alttpr.com does, so that I can quiet the low-health beep.
24. As the player, I want a "Hints" row, on by default, so that I can turn the telepathic tile hints off.
25. As the player, I want the app to come back with the settings I last generated with, so that I do not set thirteen rows again each time.
26. As the player, I want a clear one-line notice when my saved settings could not be read, with the defaults in place, so that I know why they changed.
27. As the player, I want no notice at all on the very first start, so that a fresh install looks clean.
28. As the player, I want an unsupported combination greyed out on both of its rows, so that I see which settings clash.
29. As the player, I want the reason shown when the cursor is on a greyed row, so that I know which other setting to change.
30. As the player, I want a greyed row's value still to change, so that I can leave the combination from either row.
31. As the player, I want Generate greyed and refused, with the reason, while the combination is unsupported, so that I never wait for a generation that cannot work.
32. As the player, I want pressing Start + L2/R2 (audio boost), Select + L2/R2 (brightness) or Select + Start + L1/R1 (CPU clock) to do only what keymon does, so that adjusting the console never generates a seed, pages the list or switches tab.
33. As the player, I want Menu + A or Menu + B held (keymon's recorder and blue-light toggles) not to quit or change anything, so that those shortcuts are safe here.
34. As the player, I want Start, Select and Menu pressed alone to act on release, so that I can start a keymon combination with them without side effects.
35. As the owner, I want the settings named in one library table, so that adding or renaming a setting is one edit that every file format, the CLI and the screen follow.
36. As the owner, I want the record, the case files, the CLI arguments and the ROM codes byte for byte unchanged by that refactor, so that existing seeds, goldens and records stay valid.
37. As the owner, I want `RandoHeartBeep` out of the ROM writer's header, so that a settings user does not include the ROM writer.
38. As the owner, I want the uint32 parser in the library, so that the SD layer and the CLI do not depend on the test case reader.
39. As the owner, I want the last settings stored with the history record's own settings writer and reader, so that the app has one settings format and no third JSON parser.
40. As the owner, I want the settings file written safely (a temporary file, then a rename), so that a power cut mid-write keeps the previous settings.
41. As the owner, I want the unsupported-pair list to be library data, next to the generator that would return `RANDO_UNSUPPORTED`, so that the fact and its wording live in their own layers.
42. As the owner, I want the greying mechanism tested with an injected list while the real list is empty, so that it works the day a pair is found.
43. As the owner, I want the Generate tab to report Generate, Quit and tab changes to its caller instead of doing them, so that step 5 (generation) and the app shell plug in without changing it.
44. As the owner, I want the input filter separate from the Generate tab, so that History and the generation screens get the same keymon-safe behaviour.
45. As the agent, I want each key sequence's screen compared with a hand-written snapshot, so that I see the tab as the player does.
46. As the agent, I want the snapshot test to cover every row and every value of every setting, so that no label or value is missing or cut.
47. As the agent, I want a test restoring the tab from a settings file written by the SD layer, so that the save-restore loop is proven end to end.
48. As the agent, I want a test that every setting and every value has its own string id, so that a new value cannot appear on screen untranslated.
49. As the agent, I want the core to take abstract buttons, not SDL keys, so that tests and `ui_cli` drive it without SDL.
50. As a future translator, I want each setting and each value to be its own id, even where two English words coincide ("Normal", "Off"), so that my language can use different words in different rows.
51. As the net, I want every new source built by `make lint` with `-pedantic-errors` and every new test run by `make unit-arm` under qemu-arm, so that the tab behaves the same on the device.

## Implementation Decisions

### The settings table (library)

- **A new library module**, outside the ROM writer's header, the single owner of the settings list. Its contents, names indicative:
  - `RandoSettingId`: values 0 … 10 equal the `RandoOptionKey` values (a compile-time check asserts `RANDO_OPTION_KEY_COUNT` is 11 and the first and last coincide), then `RANDO_SETTING_HINTS` (11), `RANDO_SETTING_HEART_BEEP` (12), `RANDO_SETTING_COUNT` (13).
  - `RandoHeartBeep` (unchanged values and count) and `rando_heart_beep_name` / `rando_heart_beep_find` move here; the ROM writer's header includes this one, so every current includer still compiles unchanged.
  - `rando_setting_key(id)`, `rando_setting_value_count(id)` (5, 2, 4, 3, 7, 9, 9, 4, 4, 4, 3, 2, 5), `rando_setting_value_name(id, value)`, `rando_setting_find(key)` (`RANDO_SETTING_COUNT` when unknown), `rando_setting_value_find(id, name)` (−1 when unknown), `rando_setting_default(id)`.
  - `rando_setting_get(const RandoOptions *, RandoHeartBeep, id)` → value index, and `rando_setting_set(RandoOptions *, RandoHeartBeep *, id, value)`; both assert the ranges.
  - **Value indices**: an option's index is its enum value (as `rando_options_get` returns today); heart beep's is its enum value; hints is 0 = `on`, 1 = `off` (the baseline first, the options' own convention).
  - **Defaults**: index 0 for every option except the two crystal counts (`7`), hints `on`, heart beep `half` — exactly `rando_options_init` plus the heart beep the case reader and `rando_assemble_request_init` default to; a test ties them together.
  - `rando_setting_sorted(rank)`: the setting whose key is the rank-th in bytewise order (the record's key order), computed from the table (no second list), pure and stateless.
  - `rando_parse_uint32(text, len, *out)`: the moved parser, same accepted set (`^[0-9]{1,10}\n?$`, at most 2^32 − 1), no error text (the library has no stdio); callers format their own messages.
  - **Unsupported pairs**: `rando_setting_unsupported_count()` and `rando_setting_unsupported(i)` returning a pair `{setting a, value a, setting b, value b}`; the list is empty (step 1.3). A pair is unsupported when both settings hold their values at once. Adding a pair is a deliberate change with its evidence (a combination case returning `RANDO_UNSUPPORTED`) in the commit message.
- **The names exist once.** The option value name arrays move into the table; `rando_option_key_find`, `rando_option_key_name`, `rando_option_value_name`, `rando_options_set`, `rando_options_get`, `rando_hints_name` and `rando_hints_find` stay as they are, public, implemented over the table, so `test_options`, `test_rom` and `spoiler_json.c` are untouched.
- **Callers iterate the table:**
  - ROM naming (`rando_name_code`): one loop over the thirteen settings in table order, `key=value\n` each; the text, so every code, is identical to today's (eleven options in enum order, then hints, then heart beep).
  - Record: the sorted key list goes; the writer and the parser walk `rando_setting_sorted`, look values up and set them through the table. Output text and every refusal message unchanged. The seed is parsed with `rando_parse_uint32`, the message unchanged (`seed: not a decimal uint32 on one line`).
  - Case reader: the `CaseKey` list goes; a key found in the table is required when its id is below `RANDO_OPTION_KEY_COUNT`, optional otherwise; defaults from the table; messages unchanged. `rando_case_parse_uint32` stays in the case reader's header as a thin wrapper over the library parser that writes today's message, so `test_case_reader` is unchanged.
  - Patch CLI: the `SETTING_*` defines go; one `seen` array of `RANDO_SETTING_COUNT`, keys found and values set through the table; `--seed` parsed with the library parser, messages unchanged.
- **Out of reach on purpose**: the record's second decoding of the name code and its date ranges (`notes/phase2-warn-review.md`) stay as they are: the table chunk does not touch them.

### The record's settings writer and reader (SD layer)

- `rando_record_settings_write(FILE *, const RandoOptions *, RandoHeartBeep, depth)` writes the settings object exactly as it appears in a record, its members indented one level below `depth` (the record calls it with depth 1, giving today's text byte for byte; the settings file uses depth 0: `{`, thirteen sorted members indented four spaces, `}` and a newline).
- `rando_record_settings_parse(text, len, RandoOptions *, RandoHeartBeep *, error)`: the whole text is one settings object, optional whitespace around it, with the record's own strict rules (all thirteen keys once each, known values, nothing else) and messages; it is the record's existing settings parser, not a new one. On failure the outputs are unspecified.

### The last-settings file (SD layer)

- One file, `App/ALttPR/settings.json` (constant next to the other SD paths, relative to the SD root). Not per profile, like History.
- `rando_last_settings_save(path, options, heart_beep)`: writes `<path>.tmp` with the settings writer at depth 0, closes it, renames it over `path`; on any failure removes the temporary file, leaves the old file, returns false.
- `rando_last_settings_load(path, options*, heart_beep*)` → `RANDO_LAST_SETTINGS_LOADED`, `_MISSING` (no file: defaults written to the outputs) or `_INVALID` (unreadable, longer than a small cap of 4 KiB, or refused by the parser: defaults written). It never leaves partly parsed values in the outputs.
- **What is saved**: the thirteen table settings only. **The spoiler is not saved** and starts off at every start: the settings object is the record's, which has no spoiler key, and a wrapper object would need a new parser (the plan forbids one); §2's "off by default" then holds at every start.
- **When**: the file is saved when a generation is requested (the "last settings used"). This step provides the functions and tests them; calling save on the Generate event is step 5's flow (it owns what follows Generate), and calling load at start belongs to whoever builds the app's start (step 7's `ui_cli` and Phase 4's main). The Generate tab never touches a file.

### Buttons and the input filter (UI core)

- **`UiButton`**: Up, Down, Left, Right, A, B, X, Y, L1, R1, L2, R2, Select, Start, Menu, and a count. No Power or volume (keymon's). The SDL layer (Phase 4) maps Onion's SDL keys (`docs/onion.md` §5) or the PC keyboard to these; the core never sees an SDL key.
- **The filter** (`UiInput`, a small state struct): `ui_input_event(state, button, down, *out)` returns true with the button a screen acts on, or false.
  - Start, Select and Menu are *chord starters*: their press emits nothing; their release emits them only if no other button was pressed since their press.
  - Any other button pressed while a chord starter is held is swallowed and marks that starter as used.
  - Any other button emits on press; releases emit nothing; a repeated press (key repeat, delivered by the input layer for held d-pad and L2/R2) emits again.
  - This covers every keymon combination §5 lists for an app (Select + Start + L1/R1, Start + L2/R2, Select + L2/R2, Menu + A/B held); Power and volume never arrive.
- The Generate tab consumes the filter's output, never raw events; the filter is shared with the later screens.

### Setting texts (UI core)

- **A setting-to-id map** in the core: `ui_setting_label(RandoSettingId)` and `ui_setting_value(RandoSettingId, value)` → `UiStrId`, as `const` arrays sized by `RANDO_SETTING_COUNT` and by a per-setting maximum of 9 values (`UI_SETTING_VALUES_MAX`, asserted ≥ every `rando_setting_value_count`). Out-of-range calls assert. Every setting and every value has its own id, even where English repeats ("Normal" in item pool and item functionality, "Off" in hints, heart beep and spoiler).
- **English texts** (step 3's table; the agent may change a text only to satisfy a table rule, stating it). Values in the library's index order:

| Setting | Label | Values |
|---|---|---|
| glitches | Glitches | None, Overworld Glitches, Hybrid Major Glitches, Major Glitches, No Logic |
| item_placement | Item Placement | Advanced, Basic |
| dungeon_items | Dungeon Items | Standard, Map/Compass, Map/Compass/Keys, Keysanity |
| accessibility | Accessibility | 100% Inventory, 100% Locations, Beatable |
| goal | Goal | Defeat Ganon, Fast Ganon, All Dungeons, Master Sword Pedestal, Triforce Hunt, Ganonhunt, Completionist |
| crystals_tower | Tower Crystals | 0, 1, 2, 3, 4, 5, 6, 7, Random |
| crystals_ganon | Ganon Crystals | 0, 1, 2, 3, 4, 5, 6, 7, Random |
| state | World State | Open, Standard, Inverted, Retro |
| weapons | Swords | Randomized, Assured, Vanilla, Swordless |
| item_pool | Item Pool | Normal, Hard, Expert, Crowd Control |
| item_functionality | Item Functionality | Normal, Hard, Expert |
| hints | Hints | On, Off |
| heart_beep | Heart Beep | Off, Half Speed, Quarter Speed, Double Speed, Normal Speed |
| (spoiler row) | Spoiler in History | Off, On |

  Other texts: the action row `Generate Seed`; the title `Seed Settings`; the footer `</> Change  Y Default  START Generate` (37 characters); the reason `Not supported with {}` (arity 1, filled with the other row's label); the notice `Saved settings unreadable: defaults`. Every label fits next to its longest value at the default grid (checked by rendering, below).
- Display order is the table's (row order) and the library's value order (cycling order); no second order is kept for the screen.

### The Generate tab state machine (UI core)

- **State** (`UiGenerate`, caller-owned, no allocation): the options, heart beep and spoiler flag, the cursor, the scroll, the page size, the string table pointer, the unsupported-pair source, the one-shot message (kind and id) and the buffer of the filled reason text (the screen borrows it).
- **Init**: `ui_generate_init(state, &init)` with the string table, the page size (the grid's list lines), the starting settings (options, heart beep), whether to show the "saved settings unreadable" notice, and the unsupported-pair list as a pointer and a count (production passes the library's list; tests inject their own). Cursor on row 0, scroll 0, spoiler off.
- **Rows**, in order: the thirteen table settings (label, value), `Spoiler in History`, then the action `Generate Seed`: 15 rows, so the default page (11) scrolls.
- **Keys** — `ui_generate_key(state, button)` → event (`UI_GENERATE_NONE`, `_GENERATE`, `_QUIT`, `_TAB_NEXT`, `_TAB_PREV`):
  - Up / Down: cursor −1 / +1, wrapping; scroll follows with `ui_scroll_follow`.
  - Left / Right on a setting or spoiler row: previous / next value, wrapping. A on such a row: as Right. On the action row: Left and Right do nothing.
  - Y: the row's default (the table's default; spoiler off); nothing on the action row.
  - L2 / R2: cursor one page up / down, clamped to the first / last row (no wrap), scroll follows.
  - Start anywhere, or A on the action row: `GENERATE` when no unsupported pair holds; otherwise `NONE` and an error message with the reason.
  - B, Menu: `QUIT`. L1 / R1: `TAB_PREV` / `TAB_NEXT` (the tab bar's wrap is the shell's, two tabs make both the same).
  - X, Select: nothing (Select's help is out of scope).
  - Any key clears the previous one-shot message (notice or refusal) before acting.
- **Screen** — `ui_generate_screen(state, UiScreen *out)`: tabs `Generate` and `History`, active 0; the title; the rows (resolved texts through the table pointer); cursor and scroll; `more_above` / `more_below` from the scroll and page; the footer; the message: the one-shot message if any, else, when the cursor is on a greyed row, the info message with that row's reason, else none.
- **Greying**: a pair holds when both settings have their values; then both rows and the action row are `disabled`; the reason is `Not supported with {}` filled with the label of the pair's other setting (for the action row and for Generate's refusal: the first holding pair in list order, its setting b's label). A disabled row stays selectable and changeable.
- **Reading the result**: `ui_generate_settings(state, &options, &heart_beep, &spoiler)`, for step 5 and for the save.
- **Rule compliance**: pure C99, no stdio, no allocation, no SDL, no letter in any literal (H-3.3-4); only the library's public headers and the core on its include path.

## Implementation Chunks

1. **Settings table.** The library module (ids, `RandoHeartBeep` moved, names, counts, defaults, get/set, sorted rank, uint32 parser, empty unsupported list); the option name arrays and the heart-beep names moved into it, old public functions kept over it; name, record, case reader and patch CLI iterating it; `rando_case_parse_uint32` a wrapper. New `test_settings`. Done when `test_name`, `test_record`, `test_case_reader`, `test_patch_cli`, `test_options` and `test_rom` pass with their sources unchanged, `make -C tests/rando e2e` and `e2e-combos` are green, and `make check` is green.
2. **Record settings writer and reader, last-settings file.** The two record functions (the record's own output and refusals unchanged), the last-settings save and load. New cases in `test_record` (existing ones untouched) and a new `test_last_settings`. Done when they pass and `make check` is green.
3. **Buttons and input filter.** `UiButton`, the filter, `test_ui_input`. Done when the filter tests pass, `make lint` builds it, and `make check` is green.
4. **Setting texts and the Generate tab.** The ids and English texts of the table above, the setting-to-id map, the machine (rows, movement, values, Y, pages, events, scroll), `test_ui_generate` with its snapshots and the every-row, every-value rendering test. Done when they pass and `make check` is green.
5. **Greying and restore.** The unsupported-pair greying, refusal and reason; the notice; the restore test through the SD layer's file; `make unit-arm` run with the new tests (counts for the step note). Done when the greying and restore snapshots pass and `make check` is green.

## Testing Decisions

- **What a good test is here**: it goes through public functions only — the table's functions, the record's settings writer and reader, save and load, the filter, `ui_generate_key` and `ui_generate_screen` rendered by `ui_grid_render` and `ui_grid_text` — and compares results; expected screens are hand-written snapshots, never generated from the code's output and never updated by a mode of the test; expected values come from the rules above and from today's files, not from the new code.
- **`test_settings`** (library, no fixture mode): thirteen settings in the stated order with the stated keys and value counts; every value's name matches the old public functions (`rando_option_value_name`, `rando_hints_name`, `rando_heart_beep_name`); `find` and `value_find` invert `key` and `value_name`, refuse unknown names; get/set round trip of every value of every setting, leaving the twelve others unchanged; defaults equal `rando_options_init` plus heart beep half; `rando_setting_sorted` gives the record's key order (written out in the test from the record's current text); the uint32 parser's accepted and refused inputs (empty, 11 digits, 4294967295, 4294967296, a trailing newline, two newlines, a sign, a space); every listed unsupported pair names two different settings and in-range values (vacuous today).
- **Unchanged tests as the refactor's proof**: `test_name` (codes from Python's `zlib`), `test_record` (exact record text, every refusal), `test_case_reader`, `test_patch_cli`, `test_options`, `test_rom`, `e2e`, `e2e-combos` — sources untouched in chunk 1.
- **`test_record` new cases** (chunk 2): the settings writer at depth 0 gives an exact hand-written text for the baseline and for every setting non-default; at depth 1 it equals the settings part of an existing record test's exact text; the reader round-trips every setting, hints and heart beep at a non-default value; it refuses a missing key, a duplicate, an unknown key (`spoiler` included), an unknown value, trailing data, with the record's messages.
- **`test_last_settings`** (temporary directory under `/tmp` via `mkdtemp`, like `test_seed_save`): save then load gives the same settings (`LOADED`); a missing file gives `MISSING` and the defaults; an empty file, garbage, a whole history record, a file over the cap give `INVALID` and the defaults, outputs overwritten even when they held other values; save over an existing file replaces it and leaves no `.tmp`; save into a missing directory returns false and leaves an existing file elsewhere untouched.
- **`test_ui_input`**: a lone press of every non-starter emits it once; releases emit nothing; repeats emit; Start, Select, Menu alone emit on release only; each keymon combination of §5 (Start + L2, Start + R2, Select + L2, Select + R2, Select + Start + L1, Select + Start + R1, Menu + A, Menu + B) emits nothing at all, in either release order; after a swallowed chord the next lone press works again.
- **`test_ui_generate`**: each snapshot is a key sequence from a given init, rendered at 40 × 15, compared with a hand-written file in the snapshot directory (step 3.2's practice). Snapshots:
  - the initial screen at the defaults (rows 1–11, `v` mark);
  - Up from the top: cursor on `Generate Seed`, scrolled to the end (`^` mark);
  - a value changed with Right, and one wrapped with Left (Goal from Defeat Ganon to Completionist);
  - Y back to the default after a change;
  - R2 then L2 (page moves, clamped);
  - the spoiler row turned on;
  - restored from a settings file written by `rando_last_settings_save` with every setting non-default, loaded by `rando_last_settings_load` and passed to init (spoiler still off);
  - the notice after an invalid file;
  - with an injected unsupported pair (e.g. state retro with goal pedestal): both rows and the action greyed, the cursor on one greyed row showing its reason, and the refusal after Start;
  - the same injected pair not holding: nothing greyed.
- **`test_ui_generate` behaviour checks** (no snapshot): events — Start from every row gives `GENERATE`, A on the action row gives `GENERATE`, A elsewhere gives `NONE`, B and Menu `QUIT`, L1 / R1 tab events, X and Select `NONE` with the screen unchanged; Generate refused while a pair holds (`NONE`); `ui_generate_settings` returns what the keys set; **every row and every value**: for each setting, cycling Right through all its values, the cursor line of the rendered grid holds the full English label and the full English value and no `~` (this is §4's "every Generate row" coverage at the text level); the map gives a non-`UI_STR_NONE` id for every label and every in-range value, and two different settings never share a label id (H-3.3-6).
- **`make lint`** builds the new library and core sources with `-pedantic-errors`; **`make unit-arm`** runs the new tests under qemu-arm (the SD tests list no directory, so the `readdir` quirk of step 2.3 does not apply).
- **Prior art**: `test_ui_grid` (hand-written snapshots, no update mode), `test_record` (exact text, strict refusals, round trip), `test_seed_save` (temporary SD tree), `test_options` (every value of every key), `test_case_reader` (coverage guard).

## Out of Scope

- What happens after `GENERATE` (the generation screen, saving the last settings at that moment, success and failures): step 5.
- The History tab, the tab switching between Generate and History, quitting the app: step 6 and the app shell (step 7, Phase 4).
- Loading the settings file at the app's start and the SD root resolution: `ui_cli` (step 7) and Phase 4's main.
- The SDL key mapping, key-repeat timing and the drawing: Phase 4 step 1.
- Select's per-row help text, a seed entry, sharing, cosmetics other than the heart beep (§2).
- Remembering the spoiler option.
- Any real unsupported pair: the list stays empty unless a case returns `RANDO_UNSUPPORTED`.
- alttpr.com's form-level constraints (crowd control forcing advanced placement, `dungeons` forcing Ganon's crystals 7, `completionist` forcing locations accessibility): the generator applies or ignores them as VT does; they are not "unsupported" and are not greyed.
- The record's name-code decoding and date ranges (`notes/phase2-warn-review.md`), and the other warnings of that note.
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): the outcome of H-3.3-1 to H-3.3-7; the `make unit-arm` counts; that the six unchanged test sources were not touched by chunk 1 (`git diff --stat` on them empty); that the unsupported list is empty because of step 1.3.
- **For step 5**: on `UI_GENERATE_GENERATE`, read the settings with `ui_generate_settings`, save them with `rando_last_settings_save` before generating (a failed save is not a reason to refuse generation; whether it is shown is step 5's choice), then pass the spoiler flag to `rando_seed_save`.
- **For step 6**: the History tab uses the same `UiButton` filter and the same L1/R1, B and Menu meanings; the History detail view can show settings with `ui_setting_label` / `ui_setting_value`, so the record's settings read the same in both tabs.
- **For step 7 and Phase 4**: at start, `rando_last_settings_load` on `<SD root>/App/ALttPR/settings.json`; show the notice only on `INVALID`. Feed raw presses and releases through the `UiInput` filter.
- **Decisions taken without a human** (to-prd ran unattended): the row order is the table's; Start generates from any row; values and the cursor wrap, pages clamp; A acts as Right on a value row; B and Menu quit; the spoiler is not remembered; the reason text is one generic arity-1 text naming the other setting, because no real pair exists to word a specific reason; the English labels follow alttpr.com's wording, shortened to fit the 40-column grid.
