---
title: "SDL renderer from UiScreen, drawn from the theme"
description: "The SDL app still draws a character grid with a hard-wired palette, so the theme that step 3 parses has no effect on what the owner sees."
status: needs-triage
---

## Problem Statement

Plan 2 moves the SDL app's look into a theme folder read at launch. Steps 3
and 4 give a parsed `Theme` (zones, fonts, colours, markers, bands) and use
it for the page size and wrap width. But the pixels still come from the old
path: the core's `UiScreen` is flattened into a 40×15 `UiGrid`, and
`sdl_view_draw` paints that grid, one glyph per cell, in `sdl_view_palette`'s
colours, with the font named by `--font`. Nothing in the theme changes a
pixel. The app also cannot load a PNG, so a theme's background image,
sprites and button pictures cannot be drawn.

Phase 2 builds a new look entirely as a theme (frame, Press Start 2P,
sprites). That only works if a renderer draws every zone from the theme, and
if that renderer is first proven right on a theme that must reproduce
today's pixels exactly: `classic`, compared against step 1's 13 goldens.

## Solution

1. **A theme renderer in the SDL layer.** It opens a theme's fonts (through
   SDL_ttf, with `fit` sized as today) and images (through SDL_image), then
   draws a `UiScreen` into a 640×480 surface zone by zone, following the
   drawing rules that step 3's format fixed: screen colour, background image,
   bands, tabs with their markers, title and its optional dashed rule, list
   rows in every variant (setting, selected with arrows, disabled, selected
   disabled, action, text), scroll marks, notice rows with emphasis, message
   line, footer hints with their keys as text or sprites.
2. **SDL_image wired into the build**: `sdl-pc`'s `setup:` check and flags,
   `sdl-arm`'s include path and link line, the CI `sdl` job's
   `apt-get install`.
