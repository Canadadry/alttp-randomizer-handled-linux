---
title: "Generation flow"
description: "Pressing Generate does nothing yet: the player never sees a seed being made, never learns the file name of the ROM, and gets no readable reason when the base ROM is missing, the generator gives up or the SD card refuses the write."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 5, copied from `docs/plan-1.md`)

> 5. **Generation flow.** First, the step note estimates the Cortex-A7 generation time from the slowest case's host `-O2` time at the end of Phase 1, and decides the progress indicator per generator stage (if over about 10 s) so this screen and its tests include it. A "Generating…" screen, then success (the file name) or an error message: base ROM missing or wrong MD5, generator gave up, SD card write failed. Every value of RandoSaveStatus, RandoAssembleStatus and RandoBpsResult maps to a string id through a table sized by the enum's count, with a test failing on a value without one; patch_cli.c's BPS result names follow the same rule. Verified by snapshot tests with an injected generator, for success and for each failure.

Context the step relies on, from the plan and the repository at HEAD:

- §2 *Files on the SD card*: the base ROM at `App/ALttPR/base.sfc`, its MD5 checked, "a clear message if it is wrong"; ROMs to `Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc`; records to `App/ALttPR/history/<code>.json`. *The seed is always random*; the app remembers the last settings used. *Spoiler*: only decides whether the record keeps it, never touches the ROM.
- §2 *UI architecture*: a pure C state machine, no SDL, a key in, a screen description out; every shown string through the translation table. `CLAUDE.md`: the core is pure C99, never launches the game, and `make test` must stay fast.
- Phase 4 step 2: the zip holds `App/ALttPR/` with the binary, `launch.sh`, `config.json`, the icon and `base.bps`, so the base patch is a file the app reads at run time, next to `base.sfc`.
- `docs/onion.md` §10, consequences for this step:
  - **C2**: the binary finds the SD root from its own folder (`../..` of `App/ALttPR/`, or `/mnt/SDCARD`) and joins `RANDO_SD_ROM_DIR` and `RANDO_SD_HISTORY_DIR` to it; `launch.sh` starts it in `App/ALttPR/`.
  - **C3**: `rando_seed_save` creates no directory; `Roms/SFC/ALttPR/` (not in the zip) must be created before the first save, and `App/ALttPR/history/` shipped or created.
  - **C4**: after writing a ROM, the app deletes `Roms/SFC/SFC_cache6.db` and `SFC_cache2.db`, as Onion's `reset_list.sh` does, so the SNES list shows the new ROM.
- At HEAD: `rando_seed_save` (`app/sd/rando_seed_save.h`) names, assembles straight into the ROM folder, writes the record, and returns `RandoSaveStatus` (OK, NO_NAME, ASSEMBLE, RECORD) with the assemble status and BPS code in its result; both directories must exist. `rando_assemble` returns `RandoAssembleStatus` (OK, SOURCE_SIZE, SOURCE_MD5, PATCH, PATCH_SIZES, BASE_MD5, UNSUPPORTED, GAVE_UP, OUTPUT); `RandoBpsResult` has eight values (OK … TARGET_CRC). None of the three enums has a count. `patch_cli.c`'s `bps_name` is a static array sized by its initializer, with an `"unknown"` fallback; the same file holds a static `read_capped` (read a file up to a cap, report a larger one) and a static `draw_seed` (`/dev/urandom`, then a time mix), and a 4 MiB patch cap.
- Step 2.2's note: "the pipeline takes the expected source and target digests as inputs, so the synthetic tests run its success path; one test checks that the CLI and the app pass §2's `03a63945…` and `edc01f3d…`" — the app half of that test is this step's.
- Step 2.3's note: `make unit-arm` needs `-D_FILE_OFFSET_BITS=64` for tests that list directories (already set).
- `notes/phase2-warn-review.md`: "`rando_seed_save.h`'s SD path constants are unused until step 3.5"; "repeated read/write-file helpers"; "the 4 MiB patch cap in two places".
- Step 3.2 (done): `UiScreen` (tabs, title, rows of kind setting/action/text, cursor −1 for none, scroll, info or error message, footer, borrowed texts), the 40 × 15 grid, hand-written snapshots, no update mode. Step 3.3 (done): `UiStrId` from one X-macro list with arities, `ui_str`, `ui_str_fill`, `ui_strings_check` (printable ASCII, at most 38 characters with `{}` counting zero, arity), `tests/rando/no_literals.py` refusing any letter in an `app/core` literal outside `ui_strings_en.c`.

## Hypotheses about Phase 3 step 4 (not done when this PRD was written)

Step 4's PRD is `docs/prd/triage/phase3-step4-generate-tab.md`. Each assumption about its outcome is labelled; its consequence when it fails is stated.

