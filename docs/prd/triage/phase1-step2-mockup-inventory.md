---
title: "Mockup inventory and the core changes it calls for"
description: "The UI core exposes too little for a theme to draw the mockups: the footer is a flat string, and the red titles, highlighted file names and prose pages have no field to style them by."
status: needs-triage
---

## Problem Statement

Plan 2 moves the SDL app's look into a theme drawn from `UiScreen`. Before the
theme format (step 3) is designed, every element of the owner's nine mockups
(`design/mockups/*.svg`) has to be traced to where it will come from: the
background image (`frame.png`), text placed in a zone, a sprite, or a style
variant chosen by a `UiScreen` field. Nobody has made that list, so step 3
could design a format that cannot express a mockup, and step 5 could find a
mockup element the core never tells the renderer about.

Some elements already have no source in the core today:

- The footer is one flat string (`"</> Change  Y Default  START Generate"`).
  A theme cannot draw the mockups' button sprites (d-pad, A, B, X, Y,
  START) from it, nor the yellow "Generate" label of the primary hint.
- Generation Failed and the delete confirmation have a red title; every other
  title is cyan. Nothing in `UiScreen` tells them apart.
- Seed Ready and the delete confirmation show the seed's file name yellow and
  indented; nothing marks that row.
- Generating, Seed Ready, Generation Failed, the empty History and the delete
  confirmation are short prose pages drawn in larger text on taller rows than
  the lists; nothing tells a renderer which kind of body it is drawing.

## Solution

1. The inventory: every element of the nine mockups, mapped to its source and
   to the existing or new `UiScreen` field that drives it, written below
   (Further Notes) and copied into the plan's step notes, so step 3's format
   is checked against it.
