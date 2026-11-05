# Plan 2 — Theme engine and the new look

## 1. Goal

The SDL app looks like the owner's mockups (`design/mockups/*.svg`): a
pixel-art window with a notched frame, Press Start 2P text, sprites for the
cursor, scroll arrows and buttons, and a help line for the selected setting
value. Today it draws a 40×15 monospace character grid with one colour pair
per line (`app/sdl/sdl_view.c`, `sdl_view_palette`), which the owner calls
"very, very ugly".

The look lives in a **theme**: a folder of data files read at launch. Changing
the look means editing that folder and relaunching the app: no rebuild, and no
test of the net has to change. *Goal: the visuals can be iterated on without
touching code or tests.*

The work is in two phases. Phase 1 builds the theme engine and a `classic`
theme that reproduces the current rendering pixel for pixel, so the engine is
proven before the look changes. Phase 2 builds the new theme from the mockups
and adds the per-value help text.

## 2. Rules

Locked decisions (owner, grilling of 2026-11-05):

- **Theme = a folder read at launch**: `theme.ini` (positions, sizes,
  colours, fonts, sprite names), `frame.png` (the full 640×480 background:
  frame, gradient panel, separators, the decorative hearts), sprite PNGs, and
  the TTF fonts it names. The code knows the screen's **zones** (tabs, title,
  body list, message/help line, footer) and draws `UiScreen`'s content into
  them; every position, size, colour, font and sprite comes from the theme.
  The exact file format, keys and zone list are this plan's to decide.
- **Rejected: the SVG as a live template** (rasterised at launch with
  nanosvg, elements found by `id`): nanosvg has no text, no `<use>`, no
  `@import`, so an SVG that renders in a browser but not in the app would be a
  permanent trap. **Rejected: a theme compiled in C**: changing it would mean
  rebuilding.
- **Rejected: hot reload and user themes.** Only the agent edits themes; the
  loader can be strict and fail loudly.
- **The agent builds the themes, not the owner.** The owner judges the result
  on side-by-side images.
- **The UI core stays a pure C state machine with no SDL** (CLAUDE.md ground
  rule). The SDL renderer draws from `UiScreen`, not from `UiGrid`; the grid
  stays the text renderer for `ui_cli` and the snapshots
  (`tests/rando/unit/snapshots/`). The theme loader's parsing is pure C where
  it can be (testable in `make test`); image and font loading is in
  `app/sdl/`.
- **The page size comes from the theme.** The number of body lines per page
  (and the text width the core wraps to, today `grid.cols - 2`) is derived from
  the theme at launch and given to the core (`rando_app_shell_init`'s `page`
  and `width`). Changing the font size or row height is a theme edit.
- **The footer becomes structured**: a list of hints, each a button (enum)
  plus a label from the translation table, one of them optionally marked
  primary (the mockups draw "Generate" in yellow). The text renderer rebuilds
  exactly today's strings (`</> Change  Y Default  START Generate`, `A Back`,
  `A Details  X Delete`, `B Back  X Delete`, `A Delete  B Keep`), so the
  footer's snapshots do not change. The theme maps each button to a sprite.