- **H-3.4-1** (settings table): a library settings header owns `RandoHeartBeep` (moved out of `rando_rom.h`, which still includes it) and the thirteen-setting table; a `RandoOptions` plus a `RandoHeartBeep` hold every generation setting. *If it fails* (`RandoHeartBeep` still in `rando_rom.h`), this step includes whichever library header declares it; nothing else changes.
- **H-3.4-2** (buttons): `UiButton` (Up … Menu) and the `UiInput` filter exist in the core; screens receive filtered buttons only. *If it fails*, the flow adds the minimal `UiButton` enum it needs (A, B, Menu at least) in its own header, and the filter stays step 4's business.
- **H-3.4-3** (Generate tab): `ui_generate_key` returns `UI_GENERATE_GENERATE` on Start, or on A on the action row, only when no unsupported pair holds; `ui_generate_settings(state, &options, &heart_beep, &spoiler)` reads the result; `ui_generate_screen` shows the tabs `Generate` / `History` with Generate active; the tab's state is caller-owned and changed only by its own key function. *If it fails* (other names), the end-to-end tests of chunk 5 call whatever step 4 provides; the flow's interface takes plain settings and never includes the Generate tab's header.
- **H-3.4-4** (last settings): `rando_last_settings_save(path, options, heart_beep)` returns false on failure and writes through a temporary file then a rename; the path is a constant `App/ALttPR/settings.json`, relative to the SD root, next to the other SD path constants. Step 4's PRD hands the call to this step: "on `UI_GENERATE_GENERATE`, … save them with `rando_last_settings_save` before generating (a failed save is not a reason to refuse generation; whether it is shown is step 5's choice)". *If it fails* (no such function), the runner skips the save and the step note says so; remembering settings stays step 4's open item.
- **H-3.4-5** (string ids and snapshots): step 4 added its ids to the same X-macro list and English table, and its snapshots to the same directory, so ids added here go after step 4's and nothing renumbers a snapshot. *If it fails*, this step's ids are added the way step 4 added its own.
- **H-3.4-6** (patch CLI): step 4 rewrote `patch_cli.c`'s settings parsing over the table but left `bps_name`, `read_capped`, `draw_seed`, the 4 MiB cap and the `rando_assemble` switch as they are at HEAD, and `test_patch_cli`'s expected messages unchanged. *If it fails* (any of them moved or changed), chunk 1 and 2 apply to wherever they now live, with the same rule: messages byte for byte unchanged.

## Problem Statement

The player has chosen their settings and pressed Generate, and nothing happens: there is no screen telling them a seed is being made, no screen telling them where the new ROM is and what it is called, and no message when it fails. The failures they can actually meet on a console are mundane — the base ROM was not copied, a US or headered ROM was copied instead, the base patch is missing from a hand-made install, the SD card is full or read-only, the generator gave up on a seed — and each would today be an unexplained silence or a crash.

Behind it, nothing turns the pipeline's results into words. `rando_seed_save`, `rando_assemble` and the BPS module report enums with no count, so a value added later would reach the screen with no text and nothing in `make test` would notice; `rando_patch` names BPS results through a list nothing ties to the enum. And the device-side chores §2 and `docs/onion.md` §10 assign to the first write are undone: no code finds the SD root, creates `Roms/SFC/ALttPR/` or `history/`, reads `base.sfc` and `base.bps`, draws the seed, stamps the date, or deletes Onion's SNES list caches so the new ROM shows up.

## Solution

**A timing estimate first, and no per-stage progress indicator.** Measured while writing this PRD, the slowest case is well under the 10 s line on the Cortex-A7 even at a pessimistic slowdown (decision 1 below), so the generation screen is one static "Generating Seed / Please wait..." screen drawn before the blocking call, and its tests include no progress.

**Three screens inside the Generate tab.** After Generate, the tab shows *Generating Seed* while the seed is made, then either *Seed Ready* — the ROM's file name, where it is, whether the spoiler was kept, and "Play it from the SNES list" — or *Generation Failed* — one error line saying what went wrong, and one line saying what to do about it. A or B goes back to the settings, exactly as they were; Menu quits.

**One SD-side runner does the whole device chore list.** From the SD root it reads `App/ALttPR/base.sfc` and `App/ALttPR/base.bps`, saves the last settings, creates `Roms/SFC/ALttPR/` and `App/ALttPR/history/` when missing, draws a random seed, stamps the local date, calls `rando_seed_save` with §2's digests, and after a successful write deletes Onion's SNES list caches. It returns an outcome made of string ids, never text.

**Every status has a text, checked.** `RandoSaveStatus`, `RandoAssembleStatus` and `RandoBpsResult` gain a count, and each maps to a string id (plus a what-to-do line) through a table of exactly that size; a test walks every value and fails on one without a text. `rando_patch`'s BPS names follow the same rule, its output unchanged.

**Tested without a ROM.** The flow's tests inject a fake runner; the runner's tests use the synthetic base on a temporary SD tree, with injected digests, seed, date and, for the statuses a real run cannot reach, an injected save function. Snapshots show the generating screen, success and each failure the plan names.

## User Stories

