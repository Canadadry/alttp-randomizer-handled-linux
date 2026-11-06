---
title: "The new theme's geometry"
description: "The app has only the classic theme, so the page size, the wrap width and the help line's length of the mockups' 12 px look are not settled yet, and steps 9 and 11 have nothing to check against."
status: needs-triage
---

## Problem Statement

Phase 1 left the SDL app drawing every screen from a theme folder, but the
only real theme is `classic`, today's 40×15 grid look. The owner's mockups
(`design/mockups/*.svg`) use another geometry: Press Start 2P at 16, 14, 12
and 10 px, 24 px list rows from y 98, 32 px notice rows, a message line at
baseline 412, a footer band at y 422. Until a theme carries that geometry:

- nobody knows how many list rows a page holds in the new look, or how wide
  the core must wrap a spoiler line, so step 9 cannot give `ui_cli` the grid
  that matches the new look, and its text check has no target;
- the help line (step 11) has no zone, no font and no length, so its unit
  test has no bound to check every help string against;
- Press Start 2P is not in the repository, so neither the app nor the
  mockups (step 9's local `@font-face`) can load it offline;
- the format has no place for the help line at all: the message zone has one
  font (12 px), and the help line is 10 px in another colour.

## Solution

1. **Press Start 2P is committed** under `data/` as `PressStart2P.ttf` with
   its licence as `PressStart2P-LICENSE.txt`, byte copies of
   `ofl/pressstart2p/PressStart2P-Regular.ttf` and `ofl/pressstart2p/OFL.txt`
   of the `google/fonts` GitHub repository at commit
   `24ddc1f324cac5d9f5aedca126d298f56e3f1640`, beside DejaVu Sans Mono.
2. **The format gains an optional `[help]` section**: the help line's font,
   position, right edge and colour. A theme without it has no help line;
   `classic` has none, so nothing it draws changes now or in step 11.
3. **A new theme `themes/alttp/`** (the plan's name, kept) holds a
   `theme.ini` whose zones, fonts, sizes and colours come from the 12 px
   mockups (step 1.2's inventory, as already mapped by the test fixture
   `tests/rando/unit/themes/inventory/theme.ini`), the help line from
   `00_settings_help.svg`, and byte copies of Press Start 2P and its
   licence. No `frame.png`, no sprite: the screen is the mockups' plain
   background colour, the tab separator is a plain band, and text markers
   (`>`, `<`, `^`, `v`) and the buttons' own key text stand in for the
   sprites until step 12.
4. **The theme fixes three numbers** the later steps check against: a page
   of **12** list rows, a wrap width of **48** columns, a help line of
   **58** characters. They are derived from the theme (no test pins them),
   and recorded in the step notes.
5. **`make sdl-pc` draws every state with it**: the 13 states of
   `golden_states.txt` are played into the app with `--theme themes/alttp`
   and their captures written under `tests/rando/build/sdl/`, after the
   classic golden comparison, which is unchanged.

## User Stories

1. As the owner, I want a theme folder that already has the mockups' layout, so that the new look can be judged and tuned on real screens rather than on a grid.
2. As the owner, I want to run the PC build with `--theme themes/alttp` and see every screen laid out like the mockups (zones, rows, footer), so that I can check the geometry before the frame and sprites exist.
3. As the owner, I want Press Start 2P in the repository with its licence, so that the font the mockups use is the font the app ships, with its terms kept.
4. As the owner, I want the font taken from a named commit of a known source, with its sha256 recorded, so that I can check the file I ship is the upstream one.
5. As the owner, I want the theme to need no rebuild to change, so that the iteration loop the plan promises starts with this theme.
6. As the owner, I want the 12 px mockups (01 to 08) to be the reference and only the help line taken from `00_settings_help.svg`, so that the size I chose is the one the app uses.
7. As the owner, I want going back to 16 px to stay a theme edit, so that the choice of size is never in code.
8. As the owner, I want the classic look, its goldens and the zip unchanged by this step, so that the proven phase-1 rendering stays proven.
9. As the owner, I want a selected setting's label and value never cut in the new theme, for every setting and every value the app has, so that the longest value ("Master Sword Pedestal") shows whole.
10. As the owner, I want a spoiler line wrapped to the width the new list really holds, so that no line runs past the list's right edge or under the scroll marker.
11. As the owner, I want the scroll markers kept clear of the list text, so that a full-width line and a marker never overlap.
12. As the owner, I want the help line to sit in the message zone, so that a message replacing it (step 11) takes the same place.
13. As the owner, I want screens without hints (Generating, empty History) drawn without the footer band, as the mockups show, so that the stand-in already behaves like the final look.
14. As the agent of step 9, I want the new theme's page and wrap width fixed and derivable from the theme, so that I can give `ui_cli` the matching `--lines` and `--cols` and check its text against the mockups.
15. As the agent of step 11, I want the help line's zone, font and width in the theme, so that the help strings' length bound comes from the theme and not from a number written in a test.
16. As the agent of step 11, I want a proof that every printable ASCII glyph of Press Start 2P advances by exactly its size, so that a pure C test in `make test` can count characters as pixels.
17. As the agent of step 11, I want `classic` to have no help line, so that adding the help text does not change any classic golden.
18. As the agent of step 12, I want the stand-in values kept where the sprites will need them (the footer's sprite top, the cursor and scroll columns), so that swapping a text marker for a sprite is a short edit.
19. As the agent of step 12, I want the stand-ins confined to markers, buttons, the separator band and their y offsets, so that tuning the look does not move the page, width or help length.
20. As the agent of step 10, I want every state's capture under the new theme written by `make sdl-pc`, so that I know each state renders before I compare it with its mockup.
21. As the agent of step 13, I want the new theme to be a flat folder with its own font and licence copies, so that the zip builder of step 6 can ship it as the default unchanged.
22. As the plan pilot, I want the theme's loading checked by `make test`, so that a broken `themes/alttp/theme.ini` fails the fast loop.
23. As the plan pilot, I want the theme's fonts opened and measured by `make sdl-pc`, so that a wrong or missing font file fails where SDL_ttf is available.
24. As the plan pilot, I want `make sdl-pc` to fail when any state cannot be drawn with the new theme, so that "draws every state" is a check, not a claim.
25. As the plan pilot, I want the classic golden comparison to keep running first and unchanged, so that the step cannot weaken phase 1's proof.
26. As the plan pilot, I want the theme's copies of Press Start 2P and its licence checked byte-identical to `data/`'s, so that the theme and the mockups never use two different files.
27. As the plan pilot, I want no test to pin the new theme's colours or positions, so that steps 12 and 13 edit the theme without editing a test, as the plan's goal requires.
28. As a maintainer, I want the `[help]` section to follow the loader's rules (unknown, missing and out-of-range keys refused with file, line and key), so that the strict loader stays strict.
29. As a maintainer, I want the `[help]` section optional like `title.rule` and the bands, so that a theme without a help line is valid and says so by leaving it out.
30. As a maintainer, I want `README.md` and `CLAUDE.md` to name the new theme, the new font and where it came from, so that the next session reads the right layout.
31. As a maintainer, I want the test-only inventory fixture left as it is, so that step 1.2's record keeps meaning what it meant.
32. As a maintainer, I want no SDL include and no letter literal added where the static checks forbid them, so that `no_sdl.sh` and `no_literals.py` still hold.

## Implementation Decisions

### The font

- **Source**: `https://github.com/google/fonts`, commit
  `24ddc1f324cac5d9f5aedca126d298f56e3f1640` (the `main` head on
  2026-11-06), folder `ofl/pressstart2p/`. Raw URLs:
  `https://raw.githubusercontent.com/google/fonts/24ddc1f324cac5d9f5aedca126d298f56e3f1640/ofl/pressstart2p/PressStart2P-Regular.ttf`
  and the same folder's `OFL.txt`. The folder's `METADATA.pb` names the file
  `PressStart2P-Regular.ttf`, designer CodeMan38, licence OFL, copyright
  "Copyright 2012 The Press Start 2P Project Authors (cody@zone38.net), with
  Reserved Font Name "Press Start 2P"".
- **Verification** (by the implementer, recorded in the commit message):
  - `PressStart2P-Regular.ttf`: 118204 bytes, sha256
    `034c77f1f05ec89421e4a63f0e3a4ca1ecf852cc6d2bf611f126f275728e017d`;
  - `OFL.txt`: 4413 bytes, 93 lines, sha256
    `705960c3281a5765ecc0b59bd4ed7ca59eed165748076bc2fc3e8fdbfeb944b0`; it
    starts with the copyright line above and holds "SIL OPEN FONT LICENSE
    Version 1.1 - 26 February 2007";
  - the same hashes were seen on `main` and at the pinned commit when this
    PRD was written. A file with any other hash is not committed: the
    implementer stops on it (an upstream file is never "close enough").
  - Functional check, in the pinned environment (SDL_ttf 2.0.11, FreeType
    2.13.2), measured when this PRD was written and asserted by the SDL
    test below: at 10, 12, 14 and 16 px every glyph 0x20–0x7E advances by
    exactly the size, the ascent equals the size, the descent is 0.
- **Names**: `data/PressStart2P.ttf` and `data/PressStart2P-LICENSE.txt`,
  following DejaVu's `DejaVuSansMono.ttf` and `DejaVuSansMono-LICENSE.txt`
  (and the inventory fixture's `PressStart2P.ttf`). Renaming the file is not
  a modification under the OFL; the font is shipped unmodified, so the
  Reserved Font Name is respected. `themes/alttp/` holds byte copies of both
  (themes are flat and self-contained, as `classic` is, H-6-4), so the zip
  of step 13 ships the licence beside the font.
- `data/`'s copies are the source the theme copies and the mockups (step 9)
  load; nothing in this step ships them (the zip still ships `classic`,
  H-6-5).

### The format: an optional `[help]` section

- **Section** `[help]`, optional (arity like `title.rule`, `tabs.band`,
  `footer.band`), placed in the format's table order right after
  `[message]` (that order is the order missing keys and sections are
  reported in).
- **Keys**, all required when the section is present: `font` (a font name,
  resolved like every other `font` key, "unknown font" otherwise), `x`
  (0..640), `y` (0..480, the text's y through the font's anchor), `right`
  (0..640), `colour`. Unknown and duplicate keys are refused as everywhere.
- **Cross-check**, in the end-of-file checks after the list's: `x` must be
  below `right`, else a range failure at `x`'s own line, worded like
  `list.label_x`'s (`help.x: … 0..<right-1>`).
- **Struct**: `Theme` gains `ThemeHelp help` (`present`, `font` index, `x`,
  `y`, `right`, `colour`); absent, every field is zero and `present` false.
- **Function**: `theme_help_columns(theme, advance)`: whole characters of
  `advance` px (≥ 1) between `help.x` and `help.right`; 0 when the section
  is absent. The twin of `theme_columns`, used by step 11's bound.
- `theme_assets` needs no change: the help font is one of `Theme.fonts` and
  is listed (and opened by `sdl_render_open`) like the others.
- **The renderer does not draw the help line in this step**: the core has
  no help yet (step 11). It opens the help font like any other.
- `classic` gets no `[help]`, decided so that step 11's help text leaves
  every classic golden byte-identical (classic draws no help line). The
  inventory fixture is not changed either.

### `themes/alttp/theme.ini`

Taken from the inventory fixture (which maps step 1.2's inventory to keys),
with the changes stated here; every coordinate is the mockups' (640×480).

- `[screen]`: `colour = #0c0c34` (the mockups' background), `image = none`
  (no `frame.png` until step 12), `cut = text:~`.
- **Fonts**: `ps2p_16`, `ps2p_14`, `ps2p_12`, `ps2p_10`, each `file =
  PressStart2P.ttf`, `size` its number, `cell = none`, `anchor = baseline`.
- `[tabs]`, `[tab.1]`, `[tab.2]`: as the fixture (16 px, baseline 42,
  colours `#98a8f0` / active `#f8d038`, slot x values per state, right 540
  before the hearts), except `active_before = text:>` and `marker_y = 42`
  (a text marker's y is a baseline, a sprite's its top).
- `[tabs.band]`, new: the tab separator as a plain band, x 14, y 54, w 612,
  h 4, colour `#3858d8`, no lines. It stands in for that part of
  `frame.png`; step 12 decides whether it stays.
- `[title]`, `[title.rule]`: as the fixture (14 px, x 30, baseline 82,
  `#78d8f8`, alert `#ff6868`; dashed rule y 77, 4 px, dash 8 gap 4, offset
  12, end 610).
- `[list]`: as the fixture (12 px, top 98, bottom 386, rows 24, text_y 16,
  label_x 30, label_x_cursor 60, value_right 584, gaps 12/10/10, colours,
  cursor_x 32, scroll_x 606), except:
  - `right = 606` (fixture 600): the text column ends where the scroll
    markers start, so a full-width line never runs under ▲/▼, and
    (606 − 30) / 12 = 48 columns exactly; 07's longest spoiler line in the
    mockup (one indent and 47 characters, x 42 to 606) fits on one row;
  - `value_max = 264` (fixture 240): 22 characters, so "Master Sword
    Pedestal" (21, the longest value) is never cut; the SDL test below
    checks every setting row fits;
  - markers as text: `cursor` and `cursor_disabled` `text:>`,
    `arrow_left` `text:<`, `arrow_right` `text:>`, `scroll_up` `text:^`,
    `scroll_down` `text:v`, with `cursor_y`, `arrow_y`, `scroll_y` all 16
    (the row's baseline). The fixture's sprite values (5, 6, 9) come back
    with the sprites in step 12.
- `[list.selected]`, `[list.selected_disabled]`: as the fixture (`#2040c0`
  x 22 w 596 h 24, 2 px `#6888f8` lines). They are theme constants, not
  sprites, so they are final.
- `[notice]`: as the fixture (14 px, x 30, emphasis x 50, top 98, rows 32,
  text_y 22, so baselines 120, 152, 184 as in 03).
- `[message]`: as the fixture (12 px, x 30, baseline 412, right 610, info
  `#78d8f8`, error `#ff6868`).
- `[help]`, new: `font = ps2p_10`, `x = 30`, `y = 412`, `right = 610`,
  `colour = #78d8f8`. 00 puts its help at x 34, baseline 418, but 00 is the
  16 px variant; in the 12 px layout the help takes the message's place
  (the plan's rule "the message replaces the help"), so it uses the message
  zone's x, baseline and right edge. ▼ is on the last body row (step 1.2's
  decision), so the whole width is free: (610 − 30) / 10 = **58**
  characters (the plan's "about 57 in 572 px" was measured on 00).
- `[footer]`: as the fixture (12 px, x 30, baseline 448, key_y 433, right
  610, key_gap 10, hint_gap 36, `#f8f8f8`, primary `#f8d038`), except every
  `button.*` is `hint_text`: the core's key text ("</>", "A", "B", "X",
  "Y", "START") is drawn in the label's font and colour. `key_y` stays 433,
  the sprites' top, unused by text keys. 01's three hints end at x 492, in
  the 610 room.
- `[footer.band]`: as the fixture (`#08084c` x 14 y 422 w 612 h 44, 4 px
  `#3858d8` top line); drawn only with hints, so 02 and 06 have none.
- One comment line in the file, the trap: text markers' y is a baseline, a
  sprite's is its top. Nothing else is commented.
- **What the geometry gives**, with Press Start 2P's advance equal to its
  size: page (386 − 98) / 24 = 12 rows; wrap width 48; help line 58. The
  app accepts page 1..36 and width 20..94, so `alttpr` starts with it; its
  `--shot-text` grid is 50 × 16 (width + 2 by page + 4); `ui_cli`'s
  matching grid for step 9 is `--cols 50 --lines 16`. These numbers go in
  the step notes; no test asserts them (the plan's goal: a theme edit
  changes no test).

### `make sdl-pc` draws every state with the new theme

- **`goldens.sh` gains a third mode**, `--draw DIR`: `goldens.sh ALTTPR
  THEME SD_FIXTURE --draw DIR` (the argument form of H-6-1). It plays every
  state of `golden_states.txt` into the app exactly as `--compare` does
  (fresh fixture copy, fixed seed and date, the state's flags, frame cap and
  keys, `--theme THEME`), writes `DIR/<state>.bmp` and `DIR/<state>.txt`
  (DIR emptied first), and fails with one `goldens: <state>: …` line when
  `alttpr` exits non-zero or a BMP is not a 640×480 picture of more than
  one colour. It skips the state strings check: those strings are spelled
  on classic's 38-column grid (padding between label and value, scroll-tag
  lines), which a 48-column theme does not reproduce; reaching each state
  is still proven by the classic comparison on the same key sequences. It
  reads no fingerprint and compares nothing, so it runs on any host with
  `make sdl-pc`'s prerequisites. It ends with `goldens: 13 states drawn
  with <theme folder name> in DIR`.
- The `--write` and `--compare` modes are unchanged; `--draw` reuses the
  same render loop with the strings check switched off, not a copy of it.
- **Makefile**: `sdl-pc-build` runs, after the classic comparison, `goldens.sh
  ./alttpr ../../themes/alttp ./sd_fixture --draw` into
  `tests/rando/build/sdl/theme-states/alttp/`, the theme path in one
  variable (`DRAW_THEME`) so that step 13 changes one line. `sdl-pc`'s
  closing echo mentions the drawn states. `sdl-goldens` is unchanged.
- `smoke_sdl.sh` is unchanged: its cases run on `classic`.

### Docs

- `README.md`: the theme paragraph names `themes/alttp` (the mockups'
  geometry with plain stand-ins, frame and sprites to come), how to look at
  it (`tests/rando/build/sdl/alttpr --sd tmp/sd --theme themes/alttp`), and
  where `make sdl-pc` writes its 13 captures; the fonts' paragraph names
  Press Start 2P, its source commit and licence.
- `CLAUDE.md`: `themes/` lists `alttp/`; `data/` holds both fonts and their
  licences; `make sdl-pc`'s description gains the draw of the 13 states with
  `themes/alttp`.

## Implementation Chunks

1. **The `[help]` section.** Loader, struct, `theme_help_columns`, their
   tests in `test_theme` (below). `classic` and the inventory fixture
   untouched. Done: `make test`, `make lint` green; `make sdl-pc` green in
   the pinned environment (the 13 classic goldens byte-identical: the new
   section changes nothing classic draws).
2. **Press Start 2P and `themes/alttp/`.** The two files under `data/`
   (verified as above, hashes in the commit message), the theme folder with
   its `theme.ini` and byte copies, the `make test` checks on it, README and
   CLAUDE.md for the font and the theme. Done: `make test`, `make lint`
   green.
3. **The SDL side.** `test_sdl_alttp` in `SDL_TESTS`, `goldens.sh --draw`,
   the Makefile line, CLAUDE.md for `make sdl-pc`, the captures looked at
   once by the implementer (each of the 13 shows its zones where the
   mockups put them; anything off is fixed in the theme, not the test).
   Done: the step's verification below.

Step verification (the plan's): the theme loads (`make test`), and `make
sdl-pc`, really run in the pinned environment (plan rule: a `setup:` line
does not verify), prints `goldens: 13 states byte-identical` for classic
and `goldens: 13 states drawn with alttp in …`; then `make check` green.
The step notes record the three numbers (page 12, width 48, help 58), the
`ui_cli` grid (`--cols 50 --lines 16`), and the font's source and hashes.

## Testing Decisions

- A good test drives the public surface: `theme_parse` / `theme_load` and
  the derived functions for the loader; `sdl_render_open` and SDL_ttf's own
  metrics for the fonts; the app's exit status and captures for the draw.
  No test reads the loader's or the renderer's internals, and **no test pins
  a colour, position or derived number of `themes/alttp`**: tests check
  that it loads, that its font files are the committed ones, that its fonts
  are monospace and that no setting row is cut, all derived from the theme
  at test time, so a later theme edit changes no test unless it breaks one
  of those properties.
- **`test_theme`** (`make test`; prior art: its `test_optional_sections`,
  `test_classic_cross_checks`, `test_columns_floor_partial_characters`,
  `test_inventory_loads`):
  - `[help]` absent (classic, the inventory fixture): `help.present` false,
    `theme_help_columns` 0;
  - `[help]` present on a minimal theme: values parsed, font index
    resolved; each of the five keys missing → the missing-key message;
    an unknown key; an unknown font → "unknown font"; `x` ≥ `right` → the
    range failure at `x`'s line; a second `[help]` → the loader's message
    for a repeated section, whatever step 3 made it; out-of-range `x`, `y`,
    `right`;
  - `theme_help_columns` floors partial characters (like
    `theme_columns`'s test);
  - `themes/alttp` loads with `theme_load`; its `[help]` is present; its
    page and its columns and help columns, computed with each font's own
    `size.px` as the advance, are within the app's range (page 1..
    `UI_GRID_LINES_MAX − 4`, width `UI_HISTORY_WIDTH_MIN..MAX`) and the help
    columns are at least 1;
  - every `.ttf` and `-LICENSE.txt` in `themes/alttp/` that `data/` also
    holds is byte-identical to `data/`'s (read with stdio).
- **`test_sdl_alttp`**, new, in `SDL_TESTS` (`make sdl-pc`, dummy video
  driver; prior art: `test_sdl_classic`'s font checks, `test_sdl_render`'s
  open tests):
  - `themes/alttp` loads and `sdl_render_open` succeeds (every font of the
    theme opens);
  - for every font of the theme, every glyph 0x20–0x7E advances by exactly
    `size.px` (the monospace fact step 11's pure bound relies on);
  - `sdl_render_advance` equals the list font's `size.px`;
  - for every setting (`RANDO_SETTING_COUNT`) and every one of its values
    (`rando_setting_value_count`), with the English table's label and value:
    the value's width is within `value_max`, and the label, measured with
    `TTF_SizeText` on the list font, ends at least `label_gap` before the
    left arrow (`value_right − value width − arrow_left_gap − arrow
    marker width`), from `label_x_cursor` and from `label_x` alike. The
    arrow marker's width is measured from the marker itself (text now, a
    sprite after step 12).
- **`goldens.sh --draw`**: exercised by `make sdl-pc` on the 13 states; a
  failure is one line naming the state. Its own refusal paths (a state
  exiting non-zero) are not given a planted test: the mode only adds a
  switch to the existing render loop, whose failure lines `--compare`
  already uses.
- **Unchanged and must stay green**: the 13 classic goldens and
  `environment.txt`, every snapshot, `smoke_sdl.sh`, `test_sdl_classic`,
  `test_sdl_render`, the package self-test, `no_sdl.sh`, `no_literals.py`.

## Out of Scope

- `frame.png`, the sprites, the hearts and the panel gradient (step 12).
- The help text, the core's help field and drawing the help line (step 11).
- Editing the mockups, their local `@font-face`, their key sequences, and
  `ui_cli`'s `--lines`/`--cols` run (step 9).
- `make theme-compare` (step 10).
- Shipping the new theme or the font in the zip, the default theme switch
  (step 13): `PKG_THEME` stays `classic`.
- Any change to `classic`, the goldens, the snapshots, `rando/`,
  `app/core/` or `app/sd/`; the inventory fixture.
- A 16 px variant of the theme (it stays possible as a theme edit).

## Hypotheses about steps 6 and 7

Steps 6 (`docs/prd/triage/phase1-step6-old-renderer-removed-packaging.md`)
and 7 (check-net) are not done when this PRD is written; step 6's PRD was
read, its code was not. Everything this PRD assumes about their outcome:

- **H-6-1**: `goldens.sh` takes `ALTTPR THEME SD_FIXTURE --write DIR |
  --compare DIR` (theme mandatory and second, no FONT argument, no
  `--font` passed to `alttpr`), fingerprints the theme's `*.ttf`, and
  `make sdl-pc` runs it with `../../themes/classic --compare`, printing
  `goldens: 13 states byte-identical`. `--draw` is added to that form. If
  the form differs, `--draw` follows step 6's form.
- **H-6-2**: one renderer: `sdl_view.*` and `--font` are gone, every frame
  is drawn by `sdl_render` from a theme; `alttpr --theme <dir>` works as in
  step 5, and without it the app reads `themes/default/` next to its binary.
  This step only uses `--theme`.
- **H-6-3**: `sdl_render`'s interface is step 5's (`sdl_render_open`,
  `sdl_render_advance`, `sdl_render_draw`, `sdl_render_close`, the public
  `SdlRender.fonts`), and the main still derives page and width from
  `theme_page` and `theme_columns(theme, sdl_render_advance(...))`, with
  the 1..36 / 20..94 range check.
- **H-6-4**: themes are flat folders of regular files (the zip builder
  refuses subfolders, symlinks and hidden files), and `build_zip.py` ships
  a theme folder whole; `themes/alttp/` is built to that rule so step 13
  can ship it unchanged.
- **H-6-5**: `data/` keeps DejaVu Sans Mono and its licence as the fonts'
  source, no longer shipped from `data/`; `PKG_THEME` is `classic`. Press
  Start 2P goes beside DejaVu.
- **H-6-6**: `SDL_TESTS` no longer lists `test_sdl_view`; `test_sdl_alttp`
  is appended to whatever list step 6 leaves.
- **H-6-7**: `smoke_sdl.sh` takes `ALTTPR THEME` and runs on `classic`;
  this step does not touch it.
- **H-6-8**: `CLAUDE.md` after step 6 describes `data/` as the fonts'
  source, the package's default theme folder and `make sdl-pc` without
  `--font`; this step edits only the `themes/`, `data/` and `make sdl-pc`
  lines.
- **H-6-9** (step 7): check-net leaves `make check` green with no change to
  the theme format, `sdl_render`, `goldens.sh` or the golden state list;
  any fix-bug commit it makes is outside those.

## Further Notes

- **Why an optional `[help]` section now.** The plan makes step 8 fix the
  help line's length and step 11 bound every help string by it; the length
  is only the theme's if the theme holds the help line's font and extent.
  A separate optional section (rather than help keys in `[message]`) keeps
  `classic` valid without a help line, which is what keeps classic's
  goldens identical when step 11 adds the help text.
- **Why no test pins 12, 48 and 58.** The plan's goal is that the look is
  iterated by editing the theme with no test changed. The numbers are
  recorded in the step notes; steps 9 and 11 derive theirs from the theme
  (`theme_page`, `theme_columns`, `theme_help_columns` with each font's
  size as its advance, justified by `test_sdl_alttp`'s monospace check),
  and step 12's rule re-runs them if a tuning pass moves the numbers.
- **Why `list.right = 606`.** It is the scroll markers' column (`scroll_x`)
  and gives a whole number of 12 px columns from x 30. The inventory
  fixture's 600 was not a measured value (the inventory has no list right
  edge); 610 gives the same 48 wrap columns but lets an unwrapped row (a
  History file name, a label) be drawn up to x 610, under ▲ at 606–616.
- **Why plain text stand-ins.** The plan says "no frame or sprites yet
  (plain colours stand in)". Text markers and `hint_text` buttons need no
  file, are already supported by the format, and make every state readable
  in the captures; their positions (baselines) are the only values step 12
  changes when the sprites arrive.
- **Name.** `themes/alttp/`, as the plan proposed: short, matches the app,
  and step 13 ships it as `themes/default/` anyway.
- The pinned environment for `make sdl-pc` is step 1.1's container
  (Ubuntu 24.04.5, sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11,
  FreeType 2.13.2, SDL_image 1.2.12); the font metrics above were measured
  there.
