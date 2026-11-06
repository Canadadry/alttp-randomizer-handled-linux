---
title: "make theme-compare"
description: "The owner judges the new look on side-by-side images of each mockup and the app's capture of the same state, and no tool makes those images or measures how far the app is from a mockup."
status: needs-triage
---

## Problem Statement

Plan 2's look is judged by the owner on images, not on tests: for each of
the nine mockups in `design/mockups/`, the mockup as the owner drew it next
to the app showing the same screen. Step 12 tunes `themes/alttp` until each
capture matches its mockup's layout, and needs, after every tuning pass, to
see where the two still differ and a number that says whether the pass got
closer.

After step 9 the pieces exist separately: the mockups render offline in
Chromium with the shipped Press Start 2P (`mockup_render.js`), every mockup
is paired with the keys that bring the app to its screen
(`mockup_states.txt`), and `goldens.sh --draw --states` captures the app in
those states with any theme. Nothing puts a mockup and its capture side by
side, nothing shows the pixels where they differ, and nothing gives a
score. Without that, step 12 has no loop to tune against and the owner has
nothing to look at.

## Solution

`make theme-compare`, outside the net, with an optional `THEME=<dir>`
(default `themes/alttp`):

1. Prints a `setup:` line and exits 1 when Playwright's Chromium or `make
   sdl-pc`'s SDL packages are missing (the same checks as `make mockups`).
2. Renders the nine mockups to 640×480 PNGs with step 9's render check
   (offline, local font, no ligature, its self-proof first).
3. Captures the app in each mockup's paired state with the chosen theme,
   through `goldens.sh --draw --states mockup_states.txt`.
4. For each mockup writes `tests/rando/build/theme-compare/<mockup>.png`:
   three 640×480 panels left to right, **mockup | app | diff**, separated
   by 8 px grey bars; the diff panel shows the mockup dimmed to grey where
   the two pixels are equal and magenta where they differ, brighter the
   larger the difference.
5. Prints one line per mockup, `theme-compare: <mockup>: score <s>`, where
   the score is the mean absolute difference of the two images over every
   pixel and channel, as a percentage of full scale (0.000 is identical,
   100.000 is black against white everywhere), then a summary line. The
   same score lines are written to `scores.txt` in the output folder.

Nothing fails on a score: the target exits 0 whatever the scores are, and 1
only when it could not produce the images (a setup problem, a render or
capture failure, a pairing missing, an image of the wrong size, a self-test
failing). Every run first proves its image code on planted images and
proves, on each real mockup, that the mockup scored against itself is
exactly 0.

## User Stories

1. As the owner, I want one image per mockup with the mockup, the app's capture and their difference side by side, so that I can judge the new look at a glance.
2. As the owner, I want the panels always in the same order (mockup, app, diff), so that I never wonder which side is the app.
3. As the owner, I want the diff panel to keep a dim picture of the mockup where nothing differs, so that I can see where on the screen each difference sits.
4. As the owner, I want differences drawn in a colour the mockups never use, so that a difference is never mistaken for part of the design.
5. As the owner, I want the size of a difference visible in the diff panel, so that a one-step antialiasing change and a missing element do not look the same.
6. As the owner, I want the images left in a fixed folder (`tests/rando/build/theme-compare/`), so that I know where to look after any run.
7. As the owner, I want the mockup panel to be the mockup rendered offline with the shipped font, so that the reference I see is the one I drew.
8. As the owner, I want the app panel to be the real app in the real state, played from keys on a synthetic SD card, so that I judge what the device will show.
9. As the owner, I want one score per mockup, so that I can tell whether a tuning pass brought the app closer.
10. As the owner, I want no threshold on the score, so that text that never matches to the pixel (SDL_ttf against a browser) never makes the tool fail.
11. As the owner, I want the scores kept in a file beside the images, so that I can read them after the terminal is gone.
12. As the agent of step 12, I want the comparison re-runnable after every theme edit with no rebuild of the mockups by hand, so that tuning is a short loop.
13. As the agent of step 12, I want to choose the theme compared (`THEME=`), so that I can compare `alttp` and, for reference, `classic`.
14. As the agent of step 12, I want the score to be a plain, documented formula, so that I can record the first and last pass's scores in the step notes and they mean something.
15. As the agent of step 12, I want identical inputs to give identical image bytes and scores, so that a change in a score is a change in the theme.
16. As the plan pilot, I want the score of a mockup against itself checked to be 0 on every run, so that the scoring is known to measure difference and nothing else.
17. As the plan pilot, I want the image decoder proven against Chromium's own output (each mockup's background pixel is the colour the SVG says), so that a colour-managed or misread PNG is caught, not scored.
18. As the plan pilot, I want the PNG and BMP code, the score, the diff and the composition proven on planted images before any real run, so that a wrong pixel cannot hide behind plausible scores.
19. As the plan pilot, I want a mockup with no pairing, or a pairing with no mockup, reported by name, so that the nine images are always the nine mockups.
20. As the plan pilot, I want a capture or a render of the wrong size refused with a line naming it, so that panels never misalign silently.
21. As the plan pilot, I want a missing Chromium, Playwright or SDL package to print a `setup:` line, knowing that the plan says such a line verifies nothing.
22. As the plan pilot, I want the target outside `make check` and out of CI, so that the net stays free of Chromium and SDL.
23. As a maintainer, I want the tool to reuse step 9's render check, pairing table, setup checks and `goldens.sh --draw --states`, so that a mockup is rendered and a state captured in exactly one way.
24. As a maintainer, I want the image work in the Python standard library only, so that the checks still need only `gcc`, `make` and `python3` (plus Chromium for this target).
25. As a maintainer, I want the goldens, the golden states, the snapshots, the mockups and the themes untouched, so that this step adds a tool and changes nothing it measures.
26. As a maintainer, I want `README.md` and `CLAUDE.md` to name `make theme-compare`, its output and its no-threshold rule, so that the next session finds it.
27. As a maintainer, I want the 16 px mockup (00) compared like the others with its known one-row difference stated, so that nobody tunes the theme to it.

## Implementation Decisions

### Shape

- Three pieces, as in step 9: an image tool in Python (all pixel work), a
  POSIX sh driver (the loop, in `goldens.sh`'s style), and make targets.
- The driver calls step 9's pieces, never a copy of them: the render check
  for the mockup PNGs, `goldens.sh --draw DIR --states mockup_states.txt`
  for the captures, and the setup and pairing checks of `make mockups`
  (see "Shared with `make mockups`").
- It does not run step 9's text check (`ui_cli` agreement, `MISMATCH`
  lines): `make mockups` is that check; `theme-compare` shows pixels.

### The image tool (`tests/rando/sdl/theme_compare.py`)

Python 3 standard library only (`zlib`, `struct`), run with `python3 -I`.
Commands:

- `compare MOCKUP_PNG APP_BMP OUT_PNG`: decodes both, writes the
  side-by-side PNG to `OUT_PNG`, prints the score (`%.3f`) on stdout, exit
  0. On any refusal: one line on stderr naming the file and the problem,
  exit 1, nothing written.
- `--self-test`: the planted cases below; one line per failing case, exit 1
  if any; prints nothing else when all pass.

**Decoding.**

- PNG: 8-bit, non-interlaced, colour type 2 (RGB) or 6 (RGBA), every
  filter type 0–4 on any row (Chromium picks filters per row), IDAT chunks
  concatenated, CRCs not checked (the file is Chromium's, just written).
  Colour type 6 must have alpha 255 everywhere ("not opaque"), and alpha
  is then dropped. Any other header (bit depth, palette, grey, interlace)
  is refused with its value.
- BMP: `BM`, `BITMAPINFOHEADER` or larger, 24 bpp, no compression, rows
  padded to 4 bytes, bottom-up (positive height) or top-down (negative);
  pixels BGR. That is what SDL 1.2's `SDL_SaveBMP` writes here (the
  goldens: 24 bpp, 54-byte header, 640×480). Anything else is refused.
- Both images must be 640×480 ("WxH, not 640x480").
- **Colour check**: the mockup's decoded pixel (2, 2) must be `#0c0c34`,
  the full-screen background rect of every mockup (outside the white
  frame, which starts at 6). A different value means Chromium managed the
  colour, the PNG was misread, or the file is not a mockup: refused with
  the value read.
