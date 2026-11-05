---
title: "Theme loader (pure C) and the classic theme.ini"
description: "Nothing can describe the SDL app's look as data yet: there is no theme format, no loader, and no proof that a theme file can say what today's renderer draws."
status: needs-triage
---

## Problem Statement

Plan 2 moves the SDL app's look out of the code and into a theme folder read
at launch (`theme.ini`, `frame.png`, sprites, fonts). Today every position,
size and colour is hard-wired: the 40×15 cell grid of `ui_grid`, the cell size
`sdl_view_open` derives from 640×480, the font size it picks by fitting a cell,
and the `sdl_view_palette` table of band and text colours per line style.

Before a renderer is built on a theme (step 5), three things are missing:

- a file format that can say both what today's renderer draws (one glyph per
  cell, centred and clipped; a band colour and a text colour per line style;
  a font size chosen by fitting a cell) and what the owner's mockups draw
  (step 2's inventory: proportional Press Start 2P on baselines, sprites,
  notched bands, a dashed title rule, irregular tab slots);
- a loader, testable in `make test`, that turns that file into a struct and
  refuses anything wrong with one message naming the file, the line and the
  key, since only the agent edits themes and a silent default would hide a
  typo;
- the `classic` theme's `theme.ini`, written now, so the format is proven able
  to express today's drawing before anything is built on it.

## Solution

1. **A theme format**, `theme.ini`: an INI file of sections and `key = value`
   lines, defined in full below (Implementation Decisions). Every key of a
   present section is required, nothing has a default, optional features are
   optional sections, and a marker (cursor, arrow, scroll mark, tab mark) is
   `none`, a short text, or a sprite file.
2. **A pure C99 loader** in a new module `app/theme/` (no SDL, no `malloc`,
   standard C only): `theme_parse` turns a buffer into a `Theme` struct;
   `theme_load` reads `<dir>/theme.ini` and parses it. On the first problem it
   stops and fills one message, `<file>:<line>: <key>: <problem>`.
3. **`themes/classic/theme.ini`**, the values of today's renderer written as a
   theme: one DejaVu Sans Mono font in 16×32 cells at the fitting size, every
   zone placed on the grid's lines and columns, every colour from
   `sdl_view_palette`.
4. **Proofs**: unit tests on valid and broken themes in `make test`; `classic`
   loads; an SDL test (`make sdl-pc`) checks `classic`'s colours and geometry
   against `sdl_view_palette`, `ui_grid`'s line layout and `sdl_view_open`'s
   cell size; a test-only theme written from step 2's inventory loads, proving
   the format expresses every mockup element.

Nothing the app draws changes in this step: the SDL app does not load a theme
yet (step 5), and step 1's goldens stay byte-identical.

## User Stories