- **Help line, per value** (phase 2): each value of each setting gets a
  one-line English description of what that value does ("No glitches
  required. Standard logic only."), written by the agent from VT's
  `219fcafd` behaviour and alttpr.com's descriptions, about 60 strings, all
  through the translation table (`ui_strings_en.c`). The core exposes the help
  of the selected row's current value. A unit test bounds every help string's
  length so it fits on one line in the new theme. Action rows and History rows
  have no help.
- **The message replaces the help**: when the screen has a message (info or
  error: "Not supported with Goal", "Saved settings unreadable: defaults",
  "Seed deleted"…), it takes the help line's place.
- **The hearts are decoration**: part of `frame.png`, never driven by the
  code.
- **The settings shown are the app's real ones.** The mockups list Entrance
  Shuffle, Boss Shuffle, Enemy Shuffle, Enemy Damage and Enemy Health, which
  the app does not have (they are Entrance Randomizer and Enemizer, not VT),
  and omit Hints, Heart Beep and Spoiler in History. That is mockup filler,
  **not** a request for new settings.
- **The mockups are the reference, and the agent may edit them** so that they
  match what the app really shows: real setting names and values, real file
  names from seeds the app generates, real strings. Each edit is its own
  commit, with its reason. The mockup's *look* (colours, geometry, sprites) is
  not changed to suit the code.
- **Size: the zip's 12 px.** `00_settings_help.svg` (the first mockup) uses
  16 px text on 28 px rows (11 rows); the eight others use 12 px labels on
  24 px rows, 14 px titles, 16 px tabs. The 12 px set is the reference; from
  `00_settings_help.svg` only the help line is kept (10 px, cyan `#78d8f8`,
  under the list, in the message zone). Going back to 16 px must stay a theme
  edit.
- **Details the mockups leave out**, decided: a selected *and* disabled
  action ("Generate Seed" while "Not supported with Goal") is the selected
  band with greyed text; spoiler lines too long for the list wrap with an
  indent, as the app does today.
- **A missing or broken theme**: the app prints one line naming the file and
  the problem and exits non-zero, as it does today for an unreadable font.
- **The old grid renderer goes** once the `classic` theme reproduces it.
  `classic` stays in the repository as the test theme of the SDL tests.
- **Phase 1's proof is pixel identity.** Before the renderer changes, the
  current app's frame is captured for a fixed set of states; with `classic`,
  the new engine must give byte-identical pixels for each. Those captures are
  goldens: generated once, in their own commit, from the current code, never
  regenerated to get green.
- **Mockup comparison is outside the net.** `make theme-compare` writes, for
  each mockup, the mockup rendered by Chromium (Playwright, pre-installed),
  the app's capture of the same state, and a diff image side by side, with a
  difference score. Nothing fails on the score: text never matches to the
  pixel (SDL_ttf against a browser), so a threshold would be useless or
  fragile. Each mockup is paired with the key sequence that brings the app to
  its state, in the style of the e2e cases. The mockups load Press Start 2P
  from a local file, not Google Fonts.
- **Fonts**: Press Start 2P (SIL OFL 1.1) is committed under `data/` with its
  licence, like DejaVu Sans Mono, and shipped in the zip.
- **New dependency: SDL_image 1.2**, for PNG. Onion ships it
  (`docs/onion.md`, lines 168–196: `SDL_image.h` 1.2.12,
  `libSDL_image-1.2.so.0.8.4`). On the PC it is `libsdl-image1.2-dev`: added
  to `sdl-pc`'s `setup:` check, to the CI `sdl` job's `apt-get install`, to
  `make package`'s NEEDED check (`libSDL_image-1.2.so.0`), and to
  `targets/mac/` (sdl12-compat has no SDL_image; `targets/mac/README.md`
  says how, or the mac target is marked as not building yet, stated).
- **Never weaken a check** (CLAUDE.md): the snapshots change only in the
  commit that adds the help text, which is a deliberate change stated with its
  reason; nothing else edits an expected value.
- **The package** ships the new theme as the default; the app finds its
  theme next to itself (`launch.sh` runs it from `App/ALttPR/`), and the zip's
  entry list and `build_zip.py`'s self-test (`tests/rando/test_package.py`)
  follow.

Out of scope: new settings; translations other than English; animations;
sound; touching `rando/`.

## 3. Phases and steps

### Phase 1 — Theme engine with a `classic` theme identical to today

1. **Goldens of the current rendering.** A script outside `make test` drives
   the current SDL app (dummy video driver, `--shot`) through a fixed list of
   states (Generate top, middle and end of list, greyed rows and greyed
   selected action, info and error messages, Generating, Seed Ready,
   Generation Failed, History list, empty, detail with spoiler, confirm
   delete) and writes one BMP per state under `tests/rando/sdl/goldens/`.
   This needs a way to feed the SDL app a key sequence and a fixed seed and
   date, like `ui_cli`'s (`--seed`, `--date`, keys), added to `sdl_main.c`
   first, without changing what it draws. Verified: the script runs twice
   with byte-identical output, and the goldens are committed alone.
2. **Structured footer in the core.** `UiScreen.footer` becomes a list of
   hints (button, label, primary flag); every screen builds its hints; the
   text renderer prints today's strings. Verified: `make test` green with no
   snapshot changed, and the current SDL renderer still matches step 1's
   goldens.
3. **Theme loader (pure C).** Parses `theme.ini` into a theme struct: zones,
   row height, colours, font names and sizes, sprite names; rejects unknown
   keys, missing keys and out-of-range values with a message naming the file,
   line and key. Unit tests in `tests/rando/unit/` on valid and broken
   themes. Verified: `make test`.
4. **Page and width from the theme.** The SDL main computes the page size and
   wrap width from the loaded theme and passes them to the core; `ui_cli`
   keeps its grid-derived values. Verified: `make test`; with `classic` the
   values equal today's (`ui_grid_page`: 11 body lines; width 38).
5. **SDL renderer from `UiScreen`.** `app/sdl/` draws the zones from the
   theme: background image, tabs, title, body rows in their variants (setting,
   selected with `< >` markers, disabled, selected disabled, action, text),
   scroll markers, message line, footer hints with their sprites. Fonts
   through SDL_ttf, images through SDL_image. `classic` theme committed
   (DejaVu Sans Mono, today's palette, its background as a PNG). Verified:
   `make sdl-pc` with every golden of step 1 byte-identical under `classic`,
   and the SDL tests cover a missing theme folder, a missing image, a missing
   font and a broken `theme.ini` (each: one stderr line, non-zero exit).
6. **Old renderer removed, packaging follows.** The grid-based
   `sdl_view_draw` and `sdl_view_palette` are deleted; the SDL app requires
   `--theme <dir>` or finds `themes/default/` next to itself; `make package`
   ships `classic` as default for now, with `libSDL_image-1.2.so.0` in the
   NEEDED check; CI installs `libsdl-image1.2-dev`; `targets/mac/` follows.
   Verified: `make check`, `make sdl`, `make package` (docker), goldens still
   identical.
7. skill: check-net — the whole net green after phase 1.

### Phase 2 — The new theme from the mockups

8. **Mockups made true and comparable.** Press Start 2P and its licence
   committed under `data/`; every mockup in `design/mockups/` loads it from a
   local `@font-face` instead of Google Fonts; the content edited to the
   app's real settings, values and strings (see Rules), each edit its own
   commit; each mockup paired with the key sequence (and seed, date, SD
   fixture) that brings the app to its state. Verified: Chromium renders
   every mockup offline with the local font; every key sequence, played into
   `ui_cli`, gives a screen whose text matches the mockup's text.
9. **`make theme-compare`.** Outside the net, needs Playwright's Chromium and
   `make sdl-pc`'s prerequisites (else a `setup:` line). Writes, under
   `tests/rando/build/theme-compare/`, one side-by-side image per mockup
   (mockup, app capture, diff) and prints one score per mockup. Verified: it
   runs on `classic` and writes every image; the score of a mockup against
   itself is 0.
10. **Help text per value.** The about 60 strings in `ui_strings_en.c`, the
    core's help for the selected row's value, the message replacing it; the
    text renderer shows it in the message line, so the snapshots that show a
    setting row without a message change, in this commit, stated as the
    deliberate change. A unit test bounds every help string to the new
    theme's line (about 57 characters at 10 px in 572 px; the exact bound
    from the theme). Verified: `make test`.
11. **The new theme.** `themes/alttp/` (name the plan's to settle): `frame.png`
    and sprites (triforce cursor, ◀ ▶ ▲ ▼ arrows, d-pad, A, B, X, Y and START
    buttons) drawn from the mockups' own vector shapes, `theme.ini` tuned
    with `make theme-compare` until each capture matches its mockup's layout
    (same zones, same text positions to a few pixels, same colours). Verified:
    `make sdl-pc` loads and draws every state with it; the side-by-side images
    are left for the owner (`tests/rando/build/theme-compare/`) and listed in
    `notes/owner-todo.md`.
12. **Default theme switched.** The zip ships the new theme as default
    (`classic` stays in the repository for the SDL tests); `README.md` says
    where the theme lives and how to compare it. Verified: `make package`
    (docker) and `make check`.
13. skill: check-net — the whole net green at the end.

## 4. Done when

- `make -k check` is green.
- `make sdl-pc` is green, with every phase-1 golden byte-identical under
  `classic`.
- `make package` writes a zip that ships the new theme as default, and passes
  its ABI and NEEDED checks with `libSDL_image-1.2.so.0`.
- `make theme-compare` writes a side-by-side image for each of the nine
  mockups, listed in `notes/owner-todo.md` for the owner to judge.
- Changing a colour, font size or row height in the new theme's `theme.ini`
  and relaunching the app shows the change, with no rebuild and no test of the
  net changed (shown once by hand in the step notes).

## 5. Open questions

- None from the owner. The theme file format, the zone list, the new theme's
  folder name and the golden state list are this plan's to settle.

## 6. Step notes