1. As the player, I want a "Generating Seed" screen as soon as I press Generate, so that I know the console is working and not frozen.
2. As the player, I want that screen to say it takes a few seconds, so that I wait instead of pressing buttons.
3. As the player, I want the Generate tab to stay highlighted while the seed is made, so that I know where I am.
4. As the player, I want the success screen to show the ROM's file name, so that I can find it in the SNES list.
5. As the player, I want the success screen to say the ROM is in `Roms/SFC/ALttPR`, so that I know which folder of the list to open.
6. As the player, I want the success screen to say whether the spoiler was kept in History, so that I know what I chose.
7. As the player, I want the success screen to tell me to play it from the SNES list, so that I know the next step.
8. As the player, I want the new ROM to appear in the SNES list without pressing Refresh, so that I can play it right away.
9. As the player, I want to be told to refresh the SNES list when the app could not clear its cache, so that a ROM that does not show up is not a mystery.
10. As the player, I want A or B on the result screen to bring me back to my settings unchanged, so that I can make another seed with one or two changes.
11. As the player, I want Menu to quit from the result screen as it does elsewhere, so that leaving works the same everywhere.
12. As the player, I want a clear message when `App/ALttPR/base.sfc` is missing, with where to put the JP 1.0 ROM, so that I can fix my install.
13. As the player, I want a clear message when the base ROM has the wrong size (a headered dump), so that I know to use a headerless one.
14. As the player, I want a clear message when the base ROM is not the Japanese 1.0 ROM, so that I do not try a US ROM again.
15. As the player, I want a clear message when the base patch is missing, unreadable, refused or for another ROM, with "reinstall the app", so that I know the app files are the problem, not my ROM.
16. As the player, I want the generator giving up to say "generate again", so that I know a new random seed will likely work.
17. As the player, I want a clear message when the SD card refuses a write (full, read-only, folder not creatable), so that I check the card.
18. As the player, I want no half-written ROM or orphan record left after a failure, so that History and the SNES list stay clean.
19. As the player, I want every message to fit on one line of the screen, so that nothing reads as cut off.
20. As the player, I want the seed always random, so that each press makes a new seed (§2).
21. As the player, I want the settings I generated with to be remembered even when the generation fails, so that I do not set them again.
22. As the player, I want the History record's date to be my console's local time, so that History reads naturally.
23. As the owner, I want `Roms/SFC/ALttPR/` and `App/ALttPR/history/` created when missing, so that the zip only has to ship `App/ALttPR/` (C3).
24. As the owner, I want the SD root found from the app's folder, with `/mnt/SDCARD` as the fallback, so that the app works wherever Onion mounts the card (C2).
25. As the owner, I want the base patch read from `App/ALttPR/base.bps`, so that the zip's file is the one used (Phase 4 step 2).
26. As the owner, I want every value of `RandoSaveStatus`, `RandoAssembleStatus` and `RandoBpsResult` to map to a string id through a table sized by the enum's count, so that a new status cannot reach the screen without a text.
27. As the owner, I want a test that fails on any value without a text, so that the rule is enforced by `make test`.
28. As the owner, I want `rando_patch`'s BPS names under the same rule, with its output byte for byte unchanged, so that the CLI and the app cannot drift.
29. As the owner, I want the runner's own failures (files missing, folders) under the same rule, so that there is one way statuses become words.
30. As the owner, I want the UI core to see only string ids from the runner, never file paths or stdio, so that the core stays pure and testable.
31. As the owner, I want the file reader and the seed draw that `rando_patch` already has to be shared with the app, not copied, so that the warn-review's duplicate-helper warning does not grow.
32. As the owner, I want the app to pass §2's `03a63945…` and `edc01f3d…` digests by default, checked by a test, so that step 2.2's promise holds for the app too.
33. As the owner, I want the runner's digests, seed, date and save call injectable, so that tests run the success path and every failure without a Nintendo ROM.
34. As the owner, I want the timing estimate and the progress-indicator decision written down with their numbers, so that Phase 4 step 3 can confirm or reopen them.
35. As the agent, I want snapshot tests of the generating, success and failure screens, so that I see what the player sees.
36. As the agent, I want an end-to-end test from the Generate tab's Start through the real runner on a temporary SD tree, so that the whole flow is proven on the host.
37. As the agent, I want every failure value rendered once and checked for a full, uncut text, so that §4's "generation success and failure screens" coverage holds for all of them.
38. As the agent, I want the cache removal, the folder creation and the files left after each failure checked on a temporary SD tree, so that C3 and C4 are verified, not assumed.
39. As the net, I want the new sources built by `make lint` with `-pedantic-errors` and the new tests run by `make unit-arm` under qemu-arm, so that the flow behaves the same on the device.
40. As a future translator, I want each status and each advice line to be its own id, so that my language can word each one.

## Implementation Decisions

### 1. The timing estimate and the progress-indicator decision (the step's first item)

