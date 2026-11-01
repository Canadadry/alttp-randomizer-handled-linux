---
title: "ui_cli: key sequence in, text screen out"
description: "The agent cannot look at the whole app (both tabs, a generation, History) as the player would, because nothing composes the tabs and the generation flow into one app state, and no host tool plays keys into it and prints the screen."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 7, copied from `docs/plan-1.md`)

> 7. **`ui_cli`: key sequence in, text screen out**, for the agent's own checks. Verified by a smoke test in `make test`, whose binary links only the objects `make test` already builds (`$(RANDO_LIB)`, `$(HOST_LIB)` and the app archive step 2 adds), never the `-O2` set (`GEN_OBJ`); a clean `make test` is timed before and after, numbers in the step note.

Context the step relies on, from the plan and the repository at HEAD (`1b598e6`):

- §2 *UI architecture*: "The UI is a pure C state machine, with no SDL: a key in, a screen description out … A host command-line tool reads a key sequence and prints the text screen, so the agent can 'look' at the app while working." §2 *UI*: two tabs, Generate and History. §1 goal 3: "the agent sees the UI through a text renderer".
- §5 question 2, decided at step 3.2: `app/core/` (UI core, only `rando/include` and itself on its include path), `app/sd/` (SD layer), and **`app/sdl/` and `app/main/` reserved for Phase 4 and step 7**; host tools stay in `rando/host/`.
- Step 3.2: `UiScreen`, `UiGrid` (default 40 × 15, `ui_grid_page` = lines − 4, text field `cols − 2`), `ui_grid_text` (the framed text form the agent reads); hand-written snapshots, no update mode.
- Step 3.3: every user-visible string through `UiStrId`; `no_literals.py` refuses a letter in any `app/core` literal outside `ui_strings_en.c`.
- Step 3.4: `UiInput` filter (`ui_input_event`: Start/Select/Menu emit on release alone, chords swallowed); `UiGenerate` returns `GENERATE`, `QUIT` (B, Menu), `TAB_PREV`/`TAB_NEXT` (L1/R1) and never acts on them itself; its screen already sets the two tabs, Generate active. `rando_last_settings_load` gives LOADED / MISSING / INVALID.
- Step 3.5: `UiGenFlow` (`ui_genflow_start` → `ui_genflow_pending` → the shell draws the Generating screen once, then `ui_genflow_run`; A/B in DONE give `BACK` "to the unchanged tab", Menu `QUIT`, buttons ignored while generating). `RandoAppGen` (≈ 7 MiB, static), `rando_app_gen_init(gen, sd_root)` (false when a path does not fit), `rando_app_gen_run` (a `UiGenRunFn`), `rando_app_sd_root(app_dir, …)`; tests replace `seed`, `date` and the two digests after init (`test_app_flow` does, with the synthetic base's digests).
- Step 2.2: the pipeline takes the expected digests as inputs, so a synthetic base runs the success path; `rando_parse_uint32` is in `rando_settings.h`.
- Step 2.3 and `test_name`: the default settings with seed 1 name `Open-Ganon-1CA10A.sfc` (the baseline case's config is the table's defaults).
- `tests/rando/Makefile` at HEAD: `make test` builds `$(RANDO_LIB)`, `$(HOST_LIB)`, `$(APP_LIB)` (`app/core` + `app/sd`) and `$(SUPPORT_OBJ)` (`support.o`, `synthetic_base.o`) at the default flags; `GEN_OBJ` (`build/o2/`) is built only for `rando_gen`/`rando_patch` (`e2e`). `lint` covers `rando/src` and `APP_SRC`; `unit-arm` cross-builds every `UNIT_TESTS` entry from `ARM_UNIT_SRC` (which includes `APP_SRC` by wildcard). Clean `make test`: 11.4 s at step 3.5.

## Hypotheses about Phase 3 step 6 (not done when this PRD was written)

Step 6's PRD is `docs/prd/triage/phase3-step6-history-tab.md`. Each assumption about its outcome is labelled, with what this step does if it fails.

- **H-3.6-1** (tab state machine): the core provides `UiHistory` (caller-owned, ≈ 200 KiB), `ui_history_init(state, const UiHistoryInit *)` with strings, `page`, `width`, a `const UiHistoryList *` that init *copies*, a spoiler function, a delete function and one context; `ui_history_key(state, UiButton)` → `NONE` / `QUIT` / `TAB_NEXT` / `TAB_PREV`; `ui_history_screen(state, UiScreen *)`. *If names or argument shapes differ*, the app state adapts to them; step 6's module is not changed.
- **H-3.6-2** (tabs on the History screen): `ui_history_screen` sets the two tab names itself, History active, in every view, as `ui_generate_screen` does for Generate. *If it does not*, the app state sets `tabs`/`active_tab` on the screen it hands over, for both tabs alike.
- **H-3.6-3** (the tab never switches itself): History returns L1/R1 as events from the list and detail views, none from the confirmation, and keeps its view across a switch (so switching back returns to the detail). *If it fails*, the app routes whatever events it does return; the tests below follow step 6's actual rules.
- **H-3.6-4** (re-init after a generation): calling `ui_history_init` again with a fresh list resets the tab to its list, cursor 0, scroll 0 — the newest seed under the cursor — and is the contract for "after an outcome with `ok`, the shell rescans History and re-initialises the tab" (step 6's reading of H-3.5-9). *If step 6 adds a dedicated refresh function instead*, the app calls that.
- **H-3.6-5** (store): the SD layer provides `RandoHistory` (caller-owned, long-lived, ≈ 64 KiB), `rando_history_init(h, sd_root)` → false when a joined path would not fit, `rando_history_scan(h, UiHistoryList *out)` (void; a missing history folder is an empty list without notice), and `rando_history_spoiler` / `rando_history_delete` with the `RandoHistory` as context. *If the scan returns a status*, the shell's scan adapter ignores it (the list's notice flags carry what the player sees).
- **H-3.6-6** (synchronous delete): delete has no pending phase or progress screen, so the app needs no draw-then-run for History. *If step 6 adds one*, the app exposes it through the same pending/run pair as the generation.
- **H-3.6-7** (no app state yet): step 6 adds no top-level app state or tab router (its Out of Scope gives that to step 7). *If it added one*, this step's chunk 1 shrinks to the missing parts (generation flow, rescan, raw input) on top of it, never a second router.
- **H-3.6-8** (test helpers for a temporary SD tree): `test_history` builds its tree with private helpers, as `test_app_flow` and `test_seed_save` do. *If step 6 made shared helpers* under `tests/rando/unit/`, this step's SD test and smoke driver use them instead of new copies; a record for a "seed already on the card" is then written the way `test_history` writes one.
- **H-3.6-9** (folders): History creates no directory; only `rando_app_gen_run` creates `Roms/SFC/ALttPR/` and `App/ALttPR/history/`. A fresh SD root holding only `App/ALttPR/` therefore starts with an empty History. This holds whatever step 6 does; it is stated so no `mkdir` is added here.

## Problem Statement

The agent working on this app cannot play it: there is no console, no emulator, and Phase 4's SDL window is not built yet. Each piece of the UI is tested on its own — the Generate tab, the generation flow, the History tab, each with its snapshots — but nothing puts them together the way the player meets them: start the app, see the Generate tab with the last settings, press Start, wait on "Generating", read the ROM name, go back, press R1, find the seed in History, open it, delete it. Nothing routes L1/R1 between the tabs, feeds the generation's outcome back into History, or decides which screen the player sees. Without that, the agent can neither check a change against the whole app nor try a key sequence to see what happens, and the Phase 4 shell would have to invent the routing itself, untested.

## Solution

**One app state for every shell.** A new core state machine, the *app*, owns the Generate tab, the generation flow and the History tab, plus the button filter. It takes raw presses and releases, decides which screen is shown, routes L1/R1 between the two tabs (keeping each tab where it was), starts a generation when Generate asks for one, and, after a successful generation, rescans History so the new seed is on top when the player switches tabs. It asks for nothing but injected functions, so it stays pure C99 and is tested with fakes.

**One SD wiring for every shell.** A new SD-layer module builds that app on a real SD root: it loads the last settings (with the unreadable-settings notice when needed), prepares the generation runner and the History store on the same root, and hands their functions to the app. The device's main in Phase 4 and `ui_cli` both call it, so the agent drives exactly the state the player will.

**`ui_cli`, the agent's eyes.** A host command-line tool: `ui_cli --sd <root> [options] <key>...`. It builds the app on the given directory as if it were the SD card, plays each key as a press and a release, runs a generation when one is pending (printing the Generating screen first, as the device would draw it), and prints the text screen — the last one by default, or every one with `--every`. Options fix the seed and the date so its output is reproducible, set the grid size, and replace the two expected ROM digests so the synthetic base (the only base the agent has) runs a full generation. No Nintendo ROM is ever needed.

**Proved in `make test`.** `make test` builds `ui_cli` from its own objects only — never the `-O2` generator objects — and a smoke driver runs it on a temporary SD tree holding the synthetic base: a generation, the seed found in History in the same session and at the next start, the seed deleted, every key name accepted, a usage error and an early quit, each compared against hand-written screens and checked on disk.

## User Stories

1. As the agent, I want a command that plays a key sequence into the app and prints the screen, so that I can look at the app while I work.
2. As the agent, I want the screen printed as the same framed text form the snapshot tests use, so that what I read and what the tests compare are one format.
3. As the agent, I want keys named like the console's buttons (`up`, `down`, `left`, `right`, `a`, `b`, `x`, `y`, `l1`, `r1`, `l2`, `r2`, `select`, `start`, `menu`), so that a sequence reads like play.
4. As the agent, I want `down*12` to mean twelve presses, so that long moves stay short to write.
5. As the agent, I want to pass keys on the command line or on standard input, so that I can script long sequences.
6. As the agent, I want only the final screen by default, so that a quick look is one screen.
7. As the agent, I want `--every` to print every screen with the key that led to it, so that I can follow a whole session.
8. As the agent, I want the Generating screen printed before a generation runs in `--every`, so that I see what the player sees while waiting.
9. As the agent, I want to point `ui_cli` at any directory as the SD root, so that I never need the console's card.
10. As the agent, I want a missing `Roms/` and history folder on that directory to be created by the generation, as on the device, so that an almost empty directory is a valid card.
11. As the agent, I want `--seed` and `--date` to fix the seed and the record's date, so that two runs print identical screens.
12. As the agent, I want `--source-md5` and `--base-md5` to replace the expected digests, so that the synthetic base and patch run a real generation without a Nintendo ROM.
13. As the agent, I want `--cols` and `--lines` to set the grid, so that I can check a layout at another size.
14. As the agent, I want an unknown key, a bad option or a missing `--sd` refused with a usage line on standard error and exit status 2, before anything runs, so that a typo never acts on the card.
15. As the agent, I want exit status 1 with a clear line when the SD root cannot be used, so that a setup problem is not mistaken for a UI bug.
16. As the agent, I want `ui_cli` to stop at the key that quits the app and say on standard error how many keys were left, so that I notice a quit I did not mean.
17. As the agent, I want the files a session wrote (ROM, record, last settings) left on the directory, so that the next run starts from them like the next start on the console.
18. As the agent, I want `ui_cli` built by `make test` and by a short target of its own, so that it is always there when I need it.
19. As the player, I want L1 and R1 to switch between Generate and History from either tab, so that tabs work like Onion's menus.
20. As the player, I want each tab to keep its cursor and view when I switch away and back, so that I never lose my place.
21. As the player, I want the app to open on the Generate tab with my last settings, so that making another seed is one press away.
22. As the player, I want the unreadable-settings notice when my saved settings could not be read, so that I know why the defaults are back.
23. As the player, I want Start (or Generate Seed) to show "Generating" and then the result, so that I know the seed is being made.
24. As the player, I want A or B on the result to take me back to the Generate tab as I left it, so that I can make another one.
25. As the player, I want a seed I just generated to be on top of History when I switch tabs, without restarting the app, so that I find it at once.
26. As the player, I want History to stay as it was after a failed generation, so that nothing changes when nothing was made.
27. As the player, I want the History tab to list my seeds as soon as the app starts, so that the tab is never stale.
28. As the player, I want buttons pressed while a seed is generating to do nothing, so that a nervous press cannot start a second one.
29. As the player, I want Menu to quit the app from every screen, and B to quit from the Generate tab and the History list as each tab already decides, so that leaving works the same everywhere.
30. As the player, I want keymon's button chords never to act in the app, so that changing the volume or brightness does not move a cursor.
31. As the owner, I want the device's main (Phase 4) and `ui_cli` to build the app through one function, so that what the agent checks is what runs on the console.
32. As the owner, I want the routing between tabs and the generation flow in the pure C core, so that it is tested on the host and under qemu-arm like the tabs.
33. As the owner, I want `ui_cli` to link only the objects `make test` already builds, so that `make test` does not compile the `-O2` set and stays fast.
34. As the owner, I want a clean `make test` timed before and after this step, so that its cost is known.
35. As the owner, I want `ui_cli`'s source built by `make lint` with `-pedantic-errors`, so that it stays strict C99 like the rest of `app/`.
36. As the owner, I want no Nintendo ROM in the smoke test, so that the repository stays legal and CI can run it.
37. As the owner, I want `CLAUDE.md` to say what `make test` now runs and where `ui_cli` lives, so that the next session finds it.
38. As the net, I want the smoke test to fail when a screen differs from its hand-written snapshot or a file is missing or left behind, so that a broken composition cannot pass.
39. As the net, I want the app state's tests to run under `make unit-arm`, so that the routing behaves the same on the device's CPU.
40. As a future translator, I want the app state and its wiring to add no visible text of their own, so that every text the player sees is still a table id.

## Implementation Decisions

### 1. The app state (core)

A new core module, the *app* (`ui_app`, names indicative), pure C99, no allocation, no stdio, no SDL, no string literal with a letter; only `rando/include` and `app/core` on its include path. It is the one object every shell drives.

- **Init** (`UiAppInit`): the string table (NULL = English), `page` (≥ 1) and `width` (the text field, ≥ 20 — what step 6's tab needs, H-3.6-1), a `UiGenerateInit` (settings, `settings_unreadable`, the unsupported list), the generation run function and its context, and the History functions: a **scan function** `(ctx, UiHistoryList *out)` (new type, owned by the app header), the spoiler and delete functions of step 6 (H-3.6-1), and one History context shared by the three. `ui_app_init` initialises the button filter, the Generate tab, the flow (phase IDLE) and, by calling the scan once into the app's own list buffer, the History tab; the active tab is Generate.
- **Input**: `ui_app_input(app, button, down)` feeds one raw press or release through the app's `UiInput` (step 3.4's filter, unchanged) and, when it emits, acts as `ui_app_key`. `ui_app_key(app, UiButton)` takes an already filtered button (tests and shells without raw events). Both return `UI_APP_NONE` or `UI_APP_QUIT`.
- **Routing** of a filtered button:
  - Generation **pending** (between start and run): ignored, `NONE`.
  - Flow **DONE**: to `ui_genflow_key`; `BACK` → phase IDLE, the Generate tab shown unchanged; `QUIT` → `QUIT`.
  - Otherwise to the active tab. Generate's `GENERATE` → `ui_genflow_start` with the request built from `ui_generate_settings` (options, heart beep, spoiler); `QUIT` → `QUIT`; `TAB_NEXT`/`TAB_PREV` → the other tab. History's `QUIT` → `QUIT`; its tab events → the other tab. With two tabs both events toggle; the order (Generate, History) is the tabs' order on screen, so `TAB_NEXT` from History wraps to Generate. A tab is never re-initialised by a switch.
- **Running**: `ui_app_pending(app)` is `ui_genflow_pending`. `ui_app_run(app)` asserts it, calls `ui_genflow_run`; then, **only when the outcome is `ok`**, calls the scan into the list buffer and re-initialises History with it (H-3.6-4). The active tab stays Generate (step 3.5: back "to the unchanged tab"); the player finds the seed on top at the next R1/L1. A failed outcome leaves History untouched. The app exposes nothing of the outcome beyond its screen.
- **Screen**: `ui_app_screen(app, UiScreen *)` is the flow's screen whenever the phase is not IDLE, else the active tab's screen (tabs set by each tab, H-3.6-2). The app adds and changes nothing on the screen.
- **Quit**: once `QUIT` has been returned, the app is finished: every later `ui_app_input`/`ui_app_key` returns `QUIT` and changes nothing; the screen stays the last one. Nothing is deleted or written on quit (the runner already saved the last settings at generation time).
- `ui_app_tab(app)` returns the active tab (an enum `GENERATE`, `HISTORY`, count) for tests and shells.
- **Size**: the app holds `UiGenerate`, `UiGenFlow`, `UiHistory` (≈ 200 KiB, H-3.6-1) and one `UiHistoryList` scan buffer (≈ 256 entries): a static object in production and tests. History's init copies the list (H-3.6-1), so the one buffer is reused by every rescan.
- **No new string id**: every text comes from the three components. The app header holds no text.

### 2. The SD wiring (SD layer)

A new SD-layer module, the *app shell* (`RandoAppShell`, names indicative), the only place that ties the real SD functions to the app; Phase 4's device main and `ui_cli` both use it.

- `RandoAppShell` (caller-owned, ≈ 7.3 MiB: a static object): a `RandoAppGen`, a `RandoHistory` (H-3.6-5) and the `UiApp`.
- `rando_app_shell_init(shell, sd_root, strings, page, width)` → bool, in order: `rando_app_gen_init` on the root; `rando_history_init` on the same root; `rando_last_settings_load` from the runner's settings path into a `UiGenerateInit` built by `ui_generate_init_defaults` (LOADED → those settings, MISSING → defaults silently, INVALID → defaults and `settings_unreadable`); `ui_app_init` with `rando_app_gen_run` and the runner, a scan adapter over `rando_history_scan`, `rando_history_spoiler` and `rando_history_delete` with the store. False (nothing initialised that a caller may use) when either path set does not fit — the only failure: a missing folder or file is the app's business, shown on its screens.
- **The root is the caller's**: the device main computes it with `rando_app_sd_root` from its app folder (Phase 4); `ui_cli` passes `--sd`. The shell never guesses a root.
- After init, a caller may replace the runner's `seed`, `date`, `source_md5` and `base_md5` (step 3.5's contract for tests), before the first run. `ui_cli` does exactly that for its options; nothing else is changed from outside.
- The shell creates no directory and touches no file at init beyond reading `settings.json` and the history folder (H-3.6-9).

