# Plan 1 — On-device ALttPR ROM generator for the Miyoo Mini Flip (OnionOS)

## 1. Goal

A user plays *A Link to the Past* randomizer seeds on a **Miyoo Mini Flip running OnionOS**, generated **on the console itself**, with no PC and no network:

- An Onion app (`App/ALttPR/`) reads a Japanese 1.0 ROM, generates a seed with every alttpr.com generation setting, and writes a patched `.sfc` that the user then launches from Onion's SNES list with Onion's own SNES core.
- One seed is one ROM file, so it gets its own save: many seeds run in parallel, independently. This was the user's first need ("plusieurs sav avec des seeds différentes").
- A **History** tab lists the generated seeds, shows each one's settings and spoiler, and deletes a seed's ROM, save and save states.

The goals behind every rule below:

1. **Play, not race.** Matching alttpr.com is not a feature the user asked for. It matters here only because it is the proof that the ROM is right: an agent cannot play the game, so the produced ROM is checked against VT itself.
2. **Controller-first.** The user rejected an existing project (Slashmolder/zelda3randomizer) because its randomizer UI is a PC-style ImGui panel. Every screen is driven by the d-pad and buttons, in the style of Onion's menus.
3. **Verifiable without the console and without the ROM.** No Nintendo ROM is ever in the repository or in CI; the agent sees the UI through a text renderer.

## 2. Rules

### Locked decisions

- **Approach: a ROM patcher, not a native port.** The output is a patched SNES ROM played in an emulator. Rejected:
  - *alttp-dx* (YeOldPress, plays alttpr.com seeds in an embedded SNES emulator): one ROM at a time, not one seed per save slot, and PC only.
  - *Slashmolder/zelda3randomizer* (a snesrev/zelda3 fork with an in-binary generator): PC-style ImGui UI (goal 2), no Miyoo port, and the user had no sound in it.
  - *A native zelda3 port for the Miyoo*: no such port exists (it is not in PortMaster either; checked against `PortsMaster/PortMaster-New`, 1,481 ports, Oct 2026). The randomizer's ASM patches would also have to be reimplemented in C.
  - *Generating on alttpr.com and copying to the SD card*: the user wants on-device generation.