- **Numbers taken while writing this PRD** (an export of HEAD `bb6a3c8` in a throwaway directory, Intel Xeon @ 2.10 GHz, gcc, `-O2`):
  - `make -C tests/rando measure`, `MEASURE_REPS=5`: slowest case `item_placement-basic`, **24.7 ms** median for `rando_generate` (step 1.4 recorded 21.0 ms; steps 1.5–1.11 add no draw); next slowest `goal-triforce-hunt` 8.4 ms; every golden wins at attempt 0.
  - Seed sweeps through `rando_measure sweep`: baseline 1024 seeds, all at attempt 0, max 12.1 ms; `item_placement-basic` 256 seeds, all at attempt 0, **max 33.9 ms**; `goal-triforce-hunt` 256 seeds, all at attempt 0, max 15.8 ms.
  - The whole pipeline (`rando_assemble`: two MD5s, the BPS apply, generation, the ROM writes, the cosmetic defaults, the checksum) on the synthetic base, 20 seeds, throwaway driver: baseline 23–26 ms, `item_placement-basic` **37–43 ms** (best–mean).
- **Estimate**: the Mini Flip's Cortex-A7 (the toolchain's `-mtune=cortex-a7`, nominally 1.2 GHz, slower if the user lowers the clock with keymon) is taken as **20× slower** than this host per thread, with **50×** as the pessimistic bound. Worst observed host pipeline ≈ 50 ms (33.9 ms generation + ≈ 15 ms for the rest) → ≈ 1 s at 20×, ≈ 2.5 s at 50×; plus SD-card I/O (read 1 MiB + the patch, write 2 MiB + a record), under 1 s at a pessimistic 5 MB/s. **Total ≈ 2–3.5 s, well under the ~10 s line.**
- **Decision: no progress indicator per generator stage.** The Generating screen is one static screen, drawn once before the blocking call; no stage callback is threaded through the runner, and no test checks progress.
- **Known limit, recorded for Phase 4 step 3**: the estimate covers seeds that win at attempt 0, which every measured seed did. A seed needing many attempts (the watchdog allows 1000) would be slower in proportion; a per-stage indicator would not help there. Phase 4 step 3 confirms the factor on the device's real numbers (or reopens the decision if they moved).
- **For the step note**: the implementer re-runs `make -C tests/rando measure` and the throwaway pipeline timing (a driver in `tmp/`, never committed) at the end of the step and hands the numbers to `plan-pilot`. Found while measuring: at HEAD, `make measure` **fails on a host where clang is installed** — the arm-clang shim's `string.h` lacks `strlen`, `memcmp`, `strcpy`, `strcat` and `strchr`, and clang writes no `.su` for a source with no function (`hint_names.c`), which `measure.sh` treats as an error. The numbers above were taken with the arm-clang build skipped in the throwaway copy. This is a bug in `measure`, outside this step's code: if it blocks the re-run, it goes through `fix-bug` as its own commit before the numbers are taken.

### 2. Status counts and the CLI's BPS names

- `RandoBpsResult`, `RandoAssembleStatus` and `RandoSaveStatus` each get a last enumerator `RANDO_BPS_RESULT_COUNT`, `RANDO_ASSEMBLE_STATUS_COUNT`, `RANDO_SAVE_STATUS_COUNT`. No function ever returns a count; existing values keep their numbers.
- `patch_cli.c`'s `rando_assemble` switch gains the count's case, falling to the existing final `return EXIT_INPUT` (an assertion is not needed: `-Wswitch` is the reason).
- The CLI's BPS names become a `const char *` array of `RANDO_BPS_RESULT_COUNT` entries with designated initializers, read through a new public `rando_patch_bps_name(RandoBpsResult)` in the CLI's header; out of range still gives `"unknown"`. The names themselves (`OK`, `NOT_BPS`, … `TARGET_CRC`) and every CLI message are unchanged.

### 3. Shared host file helpers

- `patch_cli.c`'s capped reader and seed draw move into the host output module (`rando_out`), public, so the CLI and the app's runner share them:
  - `rando_out_read_capped(path, buf, max, *size)` → `RandoOutRead`: `OK` (`*size` ≤ max), `MISSING` (open failed with `ENOENT`), `OPEN` (open failed otherwise), `READ` (a read error), `TOO_LARGE` (`*size` = max + 1; the true size, when it can be told, in a second output, −1 otherwise). It prints nothing; the caller formats its own message.
  - `rando_out_random_seed()` → `uint32_t`: `/dev/urandom`, falling back to today's time mix, unchanged.
  - The 4 MiB patch cap becomes one public constant there (`RANDO_OUT_PATCH_MAX`), used by the CLI and the runner.
- The CLI's messages, exit codes and behaviour stay byte for byte as they are: `test_patch_cli` passes with its existing cases untouched.

### 4. The generation flow (UI core)