### 3. `ui_cli` (host tool, `app/main/`)

A single source in `app/main/` (step 3.2's reservation), the tool's `main` and nothing else of the app's logic: argument parsing, the key-name table, the loop, printing.

- **Usage**: `ui_cli --sd <root> [--every] [--cols <n>] [--lines <n>] [--seed <n>] [--date <YYYY-MM-DDTHH:MM:SS>] [--source-md5 <32 hex>] [--base-md5 <32 hex>] [--] <key>...`. A key `-` reads further keys from standard input, separated by white space. `--sd` is required (no default root on the host: nothing should ever act on `/mnt/SDCARD` by accident).
- **Keys**: the fifteen lower-case names of `UiButton`, in a table indexed by `UiButton` and sized by `UI_BUTTON_COUNT`, so a button added to the core without a name fails to build; `<name>*<n>` repeats, `1 ≤ n ≤ 999`. Each key is fed as a press then a release through `ui_app_input`, so Start/Select/Menu behave as on the device; chords are not expressible (out of scope; `test_ui_input` covers the filter).
- **Parsing first**: every option and every key (including those read from standard input) is parsed before the SD root is touched. Any error → one `ui_cli: <what>` line and the usage on standard error, exit **2**, nothing on standard output, nothing read or written on the root. Numbers go through `rando_parse_uint32`; `--cols`/`--lines` must satisfy `ui_grid_init`'s bounds; `--date` fields must be in calendar range (month 1–12, day 1–31, hour 0–23, minute and second 0–59); digests are 32 hex digits, either case.
- **Loop**: `rando_app_shell_init` (false → `ui_cli: cannot use SD root <root>` on standard error, exit **1**); apply `--seed` (a fixed seed function), `--date` (a fixed date function), the digests. Then for each key: input it; if a generation is pending, in `--every` print the Generating screen, then `ui_app_run`; in `--every` print the screen after the key. On `QUIT`: stop, print `ui_cli: quit at key <i> of <n>, <n − i> ignored` on standard error (only when keys remain), exit 0. Without `--every`, print only the screen after the last key played (or the initial screen when there is no key).
- **Output**: each screen is `ui_grid_render` then `ui_grid_text` at the chosen size, written as is. With `--every`, each screen is preceded by one header line: `@ init` for the initial screen, `@ generating` for the screen drawn before a run (it comes before the header of the key that started it), and `@ <key name>` (the bare name, once per repetition) for the screen after each key. Without `--every`, no header. Exit 0 whenever the keys were played, whatever the app showed (an error screen is output, not a tool failure).
- **Statics**: the shell, the grid and the text buffer are static objects; `RandoAppSeedFn`/`RandoAppDateFn` take no context, so the fixed seed and date are file-scope values of the tool.
- **Texts**: `ui_cli`'s own lines (usage, errors, the quit notice) are English literals in `app/main/`: they are the tool's messages to the agent, never shown to the player, and `no_literals.py` covers `app/core` only. Every text inside a printed screen is a table id, as always.
- **No SDL** and no POSIX call: stdio, stdlib and string only, so the source builds under `-std=c99 -pedantic-errors`. It is a host tool: it is not cross-built for the device (Phase 4 builds the device main) and not run by `unit-arm`; the state it drives is, through the app's unit tests.

### 4. Build

- `tests/rando/Makefile` gains the `ui_cli` binary in the build directory, linked from `app/main`'s source with `$(APP_LIB) $(HOST_LIB) $(RANDO_LIB)` at the default `CFLAGS` and `TEST_INCLUDES` — **no `GEN_OBJ`, no `-O2`** — and a phony `ui-cli` target that builds it; the root `Makefile` gains `make ui-cli` (help line: builds `tests/rando/build/ui_cli`, the agent's text view of the app).
- `test` depends on `ui_cli` and on the smoke driver (decision 5), and runs the driver after the unit tests and before the existing checks; its closing summary line names the smoke test.
- `lint` builds `app/main`'s sources with `-pedantic-errors` and `TEST_INCLUDES`; its line counts them.
- The new core and SD sources join `APP_SRC` by the existing wildcards, so `$(APP_LIB)`, `lint` and `unit-arm` take them without new rules. `app/main` is **not** added to `APP_SRC` (its `main` would clash in every test binary) nor to `ARM_UNIT_SRC`.
- `no_sdl.sh` keeps its two directories: `app/main/` will hold Phase 4's SDL main. `ui_cli` builds with no SDL include path or library, which is the check for it.