1. As the agent writing the renderer (step 5), I want a theme struct holding every position, size, colour, font and sprite name, so that the renderer contains no visual constant of its own.
2. As the agent writing the renderer, I want `classic`'s values already proven equal to `sdl_view_palette` and the grid layout, so that a golden difference in step 5 is a renderer bug, not a theme typo.
3. As the agent writing the renderer, I want the format to say how text is placed (cells or a run, top or baseline), so that one renderer draws both `classic`'s monospace grid and the mockups' proportional text.
4. As the agent writing the renderer, I want a cell font's size to be either a number or `fit`, with `fit` defined as `sdl_view_open`'s search, so that `classic` keeps today's point size without hard-coding it.
5. As the agent writing the renderer, I want every marker to be `none`, a text or a sprite, so that `classic` draws `[ ] < > ^ v` as text where the new theme draws the triforce and arrow sprites.
6. As the agent writing the renderer, I want a text marker drawn in the colour of the text it belongs to, so that `classic`'s `^` and `<` take their line's colour exactly as the grid draws them today.
7. As the agent writing the renderer, I want each footer button mapped to either the hint's own key text or a sprite, so that `classic` prints `START Generate` and the new theme draws the START pill.
8. As the agent writing the renderer, I want bands (tab bar, footer band, selected row) as optional sections with a rectangle, a colour and top and bottom lines, so that the grid's full-width bars and the mockups' notched bands with their 2 px and 4 px lines are both expressible.
9. As the agent writing the renderer, I want the footer band drawn only when the screen has hints and the tab band only when it has tabs, as the format's stated contract, so that mockups 02 and 06 have no footer band and `classic` keeps its blank last line.
10. As the agent writing the renderer, I want a cut mark in the theme, so that text longer than its zone is cut with `~` as the grid does today.
11. As the agent writing the renderer, I want the setting row's geometry (value right edge, value width cap, arrow gaps, label gap), so that `classic` reproduces `put_setting`'s placement and the new theme its right-aligned values.
12. As the agent writing the renderer, I want per-state colours for label, value, text rows, disabled, selected and selected-disabled rows, so that the mockups' white labels with cyan values and `classic`'s single colour per line both fit.
13. As the agent writing the renderer, I want separate list and notice body sections, so that the prose pages use the mockups' 14 px text on 32 px rows while lists use 12 px on 24 px.
14. As the agent writing the renderer, I want the title's alert colour and the notice body's emphasis colour and indent, so that step 2's title tone and row emphasis have a theme value to draw with.
15. As the agent writing the renderer, I want tab slots with an inactive text x, an active text x and a marker x each, so that the mockups' irregular tab layout is reproduced without the code assuming a regular one.
16. As the agent writing the renderer, I want an optional dashed title rule section, so that the rule after the title text is a theme feature `classic` simply leaves out.
17. As the agent making the page size come from the theme (step 4), I want the list body's top, bottom, row height, text x and right edge in the struct, so that the page (11) and the wrap width (38) can be derived from `classic`.
18. As the agent building the new theme (phase 2), I want font size and row height as plain theme values, so that going back to 16 px is a theme edit, as the plan requires.
19. As the agent building the new theme, I want a typo in a key refused with its line and name, so that a misspelt key never silently falls back to a default.
20. As the agent building the new theme, I want a missing key named with its section's line, so that I see at once what a new section lacks.
21. As the agent building the new theme, I want an out-of-range value refused with the allowed range in the message, so that I can fix it without reading the loader.
22. As the agent building the new theme, I want a sprite or font file name refused if it holds a path separator, `..`, or the wrong extension, so that a theme cannot reach outside its folder.
23. As the agent building the new theme, I want a reference to an undefined font refused, so that a renamed font section is caught at launch.
24. As the agent building the new theme, I want a rectangle outside the 640×480 screen refused, so that a band cannot write off the surface.
25. As the agent building the new theme, I want `fit` without a cell refused, so that a proportional font always has an explicit size.
26. As the agent building the new theme, I want tab slots numbered 1 to N with N at most 4, so that a gap or an extra slot is an error rather than an unused section.
27. As the owner, I want a missing or broken theme to give one line naming the file and the problem, so that the app fails as loudly as it does today for an unreadable font.
28. As the owner, I want the SDL app to look exactly as it does today after this step, so that the engine is proven before the look changes.
29. As the plan pilot, I want `make test` to run the loader's tests on the host in milliseconds, with no SDL, so that the fast loop stays fast.
30. As the plan pilot, I want `make lint` to build `app/theme/` with `-pedantic-errors`, so that the loader stays strict C99.
31. As the plan pilot, I want `no_sdl.sh` to cover `app/theme/`, so that an SDL include there fails `make test`.
32. As the plan pilot, I want `make sdl-pc` to run the classic cross-check and still print `goldens: 13 states byte-identical`, so that the step is verified as the plan asks.
33. As the plan pilot, I want `make unit-arm` to build and run the loader's test under qemu like every other unit test, so that the loader is proven on the device's ABI too.
34. As a maintainer, I want the format's definition written once (this PRD, then the loader's key table), so that the theme files, the loader and the renderer agree on one list of keys.
35. As a maintainer, I want the inventory test theme to stay a fixture of the tests, so that a later change to the format that drops an inventory element fails `make test`.

## Implementation Decisions

### Module and interface

- **New module `app/theme/`**, pure C99, no SDL, no `malloc`, only the
  standard library (stdio for reading the file). Built into the app archive
  `librando_app.a` like `app/core` and `app/sd`, with only itself on its
  include path (it includes neither the core nor the library). It is not in
  `app/core/` because `no_literals.py` forbids letter literals there, and the
  key names and error messages are the loader's substance; they are messages
  for the theme's author (the agent), on stderr, like `sdl_main.c`'s
  `fail_setup` lines, not user-facing UI text, so they need no translation
  table.
- **Interface** (names final unless they clash):
  - `Theme`: the parsed theme, a plain struct of fixed-size members (no
    pointers into the source text), sized for at most 8 fonts and 4 tab
    slots (`THEME_FONTS_MAX`, `THEME_TABS_MAX`; the latter equals
    `UI_TABS_MAX`, stated in the header since the module does not include the
    core).
  - `ThemeError`: a caller-owned struct with a fixed message buffer (512
    bytes; a longer message is truncated, never overflowed).
  - `bool theme_parse(Theme *out, const char *text, const char *file, ThemeError *err)`:
    parses a NUL-terminated buffer; `file` is only used in messages. Returns
    false on the first problem, with `err` filled; `out`'s content is then
    unspecified.
  - `bool theme_load(Theme *out, const char *dir, ThemeError *err)`: reads
    `<dir>/theme.ini` into a static or caller-sized buffer (at most 16384
    bytes) and calls `theme_parse` with the file named `<dir>/theme.ini`.
  - The struct exposes each zone as a nested struct (screen, fonts, tabs and
    tab slots, title and its rule, list body, notice body, message, footer),
    each font reference as an index into the font array, each colour as
    three bytes, each marker as a kind (none, text, sprite) with its text or
    file name, each band as present/absent plus its fields.
- **The loader checks syntax, types, ranges and cross-references only.** It
  never opens a PNG or TTF: whether a named file exists, opens and fits
  (`fit` actually finding a size) is the SDL layer's check in step 5, with
  the same message shape.

### File syntax

- UTF-8 is not needed: every byte is printable ASCII (0x20–0x7E) or a line
  feed `\n`; anything else (a tab, a carriage return, a byte ≥ 0x80) is
  refused at its line. A line is at most 255 bytes.