- **Interface types** in a new core header (names indicative):
  - `UiGenRequest`: options (hints included), heart beep, spoiler flag.
  - `UiGenOutcome`: `ok`; the ROM file name (a buffer of `RANDO_NAME_ROM_MAX`, filled on success); `spoiler` (echoed); `list_stale` (success, but a SNES list cache could not be removed); and on failure `status` (a `UiStrId`, the error line), `status_arg` (a `UiStrId` filled into a one-placeholder status, `UI_STR_NONE` otherwise) and `advice` (a `UiStrId`, the what-to-do line). The outcome holds ids and the name only: no path, no text.
  - `UiGenRunFn(void *ctx, const UiGenRequest *, UiGenOutcome *)`: the injected generator. Synchronous and blocking; it must always fill the outcome.
- **State** (`UiGenFlow`, caller-owned, no allocation): phase (`IDLE`, `GENERATING`, `DONE`), the request, the outcome, the string table pointer, the run function and its context, and the buffer of the filled status text (the screen borrows it).
- **Functions**:
  - `ui_genflow_init(flow, table, run, ctx)`: phase `IDLE`.
  - `ui_genflow_start(flow, const UiGenRequest *)`: phase `GENERATING`. The shell calls it on `UI_GENERATE_GENERATE` with `ui_generate_settings`' values (H-3.4-3).
  - `ui_genflow_pending(flow)`: true in `GENERATING` before the run. **Contract for the shell** (step 7, Phase 4): after `start`, render and present `ui_genflow_screen` once, *then* call `ui_genflow_run`; the run blocks, so the Generating screen is what the player sees meanwhile.
  - `ui_genflow_run(flow)`: clears the outcome, calls the run function once, phase `DONE`. Asserts `GENERATING`.
  - `ui_genflow_key(flow, UiButton)` → `UI_GENFLOW_NONE`, `_BACK`, `_QUIT`: in `DONE`, A and B give `BACK` (phase `IDLE`; the shell shows the Generate tab again, its state untouched), Menu gives `QUIT`; every other button, and every button in `GENERATING`, gives `NONE` and changes nothing.
  - `ui_genflow_screen(flow, UiScreen *)`: asserts a phase other than `IDLE`.
