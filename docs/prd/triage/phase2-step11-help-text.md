---
title: "Help text per value"
description: "On the Generate tab the player sees a setting's value name (\"Hybrid Major Glitches\", \"Assured\", \"Ganonhunt\") but nothing says what that value does, so choosing settings on the device needs alttpr.com open beside it."
status: needs-triage
---

## Problem Statement

The Generate tab lists fourteen settings, each with a value the player
changes with left and right. The names come from alttpr.com and mean
little without its descriptions: "Hybrid Major Glitches", "Map/Compass/Keys",
"100% Inventory", "Assured", "Ganonhunt", "Tower Crystals 3". On the Miyoo
there is no tooltip and no browser. The owner's first mockup
(`design/mockups/00_settings_help.svg`) draws the answer: one cyan line
under the list saying what the selected value does ("No glitches required.
Standard logic only."). The app has the zone for it (step 8 put a `[help]`
section in `themes/alttp`, 58 characters wide) but nothing fills it: the core
has no help strings, `UiScreen` cannot carry a help line, the text renderer
cannot show one and the SDL renderer does not draw one. Step 9's text check
leaves the help line out ("help line excepted").

## Solution

Each value of each setting gets one line of English help, written from VT
`219fcafd`'s behaviour (the C port in `rando/`) and alttpr.com's
descriptions: 63 strings (61 for the 13 library settings, 2 for Spoiler in
History), all in the translation table, each at most 58 characters (the
alttp theme's help line). While the cursor is on a setting row, the line
under the list says what that row's current value does; moving the cursor
or changing the value changes it at once. A message (error or info: "Not
supported with Goal", "Saved settings unreadable: defaults") takes the
help's place. The Generate Seed row, every History screen and every notice
screen have no help.

The text renderer (`ui_cli`, the snapshots, `alttpr --shot-text`) shows the
help in the message line, tagged `h`, cut with `~` like any field too wide
for the grid. The SDL renderer draws it in the theme's `[help]` zone (font,
position, colour); a theme without `[help]` (`classic`) draws no help line,
so the 13 classic goldens stay byte-identical. Step 9's mockup check now
compares the help line too: `00_settings_help.svg`'s help line must equal
the app's.

The snapshots that show a setting row with no message change in this
step's core commit, in one line each (the message line), stated in its
message as the deliberate change of plan 2 step 11.

## User Stories

1. As a player, I want a line explaining the selected setting's value, so that I can choose settings on the device without alttpr.com.
2. As a player, I want the line to follow the value as I press left or right, so that I read what each value does before I keep it.
3. As a player, I want the line to follow the cursor up and down, so that every setting explains itself when I reach it.
4. As a player, I want the line to follow L2/R2 page jumps and the Y reset, so that it is never stale.
5. As a player, I want one short sentence per value, so that it fits on one line of the small screen.
6. As a player, I want every value of every setting covered (Glitches to Heart Beep and Spoiler in History), so that no setting is left unexplained.
7. As a player, I want each crystal count explained on its own ("opens once you hold 3 crystals"), so that I do not have to guess what the number means.
8. As a player, I want Random crystals explained (0 to 7, picked at random), so that I know the count is chosen at generation.
9. As a player, I want the goals' hunts to state their numbers (20 of 30, 40 of 50 Triforce Pieces), so that I know the size of the task VT sets.
10. As a player, I want a "Not supported with Goal" reason to replace the help on a greyed row, so that I see why the row is grey.
11. As a player, I want a refused START's error to replace the help, so that the error is never hidden.
12. As a player, I want "Saved settings unreadable: defaults" to show on launch instead of the help, so that I learn my settings were reset.
13. As a player, I want the help back as soon as the message goes (the next key), so that the line is useful again.
14. As a player, I want no help on Generate Seed, so that the line does not repeat what the row says.
15. As a player, I want no help in History, so that the date line and its notices keep their place.
16. As a player, I want no help on the Generating, Seed Ready, Generation Failed and Delete screens, so that their messages stand alone.
17. As a player, I want the help in its own colour and size (the alttp theme's 10 px cyan), so that I tell it from an error.
18. As the owner, I want the help line drawn where `00_settings_help.svg` draws it, so that the new look keeps the line I asked for.
19. As the owner, I want the text of 00's help line to match the app's, checked by `make mockups`, so that the mockup and the app agree.
20. As the owner, I want `classic` unchanged, so that the phase-1 goldens still prove the engine.
21. As a translator, I want every help string in the translation table, so that a translation covers it like any other string.
22. As a translator, I want help strings checked against their own bound (58), not the 38 of other strings, so that a translation knows its room.
23. As a maintainer, I want a unit test that every help string fits the alttp theme's help line, so that a long string is caught in `make test`.
24. As a maintainer, I want the core's help bound tied to the alttp theme by a test, so that step 12 changing the help zone updates the bound in the same step.
25. As a maintainer, I want the rendered width of every help string checked with the real font in `make sdl-pc`, so that the bound is true in pixels.
26. As a maintainer, I want a test that every setting value has its own help string and every help string belongs to exactly one value, so that a new value cannot ship without help.
27. As a maintainer, I want the precedence (message, then reason, then help) decided in the core once, so that both renderers show the same thing.
28. As a maintainer, I want the text renderer to show the help, so that the agent sees it through `ui_cli` and the snapshots.
29. As a maintainer, I want the snapshot changes confined to this step's core commit and stated, so that no expected value changes silently.
30. As a maintainer, I want `rando/` untouched, so that the help is UI work only.
31. As a maintainer, I want no string literal outside `ui_strings_en.c`, so that `no_literals.py` stays green.
32. As the agent of step 12, I want the help drawn in theme captures, so that `make theme-compare` shows it against 00.

## Implementation Decisions

### The strings (core, translation table)

- 63 new ids, arity 0, in one contiguous block of `UI_STR_LIST` right after
  `UI_STR_SPOILER_ON`, in the order of the table below (settings in library
  order, values in library value order, then Spoiler Off and On). Their
  English text goes in `ui_strings_en.c`, copied exactly from the table.
- The block is named by two aliases, `UI_STR_HELP_FIRST`
  (= `UI_STR_HELP_GLITCHES_NONE`) and `UI_STR_HELP_LAST`
  (= `UI_STR_HELP_SPOILER_ON`), declared after the enum.
- New bound `UI_STR_HELP_LEN_MAX` = 58 in `ui_strings.h`, beside
  `UI_STR_LEN_MAX` (38, unchanged). A one-line comment carries the contract:
  it equals `themes/alttp`'s help line, checked by `test_theme`.
- New `int ui_str_len_max(UiStrId id)`: `UI_STR_HELP_LEN_MAX` for ids in
  the help block, `UI_STR_LEN_MAX` otherwise. `ui_strings_check`'s
  `TOO_LONG` uses it (no other change to the check).
- Writing rules the table follows (for a later edit): plain ASCII, no `{}`,
  one or two short sentences ending with a full stop, facts from the port
  (`rando/src/world.c` hunt counts 20/30 and 40/50, item-pool caps,
  functionality values, `apply_logic_glitches`, placement in `prepare.c`
  and `world.c`, `random_int(0, 7)` for Random crystals), no claim the port
  does not implement.

### The value-to-help map (core, `ui_settings`)

- `UiStrId ui_setting_help(RandoSettingId id, int value)`: same contract
  and assert as `ui_setting_value`, from a table parallel to `values[]`.
- `UiStrId ui_spoiler_help(bool on)`: `UI_STR_HELP_SPOILER_ON` /
  `UI_STR_HELP_SPOILER_OFF`.

### The screen (core, `ui_screen.h`)

- `UiMessageKind` gains `UI_MESSAGE_HELP`, appended after
  `UI_MESSAGE_ERROR`. One message slot, one kind: the help is a message of
  its own kind, so the precedence lives in the core and every renderer only
  styles by kind. No new `UiScreen` field.

### Precedence (core, `ui_generate_screen`)

- In order: the state's message (error or info) → the selected row's
  unsupported reason (info) → the help of the selected row's current value
  (`UI_MESSAGE_HELP`): `ui_setting_help` for a library setting row,
  `ui_spoiler_help` for the Spoiler row, none for the action row.
- Computed when the screen is built from the state; `UiGenerate` gains no
  field. Nothing else (History, genflow, app shell) sets `UI_MESSAGE_HELP`.

### Text renderer (core, `ui_grid`)

- `UiLineStyle` gains `UI_LINE_HELP` (before `UI_LINE_STYLE_COUNT`), tag
  `h`. `ui_grid_render` puts a help message on the message line with that
  style; a help line too long for the field is cut with `~` as any field
  is (at 40 columns, 37 characters and `~`; at alttp's 50, 47 and `~`).
  Decided over widening the grid: the snapshots keep their 40×15 grid, and
  the grid is a view of the text, not of the layout.

### SDL renderer (`app/sdl/sdl_render.c`) — drawn in this step

- Decided: this step makes the SDL renderer draw the help, since the
  feature is the device's help line, step 12 tunes it with
  `make theme-compare`, and it is a few lines. A `UI_MESSAGE_HELP` message
  is drawn with `[help]`'s font, x, baseline y, right edge (clip and cut
  marker as for the message) and colour when the theme has `[help]`, and
  not drawn at all when it does not. Info and error stay in `[message]`.
- `classic` has no `[help]`, so the 13 goldens stay byte-identical; no
  theme file changes.

### Mockup text check (`tests/rando/sdl/mockup_text.py`, `mockups.sh`)

- The help-line rule changes from "left out, and the app's message must then
  be empty" to "a message-zone text with font-size 10 is the help line; it
  is the mockup's message line" (the "a help line and another message
  text" refusal stays: the message replaces the help). The docstring
  states the new rule.
- Self-test cases follow, as the step's deliberate change (the plan says
  step 9 excepted the help line until step 11): "help line, empty app
  message" now expects `MISMATCH self: message: mockup "Press A to pick"
  app ""`; "help line, app message" expects `mockup "Press A to pick" app
  "Not supported"`; a new case "help line, app help" (the grid's message
  line holding `Press A to pick` with tag `h`) expects no line.
- `mockups.sh`'s summary drops "(help line excepted)" and reads "… $matched
  texts match (help line included)".
- `mockup_states.txt` does not change: 00's keys already end on Glitches =
  None, whose help is the mockup's line; no mockup is edited.

### Docs

- `README.md`: the `[help]` paragraph says the help line shows the selected
  setting value's help, replaced by any message, and `classic` draws none;
  the `make mockups` paragraph says the help line is compared too.
- `CLAUDE.md`: the `make mockups` bullet adds "help line included".
- Step 10's README sentence that the help line is absent from the app until
  step 11 (H-10-2) is removed.

## Implementation Chunks

1. **Core: strings, map, precedence, text renderer, snapshots.** The 63
   strings and their block, `UI_STR_HELP_LEN_MAX` and `ui_str_len_max`,
   `ui_setting_help` / `ui_spoiler_help`, `UI_MESSAGE_HELP`,
   `ui_generate_screen`'s precedence, `UI_LINE_HELP` with tag `h`; the
   unit tests below; every snapshot and expected screen that now shows a
   help line edited by hand (expected: `generate_initial`,
   `generate_page_down`, `generate_page_up`, `generate_pair_not_holding`,
   `generate_restored`, `generate_spoiler_on`, `generate_values_changed`,
   plus whatever `make test` names; each changes in its message line only).
   One commit, its message listing every changed snapshot and stating the
   change as plan 2 step 11's deliberate one. Done: `make test` green, no
   snapshot changed outside the message line. This is the only commit that
   changes an expected screen.
2. **SDL: draw the help.** `draw_message` handles `UI_MESSAGE_HELP` with
   `[help]`; `test_sdl_render` and `test_sdl_alttp` cases below. Done:
   `make sdl-pc` prints `goldens: 13 states byte-identical` and draws the
   alttp states; `make sdl-arm` links.
3. **Mockup check and docs.** `mockup_text.py` rule and self-test,
   `mockups.sh` summary, `README.md`, `CLAUDE.md`. Done: the step
   verification.

Step verification (the plan's), in the pinned environment (a `setup:` line
does not verify): `make test`; `make mockups` ends with all 9 texts
matching with the help line included (00's help line equals the app's
`No glitches required. Standard logic only.`) and `ui_cli` agreeing with
`alttpr` on the 8 theme-page pairings; `make sdl-pc` goldens
byte-identical; `make -k check` green. If `make theme-compare` exists and
runs (H-10-4), the step notes record 00's score before and after chunk 2
(informational) and that its capture shows the help line.

## Testing Decisions

- Tests drive public interfaces: a key in, a `UiScreen` or grid out; a
  table in, a fault out. No test reads a static table.
- `test_ui_strings`: the English table stays sound (`complete` check);
  a help id of 58 characters is sound and 59 is `TOO_LONG`; a non-help id
  of 39 is still `TOO_LONG` (the 38 bound is untouched); `ui_str_len_max`
  of the block's first and last id and of `UI_STR_TAB_GENERATE`.
- `test_ui_generate` (map and precedence):
  - for every setting and value, `ui_setting_help` is inside the block, and
    the 61 ids plus the two spoiler ids are pairwise distinct and number
    exactly `UI_STR_HELP_LAST - UI_STR_HELP_FIRST + 1` (every help string
    belongs to one value, every value has one);
  - initial screen: message kind `UI_MESSAGE_HELP`, text
    `No glitches required. Standard logic only.`;
  - right on Glitches → Overworld's help; down → Item Placement's help;
    Y resets and the help follows; L2/R2 land on a row whose help shows;
  - the Spoiler row shows Off's help, A toggles to On's;
  - the action row has kind `UI_MESSAGE_NONE` (no reason) or the reason;
  - with an unsupported pair holding, the greyed selected row shows the
    reason (info), an enabled selected row shows its help, START on a
    refused action shows the error, and the next key brings the help back;
  - settings unreadable: the info message on the first screen, the help
    after a key.
- `test_ui_grid`: a `UI_MESSAGE_HELP` screen puts its text on the message
  line with style `UI_LINE_HELP` and tag `h`; a 58-character help is cut to
  37 characters and `~` on a 40-column grid.
- `test_ui_history`: list, detail and confirm screens never carry
  `UI_MESSAGE_HELP`.
- `test_theme` (`test_alttp_loads_within_the_app_ranges`): alttp's help
  columns at its help font's px (Press Start 2P advances exactly its size,
  proven by `test_sdl_alttp`) equal `UI_STR_HELP_LEN_MAX`, and every
  English help string is at most that long.
- `test_sdl_alttp`: every English help string's `TTF_SizeText` width in
  the help font is at most `help.right - help.x`.
- `test_sdl_render`: a help screen drawn with `classic` (no `[help]`) is
  pixel-identical to the same screen with no message; drawn with alttp,
  pixels of `help.colour` appear only between `help.x` and `help.right`
  and above `help.y`; an info message of the same text drawn with alttp
  uses `message`'s colour and starts at `message.x` (the two zones are told
  apart by colour; both start at x 30 in alttp, so the width check is the
  10 px against 12 px advance: n characters span n×10 for help, n×12 for
  info).
- Snapshots: edited by hand in chunk 1 only, each in its message line; no
  other expected value changes in the step. Golden BMPs, golden states,
  mockups and themes do not change.
- Prior art: `test_ui_generate`'s reason tests, `test_ui_grid`'s message
  tests, `test_sdl_render`'s feature tests, `test_sdl_alttp`'s
  `test_no_setting_row_is_cut`, `mockup_text.py`'s planted pairs.

## Out of Scope

- Help for History rows, action rows, notices; help in the History detail's
  setting rows.
- Translations; wrapping the help over two lines; scrolling text.
- The help line's look (position, colour, size) beyond the alttp theme's
  existing `[help]` values: step 12. A `[help]` section in `classic`.
- Changes to `rando/`, the settings, their values or names; mockup edits;
  goldens; `make theme-compare` itself.

## Hypotheses about step 10

Step 10 (PRD `docs/prd/triage/phase2-step10-theme-compare.md`) is not done
when this PRD is written. The implementer checks each against step 10's
commits first.

- **H-10-1**: step 10 leaves `mockup_text.py` untouched and `mockups.sh`
  still holds the compare loop and its summary line (only the setup and
  pairing checks move to `mockup_common.sh`). *If the summary or the help
  rule moved*, this step edits them where they now are, with the same
  wording.
- **H-10-2**: step 10's `README.md` paragraph lists the help line as a known
  difference of 00 "absent from the app until step 11". *If present*, that
  clause is removed here; *if absent*, nothing.
- **H-10-3**: step 10 changes no core, `app/sdl/`, theme, snapshot, golden
  or `mockup_states.txt`. *If it did*, this step builds on its version.
- **H-10-4**: `make theme-compare` exists and runs in the pinned
  environment. It is not part of this step's verification; *if it is
  missing or red*, the score record is skipped and the step notes say why.
- **H-10-5**: `goldens.sh --draw DIR --states FILE` keeps step 9's
  interface (step 10 only calls it). *If it changed*, `make mockups` is run
  as the Makefile now runs it.

## Further Notes

- 63 strings, not "about 60": 61 library values (5+2+4+3+7+9+9+4+4+4+3+2+5)
  and Spoiler in History's 2, which is a setting row like the others.
- Why `UI_MESSAGE_HELP` rather than a `help` field: the plan's "the message
  replaces the help" then lives in one place (the core) and is unit-tested;
  a second field would make each renderer re-decide it.
- Why classic draws no help: its goldens show settings rows with no
  message; drawing help there would change them, and goldens are never
  regenerated.
- 00's help line sits at x 34 in the mockup and x 30 in alttp's `[help]`:
  a look difference left to step 12.
- The crystal strings are separate per count (no `{}`) so a translation can
  handle plurals.

## The help strings

Copy exactly; the length column is the character count (all ≤ 58).

| Id | English text | Len |
|---|---|---|
| `UI_STR_HELP_GLITCHES_NONE` | `No glitches required. Standard logic only.` | 42 |
| `UI_STR_HELP_GLITCHES_OVERWORLD` | `Logic may need overworld glitches, like fake flippers.` | 54 |
| `UI_STR_HELP_GLITCHES_HYBRID_MAJOR` | `Overworld glitches plus underworld clips in logic.` | 50 |
| `UI_STR_HELP_GLITCHES_MAJOR` | `Logic may need any major glitch: clips, wraps, YBA.` | 51 |
| `UI_STR_HELP_GLITCHES_NO_LOGIC` | `Items go anywhere. The seed may be impossible to beat.` | 54 |
| `UI_STR_HELP_PLACEMENT_ADVANCED` | `Logic may ask for hard fights with little gear.` | 47 |
| `UI_STR_HELP_PLACEMENT_BASIC` | `Logic gives you more hearts and better gear for fights.` | 55 |
| `UI_STR_HELP_DUNGEON_ITEMS_STANDARD` | `Maps, compasses and keys stay in their own dungeon.` | 51 |
| `UI_STR_HELP_DUNGEON_ITEMS_MC` | `Maps and compasses can be anywhere. Keys stay home.` | 51 |
| `UI_STR_HELP_DUNGEON_ITEMS_MCS` | `Maps, compasses and small keys can be anywhere.` | 47 |
| `UI_STR_HELP_DUNGEON_ITEMS_FULL` | `Maps, compasses, small and big keys can be anywhere.` | 52 |
| `UI_STR_HELP_ACCESSIBILITY_ITEMS` | `Every item can be found, but not every location.` | 48 |
| `UI_STR_HELP_ACCESSIBILITY_LOCATIONS` | `Every location can be reached and its item taken.` | 49 |
| `UI_STR_HELP_ACCESSIBILITY_NONE` | `Only beating the game is sure. Some items may be lost.` | 54 |
| `UI_STR_HELP_GOAL_GANON` | `Climb Ganon's Tower, beat Agahnim, then defeat Ganon.` | 53 |
| `UI_STR_HELP_GOAL_FAST_GANON` | `The pyramid hole is open. Get the crystals, beat Ganon.` | 55 |
| `UI_STR_HELP_GOAL_DUNGEONS` | `Beat all dungeon bosses and both Agahnims, then Ganon.` | 54 |
| `UI_STR_HELP_GOAL_PEDESTAL` | `Pull the Master Sword from its pedestal to win.` | 47 |
| `UI_STR_HELP_GOAL_TRIFORCE_HUNT` | `Find 20 of the 30 Triforce Pieces, then see Murahdahla.` | 55 |
| `UI_STR_HELP_GOAL_GANONHUNT` | `Collect 40 of the 50 Triforce Pieces, then beat Ganon.` | 54 |
| `UI_STR_HELP_GOAL_COMPLETIONIST` | `Collect every item and beat every boss, Ganon last.` | 51 |
| `UI_STR_HELP_CRYSTALS_TOWER_0` | `Ganon's Tower is open from the start.` | 37 |
| `UI_STR_HELP_CRYSTALS_TOWER_1` | `Ganon's Tower opens once you hold 1 crystal.` | 44 |
| `UI_STR_HELP_CRYSTALS_TOWER_2` | `Ganon's Tower opens once you hold 2 crystals.` | 45 |
| `UI_STR_HELP_CRYSTALS_TOWER_3` | `Ganon's Tower opens once you hold 3 crystals.` | 45 |
| `UI_STR_HELP_CRYSTALS_TOWER_4` | `Ganon's Tower opens once you hold 4 crystals.` | 45 |
| `UI_STR_HELP_CRYSTALS_TOWER_5` | `Ganon's Tower opens once you hold 5 crystals.` | 45 |
| `UI_STR_HELP_CRYSTALS_TOWER_6` | `Ganon's Tower opens once you hold 6 crystals.` | 45 |
| `UI_STR_HELP_CRYSTALS_TOWER_7` | `Ganon's Tower needs all 7 crystals, as in the original.` | 55 |
| `UI_STR_HELP_CRYSTALS_TOWER_RANDOM` | `Ganon's Tower needs 0 to 7 crystals, picked at random.` | 54 |
| `UI_STR_HELP_CRYSTALS_GANON_0` | `Ganon can be hurt without any crystal.` | 38 |
| `UI_STR_HELP_CRYSTALS_GANON_1` | `Ganon can be hurt once you hold 1 crystal.` | 42 |
| `UI_STR_HELP_CRYSTALS_GANON_2` | `Ganon can be hurt once you hold 2 crystals.` | 43 |
| `UI_STR_HELP_CRYSTALS_GANON_3` | `Ganon can be hurt once you hold 3 crystals.` | 43 |
| `UI_STR_HELP_CRYSTALS_GANON_4` | `Ganon can be hurt once you hold 4 crystals.` | 43 |
| `UI_STR_HELP_CRYSTALS_GANON_5` | `Ganon can be hurt once you hold 5 crystals.` | 43 |
| `UI_STR_HELP_CRYSTALS_GANON_6` | `Ganon can be hurt once you hold 6 crystals.` | 43 |
| `UI_STR_HELP_CRYSTALS_GANON_7` | `Ganon can be hurt only once you hold all 7 crystals.` | 52 |
| `UI_STR_HELP_CRYSTALS_GANON_RANDOM` | `Ganon needs 0 to 7 crystals, picked at random.` | 46 |
| `UI_STR_HELP_STATE_OPEN` | `Zelda is already rescued. The castle escape is skipped.` | 55 |
| `UI_STR_HELP_STATE_STANDARD` | `Start with the castle escape, as in the original game.` | 54 |
| `UI_STR_HELP_STATE_INVERTED` | `Start in the Dark World. The two worlds swap roles.` | 51 |
| `UI_STR_HELP_STATE_RETRO` | `Arrows cost rupees, keys sold in shops, take-any caves.` | 55 |
| `UI_STR_HELP_WEAPONS_RANDOMIZED` | `Swords are shuffled anywhere in the world.` | 42 |
| `UI_STR_HELP_WEAPONS_ASSURED` | `You start with a sword. The others are shuffled.` | 48 |
| `UI_STR_HELP_WEAPONS_VANILLA` | `Swords are where they are in the original game.` | 47 |
| `UI_STR_HELP_WEAPONS_SWORDLESS` | `No swords. The hammer, medallions and silvers stand in.` | 55 |
| `UI_STR_HELP_ITEM_POOL_NORMAL` | `Every heart and upgrade of the original game.` | 45 |
| `UI_STR_HELP_ITEM_POOL_HARD` | `Fewer hearts, no armor upgrade, Tempered Sword at most.` | 55 |
| `UI_STR_HELP_ITEM_POOL_EXPERT` | `Even fewer hearts, no armor upgrade, Master Sword at most.` | 58 |
| `UI_STR_HELP_ITEM_POOL_CROWD_CONTROL` | `For Crowd Control streams. Some items are left out.` | 51 |
| `UI_STR_HELP_ITEM_FUNCTIONALITY_NORMAL` | `Items work as in the original game.` | 35 |
| `UI_STR_HELP_ITEM_FUNCTIONALITY_HARD` | `Weaker potions, cape and Byrna. Silvers only hurt Ganon.` | 56 |
| `UI_STR_HELP_ITEM_FUNCTIONALITY_EXPERT` | `As Hard, with weaker potions still and no item stuns.` | 53 |
| `UI_STR_HELP_HINTS_ON` | `Telepathic tiles give hints about where items are.` | 50 |
| `UI_STR_HELP_HINTS_OFF` | `No item hints from the telepathic tiles.` | 40 |
| `UI_STR_HELP_HEART_BEEP_OFF` | `The low-health beep is silent.` | 30 |
| `UI_STR_HELP_HEART_BEEP_HALF` | `The low-health beep sounds at half the usual rate.` | 50 |
| `UI_STR_HELP_HEART_BEEP_QUARTER` | `The low-health beep sounds at a quarter the usual rate.` | 55 |
| `UI_STR_HELP_HEART_BEEP_DOUBLE` | `The low-health beep sounds at twice the usual rate.` | 51 |
| `UI_STR_HELP_HEART_BEEP_NORMAL` | `The low-health beep sounds as in the original game.` | 51 |
| `UI_STR_HELP_SPOILER_OFF` | `No spoiler is kept. History shows the settings only.` | 52 |
| `UI_STR_HELP_SPOILER_ON` | `The spoiler is kept and shown in this seed's History page.` | 58 |