- **Self-score**: the score of the mockup against itself is computed
  through the same function as the real score and must be exactly 0
  ("self-score <s>, not 0"), on every mockup of every run. This is the
  plan's "the score of a mockup against itself is 0", checked each time.

**Score.** For images A (mockup) and B (app) of W×H pixels:
`100 × Σ |A[c] − B[c]| / (W × H × 3 × 255)` over every pixel and the three
channels R, G, B. Printed with three decimals. 0.000 only when identical;
not normalised further, not weighted by zone, no threshold anywhere.

**Diff panel**, per pixel:

- equal in all three channels: grey `(L, L, L)` with
  `L = (299 R + 587 G + 114 B) // 1000 // 4`, from the mockup's pixel;
- otherwise magenta `(m, 0, m)` with `m = 128 + 127 × d // 255`, where `d`
  is the largest of the three channel differences (so 1 → 128, 255 → 255).

Magenta is a colour no mockup uses (the inventory's palette is blues,
yellows, red `#ff6868`/`#e83838`, white and greys).

**Side-by-side PNG**: 1936×480 (three 640 px panels and two 8 px bars),
bars `#808080`, panels mockup, app, diff from left to right. Written as
8-bit RGB, non-interlaced, filter 0 on every row, one IDAT compressed with
`zlib` level 9, no ancillary chunk: the same inputs give the same bytes.
No text is drawn in the image (no font in the standard library); the order
is stated in the summary line, `README.md` and `CLAUDE.md`.

