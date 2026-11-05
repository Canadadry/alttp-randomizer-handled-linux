---
title: "Page and width from the theme"
description: "The SDL app still takes the number of body lines per page and the wrap width from the 40×15 grid, so a theme cannot change them."
status: needs-triage
---

## Problem Statement

Plan 2 makes the look a theme read at launch, and its rules say the page
size (body lines per page) and the wrap width (the characters the core wraps
History detail text to, today `grid.cols - 2`) come from the theme: changing
the font size or the row height must be a theme edit, with no rebuild.

Today the SDL main sizes a 40×15 `UiGrid` from compile-time defaults and
hands the core `ui_grid_page(&grid)` (11) and `grid.cols - 2` (38) through
`rando_app_shell_init`'s `page` and `width`. Step 3 gives a parsed `Theme`
(the list body's top, bottom, row height, label x and right edge, the list
font and its cell), but nothing computes a page or a width from it, and the
SDL app has no way to be told which theme to read. Until that exists, the
new theme of phase 2 (12 px rows on 24 px, a different text width) could not
change how far the lists scroll or where the spoiler wraps.

## Solution

1. **Two pure functions in the theme module** (no SDL, no core include):
   the page is the number of whole list rows between the list body's top and
   bottom; the columns are the whole characters of a given advance between
   the list's label x (the no-cursor column, where History detail's wrapped
   text is drawn) and its right edge.
2. **`--theme <dir>` on the SDL app.** When given, the app loads
   `<dir>/theme.ini` with step 3's loader before anything else touches the
   SD card or SDL, derives the page and the width from it, checks them
   against the app's range, and passes them to `rando_app_shell_init`. A
   broken theme, a missing folder or an out-of-range result prints one line
   on stderr prefixed `alttpr: ` and exits 1, like an unreadable font.
   Without `--theme`, the app behaves exactly as today (grid defaults); step
   6 makes a theme mandatory.
3. **The interim grid follows the theme.** Until step 5 replaces the grid
   renderer, the SDL app's `UiGrid` is sized `width + 2` columns by
   `page + 4` lines, so the grid the old renderer draws and the page the core
   scrolls by never disagree. With `classic` that is 40×15: the same cells,
   the same font fit, the same pixels.
4. **`ui_cli` is unchanged**: it keeps deriving page and width from its grid
   (`--cols`, `--lines`, defaults 40×15). Its ranges are the same as the SDL
   app's, so step 9 can hand it `--cols width+2 --lines page+4`.
5. **Proof**: unit tests on the two functions (classic gives 11 and 38, equal
   to `ui_grid_page` and `UI_GRID_DEFAULT_COLS - 2`); the SDL smoke test runs
   the app on `classic` and on edited copies; `make sdl-pc` runs the golden
   comparison with `--theme` pointing at `classic` and still prints
   `goldens: 13 states byte-identical`.

## User Stories

1. As the agent building the new theme (phase 2), I want the page size derived from the list body's top, bottom and row height, so that moving to 24 px rows changes how many settings a page shows without touching code.
2. As the agent building the new theme, I want the wrap width derived from the list's label x, right edge and the list font's advance, so that the spoiler wraps where the new font's text area ends.
3. As the agent building the new theme, I want going back to 16 px text to be a theme edit that also changes the page and width, so that the plan's "going back to 16 px must stay a theme edit" holds for scrolling too.
4. As the agent, I want the derivation in pure C under `make test`, so that a wrong page or width is caught in seconds without SDL.
5. As the agent, I want `classic` to give exactly 11 and 38, checked against `ui_grid_page` and the grid's defaults, so that the step changes nothing the user sees.
6. As the agent, I want the page to floor partial rows, so that a row cut by the body's bottom is never counted as visible.
7. As the agent, I want the columns to floor partial characters, so that the wrapped text never runs past the list's right edge.
8. As the agent, I want the columns measured from the no-cursor label x, so that the width matches where History detail draws its wrapped lines (mockup 07: x30, no cursor).
9. As the agent, I want the columns to ignore the cursor label x, so that a theme's cursor column never changes the wrap width.
10. As the agent writing the renderer (step 5), I want the column function to take the advance as an argument, so that a proportional-layout font can pass its measured `M` advance once SDL_ttf opens it.
11. As the owner, I want `alttpr --theme <dir>` to read the look's page and width from that folder, so that the device app follows the theme.
12. As the owner, I want a missing theme folder to fail with one line naming `theme.ini` and exit 1, so that it fails as loudly as a missing font.
13. As the owner, I want a broken `theme.ini` to fail with the loader's own line (file, line, key, problem) prefixed `alttpr: `, so that I know exactly what to fix.
14. As the owner, I want a theme whose page or width is outside what the app can show to fail with one line giving both values and the allowed ranges, so that a typo in a row height never crashes the app on an assertion.
15. As the owner, I want `alttpr` without `--theme` to behave exactly as before, so that `launch.sh` and the package keep working until step 6 ships a theme.
16. As the owner, I want `--theme` without a value to be a usage error (exit 2, usage printed), so that it behaves like every other option.
17. As the owner, I want the usage line to list `--theme <dir>`, so that the option is discoverable.
18. As the owner, I want a failed theme load to leave the SD card untouched, so that a bad theme never writes anything.
19. As the plan pilot, I want the golden comparison in `make sdl-pc` run with `--theme` on `classic`, so that pixel identity is proven on the theme-derived page and width, not on the defaults.
20. As the plan pilot, I want the smoke test to show that a theme with another row height and label x really changes the page and width reaching the core (a text capture of `page + 4` lines and `width + 2` columns), so that the derivation is not only computed but used.
21. As the plan pilot, I want the smoke test to show that `classic` gives the same text capture as no theme, so that the two paths agree.
22. As the plan pilot, I want `ui_cli`'s output and every snapshot unchanged, so that the text renderer's contract is untouched.
23. As the plan pilot, I want `make sdl-arm` to still compile and link the SDL main with the theme module, so that the device build follows.
24. As the agent working on step 9, I want the SDL app's interim grid to be exactly `width + 2` by `page + 4`, so that `ui_cli --cols width+2 --lines page+4` reproduces the SDL app's page and width.
25. As a maintainer, I want the range the SDL app accepts to be the same as `ui_cli`'s `--cols` and `--lines` ranges, so that any theme the SDL app accepts can be played through `ui_cli`.
26. As a maintainer, I want a theme whose list font has no cell refused by the SDL app with one line in this step, so that the width is never computed from a guessed advance before step 5 can measure the font.

