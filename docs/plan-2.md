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

- **A `setup:` line blocks a visual check.** From step 2 on, a step whose
  check runs `make sdl-pc`, the golden comparison, `make theme-compare` or a
  `--shot` capture is not verified by a `setup:` line, whatever CLAUDE.md
  says of setup problems: the pilot runs the check in the environment step 1
  pinned, or stops.

Out of scope: new settings; translations other than English; animations;
sound; touching `rando/`.

## 3. Phases and steps

### Phase 1 — Theme engine with a `classic` theme identical to today

1. **Goldens of the current rendering.** A script outside `make test` drives
   the current SDL app (dummy video driver, `--shot`) through a fixed list of
   states (Generate top, middle and end of list, greyed rows and greyed
   selected action, info and error messages, Generating, Seed Ready,
   Generation Failed, History list, empty, detail with spoiler, confirm
   delete) and writes one BMP per state under `tests/rando/sdl/goldens/`. This
   needs a way to feed the SDL app a key sequence, a fixed seed and date, and
   the base digests' overrides, like `ui_cli`'s (`--seed`, `--date`,
   `--source-md5`, `--base-md5`, keys), added to `sdl_main.c` first, without
   changing what it draws. The script builds its SD card from the synthetic
   base the way `ui_cli`'s smoke test does
   (`tests/rando/unit/smoke_ui_cli.c`), with no ROM, plus a fixture that makes
   generation fail for the failure state; step 9 reuses it. The comparison
   against the goldens is wired into `make sdl-pc` (outside the net) in this
   step, so every later step can run it. The goldens' pixels depend on the
   host's SDL_ttf and FreeType: the step names the environment the goldens are
   made and compared in (recorded in the step notes), and settles where the
   comparison runs if it differs between that environment and the CI `sdl`
   job. Verified: every listed state is reached on that synthetic SD card,
   with no ROM on the host; the script runs twice with byte-identical output;
   the comparison also passes in a second environment (the CI `sdl` job,
   dispatched by hand) or the step records where it is pinned to run; the
   comparison has really run and passed (not a `setup:` line) in the
   environment named for the later steps, and that environment can also
   install SDL_image and run Playwright's Chromium (recorded in the step
   notes); the goldens are committed alone.
2. **Mockup inventory and the core changes it calls for.** Every element of
   the nine mockups is mapped to where it comes from: `frame.png`, text in a
   zone, a sprite, or a style variant driven by a `UiScreen` field (for
   example the dashed rule that starts after the title's text, the red title
   of Generation Failed and Delete, the yellow indented file name of Seed
   Ready). The inventory goes in the step notes; step 3's format must express
   it. Each element the core does not expose yet becomes a core change here,
   the structured footer among them: `UiScreen.footer` becomes a list of
   hints (button, label, primary flag), every screen builds its hints, the
   text renderer prints today's strings. Verified: the inventory covers all
   nine mockups; `make test` green with no snapshot changed; the current SDL
   renderer still matches step 1's goldens.
3. **Theme loader (pure C).** Parses `theme.ini` into a theme struct: zones,
   row height, colours, font names and sizes, sprite names; rejects unknown
   keys, missing keys and out-of-range values with a message naming the file,
   line and key. `classic`'s `theme.ini` is written in this step, so the
   format is proven able to express today's drawing (one glyph per cell,
   centred and clipped; the band and text colours of `sdl_view_palette` per
   line style; the font size chosen by fitting a cell) before anything is
   built on it, and every entry of step 2's inventory is expressible. Unit
   tests in `tests/rando/unit/` on valid and broken themes, and `classic`
   loads. Verified: `make test`; goldens still byte-identical
   (`make sdl-pc`).
4. **Page and width from the theme.** The SDL main computes the page size and
   wrap width from the loaded theme and passes them to the core; `ui_cli`
   keeps its grid-derived values. Verified: `make test`; with `classic` the
   values equal today's (`ui_grid_page`: 11 body lines; width 38); goldens
   still byte-identical (`make sdl-pc`).