**Speed**: pure Python over 307 200 pixels; work on `bytes`/`bytearray`
rows rather than per-pixel objects. No budget is set (outside the net);
the step notes record the whole target's time once.

### The driver (`tests/rando/sdl/theme_compare.sh`)

`theme_compare.sh ALTTPR SD_FIXTURE THEME OUT` (POSIX sh, `set -eu`,
`LC_ALL=C`, `SDL_VIDEODRIVER=dummy` through `goldens.sh`). In order:

1. The shared browser setup check (below): `setup:` line, exit 1.
2. `theme_compare.py --self-test`.
3. The shared pairing check over `design/mockups/*.svg` and
   `mockup_states.txt`: `theme-compare: <name>: no pairing` / `no mockup`,
   exit 1.
4. `OUT` emptied; the render check over the nine mockups into
   `OUT/render/` (its self-proof runs first, as in `make mockups`).
5. `goldens.sh ALTTPR THEME SD_FIXTURE --draw OUT/app --states
   mockup_states.txt`: `OUT/app/<name>.bmp` and `.txt` per pairing.
6. For each mockup, in name order: `theme_compare.py compare
   OUT/render/<name>.png OUT/app/<name>.bmp OUT/<name>.png`; prints
   `theme-compare: <name>: score <s>` and appends the same line to
   `OUT/scores.txt`. A refusal stops the run with its line, exit 1.
7. Summary: `theme-compare: 9 mockups compared with <theme name> in
   <OUT> (mockup | app | diff; scores informational, no threshold)`,
   exit 0.

`OUT` is `tests/rando/build/theme-compare/`. The side-by-side PNGs and
`scores.txt` are at its top level (what the owner opens); `render/` and
`app/` keep the inputs.

### Shared with `make mockups`

- The browser setup check (`python3`, `node`, `npm`, the `playwright`
  module, a launchable Chromium, `PLAYWRIGHT_BROWSERS_PATH` defaulting to
  `/opt/pw-browsers`) and the pairing check (every `design/mockups/*.svg`
  has a line in `mockup_states.txt` and every line a mockup) are taken out
  of `mockups.sh` into one sourced POSIX sh file beside it
  (`mockup_common.sh`), with the message prefix as a parameter so each
  driver prints its own name (`mockups:` / `theme-compare:`; the `setup:`
  lines are prefix-free as today). `make mockups`' output is unchanged by
  the move (checked once by running it before and after, recorded in the
  commit message).
- If step 9 already factored these checks (H-9-4 false in that sense), the
  existing helper is used as it is.
- The make-side SDL setup check (`pkg-config --exists sdl SDL_ttf
  SDL_image` with its `setup:` text) is written once more in the new
  target, as `sdl-pc`, `sdl-goldens` and `mockups` each hold it today;
  folding the four into one make variable is a refactor this step does
  not need and does not do.

### Make targets

- `tests/rando/Makefile`: `theme-compare`, after the SDL setup check,
  builds `$(SDL_APP)` and `$(SD_FIXTURE)` (not `ui_cli`: no text check),
  then runs `./sdl/theme_compare.sh ./$(SDL_APP) ./$(SD_FIXTURE)
  $(COMPARE_THEME) $(BUILD)/theme-compare`, with `COMPARE_THEME ?=
  ../../themes/alttp`. Added to `.PHONY`.
- Root `Makefile`: `theme-compare` passes `THEME` (a path from the
  repository root, default `themes/alttp`) as
  `COMPARE_THEME=$(abspath $(THEME))`; one help line: "the mockups beside
  the app's captures with a diff and a score per mockup, into
  tests/rando/build/theme-compare/ (needs Playwright's Chromium and sdl-pc's
  packages; no threshold)". Added to `.PHONY`.
- Not called from `check`, `sdl`, `sdl-pc` or CI.

### Known differences, not defects

