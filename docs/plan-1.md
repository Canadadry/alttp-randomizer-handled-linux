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
   - `tests/rando/oracle/vt.patch` turns `setTexts`, `randomizeCredits` and `HintService::applyHints` back on (today it comments them out). It labels their PRNG draws (`Location::getHint`'s `fy_shuffle` calls, made only from inside `applyHints`, count as `applyHints` draws), and the draws `writeToRom` itself makes (`setDiggingGameRng`, `writeRNGBlock`, the `fy_shuffle` in `Location/Prize.php`), as new stages in `stages.txt`.
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
   - `e2e`'s time grows only by its new single-setting cases (budget: per-case time unchanged ±10%, measured with `make -C tests/rando measure` against step 1's commit as above). The per-case time is the only `measure` figure this plan gates on: the size, stack and time budgets `measure.sh` inherits from `alttp-ds` (DS targets) are informational, and an `over` on them is recorded in the step note, not a failure.
4. **Port the post-fill draws** (texts, credits, hints, `writeToRom`'s own draws) into the C generator, one stage of step 2 at a time in VT's draw order. Verified by each case's claim reaching, in turn, each ported stage it draws in (a stage with no draw in the case's attempt 0, such as `applyHints` with hints off, `writeItem` without map-on-pickup, or a `writeToRom` stage when the winning attempt is not 0, is skipped for that case), then every case moving back to `all` in `claims.txt`, with `e2e` and `e2e-combos` green.
5. **`rando_rom` skeleton and claims by write group.** The `rando_rom` module writes through a caller-supplied write callback (the host logs writes, the app writes into a buffer), with no allocation. `claims.txt` gains write-log claims by setter group, the way it claims trace stages: a case claims the log prefix up to a group. The groups follow `writeToRom`'s call order, where texts, credits and SRAM are only buffered until `writeCredits`, `writeText` and `writeInitialSram` at the end: items and locations, settings, credits, text, initial SRAM, the trailing `setTotalItemCount`, then the heart beep (`setHeartBeepSpeed`, written by the oracle after `writeToRom`). `e2e` prints one `DIVERGENT <case>` line per case whose claimed prefix differs. Verified by the `selftest` target covering a synthetic divergence in a claimed group, and a pass on an unclaimed one. The same step updates `skill: check-e2e` for the write logs and `e2e-combos`: it ranks a write-log failure by the earliest differing write group, and gives the first differing `rom_writes.txt` line (address, bytes, Rom method) as the bug's reproduction; verified by running it on that synthetic divergence.
6. **Port the items and locations writes.** Verified by every case's claim reaching that group, plus `tests/rando/unit/test_rom_items.c`.
7. **Port the settings and flags writes** (the `Rom` setters `writeToRom` calls). Verified by every case's claim reaching that group, plus `test_rom_settings.c`.
8. **Port the credits writes** (`Credits`). Verified by every case's claim reaching that group, plus `test_rom_credits.c`.
9. **Port the text, dialog and hints writes** (`Text`, `Dialog`), in two halves: first the encoder and default table, verified by `test_rom_text.c` against the default-table fixture; then the seed-dependent strings, verified by the logged setter calls, then by every case's claim reaching that group.
10. **Port the initial SRAM writes** (`InitialSram`) and the trailing `setTotalItemCount`. Verified by every case's claim reaching the `setTotalItemCount` group, plus `test_rom_sram.c`.
11. **Checksum and heart beep.** Port `Rom::updateChecksum` and `setHeartBeepSpeed`. Verified by a unit test of the checksum algorithm on a synthetic 2 MiB buffer whose expected sum is computed independently, and by every case claiming the whole write log, heart-beep group included (the step 3 cases cover each heart-beep value). Every write step 2's note marks "reproduced in C" has its unit test here.
12. skill: check-e2e — the whole case list, write logs included; then `e2e-combos`.

### Phase 2 — Full ROM assembly, host only