- **Screens** (tabs `Generate` / `History`, Generate active, as the Generate tab draws them; rows are text rows; cursor −1):

  | Phase | Title | Rows | Message | Footer |
  |---|---|---|---|---|
  | Generating | `Generating Seed` | `Please wait...`, `This takes a few seconds` | none | none |
  | Success | `Seed Ready` | `Saved in Roms/SFC/ALttPR as:`, the ROM name, `Spoiler kept in History` or `Spoiler not kept` | info: `Play it from the SNES list`, or `Refresh the SNES list to see it` when `list_stale` | `A Back` |
  | Failure | `Generation Failed` | the advice line | error: the status text, `{}` filled with `status_arg`'s text when the status has arity 1 | `A Back` |

  The ellipsis is three ASCII dots (step 3's printable-ASCII rule). The ROM name (at most 36 characters, `docs/onion.md` §10) fits the 38-character field.
- **Rule compliance**: pure C99, no stdio, no allocation, no SDL; the core includes only library headers and itself (`rando_options.h`, `rando_name.h`, the settings header of H-3.4-1); no letter in any literal; every text above is a new id in the X-macro list with its English entry.

### 5. The runner (SD layer)

- **New SD path constants**, beside `RANDO_SD_ROM_DIR` and `RANDO_SD_HISTORY_DIR`, relative to the SD root: `App/ALttPR/base.sfc`, `App/ALttPR/base.bps`, and the two list caches `Roms/SFC/SFC_cache6.db` and `Roms/SFC/SFC_cache2.db`. The settings file's constant is step 4's (H-3.4-4).
- **SD root (C2)**: `rando_app_sd_root(app_dir, out, cap)` — pure string work: `app_dir` minus its last two path components (trailing slashes ignored), so `/mnt/SDCARD/App/ALttPR` gives `/mnt/SDCARD` and a relative `App/ALttPR` gives `.`; when `app_dir` is NULL, empty or has fewer than two components, `/mnt/SDCARD`. False when `out` is too small. Finding `app_dir` (the binary's folder, or `ui_cli`'s flag) is the caller's: Phase 4 main and step 7.
- **Configuration** (`RandoAppGen`, caller-owned, long-lived): the SD root; the joined paths; the expected source and base digests; the seed function, date function and save function (pointers); a work area holding the 1 MiB + 1 source buffer, the `RANDO_OUT_PATCH_MAX` + 1 patch buffer and a `RandoAssembleWork` (≈ 7 MiB, so it is a static object in production, as in `patch_cli.c`).
- `rando_app_gen_init(gen, sd_root)`: joins the paths (false when any would exceed `RANDO_SEED_DIR_MAX` / `RANDO_SEED_PATH_MAX`), sets §2's digests from `rando_assemble_jp10_md5` and `rando_assemble_base_md5`, the seed function to `rando_out_random_seed`, the date function to the local time (`time` + `localtime`, C99), the save function to `rando_seed_save`. Tests replace any of them after init.
- `rando_app_gen_run` is a `UiGenRunFn` (ctx = `RandoAppGen`). In order, each step only when the earlier ones succeeded:
  1. Save the last settings with `rando_last_settings_save` (H-3.4-4). Its failure is ignored and not shown: an SD card that refuses this file will refuse the ROM too, and that failure is the one reported.
  2. Read `base.sfc` capped at `RANDO_ROM_SOURCE_SIZE` (+1): missing → `BASE_MISSING`; open or read error → `BASE_READ`. A file of another size is passed on, so `rando_assemble` reports `SOURCE_SIZE`.
  3. Read `base.bps` capped at `RANDO_OUT_PATCH_MAX`: missing → `PATCH_MISSING`; open or read error → `PATCH_READ`; larger → `PATCH_TOO_LARGE`.
  4. Create `Roms`, `Roms/SFC`, `Roms/SFC/ALttPR`, `App`, `App/ALttPR` and `App/ALttPR/history` under the root, each when missing (POSIX `mkdir`, an existing directory accepted; the SD source defines `_POSIX_C_SOURCE` itself) → `FOLDERS` on any other failure (C3).
  5. Build the request with `rando_assemble_request_init`, then the configured digests, the buffers, the options and heart beep, a seed from the seed function; call the save function with the ROM and history directories, the work area, the date, the spoiler flag.
  6. On `RANDO_SAVE_OK`: remove both list caches; a cache that is absent is fine; one still present after `remove` sets `list_stale` (C4). On any other status nothing is removed.
- Its own status enum `RandoAppGenStatus` (`OK`, `BASE_MISSING`, `BASE_READ`, `PATCH_MISSING`, `PATCH_READ`, `PATCH_TOO_LARGE`, `FOLDERS`, `SAVE`, count) records which step stopped; `SAVE` means the save function's status decides.

### 6. Status texts: four enum-sized tables

- In the SD layer — the only layer that sees all three enums plus the runner's own (the core cannot include `app/sd`, and `RandoSaveStatus` lives there) — four `const` tables, each sized by its enum's count, entries by designated initializer, each entry a status id and an advice id: `RandoAppGenStatus`, `RandoSaveStatus`, `RandoAssembleStatus`, and `RandoBpsResult` (status id only: it is the `{}` of the patch-refused text). Public lookups return the entry; out of range asserts. Delegation: `SAVE` takes the save status's entry; `RANDO_SAVE_ASSEMBLE` takes the assemble status's; `RANDO_ASSEMBLE_PATCH` sets `status_arg` from the BPS table. The `OK` and delegating entries still have their own ids (every value has one; the test does not special-case them).
- **English texts** (ids added in the X-macro list; the agent may change a text only to satisfy a table rule, stating it). Advice lines: `Needs App/ALttPR/base.sfc (JP 1.0)`, `Reinstall App/ALttPR from the zip`, `Check that the SD card is writable`, `Press A and generate again`, `Change the settings and try again`, and for the `OK` entries `Nothing to fix`.

  | Enum value | Status text | Advice |
  |---|---|---|
  | app `OK` | `Seed generated` | nothing to fix |
  | app `BASE_MISSING` | `Base ROM not found` | base |
  | app `BASE_READ` | `Base ROM could not be read` | base |
  | app `PATCH_MISSING` | `Base patch not found` | reinstall |
  | app `PATCH_READ` | `Base patch could not be read` | reinstall |
  | app `PATCH_TOO_LARGE` | `Base patch is too large` | reinstall |
  | app `FOLDERS` | `Could not create the ROM folders` | SD card |
  | app `SAVE` | `Seed could not be saved` | SD card |
  | save `OK` | `Seed saved` | nothing to fix |
  | save `NO_NAME` | `No free file name for this seed` | generate again |
  | save `ASSEMBLE` | `ROM could not be built` | generate again |
  | save `RECORD` | `History record write failed` | SD card |
  | assemble `OK` | `ROM built` | nothing to fix |
  | assemble `SOURCE_SIZE` | `Base ROM has the wrong size` | base |
  | assemble `SOURCE_MD5` | `Base ROM is not the JP 1.0 ROM` | base |
  | assemble `PATCH` | `Patch refused: {}` | reinstall |
  | assemble `PATCH_SIZES` | `Base patch is for another ROM` | reinstall |
  | assemble `BASE_MD5` | `Base patch gave the wrong ROM` | reinstall |
  | assemble `UNSUPPORTED` | `These settings are not supported` | change settings |
  | assemble `GAVE_UP` | `Generator gave up on this seed` | generate again |
  | assemble `OUTPUT` | `SD card write failed` | SD card |
  | BPS `OK` … `TARGET_CRC` | `no error`, `not a BPS file`, `malformed`, `bad patch checksum`, `wrong source size`, `wrong source checksum`, `target too small`, `wrong target checksum` | — |

  `Patch refused: ` is 15 characters and the longest BPS text 21, so every filled line fits 38; a test checks it rather than trusting this sum.

## Implementation Chunks

1. **Status counts and the CLI's BPS names.** The three `_COUNT` enumerators, the switch case, the enum-sized BPS name table and `rando_patch_bps_name`. New `test_patch_cli` case: every value has a non-empty name, the names are distinct and equal today's spellings; out of range gives `unknown`. Done when `test_patch_cli`'s existing cases, `test_bps`, `test_assemble`, `test_seed_save` pass unchanged and `make check` is green.
2. **Shared host file helpers.** The capped reader, the seed draw and the patch cap moved into `rando_out`, the CLI using them. New `test_out` (no host-output test exists at HEAD; registered in `UNIT_TESTS` and the `LOGIC_TESTS` filter-out list): `OK` at exactly the cap, `TOO_LARGE` at cap + 1 with the true size, `MISSING`, a directory giving `READ` (glibc opens a directory for reading and the read fails with `EISDIR`; `OPEN` is reached only by permissions, which a root test run ignores, so it is not tested), an empty file; two seed draws in a row differ (with `/dev/urandom` present). Done when `test_patch_cli` passes unchanged and `make check` is green.
3. **The flow state machine (core).** Request, outcome, run function, `UiGenFlow`, its ids and English texts, `test_ui_genflow` with a fake run function: phases and events, the run called exactly once and only after `start`, keys ignored while generating, A/B/Menu in `DONE`, the outcome echoed; snapshots of the Generating screen, success (spoiler kept), success (spoiler not kept, `list_stale`), and a failure with a filled `{}` status. Done when they pass, `no_literals.py` and `no_sdl.sh` pass, `make lint` builds it, and `make check` is green.
4. **The runner and the status tables (SD layer).** Path constants, `rando_app_sd_root`, `RandoAppGen`, init, run, the four tables, `test_app_gen` on a temporary SD tree. Done when it passes and `make check` is green.
5. **End to end and the every-value check.** `test_ui_genflow` (or a second test binary if link order needs it) driving the Generate tab from its defaults: Start → `ui_genflow_start` → screen → `ui_genflow_run` through the real runner on a temporary SD tree with the synthetic base → result screen; the snapshots listed under Testing Decisions; the every-value rendering check; `make unit-arm` with the new tests (counts for the step note); the timing re-run of decision 1 (numbers for the step note). Done when they pass and `make check` is green.

## Testing Decisions

- **What a good test is here**: it goes through public functions only — `rando_patch_bps_name`, the `rando_out` helpers, `ui_genflow_*` rendered by `ui_grid_render` and `ui_grid_text` at 40 × 15, `rando_app_sd_root`, `rando_app_gen_init`/`run`, the table lookups — and checks results and files on disk. Expected screens are hand-written snapshots in the snapshot directory, never generated from the code's output, with no update mode (step 3.2's practice). Expected texts come from the tables above, not from the code.
- **`test_ui_genflow`** (core, fake run function counting its calls and recording the request):
  - `start` does not call the run function; `pending` is true until `run`; `run` calls it once with the request given to `start`; a second `run` asserts (not tested by death, documented).
  - In `GENERATING`, every button gives `NONE` and leaves the screen identical; in `DONE`, A and B give `BACK` and phase `IDLE`, Menu `QUIT`, every other button `NONE`.
  - Screens: tabs, Generate active, cursor −1, the texts of decision 4's table; a failure with arity-1 status and `status_arg` shows the filled text; a failure with an arity-0 status ignores `status_arg`.