- **00** is the 16 px mockup; its pairing plays at the theme's page (12
  rows with `alttp`), so its capture holds one row more and smaller text
  than the mockup: its score is high by construction. Stated in
  `README.md`'s paragraph; step 12 records it for 00 rather than tuning to
  it.
- **The help line** (00) is absent from the app until step 11.
- **Frame and sprites** are absent from `alttp` until step 12, and
  `classic` is another look altogether: both give large scores now; the
  step's verification is that the images exist and the self-score is 0,
  not any score's value.

### Docs

- `CLAUDE.md`, Checks: a `make theme-compare` bullet (what it writes, the
  panel order, the score formula in one line, no threshold, outside the
  net, its `setup:` line verifies nothing, `THEME=`), and it joins the
  "Not in the net" list.
- `README.md`, Development, after step 9's `make mockups` paragraph: how
  to run it, where the images and `scores.txt` land, the panel order, what
  the score is and why there is no threshold, 00's known difference.

## Implementation Chunks

1. **The image tool.** `theme_compare.py` with `compare` and
   `--self-test`. Done: `python3 -I tests/rando/sdl/theme_compare.py
   --self-test` passes; `compare` on a mockup PNG rendered once by hand
   with the render check and an existing `theme-states/alttp/` capture
   from `make sdl-pc` writes a 1936×480 PNG and prints a score; `make
   test` unchanged.
2. **The driver, the shared checks and the targets.** `mockup_common.sh`
   (moved out of `mockups.sh`), `theme_compare.sh`, both make targets, the
   docs. Done: the step verification below.

Step verification (the plan's): in the pinned environment (plan rule: a
`setup:` line does not verify), `make theme-compare THEME=themes/classic`
and `make theme-compare` each print nine score lines and the summary and
leave nine 1936×480 PNGs and `scores.txt` in
`tests/rando/build/theme-compare/`; every run passed the per-mockup
self-score 0; two runs in a row give byte-identical PNGs and scores;
`make mockups` still ends with its summary line; `make sdl-pc` still
prints `goldens: 13 states byte-identical`; `make check` green. The step
notes record the nine scores of each theme (informational), the target's
time, and that the images were looked at once (panels aligned, the diff
lighting where the frame and sprites are missing).

## Testing Decisions

- A good test drives the tool from outside: planted images in, pixels and
  scores out. No test reads the scripts' internals.
- **Planted cases in `--self-test`**, each built in memory by the test
  (no file in the repository), each required to give exactly its
  expectation:
  - PNG: a 5×4 RGB image encoded with each filter type 0–4 on every row,
    and with a different filter per row, decodes to the planted pixels;
    the same as colour type 6 with alpha 255 decodes the same; alpha 254
    on one pixel is refused; bit depth 16, a palette image and an
    interlaced header are each refused;
  - BMP: a 3×2 24 bpp image (row padding exercised) bottom-up and
    top-down decodes to the planted pixels; 32 bpp and compressed headers
    are refused;
  - score: identical images → `0.000`; a 2×1 pair with one channel off by
    51 → `3.333`; black against white → `100.000`; different sizes
    refused;
  - diff: an equal pixel `(200, 100, 50)` → grey `(31, 31, 31)`; channel
    differences of 1 and 255 → `(128, 0, 128)` and `(255, 0, 255)`;
  - composition: three 2×1 panels → a 22×1 image, panel pixels in place,
    the bars `#808080`; the encoded PNG decodes back through the tool's
    own decoder to the same pixels, and two encodings are byte-identical;
  - refusals of `compare`: a mockup whose pixel (2, 2) is not `#0c0c34`,
    and a 639×480 capture, each one line.
- **Proof against the real producers** on every run, not only planted
  ones: each real mockup's background pixel read as `#0c0c34` (Chromium's
  PNG decoded right, no colour management) and each real mockup's
  self-score exactly 0.
- The self-test is not added to `make test`: the tool is used only outside
  the net, and every run starts with it (as step 9 decided for its
  comparator).
- `make mockups` is re-run once after moving its checks into the shared
  file, its output compared before and after.
- Nothing in the net changes and nothing is weakened: no golden, golden
  state, snapshot, mockup, theme or expected value.
- Prior art: `tests/rando/sdl/goldens.sh` (state loop, BMP reading in its
  `picture` command, self-proof, failure lines); step 9's
  `mockup_text.py --self-test` and `mockup_render.js` self-proof;
  `tests/rando/no_literals.py` (planted sources).

## Out of Scope

- Any threshold, pass/fail on a score, or score in CI.
- Tuning `themes/alttp`, its frame and sprites (step 12); any theme edit.
- Any mockup edit; the text check (`make mockups`, step 9).
- The help line (step 11).
- Labels or text inside the side-by-side images; per-zone scores;
  alignment search or shifting one image to fit the other; perceptual
  metrics.
