---
title: "Goldens of the current SDL rendering"
description: "Nothing captures today's SDL frames yet, so the coming theme engine has nothing to prove pixel identity against."
status: needs-triage
---

## Problem Statement

Plan 2 replaces the SDL app's grid renderer with a theme engine, and phase 1's
proof is that the `classic` theme gives byte-identical pixels to today's
renderer. That proof needs today's frames, captured before the renderer
changes, for a fixed set of states that covers every way a line is drawn.

Today the SDL app (`alttpr`) can only render its first frame headless
(`--frames 1 --shot`). It cannot be driven through a key sequence, its seed
and date come from the clock, and its base digests are the real JP 1.0 ones,
so it cannot reach Seed Ready, History or Generation Failed on a synthetic SD
card with no ROM. Nothing compares frames against stored captures, so a later
step cannot tell whether it changed a pixel.

## Solution

1. The SDL app takes the same fixed-run inputs as `ui_cli`: a key sequence
   (same key names and `<key>*<n>` syntax), `--seed`, `--date`,
   `--source-md5`, `--base-md5`, plus `--shot-text <file>`, which writes the
   text grid of the captured frame. The parsing and the applying of these
   options are one shared, pure-C module used by both mains, so the two
   tools cannot drift. Nothing drawn changes.
2. A standalone fixture tool writes synthetic SD cards with no ROM: `good`
   (synthetic source and base patch, as `smoke_ui_cli` builds) and
   `bad-source` (the synthetic source with one byte changed, so generation
   fails with "Base ROM is not the JP 1.0 ROM"), and prints the override
   flags that match them. Step 9 of the plan reuses it.
3. A state table lists the 13 states to capture: name, fixture, keys,
   optional frame cap, and the strings the captured frame's text must
   contain (the proof the state is reached).
4. A golden script, outside `make test`, plays every state into the SDL app
   (dummy video driver) from a fresh copy of its fixture, checks each state's
   strings in its `--shot-text`, and either writes the BMPs (write mode, run
   twice, refused unless both runs are byte-identical and the goldens folder
   is empty) or compares them against `tests/rando/sdl/goldens/` (compare
   mode, one `DIFFERENT <state>` line per mismatch).
5. The goldens are generated once in the pinned environment and committed
   alone; then the comparison is wired into `make sdl-pc`.

## User Stories