- **The emulator side is not a risk.** The user checked that an alttpr.com ROM boots, has sound and keeps its save with Onion's default SNES core.
- **Base ROM: Zelda no Densetsu: Kamigami no Triforce, Japanese v1.0**, MD5 `03a63945398191337e896e5771f77173`. VT requires it (`resources/lang/en/start.php`); the patched game is in English. A US ROM is not supported.
- **VT is frozen** at `sporchia/alttp_vt_randomizer` `219fcafd029dab597b8db400efafd8f56f8b4edb`, the commit the oracle pins (`tests/rando/oracle/gen_golden.sh`), with no tracking of later alttpr.com updates. That commit's ROM build is `Rom::BUILD = '2024-02-18'`, `Rom::HASH = 'edc01f3db798ae4dfe21101311598d44'` (MD5 of the JP ROM, resized to 2 MiB, with the base patch applied).
- **The base patch** (z3randomizer's compiled ASM and English text, applied before any seed write) **is committed to the repository and shipped in the app.** The user accepted that it may contain data derived from the game. Its source:
  - alttpr.com serves the current build's patch as a BPS at `/bps/<hash>.bps` (`routes/web.php`, `resources/js/components/VTRomLoader.vue`); as of Oct 2026 the `edc01f3d…` one returns 404 (Phase 0).
  - Otherwise it can be built with asar from `KatDevsGames/z3randomizer` `dcb0a2b42d14445f7994a0e9e4d63cbecf4b98d3` (v31.2.2, VT's `composer.json`) on the resized JP ROM: `LTTP_RND_GeneralBugfixes.asm`, `--fix-checksum=off` (`app/Console/Commands/UpdateBuildRecord.php`).
  - Either way, applying it to the JP ROM must give MD5 `edc01f3d…`.
- **Generation settings: all of alttpr.com's**, the eleven of `rando/include/rando_options.h` (glitches, item_placement, dungeon_items, accessibility, goal, crystals_tower, crystals_ganon, state, weapons, item_pool, item_functionality), plus the hints on/off toggle (`spoil.Hints`).
- **Cosmetic options: only the heart beep** (`Rom::setHeartBeepSpeed`: off, half, quarter, double, normal; one byte at `0x180033`). Rejected by the user: Link sprites (`.zspr`), heart color, menu speed, music mute, quickswap. Every other cosmetic stays at VT's default.
- **The seed is always random.** There is no seed entry and no sharing (the user does not need them). The app remembers the last settings used.
- **Files on the SD card:**
  - The base ROM is placed once at `App/ALttPR/base.sfc`; the app checks its MD5 at start and shows a clear message if it is wrong.
  - Generated ROMs go to `Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc` (e.g. `Open-FastGanon-4F2A9C.sfc`), a subfolder of Onion's SNES list.
  - History records go to `App/ALttPR/history/<code>.json`: settings, date, and the spoiler when the spoiler option was on.
- **Spoiler:** an option of the Generate screen, **off by default**. It decides only whether the spoiler is kept in the history record; it never changes the ROM and is never written next to the ROM. A seed generated without it still appears in History, with its settings but no item locations.
- **History tab:**
  - It lists the generated seeds and shows each one's settings and spoiler.
  - Deleting a seed, after a confirmation, removes the ROM, its RetroArch `.srm` save and its RetroArch save states.
  - A ROM deleted by hand with Onion's file manager simply disappears from History at the next start.
- **UI:**
  - Two tabs, **Generate** (every setting, then Generate) and **History**, in the style of Onion's menus: a clean font, lists, a cursor, no game graphics.
  - English text, every string looked up through a translation table keyed by id; only English ships.
- **UI architecture (the agent's eyes):**
  - The UI is a pure C state machine, with no SDL: a key in, a screen description out (tab, rows, cursor, values, message). Two renderers draw that description: SDL (Miyoo and PC) and text (a character grid).
  - Tests play a key sequence and compare the text screen against a snapshot.
  - A host command-line tool reads a key sequence and prints the text screen, so the agent can "look" at the app while working.
- **The app also builds for PC** (Linux and macOS, SDL, keyboard instead of the d-pad), so the user tests the SDL rendering before copying to the SD card. Only the user validates the SDL rendering and the device.
- **ROM goldens are write logs, not ROMs.** The oracle records every `Rom::write` (address and bytes) that a case makes after the base patch. The C port must reproduce that log byte for byte. Neither the oracle nor CI needs a Nintendo ROM:
  - VT's `Rom` constructor accepts no source file.
  - `writeToRom` does not read the ROM back: the only `readByte` calls are in `app/Sprite.php`, for enemizer.
  - The checksum (`Rom::updateChecksum`), which does depend on the ROM, is tested by its algorithm, not by a golden.

### Inherited guardrails (`CLAUDE.md` of `Canadadry/alttp-ds`, carried into this repository's `CLAUDE.md`)

- The rando goldens are the source of truth. Never regenerate a golden to get green: `gen_golden.sh` runs only when VT, its patch or the case list changes on purpose, in its own commit.
- Never make `make -C tests/rando test` or `make -C tests/rando e2e` slower; a slow check goes in a separate target.
- The randomizer library stays pure C99: no `malloc`, no platform library, built by host gcc and by any cross compiler.
- Never launch the game or an emulator. Never merge a pull request. Never work on or push a `claude/*` branch.
- Everything written in the repository is in English. Comments follow `CLAUDE.md`'s comment rule.

### Out of scope

- Entrance shuffle, Enemizer (boss, enemy, damage, health, pot shuffle), door shuffle: not part of VT's generator.
- Custom/customizer seeds, multiworld, tournament/race mode, seed entry or sharing, matching an alttpr.com seed.
- Any firmware other than OnionOS; PortMaster packaging.
- Translations other than English (the mechanism is in scope, the tables are not).
- What becomes of `alttp-ds` and its DS port: unrelated to this plan.

## 3. Phases and steps

### Phase 0 — The one input the agent cannot make

1. **Obtain the base patch for build `edc01f3d…`.** As of Oct 2026, `https://alttpr.com/bps/edc01f3db798ae4dfe21101311598d44.bps` returns 404, and `https://alttpr.com/base_rom/settings` points to a later build (`79bbb4ad…`), which is wrong for the frozen VT. Try archived copies of the `edc01f3d…` BPS (the Wayback Machine, community mirrors). Otherwise, building it needs the JP ROM, which the agent never has. Two allowed outcomes:
   - (a) The BPS is committed as `data/base.bps`, with its source URL and VT commit in the commit message. Its header checks pass: target size 2,097,152, and source CRC32 equal to the JP 1.0 ROM's CRC32.
   - (b) The step note asks the user to produce it, with the exact commands: resize the JP ROM to 2 MiB, run asar `--fix-checksum=off LTTP_RND_GeneralBugfixes.asm` from z3randomizer `dcb0a2b4`, create a BPS from the JP ROM to the result, and check that the result's MD5 is `edc01f3d…`.

   In either case, the step note asks the user to confirm that the patch applied to their JP ROM gives MD5 `edc01f3d…`. The user does this in parallel with Phase 1, which does not need the patch.

### Phase 1 — The ROM writer matches VT, host only

1. **Take over the randomizer from `Canadadry/alttp-ds`** (its default branch; the commit in the commit message). Copy `rando/` (`include/`, `src/`, `host/`) and `tests/rando/` (unit tests and `support.*`, `cases/`, `oracle/`, the scripts, `claims.txt`, `stages.txt`) in place of this repository's bootstrap skeleton (`rando_version.*`, `test_version.c`), and the skills `check-e2e` and `rework-comments` into `.claude/skills/`. Nothing else: not `platform/`, `vendor/`, `build.py` nor `tools/` (this repository's own hook replaces `tools/branch_guard.sh`). Keep `tests/rando/Makefile`'s `lint` target, and add `logic`, `selftest` and `e2e` to the root `Makefile`'s `check` (and to `CLAUDE.md` § Checks), with `test` unchanged. Adjust what still names the DS port (`CLAUDE.md`, skill texts) to this repository. Verified by:
   - `make test` and `make check` green;
   - `make -C tests/rando e2e` printing no `DIVERGENT` line, and its per-case time measured with `make -C tests/rando measure`, numbers in the step note. This commit is the reference for the ±10% budgets below: each budget check re-measures it in the same session, from a throwaway worktree of it under `tmp/`, next to the current tree, and records both sets of numbers (times from different machines are not comparable).
   - the unchanged oracle reproduces the taken-over goldens: `gen_golden.sh --out tmp/<dir>` on every case gives files identical to the committed goldens and fixtures, with the PHP, composer and VT versions in the step note; a difference is a setup problem, resolved (or asked to the user) before step 2;
   - the taken-over generator, cross-built with the Onion toolchain (Phase 3 step 1's toolchain item, done here from Onion's sources and cited the same way; Phase 3 step 1 reuses it), run under qemu-arm user mode on the e2e cases, matching every claimed golden. Its target (outside `test` and `e2e`) compares write logs too from step 5, when the write-log claims start; step 12 and the end of Phase 2 run it again, the latter with Phase 2's host tests under qemu-arm too, and Phase 4 step 2 reuses it.
2. **The oracle runs the post-fill code and records the write log.**
   - `tests/rando/oracle/vt.patch` turns `setTexts`, `randomizeCredits` and `HintService::applyHints` back on (today it comments them out). It labels their PRNG draws, and the draws `writeToRom` itself makes (`setDiggingGameRng`, `writeRNGBlock`, the `fy_shuffle` calls in `Location::getHint` and `Location/Prize.php`), as new stages in `stages.txt`.
   - `oracle_case.php` runs `World::writeToRom` on a sourceless `Rom`, with the case's heart beep. It records every write as `cases/<case>/rom_writes.txt`: one `<hex address> <hex bytes> <Rom method>` line per write, in call order, tagged by the backtrace the same way `prng_stage` tags draws. `updateChecksum` is excluded.
   - Texts, credits and initial SRAM reach the ROM as a few large writes, so the oracle also logs, per case, the setter calls that fill those buffers (key and bytes, in call order), and writes one seed-independent fixture: the encoded default text table.
   - The case format gains a hints key and a heart-beep key; their defaults are VT's `on` and `half`.
   - Regenerate the goldens in their own commit.
   - The step note states the oracle's `writeToRom` `$save` value, and lists every ROM write between VT's generation entry point and the downloaded file that the log does not capture (to start: `setSeedString` and `setStartScreenHash` under `$save`, `RandomizerController::prepSeed`'s tournament writes, `resources/js/rom.js` `save()`'s quickswap, music volume, MSU-1 resume and reduce-flashing writes). Each gets one outcome: added to the log; reproduced in C outside the log with a unit test checked against VT's source (step 11); or left out, with its reason.

   Verified by:
   - `gen_golden.sh` is reproducible (two runs give identical files);
   - `make -C tests/rando e2e` stays green, with the existing claims held at their current stage (`fill`) instead of `all`.
   - a `fill` claim still compares what the fill produces (the spoiler's location sections and the attempts, as checks `stages.txt` assigns to `fill`), proven by a `selftest` case where a synthetic placement divergence under a `fill` claim prints `DIVERGENT`; step 3's "green" includes these checks.
3. **Golden cases for the new settings and for combinations.**
   - Add cases for hints off and each heart-beep value. Add enough multi-setting combinations to cover what the UI allows at once: at least every state × every glitches value, dungeon_items full with every state, and triforce-hunt and ganonhunt with every state.
   - Only the single-setting cases join `e2e`. The combination cases go into a separate target, `make -C tests/rando e2e-combos`, added to the root `make check` next to `e2e` (`test` and `e2e` unchanged), so every later step's net guards the combinations.
   - List every combination where the C generator returns `RANDO_UNSUPPORTED`, and either port it or record it as a UI restriction (Phase 3 greys it out).
   - Commit the case list and goldens on their own.

   Verified by:
   - the goldens exist, and every new case is green or listed as unsupported; a combination that is `DIVERGENT` at its claimed stage is a bug, fixed in this step before step 4;
   - `e2e`'s time grows only by its new single-setting cases (budget: per-case time unchanged ±10%, measured with `make -C tests/rando measure` against step 1's commit as above).
4. **Port the post-fill draws** (texts, credits, hints, `writeToRom`'s own draws) into the C generator, one stage of step 2 at a time in VT's draw order. Verified by every case's claim reaching each stage in turn, then moving back to `all` in `claims.txt`, with `e2e` and `e2e-combos` green.
5. **`rando_rom` skeleton and claims by write group.** The `rando_rom` module writes through a caller-supplied write callback (the host logs writes, the app writes into a buffer), with no allocation. `claims.txt` gains write-log claims by setter group, the way it claims trace stages: a case claims the log prefix up to a group. The groups follow `writeToRom`'s call order, where texts, credits and SRAM are only buffered until `writeCredits`, `writeText` and `writeInitialSram` at the end: items and locations, settings, credits, text, initial SRAM, then the trailing `setTotalItemCount`. `e2e` prints one `DIVERGENT <case>` line per case whose claimed prefix differs. Verified by the `selftest` target covering a synthetic divergence in a claimed group, and a pass on an unclaimed one. The same step updates `skill: check-e2e` for the write logs and `e2e-combos`: it ranks a write-log failure by the earliest differing write group, and gives the first differing `rom_writes.txt` line (address, bytes, Rom method) as the bug's reproduction; verified by running it on that synthetic divergence.
6. **Port the items and locations writes.** Verified by every case's claim reaching that group, plus `tests/rando/unit/test_rom_items.c`.
7. **Port the settings and flags writes** (the `Rom` setters `writeToRom` calls). Verified by every case's claim reaching that group, plus `test_rom_settings.c`.
8. **Port the credits writes** (`Credits`). Verified by every case's claim reaching that group, plus `test_rom_credits.c`.
9. **Port the text, dialog and hints writes** (`Text`, `Dialog`), in two halves: first the encoder and default table, verified by `test_rom_text.c` against the default-table fixture; then the seed-dependent strings, verified by the logged setter calls, then by every case's claim reaching that group.
10. **Port the initial SRAM writes** (`InitialSram`), so the full log matches. Verified by every case claiming the whole write log, plus `test_rom_sram.c`.
11. **Checksum and heart beep.** Port `Rom::updateChecksum` and `setHeartBeepSpeed`. Verified by a unit test of the checksum algorithm on a synthetic 2 MiB buffer whose expected sum is computed independently, and by the heart-beep byte matching the step 3 cases. Every write step 2's note marks "reproduced in C" has its unit test here.
12. skill: check-e2e — the whole case list, write logs included; then `e2e-combos`.

### Phase 2 — Full ROM assembly, host only

1. **BPS applier in C** (no allocation, caller buffers, source and target CRC32 checks). Verified by unit tests on small hand-built BPS files covering every BPS action and each CRC failure.
2. **ROM assembly pipeline**: check the source MD5 (`03a63945…`), resize to 2 MiB, apply the base patch, check MD5 `edc01f3d…`, generate, run `rando_rom`, apply the heart beep, update the checksum, and write the `.sfc`. A host CLI `rando_patch --base <jp.sfc> --out <file> [settings]` does it end to end. As in VT's `rom.js`, the BPS applies to the unresized 1 MiB JP ROM, not the 2 MiB copy. Verified by:
   - a host test with a **synthetic** base ROM and a synthetic BPS, which checks the order of operations and each error path (wrong MD5, bad BPS, write failure);
   - a test that the written ROM equals the base-patched buffer with exactly the logged writes applied;
   - the header-size test that Phase 4 step 2's gate runs on `data/base.bps` is written here, and checked on a synthetic BPS header.
3. **Output naming, history record and spoiler.** Name a ROM `<Mode>-<Goal>-<code>.sfc`, where `<code>` is 6 hex digits derived from the seed and settings, with a counter suffix on collision. The history record JSON holds the settings, an ISO date and the optional spoiler (reuse `rando/host/spoiler_json.c`'s format). Verified by unit tests: naming of every mode and goal, collision handling, and a record round-trip (write, then read back identical).

### Phase 3 — Onion facts, then the app's UI core and text renderer, host only

1. **Confirm the Onion specifics from Onion's sources** (`OnionUI/Onion`) and record them in `docs/` with links:
   - the app folder layout (`App/<Name>/config.json`, `launch.sh`, icon);
   - the toolchain (the Miyoo Mini cross toolchain Onion builds with), and the SDL version and video setup Onion apps use;
   - the screen resolution on the Mini Flip;
   - the button mapping;
   - the exact RetroArch save and save-state paths for the default SNES core (e.g. under `Saves/CurrentProfile/`).

   Verified by: each item cites the Onion source file, and each §2 path and layout (`App/ALttPR/`, `Roms/SFC/ALttPR/`, the `.srm` and save-state locations) is checked against these facts; a mismatch stops the step and asks the user before steps 4 and 6.
2. **Screen model and text renderer.** The screen description struct and a renderer to a fixed character grid. Verified by a snapshot test of a hand-built screen.
3. **String table (i18n).** Every UI string goes through an id and the English table; a test fails if the UI core contains a string literal that is shown to the user. Verified by that test.
4. **Generate tab state machine.**
   - Every setting of §2 is a row; left and right change the value, and up and down move.
   - Unsupported combinations from Phase 1 step 3 are greyed out with a reason.
   - The rows include the spoiler option (default off), the heart beep option, and the Generate action.
   - The last settings are saved and restored.

   Verified by key-sequence snapshot tests, including restoring from a saved settings file.
5. **Generation flow.** First, the step note estimates the Cortex-A7 generation time from the slowest case's host `-O2` time at the end of Phase 1, and decides the progress indicator per generator stage (if over about 10 s) so this screen and its tests include it. A "Generating…" screen, then success (the file name) or an error message: base ROM missing or wrong MD5, generator gave up, SD card write failed. Verified by snapshot tests with an injected generator, for success and for each failure.
6. **History tab.** The list is newest first and reconciled at start with the ROMs actually present. It has a detail view (settings, spoiler if kept, scrollable) and a delete action with confirmation, which removes the ROM, `.srm` and save states, then the record. Verified by snapshot tests on a temporary directory tree mimicking the SD card, checking the files removed and the files kept.
7. **`ui_cli`: key sequence in, text screen out**, for the agent's own checks. Verified by a smoke test in `make test`.

### Phase 4 — Onion and PC builds

CI is manual-only (`workflow_dispatch`) until the owner turns it on: each step's local target is its verification, and its step note asks the user to run the matching CI job.

1. **SDL renderer and input**, shared by the Miyoo and PC builds: Onion-style list rendering, and key mapping from the d-pad and buttons, or the keyboard on PC. Verified by a local make target, also added as a CI job: the PC build compiles with no warnings and starts, renders one frame offscreen and exits in a headless smoke test (SDL dummy video driver); the same renderer and input layer cross-compile, with no warnings, against Onion's SDL in the Onion toolchain (compile only). The visual check is the user's (§4).
2. **Miyoo build and package.** Gate: `data/base.bps` exists and passes Phase 0's header checks, and a host test reads its header and checks that the pipeline feeds it a source of the declared size (1,048,576) and expects the declared target size (2,097,152); otherwise the step stops and asks the user instead of packaging without it. The step makes a cross build in the Onion toolchain (a container or a pinned toolchain archive), and a release zip with the `App/ALttPR/` layout (binary, `launch.sh`, `config.json`, icon, `base.bps`). Verified by a local make target (the cross build in the toolchain container), also added as a CI job: it produces the zip; `file` reports an ARM 32-bit EABI hard-float executable; and a separate target (not `test` nor `e2e`) runs the cross-built generator under qemu-arm user mode on the e2e cases, whose traces and write logs must match the goldens.
3. **Performance budget**: measure the generation time of the slowest golden case on the host at `-O2`, and estimate the time on the Cortex-A7, confirming Phase 3 step 5's progress-indicator decision (about 10 s), or reopening it if the numbers moved. Verified by: `make -C tests/rando measure` (outside `test` and `e2e`), with its numbers recorded in the step note.

### Phase 5 — Cleanup

1. skill: rework-comments — every file written by this plan.
2. **README**: install (copy `App/ALttPR/`, put the JP 1.0 ROM as `base.sfc`), use, and deletion behavior. Verified by review against §2.

## 4. Done when

- `make -C tests/rando test`, `make -C tests/rando e2e` and `make -C tests/rando e2e-combos` are green: every case claims `all` and its whole write log. `e2e`'s per-case time, as measured by `make -C tests/rando measure` against Phase 1 step 1's commit on the same machine, is unchanged within ±10%, apart from the write-log diff.
- The UI snapshot tests cover every Generate row, the generation success and failure screens, and History list, detail and delete.
- CI produces the PC build and the Onion release zip.
- The user confirmed the base patch MD5 (Phase 0 step note).
- **Human checks by the user**, listed in the final step note:
  - the app runs on the Mini Flip, generates a seed, and the ROM boots and plays to at least the first dungeon;
  - two seeds keep separate saves;
  - delete from History removes the ROM, save and states;
  - the PC build looks right.

## 5. Open questions

1. ~~Which repository.~~ Resolved: this repository (`Canadadry/alttp-randomizer-handled-linux`), bootstrapped at the same paths as `alttp-ds` (`rando/`, `tests/rando/`); Phase 1 step 1 takes the code over.
2. Whether the app's UI core and host tools live under `rando/host/` or a new top-level `app/` directory: decide when the repository exists.

## 6. Step notes