2. The core changes the inventory calls for, all in `app/core/`:
   - **Structured footer**: `UiScreen.footer` becomes a list of hints, each a
     button (an enum), the button's key text and a label (both from the
     translation table), and a primary flag. Every screen builds its hints.
     The text renderer joins them back into exactly today's strings, so no
     snapshot and no `ui_cli` output changes.
   - **Title tone**: normal or alert; alert on Generation Failed and on the
     delete confirmation.
   - **Row emphasis**: a flag on a row; set on the file-name row of Seed
     Ready and of the delete confirmation.
   - **Body kind**: list or notice; notice on the five prose pages.
   The text renderer ignores the three style fields, so its output is
   byte-identical; the SDL app, which still draws through the text grid,
   gives the same pixels (step 1's goldens prove it).

## User Stories

1. As the agent writing the theme format (step 3), I want every mockup element mapped to frame, zone text, sprite or a `UiScreen`-driven variant, so that I design a format that can express all nine mockups.
2. As the agent writing the theme format, I want the inventory to name each colour, font size, row height and position the mockups use, so that I can check each against a key of the format.
3. As the agent writing the theme format, I want the inventory to say which elements are conditional (the footer band only when there are hints, the scroll arrows only when there is more), so that the format can express a zone background drawn only when the zone has content.
4. As the agent writing the new renderer (step 5), I want every style variant to be driven by a field of `UiScreen`, so that the renderer never guesses a style from a string's content.
5. As the agent writing the new renderer, I want the footer as a list of hints with a button enum, so that I can draw each button's sprite.
6. As the agent writing the new renderer, I want a primary flag on a hint, so that the theme can draw the "Generate" label yellow as the mockups do.
7. As the agent writing the new renderer, I want the hints' key text alongside the enum, so that a text renderer needs no table and a sprite renderer needs no text.
8. As the agent writing the new renderer, I want an alert title tone, so that Generation Failed and the delete confirmation get their red title from the theme.
9. As the agent writing the new renderer, I want an emphasis flag on a row, so that the seed's file name on Seed Ready and the delete confirmation is drawn yellow and indented.
10. As the agent writing the new renderer, I want a body kind (list or notice), so that the prose pages use the larger text and taller rows of the mockups and the lists use the 12 px rows.
11. As the agent writing the new theme (phase 2), I want the inventory to record the mockups' own inconsistencies (the tab slots' text positions), so that the theme reproduces them instead of the code assuming a regular layout.
12. As the agent making the mockups true (step 9), I want the inventory to list the mockups' filler content (settings and file names the app does not have), so that I know what to edit.
13. As the agent adding the help line (step 11), I want the inventory to place the help line in the message zone with its own font size, so that I know where the help goes and that this step does not add it.
14. As the owner, I want the footer's text unchanged on every screen, so that the text renderer, `ui_cli` and the snapshots still show `</> Change  Y Default  START Generate`, `A Back`, `A Details  X Delete`, `B Back  X Delete` and `A Delete  B Keep`.
15. As the owner, I want the SDL app to look exactly as it does today after this step, so that the theme engine is proven before the look changes.
16. As a translator, I want every button key text and hint label in the translation table, so that the footer can be translated label by label.
17. As a translator, I want "Generate" as a hint label to be its own string, distinct from the tab name, so that the two can be translated differently.
18. As the agent, I want the five whole-footer strings removed from the table once nothing uses them, so that the table holds no dead entry.
19. As the agent, I want a screen with an invalid hint (count out of range, unknown button, two primary hints) refused by `ui_screen_valid`, so that the renderers keep their "invalid screen draws blank" contract.
20. As the agent, I want an invalid title tone or body kind refused by `ui_screen_valid`, so that a renderer indexing a style table by them cannot read out of range.
21. As the agent, I want `ui_screen_init` to give no hints, the normal tone and the list body, so that a screen that sets nothing renders as today.
22. As the agent, I want a test proving the text renderer ignores tone, emphasis and body kind, so that a later change cannot silently make the snapshots depend on them.
23. As the agent, I want a test proving the joined footer is cut with `~` at the field's end exactly as the flat string was, so that the small-grid snapshot still matches.
24. As the agent, I want each screen builder's hints, tone, emphasis and body kind checked by its own unit test, so that a screen that forgets one is caught in `make test`.
25. As the agent, I want the Generate tab's START hint primary even while the Generate Seed action is greyed, so that the footer matches mockup 01.
26. As the agent, I want the empty History and the Generating screen to keep having no hints, so that the theme draws no footer band on them, as mockups 02 and 06 show.
27. As the plan pilot, I want `make test` green with no snapshot changed, so that the step is verified the way the plan asks.
28. As the plan pilot, I want step 1's golden comparison run by `make sdl-pc` and passing, so that the step is proven not to change a pixel.
29. As the plan pilot, I want `make lint` green, so that the new types stay pure C99 under `-pedantic-errors`.
30. As the plan pilot, I want `no_literals.py` green, so that the button key texts ("START", "A"…) live only in the English table.
31. As the plan pilot, I want the inventory copied into the plan's step notes, so that step 3's PRD reads it from the plan.
32. As a maintainer, I want the SDL tests that build a `UiScreen` by hand moved onto hints, so that `make sdl-pc` builds after the change.

## Implementation Decisions

- **Hints replace the footer string.** `UiScreen` loses `footer` and gains a
  fixed array of hints with a count (at most 4; the most any screen uses
  is 3). A hint holds:
  - `button`: a new enum of footer buttons, separate from `UiButton`
    because the "</>" hint is the left/right pair, drawn as one d-pad sprite:
    left/right, A, B, X, Y, START (plus a count).
  - `key`: the button's text for text renderers ("</>", "A", "START"…),
    resolved from the translation table by the screen builder.
  - `label`: the action's text ("Change", "Back"…), from the table.
  - `primary`: at most one hint per screen.
  The key text travels in the hint (rather than being looked up by the text
  renderer from the enum) because the grid renderer has no string table and
  must not gain one, and because the grid's own unit tests feed arbitrary
  key texts ("L/R") that no enum value names.
- **A helper builds a hint from the table**: given the table, a button, a
  label id and the primary flag, it returns the hint with the button's key
  text filled in. It lives in the strings module (which already owns the
  table and `ui_str`), so the screen builders stay one call per hint. A
  helper appends a hint to a screen, refusing past the maximum, like
  `ui_screen_add_row`.