1. As the plan pilot, I want one BMP per listed state of today's SDL app, so that later steps can prove the theme engine draws the same pixels.
2. As the plan pilot, I want the goldens generated from the current code before any renderer change, so that they record today's look and not the engine's.
3. As the plan pilot, I want the golden comparison run by `make sdl-pc`, so that every later step checks pixel identity with the command it already runs.
4. As the plan pilot, I want one `DIFFERENT <state>` line per mismatching state, so that I know which screen a change broke.
5. As the plan pilot, I want a missing golden or an extra golden to fail the comparison, so that a state cannot silently drop out of the proof.
6. As the plan pilot, I want the comparison to prove itself on a planted corruption each run, so that a comparison that compares nothing cannot pass.
7. As the plan pilot, I want the comparison to name the environment it was pinned to and refuse with a `setup:` line in a different one, so that a FreeType or SDL_ttf difference is not mistaken for a renderer bug.
8. As the plan pilot, I want the write mode to refuse when goldens already exist, so that nobody regenerates goldens to get green.
9. As the plan pilot, I want the write mode to render every state twice and refuse unless both runs agree byte for byte, so that the goldens are deterministic.
10. As the plan pilot, I want each captured state to carry its text grid and expected strings, so that a golden of the wrong screen is caught when it is written.
11. As the agent, I want to drive the SDL app with the same key names as `ui_cli`, so that a key sequence found with `ui_cli` can be replayed in the SDL app.
12. As the agent, I want `--seed`, `--date`, `--source-md5` and `--base-md5` in the SDL app, so that its seed names, dates and digest checks are fixed and match a synthetic base.
13. As the agent, I want scripted keys to survive a generation, so that one sequence can generate a seed and then open History.
14. As the agent, I want `--frames` to still cap the frames of a scripted run, so that the Generating frame, drawn before the generation runs, can be captured.
15. As the agent, I want a scripted run to end after its last key, so that `--shot` captures the screen the keys led to.
16. As the agent, I want a `menu` key in the script to quit as it does interactively, so that scripted and interactive runs agree.
17. As the agent, I want bad keys and bad override values refused with exit status 2 and the usage, as `ui_cli` does, so that a typo in the state table fails loudly.
18. As the agent, I want the scripted keys to go through the real key map (button to device key to `sdl_keys_event`), so that the capture uses the same input path as the device.
19. As the agent, I want one shared module for these options in both mains, so that `ui_cli` and the SDL app cannot disagree on a key name or a date format.
20. As the agent, I want `ui_cli`'s behaviour, messages and snapshots unchanged by the refactor, so that the net stays green with no expected value edited.
21. As the agent, I want a fixture tool that writes a synthetic SD card with no ROM, so that the goldens never need a Nintendo ROM.
22. As the agent, I want a `bad-source` fixture, so that Generation Failed is reached by real generation code, not by a stub.
23. As the agent working on step 9, I want the fixture tool callable on its own with a target folder and fixture name, printing the matching override flags, so that the mockup key sequences can reuse it.
24. As the agent, I want each state to start from a fresh copy of its fixture, so that saved settings and History of one state never leak into another.
25. As the agent, I want the state list to cover every line style of today's palette (tabs, title, row, selected, disabled, selected disabled, text, info, error, footer, blank) and both scroll markers, so that the proof covers every colour pair the renderer draws.
26. As the agent, I want the Generate tab captured at the top, the middle and the end of its list, so that both scroll markers and the Generate Seed action are in the goldens.
27. As the agent, I want greyed rows with a greyed selected row, and a greyed selected Generate Seed action, so that the disabled and selected-disabled styles are in the goldens.
28. As the agent, I want an info message and an error message captured, so that both message styles are in the goldens.
29. As the agent, I want Generating, Seed Ready and Generation Failed captured, so that every genflow screen is in the goldens.
30. As the agent, I want the History list, the empty History, a seed's detail with its spoiler and the delete confirmation captured, so that every History screen is in the goldens.
31. As the owner, I want the goldens committed alone, so that their commit is easy to audit and never mixed with code.
32. As the owner, I want the environment the goldens were made in recorded, so that I know where the comparison is meaningful.
33. As the owner, I want the CI `sdl` job to run the same comparison, so that a drift between environments shows up as a named `setup:` line, not a silent pass.
34. As the plan pilot, I want the environment's ability to install SDL_image and run Playwright's Chromium recorded, so that steps 5 and 10 know their environment works.
35. As a maintainer, I want CLAUDE.md's Checks section to describe the golden comparison and the write target, so that later sessions know the check exists and how not to misuse it.
36. As a maintainer, I want the SDL app's existing first frame unchanged by these flags, so that the existing smoke test and the device build behave as before.

## Implementation Decisions