3. **`classic` becomes a full theme folder**: its `theme.ini` names a
   background PNG (640×480, solid `#14161a`), and the folder holds that PNG
   and DejaVu Sans Mono (a copy of `data/`'s file, with its licence).
4. **`alttpr --theme <dir>` draws through the theme.** With `--theme`, every
   frame is drawn by the theme renderer with the theme's fonts; `--font` is
   refused alongside it. Without `--theme`, the old grid renderer and
   `--font` still work, unchanged, until step 6 deletes them.
5. **Loud failures**: a missing theme folder, a broken `theme.ini`, a missing
   or unreadable image, a missing font, a font that fits no cell, a
   background of the wrong size, or a screen with more tabs than the theme
   has slots: one stderr line naming the file (and key) and the problem,
   exit 1, the SD card untouched.
6. **Proof**: every golden of step 1 byte-identical under `classic`
   (`make sdl-pc` runs the comparison with `--theme`); an SDL test draws many
   synthetic screens through both renderers and requires identical pixels;
   feature tests on a test-only theme cover what `classic` does not use
   (images, sprites, run fonts, baseline anchor, band lines, title rule,
   irregular tab slots, footer flow with sprites).

## User Stories

1. As the owner, I want `alttpr --theme themes/classic` to look exactly like today's app, so that the engine is proven before the look changes.
2. As the owner, I want the look to come from the theme folder alone, so that phase 2 can change it without rebuilding the app.
3. As the owner, I want a missing theme folder to fail with one line naming `theme.ini` and exit 1, so that it fails as loudly as an unreadable font does today.
4. As the owner, I want a broken `theme.ini` to fail with the loader's own line (file, line, key, problem), so that I know what to fix.
5. As the owner, I want a missing image to fail with one line naming the image file and the theme key that names it, so that a typo in a sprite name is found at launch.
6. As the owner, I want a missing or unreadable font to fail with one line naming the font file and its `font.<name>.file` key, so that the theme's fonts are checked at launch.
7. As the owner, I want a font whose `fit` finds no size to fail with one line giving the cell size, so that a too-small cell is not silently drawn with a wrong size.
8. As the owner, I want a background image that is not 640×480 refused with its actual size, so that a half-drawn screen is never shown.
9. As the owner, I want a screen with more tabs than the theme has tab slots to stop the app with one line, so that a tab is never silently missing.
10. As the owner, I want every failure at launch to leave the SD card untouched, so that a bad theme never writes anything.
11. As the owner, I want `alttpr` without `--theme` to behave exactly as before, so that `launch.sh` and the package keep working until step 6.
12. As the owner, I want `--font` together with `--theme` refused as a usage error, so that I never wonder which font was used.
13. As the owner, I want the device build to link against Onion's SDL_image, so that the app I copy to the Miyoo can load PNGs.
14. As the agent building the new theme (phase 2), I want the background image drawn under everything, so that `frame.png` carries the frame, the panel gradient, the separators and the hearts.
15. As the agent building the new theme, I want sprite markers drawn at their positions in their own pixels, so that the triforce cursor and the arrows look as drawn.
16. As the agent building the new theme, I want each footer button drawn as its sprite when the theme says so, and the hint flow measured on the sprite's real width, so that the START pill pushes its label to the right place.
17. As the agent building the new theme, I want proportional fonts drawn as one run on a baseline, so that Press Start 2P text sits where the mockups put it.
18. As the agent building the new theme, I want a value right-aligned to the value column and cut to its maximum width, with the arrows placed relative to the drawn value, so that settings line up as in the mockups.
19. As the agent building the new theme, I want text markers drawn in the colour of the text they belong to, so that `classic`'s `^` and `<` take their line's colour.
20. As the agent building the new theme, I want the selected row's band, with its top and bottom lines, drawn only on the selected row, so that the notched highlight of the mockups is a theme value.
21. As the agent building the new theme, I want the tab band drawn only when the screen has tabs and the footer band only when it has hints, so that mockups 02 and 06 have no footer band.
22. As the agent building the new theme, I want the title's dashed rule to start after the title's rendered width, so that it follows the title's length.
23. As the agent building the new theme, I want the title drawn in its alert colour on Generation Failed and Delete, so that the screen's tone shows.
24. As the agent building the new theme, I want an emphasised notice row drawn at its own x and colour, so that the file name of Seed Ready stands out.
25. As the agent building the new theme, I want the active tab drawn at its slot's active x with its markers, so that the mockups' irregular tab layout works.
26. As the agent building the new theme, I want the list's wrap width computed from the list font's real `M` advance when the font has no cell, so that a proportional font gives the right page width.
27. As the agent writing step 6, I want the old renderer still present but unused under `--theme`, so that removing it is a pure deletion.
28. As the agent writing step 6, I want the renderer's open and draw to need nothing but a `Theme`, its folder and a surface, so that finding `themes/default/` next to the binary is only a path change.
29. As the plan pilot, I want `make sdl-pc` to print `goldens: 13 states byte-identical` with the comparison run under `--theme` on `classic`, so that pixel identity is the step's proof.
30. As the plan pilot, I want an SDL test that draws the same synthetic screens through the grid renderer and the theme renderer and compares every pixel, so that edge cases the 13 goldens never reach (cut texts, long values, every scroll mark, overflowing footers) are proven too.
31. As the plan pilot, I want feature tests on a test-only theme with real PNGs, so that sprites, images, run fonts, baselines, band lines, rules and sprite buttons are proven by pixels, not only parsed.
32. As the plan pilot, I want the smoke test to cover a missing theme folder, a missing image, a missing font and a broken `theme.ini`, each with one stderr line and a non-zero exit, so that the plan's verification is met.
33. As the plan pilot, I want `make test` to check the list of a theme's assets with their key names, in pure C, so that a new sprite key that the renderer forgets to load fails in seconds.
34. As the plan pilot, I want `sdl-pc` to print a `setup:` line naming `libsdl-image1.2-dev` when SDL_image is missing, so that a missing dependency is a setup problem, not a bug.
35. As the plan pilot, I want `make sdl-arm` to compile the SDL layer with the theme header and link `-lSDL_image`, so that the device build follows the PC build.
36. As the plan pilot, I want the CI `sdl` job to install `libsdl-image1.2-dev`, so that a dispatched run builds.
37. As a maintainer, I want the SDL layer to keep no letter literal (`no_literals.py`), every message formed by the SDL main from a status and a key name the theme module provides, so that the existing rule holds.
38. As a maintainer, I want `classic`'s font copy proven byte-identical to `data/`'s, so that the goldens' font fingerprint still describes what is drawn.
39. As a maintainer, I want the frame loop to hand the presenter the `UiScreen` as well as the grid, so that the theme renderer draws from the core's description and `--shot-text` keeps its grid capture.
40. As a maintainer, I want CLAUDE.md's description of `make sdl-pc` and of `app/sdl/`'s dependencies kept true, so that the next session reads the right prerequisites.

## Implementation Decisions

### Modules

- **Theme module (pure C99, `make test`)**: one new function, the asset list.
  `theme_assets` fills a caller array with every file the theme references,
  each with its kind (font or image), its file name and its full key name as
  the loader's messages spell it (`screen.image`, `font.<name>.file`,
  `tabs.active_before`, `tabs.active_after`, `list.cursor`,
  `list.cursor_disabled`, `list.arrow_left`, `list.arrow_right`,
  `list.scroll_up`, `list.scroll_down`, `footer.button.left_right`,
  `footer.button.a`, `.b`, `.x`, `.y`, `.start`), in that order, fonts in
  `Theme.fonts` order, only markers and buttons of kind sprite, the image
  only when present. A file named by several keys appears once per key.
  It returns the count; `THEME_ASSETS_MAX` (32) is enough for 1 image,
  8 fonts and 14 sprite keys. The key names live here because
  `no_literals.py` forbids letter literals under `app/sdl/`. The same module
  gains the path join, `<dir>/<file>` into a caller buffer, false when it does
  not fit.
