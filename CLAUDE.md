# Notes for Claude sessions

*ALttPR on Miyoo*: an on-device *A Link to the Past Randomizer* seed
generator for the Miyoo Mini Flip running OnionOS. An Onion app, driven by the
d-pad, reads a Japanese 1.0 ROM, generates a seed with every alttpr.com
setting (a C port of VT, frozen at `sporchia/alttp_vt_randomizer`
`219fcafd029dab597b8db400efafd8f56f8b4edb`), and writes a patched `.sfc` that
Onion's SNES core plays; a History tab lists and deletes seeds. It also builds
for PC (SDL) so the owner can check the rendering. Start with the current plan
in `docs/`, then `README.md`.

## Layout

- `rando/` is the randomizer library: `include/` its public headers, `src/`
  its sources. The generator, the oracle and their tests are taken over from
  `Canadadry/alttp-ds` (`rando/`, `tests/rando/`).
- `rando/host/` is host-side code on top of the library: the case reader, the
  spoiler JSON, the playthrough, the output, and the generator's `main`
  (`host/main/rando_gen.c`).
- `app/` is the Onion app's own code. `app/core/` is the UI core: pure C99
  like the library, built with only `rando/include` and itself on the include
  path, no SDL. `app/sd/` is the SD-card layer (`record.c`, `seed_save.c`).
  `app/sdl/` is the SDL 1.2 renderer and input layer (key map, view, frame
  loop). `app/core` uses the library; `app/sd` the library, `rando/host` and
  `app/core` (it hands the core its runners and stores); `app/sdl` the
  library and `app/core`; nothing in `rando/` includes `app/`. Its tests live
  in `tests/rando/unit/`, the SDL ones in `tests/rando/sdl/`.
  `app/main/` holds the mains: `ui_cli.c`, the host tool that plays a
  key sequence into the app on a directory used as the SD card and prints
  the text screen; and `sdl_main.c`, the SDL app for the device and the PC.
- `data/` holds the shipped font (DejaVu Sans Mono) and its licence.
- `tests/rando/` holds its tests: `unit/test_<module>.c`, one binary per
  module, registered in `tests/rando/Makefile`'s `UNIT_TESTS`, failing through
  `unit/support.h`'s `test_fail`.
- `tests/rando/cases/` holds the e2e cases, one directory each with its VT
  golden; `tests/rando/oracle/` the PHP VT oracle (`gen_golden.sh`, `vt.patch`)
  and the fixtures it writes.

## Checks

- **`make test` is the fast loop**: unit tests, on the host, in seconds, with
  no network and no external service. Never make it slower: anything slow or
  needing a service is a separate target, called from `make check`.
- **`make check` is the net**: everything that must be green before a push.
  A change that keeps it green is correct; one that breaks it is a bug,
  whatever the unit tests say. A target failing because its environment is
  missing (a service down, a dependency not installed) is a setup problem,
  not a bug.
- Never weaken a check to get green: no skipped or deleted test, no edited
  expected value, no lowered threshold, unless that is the deliberate change,
  stated with its reason.
- Throwaway files go in `tmp/` (git-ignored).
- Install: nothing (`make install` says so). The checks need only host `gcc`,
  `make` and `python3`.
- `make test` = `make -C tests/rando test` (the randomizer's and the app's
  unit tests, linked with the app archive `librando_app.a`; the UI snapshots
  in `tests/rando/unit/snapshots/`, edited by hand only; and
  `tests/rando/no_sdl.sh`, which fails on any SDL include under `app/core/`,
  `app/sd/` or `app/main/` (`sdl_main.c` exempt by exact path) after proving
  itself on planted ones; and `tests/rando/no_literals.py`, which fails on
  any string literal with a letter under `app/core/` outside the English
  table `ui_strings_en.c`, or under `app/sdl/`, after proving itself on
  planted sources; and `ui_cli`, built from `make test`'s
  own objects, with its smoke test `unit/smoke_ui_cli.c`) plus
  `test-template` (the tooling under `.claude/`).
- `make ui-cli` builds `ui_cli` alone; look at the app with
  `tests/rando/build/ui_cli --sd <dir> --every <keys>` (`ui_cli` with no
  argument prints its usage).
- `make check` = `test` plus `lint` (`rando/` and `app/` built with
  `-std=c99 -pedantic-errors`, except `app/sdl/` and `sdl_main.c`, which
  `make sdl` builds that way), `logic` (unit tests replaying the oracle fixtures),
  `selftest` (the e2e driver's own self-test), `e2e` (every case diffed
  against its golden, one `DIVERGENT <case>` line per failure) and
  `e2e-combos` (every combination case diffed against its golden, kept out
  of `e2e`). Run
  `make -k check` to see every failure at once.
- `make -C tests/rando measure`, outside the net, gives the per-case times
  every later budget is measured against.