- `notes/owner-todo.md` and listing the images for the owner (step 12).
- New golden states, goldens, fixtures; changes to `ui_cli`, `alttpr`, the
  core, the strings, `rando/`.
- A CI job (CI has no Playwright and is manual).

## Hypotheses about step 9

Step 9 (PRD `docs/prd/triage/phase2-step9-mockups-made-true.md`) is not
done when this PRD is written. The implementer checks each hypothesis
against step 9's commits first; the line after each says what to do if it
does not hold.

- **H-9-1**: `tests/rando/sdl/mockup_render.js` exists, run with `node`,
  `NODE_PATH` = `npm root -g` and `PLAYWRIGHT_BROWSERS_PATH` defaulting to
  `/opt/pw-browsers`; arguments an output folder then SVG files; proves
  itself on planted SVGs first; for each SVG writes `<out>/<name>.png`, a
  640×480 screenshot with the SVG at the origin, and exits 1 with a
  `mockups: <name>: <problem>` line on an external request, an unloaded
  font or a text of the wrong width. *If its interface differs*, the
  driver calls it as it is; *if it writes another size or offset*, the
  colour check or the size check refuses the PNG, and the fix belongs in
  the render check (one place), not in a crop here.
- **H-9-2**: `tests/rando/sdl/mockup_states.txt` holds one tab-separated
  line per mockup, named by the mockup's file name without `.svg`, in
  `golden_states.txt`'s first five columns plus a page column that
  `goldens.sh --draw` ignores. *If names differ from the files*, the
  pairing check reports them and the table is not edited here.
- **H-9-3**: `goldens.sh ALTTPR THEME SD_FIXTURE --draw DIR --states FILE`
  plays FILE with `--theme THEME`, seed 1 and date
  `2026-11-01T14:03:27`, and writes `DIR/<name>.bmp` (24 bpp, 640×480,
  checked to be more than one colour) and `.txt`. *If `--states` is
  missing*, this step stops and reports: adding it is step 9's.
- **H-9-4**: `mockups.sh` holds the browser setup check and the pairing
  check inline (its steps 1 and 3). *If step 9 put them in a shared file
  already*, that file is used and `mockup_common.sh` is not created.
- **H-9-5**: the font edit kept every mockup's full-screen
  `<rect width="640" height="480" fill="#0c0c34"/>` first, and the white
  frame starts at x/y 6 (step 1.2's inventory), so pixel (2, 2) of each
  render is `#0c0c34`. *If a mockup's background changed*, the colour
  check names it; the mockup is not edited here.
- **H-9-6**: `tests/rando/Makefile` has a `mockups` target building
  `$(SDL_APP)`, `$(SD_FIXTURE)` and `$(UI_CLI)` after the SDL setup check,
  and a root `make mockups`; `make mockups` ends with its summary line in
  the pinned environment. *If `make mockups` is red*, this step stops:
  the comparison would set wrong references side by side.
- **H-9-7**: 02's pairing is capped (frame cap 3) so its capture is the
  Generating screen, and 00 plays at the theme's page. Nothing here
  depends on it beyond the "Known differences" wording, which follows the
  real table.
- **H-9-8**: `CLAUDE.md` and `README.md` hold step 9's `make mockups`
  lines; this step adds its own beside them and edits none of them.

## Further Notes

- **Why a mean absolute difference.** Text never matches to the pixel
  (SDL_ttf's and Skia's rasterisers differ), so a count of differing
  pixels saturates on antialiasing and says little about a pass. The mean
  absolute difference moves with what step 12 changes (a band's colour, a
  zone's position, a missing sprite) and is 0 only on identity, which the
  self-score proves every run. It is informational by the plan's rule.
- **Why Python decodes the PNG.** Compositing in a Chromium canvas would
  avoid a decoder but taints the canvas on `file://` images and applies
  colour management to decoded images; the Python decoder, proven on
  planted filters and on each mockup's known background colour, keeps the
  pixels exactly as written and the checks on the repository's only
  required tools.
- **Why no text in the image.** The standard library has no font; panel
  order is fixed and stated instead. A label strip can be added later
  without changing the score.
- **Why the default theme is `alttp`.** It is the theme step 12 tunes;
  `classic` is the plan's verification run (`THEME=themes/classic`),
  proving the tool works on a theme with a background image.
- Needs the environment pinned in step 1.1 (SDL_image and Playwright
  1.63's Chromium under `/opt/pw-browsers`, probed there).
