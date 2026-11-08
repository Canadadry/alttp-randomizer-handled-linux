# Notes for Claude sessions

*Zelda Randomizer on Miyoo*: an on-device randomizer seed generator for the
Miyoo Mini Flip running OnionOS, for now *A Link to the Past Randomizer* only.
An Onion app (`App/ZeldaRando/`, binary `zrando`), driven by the d-pad, reads
a Japanese 1.0 ROM, generates a seed with every alttpr.com setting (a C port
of VT, frozen at `sporchia/alttp_vt_randomizer`
`219fcafd029dab597b8db400efafd8f56f8b4edb`), and writes a patched `.sfc` that
Onion's SNES core plays; a History tab lists and deletes seeds. It also builds
for PC (SDL) so the owner can check the rendering. Start with the current plan
in `docs/`, then `README.md`.

## Layout

- `alttp/` is the randomizer library: `include/` its public headers, `src/`
  its sources. The generator, the oracle and their tests are taken over from
  `Canadadry/alttp-ds` (`rando/`, `tests/rando/`).
- `alttp/host/` is host-side code on top of the library: the case reader, the
  spoiler JSON, the playthrough, the output, and the generator's `main`
  (`host/main/rando_gen.c`).
- `ladx/` is LADX's C library (Phase 3): `include/` its public headers,
  `src/` its sources (`sha512.c`, `sha1.c`, `pyrandom.c`, CPython's `random.Random`;
  `rom.c`, the base check and LADXR's `ROMWithTables` tables read from the
  user's ROM, `ladx_rom.h`, tested by `test_rom` on `unit/synth_rom.c`'s
  synthetic ROM; `ks.c`, KS as `known_source.build`, `ladx_ks.h`, tested
  by `test_ks`, span list in `tests/ladx/README.md`, "KS span list";
  `settings.c`, LADXR's settings table, parsing, `validate`, short string
  and the unsupported values and pair with their reason ids; `items.c`,
  LADXR's item names; `requirements.c`, its requirement nodes and the
  explorer's inventory; the logic graph, its location kinds, dungeons,
  overworld and explorer, `ladx_logic.h`, tested by `test_logic`, contract
  in `tests/ladx/README.md`, "Unit tests and lint"; `rng.c`, the traced
  generator `ladx_rng.h`, one record per call to a caller's sink;
  `world_setup_randomize.c`, `WorldSetup.randomize` with `buildMaze`, and
  `randomizer.c`, `ladx_randomize`, both in `ladx_randomizer.h`, tested by
  `test_world_setup`; `itempool.c`, `ItemPool` with Python's dict order,
  `ladx_itempool.h`, tested by `test_itempool`; `placer.c`,
  `RandomItemPlacer`, `ladx_placer_*` in `ladx_randomizer.h`, tested by
  `test_fill`; `wlog.c`, the write-log builder, parser and projection
  `ladx_wlog.h` (tokens decoded by `wlog_token.c`, shared with
  `tables.c`), tested by `test_wlog`; `encoder.c`, the write-log
  encoder `ladx_encoder.h` (the C-group rule, KS-match, the fragment rule,
  table groups' atoms passed through), tested by `test_encoder`, which
  replays `encoder_vectors.txt` with `--fixtures`; `pipeline.c`, the
  walk of `order.txt` through the encoder with the registry of ported `C`
  groups, `ladx_pipeline.h`, tested by `test_pipeline` (contract in
  `tests/ladx/README.md`, "Pipeline"); `bank3e.c`, the `C` group
  `bank3e` (`ladx_groups.h`, its constants from `data/ladx/bank3e.txt`),
  tested by `test_bank3e`, claimed `rom-group` by `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline" and "Patch tables"; `title.c`,
  the `C` group `title` with `md5.c` and `background.c`
  (`BackgroundEditor`), tested by `test_title`, `test_md5` and
  `test_background`, claimed `rom-group` by `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline"; `marin.c`, the `C` group `marin`
  (Marin's starting text from `marin_lines.c`, LADXR's `marin.txt`
  embedded, and the shortened texts) with `text.c`, `formatText`
  (`ladx_text.h`), tested by `test_marin` and `test_text`, claimed
  `rom-group` by `e2e-rom` with its trace lines, its draws run without
  the ROM too, contract in `tests/ladx/README.md`, "Pipeline";
  `hints.c`, the `C` group `hints` (`addHints`), tested by `test_hints`,
  claimed `group` by `e2e` with its trace lines, contract in
  `tests/ladx/README.md`, "Pipeline"; `music.c`, the `C` group `music`
  (`randomizeMusic`), tested by `test_music`, claimed `rom-group` by
  `e2e-rom` with its trace lines, its draws run without the ROM too,
  contract in `tests/ladx/README.md`, "Pipeline"; `endscreen.c`, the `C`
  group `endscreen` (`updateEndScreen`, the drawn cat's image from
  `data/ladx/endscreen.txt`), tested by `test_endscreen`, claimed
  `rom-group` by `e2e-rom` with its trace line, its draw run without the
  ROM too, contract in `tests/ladx/README.md`, "Pipeline" and "Patch
  tables"; `notext.c`, the `C` group `notext` (`noText`, every text cut
  to its terminator), tested by `test_notext`, claimed `rom-group` by
  `e2e-rom`, contract in `tests/ladx/README.md`, "Pipeline"; `room.c`,
  LADXR's `RoomEditor` (`ladx_room.h`, for every group editing rooms),
  tested by `test_room`, contract in `tests/ladx/README.md`, "Room
  editor"; `goal.c`, the `C` group `goal` (the sign maze, the specific
  instruments and the instrument count, assembled bytes from
  `data/ladx/goal.txt`), tested by `test_goal`, claimed `rom-group` by
  `e2e-rom`, contract in `tests/ladx/README.md`, "Pipeline" and "Patch
  tables"; `item_patch.c`, the `C` group `items` (`setMultiChest` and
  every location's patch, assembled bytes from `data/ladx/items.txt`),
  tested by `test_items`, claimed `rom-group` by `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline" and "Patch tables"; `entrances.c`,
  the `C` group `entrances` (`changeEntrances`, every entrance room
  re-stored), tested by `test_entrances`, claimed `rom-group` by
  `e2e-rom`, contract in `tests/ladx/README.md`, "Pipeline"; `bosses.c`,
  the `C` group `bosses` (`changeBosses` and `changeMiniBosses`, assembled
  bytes from `data/ladx/bosses.txt`), tested by `test_bosses`, claimed
  `rom-group` by `e2e-rom`, contract in `tests/ladx/README.md`,
  "Pipeline" and "Patch tables"; `enemizer.c`, the `C` group `enemizer`
  (`randomizeEnemies`, its own traced generator, the sprite slots of
  `sprite_data.c`, `entityData.SPRITE_DATA`, tested by
  `test_sprite_data`), tested by `test_enemizer`, claimed `rom-group`
  with its trace by `e2e-rom`, contract in `tests/ladx/README.md`,
  "Pipeline" and "Patch tables"; `warp_home.c`, the `C` group
  `warp-home` (`warpHome`, assembled bytes from `data/ladx/warp-home.txt`),
  tested by `test_warp_home`, claimed `rom-group` by `e2e-rom`, contract
  in `tests/ladx/README.md`, "Pipeline" and "Patch tables"; `sprites.c`,
  the `C` group `sprites` (`updateSpriteData`, every room's sprite data,
  CPython's set order, patches from `data/ladx/sprites.txt`), tested by
  `test_sprites`, claimed `rom-group` by `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline" and "Patch tables"; `save.c`, the `C`
  group `save` (`ROMWithTables.save`'s eleven stores) on `store.c`,
  LADXR's `PointerTable` storage and `store` (`ladx_store.h`), tested by
  `test_save` and `test_store`, which replays `store_vectors.txt` with
  `--fixtures`, claimed `rom-group` by `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline" and "Store fixture"; `header.c`, the
  `C` group `header` (`ROM.fixHeader`, the name and both checksums over
  the encoder's bytes, `ladx_header_fix`), tested by `test_header`, which
  replays `header_vectors.txt` with `--fixtures`, claimed `rom-group` by
  `e2e-rom`, contract in `tests/ladx/README.md`, "Pipeline" and "Header
  fixture"; `spoiler.c`, the `C` group `spoiler` (`SpoilerLog` read back
  from the encoder's state, `outputJson`, `ladx_spoiler.h`; `ladx_gen
  --rom` writes `spoiler.json`), tested by `test_spoiler`, claimed by
  `claims.txt`'s `spoiler` line under `e2e-rom`, contract in
  `tests/ladx/README.md`, "Pipeline", "Spoiler" and "E2E driver and
  claims"; `tables.c`, the patch tables' reader, key, walk, records
  and applier, `ladx_tables.h`, tested by `test_tables`, contract in
  `tests/ladx/README.md`, "Table application"; `assemble.c`,
  `ladx_assemble` (`ladx_assemble.h`: the base checked and parsed, the
  fill, the pipeline, the image read out and its header fixed, on
  caller-owned memory), tested by `test_assemble`, contract in
  `tests/ladx/README.md`, "Assembly"; contract in
  `tests/ladx/README.md`, "Unit tests and lint");
  pure C99 like `alttp/`, built with only `ladx/include` on the include path,
  never including `alttp/` or `app/`. `ladx/host/` is host-side code on top
  of it (C99, stdio allowed, `ladx/include` and itself on the include path):
  `run_text.c`, `run.txt`'s line renderer with CPython's `float.hex`
  (contract in `tests/ladx/README.md`, "Unit tests and lint"), and
  `inputs.c`, the mains' inputs (`ladx_inputs.h`: a case directory, a
  tables directory, a whole file); `main/ladx_gen.c`, the host generator
  `ladx_gen` (`randomizer.c`'s `ladx_randomize` run on a case, its
  `run.txt` written, `--rom` through `ladx_assemble`; contract in
  `tests/ladx/README.md`, "Generator `ladx_gen`"); and
  `main/ladx_patch.c`, the CLI `ladx_patch --base <file> --out <file>
  [settings]` (`rom-check`'s generator; contract in
  `tests/ladx/README.md`, "Generator `ladx_patch`").
- `app/` is the Onion app's own code. `app/core/` is the UI core: pure C99
  like the library, built with only `alttp/include`, `ladx/include` and
  itself on the include path, no SDL; the app opens on its game-choice
  screen (`ui_games.c`), A opens a game (its Generate and History tabs:
  ALttP's `ui_generate.c`, LADX's `ui_ladx_generate.c`, its rows from the
  library's settings table under category headings, unsupported values
  greyed with their reason, L2/R2 jumping categories, labels in
  `ui_ladx_settings.c`; one flow `ui_genflow.c` and one `ui_history.c` for
  both), B goes back to it, and B there quits, so every key sequence of a
  test that means ALttP starts with `a` (LADX: `down a`, or `a` once LADX
  is remembered). `app/sd/` is the SD-card layer
  (`record.c`, `seed_save.c`); its paths live in `rando_seed_save.h`: the
  last game opened in `App/ZeldaRando/settings.json` (`last_game.c`), and
  per game ALttP's `base.sfc`, `base.bps`, `history/` and `settings.json` in
  `App/ZeldaRando/alttp/`, its ROMs in `Roms/SFC/ZeldaRando/`; LADX's
  `base.gbc`, patch tables `tables/` (`data/ladx/`), `history/` (records
  `<code>.json`, spoilers `<code>.spoiler.json`) and `settings.json` in
  `App/ZeldaRando/ladx/`, its ROMs in `Roms/GBC/ZeldaRando/`, its saves under
  `Gambatte`. LADX's side: `ladx_gen.c` (base check and runner, its ~25 MiB
  work area allocated per generation and freed), `ladx_name.c`
  (`<Logic>-<Goal>-<CODE>.gbc`), `ladx_record.c`, `ladx_history.c`,
  `ladx_last_settings.c` (the last LADX settings, saved at each
  generation); both games delete through `seed_files.c`; `app_shell.c`
  hands the core both games' runners and History stores.
  `app/sdl/` is the SDL 1.2 renderer and input layer (key map, grid view,
  theme renderer `sdl_render.c`, frame loop). `app/core` uses the library;
  `app/core` also reads `ladx/include`'s settings table;
  `app/sd` both libraries, `alttp/host`, `ladx/host` and `app/core` (it
  hands the core its runners and stores; `tests/alttp` builds `ladx/` into
  every app link, `-ffp-contract=off` kept); `app/sdl` the library, `app/core` and `app/theme`;
  nothing in `alttp/` or `ladx/` includes `app/`. Its tests live
  in `tests/alttp/unit/`, the SDL ones in `tests/alttp/sdl/`.
  `app/theme/` is the theme loader (`theme.ini` into a `Theme` struct):
  pure C99, no SDL, built with only itself on the include path.
  `app/main/` holds the mains: `ui_cli.c`, the host tool that plays a
  key sequence into the app on a directory used as the SD card and prints
  the text screen; and `sdl_main.c`, the SDL app for the device and the PC.
  `app/package/` holds the Onion app folder's files (`launch.sh`,
  `config.json`, `icon.png`) and `build_zip.py`, which writes the release zip
  (self-test `tests/alttp/test_package.py`).
- `themes/` holds one flat folder per theme (`themes/classic/`: the phase-1
  grid look, kept as the SDL tests' theme; `themes/alttp/`: the shipped look,
  the mockups' look at 12 px in Press Start 2P,
  `frame.png` and thirteen sprite PNGs made from the mockups by
  `make theme-art-write`).
- `data/` holds the fonts' source (DejaVu Sans Mono and Press Start 2P, each
  with its licence; the themes ship byte copies, checked by `test_theme`) and
  the base patch `base.bps`. Press Start 2P is `google/fonts`
  `ofl/pressstart2p/` at `24ddc1f324cac5d9f5aedca126d298f56e3f1640`, renamed.
  `data/ladx/` holds LADX's patch tables (`order.txt` and one `<group>.txt`
  per `table` group of the inventory, plus `bank3e.txt`, `bosses.txt`,
  `endscreen.txt`, `enemizer.txt`, `goal.txt`, `items.txt`, `sprites.txt` and `warp-home.txt`, the `C` groups `bank3e`'s,
  `bosses`', `endscreen`'s, `enemizer`'s, `goal`'s, `items`', `sprites`' and `warp-home`'s constants files, `endscreen.txt` holding
  LADXR's 24 cat images; format, units, keys, entries and the
  order walk in `tests/ladx/README.md`'s "Patch tables"), written only by
  `gen_tables.sh`, never by hand.
- `tests/alttp/` holds its tests: `unit/test_<module>.c`, one binary per
  module, registered in `tests/alttp/Makefile`'s `UNIT_TESTS`, failing through
  `unit/support.h`'s `test_fail`.
- `tests/ladx/unit/` holds LADX's unit tests, `test_<module>.c`, registered
  in `tests/ladx/Makefile`'s `UNIT_TESTS`, failing through its own
  `unit/support.h` (no `alttp/` header).
- `tests/alttp/cases/` holds the e2e cases, one directory each with its VT
  golden; `tests/alttp/oracle/` the PHP VT oracle (`gen_golden.sh`, `vt.patch`)
  and the fixtures it writes.
- `tests/ladx/oracle/` holds the LADX oracle: LADXR pinned at `4325207e`,
  cloned (never vendored) into `tests/ladx/oracle/ladxr/` (git-ignored) and
  patched with `ladxr.patch` (order fixes and environment fixes, one commit
  each); the Python scripts that run it (shared helpers `ladx_oracle.py`, unit-tested by
  `test_oracle.py`); Phase 2's ROM-free tools, each unit-tested by its
  `test_<tool>.py` (`nintendo_bytes.py`, the Nintendo-bytes check;
  `push_gate.py`, Phase 2's push gate; `read_trace.py`, the ROM read
  and write trace; `inventory_check.py`, the grep-diff and table check of
  `notes/ladx-inventory.md`, Phase 2's inventory of `generateRom` settled
  on real data; `read_check.py`, the check of real read traces against it;
  `tables.py`, the format of the patch tables `data/ladx/` (Phase 2 step
  6: `order.txt` and one `<group>.txt` per `table` group, documented in its
  docstring; `parse`, `render`, `select` and `check`); `key_trace.py`, the
  trace of the settings and assembler constants each row reads;
  `gen_tables.py`, the tables' planning and build (units fused from the
  coverage runs' encodings, keys, reachable tuples, coverage runs, entries
  cut from each run's encoding), its rules in its docstring; its `build`
  command, with the ROM, is `gen_tables.sh`'s worker; `table_replay.py`,
  the tables' host replay against the goldens; `known_source.py`, the
  C-knowable source map KS built from the ROM, only with the ROM present
  (`tests/ladx/README.md`, "Encoding rule"; `--spans <file>` writes its
  span list, "KS span list"); `rom_check.py`, the full-ROM check's driver (Phase 2 step 9;
  its generator contract in `tests/ladx/README.md`, "Full-ROM check"),
  `fetch_rom.sh`, the ROM fetcher, proved by its own run, and
  `inventory_trace.sh`, the inventory's real-data driver; the
  hand-written inputs `tier1.txt`,
  `determinism_cases.txt`, `side_seeds.txt`, `order_audit.txt`,
  `table_keys.txt` (key widenings, `<call>\t<component>\t<reason>`) and
  `table_units.txt` (forced fusions, `<call>\t<call>\t<reason>`); and the
  six fixtures they
  write for the C port (`pyrandom_vectors.txt`, `settings.txt`,
  `settings_validate.txt`, `logic_graphs.txt`, `logic_vectors.txt`,
  `item_pools.txt`), each describing its format in its `#` header.
  `notes/plan3-scope-cuts.md` lists every tier-1 value cut from plan 3
  with its reason; `tier1.txt`'s header names the cut values it leaves
  out (`goal=bingo*`, cut `goal-bingo`). `tests/ladx/cases/` and
  `tests/ladx/combos/` hold LADX's cases (Phase 2 step 4), one
  directory each, named from its settings (`<key>-<value>`, joined with `+`
  in LADXR's order; `baseline`; a sample combo `sample-<nn>`): `config.txt`, one `key=value` line per
  non-default setting in LADXR's order (empty for the baseline), and `seed`,
  32 lowercase hex digits, `derive(name, k)` (the first 16 bytes of SHA-256
  of `ladx-case <name> <k>`, `k < 64`). Their expected list is derived by
  `ladx_cases.py` (unit-tested by `test_ladx_cases.py`) from `tier1.txt`
  (the baseline and every non-default value alone), the hand-written
  `combos.txt` (`cross <keyA> <keyB>` and `case <k=v> ...` lines; a cross
  cell that validate rewrites, per `settings_validate.txt`, is skipped and
  named; `refused <k=v>`, a value LADXR refuses on every seed alone, has no
  single case and must be in a combo: `boomerang=default`, covered by
  `accessibility-goal+boomerang-default`; `sample <nn> <k=v> ...`, the
  random sample of Phase 2 step 6, folder `combos/sample-<nn>`, 12 lines
  written once by `gen_tables.sh --sample`, never re-drawn) and the
  `settings.txt` fixture.
  Each seed is the first candidate `k` that clears the strict
  Nintendo-bytes check and gives `outcome ok` with the ROM (`ladx_cases.py
  pick`, re-proven by `verify`). Each folder also holds its goldens,
  written by `gen_golden.sh` (Phase 2 step 5): `run.txt`, `spoiler.json`
  (absent on `race` cases) and `rom_writes.txt`, the write log (format v1,
  every source byte a reference, its encoding rule, KS, `I` lines,
  projection and decode documented in `tests/ladx/README.md`, the
  reference for Phase 3's C logger).
  `tests/ladx/Makefile` holds LADX's own targets (`help`, the default;
  `test`, `lint` and `logic`, run by the root `make test`, `make lint`
  and `make logic`;
  `selftest`, `e2e` and `e2e-combos`, run by the root ones after
  `tests/alttp`'s: LADX's e2e driver `run_e2e.py`, its claims
  `claims.txt` (a stage and the write-log groups claimed, each through its
  `rom_writes.txt` projection; `ladx_gen --tables data/ladx` writes C's),
  contract in `tests/ladx/README.md`, "Stage projection" and
  "E2E driver and claims";
  `unit-arm`, `measure` (the run stopped after the fill and, with the
  ROM, the whole pipeline timed on every case, projected onto the
  Cortex-A7), `measure-arm` (the same, plus Onion's `ladx_patch` under
  `qemu-arm`; contract of both in `tests/ladx/README.md`, "Measure"),
  `rom-check`, `rom-check-selftest`, `e2e-rom`,
  `synthetic-refs`, outside the net; `e2e-synthetic`, in the net); Phase 3
  adds its targets there. `tests/ladx/oracle/fill_outcomes.txt`, LADXR's
  fill outcomes on seeds that are not case seeds, is written only by
  `gen_fill_outcomes.py` (`--check` re-renders it), replayed by
  `test_fill --fixtures`; contract in `tests/ladx/README.md`,
  "Fill-outcome fixture". `tests/ladx/oracle/encoder_vectors.txt`,
  `write_log.py`'s encoder on synthetic event streams over a synthetic ROM
  and KS (no ROM), is written only by `gen_encoder_vectors.py` (`--check`
  re-renders it; tested by `test_gen_encoder_vectors.py`), replayed by
  `test_encoder --fixtures`; contract in `tests/ladx/README.md`,
  "Encoder fixture".
  `tests/ladx/oracle/store_vectors.txt`, LADXR's own `PointerTable` on
  synthetic tables (no ROM), is written only by `gen_store_vectors.py`
  (the LADXR checkout; `--check` re-renders it; tested by
  `test_gen_store_vectors.py`), replayed by `test_store --fixtures`;
  outside the net; contract in `tests/ladx/README.md`, "Store fixture".
  `tests/ladx/oracle/header_vectors.txt`, LADXR's own `ROM.fixHeader` on
  synthetic banks (no ROM), is written only by `gen_header_vectors.py`
  (the LADXR checkout; `--check` re-renders it; tested by
  `test_gen_header_vectors.py`), replayed by `test_header --fixtures`;
  outside the net; contract in `tests/ladx/README.md`, "Header fixture".

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
- `make test` = `make -C tests/alttp test` (the randomizer's and the app's
  unit tests, linked with the app archive `librando_app.a`; the UI snapshots
  in `tests/alttp/unit/snapshots/`, edited by hand only; and
  `tests/alttp/no_sdl.sh`, which fails on any SDL include under `app/core/`,
  `app/sd/`, `app/theme/` or `app/main/` (`sdl_main.c` exempt by exact path) after proving
  itself on planted ones; and `tests/alttp/no_literals.py`, which fails on
  any string literal with a letter under `app/core/` outside the English
  table `ui_strings_en.c`, or under `app/sdl/`, after proving itself on
  planted sources; and `ui_cli`, built from `make test`'s
  own objects, with its smoke test `unit/smoke_ui_cli.c`) plus `test-ladx`
  plus `test-template` (the tooling under `.claude/`: the pre-push hook,
  plan-pilot's `progress.md` hook and `check-e2e`'s failure ranker
  `rank_e2e.py`, contract in its `SKILL.md`, "The ranker") plus `rom-guard`
  (`tests/rom_guard.py`, git and python3 only): it fails, one
  `rom_guard: <path>: <reason>` line per hit, when git's index (the staged
  blob, not the working tree) holds a `.gb`, `.gbc`, `.sfc` or `.smc` path in
  any case, or a blob that is the LADX English 1.0 ROM (SHA-1) or the ALttP
  JP 1.0 ROM (MD5, with or without a copier header), whatever its name; it
  proves itself first on planted files in throwaway repositories; no git or
  no work tree gives a `setup:` line; else `rom_guard: <n> tracked files, no
  ROM`; plus `ladx-tables`.
- `make test-ladx` (`make -C tests/ladx test`, run by `make test`): LADX's
  unit tests on the host; contract in `tests/ladx/README.md`, "Unit tests
  and lint".
- `make test-ladx-oracle` (`unittest discover` of
  `tests/ladx/oracle/test_*.py` under `python3 -I`: the LADX oracle's helpers
  and Phase 2's tools, `rom_check.py`'s self-test included, no LADXR, no
  ROM, no network; run by `make check`, kept out of `make test`).
- `make ladx-cases` (`python3 -I tests/ladx/oracle/ladx_cases.py check`,
  no ROM, no LADXR, no network; run by `make check`, kept out
  of `make test`): it
  proves itself on planted trees (a missing and an extra case, a sample
  combo's folder under its settings' name, a config order swap and a wrong value, a seed not from the derivation, an
  uppercase seed, an extra file, a name in both directories, a missing
  golden, a spoiler on a race case, a malformed write log, a log over
  budget), then compares `tests/ladx/cases/` and `tests/ladx/combos/` with
  the expected list and checks each folder's goldens: `run.txt`,
  `rom_writes.txt` (parsing, version line included) and `spoiler.json`
  exactly when `race` is off, and the size budget (`rom_writes.txt` 224 KiB
  per case and 22 MiB in all, all golden files 34 MiB); one `MISSING`,
  `EXTRA`, `WRONG`, `BADSEED`, `STRAY`, `BOTH`, `NOGOLDEN`, `BADGOLDEN` or
  `BUDGET` line per mismatch (exit 1), else `ladx-cases: goldens in <n>
  folders, rom_writes.txt <b> bytes (limits ...), golden
  files <t> bytes (limit ...)` and one `skip <name>:
  validate sets <k=v>` line per skipped cell, one `refused <name>: LADXR
  refuses every seed, covered by <combo>...` line per refused single and
  `ladx-cases: <n> cases, <m> combos, <s> cells skipped by validate[, <r>
  singles refused by LADXR], every tier-1 value and cross cell covered`; a
  malformed input exits 2.
  `ladx_cases.py list [<name>...]`
  prints the case directories as `ladx_oracle.py cases` prints a case list,
  after the same comparison.
- `make ladx-tables` (`python3 -I tests/ladx/oracle/tables.py check [--root
  <dir>] [--inventory <file>] [--inputs <dir>]`, no ROM, no LADXR, no
  network; run by `make test`) proves itself on planted trees, then checks the tables under `<root>`
  (default `data/ladx/`) against the inventory, `tier1.txt` and
  `settings.txt`: every file parses, `order.txt`'s rows are the inventory's
  `table` and `C` rows in its section order with their sides and groups,
  one file per `table` group and no other, every `table` row in one unit
  (numbered in first-row order, a unit's rows consecutive among the table
  rows), no duplicate tuple, every value in its component's domain
  (tier-1 values, a setting outside tier 1 its default, `asm:` constants
  0/1, `count` decimal); one `MISSING`, `EXTRA`, `BADFILE`, `BADROW`, `DUP`
  or `BADKEY` line per mismatch (exit 1), else `ladx-tables: <u> units in
  <g> groups, <e> entries, <s> bytes`; a malformed input exits 2.
- The LADX oracle, outside the net (it needs git, the network for GitHub and
  python3 >= 3.11, else a `setup:` line; no ROM): `tests/ladx/oracle/setup.sh
  [--fresh]` clones LADXR at its pin into `LADXR_DIR` (default
  `tests/ladx/oracle/ladxr`), applies `ladxr.patch` and prints `setup: ladxr
  <commit>, python <x.y.z>[, ladxr.patch applied]`, warning when Python's
  minor version is not the fixtures' one. `gen_fixtures.sh [--out <dir>]`
  rewrites the six fixtures; `gen_fixtures.sh --check` renders them twice
  (`PYTHONHASHSEED=0` on the setup checkout, `=1` on a fresh clone under
  `tmp/ladx-oracle/`) and prints one `DIFFERENT`, `MISSING` or `EXTRA` line
  per file unlike the other run or the committed one, else `fixtures: 6 files
  byte-identical on two runs and to tests/ladx/oracle`. `determinism.sh`
  runs `determinism_cases.txt` through LADXR in `SKIP_ROM_GENERATION` mode
  (`oracle_run.py`: world setup, pools, placement, RNG trace) under the same
  two conditions, after proving its comparator, and prints `same <case>` or
  `DIFFERENT <case>: line <n>: ...` per case, then `determinism: <n> cases
  identical (...)`, or fails when the two runs did not vary.
  With the ROM (`tmp/ladx/base.gbc`, SHA-1 `d90ac17e…`, else a `setup:`
  line): `gen_golden.sh [--out <dir> | --root <dir>] [--mirror <dir>]
  [--cases <file>] [--ladxr <dir>] [--hashseed <n>] [--rom <file>] [--work
  <dir>] [<case>...]`, LADX's case runner, runs each case (default the case
  directories, `<root>/cases/` then `<root>/combos/`, through
  `ladx_cases.py list`; with `--cases`, `--out` only, that list file, e.g.
  `determinism_cases.txt`) through the whole generator (`oracle_run.py
  --rom --reads`, one process per case, LADXR in a new directory under the
  work root; about 15 s per case) and encodes each ok case's write log
  (`write_log.py encode`, its `I` lines from the run's `invariant.txt`).
  With `--out` it writes `<dir>/<case>/`:
  `run.txt` (the sections above, the trace covering `generateRom` and the
  spoiler, then `rom_sha1`, `spoiler_sha1` and the outcome), `meta.txt`,
  `events.txt`, `invariant.txt`, `reads.txt`, `writes.txt`, and `out.gbc`, `spoiler.json`
  and `rom_writes.txt` when written. Without `--out` it writes the
  goldens: every file into `<mirror>/<case>/` (default
  `tmp/ladx-goldens/`), then only `run.txt`, `rom_writes.txt` and
  `spoiler.json` into the case folder under `<root>` (default
  `tests/ladx`; else inside `tmp/`), replacing its old ones, a case not ok
  left untouched (`gen_golden: <n> goldens written to <root>`). It refuses
  an `--out` or `--mirror` outside `tmp/` (exit 2): ROM-derived outputs
  stay there. Without `--cases` it exits 1 after its summary with
  `gen_golden: <n> cases not ok` when a case's outcome is not `ok` or its
  log does not encode; with `--cases` any outcome passes.
  `python3 -I tests/ladx/oracle/golden_check.py [--root <dir>] [--mirror
  <dir>] [--rom <file>] [--ladxr <dir>] [--inventory <file>] [<case>...]`
  verifies the goldens with the ROM, against the mirror's event streams:
  per case `decode` (the log replayed onto the ROM gives `rom_sha1`),
  `encode` (the golden is its stream's encoding, `I` lines from the
  mirror's `invariant.txt`), `tags` (every group a
  `table` or `C` group of the inventory), `scrambled` (no C group's
  projection changes with every byte outside the C-knowable map
  scrambled), `projection` (dropping or changing another group's records
  never changes a group's projection), `nintendo` (the Nintendo-bytes
  policy of a case folder, 0 literal runs) and `fragments` (no literal
  piece, of any length, of a `B` or `T` record occurs in the KS bytes it
  replaces: `<group> <n> fragments, <b> bytes, first at line <l>`); `ok
  <case>` or `BAD <case>: <check>: <detail>` lines, with no case named the
  union of the goldens' groups against the inventory's writing groups less
  `spoiler` (`BAD tags: ...`), then `golden-check: <n> cases, <b> bad, <k>
  literal runs, <f> fragments`; about 13 s per case.
  `oracle_run.py --rom --dir <dir> --reads [--inventory <file>]` (off by
  default; `run.txt`, `out.gbc` and `spoiler.json` unchanged by it) also
  traces the whole run with `read_trace.py` and writes `reads.txt`,
  `writes.txt`, `events.txt` and `invariant.txt` into `<dir>`, whatever the
  outcome: `invariant_run.py` re-runs each C-group call of `generateRom`
  that writes a computed byte on a scrambled copy of the ROM state (bytes
  kept: never-written C-knowable source bytes and bytes earlier writes
  computed; RNG state put back; `patch`'s old-byte check off on the copy)
  and flags, per event byte, whether the copy holds the same byte.
  With `--reads`, `--keys` also installs `key_trace.py` over the same
  sections and writes `keys.txt`, and `--no-invariant` skips the re-run
  (no `invariant.txt`); the other files are the same with or without
  either.
  `gen_tables.sh [--out <dir>] [--ladxr <dir>] [--rom <file>]` (with the
  ROM and the checkout at the pin, else a `setup:` line, exit 2) builds
  LADX's patch tables into `data/ladx/` (default; any other `<dir>` must
  be inside `tmp/`, else exit 2): `gen_tables.py build`,
  `PYTHONHASHSEED=0`, plans coverage runs on the keys the runs so far give
  and runs them until a plan asks for no new one, each `oracle_run.py
  --rom --reads --keys --no-invariant` under `tmp/ladx-tables/runs/<run>/`
  on the first ok candidate seed `derive("ladx-tables <run>", k)`, `k <
  64` (`<run>: k=<k> <k=v>...`, or `<run>: no ok seed below 64 ...`, after
  which its merged witnesses are re-planned alone); it encodes each run
  with `write_log.py`'s encoder, cuts each unit's entry out of its segment
  and writes the tables, else prints one `CONFLICT <unit> <tuple>`,
  `UNREACHED <unit> <tuple>`, `DEPENDS-C <unit> <group>`, `FUSE-ACROSS
  <unit> <row>` or `SPLIT <unit>` (a unit writing in two segments of one
  run) line each and exits 1, writing nothing; else `gen_tables: <n>
  coverage runs in <s> s, <f> files written to <dir>`. Keys come from the
  inventory, the traced setting and constant reads, `table_keys.txt` and
  the keys of the earlier units that wrote inside a range the unit read
  and of the later units of its segment that rewrote its bytes (rows
  fuse only on a source match spanning them).
  `gen_tables.sh --check [--rom <file>]` rebuilds them into
  `tmp/ladx-tables/check/` with `PYTHONHASHSEED=1` on a fresh clone
  (`tmp/ladx-tables/ladxr-check/`, the network needed) and prints one
  `DIFFERENT`, `MISSING` or `EXTRA` line per file unlike `data/ladx/`
  (exit 1), else `tables: <n> files byte-identical to data/ladx`.
  `gen_tables.sh --const [--out <dir>]` (with the ROM, outside the net)
  rewrites only the `C` groups' constants files (`bank3e.txt`,
  `bosses.txt`, `endscreen.txt`, `enemizer.txt`, `goal.txt`, `items.txt`, `sprites.txt` and `warp-home.txt`; contract in `tests/ladx/README.md`, "Patch tables"),
  about 70 seconds.
  `gen_tables.sh --sample [--ladxr <dir>]` (the checkout at the pin, no
  ROM) appends the 12 `sample <nn> k=v ...` lines to `combos.txt`
  (`gen_tables.py sample`: line `i` takes each setting's tier-1 value at
  SHA-256(`ladx-sample <i> <key>`) big-endian mod its value count, through
  LADXR's live validate, non-default values only; a draw left with
  `boomerang=default` and `accessibility=all` is redrawn with ` #<j>`
  appended to every hashed text) and refuses (exit 2) a `combos.txt` that
  already has them: the sample is drawn once. A sample combo failing
  `table_replay.py` means a key misses a dependency: widen it in
  `table_keys.txt` and regenerate the tables.
  `python3 -I tests/ladx/oracle/table_replay.py [--root <dir>] [--mirror
  <dir>] [--rom <file>] [--tables <dir>] [--ladxr <dir>] [--inventory
  <file>] [--inputs <dir>] [<case>...]` (with the ROM, the LADXR checkout
  and the mirror's event streams, else a `setup:` line, exit 2) proves
  itself on planted tables, goldens and streams (a flipped table byte, a
  copy token swapped for the same bytes, an entry under another key, a
  table holding a fragment; `table-replay: self-test, 5 planted replays
  as expected`), scans every `B` and `T` record of the tables for
  fragments (one `BAD tables: fragments <file> line <n>: <k> fragments,
  <b> bytes` line each, as `golden_check.py`'s `fragments`), then, per
  case and combo (default every one), walks `order.txt` for the case's key
  (`config.txt` validated by LADXR, constants and the spot count through
  `gen_tables.py`'s model), applying each unit's entry at its first row and,
  at a `C` row, the golden's `C` segments whose first write that row made
  (segments matched one to one with `<mirror>/<case>/events.txt`); prints
  `ok <case>` or one `BAD <case>: sha1|projection <group>|key <unit>|align:
  <detail>` line per problem (`sha1`: the replayed log's decoded SHA-1 is
  not `run.txt`'s `rom_sha1`, with the first differing offset and its last
  writer in the replay and the golden; `projection`: a table group's
  projection differs from the golden's), then `table-replay: <n> cases, <b>
  bad, <f> table fragments`, exit 1 on a bad one or a fragment; about 1 minute for 128 cases. Never prints a
  ROM byte.
  `inventory_trace.sh [--out <dir>] [--cases <file>] [--seeds <file>]
  [<case>...]` runs every case that way (default: the case folders, like
  `gen_golden.sh`, then, when no case is named, `determinism_cases.txt`'s
  as `det-<name>`; `--cases <file>`: that list only)
  under its own seed into `tmp/ladx-inventory/<case>/seed-0/` and under each
  seed of `side_seeds.txt` into `seed-<k>/` (one `<case> seed-<k>: outcome
  <ok|error|crash>` line each; about 50 min for the 405 runs), refuses a `<dir>`
  outside `tmp/`, then runs `read_check.py` on it and exits with its code.
  `determinism.sh --full` proves its comparators on planted case
  directories, runs `gen_golden.sh` twice (A: `PYTHONHASHSEED=0`, the setup
  checkout, inside this repository; B: `=1`, a fresh clone, its work root a
  throwaway git repository holding a tag) and LADXR's own CLI once per case
  (`oracle_run.py --plain`: `main.main`, no hook, `os.urandom` handing it
  the seed; its retry is the driver's `error`), compares every file (sizes,
  differing-byte counts and offsets, never a byte value) and prints `same
  <case>`, `DIFFERENT <case>: <file>: ...` or `DIFFERENT <case> (plain):
  ...` per mismatch, then `determinism: <n> cases identical with the ROM
  (...), driver equal to LADXR's CLI`; it fails when the runs did not vary
  in hash seed, checkout and the work root's `git describe`, and on a new
  untracked file (`STRAY <path>`). `determinism.sh --full` runs
  `determinism_cases.txt` (passed to `gen_golden.sh` as `--cases`; about
  9 minutes); `determinism.sh --full --all [<case>...]` runs every case directory
  (only the named ones when given) the same way (about 55 minutes for 116
  cases, 6 minutes for the 12 `sample-<nn>`), `rom_writes.txt` included.
  `ladx_cases.py pick [<name>...]` (with the ROM and the checkout at the
  pin, else a `setup:` line, exit 2) writes each expected case that has no
  directory yet (a named one must not exist: it never touches a case; to
  re-pick, delete the directory by hand) after checking with the live
  `Settings.validate` that it keeps the case's values: for `k = 0…63` the
  candidate `derive(name, k)` is refused as `nintendo-bytes` (its seed file
  fails the strict check) or by its outcome (`oracle_run.py --rom`,
  `PYTHONHASHSEED=0`, under `tmp/ladx-cases/`), and the first accepted one
  is written; one `<name>: seed k=<k> (<r> refused: k=<j> <reason>; ...)`
  line per case, then the refusal counts by kind; exit 1 when a case has
  none below 64. `ladx_cases.py verify [<name>...]` checks the tree, then
  re-runs each case's candidates `0…k` (`verified <name> k=<k>`, else
  `UNVERIFIED <name>: ...`) and every candidate of each refused single
  (`refused <name>: 64 candidates not ok (...)`), then `ladx-cases: <n>
  seeds verified first-ok, <r> refused singles not ok on any candidate`.
  Each takes about 3 minutes.
  `python3 tests/ladx/oracle/audit_order.py <ladxr dir>` checks every
  directory listing, set and environment read (subprocess, environment
  variables, clock, working directory, `os.urandom`, platform and Python
  version) of LADXR's sources against `order_audit.txt` (verdicts
  `patched`, `unreached`, `order-free`, `inert`; one `UNAUDITED` or `STALE`
  line each, else `order audit: <n> sites, all audited`). `extract_logic.py --graph <config> --out <dir> <ladxr dir>`
  writes one configuration's full logic graph, to diff against a
  `logic_vectors.txt` digest. Run every script with `python3` without `-I`
  or `-E` (both drop `PYTHONHASHSEED`).
- The Nintendo-bytes check, outside the net (python3 and git):
  `python3 -I tests/ladx/oracle/nintendo_bytes.py [--rom <file>] [--ladxr
  <dir>] [--inventory <file>] [--scope strict|general|writes|tables] [--min <n>]
  [--verbose] <file-or-dir>...` proves itself first on planted files against
  a planted synthetic ROM, corpus and inventory, one per scope, view and
  clearance (`nintendo-bytes: self-test, 62 planted files as expected`, else
  `self-test failed: <case>`, exit 1). Then
  it prints one `LITERAL <path> <view> @<offset> <length> bytes <scope>`
  line per uncleared run of a file that also occurs in the ROM (`--rom`,
  default `tmp/ladx/base.gbc`), never the bytes, then `nintendo-bytes: <n>
  files, rom <sha1>, ladxr <short>, <k> literal runs, cleared <t> trivial,
  <l> low-info, <p> ladxr-public, <c> computed, <i> invariant`; exit 1
  when `k > 0`. Scope
  by repository-relative path: every file under `data/ladx/` (the patch
  tables) is `tables`; in a case folder
  (`tests/ladx/cases|combos/<case>/`) `run.txt` and `spoiler.json` are
  `general` and `rom_writes.txt` is `writes`; else `strict` under
  `tests/ladx/cases/` and `tests/ladx/combos/` (the ROM-derived
  artifacts), `general` for any
  other path or a file outside the repository (`--scope` overrides). Scope
  `writes`: the file must parse as a write log (else one `writes ...
  unparsed` literal), its literal tokens are checked at 4 bytes at their
  byte alignment (view `writes`), the rest of its text at the general
  thresholds. Scope `tables`: the file must parse with `tables.py`'s
  parser (else one `tables ... unparsed` literal), the literal tokens of
  its `B` and `T` records are checked at 4 bytes at their byte alignment
  (view `tables`) with neither the `computed` nor the `invariant`
  clearance, the rest of its text at the general thresholds. This is a
  deliberate change of step 2.1's strict rule for `data/ladx/` (Phase 2
  step 6): measured on table-group records (~100 KB, 4 goldens' worth),
  the strict scope gave 107 literal runs, all record syntax coinciding
  with ROM bytes (104 raw-view, like `T texts 7d` against ROM text, 3
  hex-view at the other nibble alignment), none ROM content. Views:
  `raw`; `hex` (maximal hex-digit runs, both nibble alignments); `bytelist`
  (lists of `0xNN`/`$NN` tokens separated by commas and whitespace).
  Minimum contiguous match: strict 4 bytes in every view; general raw 16,
  hex and bytelist 8. Cleared runs are counted, listed by `--verbose` as
  `CLEARED <path> <view> @<offset> <length> bytes <category>`: `trivial`
  (an arithmetic progression mod 256, constant runs included), `low-info`
  (at most 3 distinct values, general scope only), `ladxr-public` (the
  whole run occurs in a text file, valid UTF-8 without NUL, tracked by the
  LADXR checkout at `4325207e`, or in its decoded hex-digit runs; binary
  files left out; in the general scope's raw view only, also a run whose
  ASCII spaces stripped at both ends leave a run occurring whole in those
  files or one shorter than the raw threshold, 16; a deliberate change of
  plan 3 step 4.2, same reasoning as step 2.1's scope split: the UI
  snapshots pad LADXR's own public setting labels with the grid's spaces,
  which carry no ROM information; never in the strict, `writes` or
  `tables` scope; the self-test plants a padded public label (cleared), a
  padded short word (cleared), a padded non-public ROM run of 33 bytes and
  the padded label under a strict path (both refused)), `computed` (view `writes` only: the run occurs nowhere
  whole in the source bytes its token's group reads, from the inventory's
  `Reads (trace)` ranges, `--inventory`, default `notes/ladx-inventory.md`,
  `written:`/`mixed:` followed to the writers, `computed` coverage rows
  left out; a missing inventory is a `setup:` line when a run needs it),
  `invariant` (view `writes` only: every byte of the run lies in its
  record's `I` ranges, the bytes the oracle's re-run on a scrambled copy
  gave unchanged, and the run is not C-knowable source bytes of
  `known_source.py`).
  `--min <n>` is an experimental override of every
  threshold, shown as `min <n> (override)` in the summary, never used by
  the gate or a plan check. Directories are walked sorted, `.git` skipped.
  A missing ROM or input, or no LADXR checkout at the pin (`--ladxr`,
  default `LADXR_DIR` or `tests/ladx/oracle/ladxr`) is a `setup:` line,
  exit 2.
- The ROM fetcher, outside the net (curl and `sha1sum` or `shasum`, else a
  `setup:` line): `tests/ladx/oracle/fetch_rom.sh [<url>]` takes the URL
  from its argument, else `LADX_ROM_URL`, never from a file, and never
  writes or echoes it. It proves itself first, silently, on a planted 4 KiB
  file served as `file://` (refused against the LADX SHA-1 with no target,
  no part and no URL in its output; accepted against its own; else
  `fetch_rom: self-test failed: <case>`, exit 1). Then a `tmp/ladx/base.gbc`
  already of SHA-1 `d90ac17e…` is kept and its SHA-1 printed; else, with no
  URL, `setup: fetch_rom: no URL (argument or LADX_ROM_URL) and no ROM in
  tmp/ladx/`; else it downloads to `base.gbc.part` and prints only the
  SHA-1 once moved into place. A dead link (`download failed (curl exit
  <n>, HTTP <code>)`) or another SHA-1 (`refused, SHA-1 <x> ...`, the file
  deleted) is a `setup:` line, exit 2.
- Phase 2's push gate, outside the net (git and python3; armed on the real
  repository only from Phase 2 step 1): `python3 -I
  tests/ladx/oracle/push_gate.py`, a pre-push hook chained after
  `no_claude_branch.sh` (both fed the same ref lines, both always run). It
  refuses a push when the link in `LADX_ROM_URL` is in `git log -p --text`
  of a pushed commit (`the link is in commit <sha>`), when a blob the push
  uploads, intermediate commits included, holds a literal run under the
  Nintendo-bytes check's policy, through the check's own `check_blob`, the
  scope taken from the blob's path (`<path> (blob <sha>): literal run of <n>
  bytes at @<offset> (<view>, <scope>)`; the `computed` clearance from the
  working tree's `notes/ladx-inventory.md`, a blob needing it refused with
  `setup:` when it is missing), or when an untracked file sits
  outside the git-ignored paths (`docs/prd/` allowed, but searched for the
  link); it fails closed (no `LADX_ROM_URL`, no `tmp/ladx/base.gbc` of
  SHA-1 `d90ac17e…`, or no LADXR checkout at `4325207e` in `LADXR_DIR`,
  default `tests/ladx/oracle/ladxr`: `push-gate: setup: …, push refused`),
  never echoes the link or the bytes, else prints `push-gate: ok, <c>
  commits, <b> blobs, no link, 0 literal runs, cleared <t> trivial, <l>
  low-info, <p> ladxr-public, <c> computed, <i> invariant, no stray
  untracked file`. While
  it is armed,
  every push needs `LADX_ROM_URL` exported in the same shell command as
  `git push`, the ROM and the pinned LADXR checkout; a refusal for one of
  them is a setup problem, not a bug. `--self-test` makes 23 planted pushes
  through the installed chain in a planted clone with a planted ROM, link
  and LADXR checkout (among them a 4-byte ROM run under a strict path, a
  16-byte one and an 8-byte `bytelist` under general paths, refused; a
  12-byte English coincidence and a run of the planted corpus, passing;
  a case folder's `run.txt` under the general scope and a `computed` run of
  its `rom_writes.txt`, passing, a literal token in the group's read set and
  an unparseable log, refused; a 4-byte literal token of a patch table
  under `data/ladx/`, refused in the `tables` scope; a LADXR label padded
  with spaces under a general path, passing, and a padded ROM run of 33
  bytes, refused;
  `push-gate: self-test, 23 planted pushes as expected`, else one
  `self-test failed: <case>` line each); `--install` runs it, then writes
  the chain into the common hooks directory only over no hook,
  `no_claude_branch.sh`'s own or the chain (else `the gate is NOT armed`,
  exit 1), never touching `prepare-commit-msg`; `--uninstall` removes the
  chain and runs `no_claude_branch.sh --install`.
- The ROM read trace, outside the net (python3 and the LADXR checkout, else
  `setup: read-trace: no LADXR checkout in <dir>, run
  tests/ladx/oracle/setup.sh`, exit 2; no ROM): `read_trace.py`'s
  `install(modules, sections, inventory)` makes LADXR's `ROM` banks and
  `ROMWithTables` tables record every read outside `ROM.patch` (channels
  `index`, `slice`, `iter`, `search`, `compare`, `convert`, `table`; an
  empty slice reads no byte and is not recorded) as
  `<group>\t<site>\t<caller>\t<channel>\t<target>\t<detail>\t<provenance>`
  (`source`, `written:<groups>` or `mixed:<groups>`: who wrote the bytes
  read since parsing), and every bank byte and table entry written
  (`ROM.patch` included), the site `<file> <function>\t<line>[ #<k>]` of the
  innermost section frame (`#<k>` on the k-th occurrence of that line text
  in the function); writes are summed per site as a count and a SHA-1 of
  the net values.
  `python3 -I tests/ladx/oracle/read_trace.py --self-test [--ladxr <dir>]`
  runs one planted read per channel and per provenance, a planted parse,
  `rom.patch` and writes through LADXR's real `ROM` and `PointerTable` on
  64 synthetic banks (`read-trace: self-test, 7 channels, 4 provenances and
  4 writes traced, patch reads excluded`, else one `self-test failed:
  <case>` line each, exit 1); `--blind-spots`
  lists the buffer-protocol lines a Python trace cannot see (`BLIND
  <file>\t<line>`, then `read-trace: <n> buffer-protocol sites to review`).
- The key trace, outside the net (python3 and the LADXR checkout, else
  `setup: key-trace: no LADXR checkout in <dir>, run
  tests/ladx/oracle/setup.sh`, exit 2; no ROM): `key_trace.py`'s
  `install(settings_class, assembler, sections, roots)` records, at the
  innermost section frame's site (as `read_trace.py`), every attribute read
  of LADXR's `Settings` and every lookup in the assembler's `CONST_MAP`
  (`ladx-key-reads 1`, then sorted `setting|const\t<site>\t<name>` lines;
  `parse`). `python3 -I tests/ladx/oracle/key_trace.py --self-test [--ladxr
  <dir>]` runs a planted section on the real `Settings` and assembler
  (`key-trace: self-test, 3 setting reads and 1 constant lookup traced, the
  read outside the sections left out`, else one `self-test failed: <case>`
  line each, exit 1).
- The inventory check, outside the net (python3 and the LADXR checkout, else
  `setup: inventory: no LADXR checkout in <dir>, run
  tests/ladx/oracle/setup.sh`, exit 2; no ROM): `python3 -I
  tests/ladx/oracle/inventory_check.py [--ladxr <dir>] [--inventory <file>]
  [--print]` proves itself first on a planted tree and inventory
  (`inventory: self-test, 14 planted inventories as expected`, else one
  `self-test failed: <case>` line each, exit 1), then extracts every
  statement of `generator.py generateRom`, `randomizer.py
  Randomizer.__init__` after the fill, `romTables.py ROMWithTables.__init__`
  and `.save`, `rom.py ROM.save` and `ROM.fixHeader` (keyed by stripped
  line, ` #<k>` on a repeat, as `read_trace.py`'s sites) with the settings,
  `args`, RNG and placement each names, and diffs them against
  `notes/ladx-inventory.md`'s tables (default): one `MISSING`, `STALE`,
  `MISMATCH <section>: <call>: <column> inventory <a>, source <b>` or `BAD`
  (a `Side` other than `table`, `C`, `none`, `unused`, an empty `Group`, a
  malformed table) line each, then checks the Groups, Read coverage and
  Blind spots tables (every row's group listed with its side, a `C`
  group's Evidence naming itself and no `extract:<name>`, every coverage
  Kind `copy`, `written`, `computed`, `table`, `rom` (read from the user's
  ROM at runtime, verified by `rom-check`) or `cut:<id>`, the blind spots equal to `read_trace.py --blind-spots`), exit 1
  on a finding, else `inventory: <rows> rows in 6 sections match <commit>`.
  `--print` writes the extracted tables, hand-written columns kept, to
  refresh the file by hand.
- The read check, outside the net (python3, the LADXR checkout and the
  traces of `inventory_trace.sh`, else a `setup:` line, exit 2; no ROM):
  `python3 -I tests/ladx/oracle/read_check.py [--ladxr <dir>] [--inventory
  <file>] [--traces <dir>] [--scope-cuts <file>] [--print]` proves itself
  first on planted traces and inventories (`read-check: self-test, <n>
  planted trace sets as expected`), then keys every traced read and write
  to its inventory row (a loop or condition header to the first row of its
  body) and prints one `UNTAGGED`, `UNCOVERED` (a `C` group's read no Read
  coverage entry matches), `BAD-COVER`, `NONE-ACCESS`, `UNUSED-REACHED`,
  `SEED-VARIES` (a `table` row's writes differ between seeds),
  `TABLE-READS-SEED`, `DEPENDS` or `READS-COLUMN` line per finding, the
  information lines `STABLE <row>` and `UNREACHED <row>`, then
  `read-check: <cases> cases, <reads> reads, <groups> groups covered`, else
  `read-check: <n> findings`, exit 1. A `cut:<id>` coverage needs its id in
  `notes/plan3-scope-cuts.md`; a `rom` coverage needs a Reason. `--print` writes the inventory with its
  `Reads (trace)` cells and Depends on column taken from the traces.
- The full-ROM check, outside the net (python3; the ROM and Phase 3's
  generator, else a `setup:` line): `make -C tests/ladx rom-check
  [LADX_ROM=<file>] [LADX_GEN=<binary>] [CASES="<case> ..."]` (`python3 -I
  tests/ladx/oracle/rom_check.py`, run from the repository's root, paths
  repository-relative; `LADX_ROM` default `tmp/ladx/base.gbc`, `LADX_GEN`
  default `tests/ladx/build/ladx_patch`, `CASES` empty for every case and
  combo) proves itself first on a synthetic 64 KiB ROM, a planted case tree
  and a fake generator (17 planted runs: ok, a planted SHA-1 mismatch,
  refused, exit 1, no output, timeout, two `NOGOLDEN`s, the arguments and
  environment passed, no link and no ROM run printed, the setup and
  `--work` refusals; `rom-check: self-test, <n> planted runs as expected`,
  else one `rom-check: self-test failed: <case>` line each, exit 1;
  `make -C tests/ladx rom-check-selftest` stops there). Then it takes the
  cases from `ladx_cases.py`'s checked listing (a tree unlike the expected
  list: its `MISSING`/`EXTRA`/... lines, exit 1; an unknown name exit 2),
  runs the generator on each, cases then combos, under
  `tmp/ladx-rom-check/<case>/` (`--work`, refused outside the repository's
  `tmp/` or over the ROM or generator; emptied at the start; `out.gbc` and
  `gen.log`, the generator's stdout and stderr, never echoed; an `ok`
  case's `out.gbc` deleted) and compares each output's SHA-1 with
  `run.txt`'s `rom_sha1`. Generator contract: `<gen> --base <rom> --out
  <file> --seed <32 lowercase hex> [<key>=<value> ...]` (the case's
  non-default settings in LADXR's order), exit 0 with the file written,
  exit 3 for a refused base, anything else a failure; run in its case
  directory, `LADX_ROM_URL` removed from its environment, 120 s per case
  (`--timeout`). Lines: `rom-check: base <sha1> (the LADX English 1.0 ROM
  | not the LADX ROM), generator <path>`; per case `ok <case>`, `MISMATCH
  <case>: sha1 <got>, golden <want>`, `REFUSED <case>: base refused (exit
  3)`, `FAILED <case>: <exit <n> | signal <n> | timeout <s> s | no
  output>`, `NOGOLDEN <case>: <reason>` (no `rom_sha1`, or an outcome not
  `ok`); then `rom-check: <n> cases, <m> combos: <a> ok, <b> mismatched,
  <c> refused, <d> failed, <e> no golden`; exit 0 when every case is `ok`,
  else 1; a missing ROM (`setup: rom-check: no ROM at <path> ...`) or
  generator (`setup: rom-check: no generator at <path> ...`) or a usage
  error exits 2. Digests only, never ROM bytes or the link. The target
  builds `ladx_patch` first; with the ROM every case and combo is `ok`.
- `make ui-cli` builds `ui_cli` alone; look at the app with
  `tests/alttp/build/ui_cli --sd <dir> --every <keys>` (`ui_cli` with no
  argument prints its usage). `--seed`, `--date`, `--source-md5` and
  `--base-md5` fix a run (`--ladx-seed <32 hex>` and `--date` a LADX
  one); `--unsupported <setting>=<value>,...` is a
  test-only override of the library's unsupported pairs. The SDL app
  `zrando` takes the same keys and flags (one shared module,
  `app/sd/rando_fixed_run.c`), plus `--shot-text <file>`; with keys, its run
  ends after the last one.
- `make check` = `test` plus `test-ladx-oracle`, `ladx-cases`, `lint`
  (`alttp/`, `ladx/` and `app/` built with
  `-std=c99 -pedantic-errors`, except `app/sdl/` and `sdl_main.c`, which
  `make sdl` builds that way; `ladx/`'s half is `make -C tests/ladx lint`,
  contract in `tests/ladx/README.md`, "Unit tests and lint"), `logic` (unit tests replaying the oracle fixtures
  of both games: `tests/alttp`'s, then `make -C tests/ladx logic`, contract in
  `tests/ladx/README.md`, "Unit tests and lint"),
  `selftest` (the e2e drivers' own self-tests), `e2e` (every case diffed
  against its golden, one `DIVERGENT <case>` line per failure) and
  `e2e-combos` (every combination case diffed against its golden, kept out
  of `e2e`), each `tests/alttp`'s then LADX's (`make -C tests/ladx
  selftest|e2e|e2e-combos`, LADX's `e2e` and `e2e-combos` first comparing
  C's projection of every golden with `write_log.py`'s, `run_e2e.py
  --projections`; contract in `tests/ladx/README.md`, "E2E
  driver and claims") and `e2e-synthetic` (`make -C tests/ladx
  e2e-synthetic`: LADX's ROM-free regression reference
  `tests/ladx/synthetic_refs.txt`, digests of `ladx_assemble`'s outputs
  on a synthetic base, every case and combo; rewritten only by `make -C
  tests/ladx synthetic-refs`, outside the net, after `e2e-rom` and
  `rom-check` are green with the ROM, in its own commit; contract in
  `tests/ladx/README.md`, "Synthetic reference"). Run
  `make -k check` to see every failure at once.
- `make -C tests/ladx e2e-rom [LADX_ROM=<file>]`, outside the net (the
  ROM, else a `setup:` line, exit 2): C's KS span list against
  `known_source.build`'s, then `ladx_gen --rom` on every case and combo,
  every `group` and `rom-group` of `claims.txt` claimed, the spoiler and
  the net effect of the whole write log (`net`) too; contract in
  `tests/ladx/README.md`, "ROM-present e2e".
- `make -C tests/alttp measure`, outside the net, gives the per-case times
  every later budget is measured against, the whole pipeline's time
  (`rando_assemble` on the synthetic base) and its Cortex-A7 projection
  against the ~10 s line.
- `make -C tests/alttp measure-compare`, outside the net, times the current
  generator against a throwaway worktree of `40314c3`
  (`tmp/measure-ref-40314c3`), interleaved per case, full and stopped after
  `fill`, with ±10% verdicts; a reference that does not build is a `setup:`
  line.
- `make e2e-arm`, outside the net, builds the generator with Onion's pinned
  toolchain (`onion-toolchain.sh`, `docs/toolchain.md`) and runs the e2e
  cases on it under `qemu-arm`. `make unit-arm`, outside the net too, builds
  every unit test the same way and runs it under `qemu-arm` from
  `tests/alttp`, `--fixtures` included for `LOGIC_TESTS`, then LADX's
  (`make -C tests/ladx unit-arm`, contract in `tests/ladx/README.md`, "Unit
  tests and lint"). Both need docker
  and qemu-user; a missing one fails with a `setup:` line, a setup problem,
  not a bug.
- `make sdl` = `sdl-pc` + `sdl-arm`, outside the net. `make sdl-pc` needs
  `libsdl1.2-dev` (sdl12-compat on SDL2), `libsdl-ttf2.0-dev` and
  `libsdl-image1.2-dev`, else a
  `setup:` line: it builds the SDL layer and `sdl_main.c`, runs the SDL tests
  of `tests/alttp/sdl/` and the smoke test `sdl/smoke_sdl.sh`, all with the
  dummy video driver. `make sdl-arm` needs docker: it compiles and links the
  same code with Onion's toolchain against its SDL 1.2, SDL_ttf and
  SDL_image, and runs nothing. The owner looks at the PC build with
  `make sdl-pc`, then `tests/alttp/build/sdl/zrando --sd tmp/sd --theme
  themes/alttp` (the theme draws every frame; without `--theme` the app
  reads `themes/default/` next to its binary; `--font` is an unknown
  option).
- `make sdl-pc` also runs the golden comparison
  (`tests/alttp/sdl/goldens.sh --compare`): it proves itself on a planted
  flipped pixel, a removed golden and an extra one, then plays the 18 states of
  `sdl/golden_states.txt` (each on a fresh SD from `sd_fixture`, drawn
  through `--theme themes/classic`) and prints
  one `DIFFERENT`, `MISSING` or `EXTRA` line per mismatch (each BMP
  capture's decoded pixels against its lossless PNG golden), else
  `goldens: 18 states pixel-identical`. The goldens in
  `tests/alttp/sdl/goldens/` (PNGs) are pinned to the Claude Code cloud container
  (Ubuntu 24.04.5, sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11,
  FreeType 2.13.2), fingerprinted in `goldens/environment.txt`; on another
  host it fails with `setup: goldens pinned to <fp>, this host is <fp>`, a
  setup problem that verifies nothing, not a bug. Then it draws the same 18
  states with `themes/alttp` (`goldens.sh --draw`, the theme in the
  Makefile's `DRAW_THEME`): no comparison and no state strings, each capture
  must be a 640×480 picture of more than one colour; it writes them to
  `tests/alttp/build/sdl/theme-states/alttp/` and prints
  `goldens: 18 states drawn with alttp in <dir>`.
- `make sdl-goldens`, outside the net, writes the goldens once (every state
  rendered twice, refused unless byte-identical, written as PNGs) and
  refuses a folder that already holds any: goldens are never regenerated to get green.
- `make sdl-goldens-add STATES="<state> ..."`, outside the net, with
  `make sdl-pc`'s packages (else its `setup:` line; `goldens.sh --add DIR
  STATE...`), writes only the goldens of the named states of
  `sdl/golden_states.txt` (empty `STATES` is a usage error). It first proves
  itself on planted files, then refuses, writing nothing, an unknown or
  repeated state, a state that already has a golden, a folder with no
  `environment.txt`, or a host fingerprint other than it (the `setup:` line
  of `--compare`); it renders those states twice with `themes/classic`,
  refuses unless byte-identical (`NONDETERMINISTIC <state>`), writes their
  PNGs and touches no other file, `environment.txt` included; it prints
  `goldens: <n> states added to <dir>, rendered twice byte-identical:
  <names>`. To rewrite one golden on purpose, delete only that PNG, then
  add it; `GOLDENS_DIR=<dir>` points it at a scratch copy.
- `make screenshots`, outside the net, with `make sdl-pc`'s packages (else
  its `setup:` line; `tests/alttp/sdl/screenshots.sh`): draws the README's
  states (`SHOT_STATES` in `tests/alttp/Makefile`, from
  `sdl/golden_states.txt`) with `themes/alttp` twice, refuses unless
  byte-identical, and overwrites `docs/screenshots/<state>.png`. No check:
  they are pictures, rewritten and committed after a theme change.
- `make mockups`, outside the net, checks `design/mockups/` against the app
  (`tests/alttp/sdl/mockups.sh`): the comparator's self-test
  (`mockup_text.py --self-test`), the render check's self-proof and its
  offline Chromium render of every mockup with the local Press Start 2P and
  no ligature (`mockup_render.js`), every mockup paired in
  `tests/alttp/sdl/mockup_states.txt` (seed 1, the golden runs' date),
  `zrando` with `themes/alttp` playing every pairing (`goldens.sh --draw
  --states`), the `ui_cli` grid read from those captures, `ui_cli`
  byte-identical to `zrando` on the theme's page, and each mockup's text
  compared with `ui_cli`'s, help line included (one `MISMATCH` line per
  difference; the rules are in `mockup_text.py`'s docstring); it writes
  `tests/alttp/build/mockups/` and ends with one summary line. Without
  python3, node, npm, Playwright or its Chromium (`NODE_PATH=$(npm root
  -g)`, browsers in `/opt/pw-browsers`) or `make sdl-pc`'s packages it
  prints a `setup:` line, a setup problem that verifies nothing.
- `make theme-compare`, outside the net, with the same needs and `setup:`
  lines as `make mockups` (`tests/alttp/sdl/theme_compare.sh`, image tool
  `theme_compare.py`): it renders the mockups like `make mockups`, captures
  the app in each pairing with `THEME` (default `themes/alttp`, e.g.
  `THEME=themes/classic`), and writes `tests/alttp/build/theme-compare/`:
  one `<mockup>.png` per mockup, panels **mockup | app | diff** (diff: the
  mockup in dim grey where equal, magenta where not), and `scores.txt`,
  one `theme-compare: <mockup>: score <s>` line each. The score is
  100 × Σ|A−B| / (W×H×3×255) over every pixel and RGB channel. There is no
  threshold: it fails only when the images cannot be produced. Each run
  first proves the tool on planted images and checks that each mockup's
  self-score is 0. Mockup 00 (16 px) scores high by construction. It also
  writes `offsets.txt` (per mockup text: dx, dy of its colour's ink in its
  box grown by 4 px, or `not found`) and prints per mockup
  `theme-compare: <mockup>: outside <n> px, <k> texts, worst |dx| <a> |dy|
  <b>, <m> beyond 2 px, <z> not found` (`outside`: differing pixels in no
  text window), informational like the score.
- `make theme-art` and `make theme-art-write`, outside the net, need python3,
  node, npm, Playwright and its Chromium (else the `setup:` lines of
  `make mockups`; `tests/alttp/sdl/theme_art.sh`, table `theme_art.txt`,
  renderer `theme_art.js`, normaliser `theme_art.py`, each self-proved
  first). They render the 14 PNGs of `themes/alttp/` from the mockups' own
  shapes (`frame.png` checked equal in all nine mockups). `theme-art`
  compares the pixels (not the bytes) with the committed PNGs: one
  `DIFFERENT`, `MISSING` or `EXTRA <file>` line per mismatch, else
  `theme-art: 14 images pixel-identical to themes/alttp (Chromium <v>)`.
  `theme-art-write` renders twice, refuses unless byte-identical, then
  writes them. The art is a build product of the mockups, regenerated when
  they change (unlike the goldens), pinned to Playwright 1.63.0's Chromium.
- `make package`, outside the net, needs docker: it runs `sdl-arm`, strips
  the binary with Onion's toolchain, checks it with `file` and `readelf`
  (32-bit ARM EABI5, dynamically linked, `/lib/ld-linux-armhf.so.3`,
  stripped, `hard-float ABI`, NEEDED `libSDL-1.2.so.0`,
  `libSDL_ttf-2.0.so.0` and `libSDL_image-1.2.so.0`), re-runs
  `test_patch_header --file` on `data/base.bps`, and writes
  `tests/alttp/build/package/ZeldaRando.zip`, whose only top-level folder is
  `App/ZeldaRando/` (binary `zrando`, label "Zelda Randomizer"), with the
  theme folder `PKG_THEME` (`themes/alttp`) shipped as
  `App/ZeldaRando/themes/default/` and the base patch as
  `App/ZeldaRando/alttp/base.bps`; the same commit gives the same
  zip bytes.
- `make mac`, `mac-rom ROM=<file>`, `mac-run`, `mac-clean`, outside the
  net, macOS only (`targets/mac/`): the SDL app on sdl12-compat with a
  locally built SDL_ttf and SDL_image, and a fake SD card in `tmp/sd`; the owner runs
  `mac-run`, never an agent.
- Not in the net, on purpose: `tests/alttp/oracle/gen_golden.sh` (the PHP VT
  oracle) runs only to regenerate goldens; the LADX oracle's `setup.sh`,
  `gen_fixtures.sh`, `determinism.sh`, `gen_golden.sh`, `gen_tables.sh`,
  `table_replay.py`, `gen_fill_outcomes.py`, `gen_encoder_vectors.py`, `gen_store_vectors.py`, `gen_header_vectors.py` and `audit_order.py` run only to regenerate or re-prove its fixtures and outputs; `nintendo_bytes.py` needs the ROM,
  which only Phase 2 holds, and the LADXR checkout, `fetch_rom.sh` fetches it, and `push_gate.py`
  guards Phase 2's pushes, `read_trace.py` traces LADXR's ROM reads and
  `key_trace.py` its settings and constant reads,
  `inventory_check.py` diffs the inventory against LADXR, and
  `inventory_trace.sh` and `read_check.py` check it on real reads, and
  `make -C tests/ladx rom-check` needs the ROM and Phase 3's generator
  (their tests run in `make test-ladx-oracle`, part of `make check`). The SDL build (`make sdl`,
  `libsdl1.2-dev` and `libsdl-ttf2.0-dev`) and the qemu-arm run of the cross
  build are not `check`, nor are `make package`, `make sdl-goldens`, `make sdl-goldens-add`,
  `make screenshots`, `make mockups`, `make theme-compare`, `make theme-art` and
  `make theme-art-write`.
  `make patch-header` (the base patch's header, source CRC32, size cap and
  pipeline checks) checks any `BPS=<file>`; `make test` checks `data/base.bps`.
- CI: `.github/workflows/check.yml` runs `make -k check`, in a job
  `sdl` `make sdl` on `ubuntu-24.04`, the goldens' pinned release (the PC
  binary and `themes/default/` uploaded as the artifact `zrando-pc`), and
  in a job `package` `make package` then `make e2e-arm` (the zip uploaded as
  the artifact `zrando-miyoo`), all manually only (`workflow_dispatch`)
  until the owner turns on its push and pull-request triggers.

## Ground rules

- **The rando goldens are the source of truth.** Never regenerate a golden to
  get green: `gen_golden.sh` runs only when VT, its patch or the case list
  changes on purpose, in its own commit. *Goal: an agent cannot play the game;
  matching VT byte for byte is the proof that the ROM is right.*
- **`alttp/` stays pure C99**: no `malloc`, no platform library, built by host
  gcc and by any cross compiler. *Goal: the same code runs on the host, on the
  Miyoo and under qemu.*
- **No Nintendo ROM in the repository or in CI**; only the base patch
  (`data/base.bps`) is committed. Tests use synthetic ROMs and write logs.
  `make test`'s ROM guard (`tests/rom_guard.py`) enforces it on the index.
  The LADX oracle gets the real ROM from a temporary link held only in
  `LADX_ROM_URL`, never in a file, a note, a commit message or a PR,
  fetched by `fetch_rom.sh` into `tmp/ladx/` and refused unless SHA-1
  `d90ac17e…`. Nothing committed holds a literal run of that ROM, checked
  by `nintendo_bytes.py` and, while Phase 2's push gate is armed, before
  every push. Nothing committed may hold ROM content in any encoding
  either: re-encoded numbers, per-piece hashes, or anything from which ROM
  bytes can be rebuilt, not only literal runs; code that needs source-ROM
  content reads it from the user's ROM at runtime, verified by `rom-check`.
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
  installs it, writing only over no hook or its own, so a hook chaining it
  is left alone. From Phase 2 step 1 the pre-push hook is
  `push_gate.py --install`'s chain (no_claude_branch.sh, then the gate);
  `push_gate.py --uninstall` restores the plain one.
- No pull request unless the owner asks for one; `plan-pilot` opens its own
  at the end of a finished plan.