- `make e2e-arm`, outside the net, builds the generator with Onion's pinned
  toolchain (`onion-toolchain.sh`, `docs/toolchain.md`) and runs the e2e
  cases on it under `qemu-arm`. `make unit-arm`, outside the net too, builds
  every unit test the same way and runs it under `qemu-arm` from
  `tests/rando`, `--fixtures` included for `LOGIC_TESTS`. Both need docker
  and qemu-user; a missing one fails with a `setup:` line, a setup problem,
  not a bug.
- `make sdl` = `sdl-pc` + `sdl-arm`, outside the net. `make sdl-pc` needs
  `libsdl1.2-dev` (sdl12-compat on SDL2) and `libsdl-ttf2.0-dev`, else a
  `setup:` line: it builds the SDL layer and `sdl_main.c`, runs the SDL tests
  of `tests/rando/sdl/` and the smoke test `sdl/smoke_sdl.sh`, all with the
  dummy video driver. `make sdl-arm` needs docker: it compiles and links the
  same code with Onion's toolchain against its SDL 1.2, and runs nothing.
  The owner looks at the PC build with `make sdl-pc`, then
  `tests/rando/build/sdl/alttpr --sd tmp/sd --font data/DejaVuSansMono.ttf`.
- Not in the net, on purpose: `tests/rando/oracle/gen_golden.sh` (the PHP VT
  oracle) runs only to regenerate goldens. The SDL build (`make sdl`,
  `libsdl1.2-dev` and `libsdl-ttf2.0-dev`) and the qemu-arm run of the cross
  build are not `check`.
  `make patch-header` (the base patch's declared sizes, `BPS=<file>`) is
  outside it until `data/base.bps` exists.
- CI: `.github/workflows/check.yml` runs `make -k check`, and in a job
  `sdl` `make sdl` (the PC binary uploaded as the artifact `alttpr-pc`),
  manually only (`workflow_dispatch`) until the owner turns on its push and
  pull-request triggers.

## Ground rules

- **The rando goldens are the source of truth.** Never regenerate a golden to
  get green: `gen_golden.sh` runs only when VT, its patch or the case list
  changes on purpose, in its own commit. *Goal: an agent cannot play the game;
  matching VT byte for byte is the proof that the ROM is right.*
- **`rando/` stays pure C99**: no `malloc`, no platform library, built by host
  gcc and by any cross compiler. *Goal: the same code runs on the host, on the
  Miyoo and under qemu.*
- **No Nintendo ROM in the repository or in CI**; only the base patch
  (`data/base.bps`) is committed. Tests use synthetic ROMs and write logs.
  *Goal: a legal repository, verifiable without the ROM.*
- **VT is frozen** at `219fcafd`, with no tracking of later alttpr.com
  updates. *Goal: a stable oracle.*
- **The UI core is a pure C state machine, with no SDL**: a key in, a screen
  description out, drawn by an SDL and a text renderer; every string shown to
  the user goes through the translation table. *Goal: the agent sees the UI
  through the text renderer, and translations stay possible.*
- **Controller-first**: every screen is driven by the d-pad and buttons, in
  the style of Onion's menus, never a PC-style panel. *Goal: the reason
  zelda3randomizer was rejected.*
- **Never launch the game or an emulator.** *Goal: verification is the host
  tests; playing and checking the SDL rendering is the owner's job.*
- **Comments**: a comment stays only if it carries a trap someone would
  reintroduce (40 words at most) or a C contract the name doesn't state
  (who frees the memory, what is returned when a field is missing). `TODO` is
  allowed. In a `Makefile`, only help for reading a command. Everything else
  goes in the commit message. *Goal: comments that stay true.*

## Language

Everything written in the repository is in English: code, comments, notes,
skills, commit messages, PR titles and descriptions, plans, PRDs. The
conversation with the owner can be in French.

## What we don't do

- **Never merge a pull request**, in any way (GitHub tool, `gh`, auto-merge).
  Merging is the owner's job; denied in `.claude/settings.json`.

## Plans

Plans live in `docs/plan-<N>.md` and are run with the skills of `.claude/skills/`:
`grill-me` → `plan-write` → `plan-converge` → `plan-pilot`. A finished plan is
deleted (git is the history); findings go to `notes/`, not to the plan.
`progress.md` at the root is a running plan's checklist, guarded by the git
hook `plan-pilot` installs (`.claude/skills/plan-pilot/scripts/progress_check.sh --install`).

## Branches

- **Never work on or push a `claude/*` branch.** A session starts on one; at
  its first commit it creates a branch named after the work, then commits and
  pushes there as it goes (`git push -u origin <branch>`).
- Name: `<type>/<short-topic>`, the type of the first commit (`feat/import-csv`,
  `fix/empty-header`, `chore/ci`); `plan-<N>-<topic>` for a session that
  starts by writing a plan.
- The pre-push hook `.claude/hooks/no_claude_branch.sh` refuses any push to
  a `claude/*` branch; the `SessionStart` hook of `.claude/settings.json`
  installs it.
- No pull request unless the owner asks for one; `plan-pilot` opens its own
  at the end of a finished plan.