## Implementation Decisions

### The derivation (theme module, pure C99)

- Two functions join the theme module's public header (the module of step 3,
  H-3-1), with no include of the core or SDL:
  - `int theme_page(const Theme *theme)`: `(list.bottom − list.top) /
    list.row_height`, integer division (floor). The loader's cross-checks
    guarantee at least one row (H-3-4), so the result is ≥ 1.
  - `int theme_columns(const Theme *theme, int advance)`: `(list.right −
    list.label_x) / advance`, integer division. `advance ≥ 1` is the caller's
    contract (asserted). The cursor label x (`list.label_x_cursor`) is not
    used: the core's width serves only History detail's wrapped spoiler text,
    which has no cursor.
- Neither function checks the result against the app's limits: the theme
  module does not know the core's constants. The SDL main does (below).
- The notice body never affects the page (step 3's format: the page comes
  from `[list]` only, H-3-9).

### The SDL main

- New option `--theme <dir>`, listed in the usage line; a missing value is
  `fail_usage` (exit 2).
- With `--theme`, right after the arguments are parsed and the SD root is
  resolved, and before `rando_app_shell_init`, `SDL_Init` and `TTF_Init`:
  1. `theme_load` on the folder; on failure, `fail_setup` with the loader's
     message as the only text, so stderr reads `alttpr: <dir>/theme.ini:...`.
  2. The advance is the list font's cell width. If the list font has no cell,
     `fail_setup` with `<dir>/theme.ini: list.font: no cell, the width needs
     the font's metrics` (removed by step 5, which measures the font's `M`
     advance once it opens fonts through SDL_ttf and passes it to
     `theme_columns`).
  3. `page = theme_page`, `width = theme_columns(theme, advance)`.
  4. Range: page 1..`UI_GRID_LINES_MAX − 4` (36) and width
     `UI_HISTORY_WIDTH_MIN`..`UI_HISTORY_WIDTH_MAX` (20..94), the same as
     `ui_cli`'s `--lines` (5..40) and `--cols` (22..96) minus the grid's
     margins. Outside: `fail_setup` with `<dir>/theme.ini: page <p>, width
     <w>: the app shows page 1..36, width 20..94`.
  5. The grid is initialised `width + 2` columns by `page + 4` lines, and
     the core gets `page` and `width`; everything after (the view opened on
     the grid's cols and lines, the loop, `--shot`, `--shot-text`) is
     unchanged.
- Without `--theme`: exactly today's code path, 40×15 grid, page and width
  from the grid.
- The `Theme` struct is static in the main, like the shell and the grid (the
  main already keeps large state static to stay off the stack).
- `--theme` takes no part in finding the font: `--font` still names the font
  file; theme fonts and images are resolved by step 5.
- `ui_cli` gets no `--theme`: it keeps its grid-derived values.

### Scripts and build

- `goldens.sh` accepts an optional trailing `--theme DIR` (after `--write
  DIR` or `--compare DIR`) and then passes `--theme DIR` to every `alttpr`
  run; its usage line says so. `make sdl-pc` calls the comparison with
  `--theme ../../themes/classic`. `make sdl-goldens` is unchanged (it is
  refused anyway, the goldens exist). The fingerprint is unchanged: the theme
  is part of the repository, not of the environment.
- `smoke_sdl.sh` takes the theme folder as a third argument (`ALTTPR FONT
  THEME`); `make sdl-pc` passes `../../themes/classic`. Its broken and edited
  themes are copies of `classic` made under its temporary folder.
- The SDL main is compiled with the include path that has the theme module
  and linked with the app archive that holds it (H-3-5); `sdl-arm` already
  compiles every app source and the main with that include path, so it needs
  no change beyond what step 3 did. If H-3-5 does not hold, this step adds
  the theme module to both the `sdl-pc` and `sdl-arm` builds.
- No change to `app/core/`, `app/sd/`, `rando/`, `ui_cli`, the snapshots or
  the goldens.

## Implementation Chunks

1. **The derivation.** `theme_page` and `theme_columns` in the theme module,
   with their tests in `test_theme`. Done: `make test` and `make lint` green.
2. **The SDL app reads a theme.** `--theme` in the SDL main (load, advance,
   derive, range, grid size, shell init), `smoke_sdl.sh` and `goldens.sh`
   taking the theme, `make sdl-pc` passing `classic`. Done: `make test`
   green; `make sdl-pc` really run in the pinned environment (step 1's notes:
   a `setup:` line does not verify), with the smoke test passing and
   `goldens: 13 states byte-identical`; `make sdl-arm` (docker) links.

## Testing Decisions

- A good test drives the public interface only: a theme text in, the page and
  the columns out; the SDL app's exit status, stderr and `--shot-text`
  capture for a given theme folder. No test reads the main's internals.
- **`test_theme`** (unit, `make test`; prior art: step 3's own `test_theme`,
  which builds broken themes from `classic`'s text with one line edited):
  - `classic` loaded from `../../themes/classic`: `theme_page` is 11 and
    equals `ui_grid_page` of a default 40×15 grid; `theme_columns` with the
    list font's cell width (16) is 38 and equals `UI_GRID_DEFAULT_COLS − 2`.
  - Floors, each from `classic`'s text with one value edited: row height 24
    → 14 pages (352/24); row height 30 → 11; label x 20 → 37 columns
    (604/16); `theme_columns(classic, 12)` → 50 (608/12).
  - `list.label_x_cursor` edited to 100 leaves the columns at 38.
  - A notice row height edited leaves the page at 11.
- **`smoke_sdl.sh`** (in `make sdl-pc`, prior art its own `expect` and
  `shown` helpers):
  - `--theme <classic> --frames 1 --shot-text` exits 0 and its text capture
    is byte-identical to the same run without `--theme`.
  - A copy of `classic` with `list.row_height = 64` and `list.label_x = 208`
    (page 352/64 = 5, width 416/16 = 26) exits 0 and its text capture has 9
    grid lines of 28 columns.
  - A missing folder: exit 1, exactly one stderr line, starting `alttpr: `
    and naming `theme.ini`; the SD tree is untouched.
  - A copy with an unknown key appended: exit 1, one line naming
    `theme.ini:<line>`.
  - A copy with `list.label_x = 400` (width 224/16 = 14): exit 1, one line
    with `page 11, width 14`.
  - A copy whose list font has `size = 16` and `cell = none`: exit 1, the
    `no cell` line.
  - `--theme` with no value: exit 2 with the usage.
  - The edited keys are step 3's names (H-3-2, H-3-3); if the loader's
    cross-checks require a companion edit for a copy to load (e.g. a band's
    height against the row height), the copy makes it, and the expected page
    and width are recomputed from the copy's values.
- **Pixel identity**: `make sdl-pc` with the comparison run under `--theme`
  on `classic`, 13 states byte-identical, in the pinned environment.
- **Unchanged**: every snapshot, `smoke_ui_cli`, `ui_cli`'s output; `make
  lint`, `no_sdl.sh` and `no_literals.py` green (`sdl_main.c` is exempt from
  `no_sdl.sh` and outside `no_literals.py`'s directories, so its new messages
  need no table entry, like `fail_setup`'s existing ones).

## Out of Scope

- Drawing from the theme, opening theme fonts or images, measuring a font's
  advance (step 5); the SDL app still draws through the grid with `--font`.
- A default theme folder, making `--theme` mandatory, packaging a theme,
  `launch.sh` (step 6).
- `ui_cli --theme`; `ui_cli` keeps `--cols` and `--lines`.
- Page or width from the notice body; the help line's length (steps 8 and
  11).
- Any change to the core's page and width semantics, to `rando_app_shell_init`,
  to `app/core/`, `rando/`, the snapshots or the goldens.

## Hypotheses about step 3

Step 3 (`docs/prd/triage/phase1-step3-theme-loader.md`) is being
implemented while this PRD is written; its PRD was read, its code was not.
Everything this PRD assumes about its outcome:

- **H-3-1**: the loader is a pure C99 module `app/theme/` with a public
  header exposing `Theme`, `ThemeError` (a fixed message buffer) and `bool
  theme_load(Theme *out, const char *dir, ThemeError *err)`, whose message
  starts `<dir>/theme.ini` (for a missing file: `<dir>/theme.ini: cannot
  open: <strerror>`). If names differ, this step uses step 3's names and
  adds its two functions to that module's header.
- **H-3-2**: `classic` is `themes/classic/theme.ini` at the repository root,
  reached from `tests/rando` as `../../themes/classic`, with `[list]`
  `top = 64`, `bottom = 416`, `row_height = 32`, `label_x = 16`,
  `label_x_cursor = 16`, `right = 624`, and `[font.mono]` `size = fit`,
  `cell = 16 32`, the list's font. If any of these differs, the page and
  width derived from it must still be 11 and 38 (the plan's check); if they
  are not, `classic` is wrong and is fixed in this step, stated.
- **H-3-3**: the `Theme` struct exposes the list section's `top`, `bottom`,
  `row_height`, `label_x`, `right` as ints, its font as an index into the
  font array, and each font's cell as present/absent with its width and
  height. The functions read those fields whatever their exact names.
- **H-3-4**: the loader refuses a list body with fewer than one row
  (`top < bottom`, at least one row between) and `label_x ≥ right`, so
  `theme_page ≥ 1` and the columns' numerator is positive. If it does not,
  `theme_page` returning 0 is caught by the SDL main's range check.
- **H-3-5**: `app/theme/*.c` is in the app archive and `app/theme` on the
  include path the SDL main and `sdl-arm` compile with (`TEST_INCLUDES`),
  so the SDL main can include the header and link the module in `sdl-pc` and
  `sdl-arm` with no build change.
- **H-3-6**: `test_theme` is in `UNIT_TESTS`, runs from `tests/rando`, and
  already builds edited variants of `classic`'s text; this step adds cases to
  it rather than a new test binary.
- **H-3-7**: step 3 adds no page or width function and does not change the
  SDL main (its Out of Scope: "Page size and width computed from the theme
  and given to the core (step 4)"); the SDL app has no `--theme` after it.
- **H-3-8**: the loader never opens fonts or images, and `themes/classic/`
  holds no font file yet (step 5), so this step opens none and keeps
  `--font`.
- **H-3-9**: the format says the page the core scrolls by comes from `[list]`
  only; `[notice]` (at most 3 rows, no scroll) has its own row height that
  never reaches the core.
- **H-3-10**: `smoke_sdl.sh` and `goldens.sh` are as committed in step 1
  (`ALTTPR FONT` and `ALTTPR FONT SD_FIXTURE --write|--compare DIR`); step 3
  does not change their arguments. If it does, this step's argument changes
  follow its form.

## Further Notes

- **Why size the interim grid from the theme.** The old renderer draws
  `ui_grid_page(grid)` body lines whatever page the core was given; a theme
  with another page would scroll one way and draw another. Sizing the grid
  `width + 2` by `page + 4` keeps the two equal, costs one line, gives
  `classic` its 40×15 grid back, and is exactly the size step 9 hands to
  `ui_cli`. Step 5 removes the grid from the SDL drawing path; step 6
  deletes the old renderer.
- **Why `--theme` is optional now.** Making it mandatory would break
  `launch.sh` and the package before step 6 ships a theme next to the
  binary. The no-theme path is today's code and is still exercised by the
  smoke test's other runs.
- **Why the label x without a cursor.** The core's `width` bounds only History
  detail's wrapped lines (setting rows and spoiler text, no cursor: mockup 07
  draws them at x30, the no-cursor column). With a cursor, labels are cut by
  the renderer at draw time, not wrapped by the core.
- **The range is `ui_cli`'s.** `ui_cli` accepts `--cols` 22..96 and
  `--lines` 5..40, i.e. width 20..94 and page 1..36; the core's own limits
  (`UI_HISTORY_WIDTH_MIN/MAX`, `UI_ROWS_MAX` 64) are wider or equal, so a
  value accepted here never trips a core assertion.
- The plan's verification: `make test`; `classic`'s values equal today's (11
  and 38) in `test_theme`; goldens byte-identical through `make sdl-pc`.
