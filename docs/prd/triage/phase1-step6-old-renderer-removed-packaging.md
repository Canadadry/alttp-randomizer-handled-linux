---
title: "Old renderer removed, packaging follows"
description: "Once the theme renderer draws classic pixel for pixel, the grid renderer, --font and the zip's loose font are dead weight, and the release zip, the Miyoo launch and the mac build still assume them."
status: needs-triage
---

## Problem Statement

After step 5 the SDL app has two renderers: the theme renderer (`--theme
<dir>`), proven byte-identical to the 13 goldens under `classic`, and the old
grid renderer (`sdl_view_draw`, `sdl_view_palette`, `--font`), still used
when `--theme` is absent. The old path is the one the device actually runs:
`launch.sh` starts `./alttpr` with no option, and the zip ships
`DejaVuSansMono.ttf` loose in `App/ALttPR/` for its default `--font`. So:

- the look the owner will iterate on in phase 2 (a theme folder) never
  reaches the Miyoo: the zip has no theme, and the binary does not look for
  one;
- `make package`'s NEEDED check does not require `libSDL_image-1.2.so.0`, so
  a binary that does not load PNGs would pass it;
- `targets/mac/` builds without SDL_image and runs with `--font`, so the
  owner's PC check on the Mac does not link since step 5;
- two renderers mean two code paths for every future change, and the old one
  carries a hard-wired palette the plan wants gone.

## Solution

1. **One renderer.** `sdl_view.c`/`sdl_view.h` (`sdl_view_open`,
   `sdl_view_draw`, `sdl_view_close`, `sdl_view_palette`) and their test are
   deleted. `--font` is no longer an option (`alttpr --font x` is a usage
   error, exit 2). Every frame is drawn by the theme renderer.
2. **The theme is required, with a default next to the binary.** `alttpr
   --theme <dir>` as today; without `--theme`, the app reads
   `themes/default/` in the directory that holds the `alttpr` binary. A
   missing or broken default fails exactly like a bad `--theme`: one
   `alttpr: ` line naming `<dir>/themes/default/theme.ini`, exit 1, the SD
   card untouched.
3. **The zip ships the theme.** `App/ALttPR/themes/default/` holds a copy of
   every file of `themes/classic/` (for now; step 13 switches the default to
   the new theme). The loose `DejaVuSansMono.ttf` and its licence leave
   `App/ALttPR/` (they ship inside the theme folder). `launch.sh` is
   unchanged: it already runs `./alttpr` from `App/ALttPR/`, where
   `themes/default/` now is.
4. **`make package`'s NEEDED check** requires `libSDL_image-1.2.so.0` next to
   `libSDL-1.2.so.0` and `libSDL_ttf-2.0.so.0`.
5. **`targets/mac/` follows**: it builds SDL_image 1.2 (PNG only) the way it
   builds SDL_ttf, links it, and runs the app with `--theme themes/classic`;
   its README says so.
6. **Docs and checks follow**: the smoke test, the golden script, the
   package self-test, `README.md` and `CLAUDE.md` describe the one renderer,
   the default theme and the new zip layout.

## User Stories