1. **BPS applier in C** (no allocation, caller buffers, source and target CRC32 checks). The target is a buffer of its own, never the source's: a SourceCopy may read any source offset, including ones an in-place apply would already have overwritten, and Phase 2 step 2 still reads the unpatched 1 MiB source after the patch (the default Link sprite, step 1.11's note). Verified by unit tests on small hand-built BPS files covering every BPS action and each CRC failure, including a SourceCopy back to a source offset below the current output offset, after a TargetRead wrote different bytes there (the output holds the source's bytes).
2. **ROM assembly pipeline**: check the source MD5 (`03a63945…`), apply the base patch from the 1 MiB JP ROM into a separate 2 MiB buffer (the patch's declared target size is the resize), check MD5 `edc01f3d…`, generate, run `rando_rom`, apply the heart beep and the writes Phase 1 step 2's note marks "reproduced in C" (in the order that note gives), update the checksum, and write the `.sfc`. A host CLI `rando_patch --base <jp.sfc> --out <file> [settings]` does it end to end. As in VT's `rom.js`, the BPS applies to the unresized 1 MiB JP ROM, not a 2 MiB copy; that buffer is only read, and stays unchanged until `rando_rom_write_cosmetic_defaults` has copied the Link sprite from it. `[settings]` are the case format's keys and values (every `RandoOptionKey`, `hints`, `heart_beep`), parsed through the library's name tables; heart beep and hints get theirs in the library, and `case_reader.c` uses them instead of its own list. The ROM and source sizes (`0x200000`, `0x100000`) are one pair of public constants in `rando_rom.h`, used by the checksum, the cosmetic defaults, the pipeline and the header-size test. Verified by:
   - a host test with a **synthetic** base ROM and a synthetic BPS, which checks the order of operations and each error path (wrong MD5, bad BPS, write failure);
   - a test that the written ROM equals the base-patched buffer with exactly the logged writes and the "reproduced in C" writes applied, then the checksum;
   - a test whose synthetic BPS writes different bytes over the sprite ranges (`0x80000+0x7000`, `0xDD308+120`, `0xDEDF5+4`) checks that the written ROM holds the synthetic source's bytes there;
   - MD5 (a library module like step 1's CRC32) checked against RFC 1321's test-suite values and one longer synthetic buffer whose digest is computed with Python's `hashlib`;
   - the pipeline takes the expected source and target digests as inputs, so the synthetic tests run its success path; one test checks that the CLI and the app pass §2's `03a63945…` and `edc01f3d…`;
   - the header-size test that Phase 4 step 2's gate runs on `data/base.bps` is written here, and checked on a synthetic BPS header.
3. **Output naming, history record and spoiler.** Name a ROM `<Mode>-<Goal>-<code>.sfc`, where `<code>` is 6 hex digits derived from the seed and settings, with a counter suffix on collision. The history record JSON holds the settings, an ISO date and the optional spoiler (reuse `rando/host/spoiler_json.c`'s format). Verified by unit tests: naming of every mode and goal, collision handling, and a record round-trip (write, then read back identical) with every setting, hints and heart beep at a non-default value.

### Phase 3 — Onion facts, then the app's UI core and text renderer, host only

1. **Confirm the Onion specifics from Onion's sources** (`OnionUI/Onion`) and record them in `docs/` with links:
   - the app folder layout (`App/<Name>/config.json`, `launch.sh`, icon);
   - the toolchain (the Miyoo Mini cross toolchain Onion builds with), and the SDL version and video setup Onion apps use;
   - the screen resolution on the Mini Flip;
   - the button mapping;
   - the exact RetroArch save and save-state paths for the default SNES core (e.g. under `Saves/CurrentProfile/`).

   Verified by: each item cites the Onion source file, and each §2 path and layout (`App/ALttPR/`, `Roms/SFC/ALttPR/`, the `.srm` and save-state locations) is checked against these facts; a mismatch stops the step and asks the user before steps 4 and 6.
2. **Screen model and text renderer.** The screen description struct and a renderer to a fixed character grid. Verified by a snapshot test of a hand-built screen; by `make lint` building the UI core's sources with `-pedantic-errors`; by a check in `make test` that no UI-core source includes an SDL header; and by `make unit-arm` running the snapshot test under qemu-arm (its sources added to `ARM_UNIT_SRC` if outside `rando/`).
3. **String table (i18n).** Every UI string goes through an id and the English table; a test fails if the UI core contains a string literal that is shown to the user. Verified by that test.
4. **Generate tab state machine.**
   - First, the settings list becomes one library table (every RandoOptionKey, then hints, then heart_beep, each with its key name and value names, owned outside rando_rom.h); name.c, record.c, case_reader.c and patch_cli.c iterate it instead of their own lists, and the uint32 parser record.c and patch_cli.c take from rando_case.h moves with it. Verified by test_name, test_record, test_case_reader, test_patch_cli, e2e and e2e-combos green and unchanged.
   - Every setting of §2 is a row, one per table entry; left and right change the value, and up and down move.
   - Unsupported combinations from Phase 1 step 3 are greyed out with a reason.
   - The rows include the spoiler option (default off), the heart beep option, and the Generate action.
   - The last settings are saved and restored through rando_record's settings writer and reader, not a new parser.

   Verified by key-sequence snapshot tests, including restoring from a saved settings file.
5. **Generation flow.** First, the step note estimates the Cortex-A7 generation time from the slowest case's host `-O2` time at the end of Phase 1, and decides the progress indicator per generator stage (if over about 10 s) so this screen and its tests include it. A "Generating…" screen, then success (the file name) or an error message: base ROM missing or wrong MD5, generator gave up, SD card write failed. Every value of RandoSaveStatus, RandoAssembleStatus and RandoBpsResult maps to a string id through a table sized by the enum's count, with a test failing on a value without one; patch_cli.c's BPS result names follow the same rule. Verified by snapshot tests with an injected generator, for success and for each failure.
6. **History tab.** The list is newest first and reconciled at start with the ROMs actually present. It has a detail view (settings, spoiler if kept, scrollable) and a delete action with confirmation, which removes the ROM, `.srm` and save states, then the record. Verified by snapshot tests on a temporary directory tree mimicking the SD card, checking the files removed and the files kept.
7. **`ui_cli`: key sequence in, text screen out**, for the agent's own checks. Verified by a smoke test in `make test`, whose binary links only the objects `make test` already builds (`$(RANDO_LIB)`, `$(HOST_LIB)` and the app archive step 2 adds), never the `-O2` set (`GEN_OBJ`); a clean `make test` is timed before and after, numbers in the step note.

### Phase 4 — Onion and PC builds

CI is manual-only (`workflow_dispatch`) until the owner turns it on: each step's local target is its verification, and its step note asks the user to run the matching CI job.

1. **SDL renderer and input**, shared by the Miyoo and PC builds: Onion-style list rendering, and key mapping from the d-pad and buttons, or the keyboard on PC. Verified by a local make target, also added as a CI job: the PC build compiles with no warnings and starts, renders one frame offscreen and exits in a headless smoke test (SDL dummy video driver); the same renderer and input layer cross-compile, with no warnings, against Onion's SDL in the Onion toolchain (compile only). The visual check is the user's (§4).
2. **Miyoo build and package.** Gate: `data/base.bps` exists and passes Phase 0's header checks, and a host test reads its header and checks that the pipeline feeds it a source of the declared size (1,048,576) and expects the declared target size (2,097,152); otherwise the step stops and asks the user instead of packaging without it. The step makes a cross build in the Onion toolchain (a container or a pinned toolchain archive), and a release zip with the `App/ALttPR/` layout (binary, `launch.sh`, `config.json`, icon, `base.bps`). Verified by a local make target (the cross build in the toolchain container), also added as a CI job: it produces the zip; `file` reports an ARM 32-bit EABI hard-float executable; and a separate target (not `test` nor `e2e`) runs the cross-built generator under qemu-arm user mode on the e2e cases, whose traces and write logs must match the goldens.
3. **Performance budget**: measure the generation time of the slowest golden case on the host at `-O2`, and estimate the time on the Cortex-A7, confirming Phase 3 step 5's progress-indicator decision (about 10 s), or reopening it if the numbers moved. Verified by: `make -C tests/rando measure` (outside `test` and `e2e`), with its numbers recorded in the step note.

### Phase 5 — Cleanup

1. skill: rework-comments — every file written by this plan; its input includes the comment warnings each phase's warn-review listed (rule violations, unchecked claims; `notes/`).
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
2. ~~Whether the app's UI core and host tools live under `rando/host/` or a new top-level `app/` directory, and where the device-bound `record.c` and `seed_save.c` live.~~ Resolved at Phase 3 step 2: a new top-level `app/` — `app/core/` (UI core, pure C99, only `rando/include` and `app/core` on its include path), `app/sd/` (`record.c`, `seed_save.c`), `app/sdl/` and `app/main/` for Phase 4 and step 7; host tools stay in `rando/host/`. The core is built by `make lint` and `make unit-arm`, and `make test` refuses an SDL header in it.

## 6. Step notes

### Step 0.1

Outcome (b): no archived copy of the `edc01f3d…` BPS was found. `https://alttpr.com/bps/edc01f3db798ae4dfe21101311598d44.bps` still returns 404; the Wayback Machine's availability API reports no snapshot of that URL (with or without scheme, any timestamp); its CDX index and `web.archive.org` itself are blocked by this session's egress policy; a GitHub code search for the hash finds only VT's `app/Rom.php`. `data/base.bps` is not committed; Phase 4 step 2's gate waits on it.

The user builds it (asar and flips on a PC, with their JP 1.0 ROM `jp.sfc`, headerless, MD5 `03a63945398191337e896e5771f77173`):

```
git clone https://github.com/KatDevsGames/z3randomizer && git -C z3randomizer checkout dcb0a2b42d14445f7994a0e9e4d63cbecf4b98d3
cp jp.sfc work.sfc && truncate -s 2097152 work.sfc     # zero-padded, as VT's resizeUint8
(cd z3randomizer && asar --fix-checksum=off LTTP_RND_GeneralBugfixes.asm ../work.sfc)
md5sum work.sfc                                         # must print edc01f3db798ae4dfe21101311598d44
flips --create --bps jp.sfc work.sfc base.bps           # source: the unresized 1 MiB jp.sfc
```

The BPS source is the unresized 1 MiB ROM because VT's browser applies it to `originalData`, the file as loaded before `resize` (`resources/js/rom.js` `parseBaseBPS`), and `resources/js/bps.ts` `applyPatch` rejects any source whose CRC32 differs from the patch's. The user then commits `base.bps` as `data/base.bps` and confirms here that `work.sfc`'s MD5 is `edc01f3d…`. Phase 1 does not need it.

### Step 1.1

Took over `rando/` and `tests/rando/` verbatim from `Canadadry/alttp-ds` `8d772553f7b9a387d35e072daa9926cc07f9b62f` (`diff -r` clean but for `tests/rando/Makefile`'s kept `lint`), plus the `check-e2e` and `rework-comments` skills, retargeted (`check-e2e` is a plan `skill:` step; `rework-comments` covers `rando/`, `tests/`). Root `check` = `test lint logic selftest e2e`; `test` unchanged.

- `e2e`: 44 pass, 0 divergent, 0 pending; all 44 claims `all`, every `attempts` 0.
- Reference `measure` (commit `40314c3`, Xeon 2.10 GHz, nproc 4; full log kept in `tmp/measure-ref-40314c3.log`), host `-O2` per-case median: most cases 2.9–5.3 ms; `glitches-no_logic` 0.98, `goal-triforce-hunt` 5.8, `goal-ganonhunt` 6.6, `dungeon_items-full` 6.9, `item_placement-basic` 17.1 ms (max); sweep median 4.4, max 17.8 ms. `measure.sh` still prints DS-era budgets (`over` on time 13000 µs; code+rodata 126836 of 131072): kept unchanged so the reference stays comparable.
- Oracle reproduction: php 8.3.6, Composer 2.8.12, VT `219fcafd`; `VT_DIR=tmp/vt<N> gen_golden.sh --out tmp/oracle-b<N>` in four parallel batches of 11 cases: identical, 44 cases, 10 fixtures, in every batch.
- Cross build: `make e2e-arm` (outside `test`, `e2e`, `check`) builds `rando_gen` static in `aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132` (Onion's `Makefile` `TOOLCHAIN`, `:latest` on 2026-11-01; gcc 8.3.0, glibc 2.28) through `onion-toolchain.sh`, with Onion's miyoomini flags; `file`: ELF 32-bit ARM EABI5; under qemu-arm 8.2.2: 44 pass, 0 divergent. Facts and links in `docs/toolchain.md` (OnionUI/Onion `07505ea5`). Needs `dockerd` running and `qemu-user`; a missing one prints a `setup:` line. Write-log comparison joins it at step 5.
- Plan review after this step: accepted five method changes (heart-beep group after `setTotalItemCount`, owned by step 11; `getHint` draws counted under `applyHints`; step 4 skips stages a case draws nothing in; `measure` gates only per-case time; Phase 2 applies the "reproduced in C" writes).

### Step 1.2

- **Oracle:** `vt.patch` turns `setTexts`, `randomizeCredits` and `applyHints` back on and labels `writeToRom`'s draw sites. On the winning attempt, after `isWinnable`, `gen_case.php` runs `writeToRom($rom, false)` on a sourceless logging `Rom` (`oracle_rom.php`), then `setHeartBeepSpeed(<case>)`, and writes `rom_writes.txt` (`%06X <HEX> <tag>`, tag = first non-closure, non-`Collection` frame below `writeToRom`) and `rom_buffers.txt` (`text`, `credit`, `sram` setter calls). New fixture `oracle/text_default.txt` (408 ids). Self-checks: the log equals `getWriteLog()`; the setter logs rebuild the `writeText` and `writeInitialSram` writes.
- **Stages** after `fill`: `setTexts`, `randomizeCredits`, `applyHints` (`getHint` draws included), `writeItem` (only `dungeon_items` mc, mcs, full), `setDiggingGameRng`, `writeRNGBlock`, and `writePrizePacksToRom`. **Deviation:** the last one (`getDropsPool`'s `fy_shuffle` of the 63 drops, every case) was in neither the plan nor the PRD; found when the oracle threw on an unlabelled draw, added as the last stage.
- **Harness:** `check <stage> attempts`; a claim owes an earlier stage's checks except `pool`; `fill` checks every spoiler section plus `attempts`. All 44 claims are `fill`; a real-table selftest proves a changed placement prints `DIVERGENT`. A stop at `fill` or later retries to the winning attempt; `rando_gen` then writes `attempts` and the playthrough. Case keys `hints` (on/off) and `heart_beep` (off/half/quarter/double/normal), defaults `on`, `half`.
- **Regeneration:** php 8.3.6, Composer 2.8.12, VT `219fcafd`. Three runs, each four parallel `--out` batches of 11 cases, gave identical files (`diff -r` empty). Against the old goldens: each trace is the old one plus post-fill draws; `spoiler.json`, `attempts` (all 0) and the 10 old fixtures byte-identical; every `rom_writes.txt` ends `180033 40 setHeartBeepSpeed`. The longer baseline trace (72 KB) overflowed `test_generate`'s 64 KiB golden buffer; fixed before the goldens commit.
- **`$save` = `false`:** `true` only adds `setSeedString` and `setStartScreenHash`, both left out below.
- **ROM writes outside the log** (re-read `prepSeed`, `writeToRom`, `rom.js`, the settings and sprite components; nothing beyond this list):

| Write | Outcome |
|---|---|
| `setSeedString`, `setStartScreenHash` (under `$save`) | left out: alttpr.com seed-record hash; no seed database or sharing on the device |
| `prepSeed` Enemizer, `setTournamentType`, `rummageTable` | left out: Enemizer and tournament are out of scope |
| `prepSeed` `applyPatchFile` (base patch, `$log = false`) | not a seed write: it is `data/base.bps` (Phase 2) |
| `setMSU1Resume`, palette and SFX shuffle | left out: write nothing at VT's defaults |
| heart beep (`setHeartBeepSpeed`) | in the log, last line |
| `setQuickswap` (`0x18004B`=`00`), `setMusicVolume` (`0x18021A`=`00`), `setReduceFlashing` (`0x18017F`=`00`), `setMenuSpeed` normal (`0x180048`=`08`, `0x6DD9A`=`11`, `0x6DF2A`=`12`, `0x6E0E9`=`12`), `setHeartColor` red (branch on base-patched byte `0x7FE2`), default Link sprite (from the JP ROM: gfx `0x80000`, palette `0xDD308`, gloves `0xDEDF5`, author `0x118002`/`0x118020`), `updateChecksum` | reproduced in C, step 11, applied in that order by Phase 2 step 2 |

- Noted: `writeToRom`'s `crystals.tower === 0` is false for the string `'0'` that the web form and the oracle pass, so `crystals_tower-0` makes no `preOpenGanonsTower` call.

### Step 1.3

- **Cases:** six single-setting cases join `e2e` (`hints-off`, `heart_beep-{off,half,quarter,double,normal}`); 23 combinations in `tests/rando/combos/`, run by `make e2e-combos` (in `check` after `e2e`, claims in `combo_claims.txt`): state × glitches (12), `dungeon_items` full × state (3), triforce-hunt and ganonhunt × state (6), `glitches-major_glitches+dungeon_items-full`, `state-retro+item_pool-expert`. All seed 1, all won at attempt 0; every one selectable on alttpr.com (crowd control forces advanced placement, `dungeons` forces Ganon's crystals 7, `completionist` forces locations accessibility; none of the 23 hits them). Goldens: two four-batch `--out` runs identical (php 8.3.6, Composer 2.8.12, VT `219fcafd`); the 44 old cases and fixtures byte-identical, the two pool fixtures gain six records.
- **Results:** `e2e` 50 pass, `e2e-combos` 23 pass, all at `fill`. One divergence, fixed (`0753c46`): `hints-off` at `fill`, meta `"hints"` hard-coded `"on"` in `spoiler_json.c`. **Unsupported: none** — no combination returns `RANDO_UNSUPPORTED`, so Phase 3 greys nothing out on this account. `test_case_reader` guards the coverage (≥ 50 cases, ≥ 23 combos, every required pair, no name in both directories).
- **Deviation:** `measure.sh`'s golden-parser self-check pinned baseline's pre-1.2 draws (805 total, nothing after `fill`) and refused the regenerated goldens; updated to the golden's real counts (2505: `setTexts` 255, `randomizeCredits` 78, `applyHints` 279, `setDiggingGameRng` 1, `writeRNGBlock` 1024, `writePrizePacksToRom` 63). Its "differs" line now lists every case until step 4 ports those draws (informational).
- **Budget** (per-case time ±10% on the 44 shared cases, against a throwaway worktree of `40314c3`, same session): two sequential runs of both trees (5 and 21 reps) gave mean ratios 0.93 and 0.98, with cases flagged outside ±10% in both directions and different cases each run (noise of this shared machine). The only case over in both, `glitches-no_logic` (+12%, +11%, ~1 ms), measured interleaved (six alternating pairs of 51 reps, driver `cases`): ≈1.000 ms ref, ≈1.012 ms current, +1%. Verdict: within budget. Lesson for later budget checks: compare interleaved, not run after run.

### Step 1.4

- **Port:** every post-fill draw is in C, in VT's order. `setTexts`, `randomizeCredits` and `applyHints` run on every attempt that completes the fill, before the winnability check (as in VT's `randomize()`); `writeItem`, `setDiggingGameRng`, `writeRNGBlock` and `writePrizePacksToRom` run on the winning attempt only. Choices are kept as ids and indices in `RandoGen.post` (`RandoPostFill`, `rando/include/rando_postfill.h`): texts, credits, hints per telepathic tile, prize music, digging value, the 1024-byte RNG block (a drawn 256 stored as 0); the drops are assigned into the world's prize packs. Also: a copy of the advancement list after `prepareWorld`, the Bow overflow count in `RandoConfig`, and `hints` as `RandoOptions.hints` (`RandoCase.hints` gone).
- **New fixture:** `oracle/hint_tables.txt` (hint and string alternative counts read from VT), its own commit with every golden byte-identical; checked by `test_postfill --fixtures` in `make logic`.
- **VT quirks the goldens settled:** pair hint picks never hint (`getName()`'s `:<world id>` suffix misses every lookup); a ProgressiveBow hint skips `setTexts`' alternate bow (a new `Item\Bow` that PHP's loose `==` doesn't match), so `applyHints` runs after `setTexts`; the bow pick is drawn whatever the branch; `random()` over nothing makes no draw; the hint merge keeps a location at its first position; Ganons Tower's Agahnim 2 draws prize music, Hyrule Castle Tower's doesn't.
- **Claims:** moved stage by stage (`hints-off` skips `applyHints` and `writeItem`; only the seven `dungeon_items` mc/mcs/full cases draw in `writeItem`); now every claim is `all`: `e2e` 50 pass, `e2e-combos` 23 pass. `test_generate`'s stop tests were rewritten on purpose at each stage (baseline 805 → 2505 draws).
- **Time** (recorded, not gated: step 4 sets no budget). Interleaved, ref `40314c3` / current, two `MEASURE_REPS=11` runs each, per-case minimum: mean ratio 1.024 on the 44 shared cases, min 0.86 (`goal-ganonhunt`), max 1.31 (`goal-triforce-hunt`), `item_placement-basic` 17.1 → 21.0 ms (+23%), baseline 0.99. The growth is the ~1,700 post-fill draws per case. §4's ±10% "apart from the write-log diff" will need its own judgement at the end of the plan: this step's draws are part of what VT does, not the write log.

### Step 1.5

- **`rando_rom`** (`rando/include/rando_rom.h`, `rando/src/rom.c`): `bool rando_rom_write(const RandoGen *, RandoHeartBeep, const RandoRomWriter *)` calls one empty function per group in `writeToRom`'s order (items, settings, credits, text, initial SRAM, `setTotalItemCount`, heart beep) through a static `emit` that stops at the first refused write; the callback gets (ctx, PC address, bytes, count ≥ 1, static method tag). Bounds-checked `RandoRomBuffer` writer in the library; the host log writer (`rando_out`) prints `%06X <HEX> <method>`; `rando_gen` writes `rom_writes.txt` on every complete run (empty for now). `RandoHeartBeep` moved to `rando_rom.h`. The "no write after a refused one" test needs a real write: handed to step 6.
- **Claims by write group:** `tests/rando/write_groups.txt` (settings is the default `*` group); a claim `<case> all <group>` also compares the golden log up to that group's last write (last group = the whole log, extra C writes divergent), printing the first differing write; every claimed golden is checked against the group order (all 73 pass). Selftests: a divergence in a claimed group is `DIVERGENT`, one in a later group passes, and every error path. One selftest changed on purpose (a three-field claim is now the write-claim form). No real write claim yet.
- **`skill: check-e2e`** now covers `e2e-combos` and write logs (seed-level first, then earliest group and write; reproduction = first differing `rom_writes.txt` line + replay command); dry run on a synthetic set ranked them as specified.
- **`make e2e-arm`:** 50 pass under qemu-arm; it compares write logs through the same claims.

### Step 1.6

- **Items group** (`rando/src/rom_items.c`; `emit` became `rando_rom_emit` behind the private `rando/src/rom_emit.h`): regions in order, each region's filled locations then its empty ones written as Nothing (the C world itself keeps them empty); item bytes in 13 slots and per-location address lists generated from VT's `Item.php` and `Region` classes, checked to agree across variants. Aliases write their target, events nothing, prizes slots 1–6 plus music (default by pendant/crystal, or `post.prize_music` under map-on-pickup; Hyrule Castle Tower's Agahnim none, Agahnim 2 `0x16` at `0x155C9`), medallions their t*/m* slots, Blacksmith `0x3355C` when swords are not in the pool (no golden reaches it: unit-tested from VT's source).
- **Substitutions** before alias resolution, VT's order: vanilla keys, big keys, maps, compasses, then generic keys `KeyGK`; `KeyH2`/`MapH2` kept at Secret Passage and Link's Uncle. Hera's cage also writes `0x4E3BB` (`E4` for Key/KeyP3, `EB` otherwise); the alternate bow writes `0x65`.
- **Result:** all 73 cases claim `all items` (315 lines each), `make check` green. Chunk 1 alone matched 0 of 73; every residue was a chunk 2 rule. `test_rom` now expects only `writeItem` writes (rewritten on purpose); `test_rom_items` holds the step 5 refusal test (refusing the third write gives 3 calls and false).
- **For later steps:** VT's `writeItem` overrides also fill the credits and text buffers (steps 8, 9); not written here.

### Step 1.7

- **Settings group** (`rando/src/rom_settings.c`, fixed data in `rom_settings_data.c`): every row `writeToRom` makes outside the other groups, in VT's call order with VT's tags (the outer call's name: `setGameState`, `writePrizePacksToRom`, `writeToRom`…), 90 to 296 writes per case. Config-only rows first (goals and clamped crystals, item functionality, limits and overflow replacements via `rando_rom_item_byte`, retro, dungeon items, logic, swordless, digging, RNG block…), then the world-dependent ones: shops (last active id `0xFF`, take-any extra write first, cleared Capacity Upgrade stays active), prize packs (no-fairy and rupee-bow swaps, dig prizes first under rupee bow), standard escape fills (all five outcomes unit-tested; goldens reach two), map reveals, uncle rows (shield always written; an `UncleSword` alias is not a Sword). Inverted: `setInvertedMode`'s 192 writes plus `setFixFakeWorld` in place, extracted from VT's `Rom.php` by evaluating each write, never from a golden.
- **Config:** `RandoConfig` gained the World constructor keys these rows read (item functionality, sword/armor/heart overflow counts, free-item text/menu bits, dungeon count pickup); single-valued keys are named constants.
- **Result:** all 73 cases claim `all settings`, `make check` green. Tests: `test_rom_settings` (one case per row family, refusal rule); `test_rom` rewritten on purpose (items, then settings ending `enableFastRom`); `test_rom_items` records only the items group now, and its world builder enables the default shops and drops.
- **For later steps:** `setInvertedMode`'s `text->setString` calls belong to the text group (step 9); `crystals.tower === 0` stays false for the string `'0'` (step 1.2 note).

### Step 1.8

- **New fixture** `oracle/credit_tables.txt` (`credit_tables.php`, its own commit, every golden byte-identical, two runs identical): VT's encoded bytes for the 16 scene defaults, the 78 `randomizeCredits` alternatives, and 179 items × 6 override scenes; every scene segment of every golden's credits write appears in it. Needed because the goldens reach only 14–18 texts per override scene.
- **Credits group** (`rando/src/rom_credits.c`, lists in `rom_credits_data.c`, override tables in `rom_credits_overrides.c`): a literal port of VT's `Credits` (centring, header, `small`/`small_alt`/`large` converters, apostrophe and comma records), data at `181500` and 17 little-endian pointers at `076CC0`, both `writeCredits`. The seven picks and smithy coin come from `post.credits`; the six `writeItem` override scenes read their location's own item (empty as Nothing, alias as target, no key substitution): `BigKeyA2` at the pedestal, then key/big key/map/compass class texts, the item's entry, the default. Lists and tables were extracted mechanically from VT's source; unit expectations were computed by a separate Python port of VT's rules.
- **Result:** all 73 cases claim `all credits`, `make check` green; `test_rom_credits --fixtures` (in `make logic`) replays every pick, untouched-default and item record (1074/1074). `test_rom` rewritten on purpose (two `writeCredits` after settings). Unreachable fixture names: none. One PRD unit case has no test: no pick or override text holds a byte outside letters, space and `, ' - .`, so "unknown byte → `9F`" is reachable only through the space, already covered.

### Step 1.9

- **First half — encoder and default table:** `rando_text_encode` (`rando/include/rando_text.h`, `text.c`) ports `convertDialogCompressed` (PHP 8.3 `wordwrap` / `mb_wordwrap` re-pack, row and scroll markers, the pause rule where a trailing `{…}` line lowers the count twice, commands, `{NOTEXT}`, `{INTRO}`, 189-entry character table, 2046-byte cut); 66 unit cases with bytes from VT's own `Dialog`, and all 560 encoder calls of `Text::en()` matched in a throwaway check. `RandoTextId` lists the 408 ids; defaults kept as source strings (8 raw arrays) and encoded; the text group writes the table at `0E0000` in one `writeText` from a 31 KB stack buffer. `test_rom_text --fixtures` replays `text_default.txt`.
- **New fixture** `oracle/text_tables.txt` (`text_tables.php`, its own commit, goldens byte-identical, two runs identical): 326 string-file picks, 179 × 3 tablet/pedestal texts, 223 hint item and 406 hint location names.
- **Second half — seed texts** (`text_seed.c`, strings generated from VT's `getTextArray` and `hint.php`, tablet/pedestal tables from VT's own functions): setTexts picks, Sahasrahla/bomb-shop regions, bow branches, the GT sign, goal texts, murahdahla, hints-off sign, joke and location telepathic tiles (`ucfirst("<item> <location>")`, missing keys printed literally), tablets and pedestal (empty as Nothing, Key as KeyGK under generic keys), and `setInvertedMode`'s texts — **11, not the PRD's 12**. The hint names are now the single source of the hint alternative counts (`test_postfill --fixtures` green). A cleared `RandoPostFill` now holds the fake bow hint.
- **Result:** every fixture record replays; `test_rom_text --fixtures` matches every golden's `rom_buffers.txt` text lines for every id; all 73 cases claim `all text`, `make check` green. `test_rom` rewritten on purpose (one `writeText` after the credits).

### Step 1.10

- **Initial SRAM** (`rando/src/rom_sram.c`): VT's 0x500-byte `InitialSram` built on the stack in VT's call order — constructor bytes, `preOpenPyramid` (fast_ganon, ganonhunt), open mode (open/retro/inverted), swordless curtains, `setStartingEquipment` (heart guard, pre-collected items with aliases as targets, rupees, counters, capacities, swordless override), the zero timer, `preOpenGanonsTower` only for a *drawn* tower count of 0 (no golden reaches it) — written once at `183000` as `writeInitialSram`. Starting equipment is ported only for the items this generator pre-collects (heart containers, bomb/arrow upgrades, Pegasus Boots, sword, events); any other item asserts.
- **Total item count:** the variant's locations that are not Prize, Fountain, Medallion or Trade (216 on every state, no literal), at `180196` as `setTotalItemCount`.
- **Result:** all 73 cases claim `all setTotalItemCount`, `make check` green. `test_rom_sram` (one case per rule, refusal) and `test_rom_sram --fixtures` in `make logic` (all 73 SRAM tables and counts; a differing offset names its golden `sram` setter lines). `test_rom` rewritten on purpose twice (SRAM, then the count).

### Step 1.11

- **Heart beep:** the last group writes `0x180033` as `setHeartBeepSpeed` (off `00`, half `40`, quarter `80`, double `10`, normal `20`). **Every case and combination now claims `all heart_beep`: its whole `rom_writes.txt` equals VT's, extra writes included** (50 + 23, `make check` green).
- **Checksum** (`rando_rom_update_checksum`, `rom_checksum.c`): VT's `updateChecksum` on a 2 MiB caller buffer (sum from `0x1FE`, skip `7FDC–7FDF`, inverse then checksum little-endian at `7FDC`); other sizes return false. Tested on four synthetic buffers against bytes computed independently in Python (all 00 → `01 FE FE 01`, all FF → `FD 01 02 FE`, `i&0xFF` → `77 01 88 FE`, `(i*7)^(i>>11)` → `43 FE BC 01`), the sum property, idempotence, the skipped range, wrong sizes.
- **Browser defaults** (`rando_rom_write_cosmetic_defaults`, `rom_cosmetics.c`), the "reproduced in C" writes of step 1.2's note, in this order: `0x18004B=00`, `0x18021A=00`, `0x18017F=00`, menu speed (`0x180048=08`, `0x6DD9A=11`, `0x6DF2A=12`, `0x6E0E9=12`), heart colour red (if `rom[0x7FE2] <= 03 || == FF`: `24` at `0x6FA1E…0x6FA30` step 2 and `0x65561=05`; else `0x187020=00`), then the Link sprite (author "NINTENDO" centred, at `0x118002`/`0x118020` only behind the `02 37` guards; gfx `0x80000+0x7000`, palette `0xDD308+120`, gloves `0xDEDF5+4` copied from the 1 MiB JP ROM). 2 MiB ROM and 1 MiB source only. Kept outside `rando_rom_write` because VT's log has none of them and they read ROM bytes; Phase 2 calls `rando_rom_write`, then these, then the checksum. z3randomizer's source suggests the real base build takes the new heart-colour branch and passes the author guard: checkable only once `data/base.bps` exists.

### Step 1.12

`skill: check-e2e` over the whole case list: `make -C tests/rando e2e` 50 pass, `make -C tests/rando e2e-combos` 23 pass, every claim `all heart_beep` (trace, `attempts`, `spoiler.json` and the whole write log): E2E-GREEN on the first run, no `fix:` commit. `make e2e-arm` (Onion toolchain, static ARM EABI5, qemu-arm 8.2.2): 50 pass, write logs included.

### Step 2.1

- **Modules** (`rando_crc32.h`/`crc32.c`, `rando_bps.h`/`bps.c`, no allocation, no state): zlib's CRC32 over a constant table; `rando_bps_read_header` (magic, VT's `decodeBPS` numbers with overflow and footer bounds, metadata skipped, three footer CRC32s, patch CRC32) and `rando_bps_apply` (source size then CRC32, capacity, VT's four actions with separate SourceCopy/TargetCopy offsets, exactly the declared target size, target CRC32). Deliberate departures from VT, all rejections that a flips patch never hits: source size must equal the header's, the declared target size is authoritative, any out-of-range access or runaway number is MALFORMED.
- **Target is its own buffer** (plan review): the source is only read; a test has a SourceCopy back below the output offset after a TargetRead overwrote it, and the output holds the source's bytes. The overlap ban is a stated contract, not checked at run time.
- **Tests:** `test_crc32` (`""`, `"a"`, `"123456789"`, 0..255 → `29058C73`, from Python's `zlib`), `test_bps` (header sizes incl. `00 7F BE`/`00 7F FE` → 1,048,576/2,097,152, one patch per action and per fault, order tests, a 31-byte literal patch whose CRC32s were computed with `zlib.crc32`); six mutations of the applier each caught. Hypotheses about steps 1.11–1.12 held (no CRC/MD5/BPS code existed; registration unchanged).

### Step 2.2

- **Pipeline** (`rando_assemble.h`, `assemble.c`; no file I/O, no allocation, caller-owned `RandoAssembleWork` = 2 MiB image + `RandoGen`): source size, source MD5, patch declared sizes (`rando_assemble_check_patch`), BPS apply from the 1 MiB source into the separate image, base MD5, generate (trace hook passed through), `rando_rom_write`, `rando_rom_write_cosmetic_defaults` (sprite from the still-unchanged source), checksum, caller output. One status per failure (SOURCE_SIZE, SOURCE_MD5, PATCH + BPS code, PATCH_SIZES, BASE_MD5, UNSUPPORTED, GAVE_UP, OUTPUT); nothing runs after a failure. §2's digests enter through `rando_assemble_request_init`; tests inject their own.
- **Supporting modules:** `rando_md5` (RFC 1321 A.5, padding-boundary lengths and a 1 MiB + 13 buffer from `hashlib`); `RANDO_ROM_SIZE`/`RANDO_ROM_SOURCE_SIZE`; heart-beep and hints name tables in the library, used by `case_reader.c` and the CLI.
- **CLI:** `rando_patch --base <jp.sfc> --out <file> [--bps <patch>] [--seed <n>] [key=value ...]` (`rando/host/patch_cli.c`); `--bps` defaults to `data/base.bps`, a missing seed is drawn from `/dev/urandom`. A headered (1 MiB + 512) ROM is refused as a wrong size, not stripped.
- **Tests:** `test_assemble` (every path, order tests, the sprite ranges hold the source's bytes, and a golden replay: synthetic base + each case's `rom_writes.txt` + defaults + checksum = the assembled image byte for byte, baseline in `make test`, all 73 in `make logic`); `test_patch_header` and `make patch-header` (outside `check`; reports `data/base.bps` missing for now) for Phase 4's gate; `test_patch_cli` (default request carries `03a63945…`/`edc01f3d…`; end to end on synthetic files).
- **Not tested:** `UNSUPPORTED` (no combination reaches it, step 1.3); a refused `rando_rom_write` into the 2 MiB buffer is impossible and asserted; the partial-file removal after a failing `fwrite`/`fclose` (only a failing `fopen` is tested). Side effect: `patch_cli.c`'s static buffers (~7 MiB `bss`) are linked into `rando_gen` and the ARM build too, by the host wildcard.
- H-2.1 hypotheses held (the BPS no-overlap rule is a contract, not a run-time check).

### Step 2.3

- **Naming** (`rando_name.h`, library, no stdio): `<Mode>-<Goal>-<code>[-<n>].sfc`, mode Open/Standard/Inverted/Retro, goal Ganon/FastGanon/Dungeons/Pedestal/TriforceHunt/GanonHunt/Completionist; code = low 24 bits of the CRC32 of a fixed text of the seed and all 13 settings (heart beep included), 6 uppercase hex; suffix `-2`…`-99` through a caller "taken" check (ROM or record present), never overwriting. Baseline seed 1 → `Open-Ganon-1CA10A.sfc`. `rando_options_get` added.
- **Record** (`rando/host/rando_record.h`, `record.c`): JSON sorted and indented like the spoiler — `code`, `date` (local ISO 8601, caller-supplied), `format` 1, `rom`, `seed`, `settings` (13 by name), optional `spoiler` last, written by `spoiler_json.c` unchanged. `rando_record_parse` is strict and allocation-free, returns the spoiler as a span, and refuses a record whose `rom` differs from the name its settings and code give (History can only delete files the app named).
- **Save** (`rando_seed_save.h`, `seed_save.c`): name, assemble straight into `Roms/SFC/ALttPR/<name>`, then `App/ALttPR/history/<code>.json`; a failed record removes the ROM (both or neither); the spoiler option never reaches the ROM. Directories are not created (C99 has no `mkdir`): left to Phases 3–4. The ROM file writer moved from `patch_cli.c` to `rando_out`; the synthetic base builder to `tests/rando/unit/synthetic_base.c`.
- **Deviation:** the PRD's chunk 5 (`rando_patch --sd/--spoiler`) was dropped before triage as beyond the step; `rando_patch` keeps step 2.2's interface.
- **Tests:** `test_name` (28 names, codes from Python's `zlib`, a code change per setting, collisions), `test_record` (exact text; spoiler span = baseline golden; round-trip with every setting, hints and heart beep non-default, with and without spoiler; every refusal; `--fixtures` on all 73), `test_seed_save` (success, spoiler on/off same ROM, collisions, each failure leaving nothing).
- **End of Phase 2, ARM:** `make unit-arm` (new, outside `check`) cross-builds all 31 unit tests in the Onion toolchain and runs them under qemu-arm plus the 12 fixture replays: all pass; `make e2e-arm` 50 pass. It needs `-D_FILE_OFFSET_BITS=64`: qemu-user on a 64-bit host kernel gives the 32-bit test binaries ext4 directory positions that overflow `readdir` (`EOVERFLOW`); a 32-bit device kernel does not, and only tests list directories so far (matters for History's tests).
- **Fix found by plan review:** `make test` depended on `rando_patch`, rebuilding the whole library at `-O2` (a clean `make test` doubled); the dependency moved to `e2e`, so `make check` still builds it. Clean `make test`: 11 s.
- **Plan review after Phase 2** (accepted): one settings table before the Generate tab (step 3.4); open question 2 decided at step 3.2, with the UI core in `lint`, `unit-arm` and an SDL-include check; `ui_cli` links only `make test`'s objects (step 3.7); every save/assemble/BPS status mapped to a string id by an enum-sized table (step 3.5). Warnings left open: `notes/phase2-warn-review.md`.

### Step 3.1

- **`docs/onion.md`** (every fact linked at a full commit hash): Onion `main` `07505ea5` and `v4.5-dev` `2bab767e` — **only `v4.5-dev` knows the Mini Flip (device 285, PR #1860)**, so the app needs a Flip-aware Onion build; RetroArch 1.22.2 (`69a4f0ea`, via Onion's RetroArch-patch `f9e959f7`); libretro/supafaust `75c658cc`. Apps use **SDL 1.2** (+ SDL_ttf 2.0.11, SDL_image 1.2.12, fbcon; no SDL2 in the toolchain image, only bundled by some ports); an app gets **640×480** unless it ships `full_resolution`; the full button map (A=SPACE, B=LCTRL, …, Menu=ESCAPE) and the combinations keymon keeps; the SNES list (`Emu/SFC`, one subfolder level, cache `Roms/SFC/SFC_cache6.db` refreshed by deleting it); save `Saves/CurrentProfile/saves/Supafaust/<base>.srm` and states `…/states/Supafaust/<base>.state*` (folder from the core's `library_name`; Onion's READMEs' "Beetle Supafaust" is wrong for this core); other per-ROM files; Guest Mode profiles. `docs/toolchain.md` re-checked at `2bab767e`, unchanged.
- **Checked against §2** (`docs/onion.md` §10): all seven paths and layouts MATCH, none MISMATCH; consequences assigned: C1–C2 app packaging and SD root (Phase 4 step 2), C3 create `Roms/SFC/ALttPR/` and `history/` (step 3.5 / Phase 4 step 2), C4 delete Onion's SNES list caches after a write or delete (steps 3.5, 3.6), C5–C8 delete's file set, Guest Mode profiles, same-name ROMs elsewhere, other per-ROM files (step 3.6). Not §2: the grid is sized from 640×480 (step 3.2), keys follow Onion's Tweaks/Package Manager conventions (step 3.4), SDL 1.2 vs a bundled SDL2 (Phase 4 step 1).
- **Device checks for the owner** (end of `docs/onion.md`): the Onion build on the Flip (`version.txt`, `/tmp/deviceModel` = 285), the app resolution, the `Supafaust` folder of an existing SNES save, the card's retroarch.cfg values, the subfolder list refresh, an icon inside `App/ALttPR/`.

### Step 3.2

- **Open question 2 decided:** a new top-level `app/` (§5 struck through with the answer). `app/sd/` now holds `record.c`, `seed_save.c` and their headers (moved unchanged); `librando_app.a` is linked into every unit test; `lint` covers `app/` (core with its restricted include path); `unit-arm` cross-builds it. `rando_gen`, `rando_patch`, `e2e-arm`, `measure` untouched.
- **Screen model** (`app/core/ui_screen.h`): up to 4 tabs, a title, up to 64 rows (setting/action/text, greyable), a cursor (−1 none), scroll with more-above/below flags, an info or error message, a footer; borrowed text, NULL draws empty; `ui_screen_valid`, `ui_scroll_follow`.
- **Grid and text form** (`app/core/ui_grid.h`): 20–96 × 5–40 cells, default **40 × 15** (Onion's 640×480, 16×32 px cells), one style per line; layout: tab bar, title, list (right-aligned `< value >` on the cursor row, `^`/`v`, `~` cuts, non-ASCII as `?`), message, footer; page = lines − 4. `ui_grid_text` frames the grid with a style tag per line — what the agent reads. No string literal in `ui_grid.c`.
- **Tests:** `test_ui_screen`, `test_ui_grid` (six hand-written snapshots in `tests/rando/unit/snapshots/`, no update mode, plus rule tests); `tests/rando/no_sdl.sh` in `make test`, self-proved on every run. `make unit-arm`: 33 unit tests and 12 fixture replays under qemu-arm, snapshots included. Clean `make test`: 10.7 s.
- H-3.1 hypotheses held (640×480, Onion's header/list/footer menus, SDL_ttf available, the `docs/onion.md` path updated).

### Step 3.3

- **String table** (`app/core/ui_strings.h`, `ui_strings.c`, English in `ui_strings_en.c`): ids from one X-macro list giving both `UiStrId` and each id's arity (0 or 1 `{}`); `UI_STR_NONE` = 0 reserved and never shown; designated-initializer English table; `ui_str` falls back to English and never returns NULL; `ui_str_fill` without printf; `ui_strings_check(table, complete, …)` refuses missing (when complete), empty, non-printable-ASCII, stray-brace, wrong-arity or too-long (> `UI_GRID_DEFAULT_COLS − 2` = 38) entries. The extra `complete` argument tells English from a translation, which may leave entries NULL. Seed ids: the two tab names and the delete confirmation.
- **Literal check** (`tests/rando/no_literals.py`, in `make test` after `no_sdl.sh`): a stdlib C lexer fails on any `app/core` string literal holding a letter after escapes and printf conversions are removed; `ui_strings_en.c` is the only exemption, by exact path; self-proof on planted cases every run (two cases added after disabling lexer rules left verdicts unchanged). Hand check: a planted `"Planted"` failed the build.
- **Results:** H-3.2 hypotheses held (no step-2 core literal needed changing). `make unit-arm`: 34 unit tests + 12 fixture replays. Clean `make test`: 10.5 s → 10.9 s (the script itself ~0.06 s).

### Step 3.4

- **One settings table** (`rando/include/rando_settings.h`, `settings.c`): the 11 option keys, then hints, then heart beep, with key and value names, defaults, the bytewise sort order, `rando_parse_uint32`, and the unsupported-pair list (empty, per step 1.3). `RandoHeartBeep` moved here from `rando_rom.h`. `name.c`, `app/sd/record.c`, `case_reader.c` and `patch_cli.c` walk it; the old name functions keep their signatures; `test_name`, `test_record`, `test_case_reader`, `test_patch_cli`, `test_options`, `test_rom` passed unedited, e2e/e2e-combos unchanged.
- **Last settings** (`app/sd/last_settings.c`): `App/ALttPR/settings.json` through the record's now-public settings writer and reader (record output byte-identical), temp file then rename; load gives LOADED, MISSING (defaults, silent) or INVALID (defaults + a one-time notice). **The spoiler option is not remembered** (starts off each launch): the record's settings object has no spoiler key and the plan forbids a new parser.
- **Input filter** (`app/core/ui_input.h`): abstract `UiButton`s; Start/Select/Menu act only when released alone, any button pressed while one is held is swallowed, so none of keymon's chords (`docs/onion.md` §5) reaches the UI.
- **Generate tab** (`app/core/ui_generate.h`, `ui_settings.h`): 13 setting rows, "Spoiler in History", "Generate Seed"; Up/Down move (wrap), Left/Right change (wrap), A = Right or generate on the action, Y default, L2/R2 page, Start generates anywhere; B, Menu, L1/R1 returned as events. Every label and value has its own string id. Unsupported pairs grey both rows and the action ("Not supported with {}") and refuse Generate — tested with an injected pair.
- **Tests:** `test_settings`, `test_last_settings`, `test_ui_input`, `test_ui_generate` (14 snapshots at 40×15 — from an independent Python reimplementation of the layout rules, not the C output — every value rendered uncut, settings read back, restore from a file written by the SD layer, the invalid-file notice). `make unit-arm`: 38 unit tests + 12 fixture replays. Clean `make test`: 10.7 s.
- H-3.3 hypotheses held; H-3.3-6 (an enum-sized status table) does not exist yet: it comes with step 3.5.