5. **SDL renderer from `UiScreen`.** `app/sdl/` draws the zones from the
   theme: background image, tabs, title, body rows in their variants (setting,
   selected with `< >` markers, disabled, selected disabled, action, text),
   scroll markers, message line, footer hints with their sprites. Fonts
   through SDL_ttf, images through SDL_image, wired into the build in this
   step (`sdl-pc`'s `setup:` check and link flags, `sdl-arm`'s link line, the
   CI `sdl` job's `apt-get install`). `classic` theme committed (DejaVu Sans
   Mono, today's palette, its background as a PNG). Verified: `make sdl-arm`
   (docker) links against Onion's SDL_image;
   `make sdl-pc` with every golden of step 1 byte-identical under `classic`,
   and the SDL tests cover a missing theme folder, a missing image, a missing
   font and a broken `theme.ini` (each: one stderr line, non-zero exit).
6. **Old renderer removed, packaging follows.** The grid-based
   `sdl_view_draw` and `sdl_view_palette` are deleted; the SDL app requires
   `--theme <dir>` or finds `themes/default/` next to itself; `make package`
   ships `classic` as default for now, with `libSDL_image-1.2.so.0` in the
   NEEDED check; `targets/mac/` follows.
   Verified: `make check`, `make sdl`, `make package` (docker), goldens still
   identical.
7. skill: check-net — the whole net green after phase 1.

### Phase 2 — The new theme from the mockups

8. **The new theme's geometry.** `themes/alttp/` (name the plan's to settle)
   with a `theme.ini` whose zones, fonts and sizes are taken from the 12 px
   mockups, and Press Start 2P with its licence committed under `data/`; no
   frame or sprites yet (plain colours stand in). This fixes the page size,
   the wrap width and the help line's length, which steps 9 and 11 check
   against. Verified: the theme loads (`make test`) and `make sdl-pc` draws
   every state with it.
9. **Mockups made true and comparable.** Every mockup in `design/mockups/`
   loads Press Start 2P from a local `@font-face` instead of Google Fonts; the
   content edited to the app's real settings, values and strings (see Rules),
   each edit its own commit; each mockup paired with the key sequence (and
   seed, date, SD fixture) that brings the app to its state. The text is
   played through `ui_cli`, given the grid size that yields the new theme's
   page and width (`--lines`, `--cols`), checked first to report the same
   page and width as the SDL app with that theme. Verified: Chromium renders
   every mockup offline with the local font; every key sequence, played with
   the new theme's page and width, gives a screen whose
   text matches the mockup's text, help line excepted (checked in step 11).
10. **`make theme-compare`.** Outside the net, needs Playwright's Chromium and
   `make sdl-pc`'s prerequisites (else a `setup:` line). Writes, under
   `tests/rando/build/theme-compare/`, one side-by-side image per mockup
   (mockup, app capture, diff) and prints one score per mockup. Verified: it
   runs on `classic` and writes every image; the score of a mockup against
   itself is 0.
11. **Help text per value.** The about 60 strings in `ui_strings_en.c`, the
    core's help for the selected row's value, the message replacing it; the
    text renderer shows it in the message line, so the snapshots that show a
    setting row without a message change, in this commit, stated as the
    deliberate change. A unit test bounds every help string to the new
    theme's help line (about 57 characters at 10 px in 572 px; the exact
    bound from step 8's theme). Verified: `make test`; the help line of
    `00_settings_help.svg` matches the app's text.
12. **The new theme's look.** In `themes/alttp/`: `frame.png` and sprites
    (triforce cursor, ◀ ▶ ▲ ▼ arrows, d-pad, A, B, X, Y and START
    buttons) drawn from the mockups' own vector shapes, `theme.ini` tuned
    with `make theme-compare` until each capture matches its mockup's layout
    (same zones, same text positions to a few pixels, same colours). Verified:
    `make sdl-pc` loads and draws every state with it; for each mockup, the
    step notes record which of the three criteria hold on its side-by-side
    image and why any does not, with the `make theme-compare` score of the
    first and last tuning pass (informational, no threshold); the step is
    done when every criterion holds or has its reason; after the last pass
    the page, width and help line length still equal step 8's, or step 9's
    text check is re-run and step 11's bound updated in this step, stated;
    the side-by-side images
    are left for the owner (`tests/rando/build/theme-compare/`) and listed in
    `notes/owner-todo.md`.