1. As the owner, I want the zip I copy to the Miyoo to carry its theme folder, so that the look I approve in phase 2 is what the device shows.
2. As the owner, I want the app on the Miyoo to start with no option, as `launch.sh` starts it today, so that Onion's launcher keeps working unchanged.
3. As the owner, I want the app to find `themes/default/` next to its binary wherever it is started from, so that running it by its full path works as well as `./alttpr`.
4. As the owner, I want a missing `themes/default/` to stop the app with one line naming the `theme.ini` it looked for, so that a half-copied SD card is diagnosed at once in `alttpr.log`.
5. As the owner, I want a broken default theme to report the loader's own line (file, line, key, problem), so that it fails like a broken `--theme`.
6. As the owner, I want a failed start to leave the SD card untouched, so that a bad theme never writes a seed or a record.
7. As the owner, I want `--theme <dir>` to still override the default, so that I can try another theme on the PC without copying it next to the binary.
8. As the owner, I want `--font` gone, so that there is only one place the app's font comes from: the theme.
9. As the owner, I want `alttpr --font x` refused as an unknown option with the usage, so that an old command line fails loudly instead of being half-honoured.
10. As the owner, I want `make package` to refuse a binary that does not need `libSDL_image-1.2.so.0`, so that a device build unable to load PNGs never ships.
11. As the owner, I want the zip's entry list fixed and checked, theme files included, so that nothing missing or extra ships.
12. As the owner, I want the same commit to give the same zip bytes, theme included, so that two builds can be compared.
13. As the owner, I want the zip's theme files to be byte copies of `themes/classic/`, so that the device draws exactly what the goldens prove.
14. As the owner, I want the zip to still contain only `App/ALttPR/` at its top level, so that unzipping at the SD card's root stays the install.
15. As the owner on a Mac, I want `make -C targets/mac` to build the app again, with SDL_image, so that I can check the rendering on my machine.
16. As the owner on a Mac, I want `make -C targets/mac run` to start the app on `themes/classic`, so that the run target works without `--font`.
17. As the owner on a Mac, I want `targets/mac/README.md` to name every Homebrew package the build needs (libpng included) and what was not verified by the agent, so that a failure there is a known risk, not a surprise.
18. As the owner, I want `README.md` to say the app reads its look from a theme folder, where the default one lives, and how to run the PC build with `--theme themes/classic`, so that the instructions I follow work.
19. As the agent of phase 2, I want the renderer to be the only drawing path, so that every theme change is seen on every screen and no hidden fallback draws something else.
20. As the agent of phase 2, I want the package's theme folder to be one Makefile variable, so that step 13 switches the default theme with a one-line change.
21. As the agent of phase 2, I want the zip builder to take a theme folder and ship all its files, so that a theme with a frame, sprites and two fonts ships without editing the builder.
22. As the agent of phase 2, I want the zip builder to refuse a theme folder without `theme.ini`, with a subfolder, or with a hidden or non-regular file, so that a stray file or a wrong path is caught at packaging time.
23. As the plan pilot, I want the 13 goldens still byte-identical under `classic` after the deletion, so that removing the old renderer is proven to change no pixel.
24. As the plan pilot, I want `make sdl-pc` to keep running the golden comparison, now with the theme as the golden script's only renderer input, so that pixel identity stays checked by every later step.
25. As the plan pilot, I want the golden fingerprint (`environment.txt`) unchanged, so that no golden-side file is edited to get green.
26. As the plan pilot, I want `make sdl-goldens` to keep working (refusing an existing set), so that step 9's reuse of the script and its fixtures still has a writer.
27. As the plan pilot, I want the values the old palette and the old font fit gave to stay asserted in `test_sdl_classic`, frozen as constants before the oracle is deleted, so that the record "classic is today's palette and size" survives the deletion.
28. As the plan pilot, I want the step 5 equivalence harness removed together with the renderer it compared against, stated in the commit, so that the deletion of an oracle-based test is a deliberate, explained change.
29. As the plan pilot, I want the smoke test to prove the default lookup with the real binary copied into an `App/ALttPR/` with `themes/default/`, started both from that folder and by its full path from elsewhere, so that "next to itself" is tested, not assumed.
30. As the plan pilot, I want the smoke test to prove that the default theme and `--theme themes/classic` capture the same text and the same frame, so that the default lookup cannot silently load something else.
31. As the plan pilot, I want `make test`'s package self-test to cover the new layout and its refusals, so that the zip's shape is checked in seconds on the host, without docker.
32. As the plan pilot, I want `make check`, `make sdl`, and `make package` (docker) green at the end of the step, so that the plan's verification is met.
33. As a maintainer, I want no SDL include and no letter literal added where the static checks forbid them, so that `no_sdl.sh` and `no_literals.py` still hold.
34. As a maintainer, I want `CLAUDE.md` to describe `data/` as the fonts' source and the package as shipping the default theme, so that the next session reads the right layout.
35. As a maintainer, I want the SDL test list in the Makefile to lose `test_sdl_view`, so that no test of deleted code is left to fail.