- **Shared fixed-run options module, in the SD layer.** A new pure-C module
  in `app/sd/` (built into the app archive, no SDL) owns: the key-name table
  in `UiButton` order (moved out of `ui_cli`), key-token parsing
  (`<key>`, `<key>*<n>` with 1 to 999, `-` for standard input, at most 4096
  keys), parsing of `--seed`, `--date YYYY-MM-DDTHH:MM:SS`, `--source-md5`
  and `--base-md5` (today's `ui_cli` rules), and applying them to a
  `RandoAppShell` (seed and date callbacks, the two digests). It lives in
  `app/sd/` because it writes `RandoAppShell`'s generator fields and because
  `app/core/` forbids letter literals outside the English table. The fixed
  seed and date are module-level statics (the callbacks take no context):
  its contract states one fixed run per process. It returns status values;
  each main prints its own message, so `ui_cli`'s messages stay byte for
  byte (`bad --seed`, `unknown key`, `bad repeat count`, `too many keys`…).
- **`ui_cli` uses the module**; its options, outputs, exit statuses and the
  snapshots are unchanged (`smoke_ui_cli` is the proof).
- **Key map inverse.** `app/sdl`'s key map gains `sdl_keys_key(UiButton,
  SDLKey *)`, returning the device key of each button (the key the Miyoo
  sends, not an alias); every `UiButton` has one, and
  `sdl_keys_button(sdl_keys_key(b)) == b` for all.
- **Scripted input in the SDL loop.** `sdl_loop_run` takes an optional
  script (a key list); with one, each key is played as an `SDL_KEYDOWN` then
  an `SDL_KEYUP` event for its device key, built in memory and passed to
  `sdl_keys_event`, never pushed on SDL's queue, so the drain after a
  generation (which drops queued keys) does not drop scripted ones. Frames
  are presented exactly where an interactive run presents them. When the
  script is exhausted the loop returns a new result, `SDLLOOP_SCRIPT_END`;
  the frame cap still applies and wins when reached first; a quit from the
  app (`menu`) returns `SDLLOOP_QUIT`. Without a script the loop is
  unchanged.
- **SDL main flags.** `alttpr` accepts, besides its current flags: keys as
  positional arguments after the options (and after `--`), exactly as
  `ui_cli`; `--seed`, `--date`, `--source-md5`, `--base-md5`; and
  `--shot-text <file>`, which writes `ui_grid_text` of the last presented
  grid. `--shot` and `--shot-text` are written whether the run ended on the
  frame cap, the script's end or a quit. Usage errors exit 2 with the usage
  line, as today. `sdl_view_draw`, the palette, the font fitting and the
  surface formats are not touched.
- **SD fixture tool.** A host C program under `tests/rando/` (pure C, no
  SDL, linked with the test support and synthetic-base objects), built on
  demand by its own make target and by the golden rules, not by `make test`.
  Usage: `<tool> <folder> <fixture>`; it creates the folder's
  `App/ALttPR/` with the base ROM file and base patch file at the shell's
  paths, and prints on one line the `--source-md5 … --base-md5 …` flags that
  match the synthetic base. Fixtures: `good` (as `smoke_ui_cli`'s tree) and
  `bad-source` (source byte 0 inverted, same printed flags, so the source
  digest check fails). An unknown fixture or an existing non-empty folder:
  exit 2, nothing written.
- **State table.** A plain text file next to the golden script, one state
  per line, tab-separated: name, fixture, frame cap (`-` for none), keys
  (ui_cli syntax), expected strings separated by ` | `. Every run uses
  `--seed 1 --date 2026-11-01T14:03:27` and the fixture's digests, as
  `smoke_ui_cli` does. The 13 states and the strings each must show (the keys
  are the implementer's to find with `ui_cli`, the strings decide):
  1. `generate_top` (good): "Seed Settings", "< None >", "v".
  2. `generate_middle` (good): list scrolled with both scroll markers
     (`^` and `v`), cursor on a setting row in the middle.
  3. `generate_end` (good): cursor on "Generate Seed", `^` shown.
  4. `generate_greyed_rows` (good): Goal "Master Sword Pedestal" and World
     State "Retro", cursor on one of the two greyed rows,
     "Not supported with".
  5. `generate_greyed_action` (good): cursor on the greyed "Generate Seed",
     "Not supported with Goal" (info style).
  6. `generate_refused` (good): START pressed on the greyed state, the
     error-style "Not supported with Goal".
  7. `generating` (good): frame cap stops on the pre-run frame,
     "Generating Seed", "Please wait...".
  8. `seed_ready` (good): "Seed Ready", "Open-Ganon-1CA10A.sfc",
     "Play it from the SNES list".
  9. `generation_failed` (bad-source): "Generation Failed",
     "Base ROM is not the JP 1.0 ROM".
  10. `history_list` (good): two seeds generated, "Generated Seeds",
      "Open-Ganon-1CA10A.sfc", "Open-Ganon-1CA10A-2.sfc",
      "Generated 2026-11-01 14:03".
  11. `history_empty` (good): "No seeds yet".
  12. `history_detail_spoiler` (good): Spoiler in History "On" before
      generating, detail opened, "Spoiler", "B Back  X Delete".
  13. `history_confirm` (good): "This cannot be undone", "A Delete  B Keep".
  Together they cover every `UiLineStyle` but `UI_LINE_STYLE_COUNT`; the
  script fails if a state's text lacks one of its strings.
- **Golden script.** A POSIX shell script with an inline `python3 -I` part
  (style of `smoke_sdl.sh`), outside `make test`. Arguments: the SDL binary,
  the font (`data/DejaVuSansMono.ttf`), the fixture tool, and a mode.
  Every run: `SDL_VIDEODRIVER=dummy`, `LC_ALL=C`, a fresh temporary SD copy
  per state, BMP and text written to a work folder under
  `tests/rando/build/sdl/`.
  - `--write <goldens dir>`: refuses (exit 1) if the folder holds any BMP;
    renders every state twice into two work folders, fails on any byte
    difference between them (`NONDETERMINISTIC <state>`), then copies the
    BMPs and writes the environment fingerprint file.
  - `--compare <goldens dir>`: first proves itself on a planted case (a copy
    of one golden with one pixel byte flipped must give `DIFFERENT`; a
    removed golden must fail) in a temporary folder; then compares the
    fingerprint (mismatch: one `setup: goldens pinned to <fp>, this host is
    <fp>` line, exit 1); then renders each state once and compares bytes,
    printing `DIFFERENT <state>` per mismatch, `MISSING <state>` for a state
    with no golden and `EXTRA <file>` for a golden with no state; exits 1 on
    any; on success prints `goldens: 13 states byte-identical`.
- **Environment fingerprint.** One text file in the goldens folder, part of
  the goldens commit: the `pkg-config --modversion` of `sdl`, `SDL_ttf`,
  `freetype2` and, when present, `sdl2` (sdl12-compat renders through SDL2),
  plus the font file's SHA-256. Compared as a whole.
- **Make targets.** A new target outside every net, `sdl-goldens`, builds
  what the script needs and runs it in write mode; it says it refuses an
  existing set. `sdl-pc-build` runs the compare mode after the SDL tests and
  the smoke test, and its summary line mentions it. Neither `test` nor
  `check` runs either.
- **Pinned environment** (this decision settles where the comparison runs):
  the goldens are made and compared in this session's container: Ubuntu
  24.04.5 LTS, sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11,
  FreeType 2.13.2 (`freetype2` 26.1.20), SDL_image 1.2.12 installed, docker
  available. The CI `sdl` job (ubuntu-latest, today Ubuntu 24.04 with the
  same noble packages, expected identical but unverified) cannot be
  dispatched by the agent; the step therefore records the comparison as
  pinned to `make sdl-pc` in that container, and the CI job runs the same
  comparison whenever the owner dispatches it, failing with the named
  `setup:` line if its fingerprint differs.
- **Goldens commit.** The BMPs and the fingerprint file, alone, in one
  commit made by running `make sdl-goldens` on the code of the previous
  commits; message `test: add goldens of the current SDL rendering`, stating
  the environment.

## Implementation Chunks

1. **Shared fixed-run options.** The `app/sd/` module, its unit test
   registered in `UNIT_TESTS`, `ui_cli` moved onto it. Done: `make test`
   green, no snapshot changed, `make lint` green.
2. **Scripted SDL app.** `sdl_keys_key`, the scripted `sdl_loop_run`,
   `alttpr`'s new flags; SDL tests and `smoke_sdl.sh` extended. Done:
   `make sdl-pc` green; `make sdl-arm` green; a one-off check that the first
   frame (`--frames 1 --shot`, no new flag) of the new binary is
   byte-identical to the previous commit's (built in a `tmp/` worktree),
   stated in the commit message.
3. **Fixture tool, state table and golden script.** Both modes and the
   self-proof, the `sdl-goldens` target; not yet wired into `sdl-pc`. Done:
   write mode into a `tmp/` folder succeeds with every state's strings
   found; compare mode against that folder passes; a planted flipped byte,
   a removed and an added golden each fail with their line; `make sdl-pc`
   and `make test` green.
4. **The goldens, committed alone.** `make sdl-goldens` into
   `tests/rando/sdl/goldens/` in the pinned environment; the commit holds
   only that folder.
5. **Comparison in `make sdl-pc`, and the record.** `sdl-pc-build` runs the
   compare mode; CLAUDE.md's Checks section describes it, `sdl-goldens`, the
   pinned environment and the fingerprint `setup:` line; `make help` lists
   `sdl-goldens`. Done: `make sdl-pc` prints
   `goldens: 13 states byte-identical` (really run, not a `setup:` line);
   `make check` green; the environment probe is run once and its result
   handed to the step notes: `pkg-config --modversion SDL_image` succeeds,
   and Playwright's Chromium (`playwright` 1.63.0, browsers under
   `PLAYWRIGHT_BROWSERS_PATH=/opt/pw-browsers`) takes one headless
   screenshot of a mockup SVG into `tmp/`.

## Testing Decisions

- Good tests here check behaviour through public interfaces: what a parser
  accepts and refuses, which events a script produces through the real key
  map, which frames a run presents, which files a run writes and what exit
  status it gives. No test reads a module's internals.
- **Shared options module**: a unit test in `tests/rando/unit/`
  (one binary, `test_fail` from `support.h`): every key name maps to its
  button; `*1`, `*999` accepted, `*0`, `*1000`, unknown names refused;
  the key cap; valid and invalid dates (month 13, 1969, wrong separators);
  seed and md5 parsing; applying overrides changes the shell's seed, date
  and digests and nothing else. Prior art: `test_app_shell.c`,
  `test_app_gen.c`.
- **`ui_cli`**: unchanged `smoke_ui_cli` and its snapshots are the
  regression proof; no new expected value.
- **Key map**: `test_sdl_keys.c` gains the inverse round trip for every
  button. Prior art: its `test_device_keys`.
- **Loop**: `test_sdl_loop.c` gains: a script plays down and up per key and
  presents the same frames as the pushed-event run; scripted keys after a
  generation are played (the counterpart of `test_keys_during_run_dropped`);
  script end returns `SDLLOOP_SCRIPT_END`; a frame cap inside the script
  stops on the pre-run Generating frame (counterpart of
  `test_generating_before_run`); `menu` returns `SDLLOOP_QUIT`.
- **SDL main**: `smoke_sdl.sh` gains: a scripted run with the overrides on a
  synthetic tree writes `--shot` and `--shot-text`, whose text holds the
  expected screen; bad key, bad `--seed`, bad `--date`, bad md5 exit 2; the
  existing cases unchanged. Prior art: its `expect` helper.
- **Fixture tool and golden script**: tested by the script's own self-proof
  in compare mode (planted flipped byte, removed golden) and by write mode's
  double-render check, in the style of `no_sdl.sh` proving itself on planted
  sources. They are not unit-tested in `make test`, which must stay fast and
  SDL-free.
- **The goldens** are verified by chunk 5's `make sdl-pc` run.

## Out of Scope

- Any change to what the SDL app draws: palette, font fitting, layout.
- The theme engine, `theme.ini`, `classic`, SDL_image wiring into the build
  (steps 3 to 6).
- The mockup inventory and the structured footer (step 2).
- Mockup key sequences and `make theme-compare` (steps 9 and 10); this step
  only provides the reusable fixture tool.
- A saved-settings-unreadable fixture: the info style is covered by the
  greyed states.
- Dispatching the CI `sdl` job (only the owner can).
- Changing `make test` or `make check`.

## Further Notes

- Facts for the step notes (plan section 6): goldens made and compared in
  the Claude Code cloud container, Ubuntu 24.04.5 LTS, sdl12-compat 1.2.68
  on SDL2 2.30.0, SDL_ttf 2.0.11, FreeType 2.13.2, SDL_image 1.2.12
  installed, docker running (`make sdl-arm` passes), Playwright 1.63.0 with
  Chromium under `/opt/pw-browsers`. The CI `sdl` job was not dispatched;
  the comparison is pinned to `make sdl-pc` in that container, and the
  fingerprint file names it.
- From step 2 on, the plan's rule applies: a `setup:` line from the
  comparison does not verify a step; the pilot runs it in the pinned
  environment or stops.
- Goldens are never regenerated to get green (plan rules, CLAUDE.md): the
  write mode's refusal on an existing set enforces it.
- The `history_list` and `history_detail_spoiler` states generate seeds
  with seed 1 twice, which gives the `-2` suffix seen in today's snapshots;
  each state's generation runs on the synthetic base in well under a second.