13. **Default theme switched.** The zip ships the new theme as default
    (`classic` stays in the repository for the SDL tests); `README.md` says
    where the theme lives and how to compare it. Verified: `make package`
    (docker) and `make check`; and the plan's goal shown once: one colour,
    one font size and one row height edited in the new theme's `theme.ini`,
    the existing binary relaunched with no rebuild, each change seen in a
    `--shot` capture, the edits reverted, `git diff` touching nothing under
    the tests, all recorded in the step notes.
14. skill: check-net — the whole net green at the end.

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

### Step 1.1

- Done in 5 chunks (`fb5bb15`..`e200728`): fixed-run options shared in
  `app/sd/fixed_run.c`; `alttpr` plays keys with `--seed`, `--date`,
  `--source-md5`, `--base-md5`, `--shot-text`; `tests/rando/sdl/goldens.sh`
  (write/compare, self-proof, fingerprint) over the 13 states of
  `golden_states.txt` on synthetic SD cards from `sd_fixture` (`good`,
  `bad-source`), no ROM; the 13 goldens in `tests/rando/sdl/goldens/`;
  `make sdl-pc` prints `goldens: 13 states byte-identical`; `make
  sdl-goldens` writes them, outside the net.
- Environment, pinned: this session's cloud container, Ubuntu 24.04.5,
  sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11, FreeType 2.13.2
  (`freetype2` 26.1.20), SDL_image 1.2.12 installed with `apt-get install
  libsdl1.2-dev libsdl-ttf2.0-dev libsdl-image1.2-dev`; docker started with
  `dockerd` by hand. Playwright 1.63.0's Chromium (`/opt/pw-browsers`) took a
  headless screenshot of a mockup (probe). The second environment (CI `sdl`
  job) was not dispatched: an agent cannot. The comparison is pinned to this
  container by `goldens/environment.txt`; elsewhere it prints `setup:
  goldens pinned to …`. So the CI `sdl` job, if dispatched, now stops on
  that line: an owner item, not a bug.
- Deviation, decided by guideline: the library has no unsupported setting
  pair, so greyed rows, the greyed selected action and the refused START
  were unreachable. A test-only override `--unsupported
  <setting>=<value>,<setting>=<value>` (one pair) was added to
  `fixed_run.c` and `rando_app_shell_init` (NULL keeps the library list);
  states 4-6 use `state=retro,goal=pedestal`. The state table gained a sixth
  column (per-state flags); expected strings carry line tags (`X|`, `v|`).
- `alttpr foo` now fails as "unknown key: foo" (exit 2 as before).
- `git commit --amend` is denied in this session: a PRD removal rides in
  the next step-note commit instead of being folded into the chunk commit.

### Step 1.2

- Done in 3 chunks (`cebecfa`, `2569dd7`, `3824907`), no snapshot and no
  SDL golden changed: `UiScreen.footer` became `hints[]` (`UiHint`: a
  `UiHintButton`, key text and label from the table, `primary`; START
  Generate is primary); `UiScreen.title_tone` (`UI_TITLE_NORMAL`/`ALERT`:
  alert on Generation Failed and Delete), `UiScreen.body_kind`
  (`UI_BODY_LIST`/`NOTICE`: notice on Generating, Seed Ready, Generation
  Failed, empty History, Delete), `UiRow.emphasis` (the file-name row of
  Seed Ready and Delete). Notice rows stay `UI_ROW_TEXT`.
- Correction found by step 3's PRD: a hint label is not at "sprite x + 32"
  (fails for START in 01, pill x322 66 px wide, label x398); it sits 10 px
  after the key's right edge, which matches all three labels of 01.
- Push of these commits refused by GitHub (`remote rejected … fatal error
  in commit_refs`) on five tries; kept local and retried later.

#### Mockup inventory