### 5. The smoke driver

- A C program next to the unit tests, named so that it is not a `test_*` unit (`unit-arm`'s loop and `UNIT_TESTS` never pick it up), linked with `$(SUPPORT_OBJ)` and the three archives — `make test`'s objects only. It takes the `ui_cli` binary's path as its argument (the Makefile passes it), builds a temporary SD root under `/tmp` with `mkdtemp` holding only `App/ALttPR/base.sfc` and `base.bps` (the synthetic source and its good patch from `synthetic_base`), runs `ui_cli` through `popen` (POSIX, declared with `_POSIX_C_SOURCE` as the tests already do), compares standard output with hand-written snapshots, checks exit statuses and files, and removes the tree.
- Every run passes `--sd <tree> --seed 1 --date 2026-11-01T14:03:27` and the synthetic digests (formatted as hex from `synthetic_base`'s values). With the table's defaults and seed 1 the ROM is `Open-Ganon-1CA10A.sfc` (`test_name`'s pinned value), so the snapshots are written by hand from the step 3.4–3.6 layouts, never from `ui_cli`'s output.
- Runs, in order, on the same tree (each a fresh process, so each also proves the start-up path):
  1. **Generate and find**: `--every start a r1` → five headed blocks, compared whole against one snapshot: `@ init` the Generate tab with defaults, `@ generating` the Generating screen, `@ start` the success screen with the ROM name, `@ a` the Generate tab again, `@ r1` the History list with the one seed and its date. Afterwards the ROM, its record and `settings.json` exist.
  2. **Next start**: `r1` → the History list, final screen only, the same seed listed (proves the scan at init).
  3. **Delete**: `r1 x a` → the History screen after the delete (`Seed deleted`, the empty list); the ROM and the record are gone, `settings.json` kept.
  4. **Every key name**: the fifteen names once each in one run, `menu` last → exit 0 (an unknown name would exit 2); standard output is one screen.
  5. **Early quit**: `menu down` → exit 0, the quit notice on standard error, standard output the screen at the quit.
  6. **Usage**: `bogus` and a missing `--sd` → exit 2, nothing on standard output, the tree unchanged (no file added).
  7. **Bad root**: `--sd` naming a path longer than the runner's limit → exit 1.
- The snapshots live in the snapshot directory with the others, edited by hand only.

### 6. Documentation

- `CLAUDE.md` § Layout: `app/main/` holds the mains, `ui_cli` (the host tool) first. § Checks: `make test` also builds `ui_cli` and runs its smoke test; `make ui-cli` builds it, and how to use it in one line (`tests/rando/build/ui_cli --sd <dir> --every <keys>`). Only these lines change.

## Implementation Chunks

1. **App state (core).** `ui_app` (decision 1) and `test_ui_app` with fake run, scan, spoiler and delete functions, and its snapshots. Needs step 6's tab (H-3.6-1 … H-3.6-4, H-3.6-7). Done when it passes, `no_literals.py`, `no_sdl.sh` and `make lint` pass, and `make check` is green.
2. **App shell (SD).** `RandoAppShell` and `rando_app_shell_init` (decision 2), and `test_app_shell` on a temporary SD tree. Needs chunk 1 and step 6's store (H-3.6-5, H-3.6-8, H-3.6-9). Done when it passes and `make check` is green.
3. **`ui_cli`, build and smoke.** Time a clean `make test` first (`make -C tests/rando clean`, then `make test`, wall time, three runs, median); the tool (decision 3), the Makefile and root targets (decision 4), the smoke driver and its snapshots (decision 5), the `CLAUDE.md` lines (decision 6); then time a clean `make test` the same way, and check that the build log of a clean `make test` holds no `build/o2/` object. Done when `make test` and `make check` are green and both timings are recorded for the step note (the commit message carries them).

Chunk 1 is independent of `app/sd`; 2 needs 1; 3 needs 2.

## Testing Decisions

- **What a good test is here**: it goes through public functions and the tool's command line only — `ui_app_*` screens rendered by `ui_grid_render`/`ui_grid_text` at 40 × 15, `rando_app_shell_init` then the app, and `ui_cli`'s standard output, standard error, exit status and files on disk. Expected screens are hand-written snapshots, never produced by the code under test, with no update mode (step 3.2's practice). Where a screen is already pinned by a component's own snapshots, the app's test checks *equality with the component driven the same way* rather than repeating the snapshot.
- **`test_ui_app`** (core; fakes count their calls; the fake scan returns a scripted list, the fake run a scripted outcome):
  - init calls the scan exactly once; the first screen equals `ui_generate_screen` of a tab initialised from the same `UiGenerateInit`;
  - L1 and R1 from Generate show History, and from History show Generate; Generate's cursor after Down×3 survives a round trip; History's detail view survives a round trip (H-3.6-3); L1/R1 in History's confirmation change nothing;
  - Start → pending, the screen equals the flow's Generating screen, every button while pending changes nothing and calls nothing; `ui_app_run` calls the run exactly once with the tab's settings (options, heart beep, spoiler after toggling it); the success screen; A → the Generate tab exactly as before Start;
  - after an `ok` outcome the scan is called a second time and History, on R1, lists the new entry on top with the cursor on it; after a failed outcome no second scan and History unchanged;
  - `QUIT` from Generate on B and on Menu, from the History list on Menu, from the flow's result on Menu; after `QUIT` every input returns `QUIT` and the screen is unchanged;
  - raw input through `ui_app_input`: Menu acts only on release; a Menu press with a Down press in between moves nothing and quits nothing (the filter is wired);
  - `ui_app_tab` follows every switch.
  - Snapshots (hand-written, 40 × 15): the History tab reached by R1 with two fake seeds; History after a successful generation's rescan, three seeds, the new one on top under the cursor. Two.
- **`test_app_shell`** (SD; temporary tree with private helpers, H-3.6-8): a root holding only `App/ALttPR/` → Generate with defaults, no notice, History empty without notice; a `settings.json` saved by `rando_last_settings_save` with non-default values → those values on the Generate screen; an unparsable `settings.json` → the defaults and the unreadable-settings notice; a record and its ROM already on the card (written as `test_history` writes them) → listed in History at init; a root too long for the runner → false; end to end with the synthetic base and digests and a fixed seed and date: Start, run, A, R1 → the new seed on top in the same session.
- **The smoke driver**: decision 5. It is the step's "smoke test in `make test`".
- **Registration**: `test_ui_app` and `test_app_shell` in `UNIT_TESTS` and in the `LOGIC_TESTS` filter-out list (no fixture mode); both run under `make unit-arm` (their sources join `ARM_UNIT_SRC` through `APP_SRC`; counts for the step note). The smoke driver is outside `UNIT_TESTS`, run by `test` only.
- **Prior art**: `test_app_flow` (temporary SD root with the synthetic base, the runner's digests and functions replaced after init, snapshots of the flow), `test_ui_genflow` and `test_ui_generate` (fakes, hand-written snapshots), `test_last_settings` (settings files on disk), `test_patch_cli` (a host CLI driven end to end on synthetic files).

## Out of Scope

- The SDL renderer, input mapping, the device's `main` and its frame loop: Phase 4 step 1 (it calls `rando_app_shell_init` and `ui_app_input`/`ui_app_pending`/`ui_app_run`/`ui_app_screen`).
- Finding the SD root from the app folder on the device (`rando_app_sd_root` exists; Phase 4 calls it).
- Cross-building `ui_cli` or running the smoke test under qemu-arm.
- Chords, held buttons, key repeat timing or raw press/release syntax in `ui_cli`.
- A seed or settings entry in `ui_cli` other than through the app's own keys (the seed option only fixes the runner's draw, for reproducibility).
- Any change to the Generate tab, the generation flow, the History tab or their snapshots; any new string id.
- Translating `ui_cli`'s own messages.
- The open warnings of `notes/phase2-warn-review.md`.
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): clean `make test` before and after (median of three each, same machine), and that no `build/o2/` object is built by it; `make unit-arm` counts with the two new tests; the outcome of H-3.6-1 … H-3.6-9; the `ui_cli` usage line.
- **For Phase 4 step 1**: one static `RandoAppShell`; `rando_app_shell_init(<rando_app_sd_root of the app folder>, NULL, ui_grid_page, cols − 2)`; per SDL event `ui_app_input`; after each event, if `ui_app_pending`, draw `ui_app_screen` and present it, then `ui_app_run`; draw `ui_app_screen`; exit on `UI_APP_QUIT`. The SDL grid's size must give the same `page` and `width` as the text grid it mirrors.
- **For the agent** (also in `CLAUDE.md`): `make ui-cli`, then `tests/rando/build/ui_cli --sd tmp/sd --every <keys>`; a card directory needs only `App/ALttPR/` with a base for a generation, and only the synthetic base exists without the owner's ROM — `test_app_flow`'s and the smoke driver's way of writing it is the reference.
- **Decisions taken without a human** (to-prd ran unattended): the app state is a new core module owning the button filter and both tabs, rescanning only after an `ok` outcome and staying on the Generate tab; the SD wiring is one shared function; `--sd` is required with no default; keys are fed as press-and-release through the filter; parsing completes before any SD access; `--seed`, `--date` and the two digest options exist for reproducibility and for the synthetic base; the smoke driver is a C program run by `make test` outside `UNIT_TESTS` and outside `unit-arm`; `ui_cli` is linted but not cross-built; `app/main` stays out of `no_sdl.sh`; no new string id.
