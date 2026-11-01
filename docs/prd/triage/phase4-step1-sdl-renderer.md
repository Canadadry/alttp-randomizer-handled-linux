---
title: "SDL renderer and input, shared by the Miyoo and PC builds"
description: "The app exists only as a text grid the agent reads: nothing draws it on the Mini Flip's screen or a PC window, and no button or key reaches it, so neither the player nor the owner can use it."
status: needs-triage
---

## The step (Plan 1, Phase 4, step 1, copied from `docs/plan-1.md`)

> CI is manual-only (`workflow_dispatch`) until the owner turns it on: each step's local target is its verification, and its step note asks the user to run the matching CI job.
>
> 1. **SDL renderer and input**, shared by the Miyoo and PC builds: Onion-style list rendering, and key mapping from the d-pad and buttons, or the keyboard on PC. Verified by a local make target, also added as a CI job: the PC build compiles with no warnings and starts, renders one frame offscreen and exits in a headless smoke test (SDL dummy video driver); the same renderer and input layer cross-compile, with no warnings, against Onion's SDL in the Onion toolchain (compile only). The visual check is the user's (§4).

Context the step relies on, from the plan, `docs/onion.md` and the repository at HEAD (`5aadf64`):

- §2 *UI*: two tabs, Generate and History, "in the style of Onion's menus: a clean font, lists, a cursor, no game graphics". §2 *UI architecture*: "a key in, a screen description out … Two renderers draw that description: SDL (Miyoo and PC) and text (a character grid)." §2: "The app also builds for PC (Linux and macOS, SDL, keyboard instead of the d-pad) … Only the user validates the SDL rendering and the device." §4: "the PC build looks right" is a human check.
- `docs/onion.md` §3: Onion apps use **SDL 1.2** (`<SDL/SDL.h>`, `<SDL/SDL_ttf.h>`), the toolchain sysroot holds SDL 1.2.15, SDL_ttf 2.0.11, SDL_image 1.2.12, `SDL_VIDEO_DRIVER_FBCON` and `SDL_VIDEO_DRIVER_DUMMY`, and **no SDL2**; the same SDL 1.2 libraries ship on the card in `/mnt/SDCARD/miyoo/lib/` (on `LD_LIBRARY_PATH`); SDL2 exists on the card only bundled by Drastic, PICO-8 and `parasyte`. Onion's video setup: `SDL_SetVideoMode(w, h, 32, SDL_HWSURFACE)`, draw into an `SDL_CreateRGBSurface` of the same size, blit, and `SDL_EnableKeyRepeat(300, 50)`. §10: "Phase 4 step 1 chooses between the device's SDL 1.2 and a bundled SDL2".
- `docs/onion.md` §4: an app gets **640×480** unless it ships `full_resolution` (it does not, C1). §5: the button map as SDL 1.2 keys — Up/Down/Left/Right = arrows, A = `SDLK_SPACE`, B = `SDLK_LCTRL`, X = `SDLK_LSHIFT`, Y = `SDLK_LALT`, L1 = `SDLK_e`, R1 = `SDLK_t`, L2 = `SDLK_TAB`, R2 = `SDLK_BACKSPACE`, Select = `SDLK_RCTRL`, Start = `SDLK_RETURN`, Menu = `SDLK_ESCAPE`, Power = `SDLK_FIRST`, volume none; keymon does not grab the input, so the app also receives keymon's chords.
- Step 3.2: `UiGrid` (`app/core/ui_grid.h`), default **40 × 15** cells sized from 640×480 as **16 × 32 px**, one `UiLineStyle` per line (blank, tabs, title, row, selected, disabled, selected-disabled, text, info, error, footer), printable ASCII only in cells (non-ASCII already drawn as `?`), `ui_grid_page` = lines − 4. §5 question 2: `app/sdl/` and `app/main/` are reserved for Phase 4 and step 3.7; `no_sdl.sh` refuses SDL in `app/core` and `app/sd` only.
- Step 3.3: every text the player sees is a `UiStrId`; `no_literals.py` scans `app/core`.
- Step 3.4: `UiInput` (`ui_input_event`): Start, Select and Menu emit on release alone, any chord is swallowed, other buttons emit on every press, repeats included.
- **Step 3.7 chunk 1, committed at `5aadf64` (facts, not hypotheses):** `app/core/ui_app.h` — `UiApp` (caller-owned, about 210 KiB, static), `ui_app_init(app, const UiAppInit *)`, `ui_app_input(app, UiButton, bool down)` → `UI_APP_NONE` / `UI_APP_QUIT` (through the app's own `UiInput`), `ui_app_key`, `ui_app_pending`, `ui_app_run` (blocks; "the shell draws `ui_app_screen` once first"), `ui_app_screen(app, UiScreen *)`, `ui_app_tab`. Once QUIT is returned, every later call returns QUIT.
- `CLAUDE.md` § Checks names "The SDL PC build (`libsdl2-dev`)" as a CI job outside `check`; `make check` needs only host `gcc`, `make` and `python3`. `.github/workflows/check.yml` has one job, `make -k check`, `workflow_dispatch` only.
- `tests/rando/Makefile`: `$(APP_LIB)` (core + sd by wildcard), `$(HOST_LIB)`, `$(RANDO_LIB)` at the default `CFLAGS`; `CORE_INCLUDES` (library + core only); `TOOLCHAIN` (`onion-toolchain.sh`) and `ONION_CFLAGS`, used by `e2e-arm` and `unit-arm`, which fail with a `setup:` line when docker or qemu is missing.

### What this host offers (checked while writing this PRD, Ubuntu 24.04, the same release as GitHub's `ubuntu-latest`)

- `apt` candidates: `libsdl1.2-dev` 1.2.68-2 (**source `sdl12-compat`**: SDL 1.2's API on SDL2, it depends on `libsdl2-dev`), `libsdl-ttf2.0-dev` **2.0.11**-6 (the device's exact SDL_ttf version), `libsdl-image1.2-dev` 1.2.12, `libsdl2-dev` 2.30.0, `libsdl2-ttf-dev` 2.22.0; `fonts-dejavu-core` 2.37 is installed. `pkg-config` knows `sdl` (sdl12_compat) and `SDL_ttf`.
- A throwaway prototype (not committed) using only SDL 1.2's API, built `-std=c99 -Wall -Wextra -Werror -pedantic-errors` with `pkg-config --cflags --libs sdl SDL_ttf`: with `SDL_VIDEODRIVER=dummy` it opened a 640×480×32 mode, opened DejaVu Sans Mono, rendered and blitted text, saved a BMP and exited 0. DejaVu Sans Mono at **26 pt has an advance of 16 px and a height of 32 px** — exactly the 16 × 32 cell. Pushed `SDL_KEYDOWN`/`SDL_KEYUP`/`SDL_QUIT` events came back from `SDL_WaitEvent` after sixteen `SDL_ACTIVEEVENT`s that sdl12-compat queues at start.
- The same source compiled **and linked** (`-lSDL -lSDL_ttf`, dynamic) in the pinned Onion image with `ONION_CFLAGS` and `-pedantic-errors`, with no `-I`: `<SDL/SDL.h>` resolves in the sysroot. The image's `sdl-config` is the **x86 host's** (`-L/usr/lib/x86_64-linux-gnu`), so a cross build must never call `sdl-config` or `pkg-config`.
- A one-character `TTF_RenderText_Blended` surface is the full 16 × 32 cell for every glyph tried (`M g _ [ ~ ^`), while `TTF_RenderGlyph_Blended` returns the glyph's tight bitmap (`_` is 16 × 2): the first can be blitted at the cell's corner, the second would need per-glyph metrics.

## Hypotheses about Phase 3 step 7 chunks 2–3 (not done when this PRD was written)

Step 7's PRD is `docs/prd/triage/phase3-step7-ui-cli.md`; its chunk 1 (`ui_app`) is committed, chunks 2 (SD wiring) and 3 (`ui_cli`, build, smoke) are not. Each assumption about their outcome is labelled, with what this step does if it fails.

- **H-3.7-1** (the SD wiring): `app/sd` provides `RandoAppShell` (caller-owned, about 7.3 MiB, static) and `rando_app_shell_init(shell, sd_root, strings, page, width)` → bool, with the shell's `UiApp` reachable from the caller (a member or an accessor). *If names or shapes differ*, the SDL main adapts to them; step 7's module is not changed, and no second wiring is written.
- **H-3.7-2** (init only reads): `rando_app_shell_init` creates no directory and writes no file — it reads `settings.json` and lists the history folder; its only failure is a path that does not fit. *If it writes something at init*, the smoke test's "tree unchanged" check lists exactly what step 7 writes, rather than being dropped.
- **H-3.7-3** (one library): the shell's source joins `$(APP_LIB)` by the existing `app/sd` wildcard, so the SDL build links it with `$(APP_LIB) $(HOST_LIB) $(RANDO_LIB)` and no new rule. *If it lives elsewhere*, the SDL link line adds that archive.
- **H-3.7-4** (`app/main` and `lint`): `app/main/` holds `ui_cli.c`; `lint` builds `app/main`'s sources with `-pedantic-errors` and `TEST_INCLUDES`, no SDL. *If `lint` takes `app/main/*.c` by wildcard*, this step narrows that list to the non-SDL mains (the SDL main is built with `-pedantic-errors` by the SDL target instead), so `make lint` never needs SDL headers.
- **H-3.7-5** (`no_sdl.sh`): it still scans `app/core` and `app/sd` only, and `ui_cli` builds with no SDL include path, so an SDL main in `app/main/` is allowed. *If step 7 added `app/main` to it*, this step removes it again with the reason (the SDL main lives there, as step 3.2 reserved) and keeps `ui_cli`'s SDL-freedom proved by its build flags.
- **H-3.7-6** (the shell recipe, step 7's Further Notes): one static shell; `rando_app_shell_init(<root>, NULL, ui_grid_page(grid), cols − 2)`; per event `ui_app_input`; when `ui_app_pending`, draw `ui_app_screen` and present it, then `ui_app_run`; exit on `UI_APP_QUIT`; nothing to save on quit. *If step 7 needs another call per frame* (a tick, a flush), the loop makes it.
- **H-3.7-7** (device defaults): after `rando_app_shell_init`, the runner's seed (`/dev/urandom`), date (local time) and digests (§2's `03a63945…`/`edc01f3d…`) are the device's, so the SDL main overrides none of them. *If step 7 leaves one unset*, the SDL main sets it the way `rando_app_gen_init` documents.
- **H-3.7-8** (Makefile shape): `tests/rando/Makefile` keeps `$(APP_LIB)`, `$(HOST_LIB)`, `$(RANDO_LIB)`, `CORE_INCLUDES`, `TEST_INCLUDES`, `TOOLCHAIN`, `ONION_CFLAGS` and `ARM_UNIT_SRC` (without `app/main`) as at `5aadf64`, and adds the `ui_cli` binary and smoke to `test`. *If names moved*, this step uses the new ones.
- **H-3.7-9** (`CLAUDE.md`): step 7 adds `app/main/` and `ui_cli` lines to § Layout and § Checks. *If it did not*, this step's `app/main` line also names `ui_cli`.

## Problem Statement

The player owns a Miyoo Mini Flip and wants to make a seed on it. Today the app is a state machine that only the agent can see, as a framed character grid printed by a host tool: nothing draws it on the console's screen, and no d-pad press or button reaches it. The owner, who must check that the app "looks right" before copying it to the SD card, has nothing to open on a PC either. And the device's SDL question is still open: the PC build was imagined on SDL2 (`libsdl2-dev`), while Onion's toolchain and every Onion app use SDL 1.2, so as things stand the renderer would have to be written twice or the device would need a bundled SDL2.

## Solution

**One SDL 1.2 renderer and input layer for both builds.** The app's SDL code is written against SDL 1.2's API and SDL_ttf 2.0's only. On the Mini Flip it builds in Onion's toolchain against the sysroot's SDL 1.2.15 and SDL_ttf 2.0.11 and runs on the copies Onion already ships on the card — nothing bundled. On a PC it builds against any SDL 1.2 provider: on current Linux distributions that is `sdl12-compat` (`libsdl1.2-dev`, which runs on SDL2), plus `libsdl-ttf2.0-dev` 2.0.11, the device's own SDL_ttf version. There is no shim and no second API.

**What the player sees.** The same 40 × 15 grid the agent reads, drawn cell by cell at 640×480 in a clean monospace font (DejaVu Sans Mono, shipped with the app): a dark background, the tab bar and footer on their own bands, the cursor row as a full-width highlight bar, greyed rows dimmed, info and error lines coloured. Every glyph on the screen is the grid's character at that cell, so the agent's text view and the player's screen never disagree.

**How the player drives it.** The d-pad and buttons, read as the SDL keys Onion's keymap gives them, go through the app's button filter, so keymon's chords never move a cursor. On a PC the same keys work (arrows, Space, Ctrl, Shift, Alt, E, T, Tab, Backspace, right Ctrl, Enter, Escape), plus friendlier aliases (`A`, `B`, `X`, `Y` keys, Page Up / Page Down for L2 / R2); closing the window quits. Pressing Start shows "Generating" on screen before the generation blocks, and presses made while it runs are dropped.

**One main for the console and the PC.** The same program starts on the device from its app folder (the SD root found from it, as `launch.sh` will run it in Phase 4 step 2) and on a PC with `--sd <dir>`. For checks it can stop after a number of frames and save the last one as a BMP.

**Proved by `make sdl`.** A local target, also a manual CI job: the PC build with no warnings; SDL unit tests of the key map, the renderer and the frame loop under the dummy video driver; a headless smoke test that starts the program, renders one frame offscreen and exits; and a cross-compile and link of the same SDL code and main in Onion's toolchain, with no warnings. `make test` and `make check` are unchanged except for one SDL-free scan, and still need no SDL.

## User Stories

1. As the player, I want the app drawn on the Mini Flip's screen, so that I can use it without a PC.
2. As the player, I want it to look like Onion's own menus — a list, a cursor bar, a clean font, no game graphics — so that it feels part of the console.
3. As the player, I want the tab bar at the top showing Generate and History with the active one marked, so that I always know where I am.
4. As the player, I want the row under the cursor shown as a full-width highlight bar, so that I see the selection at a glance.
5. As the player, I want greyed-out settings dimmed, and dimmed differently when the cursor is on them, so that I see what I cannot choose.
6. As the player, I want error lines in a warning colour and notices in a calm one, so that I tell a failure from information.
7. As the player, I want the footer's button hints on their own band, so that they read as help, not as list rows.
8. As the player, I want the more-above and more-below marks and cut texts drawn as on the text grid, so that I know a list scrolls.
9. As the player, I want the d-pad to move and change values, so that I can drive the app like Onion's menus.
10. As the player, I want A, B, X, Y, L1, R1, L2, R2, Select, Start and Menu to do what each screen says, so that the app follows Onion's conventions.
11. As the player, I want a held d-pad direction to repeat at Onion's speed, so that long lists are quick to cross.
12. As the player, I want volume, brightness, CPU and screenshot chords never to act in the app, so that adjusting the console does not change my settings.
13. As the player, I want the Power button left to Onion, so that sleep and shutdown work as usual.
14. As the player, I want "Generating" shown as soon as I press Start, so that I know the console is working.
15. As the player, I want buttons I press while a seed is being made to be ignored, also after it finishes, so that a nervous press cannot start another seed or leave the result screen.
16. As the player, I want the app to start from its folder in Onion's Apps list with no setup, so that it just works after copying.
17. As the player, I want the app to use Onion's own SDL libraries, so that nothing extra must be installed or kept up to date.
18. As the player, I want Menu (or B where the screen says so) to quit back to Onion with the screen restored, so that leaving is clean.
19. As the player, I want the app to use no CPU while I am not pressing anything, so that it does not drain the battery.
20. As the owner, I want a PC build that shows exactly what the console will show, so that I can check the look before copying to the SD card.
21. As the owner, I want the PC build to use my keyboard with the console's keys and readable aliases, so that I can drive it without a controller.
22. As the owner, I want closing the PC window to quit like Menu, so that the PC build behaves like a desktop program.
23. As the owner, I want the PC build to take any directory as the SD card, so that I never touch my real card while testing.
24. As the owner, I want the program to refuse to guess an SD root when it is not started from an `App/ALttPR` folder and no `--sd` is given, so that it never creates `Roms/` folders in the wrong place.
25. As the owner, I want the PC build on Linux from packages my distribution has (`libsdl1.2-dev`, `libsdl-ttf2.0-dev`), so that building it is one install line.
26. As the owner, I want the PC build to compile on macOS from an SDL 1.2 provider such as `sdl12-compat`, so that §2's macOS build stays possible.
27. As the owner, I want one renderer for the console and the PC, so that what I check on the PC is what the console draws.
28. As the owner, I want the font shipped with its licence in the repository, so that the app does not depend on a font the card may lack and stays legal.
29. As the owner, I want to swap the font with a command-line option, so that I can try another monospace font without a rebuild.
30. As the owner, I want the colours in one table, one entry per line style, so that adjusting the look is one place.
31. As the owner, I want `make sdl` to tell me with one `setup:` line which packages are missing, so that a missing library is not mistaken for a bug.
32. As the owner, I want a CI job that runs the same target, so that I can check the SDL build on GitHub with one click.
33. As the owner, I want `make test` and `make check` to stay free of SDL, so that the fast loop and the net need only gcc, make and python3.
34. As the owner, I want `CLAUDE.md` to name the SDL build's real packages instead of `libsdl2-dev`, so that the next session installs the right ones.
35. As the agent, I want a headless smoke test with the dummy video driver, so that I prove the program starts, draws and exits without a display.
36. As the agent, I want the program to save its last frame as a BMP on request, so that a check can look at real pixels without a screen.
37. As the agent, I want the program to stop after a given number of frames, so that a smoke test never hangs waiting for a key.
38. As the agent, I want the renderer tested on pixels that do not depend on the font rasteriser (empty cells are exactly the band colour, filled cells are not), so that the test holds on any FreeType version.
39. As the agent, I want the key map tested against `docs/onion.md`'s table, every button reachable from a device key, so that a wrong key code fails a test, not the console.
40. As the agent, I want the frame loop tested with pushed SDL events and a fake app, so that "Generating is presented before the run" and "presses during the run are dropped" are proved on the host.
41. As the agent, I want the SDL code cross-compiled and linked in Onion's toolchain with `-pedantic-errors`, so that an API the device's SDL 1.2.15 lacks fails before the owner copies anything.
42. As the agent, I want the SDL layer to hold no visible text of its own, checked by the literal scan, so that every text on the screen is still a table id.
43. As the agent, I want the SDL layer to see only the library's and the UI core's headers, so that it cannot reach into the SD layer or the host code.
44. As a future translator, I want the renderer to draw whatever the grid holds, so that a translated table needs no renderer change.
45. As the Phase 4 step 2 author, I want the cross build target and the main's command line settled here, so that packaging only adds `launch.sh`, `config.json`, the icon, the font and `base.bps` around them.

## Implementation Decisions

### 0. The SDL decision: SDL 1.2's API everywhere

- **Decided: the renderer, the input layer and the main are written against SDL 1.2 and SDL_ttf 2.0 only.** Device: Onion's toolchain sysroot (SDL 1.2.15, SDL_ttf 2.0.11) at build time, Onion's `/mnt/SDCARD/miyoo/lib/` copies at run time, dynamically linked, nothing bundled. PC Linux: `libsdl1.2-dev` (sdl12-compat over SDL2) and `libsdl-ttf2.0-dev` 2.0.11, found through `pkg-config sdl SDL_ttf`. PC macOS: any provider of the same two `pkg-config` names (sdl12-compat for SDL; SDL_ttf 2.0 may have to come from MacPorts or a source build), not verified by this step.
- **Why, against a bundled SDL2:**
  - The toolchain image has no SDL2 headers or libraries (`docs/onion.md` §3); a bundled SDL2 means cross-building SDL2 and SDL2_ttf with a Miyoo video backend, or shipping the `parasyte`/Drastic binaries with headers that do not belong to them — a new dependency to pin and keep, against §1 goal 3's "verifiable without the console".
  - Every Onion app draws with SDL 1.2 over `fbcon` exactly as `sdl_init.h` does; following it is the only setup with prior art on the device (rotation, framebuffer size, restore on exit).
  - sdl12-compat makes SDL 1.2's API available on every current PC, so the PC build still runs on SDL2 underneath — `libsdl1.2-dev` pulls `libsdl2-dev` in — and `CLAUDE.md`'s "`libsdl2-dev`" was never a requirement of the code, only of the package set.
  - SDL_ttf 2.0.11 is the same version on the device and on Ubuntu 24.04, so the PC rasterises the font with the device's library version.
- **Why not a shim over both APIs:** two back-ends double the surface to test and the owner's visual check would cover only the PC's; nothing in the app needs an SDL2-only feature.
- **Consequence:** `CLAUDE.md`'s "The SDL PC build (`libsdl2-dev`)" becomes "(`libsdl1.2-dev`, which is sdl12-compat on SDL2, and `libsdl-ttf2.0-dev`)". `docs/onion.md` stays a document of facts; the decision is recorded in the step note and in this PRD.

### 1. The key map (`app/sdl`, module *keys*, names indicative)

- One table from SDL 1.2 key to `UiButton`, the union of the **device keys** (`docs/onion.md` §5: the fifteen codes of the four directions and eleven buttons, one per `UiButton`) and the **PC aliases** (`a`→A, `b`→B, `x`→X, `y`→Y, Page Up→L2, Page Down→R2). One table serves both builds without a compile-time switch: the console never sends the alias codes, and every device code is also a PC key. No alias may reuse a device code (checked by a test).
- `sdl_keys_button(SDLKey, UiButton *out)` → bool; false for every other key, including Power (`SDLK_FIRST`, the same value as `SDLK_UNKNOWN`) and any key with no meaning — those never reach the app.
- `sdl_keys_event(const SDL_Event *, SdlInput *out)` → one of *none*, *button* (with the button and down/up) or *quit*: `SDL_KEYDOWN`/`SDL_KEYUP` of a mapped key give *button*; `SDL_QUIT` gives *quit*; everything else (`SDL_ACTIVEEVENT`, mouse, unmapped keys, expose) gives *none*. Key repeat arrives as further `SDL_KEYDOWN`s, which the core's filter already treats as presses.
- No allocation, no text.

### 2. The renderer (`app/sdl`, module *view*, names indicative)

- `SdlView` (caller-owned): the open font, the cell size, a lazily filled glyph cache, the palette.
- `sdl_view_open(view, font_path, surface_w, surface_h, cols, lines)` → a status enum (OK, FONT_OPEN, FONT_TOO_SMALL, NO_MEMORY), with `TTF_Init` done by the caller. Cell = `surface_w / cols` × `surface_h / lines` (16 × 32 for 640×480 and 40 × 15); any remainder is a border of background. The **point size is the largest in 8…64 whose `'M'` advance fits the cell width and whose `TTF_FontHeight` fits the cell height**; none → FONT_TOO_SMALL. DejaVu Sans Mono gives 26 pt for 16 × 32.
- `sdl_view_draw(view, const UiGrid *, SDL_Surface *dst)`: fills the whole surface with the background, then for each line fills its band (full width, the cell height) in the line style's band colour, then blits each non-space cell's glyph at the cell's top-left corner, horizontally centred when the advance is narrower than the cell. Cells are drawn exactly as the grid holds them — brackets around the active tab, `< value >`, `^`, `v`, `~`, `?` — so the screen and `ui_grid_text` show the same characters.
- **Glyphs** are one-character `TTF_RenderText_Blended` surfaces (full cell height, glyph at its baseline), never `TTF_RenderGlyph_Blended` (a tight bitmap that would need per-glyph metrics). Cached per (character 0x20–0x7E, text colour) on first use; a render failure draws nothing for that cell and is not retried every frame. Blended surfaces carry per-pixel alpha and blend over the band.
- **Palette**: one entry per `UiLineStyle` (band colour, text colour), in a table sized by a new `UI_LINE_STYLE_COUNT` enumerator added at the end of `UiLineStyle` in the core (no layout or snapshot changes). Starting values, for the owner to adjust after the visual check: background `#14161A`; rows `#E6E6E6`, text rows `#B8BCC4`; tabs and footer on a `#22262E` band, tab text `#FFFFFF`, footer text `#9AA0A8`; title `#7FB4FF`; selected: band `#2F6FD6`, text `#FFFFFF`; disabled text `#6B7078`; selected-disabled: band `#3A3F48`, text `#8A9099`; info `#9AD0FF`; error `#FF6B6B`.
- `sdl_view_close(view)` frees the cache and the font. The SDL layer may allocate (it is not `rando/`); everything it allocates is freed by close.

### 3. The frame loop (`app/sdl`, module *loop*, names indicative)

- `sdl_loop_run(app, grid, present, present_ctx, max_frames)` drives a `UiApp` (step 3.7's, `5aadf64`) with SDL events. `present(ctx, const UiGrid *)` is injected: the main draws and flips; tests record the grid's text form. `max_frames` 0 means no limit. Returns *quit* (the app quit or `SDL_QUIT`), *frames* (the limit was reached) or *error* (`SDL_WaitEvent` failed).
- Order: render `ui_app_screen` into the grid and present it (frame 1); stop when the limit is reached; then wait for events (`SDL_WaitEvent`, so no CPU is used while idle). For each event: *button* → `ui_app_input`; `UI_APP_QUIT` → return *quit*; then **if `ui_app_pending`: present the Generating screen, call `ui_app_run`, then drain the event queue, dropping every key event that arrived during the run** (an `SDL_QUIT` among them is kept and ends the loop); then present the current screen. *quit* → return *quit* (nothing is saved on quit, H-3.7-6). An `SDL_VIDEOEXPOSE` re-presents. Every presentation counts as a frame.
- The drain is safe with the core's filter: a key pressed and released during the run is gone entirely; a key released after it only sends a release, which the filter ignores for ordinary buttons and for a starter it never saw pressed.
- The loop sees only `app/core` and `rando/include` headers, plus SDL's.

### 4. The SDL main (`app/main`, one source for the device and the PC)

- Usage: `alttpr [--sd <root>] [--font <file>] [--frames <n>] [--shot <file.bmp>]` (binary name indicative; Phase 4 step 2 may rename it when packaging).
  - `--sd`: the SD root. Without it, the root is `rando_app_sd_root` of the current directory **only when the current directory's last two components are `App` and `ALttPR`** (how `launch.sh` starts it, C2); otherwise usage error. This keeps the device's no-argument start and never creates `Roms/` under an arbitrary PC directory.
  - `--font`: default `DejaVuSansMono.ttf` in the current directory (the app folder on the device, where step 2 ships it).
  - `--frames <n>` (n ≥ 1): stop after n frames, exit 0. `--shot`: on exit, save the offscreen surface (the last frame) as a BMP.
  - Parsing completes before anything else; a usage error prints one line and the usage on standard error and exits 2. A setup failure (SD root does not fit, font missing or too small, SDL or video init) prints one line with SDL's or SDL_ttf's error and exits 1, after `SDL_Quit` so the device's console is restored. A normal quit exits 0.
- Sequence: parse; `rando_app_shell_init(shell, root, NULL, lines − 4, cols − 2)` (H-3.7-1, H-3.7-6; false → exit 1); `SDL_Init(SDL_INIT_VIDEO)`; `SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE)` as Onion's apps do (falls back to a software surface on a PC); `SDL_ShowCursor(SDL_DISABLE)`; `SDL_EnableKeyRepeat(300, 50)` (Onion's values); an offscreen `SDL_CreateRGBSurface` of the video surface's size, 32 bpp; `TTF_Init`; `sdl_view_open` with the default 40 × 15 grid; `sdl_loop_run` with a presenter that draws into the offscreen surface, blits it to the video surface and `SDL_Flip`s; `--shot`; close the view, `TTF_Quit`, `SDL_Quit`.
- The main sets no `SDL_VIDEODRIVER` itself (the caller's environment chooses dummy, x11, wayland or fbcon), and no window caption (a caption would be visible text outside the string table).
- The tool's own lines (usage, setup errors) are English literals in `app/main`, as `ui_cli`'s are: they go to standard error, never to the screen.
- Uses POSIX (`getcwd`) with `_POSIX_C_SOURCE` defined in the source; builds with `-std=c99 -pedantic-errors`.

### 5. The font

- **DejaVu Sans Mono** (Bitstream Vera licence with the DejaVu changes, which allows redistribution with the licence text) is committed under `data/` beside where `base.bps` will live, with its licence file. 343 KiB. The tests and the smoke test use this committed copy, never a system font.
- Monospace is required: the renderer places one glyph per cell. Another monospace font works through `--font` and the point-size search.

### 6. Build: `make sdl` (outside `test` and `check`)

- `tests/rando/Makefile` gains:
  - **`sdl-pc`**: fails with `setup: SDL 1.2 and SDL_ttf 2.0 development files not found (apt install libsdl1.2-dev libsdl-ttf2.0-dev)` when `pkg-config --exists sdl SDL_ttf` fails. Otherwise builds, in a build subdirectory of its own, the `app/sdl` sources with `$(CFLAGS) -pedantic-errors`, `CORE_INCLUDES` and `pkg-config --cflags sdl SDL_ttf` (the SDL layer sees the library and the core only); the SDL main with `TEST_INCLUDES` plus `app/sdl` plus the SDL flags; links the main with the SDL objects, `$(APP_LIB) $(HOST_LIB) $(RANDO_LIB)` (H-3.7-3) and `pkg-config --libs sdl SDL_ttf`; builds and runs the SDL tests (decision 7) with `SDL_VIDEODRIVER=dummy`; runs the smoke test. The SDL flags are evaluated only by these rules, so `make test` never calls `pkg-config`.
  - **`sdl-arm`**: through `$(TOOLCHAIN)`, compiles the `app/sdl` sources and the SDL main with `$(CFLAGS) -pedantic-errors -O2 $(ONION_CFLAGS)` and no SDL include flag (the sysroot provides `<SDL/SDL.h>`; never `sdl-config` or `pkg-config`, which are the host's in that image), compiles `$(APP_SRC) $(HOST_SRC) $(RANDO_SRC)` the way `unit-arm` does, and links dynamically with `-lSDL -lSDL_ttf`. It checks with `file` that the result is an `ELF 32-bit LSB executable, ARM, EABI5` and dynamically linked, and runs nothing. Linking goes one step past the plan's "compile only" on purpose: it is what proves every SDL and SDL_ttf symbol the code uses exists in the device's 1.2.15 / 2.0.11 libraries, and costs one command; Phase 4 step 2 reuses this target for the packaged binary.
  - **`sdl`**: `sdl-pc` then `sdl-arm`.
- Root `Makefile`: `make sdl`, `make sdl-pc`, `make sdl-arm`, each with a help line (outside the net; `sdl-pc` needs the two SDL packages, `sdl-arm` docker).
- `make test` gains only `no_literals.py` run on `app/sdl` as well as `app/core` (a Python scan, no SDL); its summary line says so. `no_sdl.sh` is unchanged (H-3.7-5). `make lint` is unchanged except H-3.7-4's narrowing if needed.
- **CI**: `.github/workflows/check.yml` gains a job `sdl` on `ubuntu-latest`: checkout, `sudo apt-get update`, `sudo apt-get install -y libsdl1.2-dev libsdl-ttf2.0-dev`, `make sdl` (docker is on GitHub's runners; the image is pulled by digest). The workflow stays `workflow_dispatch` only; the step note asks the owner to run it.

### 7. Documentation

- `CLAUDE.md` § Layout: `app/sdl/` is the SDL 1.2 renderer and input layer (sees `rando/include`, `app/core` and SDL only); `app/main/` also holds the SDL main; `data/` holds the shipped font and its licence. § Checks: `make sdl` (= `sdl-pc` + `sdl-arm`) outside the net, its packages, the `setup:` lines; the "`libsdl2-dev`" wording replaced (decision 0); `make test`'s literal check covers `app/sdl` too. § Ground rules unchanged.
- `docs/toolchain.md` § How this repository uses it: one item for `make sdl-arm` (dynamic link against the sysroot's SDL 1.2, why no `sdl-config`).
- How the owner runs the PC build, one line in `CLAUDE.md` § Checks: `make sdl-pc`, then `tests/rando/build/<sdl dir>/alttpr --sd tmp/sd --font data/DejaVuSansMono.ttf`.

## Implementation Chunks

1. **Key map and the `sdl-pc` skeleton.** `UI_LINE_STYLE_COUNT` in the core; the keys module; its SDL test; the `sdl-pc` target (setup check, SDL objects, SDL test build and run under the dummy driver); the literal scan on `app/sdl` in `make test`. Done when `make sdl-pc` passes, `make test` and `make check` stay green, and `make sdl-pc` on a host without the packages prints the `setup:` line (checked by running it with `PKG_CONFIG_PATH` pointing to an empty directory and `PKG_CONFIG_LIBDIR` empty).
2. **Renderer and font.** The view module, the committed font and licence, its SDL test. Done when `make sdl-pc` passes and `make check` is green.
3. **Frame loop, SDL main and smoke test.** The loop module and its SDL test (fake `UiApp` built through `ui_app_init` with fake run, scan, spoiler and delete); the SDL main (decision 4, needs H-3.7-1, H-3.7-6, H-3.7-7); its link in `sdl-pc`; the smoke script. Done when `make sdl-pc` passes end to end and `make check` is green.
4. **Cross build, root targets, CI, docs.** `sdl-arm`, `sdl`, the root targets and help lines, the CI job, H-3.7-4's `lint` narrowing if needed, `CLAUDE.md` and `docs/toolchain.md`. Done when `make sdl` passes (docker available), `make check` is green, and `make sdl-arm` without docker fails with the toolchain script's `setup:` line.

Chunk 1 needs nothing new; 2 needs 1; 3 needs 2 and step 3.7's shell; 4 needs 3.

## Testing Decisions

- **What a good test is here**: it goes through the public functions — a key or event in, a button or event out; a `UiGrid` in, pixels out; pushed SDL events and a `UiApp` in, presented grids and calls out; the program's exit status, files and BMP — never through the modules' internals. Pixel checks use only what does not depend on the rasteriser: a cell holding a space is exactly its band colour on every pixel; a cell holding a visible glyph has at least one pixel different from its band; a band's colour is the palette's. No pixel snapshot is committed (FreeType versions differ between the PC, CI and the device). Screens are compared through `ui_grid_text`, equal to what the core gives at the same point, rather than new snapshot files.
- All SDL tests live apart from `tests/rando/unit/` (a directory of their own beside it, with their own list), so `UNIT_TESTS`, `unit-arm`'s loop and the `test_%` pattern rule never pick them up; they reuse `unit/support.h`'s `test_fail`. They run only under `make sdl-pc`, with `SDL_VIDEODRIVER=dummy`.
- **Keys test**: every row of `docs/onion.md` §5 that has an SDL key maps to its button (Power and volume map to nothing); every alias maps to its button; no alias equals a device code; every `UiButton` is reachable from a device key; an unmapped key (`SDLK_z`, `SDLK_UNKNOWN`) gives false; `sdl_keys_event` on a mapped `SDL_KEYDOWN` and `SDL_KEYUP`, on `SDL_QUIT`, on `SDL_ACTIVEEVENT` and on an unmapped key.
- **View test** (offscreen 640×480×32 surface, the committed font): open gives 16 × 32 cells and a size whose advance and height fit; a hand-built screen through `ui_grid_render` at 40 × 15 covering every `UiLineStyle` → each line's empty cells are exactly its band colour, each non-space cell has glyph pixels, the selected line's band spans the full width; a line of `W` and `_` leaves the empty cells of the lines above and below untouched (no glyph overflows its cell); a 640×480 surface with a 96 × 40 grid → FONT_TOO_SMALL; a missing font path → FONT_OPEN; drawing the same grid twice gives identical pixels (the cache changes nothing); open then close with no leak reported by the test's own count of open/close calls (no allocator instrumentation).
- **Loop test** (a `UiApp` on fakes, as `test_ui_app` builds it at `5aadf64`; a presenter recording each frame's text form; events pushed with `SDL_PushEvent`; an `SDL_QUIT` pushed last as a guard so `SDL_WaitEvent` cannot block):
  - `max_frames` 1 → one frame, equal to `ui_app_screen` of a fresh app, *frames* returned, no event read;
  - Return down/up → the frame before the fake run is the flow's Generating screen (the fake run records the frame count when called), the run is called once, the next frame is the result screen;
  - keys pushed by the fake run itself (Down, A, Return) are dropped: the frame after the run is still the result screen and the run count stays 1;
  - Escape down/up → *quit*; `SDL_QUIT` alone → *quit*; `t` (R1) → the History tab; a sixteen-event burst of `SDL_ACTIVEEVENT` first changes nothing;
  - a chord (Escape down, Down down, Down up, Escape up) moves nothing and does not quit (the core's filter is in the path).
- **Smoke test** (a shell script run by `sdl-pc`, temporary tree removed on exit): a tree holding only `App/ALttPR/`; `SDL_VIDEODRIVER=dummy alttpr --sd <tree> --font <committed font> --frames 1 --shot <tmp>/frame.bmp` → exit 0, the BMP is 640×480 (header read with `python3 -I`) and not a single colour, the tree's file list unchanged (H-3.7-2); the same run with the current directory `<tree>/App/ALttPR` and no `--sd` → exit 0; no `--sd` from another directory → exit 2; an unknown option and `--frames 0` → exit 2; a missing font → exit 1. This is the plan's "starts, renders one frame offscreen and exits".
- **Cross build**: `sdl-arm`'s `file` check; compile and link with `-Werror -pedantic-errors`. Nothing is run under qemu (no sysroot libraries for qemu-arm, and the plan asks compile only).
- **Unchanged nets**: `make test` (plus the `app/sdl` literal scan), `make check`, `make unit-arm` (the new core enumerator only).
- **Prior art**: `test_ui_grid` (render a hand-built screen), `test_ui_app` (a `UiApp` on fakes), `test_ui_input` (the filter), `test_patch_cli` (a program driven end to end with exit statuses), `no_sdl.sh`/`no_literals.py` (scripts run by a target), `e2e-arm`/`unit-arm` (toolchain build, `file` check, `setup:` lines).

## Out of Scope

- Packaging: `launch.sh`, `config.json`, the icon, the release zip, shipping the font and `base.bps` in `App/ALttPR/`, the `data/base.bps` gate (Phase 4 step 2).
- Running the cross-built SDL program under qemu-arm or on the device; any device check (rotation, fbcon behaviour, the app's resolution on the Flip) — the owner's, listed below.
- The performance budget (Phase 4 step 3).
- A per-stage progress indicator (step 3.5 decided against it).
- Full-resolution (752×560) support: no `full_resolution` file is shipped (C1).
- SDL2 builds, a shim, SDL_image (no icon or picture is drawn), sound.
- Mouse or touch input, chords, configurable key maps.
- Any change to the UI core's state machines, layout, strings or snapshots (only the `UI_LINE_STYLE_COUNT` enumerator is added).
- Verifying the macOS build (no macOS host): it uses the same `pkg-config` names and is the owner's to try.
- Turning on CI's push and pull-request triggers (the owner's).
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): the SDL decision and its reason (decision 0); the packages and versions used (`libsdl1.2-dev` 1.2.68 = sdl12-compat, `libsdl-ttf2.0-dev` 2.0.11) and the toolchain's (SDL 1.2.15, SDL_ttf 2.0.11); the chosen point size and cell size; `make sdl` output; the outcome of H-3.7-1 … H-3.7-9; and a request to the owner to run the CI job `sdl` (Actions → check → Run workflow).
- **Owner checks** (visual, §4, and for the device later): the PC build looks right — font, colours, highlight bar, tab bar, footer (`make sdl-pc`, then the command in `CLAUDE.md`); every key on the PC does what the footer says; on the Flip (after step 2): the screen is upright and fills 640×480, the font is legible, the d-pad repeat feels like Onion's, Menu returns to Onion with the screen restored, keymon's volume and brightness chords change nothing in the app.
- **Risks recorded, not settled here**: macOS may lack a packaged SDL_ttf 2.0; on the device, the Miyoo's SDL 1.2 may handle rotation or double-buffering differently from the dummy driver — following `sdl_init.h`'s calls exactly is the mitigation.
- **Traps for the implementer** (each also earns a short comment where it lives): `TTF_RenderGlyph_Blended` returns a tight bitmap, so glyphs are rendered as one-character strings; sdl12-compat queues `SDL_ACTIVEEVENT`s at start; key events queued during a blocking run must be dropped or they act on the result screen; the toolchain image's `sdl-config` is the host's.
- **Decisions taken without a human** (to-prd ran unattended; no `guideline` agent could be spawned from this context, so each was settled here on the plan's goals): SDL 1.2's API for both builds, no SDL2 and no shim; DejaVu Sans Mono committed with its licence; cell-by-cell rendering of the grid exactly as the text form, colours by line style only; one key table for device and PC with aliases; event-driven loop with the queue drained after a run; one main for both builds, the SD root from `--sd` or from an `App/ALttPR` current directory only; the cross target links as well as compiles; `make sdl` outside `check`, the CI job manual; SDL tests in their own directory outside `UNIT_TESTS`; the core gains only `UI_LINE_STYLE_COUNT`.

## Plan review additions (after Phase 3, applied by plan-pilot before triage)

These override anything above that conflicts with them.

- **Step 3.7 is done** (`5aadf64`, `4683c0e`, `1fe54cf`, note in the plan): H-3.7-1, -2, -3, -6, -7, -8, -9 hold as facts. H-3.7-4's "if" branch is true: `LINT_APP_OBJ` takes `app/main/*.c` by wildcard (`tests/rando/Makefile`), so `make lint` must leave out the SDL main by exact path (CI's `check` job has no SDL packages).
- **H-3.7-5's reasoning is wrong:** an SDL header resolves with no `-I` on this host (`/usr/include/SDL/SDL.h`) and in the toolchain sysroot, so build flags prove nothing. `tests/rando/no_sdl.sh` must also scan `app/main`, exempting only the SDL main by exact path, with its self-proof extended to the exemption.
- **SD root:** the SDL main takes its root from `rando_app_sd_root` (app/sd) and adds no new spelling of `App`, `ALttPR` or `/mnt/SDCARD` (replacing the "current directory ends in App/ALttPR" check).
- **CI job** `sdl` uploads the PC binary as a workflow artifact.
- **CLAUDE.md** (chunk 4's commit): § Checks' SDL packages become `libsdl1.2-dev` and `libsdl-ttf2.0-dev`; § Layout becomes "`app/core` uses the library; `app/sd` the library, `rando/host` and `app/core` (it hands the core its runners and stores); `app/sdl` the library and `app/core`;".