- **`test_app_gen`** (SD layer; temporary directory under `/tmp` via `mkdtemp`, as `test_seed_save`; synthetic base from the unit tests' synthetic base helper, its digests injected; fixed seed and date injected):
  - `rando_app_sd_root`: `/mnt/SDCARD/App/ALttPR` → `/mnt/SDCARD`, the same with a trailing slash, `App/ALttPR` → `.`, `/x/y/App/ALttPR` → `/x/y`, NULL, empty and `ALttPR` → `/mnt/SDCARD`, a too-small buffer → false.
  - `rando_app_gen_init` sets `rando_assemble_jp10_md5` and `rando_assemble_base_md5` (step 2.2's "the app passes §2's digests", byte for byte against the written-out `03a63945…` and `edc01f3d…`), and refuses a root too long for the paths.
  - **Success from an empty SD root** holding only `App/ALttPR/base.sfc` and `base.bps`: the outcome is `ok` with the name `rando_name_make` gives for the seed and settings; the ROM exists in `Roms/SFC/ALttPR/` with 2 MiB, the record in `App/ALttPR/history/`, `settings.json` holds the request's settings (read back with step 4's loader), the folders were created (C3).
  - **Caches (C4)**: both cache files present before a success → both gone after, `list_stale` false; absent → fine; a cache path that is a non-empty directory → `list_stale` true, the outcome still `ok`; a failure removes no cache.
  - **Each runner failure, real**: no `base.sfc` → `BASE_MISSING`; `base.sfc` a directory → `BASE_READ`; no `base.bps` → `PATCH_MISSING`; `base.bps` larger than the cap → `PATCH_TOO_LARGE`; `Roms` a regular file → `FOLDERS`; a base of 1 MiB + 512 → assemble `SOURCE_SIZE`; the synthetic source with §2's real digests → `SOURCE_MD5`; a corrupted patch byte → `PATCH` with the BPS code's id as `status_arg`. Each checks the outcome's ids and that no ROM or record exists afterwards, while `settings.json` does.
  - **Statuses a real run cannot reach** (`NO_NAME`, `RECORD`, `OUTPUT`, `GAVE_UP`, `UNSUPPORTED`, `BASE_MD5`, `PATCH_SIZES`): an injected save function returning each, its result's assemble status and BPS code set; the outcome's ids follow the delegation rule.
  - **The tables**: for each of the four enums, every value from 0 to count − 1 has a status id that is not `UI_STR_NONE` (and, except the BPS table, an advice id that is not); status ids are distinct within each table; an id of arity 1 appears only at assemble `PATCH`; for every BPS value, `Patch refused: {}` filled with its text is at most 38 characters.
- **Snapshots**, each a key sequence or outcome at 40 × 15: Generating; success with the spoiler kept; success, spoiler not kept and list stale; and one per failure the plan names — base missing, base wrong MD5, generator gave up, SD card write failed — plus folders not creatable, the history record failing and the patch refused with a filled BPS code. Seven failure snapshots, three for the rest.
- **Every value rendered** (no snapshot): for each value of each of the four tables, a `DONE` failure screen built from the lookup is rendered; its error line holds the full English status text (filled where arity 1) and its advice row the full advice, with no `~`. This is the "for each failure" coverage at the text level, the way step 4 covers every Generate value.
- **End to end**: from the Generate tab at its defaults (H-3.4-3), the filter-free buttons Start then the shell sequence (`start`, screen, `run`, screen), then A: the Generate tab's screen equals its screen before Start.
- **`test_patch_cli`**: the new BPS-name case only; every existing case untouched (it proves chunk 2 too).
- **`make lint`** builds the new core and SD sources with `-pedantic-errors`; **`make unit-arm`** runs the new tests under qemu-arm (the `readdir` quirk is already handled by `-D_FILE_OFFSET_BITS=64`); `no_literals.py` covers the flow; `no_sdl.sh` covers both directories.
- **Prior art**: `test_ui_grid` (hand-written snapshots), `test_seed_save` (temporary SD tree, files left after each failure), `test_assemble` (synthetic base, injected digests), `test_patch_cli` (unchanged messages as the refactor's proof), `test_ui_strings` (the table check).

## Out of Scope

- The app shell that routes keys between the Generate tab, this flow and History, draws frames and quits: step 7 (`ui_cli`) and Phase 4's main. This step fixes the contract (render once, then run).
- Loading the last settings at start, and finding the app's folder (`/proc/self/exe` or `argv[0]`): Phase 4's main and step 7.
- Shipping `App/ALttPR/history/` in the zip, `launch.sh`'s `cd`: Phase 4 step 2 (the runner creates the folder anyway).
- History's own cache removal after a delete (C4's other half), and C5–C8: step 6.
- A progress indicator, a cancel button, a time limit lower than the watchdog's 1000 attempts.
- Fixing `make measure`'s arm-clang shim: a separate `fix-bug`, only if it blocks the timing re-run.
- The other warnings of `notes/phase2-warn-review.md` (JSON parsers, little-endian readers, test constants, BPS writers in tests).
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): decision 1's numbers and decision (no per-stage indicator), with the re-run's numbers; the `make measure` clang failure and how it was handled; the outcome of H-3.4-1 … H-3.4-6; `make unit-arm` counts; that `test_patch_cli`'s existing cases were untouched; the warn-review lines this step closes (the SD path constants are now used; the patch cap is in one place; the capped reader is shared).
- **For step 6**: the History delete removes the same two caches (the constants are here), and can reuse `rando_app_sd_root` and the joined paths of `RandoAppGen`.
- **For step 7 and Phase 4**: create one static `RandoAppGen`, `rando_app_gen_init` with `rando_app_sd_root(<app folder>)`, pass `rando_app_gen_run` and it to `ui_genflow_init`; on `UI_GENERATE_GENERATE`, `ui_genflow_start` with the tab's settings, present the screen, then `ui_genflow_run`; on `BACK`, show the Generate tab again.
- **For Phase 4 step 3**: confirm the 20× factor on the device, and add a sweep of the slowest configuration's seeds, since the estimate holds only for attempt-0 seeds.
- **Decisions taken without a human** (to-prd ran unattended): no progress indicator (numbers above); the flow lives inside the Generate tab, not as a third tab; A and B both go back, Menu quits; the last settings are saved before anything else and their failure is silent; a missing cache is not an error, a cache that will not go makes a "refresh" hint, not a failure; the status tables live in the SD layer because the core cannot see `RandoSaveStatus`; each value has its own status id and shares one of six advice ids; the base patch is read from `App/ALttPR/base.bps`, not embedded; the seed draw and the capped reader move from the CLI to `rando_out` rather than being copied.