## Implementation Decisions

### The SDL main

- **Options**: `--font` and `DEFAULT_FONT` are deleted; the usage lists
  `[--theme <dir>]` with no `--font`. `--font` now falls to "unknown option"
  (exit 2, the usage), like any unknown option.
- **Default theme**: without `--theme`, the theme folder is
  `<bindir>/themes/default`, where `<bindir>` is the directory part of
  `argv[0]` (everything before its last `/`), or `.` when `argv[0]` has no
  `/`. Decided over `/proc/self/exe`: it is portable to the mac target, and
  the two ways the app is started (`launch.sh`'s `./alttpr`, a full path on
  the PC) both carry a slash. A relative `argv[0]` resolves against the
  current directory, which is where it was started from. A joined path that
  does not fit the main's buffer is one `fail_setup` line, exit 1, before
  anything touches the SD card.
- **One path**: the theme is always loaded (`theme_load`), the renderer always
  opened, every frame drawn with it; the grid is always sized
  `width + 2` × `page + 4` from the theme (step 4's sizing), and still serves
  `--shot-text` only. The `--theme`-less branch (40×15 grid, `ui_grid_page`,
  `sdl_view_open`) is deleted. Failure messages are step 5's, unchanged; the
  default theme's are the same messages with the default path in them.
- **No core change**: the core's grid defaults (`UI_GRID_DEFAULT_*`) stay;
  `ui_cli` still uses them.

### The SDL layer

- `sdl_view.c` and `sdl_view.h` are deleted. Anything else in `app/sdl/` that
  still includes `sdl_view.h` (H-5-10) gets what it uses moved to its own
  header, with no behaviour change.
- The frame loop keeps step 5's present signature (`UiScreen` and grid); the
  old presenter is deleted with the branch that used it.

### Tests that lose their oracle (deliberate changes, stated)

- **`test_sdl_view`** is deleted with the module it tests, and removed from
  `SDL_TESTS`.
- **`test_sdl_classic`**: its colour checks compare `classic`'s colours with
  `sdl_view_palette`; they now compare them with the same eleven band and
  text values written as constants in the test (copied from
  `sdl_view_palette` as it stands, a frozen record, not a new expectation).
  Its fit check compares the font size `classic`'s `fit` gives with
  `sdl_view_open`'s on the same font; it now compares it with that size as a
  constant, measured in the pinned environment from `sdl_view_open` before
  the deletion, in the same chunk that freezes the palette. Both freezes land
  while the oracle still exists, so the test passes against the oracle and
  the constants at once before the oracle goes.
- **`test_sdl_render`**: the equivalence harness (grid renderer against theme
  renderer) is removed, its reason stated in the commit: its oracle is
  deleted, and step 5 ran it green; pixel identity is now carried by the 13
  goldens. Its fit check against `sdl_view_open` becomes the same frozen
  constant. The open, failure and feature tests stay unchanged.
- No snapshot, no golden and no `environment.txt` changes.

### The golden script and `make sdl-pc`

- `goldens.sh` takes `ALTTPR THEME SD_FIXTURE --write DIR | --compare DIR`:
  the theme is mandatory and second; the FONT argument and the trailing
  `--theme` form are gone; no `--font` is passed to `alttpr`.
- The fingerprint's font line(s) come from the theme folder: one
  `font <basename> <sha256>` line per `*.ttf` in THEME, sorted by name. For
  `classic` (one font, a byte copy of `data/`'s, H-5-4) this is exactly the
  line `environment.txt` already holds, so the pinned fingerprint is
  unchanged.
- `make sdl-pc` calls `goldens.sh ./alttpr ../../themes/classic ./sd_fixture
  --compare …`; `make sdl-goldens` calls it with `--write` on the same
  theme. `smoke_sdl.sh` takes `ALTTPR THEME` (the FONT argument goes).

### The package

- **`build_zip.py`**: usage `build_zip.py [--check] ALTTPR PACKAGE_DIR
  DATA_DIR THEME_DIR ZIP`. `DATA_DIR` now provides only `base.bps`. Every
  file of `THEME_DIR` is shipped as `App/ALttPR/themes/default/<name>`,
  mode 0644, deflated, epoch timestamps, with directory entries
  `App/ALttPR/themes/` and `App/ALttPR/themes/default/` (mode 0755). Refused
  with one `build_zip: ` line and no zip written: a `THEME_DIR` that does not
  exist or has no `theme.ini`; an entry of `THEME_DIR` that is a
  subdirectory, a symlink or another non-regular file; a name starting with
  `.`. Themes are flat by decision (step 3's file names are relative to the
  theme folder; no plan-2 theme needs a subfolder). `--check` reads the zip
  back against the same inputs, theme files included.
- **Zip entry list** (sorted, as today):
  `App/`, `App/ALttPR/`, `App/ALttPR/alttpr`, `App/ALttPR/base.bps`,
  `App/ALttPR/config.json`, `App/ALttPR/icon.png`, `App/ALttPR/launch.sh`,
  `App/ALttPR/themes/`, `App/ALttPR/themes/default/`, then the theme's files
  (`classic`: `DejaVuSansMono-LICENSE.txt`, `DejaVuSansMono.ttf`,
  `background.png`, `theme.ini`, H-5-4).
- **`launch.sh` and `config.json`**: unchanged (the default lookup makes
  `./alttpr` from `App/ALttPR/` find `themes/default/`).
- **Makefile**: a `PKG_THEME` variable, `../../themes/classic` for now, passed
  to `build_zip.py`; the NEEDED loop lists `libSDL-1.2.so.0
  libSDL_ttf-2.0.so.0 libSDL_image-1.2.so.0`. The rest of `package` (strip,
  `file`, `readelf -h`, `test_patch_header`) is unchanged.
- **`data/`** keeps `DejaVuSansMono.ttf` and its licence: they are the source
  the feature theme's build copies and that `test_sdl_classic` compares
  `classic`'s copy with (H-5-5), and phase 2 puts Press Start 2P beside
  them. They are no longer shipped from `data/`.

### `targets/mac/`

- `deps` also requires Homebrew's `libpng`.
- SDL_image 1.2 is built like SDL_ttf: shallow clone of
  `libsdl-org/SDL_image` branch `SDL-1.2` into `tmp/sdl-deps`, every `IMG*.c`
  of its root compiled with `-DLOAD_PNG -DSDL_IMAGE_USE_COMMON_BACKEND`
  (the ImageIO backend is Objective-C and is not used) against
  `pkg-config --cflags sdl libpng`, archived as a static `libSDL_image.a`,
  `SDL_image.h` copied beside `SDL_ttf.h`, and a `SDL_image.pc` written
  (`Requires: sdl libpng`). If an `IMG*.c` other than the PNG one fails to
  compile as its stub, the implementer limits the list to the files that
  compile and states it.
- `build` passes `SDL_CFLAGS`/`SDL_LIBS` from `pkg-config … sdl SDL_ttf
  SDL_image`; `run` uses `--theme themes/classic` instead of `--font`.
- No Mac is available: the implementer proves the SDL_image recipe once on
  the Linux host (the same clone and compile into `tmp/`, against the host's
  SDL and libpng, then links the PC app against that archive instead of the
  system's) and records the result, or the reason it could not run, in the
  step notes. `targets/mac/README.md` lists `libpng` in the `brew install`
  line and gains a line in Limits: the recipe was checked on Linux only.
  `notes/owner-todo.md` gains one item: run `make -C targets/mac run` and
  report whether it builds and draws.

### Docs

- `README.md`: the package paragraph (NEEDED list with SDL_image, the zip's
  contents with `themes/default/`), the PC instructions (`libsdl-image1.2-dev`,
  `--theme themes/classic`, no `--font`), the `--shot` example, and the
  `--theme` paragraph (the theme gives the whole look; without `--theme` the
  app reads `themes/default/` next to its binary).
- `CLAUDE.md`: `data/` as the fonts' source; `make package`'s NEEDED list
  and the zip's theme folder; the owner's look command without `--font`;
  `make sdl-pc`'s description if it still mentions `--font`.

## Implementation Chunks

1. **Freeze the oracle's values.** In the pinned environment, read
   `sdl_view_open`'s fitted size on `classic`'s font; write the palette's
   eleven entries and that size as constants in `test_sdl_classic`, and the
   size in `test_sdl_render`'s fit check, asserted alongside the existing
   comparisons with the oracle (both must hold). Done: `make sdl-pc` green in
   the pinned environment (goldens 13 byte-identical).
2. **Delete the old renderer; the theme is required, with its default.**
   Delete `sdl_view.*`, `test_sdl_view`, the oracle comparisons of
   `test_sdl_classic` and `test_sdl_render` (constants stay), the
   equivalence harness, `--font`, the `--theme`-less branch; add the
   default lookup; `goldens.sh` and `smoke_sdl.sh` take their new arguments;
   smoke cases (below); `sdl-pc` and `sdl-goldens` follow; README and
   CLAUDE.md for the app side. Done: `make test`, `make lint` green; `make
   sdl-pc` really run in the pinned environment with `goldens: 13 states
   byte-identical`; `make sdl-arm` (docker) links.
3. **Packaging follows.** `build_zip.py` with `THEME_DIR`, the new entry list
   and refusals; `test_package.py`; the Makefile's `PKG_THEME` and NEEDED
   list; README and CLAUDE.md for the package. Done: `make test` green;
   `make package` (docker) writes the zip and passes the NEEDED check with
   `libSDL_image-1.2.so.0`; the zip's theme files are byte-identical to
   `themes/classic/` (`build_zip.py`'s own read-back).
4. **`targets/mac/` follows.** The SDL_image recipe, the link, `run` with
   `--theme`, README, the owner-todo item, the Linux proof of the recipe
   recorded. Done: `make -n -C targets/mac build` shows `SDL_image` in the
   flags; the Linux proof run (or its recorded reason); then the step's full
   verification: `make check`, `make sdl`, `make package`.

## Testing Decisions

- A good test drives the public surface: the app's exit status, stderr,
  captures and SD tree for given arguments and folders; the zip builder's
  exit status, stderr and the zip it writes. No test reads the renderer's
  internals.
- **`smoke_sdl.sh`** (`make sdl-pc`; prior art: its `expect`, `one_line`,
  `listing` and `variant` helpers, step 4's and step 5's theme cases):
  - every existing case drops `--font` and passes `--theme "$theme"`
    (`classic`), except those that test the default;
  - **default lookup**: the built `alttpr` copied to `$tmp/app/App/ALttPR/`
    with `themes/default/` a copy of `classic`; started as `./alttpr` from
    that folder with `--sd` on a separate tree, and by its full path from
    another directory: both exit 0, and their `--shot` and `--shot-text`
    captures are byte-identical to the same run with `--theme "$theme"`;
    started from `App/ALttPR/` with no `--sd`, the SD tree (derived from the
    working directory, H-5-13) is unchanged by a one-frame start;
  - **default missing**: the same copy without `themes/default/`: exit 1,
    one `alttpr: ` line naming `…/themes/default/theme.ini`, SD tree
    unchanged;
  - **default broken**: `bogus = 1` appended to the default's `theme.ini`:
    exit 1, one line starting `alttpr: <path>/themes/default/theme.ini:<n>: `;
  - **`--font` refused**: `alttpr --font x --frames 1` exits 2 with the usage;
    the usage does not mention `--font`;
  - the step 4 check "the same text as no theme" is replaced by the default
    lookup's equality above (there is no theme-less run any more).
- **`goldens.sh`**: its self-proof unchanged; `make sdl-pc` prints `goldens:
  13 states byte-identical` with the new argument form, in the pinned
  environment (plan rule: a `setup:` line does not verify).
- **`test_sdl_classic`, `test_sdl_render`**: as stated above; the frozen
  constants are checked first against the oracle (chunk 1), then alone.
- **`test_package.py`** (`make test`; prior art: its own `Builder`,
  `RealPackage` and `Launch` classes):
  - `Builder` plants a theme folder (`theme.ini`, a `.ttf`, a `.png`, a
    `.txt`); `EXPECTED` becomes the new entry list; byte-identity, modes,
    compression, timestamps and two-runs-identical cover the theme files;
    `test_missing_input_is_refused_naming_it` covers `base.bps` and every
    planted theme file (a deleted theme file changes the entry list: with
    `--check` it is refused; at build time a theme folder lacking
    `theme.ini` is refused naming it);
  - new refusals, each one stderr line naming the offender and no zip
    written: `THEME_DIR` missing; no `theme.ini`; a subdirectory; a symlink;
    a hidden file;
  - `--check` refuses a changed theme file, naming it;
  - `RealPackage` builds from `themes/classic` and checks that every file of
    `themes/classic/` is in the zip under `App/ALttPR/themes/default/`, that
    no `.ttf` sits directly in `App/ALttPR/`, and that `theme.ini` is there;
  - `Launch` is unchanged.
- **`make package`** (docker, outside the net): the NEEDED check with
  `libSDL_image-1.2.so.0`; run once in the step, in the pinned environment.
- **Static checks**: `make lint`, `no_sdl.sh`, `no_literals.py` unchanged in
  form; they must stay green with the deletion.

## Out of Scope

- The new theme, Press Start 2P, frame and sprites (steps 8 and 12); the
  default switched to the new theme (step 13).
- Any change to `app/core/`, `app/sd/`, `rando/`, `ui_cli`, the snapshots,
  the goldens or `environment.txt`.
- Removing `data/`'s DejaVu copy.
- Changing `launch.sh`, `config.json` or `icon.png`.
- The CI `sdl` job's artifact: it still uploads the PC binary alone, which
  now needs `--theme` (or a `themes/default/` next to it) to run; left as a
  note for the owner, since an agent cannot dispatch the job to check a
  change.
- Hot reload, user themes, theme subfolders.

## Hypotheses about step 5

Step 5 (`docs/prd/triage/phase1-step5-sdl-renderer-from-uiscreen.md`) is not
done when this PRD is written; its PRD was read, its code was not.
Everything this PRD assumes about its outcome:

- **H-5-1**: the theme renderer exists in `app/sdl/` with step 5's interface
  (`SdlRender`, `sdl_render_open`, `sdl_render_advance`, `sdl_render_draw`,
  `sdl_render_close`, `SdlRenderFailure`), and the SDL main under `--theme`
  formats its failures from `theme_assets`. If the names differ, this step
  uses step 5's.
- **H-5-2**: without `--theme`, the main still runs today's grid path
  (`sdl_view_open`, `--font`, 40×15 grid); with `--theme`, `--font` is a
  usage error (exit 2). This step deletes the first and makes the second moot.
- **H-5-3**: the frame loop's present callback takes the `UiScreen` and the
  grid, the grid is rendered every frame for `--shot-text`, and a false
  return ends the run with `SDLLOOP_PRESENT_FAILED`.
- **H-5-4**: `themes/classic/` holds exactly four regular files, flat:
  `theme.ini` (with `screen.image = background.png`), `background.png`,
  `DejaVuSansMono.ttf` and `DejaVuSansMono-LICENSE.txt`, the font and licence
  byte copies of `data/`'s. If it holds more, the zip ships them all (the
  builder ships the whole folder); the entry list in `test_package.py`'s
  `RealPackage` check follows the folder, not a fixed list.
- **H-5-5**: `test_sdl_classic` compares `classic` with `sdl_view_palette`
  and with `sdl_view_open`'s fit, and checks the two font copies are
  byte-identical; `test_sdl_render` holds the equivalence harness (grid with
  `data/`'s font against the theme renderer on `classic`) and a fit check
  against `sdl_view_open`; its feature theme's build copies `data/`'s font.
- **H-5-6**: `goldens.sh` takes `ALTTPR FONT SD_FIXTURE --write|--compare DIR
  [--theme THEME]`, passes no `--font` under `--theme`, fingerprints FONT;
  `make sdl-pc` runs it with `--theme ../../themes/classic` and prints
  `goldens: 13 states byte-identical`; `sdl-goldens` still writes without
  `--theme`.
- **H-5-7**: `smoke_sdl.sh` takes `ALTTPR FONT THEME` and holds step 4's and
  step 5's theme cases (missing folder, unknown key, missing image, missing
  font, fit failure, wrong background size, `--font` with `--theme`), all of
  which keep their checks here with `--font` dropped.
- **H-5-8**: SDL_image is wired: `sdl-pc`'s and `sdl-goldens`' `setup:` check
  asks for `SDL_image` and names `libsdl-image1.2-dev`; `SDL_LIBS` includes
  it; `sdl-arm` links `-lSDL_image` (with any `-rpath-link` step 5 needed);
  the CI `sdl` job installs `libsdl-image1.2-dev`.
- **H-5-9**: `targets/mac/` is untouched by step 5 and does not link (its
  `SDL_LIBS` lacks SDL_image), as step 5's step notes state.
- **H-5-10**: the theme renderer does not depend on `sdl_view.h` (no shared
  `SdlRgb`, no shared glyph code). If it does, this step moves what is
  shared into the renderer's own header before deleting `sdl_view.*`.
- **H-5-11**: under `classic`, `sdl_render_open`'s `fit` gives the same size
  as `sdl_view_open` (step 5 proved it), so the frozen constant of chunk 1
  holds for both.
- **H-5-12**: `alttpr` initialises `IMG_Init(IMG_INIT_PNG)` and the device
  binary's dynamic section names `libSDL_image-1.2.so.0` (Onion's soname,
  `docs/onion.md`).
- **H-5-13**: without `--sd`, the SD root is still derived from the working
  directory (`rando_app_sd_root`), so started from `App/ALttPR/` the app uses
  the card's root; this step does not change it.
- **H-5-14**: step 5 updated `CLAUDE.md` for `make sdl-pc`'s prerequisites
  and `app/sdl`'s dependencies; this step edits only what still mentions
  `--font`, the package and `data/`.

## Further Notes

- **Why the default is `argv[0]`'s directory, not the working directory.**
  `launch.sh` makes both the same on the device, but on the PC the owner
  starts `tests/rando/build/sdl/alttpr` from the repository root; the
  working directory would look for `themes/default/` there and find nothing.
  With `argv[0]`, the lookup is "next to the binary", as the plan says, and
  the PC simply passes `--theme`.
- **Why the builder ships the whole folder.** A list of referenced files
  needs the C loader (`theme_assets`) and would drop the licence file, which
  no key names. The folder is the theme by the plan's definition; the
  refusals (subfolder, symlink, hidden file) keep it strict.
- **Why freeze before deleting.** `test_sdl_classic` and `test_sdl_render`
  hold the only assertions that `classic` *is* the old palette and the old
  font size, rather than a theme that merely matches 13 screens. Freezing
  them while the oracle still runs (chunk 1) is what makes the deletion a
  pure removal and not a weakened check.
- **Why the equivalence harness goes.** It compared two renderers; with one
  left there is nothing to compare. Step 5 ran it green on every corner case
  of the grid's rules, and the goldens keep guarding the 13 real states.
- The plan's verification for this step: `make check`, `make sdl`, `make
  package` (docker), goldens still identical.