- **Theme renderer (new, `app/sdl/`)**, the deep module of the step. Public
  interface (names final unless they clash):
  - `SdlRender`: a caller-owned struct (static in the main, like `SdlView`),
    holding the opened fonts, the loaded images by asset index, a glyph cache
    and a pointer to the `Theme`, which must outlive it.
  - `SdlRenderFailure`: which asset failed (its index into `theme_assets`'
    list), the reason (`open`, `fit`, `size`, `path`) and a copy of SDL_ttf's
    or SDL_image's error text (the error is cleared by `SDL_Quit`).
  - `bool sdl_render_open(SdlRender *r, const Theme *theme, const char *dir, SdlRenderFailure *f)`:
    TTF and IMG must already be initialised (caller's contract); SDL video
    need not be. Opens each font: a fixed size as given; `fit` by today's
    search (64 down to 8, `M` advance ≤ cell W and font height ≤ cell H).
    Loads each image with `IMG_Load`; the screen image must be exactly
    640×480 (`size` failure otherwise) and is converted once to the
    renderer's surface format (32-bit, masks `0x00FF0000`, `0x0000FF00`,
    `0x000000FF`, no alpha: the main's offscreen format); sprites keep their
    own format and alpha. On failure, everything opened so far is closed.
  - `int sdl_render_advance(const SdlRender *r)`: the list font's advance:
    its cell W when it has a cell, else its `M` advance from
    `TTF_GlyphMetrics`. The main passes it to `theme_columns`.
  - `bool sdl_render_draw(SdlRender *r, const UiScreen *screen, SDL_Surface *dst)`:
    draws the whole frame; `dst` is 640×480 in the renderer's format
    (asserted). Returns false, drawing nothing, when the screen has more tabs
    than the theme's tab slots; the main reports it. The page is
    `theme_page(theme)`, the value the core was given.
  - `void sdl_render_close(SdlRender *r)`.
- **Inside the renderer**, a text layer that the zones share (an internal
  split, the implementer may make it its own file): measure and draw a text
  in a font at an anchor with a room and a cut mark; cell fonts draw each
  byte as its own one-character `TTF_RenderText_Blended` surface (today's
  `glyph`), centred when narrower than W, clipped to its cell, spaces
  skipped; run fonts render the string once per draw with
  `TTF_RenderText_Blended` and free it after the blit. Bytes outside
  0x20–0x7E are drawn as `?`, as the grid's pen does.
- **Glyph cache**: per font, up to 16 colours × 95 glyphs, a colour's slot
  taken on first use; glyphs of a 17th colour are rendered and freed on each
  draw, never cached. Classic uses 11 colours.
- **Frame loop**: the present callback becomes
  `bool present(void *ctx, const UiScreen *screen, const UiGrid *grid)`.
  The loop still renders the grid every frame (for `--shot-text`) and passes
  both; a false return ends the run with a new result
  `SDLLOOP_PRESENT_FAILED`. The old path's presenter ignores the screen.
- **The old renderer** (`sdl_view_*`, `sdl_view_palette`) is untouched and
  still used without `--theme`; step 6 deletes it.

### Drawing rules (step 3's format, made exact for pixel identity)

Step 3's "Drawing semantics" stand; these decisions settle what it left open.

- **Order**: fill with `screen.colour`; blit the screen image (no alpha,
  a plain copy); bands (tabs band, selected-row band, footer band); then
  sprites and text, zone by zone (tabs, title and rule, body, message,
  footer). Glyphs are clipped to their cell, so in `classic` no glyph
  overlaps another line's band and the order gives the grid's pixels.
- **Cut**: a text that overflows its room is cut so that the cut mark ends
  inside the room. Cell font: with n = floor(room / W) cells, the first n − 1
  bytes are drawn and the mark fills cell n − 1 (today's `pen_done`); n = 0
  draws nothing. Run font: trailing characters are dropped until the kept
  text plus the mark fit, and the mark is drawn right after the kept text.
  The mark is drawn in the colour of the text it cuts.
- **Footer cut is one flow**: the hints are laid out key, `key_gap`, label,
  `hint_gap`, … from `footer.x`. If the flow ends beyond `footer.right`, the
  cut applies to the flow as a whole: with a cell font the mark takes the
  last whole cell of the room counted from `footer.x` and nothing of the flow
  is drawn from that cell on (a gap included, as the grid does); with a run
  font the overflowing piece is cut as a run, later pieces are dropped, and a
  sprite key that does not fit is dropped with the mark drawn at its x if
  the mark fits. A text key is drawn in its label's colour.
- **Row colours by state** (label / value; arrows take the value colour,
  cursor and scroll marks the label colour): selected and disabled →
  `selected_disabled_colour`; disabled → `disabled_colour`; selected →
  `selected_colour`; text row → `text_colour` (label); otherwise
  `label_colour` / `value_colour`. A scroll mark on a line with no row uses
  `label_colour`.
- **List rows**: rows `scroll` … `scroll + page − 1` that exist, row k at
  `list.top + k·row_height`. The selected band (or the selected-disabled one)
  on the cursor's row when visible. Setting rows: value right-aligned to
  `value_right` after being cut to `value_max`; the left arrow ends
  `arrow_left_gap` before the drawn value's left edge, the right arrow starts
  `arrow_right_gap` after `value_right`; arrows only on the selected, enabled
  setting, but the label's room always ends `label_gap` before where the left
  arrow would be. Action and text rows are cut at `list.right`. Labels start
  at `label_x_cursor` when `cursor ≥ 0`, else `label_x`. Up mark on the first
  page line when `scroll > 0 || more_above`, down mark on the page's last
  line when `scroll + page < row_count || more_below` (the grid's tests).
- **Notice rows**: from `scroll`, at most `page` rows, in the notice font,
  colour and geometry; no cursor, no band, no scroll marks (notice screens
  have none: H-4-9's goldens `generating`, `seed_ready`,
  `generation_failed`, `history_empty`, `history_confirm` prove it).
- **Markers**: a text marker is drawn in the zone's font with the font's
  anchor applied to its y; a sprite marker's y is its top. A marker's width
  (for "right after the text" and "ends before the value") is the text's
  measured width or the sprite's `w`.
- **Tabs**: slot i's text at `x` or `active_x`, cut at `tabs.right`;
  `active_before` at (`marker_x`, `marker_y`), `active_after` right after the
  active text's end. More tabs than slots: draw returns false.
- **Title**: drawn only when non-empty, in `colour` or `alert_colour` by
  `title_tone`; the rule only with a non-empty title, dashes of `dash` px
  every `dash + gap` px from the drawn title's end + `offset`, the last dash
  clipped at `end`, `thickness` px tall from the rule's `y`.
- **Message**: drawn when `message_kind` is not none, `info_colour` or
  `error_colour`.
- **Bands**: rectangle in `colour`, then `top_line` px of `line_colour` at
  its top and `bottom_line` px at its bottom, inside the rectangle.
- **Sprites with alpha** are blitted with SDL's per-pixel alpha; a sprite
  without alpha is a plain copy. `classic` has no sprite.

### The SDL main under `--theme`

Built on step 4's `--theme` (H-4-2 to H-4-5):

1. Parse arguments; `--font` and `--theme` together is `fail_usage`
   (exit 2) naming both.
2. Resolve the SD root; `theme_load` (failure: the loader's message, as step
   4 prints it).
3. `SDL_Init(SDL_INIT_VIDEO)`, `TTF_Init`, `IMG_Init(IMG_INIT_PNG)` (failure
   when PNG support is absent: `IMG_Init` and `IMG_GetError`).
4. `sdl_render_open`; on failure one line, formed in the main from the
   failure and `theme_assets`' entry:
   - `open`: `alttpr: <dir>/<file>: <key>: <SDL_ttf or SDL_image error>`;
   - `fit`: `alttpr: <dir>/<file>: <key>: no size from 64 to 8 fits the <W>x<H> cell`;
   - `size`: `alttpr: <dir>/<file>: screen.image: <w>x<h>, the screen is 640x480`;
   - `path`: `alttpr: <dir>/<file>: <key>: path too long`.
5. Page `theme_page`, width `theme_columns(theme, sdl_render_advance)`,
   then step 4's range check and grid sizing (`width + 2` × `page + 4`);
   step 4's "list font has no cell" refusal is removed (its PRD says step 5
   removes it).
6. `rando_app_shell_init`, then the video mode and the offscreen as today.
7. The presenter draws with `sdl_render_draw`; a false return ends the loop
   and the main prints `alttpr: <dir>/theme.ini: the screen has <n> tabs, the
   theme <m> tab slots` and exits 1.
8. `--shot` saves the offscreen and `--shot-text` the grid, as today.
   Shutdown closes the renderer, then `IMG_Quit`, `TTF_Quit`, `SDL_Quit`;
   `fail_setup` gains `IMG_Quit`.

Every message is formed in the SDL main (exempt from `no_sdl.sh`, outside
`no_literals.py`), so `app/sdl/` stays free of letter literals.

### `classic` (`themes/classic/`)

- `theme.ini`: `screen.image = background.png`; nothing else changes.
- `background.png`: 640×480, 8-bit RGB (colour type 2, no alpha), every pixel
  `#14161a`, no `gAMA`, `sRGB`, `iCCP` or `cHRM` chunk, so decoding is exact
  whatever the libpng. Written once by a throwaway stdlib Python script in
  `tmp/`; only the PNG is committed.
- `DejaVuSansMono.ttf` and `DejaVuSansMono-LICENSE.txt`: byte copies of
  `data/`'s. A copy, not a symlink (the zip of step 6 and the docker mount
  must see a plain file). `data/` keeps its copy until step 6 decides the
  package layout.
- Deliberate test changes, stated: `test_sdl_classic` and `test_theme`
  assert `classic` has no image today; they now assert the image is present
  and named `background.png`. `test_sdl_classic` also asserts the two font
  copies are byte-identical and opens the font from the theme folder.

### Build and CI

- `sdl-pc` and `sdl-goldens`: the `setup:` check asks `pkg-config --exists
  sdl SDL_ttf SDL_image` and names `libsdl1.2-dev libsdl-ttf2.0-dev
  libsdl-image1.2-dev`; the SDL flags and libraries add `SDL_image`; the SDL
  layer's objects get the theme module's include path; the SDL tests link
  `SDL_image`.
- `sdl-arm`: the SDL sources compile with the theme include path; the link
  line becomes `-lSDL -lSDL_ttf -lSDL_image`. If the sysroot linker needs
  SDL_image's own dependencies resolved (libpng, zlib), the step adds the
  sysroot's library path to the link (`-Wl,-rpath-link`), stated.
- `make sdl-pc` runs the golden comparison with `--theme ../../themes/classic`
  (step 4 already passes it, H-4-5); `goldens.sh` no longer passes `--font`
  to `alttpr` when `--theme` is given, and still fingerprints the FONT
  argument (the data copy, byte-identical to classic's). The fingerprint
  and `environment.txt` are unchanged: PNG decoding is lossless, SDL_image's
  version does not move a pixel.
- CI `sdl` job: `apt-get install -y libsdl1.2-dev libsdl-ttf2.0-dev
  libsdl-image1.2-dev`.
- Not this step (step 6): `make package`'s NEEDED list, `targets/mac/`,
  `launch.sh`, `build_zip.py`. `targets/mac/` overrides the SDL flags without
  SDL_image, so the mac build does not link between steps 5 and 6: stated in
  the step notes for the owner, fixed by step 6.
- CLAUDE.md: `make sdl-pc` needs `libsdl-image1.2-dev` too; `app/sdl` uses
  the library, `app/core` and `app/theme`; the owner's look command gains
  the `--theme themes/classic` form.

## Implementation Chunks

1. **SDL_image in the build, the asset list, the renderer's open.**
   `theme_assets` and the path join with their `test_theme` cases; the
   build changes (`sdl-pc` setup check and flags, SDL objects' include path,
   `sdl-arm` link line, CI `apt-get`); `classic`'s background PNG, font copy
   and licence, `image = background.png`, the two test assertions updated;
   `sdl_render_open`, `sdl_render_advance` and `sdl_render_close` with a new
   SDL test `test_sdl_render` covering open on `classic` (fit size equal to
   `sdl_view_open`'s, advance 16) and every failure (missing image, missing
   font, fit failure, wrong background size, path too long), each with the
   right asset index and reason. Done: `make test`, `make lint` green;
   `make sdl-pc` really run in the pinned environment with the goldens still
   byte-identical (nothing draws through the theme yet); `make sdl-arm`
   links with `-lSDL_image`.
2. **Text layer and the single-text zones.** Cell and run text, anchors,
   cut, the glyph cache, `?` for unprintable bytes; `sdl_render_draw` for the
   screen colour and image, tabs and markers, title and rule, message and
   footer (bands, flow, sprite keys, flow cut). `test_sdl_render` gains the
   equivalence harness (grid renderer with `data/`'s font against the theme
   renderer with `classic`, every pixel) on screens with no body rows, and
   feature tests for these zones on the test theme. Done: `make sdl-pc` green
   in the pinned environment.
3. **Body zones.** List rows in every variant, value placement and arrows,
   cursor markers, selected bands, scroll marks, notice rows with emphasis;
   the equivalence cases for bodies and their feature tests. Done: `make
   sdl-pc` green in the pinned environment.
4. **The app draws through the theme.** The loop's present signature and
   result, `test_sdl_loop` following it (plus a case where a false present
   stops the loop); the main under `--theme` (order, `IMG_Init`, failures,
   advance, tab-slot failure, `--font` exclusion); `goldens.sh` without
   `--font` under `--theme`; `smoke_sdl.sh` cases; CLAUDE.md. Done: `make
   test`, `make lint` green; `make sdl-pc` really run in the pinned
   environment, smoke test passing and `goldens: 13 states byte-identical`
   under `classic` (the final proof); `make sdl-arm` (docker) links against
   Onion's SDL_image.

## Testing Decisions

- A good test drives public interfaces only: a theme folder and a
  `UiScreen` in, pixels out; the app's exit status, stderr and captures for
  given arguments. No test reads the renderer's cache or internal layout.
- **`test_theme`** (unit, `make test`; prior art: its own cases on
  `classic` and the inventory fixture): `theme_assets` on `classic` gives
  exactly `screen.image background.png` then `font.mono.file
  DejaVuSansMono.ttf`; on the inventory fixture it gives the image, the three
  fonts and every sprite key in the stated order with its file; a theme
  with a text marker lists no entry for it; the path join refuses a result
  that does not fit.
- **`test_sdl_render`** (new SDL test in `SDL_TESTS`, `make sdl-pc`, dummy
  video driver; prior art `test_sdl_view` for pixel checks and
  `test_sdl_classic` for theme loading):
  - *Open*: `classic` opens; its font's fit size equals `sdl_view_open`'s on
    the same font; advance 16; a cell-less copy's advance equals the font's
    `M` advance; each failure on an edited copy written under
    `tests/rando/build/sdl/` gives its reason and the asset index whose key
    is the expected one.
  - *Equivalence with the grid renderer*: each case is a `UiScreen` built
    through the core's screen API, drawn once by `ui_grid_render` +
    `sdl_view_draw` (40×15, `data/`'s font) and once by `sdl_render_draw`
    on `classic`, compared pixel for pixel; a difference reports the case
    name, the first differing pixel and both colours. Cases: no tabs, tab 0
    and tab 1 active (English tab names, `classic`'s slot assumption); title
    empty, short, exactly 38 characters and 39 (cut); settings with values of
    0, 5, 32, 33, 34, 35 and 40 characters, each unselected, selected,
    disabled and selected-disabled; long labels cut; action and text rows,
    indented text rows, a text row of 39 characters; cursor on the first and
    last page line; every scroll-mark combination, including `more_below` on
    a short list (mark on an empty line) and a mark on a selected row; a
    notice body with an emphasised row; info and error messages, one cut;
    footers of 0 to 4 hints, a primary hint, and one overflowing in a gap;
    bytes outside 0x20–0x7E.
  - *Features* on a test-only theme (`tests/rando/sdl/themes/features/`:
    `theme.ini` and small committed PNGs; the build copies `data/`'s font
    into its folder under `tests/rando/build/sdl/`): DejaVu as a run font with
    baseline anchor, a two-colour `frame.png`, solid-colour sprites of
    distinct sizes for the tab marker, row cursor and its disabled variant,
    arrows, scroll marks and every button, band lines, a title rule, tab
    slots with `x` ≠ `active_x`. Checks: the frame's pixels where nothing is
    drawn; each sprite's colour at its expected rectangle; band line colours
    at the band's top and bottom rows; dash and gap pixels of the rule and
    its start at the title's measured end + offset; a run text compared with
    a direct `TTF_RenderText_Blended` blit at (x, y − ascent) on a scratch
    surface; a cut run text ending with the mark inside its room; the second
    hint's key at the first label's measured end + `hint_gap`, after a
    sprite key whose width pushes the label; primary colour; no footer band
    without hints, no tab band without tabs; emphasis x and colour; a
    right-aligned value's right edge; more tabs than slots returns false
    and leaves the surface unchanged.
- **`test_sdl_classic`**: the image assertion changes (stated above); the
  font copies are compared byte for byte.
- **`test_sdl_loop`**: the new present signature; a present returning false
  ends the run with `SDLLOOP_PRESENT_FAILED`.
- **`smoke_sdl.sh`** (`make sdl-pc`; prior art its `expect`, `shown` and SD
  listing checks, and step 4's theme cases): with copies of `classic` under
  its temporary folder: a missing theme folder; an unknown key appended; a
  missing image (`image = missing.png`); a missing font file
  (`file = missing.ttf`); a cell too small to fit (`cell = 4 4`); a
  background replaced by one of the feature fixture's small PNGs; each exits
  1 with exactly one stderr line starting `alttpr: ` and naming the file
  (and the key for the asset cases), the SD tree unchanged. `--font` with
  `--theme`: exit 2 with the usage. Step 4's "no cell" case becomes: the
  copy with `size = 16`, `cell = none` exits 0 and its text capture is wider
  than 40 columns (the exact width is `test_sdl_render`'s check), the
  deliberate change step 4's PRD announced. `--theme` on `classic` with
  `--shot` writes a 640×480 frame of more than one colour.
- **Pixel identity**: `make sdl-pc` prints `goldens: 13 states
  byte-identical` with `--theme ../../themes/classic`, in the pinned
  environment (plan rule: a `setup:` line does not verify).
- **Static checks**: `make lint`; `no_sdl.sh` (the theme module stays free of
  SDL); `no_literals.py` on `app/sdl/` with the renderer added; `make
  sdl-arm` (docker) links.

## Out of Scope

- Deleting `sdl_view_draw`, `sdl_view_palette` and `--font`; making `--theme`
  mandatory; `themes/default/` next to the binary; `make package`'s NEEDED
  check, `launch.sh`, `build_zip.py` and its self-test; `targets/mac/`
  (step 6).
- The new theme, Press Start 2P, frame and sprites drawn from the mockups
  (steps 8 and 12); `make theme-compare` (step 10); the help line and its
  keys (step 11).
- Any change to `app/core/`, `app/sd/`, `rando/`, `ui_cli`, the snapshots or
  the goldens (`environment.txt` included).
- Sprite scaling, animations, hot reload, user themes.

## Hypotheses about step 4

Step 4 (`docs/prd/triage/phase1-step4-page-width-from-theme.md`) is not done
when this PRD is written; its PRD was read, its code was not. Everything this
PRD assumes about its outcome:

- **H-4-1**: the theme module's header exposes `int theme_page(const Theme *)`
  (`(list.bottom − list.top) / list.row_height`) and `int
  theme_columns(const Theme *, int advance)` (`(list.right − list.label_x) /
  advance`), and `classic` gives 11 and 38. If the names differ, this step
  uses step 4's.
- **H-4-2**: `alttpr` takes `--theme <dir>`, listed in its usage; with it, the
  main loads the theme with `theme_load` before `rando_app_shell_init`,
  `SDL_Init` and `TTF_Init`, keeps the `Theme` static, and a load failure is
  `fail_setup` with the loader's message (`alttpr: <dir>/theme.ini…`). This
  step moves `SDL_Init`, `TTF_Init` and `IMG_Init` before the shell init on
  that path (the SD tree stays untouched on failure).
- **H-4-3**: under `--theme`, the advance is the list font's cell W, and a
  list font with no cell is refused with a `no cell` line; this step
  replaces that refusal with `sdl_render_advance`.
- **H-4-4**: the main checks page 1..36 and width 20..94 with one
  `fail_setup` line and sizes the grid `width + 2` × `page + 4`; this step
  keeps both, the grid now serving only `--shot-text`.
- **H-4-5**: `goldens.sh` takes an optional trailing `--theme DIR` passed to
  every `alttpr` run, `make sdl-pc` calls the comparison with `--theme
  ../../themes/classic`, and `smoke_sdl.sh` takes the theme folder as its
  third argument (`ALTTPR FONT THEME`). If the forms differ, this step's
  changes follow step 4's forms.
- **H-4-6**: without `--theme`, the main is today's code path (40×15 grid,
  `--font`, `sdl_view_draw`), and `--font` is independent of `--theme`; this
  step makes them exclusive.
- **H-4-7**: step 4 changes no drawing code and no build rule: the SDL main
  already reaches the theme header through the include path it compiles
  with, but the SDL layer's own objects (`app/sdl/*.c`) do not, which this
  step adds.
- **H-4-8**: `smoke_sdl.sh` already has step 4's theme cases (missing folder,
  unknown key, out-of-range page or width, `no cell`, an edited copy giving
  a smaller grid); this step adds its own next to them and changes only the
  `no cell` case.
- **H-4-9**: the goldens still pass after step 4 (`goldens: 13 states
  byte-identical` with `--theme` on `classic` through the grid), so the
  notice screens of the golden set have no cursor and no scroll marks, and a
  golden difference in this step is this step's bug.

## Further Notes

- **Why two renderers side by side for one step.** The 13 goldens prove the
  states the app reaches with the synthetic SD card; they never show a cut
  value, a 33-character value, an overflowing footer or a down mark on an
  empty line. Keeping the grid renderer until step 6 makes it a free oracle:
  the equivalence test proves the theme renderer on every corner of the
  grid's rules before the oracle is deleted.
- **Why the footer cut is defined on the flow.** The grid cuts the footer as
  one string, so its cut mark can land in the gap between two hints. Cutting
  each piece alone would leave that case without a mark; the flow rule gives
  the grid's pixels with a cell font and stays meaningful with a run font.
- **Why `--font` is refused with `--theme`.** A silently ignored option is
  the kind of trap the plan rejects ("the loader can be strict and fail
  loudly"); step 6 deletes `--font` anyway.
- **Why the background is a PNG in `classic`.** The plan asks for it, and it
  puts SDL_image on the golden path: a wrong decode, a wrong conversion or a
  forgotten blit changes every golden.
- **Why the key names come from the theme module.** `no_literals.py` forbids
  letter literals under `app/sdl/`; the renderer reports an asset index and
  the main formats the message from `theme_assets`, which `make test`
  checks without SDL.
- **Alpha at 255.** SDL 1.2's per-pixel alpha blit is exact for opaque
  pixels on the device and under sdl12-compat; `classic` blits no alpha
  surface but the glyphs (as today), so its pixels do not depend on it.
- **Risk for `make sdl-arm`**: linking against the sysroot's SDL_image may
  need its dependencies' path on the link line (`-rpath-link`); the chunk
  that adds `-lSDL_image` proves the link in docker.
- The plan's verification for this step: `make sdl-arm` links against
  Onion's SDL_image; `make sdl-pc` with every golden byte-identical under
  `classic`; the SDL tests (here `smoke_sdl.sh` and `test_sdl_render`) cover
  a missing theme folder, a missing image, a missing font and a broken
  `theme.ini`, each with one stderr line and a non-zero exit.