Sources: **F** `frame.png` (static, never driven by the code); **Z** text in a
zone; **S** a sprite; **V** a style variant chosen by a `UiScreen` field
(existing, or added in this step); **T** a theme constant (position, size,
colour) the format must express. Coordinates are the mockups' (640×480).
The 12 px mockups (01 to 08) are the reference; 00 is used for its help line
and for the selected setting row, which no 12 px mockup shows.

##### Common to every mockup

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
| Title | Z, V (`title`, `UiScreen.title_tone`: `UiTitleTone` UI_TITLE_NORMAL/ALERT) | 14 px, x30, baseline 82; normal `#78d8f8`, alert `#ff6868` |
| Title rule | T | dashed `#3858d8`, 4 px, dash 8 gap 4, y77, from the title text's end + 12 to x610; drawn from the title's rendered width |
| Message line | Z, V (`message_kind`) | 12 px, x30, baseline 412; info `#78d8f8`, error `#ff6868` |
| Footer band | T, V (hint count > 0) | `#08084c` x14 y422 612×44, top bar `#3858d8` 4 px; absent on 02 and 06 (no hints): a zone background drawn only when the zone has content |
| Hint sprite | S, V (`UiHint.button`: `UiHintButton`) | at the hint's x, y433; d-pad 22×22 white cross with `#08084c` notches; A/B/X/Y 22×22 `#f8d038` notched square, letter `#08084c` 12 px inside the sprite; START 66×22 `#e83838` pill, "START" 10 px `#f8f8f8` inside the sprite |
| Hint label | Z, V (`UiHint.label`, `UiHint.primary`) | 12 px, baseline 448, at sprite x + 32; `#f8f8f8`, primary `#f8d038` |
| Hint flow | T | first hint x30; next hint at previous label's end + 36 (01: 30, 170, 322; 05: 30, 182; 07: 30, 146) |

##### List body (01, 05, 07) — `UiScreen.body_kind` = `UI_BODY_LIST`

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

##### Notice body (02, 03, 04, 06, 08) — `UiScreen.body_kind` = `UI_BODY_NOTICE`

| Element | Source | Detail |
|---|---|---|
| Rows | Z | 14 px, 32 px rows, baseline 120 then +32, x30, `#f8f8f8`; at most 3 rows |
| Emphasised row | Z, V (`UiRow.emphasis`) | `#f8d038`, x50 (03 and 08 file names) |

##### Per mockup

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

##### Fonts and sizes the format must express

Press Start 2P at 16 px (tabs), 14 px (title, notice rows), 12 px (list rows,
message, hint labels), 10 px (help line, phase 2); the START and A/B/X/Y
letters are inside their sprites. Row heights: 24 px (list), 32 px
(notice). Everything a per-zone font, size, colour, row height and
position.

### Step 1.3

- Done in 3 chunks (`50294b5`, `7d9bc46`, and the inventory fixture): pure
  C99 `app/theme/` (`theme_parse`, `theme_load` reading `<dir>/theme.ini`,
  16 KiB cap, static buffer, one `<file>:<line>: <section>.<key>: <problem>`
  message per error); `themes/classic/theme.ini` (list top 64, bottom 416,
  rows 32, label x 16, right 624; DejaVu `fit` in a 16×32 cell); test_theme
  (classic, broken variants, theme_load cases) in `make test`;
  `test_sdl_classic` in `make sdl-pc` checks classic against
  `sdl_view_palette`, the 40×15 grid and the fitted cell; the inventory
  fixture `tests/rando/unit/themes/inventory/theme.ini` expresses every T
  and S row of step 1.2's inventory with no format change (mapping in the
  step 3 PRD's Further Notes, removed with it: the fixture is the record).
- Left open by the PRD and decided by the implementers: end-of-file check
  order (missing keys, missing sections, font count, tab numbering, font
  references, fit without cell, bands, list order); cross-check errors
  point at the key's own line; new messages `[tab.<n>]: missing` and
  `cannot open: path too long`.
- Risk for `make unit-arm` (outside the net, not run): test_theme's open
  error expects glibc's "No such file or directory" text.
- Pushes of `50294b5` and `7d9bc46` went out on `make test`, `make lint` and
  `make sdl-pc` alone, before the step's `make check` (green since).