- **New strings**, all arity 0, in the string list and the English table:
  six key texts ("</>", "A", "B", "X", "Y", "START") and seven labels
  ("Change", "Default", "Generate", "Back", "Details", "Delete", "Keep").
  The five whole-footer strings (Generate, genflow, History list, detail,
  confirm) are removed.
- **Hints per screen** (the text in brackets is what the text renderer
  prints, identical to today):
  - Generate tab: left/right Change, Y Default, START Generate (primary)
    [`</> Change  Y Default  START Generate`]; START stays primary when the
    action is greyed.
  - Seed Ready and Generation Failed: A Back [`A Back`]; Generating: none.
  - History list with seeds: A Details, X Delete [`A Details  X Delete`];
    empty History: none.
  - History detail: B Back, X Delete [`B Back  X Delete`].
  - Delete confirmation: A Delete, B Keep [`A Delete  B Keep`].
- **Text renderer footer**: the footer line is drawn when the hint count is
  above zero (today: when the footer string is not empty). It writes, with
  the same pen as today's field, each hint's key, one space, its label, and
  two spaces between hints, then cuts with `~` at the field's end as
  `put_field` does. A NULL key or label draws as empty, like every other
  text. The primary flag does not change the text or the line style.
- **Title tone**: a new enum on `UiScreen`, normal or alert. Alert on
  Generation Failed and the delete confirmation; normal everywhere else,
  the History detail (file name as title) included.