- Blank lines and comment lines (first non-space character `;` or `#`) are
  ignored. There are no trailing comments (a value may start with `#`, a
  colour).
- `[name]` opens a section; `key = value` sets a key; spaces around the name,
  the key, `=` and the value are trimmed. A key before any section, a
  duplicate section, a duplicate key, an unknown section and an unknown key
  are refused.
- Every key of a present section is required; a required section missing is
  refused; an optional section is a feature switched on by its presence.
- Problems are reported in file order, first one only; after the whole file
  is read, missing keys are reported section by section in the table's order,
  then missing sections, then cross-checks.

### Value types

| Type | Syntax | Notes |
|---|---|---|
| int | decimal, optional leading `-` | each key has its own range, stated in the message |
| colour | `#rrggbb`, hex digits in either case | |
| file | `[A-Za-z0-9_.-]`, 1–63 bytes, not starting with `.`, ending `.png` (sprites, images) or `.ttf` (fonts) | resolved in the theme folder by step 5 |
| name | `[a-z0-9_]`, 1–15 bytes | font names |
| marker | `none`, `text:<chars>` (1–7 printable non-space bytes), `sprite:<file.png>` | |
| button | `hint_text` (draw the hint's own key text) or `sprite:<file.png>` | footer buttons only |
| size | an int 8–64, or `fit` | fonts only |
| cell | `none`, or two ints `W H` (W 4–64, H 4–128) | fonts only |
| anchor | `top` or `baseline` | fonts only |

### Error messages (exact; the tests assert them)

- Line problems: `<file>:<line>: <problem>`, with problems `line longer than 255 bytes`, `not printable ASCII`, `not "key = value" or "[section]"`, `key outside a section`.
- Section problems: `<file>:<line>: [<section>]: unknown section` / `duplicate section`; `<file>: [<section>]: missing`.
- Key problems: `<file>:<line>: <section>.<key>: <problem>`, with problems `unknown key`, `duplicate key`, `missing` (line = the section's header line), `not an integer`, `out of range <min>..<max>`, `not a colour (#rrggbb)`, `not a file name (<ext>)`, `not a marker (none, text:<chars>, sprite:<file.png>)`, `not hint_text or sprite:<file.png>`, `unknown font <name>`, `fit needs a cell`, `outside the screen`, and for enumerations `not <a> or <b>`.
- Whole-file problems: `<dir>/theme.ini: cannot open: <strerror text>`, `<file>: larger than 16384 bytes`, `<file>: [tab.<n>]: tab slots must be numbered 1 to N, N at most 4`, `<file>: [font.<n>]: more than 8 fonts`.
- The SDL main (step 5) prints the message prefixed `alttpr: ` and exits 1, as `fail_setup` does; this step only produces the text.

### Drawing semantics the format fixes (for step 5)

These are part of the format's definition; the loader stores the values, the
renderer of step 5 implements them, and `classic` relies on them.

- The screen is 640×480 (a loader constant, not a key); every absolute x is
  0–640, every absolute y 0–480; every rectangle must lie inside the screen.
- Drawing order: screen colour, screen image, bands, sprites and text.
- **Fonts.** `size = fit` means the largest size from 64 down to 8 whose `M`
  advance is at most the cell's W and whose font height is at most the cell's
  H (today's `sdl_view_open` search). A font with a cell draws each byte of a
  text as its own one-character surface in a W×H cell, cell i at x + i·W,
  centred horizontally when narrower than W, clipped to the cell, spaces
  skipped (today's `sdl_view_draw`). A font without a cell draws the text as
  one run. `anchor = top` puts the rendered surface's top at the text's y;
  `anchor = baseline` puts the font's ascent line there.
- **Cutting.** A text longer than its zone's room is cut: with a cell font,
  the last cell inside the room holds the cut mark (today's `pen_done`); with
  a run font, characters are dropped until the text and the cut mark fit. A
  room of zero or less draws nothing. `screen.cut = none` cuts without a mark.
- **Text markers** are drawn in the zone's font at the marker's position, in
  the colour of the text they belong to: tab marks in the tab's colour;
  arrows in the row's value colour for its state; the row cursor and scroll
  marks in the row's label colour for its state, and on a line holding no
  row in `list.label_colour`; the cut mark in the colour of the text it cuts.
  Sprite markers are drawn with their top-left at the marker's position, in
  their own pixels.
- **Bands.** A band is drawn only when its zone has content: the tab band when
  the screen has tabs, the footer band when it has hints, a selected-row band
  on the selected row. Row bands' y is relative to the row's top. A band's
  `top_line` and `bottom_line` (thickness, 0 = none) are drawn inside its
  rectangle in `line_colour`.
- **Tabs.** Slot i's text is at `tab.i.x` when inactive, `tab.i.active_x` when
  active, at `tabs.y`; the active tab gets `tabs.active_before` at
  (`tab.i.marker_x`, `tabs.marker_y`) and `tabs.active_after` right after its
  text's end, at `tabs.marker_y`. More tabs on a screen than slots is the
  renderer's error (step 5).
- **List rows.** Row k's top is `list.top + k·list.row_height`, its text at
  `+ list.text_y`. Labels start at `list.label_x_cursor` when the screen has a
  cursor (≥ 0), else `list.label_x`. Action and text rows are cut at
  `list.right`. A setting's value is right-aligned to `list.value_right`, cut
  to `list.value_max` pixels; its left arrow ends `list.arrow_left_gap` before
  the value's left edge, its right arrow starts `list.arrow_right_gap` after
  `list.value_right`, both at `+ list.arrow_y`, and only on a selected,
  enabled setting; the room for the label always reserves the arrows: the
  label is cut `list.label_gap` before the left arrow's left edge. The cursor
  (`list.cursor`, or `list.cursor_disabled` on a disabled row) is at
  (`list.cursor_x`, row top + `list.cursor_y`) on the selected row. Scroll
  marks are at (`list.scroll_x`, row top + `list.scroll_y`) on the first and
  on the last row of the page (the page's last line, whether or not a row is
  there, as today's grid).
- **Notice rows** use the notice section's font, x, top, row height and
  colour; an emphasised row uses `notice.emphasis_x` and
  `notice.emphasis_colour`. The page size the core scrolls by comes from the
  list section only (step 4).
- **Footer.** Hints flow from `footer.x`: the key (the hint's key text in the
  footer font, or the button's sprite at `footer.key_y`), then the label
  `footer.key_gap` after the key's right edge, then the next key
  `footer.hint_gap` after the label's end; the label is `footer.primary_colour`
  on the primary hint, else `footer.label_colour`; a text key takes its
  label's colour. Everything is cut at `footer.right`.
- **Title.** At (`title.x`, `title.y`), cut at `title.right`, `title.colour`
  or `title.alert_colour` by step 2's title tone. The optional rule is a
  dashed line from the title text's end + `title.rule.offset` to
  `title.rule.end`.
- **Message.** At (`message.x`, `message.y`), cut at `message.right`,
  `message.info_colour` or `message.error_colour`. Phase 2's help line adds
  its own keys then (step 11), not now.

### The keys

Ranges: X = 0–640, Y = 0–480, R = −128–128 (offsets inside a row), L = 0–640
(lengths, gaps). "font" is a font name defined by a `[font.<name>]` section.

| Section | Req. | Keys |
|---|---|---|
| `[screen]` | yes | `colour` colour; `image` `none` or a `.png` file; `cut` marker (text or none; sprite refused) |
| `[font.<name>]` | 1–8 | `file` `.ttf` file; `size` size; `cell` cell; `anchor` anchor |
| `[tabs]` | yes | `font`; `y` Y; `right` X; `colour`, `active_colour` colours; `active_before`, `active_after` markers; `marker_y` Y |
| `[tabs.band]` | no | band (absolute) |
| `[tab.<n>]` | 1–4, n = 1..N | `x`, `active_x`, `marker_x` X |
| `[title]` | yes | `font`; `x` X; `y` Y; `right` X; `colour`, `alert_colour` |
| `[title.rule]` | no | `colour`; `y` Y; `thickness` 1–16; `dash` 1–64; `gap` 0–64; `offset` L; `end` X |
| `[list]` | yes | `font`; `top`, `bottom` Y (top < bottom, at least one row between); `row_height` 8–240; `text_y` R; `label_x`, `label_x_cursor`, `right`, `value_right` X (each x < `right`, `value_right` ≤ `right`); `value_max` L; `label_gap`, `arrow_left_gap`, `arrow_right_gap` L; `label_colour`, `value_colour`, `text_colour`, `disabled_colour`, `selected_colour`, `selected_disabled_colour`; `cursor`, `cursor_disabled`, `arrow_left`, `arrow_right`, `scroll_up`, `scroll_down` markers; `cursor_x`, `scroll_x` X; `cursor_y`, `arrow_y`, `scroll_y` R |
| `[list.selected]` | yes | band (row-relative) |
| `[list.selected_disabled]` | yes | band (row-relative) |
| `[notice]` | yes | `font`; `x`, `emphasis_x`, `right` X; `top` Y; `row_height` 8–240; `text_y` R; `colour`, `emphasis_colour` |
| `[message]` | yes | `font`; `x` X; `y` Y; `right` X; `info_colour`, `error_colour` |
| `[footer]` | yes | `font`; `x` X; `y`, `key_y` Y; `right` X; `key_gap`, `hint_gap` L; `label_colour`, `primary_colour`; `button.left_right`, `button.a`, `button.b`, `button.x`, `button.y`, `button.start` buttons |
| `[footer.band]` | no | band (absolute) |

A band's keys: `x` X, `y` (Y absolute, or 0–240 row-relative), `w` 1–640,
`h` 1–480 (row-relative: at most the row height), `colour`, `top_line` 0–16,
`bottom_line` 0–16, `line_colour`. Every marker key takes any marker kind,
except `screen.cut`, which refuses a sprite.

Key names with a dot (`button.a`) are plain keys of `[footer]`; the dot is
part of the name. Section names with a dot are sections of their own.

### `classic` (`themes/classic/theme.ini`)

A new top-level folder `themes/`, one folder per theme (step 6 ships one next
to the binary; step 8 adds the new one). `classic`'s values, each traced to
today's code:

- `[font.mono]`: `file = DejaVuSansMono.ttf`, `size = fit`, `cell = 16 32`
  (640/40 × 480/15, `sdl_view_open`'s cell), `anchor = top`.
- `[screen]`: `colour = #14161a` (BG), `image = none` (step 5 may replace it
  with a solid PNG of the same colour, the plan's "background as a PNG"; the
  pixels are the same), `cut = text:~`.
- Grid lines → y: tabs 0, title 32, body lines 2–12 → `list.top = 64`,
  `list.bottom = 416` (11 rows, `ui_grid_page`), message 416, footer 448; all
  text x 16 (column 1) and `right = 624` (column 39 exclusive), tabs `right =
  640`.
- `[tabs]`: colour and active colour `#ffffff`; `active_before = text:[`,
  `active_after = text:]`, `marker_y = 0`; `[tabs.band]` 0 0 640 32
  `#22262e`, no lines; `[tab.1]` x 16, active_x 16, marker_x 0; `[tab.2]` x
  192, active_x 192, marker_x 176 (the grid's `[Generate]  History ` and
  ` Generate   [History]` with today's English tab names).
- `[title]` `#7fb4ff`, alert `#7fb4ff`; no rule.
- `[list]`: row height 32, text_y 0, label_x and label_x_cursor 16,
  value_right 592 (column 37), value_max 544 (34 columns), arrow gaps 16 and
  16 (`<` two cells left of the value, `>` in column 38), label_gap 16;
  colours label and value `#e6e6e6`, text `#b8bcc4`, disabled `#6b7078`,
  selected `#ffffff`, selected-disabled `#8a9099`; cursor and cursor_disabled
  `none` (cursor_x, cursor_y 0); arrows `text:<`, `text:>`, arrow_y 0;
  scroll `text:^`, `text:v`, scroll_x 624, scroll_y 0; `[list.selected]` 0 0
  640 32 `#2f6fd6`; `[list.selected_disabled]` 0 0 640 32 `#3a3f48`; no
  lines.
- `[notice]`: x and emphasis_x 16, right 624, top 64, row height 32, text_y
  0, colour and emphasis colour `#b8bcc4` (the notice pages' rows are text
  rows, H-2-7).
- `[message]` y 416, info `#9ad0ff`, error `#ff6b6b`.
- `[footer]` x 16, y and key_y 448, right 624, key_gap 16 (one space),
  hint_gap 32 (two spaces), label and primary `#9aa0a8`, every button
  `hint_text`; `[footer.band]` 0 448 640 32 `#22262e`, no lines.
- Line-style bands equal to the background (title, rows, message) are not
  bands: the background already has that colour.
- Comments in `classic`'s `theme.ini` follow CLAUDE.md's rule: none, or only
  a trap (e.g. that `tab.2.x` assumes the English tab name lengths).

### Build and checks

- `tests/rando/Makefile`: `app/theme/*.c` joins the app sources (archive,
  lint, the ARM unit build follows `APP_SRC`); `test_theme` joins
  `UNIT_TESTS` (not `LOGIC_TESTS`); `no_sdl.sh` gets `app/theme` among its
  directories; the SDL test `test_sdl_classic` joins `SDL_TESTS`, linked with
  the app archive as the other SDL tests are.
- `CLAUDE.md`'s Layout gains `app/theme/` (the theme loader, pure C99, no
  SDL) and `themes/` (one folder per theme), and its `make test` line lists
  `app/theme/` among `no_sdl.sh`'s directories, so the checks it describes
  stay true.

## Implementation Chunks

1. **Loader syntax and the module in the build.** `app/theme/` with the
   `Theme` and `ThemeError` types, `theme_parse` handling syntax, sections,
   keys, duplicates, unknowns, value types and the exact messages, with the
   `[screen]` and `[font.<name>]` sections only; the Makefile wiring (archive,
   lint, `no_sdl.sh`, `test_theme`). Done: `make test` and `make lint` green;
   `test_theme` covers every line, section and type problem on small inline
   themes.
2. **Full schema, `classic`, `theme_load`.** Every section and key of the
   table, required and optional sections, missing-key reporting order, the
   cross-checks (font references, `fit` without a cell, rectangles inside the
   screen and the row, list top/bottom and x ordering, tab slot numbering, at
   most 8 fonts), `theme_load` with its size cap and open error;
   `themes/classic/theme.ini`; `test_theme` loads `classic` and checks broken
   variants of it; `test_sdl_classic`; the CLAUDE.md lines. Done: `make test`,
   `make lint` green; `make sdl-pc` green, really run in the pinned
   environment, with `test_sdl_classic` passing and `goldens: 13 states
   byte-identical`.
3. **Inventory fixture.** A test-only theme under the unit tests' fixtures
   (`tests/rando/unit/themes/inventory/theme.ini`) written from step 2's
   inventory as committed in the plan's step notes (H-2-5): Press Start 2P
   at 16, 14 and 12 px (baseline anchor, no cell), the tab slots, the dashed
   rule, the footer band with its 4 px top line, the notched selected band
   with 2 px lines, sprite markers and sprite buttons, the notice body; it
   loads in `test_theme`. Each inventory row with a T or S source maps to a
   key (Further Notes); a row the format cannot express is a format change in
   this chunk, not a deferral. Done: `make test` green; the mapping table
   checked row by row against the step notes' inventory.

## Testing Decisions

- A good test feeds the loader a theme text and checks what comes out: the
  struct's values for a valid theme, `false` and the exact message for a
  broken one. No test reads the parser's internal state.
- **`test_theme`** (`tests/rando/unit/`, in `make test`, prior art
  `test_seed_save` and `test_last_settings` for file reading, `test_ui_strings`
  for table-driven checks):
  - `classic` loads through `theme_load("../../themes/classic")` (the unit
    tests run from `tests/rando`), with spot values: one font, `fit`, cell
    16×32, `list.top` 64, `list.bottom` 416, row height 32, label_x 16, right
    624, two tab slots.
  - Broken themes are `classic`'s text read once, with one line replaced,
    removed or added, so each case is one edit and its expected message
    (exact text, exact line number): unknown key, misspelt key, duplicate key,
    unknown section, duplicate section, key outside a section, malformed
    line, line too long, tab and carriage-return bytes, missing key (line =
    section header), missing required section, int not a number, int out of
    range (low and high), bad colour (short, bad digit, missing `#`), bad file
    name (slash, `..`, leading dot, wrong extension, too long), bad marker,
    sprite cut mark refused, bad button, unknown font, `fit` without a cell,
    band outside the screen, row band taller than the row, `list.top` ≥
    `list.bottom`, value_right beyond right, tab slots 1 and 3, five tab
    slots, nine fonts, empty file (missing sections).
  - Optional sections: `classic` without `[tabs.band]` or `[footer.band]`
    still loads, with the band absent; with `[title.rule]` added it loads with
    the rule present.
  - `theme_load` on a missing folder and on a file over 16384 bytes (written
    under `tests/rando/build/`) gives its message.
  - The inventory fixture loads (chunk 3), with spot values (a baseline font,
    a sprite button, the rule present).
- **`test_sdl_classic`** (`tests/rando/sdl/`, in `make sdl-pc`, prior art
  `test_sdl_view`): loads `classic` and checks it against today's renderer
  through its public symbols: the screen colour equals
  `sdl_view_palette[UI_LINE_BLANK].band`; each zone's colours equal their
  line style's entries (tabs, title, row, selected, disabled,
  selected-disabled, text for list text rows and notice rows, info, error,
  footer; bands for tabs, selected, selected-disabled, footer); the cell
  equals 640/`UI_GRID_DEFAULT_COLS` × 480/`UI_GRID_DEFAULT_LINES`; zone y's
  equal the grid's line numbers times the cell height (tabs line 0, title
  line 1, `list.top` line 2 and `list.bottom` line `lines − 2`, notice top
  line 2, message line `lines − 2`, footer line `lines − 1`) and the x's the
  cell width (text column 1, `right` column `cols − 1`); `sdl_view_open`
  on `classic`'s font file gives the size `fit` defines. This is what makes
  step 5's golden comparison a renderer check rather than a theme check.
- **Pixel identity**: step 1's goldens via `make sdl-pc`, which must print
  `goldens: 13 states byte-identical` in the pinned environment (plan rule: a
  `setup:` line does not verify). This step changes no drawing code, so a
  difference is a bug of the step.
- **Static checks**: `make lint` (the module under `-pedantic-errors`),
  `no_sdl.sh` with `app/theme` added, `make unit-arm` (outside the net) runs
  `test_theme` under qemu from `tests/rando`, reaching `../../themes/classic`
  as on the host.

## Out of Scope

- Loading images and fonts, checking that the named files exist, the
  renderer drawing from the theme, SDL_image, `--theme` (steps 5 and 6).
- Page size and width computed from the theme and given to the core (step 4);
  this step only provides the keys.
- `classic`'s background PNG and the copy of DejaVu Sans Mono or any font
  lookup rule beyond "a file name, resolved by step 5" (step 5).
- The new theme, Press Start 2P under `data/`, frame and sprites (steps 8
  and 12); the inventory fixture names sprite and font files that do not
  exist, which the loader never opens.
- The help line's keys (step 11).
- Hot reload, user themes, defaults for missing keys (plan: rejected).
- Any change to `app/core/`, `rando/`, the snapshots or the goldens.

## Hypotheses about step 2

Step 2 (`docs/prd/triage/phase1-step2-mockup-inventory.md`) is being
implemented while this PRD is written. Everything this PRD assumes about its
outcome:

- **H-2-1**: `UiScreen` carries a list of hints, each a button from an enum of
  six values (left/right pair, A, B, X, Y, START), the button's key text, a
  label and a primary flag, at most one primary; the format's six
  `footer.button.*` keys follow that enum one for one. If the enum differs,
  the button keys follow it.
- **H-2-2**: `UiScreen` gains a title tone, normal or alert (alert on
  Generation Failed and the delete confirmation), which `title.colour` and
  `title.alert_colour` serve.
- **H-2-3**: `UiRow` gains an emphasis flag, set on the file-name row of Seed
  Ready and of the delete confirmation, which `notice.emphasis_x` and
  `notice.emphasis_colour` serve.
- **H-2-4**: `UiScreen` gains a body kind, list or notice (notice on
  Generating, Seed Ready, Generation Failed, the empty History, the delete
  confirmation), which selects `[list]` or `[notice]`; notice pages hold at
  most 3 rows, have no cursor and never scroll, and the body kind does not
  change the page size (so step 4 derives the page from `[list]` only).
- **H-2-5**: the inventory, with the coordinates, colours and sizes of the
  step 2 PRD's Further Notes, is copied into the plan's step notes under step
  2; chunk 3 writes its fixture from that copy, and a value that differs
  there wins over the PRD's.
- **H-2-6**: step 2 changes no pixel: `sdl_view_draw`, `sdl_view_palette` and
  the grid's line layout (tabs line 0, title 1, body 2 to `lines − 3`,
  message `lines − 2`, footer `lines − 1`) are unchanged, and step 1's goldens
  still pass, so `classic`'s values above (taken from today's code) are
  still today's.
- **H-2-7**: the notice pages' rows, the emphasised file-name rows included,
  stay `UI_ROW_TEXT` rows, so the grid draws them in the `UI_LINE_TEXT`
  colour `#b8bcc4`, which `classic`'s notice colour and emphasis colour copy.
- **H-2-8**: the text renderer joins hints as key, one space, label, two
  spaces between hints, at column 1 of the footer line, cut with `~` at the
  field's end; `classic`'s `footer.x`, `key_gap` and `hint_gap` (16, 16, 32)
  copy that join, and a text key is drawn in its label's colour because the
  grid's footer line has one colour.
- **H-2-9**: step 2 adds no field for the list's cursor column, the setting
  arrows, the title rule, the footer band or the spoiler indent (the
  inventory's "What is not a new field"): the format derives them as stated
  above (`label_x_cursor` chosen by `cursor >= 0`, arrows on a selected
  enabled setting, the rule from the title's rendered width, the footer band
  only with hints, the indent from leading spaces).

## Further Notes

### Inventory → keys (chunk 3 checks each row against the step notes)

| Inventory element | Keys |
|---|---|
| Background, frame, panel, tab separator, hearts | `screen.image` (`frame.png`), `screen.colour` |
| Tab labels, active/inactive colours | `tabs.font`, `tabs.y` (baseline 42), `tabs.colour`, `tabs.active_colour` |
| Active tab cursor (triforce 20×18, y27) | `tabs.active_before = sprite:…`, `tabs.marker_y`, `tab.n.marker_x` (30, 186) |
| Irregular slot text x (58 active / 30 inactive, 214) | `tab.n.x`, `tab.n.active_x` |
| Title, normal and alert colours | `[title]` |
| Dashed title rule (4 px, 8/4, y77, end 610, +12) | `[title.rule]` |
| Message line, info and error | `[message]` |
| Footer band `#08084c` with 4 px top bar, only with hints | `[footer.band]` (`top_line = 4`) |
| Hint sprites (d-pad, A/B/X/Y, START) at y433 | `footer.button.*`, `footer.key_y` |
| Hint label, primary yellow, baseline 448 | `footer.label_colour`, `footer.primary_colour`, `footer.y` |
| Hint flow (30; label 10 px after the key; next hint 36 after the label) | `footer.x = 30`, `footer.key_gap = 10`, `footer.hint_gap = 36` |
| List rows 12 px / 24 px from y98, baseline +16 | `list.font`, `list.top`, `list.row_height`, `list.text_y` |
| Label column x60 with cursor, x30 without | `list.label_x_cursor`, `list.label_x` |
| Value right-aligned at 584, colours, disabled colour | `list.value_right`, `list.*_colour` |
| Selected band `#2040c0` x22 w596 with 2 px `#6888f8` lines | `[list.selected]`, `[list.selected_disabled]` |
| Row cursor (triforce 16×14 at x32, +5; greyed when disabled) | `list.cursor`, `list.cursor_disabled`, `list.cursor_x`, `list.cursor_y` |
| Selected and selected-disabled text colours | `list.selected_colour`, `list.selected_disabled_colour` |
| Value arrows ◀ ▶ | `list.arrow_left`, `list.arrow_right`, the two gaps, `list.arrow_y` |
| Scroll ▲ ▼ at x606 on the first and last row | `list.scroll_up`, `list.scroll_down`, `list.scroll_x`, `list.scroll_y` |
| Spoiler indent | no key: the core's leading spaces in the list font |
| Notice rows 14 px / 32 px, baseline 120, x30 | `[notice]` |
| Emphasised row, yellow, x50 | `notice.emphasis_x`, `notice.emphasis_colour` |
| Fonts 16, 14, 12 px | three `[font.<name>]` sections, `anchor = baseline`, `cell = none` |
| Help line (10 px, step 11) | not now: step 11 adds its keys |

### Inventory T and S rows → fixture values (chunk 3, checked row by row)

Every T or S row of plan 2's step notes (Step 1.2, "Mockup inventory"), with
the keys of `tests/rando/unit/themes/inventory/theme.ini` that express it. No
row needed a format change.

| Inventory row (source) | Keys = fixture values |
|---|---|
| Active tab cursor (S): triforce 20×18, y27 | `tabs.active_before = sprite:triforce_tab.png`, `tabs.active_after = none`, `tabs.marker_y = 27` |
| Tab slot positions (T): cursor x30/x186, text x58 active, x30 inactive, x214 both | `tab.1.marker_x = 30`, `tab.1.active_x = 58`, `tab.1.x = 30`; `tab.2.marker_x = 186`, `tab.2.x = tab.2.active_x = 214` |
| Title rule (T): dashed `#3858d8`, 4 px, 8/4, y77, title end + 12 to x610 | `[title.rule]` `colour = #3858d8`, `thickness = 4`, `dash = 8`, `gap = 4`, `y = 77`, `offset = 12`, `end = 610` |
| Footer band (T): `#08084c` x14 y422 612×44, 4 px `#3858d8` top bar, only with hints | `[footer.band]` 14 422 612 44 `#08084c`, `top_line = 4`, `bottom_line = 0`, `line_colour = #3858d8` (only with hints: the format's band contract) |
| Hint sprite (S): d-pad, A/B/X/Y, START at the hint's x, y433 | `footer.button.left_right/a/b/x/y/start = sprite:button_*.png`, `footer.key_y = 433` |
| Hint flow (T): first x30, label 10 px after the key's right edge (correction), next hint + 36 | `footer.x = 30`, `footer.key_gap = 10`, `footer.hint_gap = 36` |
| Label column (T): x60 with a cursor, x30 without | `list.label_x_cursor = 60`, `list.label_x = 30` |
| Selected band (T): `#2040c0` x22 596×24, 2 px `#6888f8` lines | `[list.selected]` and `[list.selected_disabled]` 22 0 596 24 `#2040c0`, `top_line = bottom_line = 2`, `line_colour = #6888f8` |
| Row cursor (S): triforce 16×14 at x32, row top + 5; greyed when selected and disabled | `list.cursor = sprite:triforce_row.png`, `list.cursor_disabled = sprite:triforce_row_disabled.png`, `list.cursor_x = 32`, `list.cursor_y = 5` |
| Value arrows (S): ◀ left of the value, ▶ after its right edge (594, value at 584) | `list.arrow_left/right = sprite:arrow_*.png`, `list.arrow_right_gap = 10`, `list.arrow_left_gap = 10` (mirrored), `list.value_right = 584` |
| Scroll up (S): 10×6 at x606 on the first row | `list.scroll_up = sprite:scroll_up.png`, `list.scroll_x = 606` |
| Scroll down (S): mirrored on the last body row | `list.scroll_down = sprite:scroll_down.png` (same `scroll_x`; the last row is the format's rule) |

The Z and V rows' positions and colours are in the fixture too: fonts
`ps2p_16/14/12` (`size` 16/14/12, `cell = none`, `anchor = baseline`); tabs
y42 `#98a8f0`/`#f8d038`; title x30 y82 `#78d8f8`/`#ff6868`; list top 98, 24
px rows, `text_y = 16`, bottom 386 (12 rows), label `#f8f8f8`, value
`#b8e8f8`, disabled `#7078b0`, selected `#f8d038`, selected-disabled
`#bcc4ee`; notice x30, emphasis x50, top 98, 32 px rows, `text_y = 22`
(baseline 120), `#f8f8f8`/`#f8d038`; message x30 y412 `#78d8f8`/`#ff6868`;
footer y448, `#f8f8f8`/`#f8d038`; `screen.image = frame.png`, `screen.colour
= #0c0c34`. Values the inventory does not fix, chosen for the fixture:
`tabs.right = 540` (before the hearts), `title.right`, `notice.right`,
`message.right`, `footer.right` 610, `list.right = 600`, `list.value_max =
240`, `list.label_gap = 12`, `list.arrow_y = 6`, `list.scroll_y = 9`,
`screen.cut = text:~`, and the selected-disabled band equal to the selected
one.

### Other notes

- **The hint flow follows the mockup, not the inventory's wording.** The
  step 2 PRD says a hint label sits at "sprite x + 32". That holds for the
  22 px sprites (22 + 10) but not for START: mockup 01 puts the START pill
  at x322 (66 wide) and its label "Generate" at x398, i.e. 10 px after the
  pill's right edge. The format's "label `key_gap` after the key's right
  edge" gives 62, 202 and 398 with `key_gap = 10`, matching all three
  labels of mockup 01, and `classic`'s one-space join with `key_gap = 16`.
- **`classic` assumes today's English tab names.** The grid places the second
  tab after the first one's text; `classic` fixes slot 2 at x192 because
  "Generate" is 8 characters. A translation changing that length would move
  the grid's tab but not `classic`'s; the plan has no translation, and
  `classic` is a test theme.
- **Why every key is required.** The plan rejects user themes and lets the
  loader be strict: with no defaults, the theme file is the whole look, a
  reader of `classic` sees every value, and a key the renderer needs can
  never be silently absent. The price is a long file; `classic` is about 100
  lines.
- **Why the loader is outside `app/core/`.** The core is the UI state
  machine, which never needs the theme (the page and width reach it as two
  ints, step 4), and its literal rule would forbid the key names. `app/sd/`
  is the SD-card layer and links `rando/host`, which the loader does not
  need. `app/sdl/` would put the parser behind SDL, out of `make test`.
- **Markers are not restricted per key**: any kind on every marker key,
  except `screen.cut`, which must be text or none because a cut mark is
  drawn inside the text it cuts.