- **Row emphasis**: a new boolean on `UiRow`, false by default. Set on the
  file-name row of Seed Ready and the file-name row of the delete
  confirmation; nowhere else (the History list's file names are plain rows).
- **Body kind**: a new enum on `UiScreen`, list or notice. Notice on
  Generating, Seed Ready, Generation Failed, the empty History and the
  delete confirmation; list on the Generate tab, the History list with seeds
  and the History detail. The body kind does not change the page size the
  core scrolls by; the notice pages hold at most 3 rows and never scroll.
- **What is not a new field**, decided from the inventory:
  - Whether a list reserves the cursor column (labels at x=60 in mockups 01
    and 05, x=30 in 07) is derived from `cursor >= 0`.
  - The selected setting's ◀ ▶ arrows follow today's grid rule (selected,
    a setting, not disabled).
  - The dashed rule after the title is drawn from the title text's rendered
    width; no field.
  - The spoiler's indent is the core's leading spaces (one space is 12 px at
    12 px Press Start 2P, matching mockup 07's 12 px steps).
  - The footer band is drawn only when there are hints; no field beyond the
    count.
  - The help line (mockup 00) is step 11's; no field now.
- **Validity**: `ui_screen_valid` also refuses a hint count out of range, a
  hint button out of range, more than one primary hint, a title tone out of
  range and a body kind out of range. `ui_screen_init` sets no hints, the
  normal tone and the list body.
- **The text renderer ignores tone, emphasis and body kind**: no new
  `UiLineStyle`, no new style tag. The SDL app still draws through the grid
  in this step (hypothesis H-1-4), so its pixels cannot change.
- **The UiRow initialisers** in the screen builders are updated for the new
  member (the build has `-Wextra -Werror`, which flags a missing field
  initialiser).
- **The inventory** (Further Notes) is copied verbatim into the plan's step
  notes (section 6) under step 2, in the last chunk, with the new fields'
  final names.

## Implementation Chunks

1. **Structured footer.** The hint type and button enum, the hints array in
   `UiScreen` replacing the footer string, the add helper, the table helper,
   the new strings and the removal of the five footer strings, every screen
   builder on hints, the text renderer's join, validity, and the tests that
   built `footer` by hand (core unit tests and the SDL tests under
   `tests/rando/sdl/`) moved onto hints with the same texts. Done:
   `make test` green with no file under `tests/rando/unit/snapshots/`
   changed; `make lint` green; `make sdl-pc` green with step 1's comparison
   passing (H-1-1).
2. **Style fields.** Title tone, row emphasis, body kind; set by the screen
   builders as decided; validity; the renderer-ignores-them test. Done: same
   checks as chunk 1.
3. **Inventory in the step notes.** The Further Notes inventory, with the
   final field names, appended under step 2 in the plan's step notes. Done:
   each of the nine mockups has its section; `make test` green.

## Testing Decisions

- A good test drives a public interface and checks what comes out: the
  `UiScreen` a builder produces, the grid text a screen renders to, what
  `ui_screen_valid` accepts. No test reads a builder's internal state.
- **Snapshots are the regression proof**: no file under
  `tests/rando/unit/snapshots/` changes in this step. A snapshot test whose
  input built `footer` gets the same text as hints (e.g. key "A", label
  "Select"; key "L/R", label "Tab"); its input changes form, its expected
  file does not. `smoke_ui_cli` and its expected output unchanged.
- **`test_ui_screen`**: `ui_screen_init`'s new defaults; the add-hint helper
  refuses past the maximum; validity refuses each new bad case and accepts
  one primary hint and zero primary hints.
- **`test_ui_grid`**: hints join to the old strings; a footer longer than
  the field is cut with `~` exactly as the flat string was (the 24×8
  snapshot covers it); no hints, no footer line; NULL key and label draw as
  empty; a screen rendered with alert tone, an emphasised row and the notice
  body gives the same grid text as without them.
- **`test_ui_strings`**: the new strings pass `ui_strings_check` (it already
  checks the whole table); the hint helper fills the key text of every
  button.
- **Screen builders** (`test_ui_generate`, `test_ui_genflow`,
  `test_ui_history`, prior art their existing screen checks): each screen's
  hints (buttons, labels, primary flag, count), title tone, emphasis rows
  and body kind as decided; START primary with the action greyed; no hints
  on Generating and on the empty History.
- **SDL tests** (`tests/rando/sdl/`): only moved onto hints; their
  expectations unchanged.
- **Pixel identity**: step 1's golden comparison in `make sdl-pc`, really
  run (a `setup:` line does not verify, plan rule), in step 1's pinned
  environment (H-1-3).
- **Static checks**: `make lint`, `tests/rando/no_literals.py` and
  `tests/rando/no_sdl.sh`, all through `make test` / `make check`.
- The inventory itself is not tested: it is a document, checked by review
  against the nine mockups (one section each).

## Out of Scope

- The theme format, `theme.ini`, `classic`, the loader (step 3).
- Page size and width from the theme (step 4).
- Any change to what the SDL app draws, the palette, `sdl_view_draw`, or
  any new `UiLineStyle` (steps 5 and 6).
- The help line and its strings (step 11).
- Editing the mockups (step 9); the filler is only listed.
- New settings, translations other than English, `rando/`.

## Hypotheses about step 1

Step 1 (`docs/prd/triage/phase1-step1-sdl-goldens.md`) is being implemented
while this PRD is written. Everything this PRD assumes about its outcome:

- **H-1-1**: `make sdl-pc` runs the golden comparison after the SDL tests
  and the smoke test, prints `goldens: 13 states byte-identical` on success,
  and on failure one `DIFFERENT <state>`, `MISSING <state>` or
  `EXTRA <file>` line per problem, exiting non-zero.
- **H-1-2**: the goldens and their environment fingerprint are committed
  under `tests/rando/sdl/goldens/`, one BMP per state of step 1's 13-state
  table, and a fingerprint mismatch gives one
  `setup: goldens pinned to <fp>, this host is <fp>` line.
- **H-1-3**: the pinned environment is the Claude Code cloud container
  (Ubuntu 24.04.5 LTS, sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11,
  FreeType 2.13.2), where `make sdl-pc`'s prerequisites are installed.
- **H-1-4**: step 1 did not change how the SDL app draws: `sdl_loop` still
  renders `UiScreen` into a `UiGrid` with `ui_grid_render` and draws the
  grid with `sdl_view_draw`, so the footer pixels come from the text
  renderer's footer line.
- **H-1-5**: step 1's state table checks the captured frame's text
  (`--shot-text`) for strings that include the footers `B Back  X Delete`
  and `A Delete  B Keep`; this step keeps those texts, so the table needs no
  edit.
- **H-1-6**: step 1's shared fixed-run options module (in `app/sd/`) and
  `ui_cli`'s move onto it do not read or write `UiScreen.footer`.
- **H-1-7**: tests that step 1 added under `tests/rando/sdl/` (loop, smoke)
  and that build a `UiScreen` by hand with `footer` are moved onto hints in
  chunk 1 like `test_sdl_view.c`; if step 1 added none, nothing to move.
- **H-1-8**: step 1 recorded its environment in the plan's step notes
  (section 6); this step's inventory is appended after it, under its own
  step 2 heading.

## Further Notes

### The inventory

Sources: **F** `frame.png` (static, never driven by the code); **Z** text in a
zone; **S** a sprite; **V** a style variant chosen by a `UiScreen` field
(existing, or *new* in this step); **T** a theme constant (position, size,
colour) the format must express. Coordinates are the mockups' (640×480).
The 12 px mockups (01 to 08) are the reference; 00 is used for its help line
and for the selected setting row, which no 12 px mockup shows.

#### Common to every mockup

| Element | Source | Detail |
|---|---|---|
| Screen background `#0c0c34` | F | full 640×480 |
| Outer white frame, notched corners | F | `#f8f8f8`, 6–634 × 6–474 |
| Blue border, notched corners | F | `#3858d8`, 10–630 × 10–470 |
| Panel gradient | F | `#10106c` → `#04043c`, 14–626 × 14–466 |
| Tab separator bar | F | `#3858d8`, x14 y54 612×4 |
| Three hearts | F | x544, 571, 598, y27, pixel heart ×3; decoration |
| Tab labels | Z, V (`tabs`, `active_tab`) | 16 px, baseline 42; active `#f8d038`, inactive `#98a8f0` |
| Active tab cursor | S, V (`active_tab`) | triforce, `#f8d038`, ×1.25 (20×18), y27 |
| Tab slot positions | T | slot 0: cursor x30, text x58 active, **x30 inactive**; slot 1: cursor x186, text x214 active and inactive. The mockups are irregular here: the format must allow a text x per slot and per state |
| Title | Z, V (`title`, *new* title tone) | 14 px, x30, baseline 82; normal `#78d8f8`, alert `#ff6868` |
| Title rule | T | dashed `#3858d8`, 4 px, dash 8 gap 4, y77, from the title text's end + 12 to x610; drawn from the title's rendered width |
| Message line | Z, V (`message_kind`) | 12 px, x30, baseline 412; info `#78d8f8`, error `#ff6868` |
| Footer band | T, V (hint count > 0) | `#08084c` x14 y422 612×44, top bar `#3858d8` 4 px; absent on 02 and 06 (no hints): a zone background drawn only when the zone has content |
| Hint sprite | S, V (*new* hint button) | at the hint's x, y433; d-pad 22×22 white cross with `#08084c` notches; A/B/X/Y 22×22 `#f8d038` notched square, letter `#08084c` 12 px inside the sprite; START 66×22 `#e83838` pill, "START" 10 px `#f8f8f8` inside the sprite |
| Hint label | Z, V (*new* hint label, primary) | 12 px, baseline 448, at sprite x + 32; `#f8f8f8`, primary `#f8d038` |
| Hint flow | T | first hint x30; next hint at previous label's end + 36 (01: 30, 170, 322; 05: 30, 182; 07: 30, 146) |

#### List body (01, 05, 07) — *new* body kind: list

| Element | Source | Detail |
|---|---|---|
| Rows | Z | 12 px, 24 px rows, first row top y98 (baseline 114); 12 rows visible in 01 |
| Label column | T, V (`cursor >= 0`) | x60 with a cursor (01, 05), x30 without (07) |
| Label / value | Z, V (row kind, `disabled`) | label `#f8f8f8`; value right-aligned at x584, `#b8e8f8`; disabled label and value `#7078b0` |
| Text row in a list | Z, V (row kind text) | as a label, `#f8f8f8` (07's spoiler lines) |
| Spoiler indent | Z | the core's leading spaces, 12 px each (07: x30, 42, 54) |
| Selected band | T, V (`cursor`) | `#2040c0` x22 596×24, 2 px `#6888f8` lines at top and bottom |
| Row cursor | S, V (`cursor`) | triforce 16×14 at x32, row top + 5; `#f8d038`; `#bcc4ee` when selected and disabled |
| Selected text | V (`cursor`, `disabled`) | `#f8d038` (05); selected and disabled `#bcc4ee` (01, the greyed Generate Seed; the plan extends it to a greyed selected setting) |
| Value arrows ◀ ▶ | S, V (selected, setting, not disabled) | from 00 (16 px): ◀ left of the value, ▶ after its right edge (594–606); `#f8d038`; scaled to the 12 px rows by the theme |
| Scroll up ▲ | S, V (`more_above`, scroll) | 10×6 `#f8d038`, x606–616 on the first row (01, 07) |
| Scroll down ▼ | S, V (`more_below`, scroll) | not drawn in any 12 px mockup; 00 puts it at the help line's right end (586–606), where a 57-character help line would reach it; decided: mirrored on the last body row like ▲ |

#### Notice body (02, 03, 04, 06, 08) — *new* body kind: notice

| Element | Source | Detail |
|---|---|---|
| Rows | Z | 14 px, 32 px rows, baseline 120 then +32, x30, `#f8f8f8`; at most 3 rows |
| Emphasised row | Z, V (*new* row emphasis) | `#f8d038`, x50 (03 and 08 file names) |

#### Per mockup

- **00 Settings with help** (16 px, not the reference): kept only for the
  help line (10 px, `#78d8f8`, x34, baseline 418, in the message zone, step
  11; the message replaces it) and for the selected setting row's arrows
  and yellow label and value. Its 28 px rows and spread-out footer are the
  16 px variant, ignored.
- **01 Settings, greyed and error**: list body, cursor on the greyed
  Generate Seed (selected-disabled band, greyed cursor and text), World
  State greyed, ▲, error message, hints left/right Change, Y Default,
  START Generate (primary, yellow while the action is greyed). Filler:
  Entrance Shuffle, Boss Shuffle, Enemy Shuffle, Enemy Damage, Enemy Health
  are not app settings (step 9).
- **02 Generating**: notice body, two rows, no message, no hints, no footer
  band.
- **03 Seed Ready**: notice body, emphasised file name, info message
  "Play it from the SNES list", hint A Back. Filler: `E76BF9` is not the
  synthetic base's seed name (step 9).
- **04 Generation Failed**: alert title, notice body (the advice row),
  error message, hint A Back.
- **05 History list**: list body with cursor, file-name rows, selected row
  yellow, info message (the date line), hints A Details, X Delete. Filler:
  the file names other than `Open-Ganon-1CA10A.sfc` (step 9).
- **06 History empty**: notice body, two rows, no hints, no footer band.
- **07 Seed detail with spoiler**: list body without cursor (labels x30),
  setting rows and spoiler text rows with indents, ▲, normal-tone title
  (the file name), hints B Back, X Delete. Filler: the four Enemizer
  settings (step 9).
- **08 Delete confirmation**: alert title, notice body, emphasised file
  name, hints A Delete, B Keep.

#### Fonts and sizes the format must express

Press Start 2P at 16 px (tabs), 14 px (title, notice rows), 12 px (list rows,
message, hint labels), 10 px (help line, phase 2); the START and A/B/X/Y
letters are inside their sprites. Row heights: 24 px (list), 32 px
(notice). Everything a per-zone font, size, colour, row height and
position.

### Other notes

- The mockups' tab layout is irregular (inactive "Generate" at x30 but
  inactive "History" at x214); the plan forbids changing the mockup's look to
  suit the code, so the format, not the mockup, absorbs it.
- The selected setting row in the 12 px set is not drawn by any mockup; the
  theme scales 00's to 24 px rows (step 12).
