---
title: "Output naming, history record and spoiler"
description: "The assembly pipeline writes a ROM to whatever path it is given, so seeds have no stable name, nothing records which settings made them, and the spoiler is lost; the History tab has nothing to list."
status: needs-triage
---

## The step (Plan 1, Phase 2, step 3, copied from `docs/plan-1.md`)

> **Output naming, history record and spoiler.** Name a ROM `<Mode>-<Goal>-<code>.sfc`, where `<code>` is 6 hex digits derived from the seed and settings, with a counter suffix on collision. The history record JSON holds the settings, an ISO date and the optional spoiler (reuse `rando/host/spoiler_json.c`'s format). Verified by unit tests: naming of every mode and goal, collision handling, and a record round-trip (write, then read back identical) with every setting, hints and heart beep at a non-default value.

Context the step relies on, from the plan:

- §2 *Files on the SD card*: generated ROMs go to `Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc` (e.g. `Open-FastGanon-4F2A9C.sfc`), a subfolder of Onion's SNES list; history records go to `App/ALttPR/history/<code>.json`: settings, date, and the spoiler when the spoiler option was on.
- §2 *Spoiler*: an option of the Generate screen, off by default. It decides only whether the spoiler is kept in the history record; it **never changes the ROM and is never written next to the ROM**. A seed generated without it still appears in History, with its settings but no item locations.
- §2 *History tab* (built in Phase 3 step 6, not here): lists the seeds and shows each one's settings and spoiler; delete removes the ROM, its `.srm` and its save states; a ROM deleted by hand disappears from History at the next start.
- §2: the seed is always random, no seed entry and no sharing; settings are the eleven `RandoOptionKey`s plus hints, plus the heart beep cosmetic.
- §5 open question 2 (UI core and host tools under `rando/host/` or a new `app/`): decided in Phase 3, not here; see the placement decision below.
- Phase 1 step 1 / end of Phase 2: Phase 2's host tests also run under qemu-arm, so everything this step adds to `rando/` must cross-build with the Onion toolchain (gcc 8.3, `-std=c99`).

## Facts (checked at HEAD `684d476`)

- `rando/host/spoiler_json.c` / `rando_spoiler.h`: `rando_spoiler_write_json(FILE *, const RandoWorld *, const RandoOptions *, const RandoPlaythrough *)` writes VT's `getSpoiler()` in the oracle's normalized JSON: keys sorted bytewise, PHP pretty print with 4-space indents, string values with only `"` and `\` escaped, a final `}\n`; a NULL playthrough omits the `playthrough` key. Meta's `hints` is already written from `options->hints`. Golden `spoiler.json` files are at most 26.5 KB.
- `rando/host/main/rando_gen.c` writes `spoiler.json` from `gen.world`, `gen.options` and `rando_playthrough(&gen.world, &playthrough)` (a file-scope `static RandoPlaythrough`) after a complete run; every case's golden `spoiler.json` is that output, so a C spoiler equals the golden byte for byte (`e2e` checks it).
- `rando/include/rando_options.h`: `rando_option_key_name`, `rando_option_value_name(key, int)`, `rando_options_set(options, key, value)`, `rando_option_key_find`. There is no getter from a `RandoOptions` and a key to its enum value. State names `open standard inverted retro`; goal names `ganon fast_ganon dungeons pedestal triforce-hunt ganonhunt completionist`; defaults: every enum's first value, crystals tower and ganon `7`, hints on.
- `rando/include/rando_crc32.h`: `uint32_t rando_crc32(const uint8_t *, size_t)`, zlib's CRC32.
- `rando/src/` uses no `<stdio.h>` and no `snprintf` (pure library); `rando/host/` uses stdio and is cross-built by `e2e-arm` (`$(HOST_SRC)` by wildcard).
- `tests/rando/Makefile`: `UNIT_TESTS` / `LOGIC_TESTS` filter-out list; host sources by wildcard into `librando_host.a`; prior art for files in tests: `mkdtemp` under `/tmp` (`test_case_reader`), `tmpfile()` (`test_rom`, `test_spoiler_json`).
- A run of `rando_gen` at HEAD on the all-non-default combination chosen below (seed 1) completes at attempt 0 in ~30 ms; its `spoiler.json` is 22.6 KB.
- Worked example of the code rule decided below, computed with Python's `zlib.crc32`: the baseline settings with seed 1 give the canonical text `seed=1\nglitches=none\nitem_placement=advanced\ndungeon_items=standard\naccessibility=items\ngoal=ganon\ncrystals_tower=7\ncrystals_ganon=7\nstate=open\nweapons=randomized\nitem_pool=normal\nitem_functionality=normal\nhints=on\nheart_beep=half\n`, CRC32 `DB1CA10A`, code `1CA10A`, ROM name `Open-Ganon-1CA10A.sfc`.

## Hypotheses on Phase 2 step 2 (not done yet)

Phase 2 step 2 (`docs/prd/triage/phase2-step2-rom-assembly-pipeline.md`) is not done when this PRD is written. Before chunk 1, the implementer checks each hypothesis against the tree and records in the step note whether it held; one that does not hold is handled by the rule given with it.

- **H-2.2-1** (name tables): `rando_rom.h` has `RANDO_HEART_BEEP_COUNT`, `rando_heart_beep_name(RandoHeartBeep)` and `rando_heart_beep_find(name, &out)`; `rando_options.h` has `rando_hints_name(bool)` and `rando_hints_find(name, &out)`; names as the case format (`off half quarter double normal`, `on off`); `find` returns false and leaves `*out` untouched on an unknown name. *If they are missing*, chunk 1 adds them with exactly that contract (and switches `case_reader.c` to them, `test_case_reader` unchanged). *If names differ*, use the tree's.
- **H-2.2-2** (pipeline): `rando_assemble.h` exists with `RandoAssembleStatus` (`OK`, `SOURCE_SIZE`, `SOURCE_MD5`, `PATCH`, `PATCH_SIZES`, `BASE_MD5`, `UNSUPPORTED`, `GAVE_UP`, `OUTPUT`), `RandoAssembleRequest` (source, patch, both digests, `options`, `heart_beep`, `seed`, `max_attempts`, trace, `output` / `output_ctx`), `RandoAssembleWork` (`rom[RANDO_ROM_SIZE]`, `gen`), `rando_assemble_request_init` and `rando_assemble(request, work, &bps)`. On `OK` (and `OUTPUT`), `work->gen` holds the complete generation; the output function is called once, with a ROM pointer valid only during the call. *If names differ*, use the tree's. *If `work->gen` is not left complete*, chunk 4 stops and asks through a `guideline` agent rather than re-running the generator beside the pipeline.
- **H-2.2-3** (sizes): `RANDO_ROM_SIZE` (`0x200000u`) and `RANDO_ROM_SOURCE_SIZE` (`0x100000u`) are public in `rando_rom.h`. *If not*, use the tree's constants; this step adds none.
- **H-2.2-4** (CLI): a host module behind `rando_patch` exposes `RandoPatchArgs`, `rando_patch_parse_args(argc, argv, &args, error)` and `rando_patch_run(&args, FILE *out, FILE *err)`; `--base`, `--out` (required), `--bps` (default `data/base.bps`), `--seed`, and `key=value` for the 13 settings; it draws a seed when none is given; its file output function removes a partial file on failure; exit codes 0, 1 input, 2 usage, 4 unsupported, 5 gave up. *If it differs*, chunk 5 adapts the flags below to its shape; the behaviour decided here is unchanged. *If the seed drawing or the file output function is private*, chunk 4 makes it reachable from the new host module (one shared static-free helper in the host module, no copy).
- **H-2.2-5** (synthetic base in tests): step 2's tests build a synthetic 1 MiB source and a synthetic 1 MiB → 2 MiB BPS (test-local encoder), and set the request's digests to theirs. *If that builder is static inside one test file*, chunk 4 moves it, unchanged, into a shared test helper next to `unit/support.c` (built and linked the same way), and step 2's tests use it with no change of behaviour.
- **H-2.2-6** (registration): step 2 kept the `UNIT_TESTS` / `LOGIC_TESTS` convention. *If it changed*, follow the new one.

## Problem Statement

After Phase 2 step 2, the owner can make a playable ROM, but the ROM gets whatever name the caller passes, an existing file is overwritten, and nothing remembers how a seed was made. On the console that breaks the user's first need — many seeds side by side, each with its own save — because two generations could land on one file (and one `.srm`), and Onion's ROM list would show meaningless names. The History tab (Phase 3) has nothing to list: no record of a seed's settings, its date, or its spoiler. And the spoiler option has nowhere to go: §2 says it is kept only in the history record, never next to the ROM.

## Solution

Every generated seed gets a stable, readable, unique name and a history record, written together.

- **Name**: `<Mode>-<Goal>-<code>.sfc`, e.g. `Open-Ganon-1CA10A.sfc`. `<Mode>` and `<Goal>` are short CamelCase words for the state and goal; `<code>` is 6 uppercase hex digits computed from the seed and all 13 settings, so the same seed and settings always give the same code. When that name, or that code's history record, already exists, a counter suffix is appended to the code (`1CA10A-2`, `-3`, …), so a new seed never overwrites another one or its save.
- **History record**: `App/ALttPR/history/<code>.json`, a small JSON object in the spoiler's own pretty-printed style holding a format version, the code, the ROM file name, an ISO 8601 date, the seed, the 13 settings by their case-format names, and — only when the spoiler option is on — the spoiler, byte for byte what `spoiler_json.c` writes (the same text as a golden `spoiler.json`). A reader parses it back into the same values and hands back the spoiler's text in place, so Phase 3's History can show it.
- **Saving a seed**: one host call chooses the name, runs the assembly pipeline into the ROM file, then writes the record; either both files exist afterwards or neither does. The spoiler option changes only the record, never a byte of the ROM.
- **CLI**: `rando_patch --sd <root> [--spoiler]` does the same on a PC into an SD-card-shaped tree, so the owner sees real names and records before the app exists.

## User Stories

1. As a player, I want each generated seed to get its own ROM file, so that each seed has its own save and I can play several in parallel.
2. As a player, I want the ROM name to start with the mode and the goal (`Open-FastGanon-…`), so that I can tell my seeds apart in Onion's SNES list.
3. As a player, I want a short code at the end of the name, so that two seeds with the same mode and goal still have different names.
4. As a player, I want the code to be 6 hex digits, so that names stay short on the Mini Flip's screen.
5. As a player, I want a new seed never to overwrite an existing ROM, so that I never lose a seed I am playing or its save.
6. As a player, I want a counter suffix when a code is already used, so that even a repeated seed and settings get a separate file.
7. As a player, I want every mode (open, standard, inverted, retro) and every goal (Ganon, fast Ganon, all dungeons, pedestal, triforce hunt, Ganon hunt, completionist) named distinctly, so that no two setting choices look the same in the list.
8. As a player, I want the name to contain only letters, digits and `-`, so that it is valid on the SD card's FAT file system and in RetroArch's save names.
9. As a player, I want each seed to have a history record with its settings, so that History can tell me what I was playing.
10. As a player, I want the record to hold the date the seed was made, so that History can sort newest first and I recognise my seeds.
11. As a player, I want the spoiler kept in the record only when I turned the spoiler option on, so that I am not spoiled by accident.
12. As a player, I want a seed made without the spoiler still to have a record with its settings, so that it still appears in History.
13. As a player, I want the spoiler never written next to the ROM, so that Onion's ROM list shows only playable files.
14. As a player, I want the ROM to be byte-identical whether or not I kept the spoiler, so that the option never changes the game.
15. As a player, I want the spoiler in the record to be exactly what alttpr.com's spoiler shows (VT's format), so that I can read it like the one I know.
16. As a player, I want a failed save (full SD card) to leave neither a ROM without a record nor a record without a ROM, so that History and the ROM list never disagree after a failure.
17. As a player, I want hints and heart beep recorded with the eleven settings, so that the record describes the seed completely.
18. As the History tab (Phase 3 step 6), I want to read a record back into the same settings, date, name and code, so that I can show them through the translation table.
19. As the History tab, I want the record to name its ROM file, so that delete removes that ROM and finds its `.srm` and save states by its base name.
20. As the History tab, I want the reader to refuse a record whose ROM name does not match its own settings and code, so that delete never touches a file the app did not write.
21. As the History tab, I want the spoiler handed back as a span of the text I loaded, with no copy and no allocation, so that I can display it on the device with a static buffer.
22. As the History tab, I want a malformed, truncated or unknown-format record refused with a message, so that one damaged file does not crash the list.
23. As the History tab, I want a format version in every record, so that a later format can be told apart.
24. As the Generate flow (Phase 3 step 5), I want one call that names, assembles and records a seed, so that the app cannot forget a step or write them in the wrong order.
25. As the Generate flow, I want that call to report which part failed (pipeline status, no free name, ROM write, record write) and the chosen file name on success, so that the success and error screens can say so.
26. As the Generate flow, I want the date supplied by the caller, so that the app decides where time comes from on a console with no reliable clock, and tests use a fixed one.
27. As the Generate flow, I want the ROM and history directories given as paths, so that the app passes the SD card's and the tests pass a temporary tree.
28. As the owner on a PC, I want `rando_patch --sd <root>` to write the ROM and its record into an SD-card-shaped tree, so that I can check names and records before the app exists.
29. As the owner, I want `--spoiler` on the CLI to keep the spoiler in the record, so that I can check the option's effect.
30. As the owner, I want the CLI to print the ROM path and the record path it wrote, so that I find them.
31. As the owner, I want `--out` to keep working as before, writing one ROM to an exact path with no record, so that step 2's use is unchanged.
32. As a porting agent, I want the naming rule in the pure C99 library, so that the host, the app and qemu-arm all compute the same name.
33. As a porting agent, I want the canonical text the code is computed from documented exactly, so that the code can be checked with Python's `zlib`.
34. As a porting agent, I want the record's spoiler written by the existing spoiler writer, unchanged, so that there is one spoiler format and the goldens already prove it.
35. As a porting agent, I want the record's settings written and read through the library's name tables, so that the case format, the CLI and the record cannot diverge.
36. As the net, I want every mode × goal name tested against a literal, so that a renamed word is caught.
37. As the net, I want the code checked against values computed independently with Python, so that the hash input cannot drift silently.
38. As the net, I want a change in the seed or in any one of the 13 settings to give another code, so that the code really depends on all of them.
39. As the net, I want collision handling tested with a fake "taken" check (first name taken, several taken, a code taken only by its record, all suffixes taken), so that the counter is proven without a file system.
40. As the net, I want a round-trip with every setting, hints and heart beep at a non-default value, with and without the spoiler, so that every field is proven to survive write then read.
41. As the net, I want the record's spoiler text compared with the golden `spoiler.json` of a case, so that the record carries exactly VT's spoiler.
42. As the net, I want every case and combination's record spoiler compared with its golden in `make logic`, so that the record path is proven on every setting combination.
43. As the net, I want the save call tested on a temporary tree with the synthetic base of step 2, checking the files present and absent after success and after each failure, so that "both or neither" is proven.
44. As the net, I want `make test` to stay a few seconds, so that the fast loop stays fast.
45. As the net, I want the new code to build under `lint` (library part) and the Onion cross compiler, so that the end of Phase 2 runs these tests under qemu-arm.

## Implementation Decisions

### Placement (plan §5 open question 2)

- The open question concerns the UI core and the app's host tools (Phase 3); this step does not settle it and creates no `app/` directory.
- **Naming goes in the library** (`rando/include`, `rando/src`): it is pure (settings and seed in, a string out), it is needed identically by the CLI, the app and the qemu-arm run, and it needs only `rando_crc32` and the name tables. It uses no stdio: decimal and hex formatting are done by hand into caller buffers.
- **The record and the save call go in `rando/host/`**: they need `FILE *`, `remove` and the spoiler writer, which already live there; `rando/host/` is cross-built for the device (`e2e-arm`), so the app can link them wherever Phase 3 puts its own code.

### Library: one small getter and the naming module

- **`rando_options.h`** gains `int rando_options_get(const RandoOptions *options, RandoOptionKey key)`: the key's enum value (asserts the key is in range), the inverse of `rando_options_set`. The naming text and the record writer use it with `rando_option_value_name`.
- **Naming module** (`rando_name.h`, one source). Interface (shape decided here; names may follow the tree's style):

  ```c
  #define RANDO_NAME_CODE_MAX 10   /* "1CA10A-99" + NUL */
  #define RANDO_NAME_ROM_MAX 48    /* longest: "Standard-Completionist-XXXXXX-99.sfc" (36) + NUL */
  #define RANDO_NAME_SUFFIX_MAX 99

  typedef struct RandoName {
  	char code[RANDO_NAME_CODE_MAX];  /* "1CA10A", or "1CA10A-<n>" with 2 <= n <= 99 */
  	char rom[RANDO_NAME_ROM_MAX];    /* "<Mode>-<Goal>-<code>.sfc" */
  } RandoName;

  /* Asserts the value is in range. Static strings, never freed. */
  const char *rando_name_mode(RandoOptionState state);
  const char *rando_name_goal(RandoOptionGoal goal);

  /* The 24-bit code: zlib CRC32 of the canonical text, & 0xFFFFFF. */
  uint32_t rando_name_code(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed);

  /* Builds the name for suffix 1 (none) or 2..RANDO_NAME_SUFFIX_MAX. Asserts the suffix range. */
  void rando_name_make(const RandoOptions *options, uint32_t code, int suffix, RandoName *out);

  /* True when a ROM or a record already uses candidate. */
  typedef bool (*RandoNameTakenFn)(void *ctx, const RandoName *candidate);

  /* Tries suffix 1, 2, ... 99 in order, calling taken on each, and fills out with the first free one.
   * False, out unspecified, when all 99 are taken. */
  bool rando_name_choose(const RandoOptions *options, RandoHeartBeep heart_beep, uint32_t seed,
  		       RandoNameTakenFn taken, void *ctx, RandoName *out);

  /* True when code is 6 uppercase hex digits, optionally "-<n>" with n in 2..99, no leading zero. */
  bool rando_name_code_valid(const char *code);
  ```

- **Mode words** (from `RandoOptionState`): `Open`, `Standard`, `Inverted`, `Retro`. **Goal words** (from `RandoOptionGoal`): `Ganon`, `FastGanon`, `Dungeons`, `Pedestal`, `TriforceHunt`, `GanonHunt`, `Completionist`. They match §2's example (`Open-FastGanon-…`). They are file names, not UI text: they do not go through Phase 3's translation table, and never change once seeds exist.
- **Canonical text for the code** (ASCII, built in a stack buffer): `seed=<decimal>\n`, then `<key>=<value>\n` for the eleven `RandoOptionKey`s in enum order, using `rando_option_key_name` and `rando_option_value_name`, then `hints=<on|off>\n` and `heart_beep=<name>\n` through H-2.2-1's names. The code is `rando_crc32(text) & 0xFFFFFF`, printed as 6 uppercase hex digits with leading zeros. The heart beep is included because it changes the ROM's bytes. The code identifies a seed on this console only; it is not alttpr.com's hash and is not meant for sharing (§2).
- **Suffix**: suffix 1 has no suffix (`1CA10A`); suffix `n ≥ 2` appends `-<n>` to the code, so the ROM is `Open-Ganon-1CA10A-2.sfc` and the record `1CA10A-2.json`. The suffix belongs to the code, so a ROM and its record always share it. 99 is the limit (a hundred identical seeds is not a real case); beyond it the save fails with its own status rather than overwrite.

### Host: the history record module (`rando_record.h`, one source)

- Interface:

  ```c
  #define RANDO_RECORD_FORMAT 1
  #define RANDO_RECORD_TEXT_MAX (64 * 1024)   /* largest golden spoiler is 26.5 KB */
  #define RANDO_RECORD_ERROR_MAX 256

  typedef struct RandoRecordDate { int year, month, day, hour, minute, second; } RandoRecordDate;

  typedef struct RandoRecord {
  	RandoName name;
  	RandoRecordDate date;
  	uint32_t seed;
  	RandoOptions options;          /* hints included */
  	RandoHeartBeep heart_beep;
  	bool has_spoiler;
  	/* Reader output only: the spoiler object's bytes are text[spoiler_offset, +spoiler_size). */
  	size_t spoiler_offset, spoiler_size;
  } RandoRecord;

  /* world and playthrough are read only when has_spoiler (then neither may be NULL); the spoiler's meta
   * uses record->options. False on a stream error; the caller owns and closes out. */
  bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
  			const RandoPlaythrough *playthrough);

  /* Parses a whole record. The spoiler span points into text, which the caller keeps. On failure returns
   * false and writes a reason into error; out is unspecified. */
  bool rando_record_parse(const char *text, size_t len, RandoRecord *out, char error[RANDO_RECORD_ERROR_MAX]);
  ```

- **Format** (the spoiler's own style: keys sorted bytewise, 4-space indent, `": "` separators, final newline). Top-level keys, in this order: `code`, `date`, `format`, `rom`, `seed`, `settings`, then `spoiler` only when kept. `format` is the integer 1; `seed` is a JSON integer; every other value is a string, except `settings` and `spoiler`, which are objects. `settings` holds the 13 keys, sorted bytewise (`accessibility`, `crystals_ganon`, `crystals_tower`, `dungeon_items`, `glitches`, `goal`, `heart_beep`, `hints`, `item_functionality`, `item_placement`, `item_pool`, `state`, `weapons`), each value the case format's string (`crystals_*` keep `random`, never the drawn count; the drawn count is in the spoiler's meta). Example without spoiler:

  ```json
  {
      "code": "1CA10A",
      "date": "2026-11-01T14:03:27",
      "format": 1,
      "rom": "Open-Ganon-1CA10A.sfc",
      "seed": 1,
      "settings": {
          "accessibility": "items",
          ...
          "weapons": "randomized"
      }
  }
  ```

- **The spoiler is written by `rando_spoiler_write_json`, unchanged and untouched**, straight after `"spoiler": `; because `spoiler` is the last key, its output (ending `}\n`) is followed only by the record's closing `}\n`. The spoiler's object therefore keeps its own indentation (it starts at column 0 inside the record); this is valid JSON, and it keeps the spoiler's bytes identical to a golden `spoiler.json` minus its final newline — the property the tests rely on. The playthrough is always included (as `rando_gen` writes it).
- **Date**: ISO 8601 local date and time without offset, `YYYY-MM-DDTHH:MM:SS`, from the caller's `RandoRecordDate`; the writer asserts the ranges (year 1970–9999, month 1–12, day 1–31, hour 0–23, minute and second 0–59). The console has no reliable time zone, so no offset is claimed. Where the time comes from is the caller's: the CLI uses `localtime(time(NULL))`; the app decides in Phase 3.
- **Reader** (strict, no allocation, no copy): a small JSON scanner over the caller's text. Whitespace anywhere JSON allows it, keys in any order; every top-level key except `spoiler` required exactly once; an unknown or duplicate key, a `format` other than 1, a wrong value type, trailing non-whitespace, or text longer than `RANDO_RECORD_TEXT_MAX` is refused. `code` must pass `rando_name_code_valid`; `rom` must equal what `rando_name_make` gives for the record's state, goal and code (the History delete therefore only ever targets a name this app makes); `date` must match the exact pattern and ranges; `seed` is the case reader's `rando_case_parse_uint32` rule on the digits; `settings` must hold the 13 keys once each, parsed with `rando_options_set`, `rando_hints_find`, `rando_heart_beep_find` (no default fills a missing key). `spoiler`, when present, must be an object; it is skipped structurally (strings with any backslash escape, nested objects and arrays, numbers, `true`, `false`, `null`) and returned as the span from its `{` to its matching `}`; its content is not interpreted. Error messages name the offending key or value.

### Host: the save call (`rando_seed_save.h`, one source)

- Interface (built on H-2.2-2):

  ```c
  #define RANDO_SD_ROM_DIR "Roms/SFC/ALttPR"
  #define RANDO_SD_HISTORY_DIR "App/ALttPR/history"
  #define RANDO_SEED_PATH_MAX 1024

  typedef enum RandoSaveStatus {
  	RANDO_SAVE_OK,
  	RANDO_SAVE_NO_NAME,    /* all 99 suffixes taken */
  	RANDO_SAVE_ASSEMBLE,   /* rando_assemble failed: see result->assemble and result->bps */
  	RANDO_SAVE_RECORD,     /* the record could not be written; the ROM was removed */
  } RandoSaveStatus;

  typedef struct RandoSaveResult {
  	RandoName name;
  	char rom_path[RANDO_SEED_PATH_MAX], record_path[RANDO_SEED_PATH_MAX];
  	RandoAssembleStatus assemble;
  	RandoBpsResult bps;
  } RandoSaveResult;

  /* request's output and output_ctx are replaced by the save's own file writer; every other field is
   * used as given. rom_dir and history_dir must exist. */
  RandoSaveStatus rando_seed_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
  				RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
  				RandoSaveResult *result);
  ```

- **Order**: (1) `rando_name_choose` with a "taken" check that is true when `<rom_dir>/<rom>` or `<history_dir>/<code>.json` exists (C99 `fopen(…, "rb")` succeeds); (2) `rando_assemble` on a copy of the request whose output function writes `<rom_dir>/<rom>` with `"wb"` and removes the partial file on any failure (H-2.2-4's writer, reused, not copied); a pipeline failure → `ASSEMBLE` with nothing left on disk; an `OUTPUT` failure is reported as `ASSEMBLE` with `result->assemble == OUTPUT`; (3) the record from `work->gen` (`seed` and `options` from the request, `heart_beep` from the request, the chosen name, the date, `has_spoiler = spoiler`), with `rando_playthrough(&work->gen.world, …)` only when the spoiler is kept, written to `<history_dir>/<code>.json`; any failure removes the partial record **and the ROM** → `RECORD`. So after any status but `OK`, neither file of this seed exists; after `OK`, both do.
- The `RandoPlaythrough` lives at file scope in this module (as `rando_gen` keeps its own); the module is not reentrant, as the app is single-threaded.
- The spoiler flag never reaches `rando_assemble`: the ROM is the same either way.
- **No directory creation**: C99 has no `mkdir`; a missing directory is a ROM write failure (`ASSEMBLE` / `OUTPUT`) whose message names the path. Creating `Roms/SFC/ALttPR/` and `App/ALttPR/history/` on the SD card is the app's packaging and start-up (Phases 3–4). The two relative paths are constants here so Phase 3 step 1 checks them against Onion's facts in one place.
- No translated text here: statuses only. The CLI has its own English messages; Phase 3 maps statuses to its table.

### Host: CLI extension (`rando_patch`)

- New flags: `--sd <root>` (exclusive with `--out`; exactly one of them is required) and `--spoiler` (only with `--sd`; refused with `--out`, because §2 never writes a spoiler next to a ROM). With `--sd`, the CLI calls `rando_seed_save` with `<root>/Roms/SFC/ALttPR` and `<root>/App/ALttPR/history`, the date from `localtime(time(NULL))`, and prints `seed <n>`, the ROM path and the record path. `--out` behaves exactly as in step 2 (no naming, no record).
- Messages: `NO_NAME` → "no free name for code <code>", `RECORD` → "<record path>: write failed (ROM removed)"; `ASSEMBLE` reuses step 2's message for the pipeline status. Exit codes: step 2's, with `NO_NAME` and `RECORD` as 1 (input/output).
- Usage line gains the flags: `usage: rando_patch --base <jp.sfc> (--out <file> | --sd <root> [--spoiler]) [--bps <patch>] [--seed <n>] [key=value ...]`.

## Implementation Chunks

1. **Getter and naming.** Check H-2.2-1, H-2.2-3, H-2.2-6 (add the name tables if missing); `rando_options_get`; `rando_name.h` and its source; `test_name` registered (filtered out of `LOGIC_TESTS`); a `rando_options_get` test in `test_options`. Green: `make test`, `make lint`.
2. **Record writer.** `rando_record.h` with the types and `rando_record_write`; `test_record` with the writer tests (exact text without spoiler, spoiler span equal to the baseline golden). Green: `make test`.
3. **Record reader.** `rando_record_parse`; round-trip tests with every setting non-default, with and without spoiler; every refusal; `test_record --fixtures` (every case and combination's record spoiler equals its golden) added to `LOGIC_TESTS`. Green: `make test`, `make logic`.
4. **Save call.** Check H-2.2-2, H-2.2-4, H-2.2-5 (move the synthetic-base builder into a shared test helper if needed); `rando_seed_save.h` and its source; `test_seed_save` (filtered out of `LOGIC_TESTS`) on a `mkdtemp` tree. Green: `make test`, `make check`.
5. ~~**CLI.**~~ Dropped by plan-pilot before triage: the plan's step 3 asks for naming, collisions and the record round-trip, not a new `rando_patch` mode; the save call (chunk 4) is what Phase 3's generation flow will call. `rando_patch` keeps step 2's `--out` interface unchanged.

## Testing Decisions

- **Good tests** go through public interfaces only (`rando_name_*`, `rando_options_get`, `rando_record_write`, `rando_record_parse`, `rando_seed_save`, the CLI's parse and run functions), check names, bytes, statuses and files on disk, never a static function. One behaviour per test, named after it; failures through `support.h`'s `test_fail`. Prior art: `test_options` (name tables), `test_spoiler_json` (golden `spoiler.json` compared byte for byte through `tmpfile()`), `test_case_reader` (`mkdtemp` trees, refusals naming the word), `test_crc32` / `test_rom_checksum` (independent values pasted as literals, the Python command in the commit message), step 2's `test_assemble` / `test_patch_cli` (synthetic base and patch).
- **New binaries**: `test_name`, `test_record`, `test_seed_save` in `UNIT_TESTS`; `test_name` and `test_seed_save` in the `LOGIC_TESTS` filter-out list; `test_record --fixtures` runs in `make logic`.
- **`test_name`**:
  - All 28 mode × goal pairs with one fixed code: each ROM name equals a literal (`Open-Ganon-…` through `Retro-Completionist-…`); every name matches `^[A-Za-z]+-[A-Za-z]+-[0-9A-F]{6}(-[1-9][0-9]?)?\.sfc$` and fits `RANDO_NAME_ROM_MAX`; the 28 are pairwise distinct.
  - Code literals computed with Python's `zlib.crc32` on the documented canonical text: the baseline with seed 1 → `1CA10A` (this PRD's worked example, re-checked), plus at least the all-non-default settings below with seed 1, a seed `4294967295`, and a code whose top hex digit is `0` (found by searching seeds in Python), proving the leading-zero padding.
  - Sensitivity: from the baseline, changing the seed or any one of the 13 settings to another value changes the code (each checked by literal, not only by inequality).
  - Suffixes: suffix 2 → `…-1CA10A-2.sfc` / code `1CA10A-2`; suffix 99 → `-99`.
  - `rando_name_choose` with a fake `taken`: nothing taken → no suffix; first taken → `-2`; first four taken → `-5`; taken only by code (the fake marks the code, as a lone record would) → `-2`; all 99 taken → false; the fake sees candidates in order 1, 2, 3… and is not called after the first free one.
  - `rando_name_code_valid`: accepts `1CA10A`, `1CA10A-2`, `1CA10A-99`; refuses lowercase hex, 5 or 7 digits, `-1`, `-0`, `-02`, `-100`, an empty string, a trailing character.
- **`test_record`**:
  - *Writer, no spoiler*: a fixed record (baseline settings, seed 1, date `2026-11-01T14:03:27`) writes a literal expected text exactly (the whole file).
  - *Writer, spoiler*: for the `baseline` case, generate with `rando_generate` (as the golden's `attempts`), compute the playthrough, write the record with the spoiler; the bytes after `"spoiler": ` up to the record's final `}\n` equal the golden `cases/baseline/spoiler.json`; the record without spoiler is the same text up to `settings`' closing brace.
  - *Round-trip, the plan's check*: settings `glitches=overworld_glitches`, `item_placement=basic`, `dungeon_items=mc`, `accessibility=none`, `goal=fast_ganon`, `crystals_tower=5`, `crystals_ganon=random`, `state=standard`, `weapons=assured`, `item_pool=hard`, `item_functionality=hard`, `hints=off`, `heart_beep=quarter` (every one non-default; it avoids alttpr.com's forced combinations), seed 1, a suffixed code, a date with every field non-zero and two-digit. With and without the spoiler (its world from `rando_generate` of those settings; checked complete at HEAD, see Facts): write → parse → every field equal to the written record (options compared field by field, `hints` included), `has_spoiler` as written, the spoiler span equal to `rando_spoiler_write_json`'s output for the same world minus its final newline; then write the parsed record again (same world) → byte-identical text. *If that combination stops completing* (a later generator change), the implementer picks another all-non-default one that does, records it in the step note, and keeps the test's shape.
  - *Reader refusals*, each by editing the round-trip text: missing each required key (one test per key), duplicate key, unknown top-level key, unknown settings key, missing settings key, unknown value, `format` 2, `seed` too large or negative, a non-integer seed, invalid code, `rom` not matching the code or the settings' state or goal, bad date (pattern and each range), `spoiler` not an object, an unbalanced spoiler, trailing text, empty text, text over `RANDO_RECORD_TEXT_MAX`; each returns false with the key named in the message. Accepted: keys reordered, extra whitespace, a spoiler holding escaped quotes, backslashes, `A`, nested arrays and `null` (span still exact).
  - *`--fixtures`* (in `make logic`): for every case and combination, the record spoiler span (+ `\n`) equals its golden `spoiler.json`, and the record parses back to the case's settings.
- **`test_seed_save`** (synthetic base and patch, H-2.2-5; request digests set to theirs; `mkdtemp` tree with both directories):
  - Success: status `OK`; the ROM file exists at `<rom_dir>/<name>` with the name `rando_name_choose` gives, 2,097,152 bytes, equal to what `rando_assemble` delivers for the same request; the record exists at `<history_dir>/<code>.json` and parses to the request's settings, seed, the given date and the name; no other file in either directory.
  - Spoiler on vs off, same seed, in two trees: ROM bytes identical; the record has a spoiler only when on; no `.json` or spoiler file in `rom_dir`.
  - Collisions: saving the same request twice in one tree gives `…-<code>.sfc` then `…-<code>-2.sfc`, the first ROM untouched (bytes compared); a lone `<code>.json` placed by hand in `history_dir` makes the first save take `-2`; a lone ROM placed by hand likewise.
  - Failures: a wrong source digest → `ASSEMBLE` / `SOURCE_MD5`, both directories empty; a missing `rom_dir` → `ASSEMBLE` / `OUTPUT`, no record; a missing `history_dir` → `RECORD`, the ROM removed, both directories empty; 99 names taken by hand-placed records → `NO_NAME`, nothing written, the placed files untouched.
- **`test_patch_cli`** (chunk 5, step 2's cases unchanged): parsing `--sd` with `--spoiler`, refusals (`--sd` and `--out` together, neither, `--spoiler` with `--out`, `--sd` without its value); running `--sd <tmp root> --seed 1 --spoiler` on synthetic files writes exactly one ROM and one record under the two SD subdirectories, prints `seed 1` and both paths, exit 0; a missing history directory → exit 1 and no ROM left.
- **Speed**: `make test` adds about six generations (baseline, the all-non-default one, and the saves on the synthetic base); well under a second. The 73-case record replay is in `make logic` only. `e2e`, `e2e-combos`, `measure` untouched.
- **Not tested here**: the real ROM and `data/base.bps` (never in CI); Onion's real SD paths (Phase 3 step 1 confirms them); the History list, detail and delete (Phase 3 step 6).

## Out of Scope

- The History tab itself: listing, sorting, reconciliation with ROMs deleted by hand, the detail view and the delete of ROM, `.srm` and save states (Phase 3 step 6). This step only gives it the record and its reader.
- Creating the SD card's directories, the app's clock, and the app's seed drawing (Phases 3–4).
- Any change to `spoiler_json.c`'s output, the generator, the pipeline's behaviour, the goldens, the oracle, `e2e` or `check-e2e`.
- Translating the mode and goal words (they are file names, not UI strings).
- Seed sharing, alttpr.com's hash or permalink, or reading a seed back into a regeneration (§2: no seed entry, no sharing; the seed is stored for completeness only).
- Writing a spoiler file anywhere but inside the record.
- Running the new tests under qemu-arm (the end of Phase 2).

## Further Notes

- Step note for this step: the outcome of H-2.2-1 to H-2.2-6; the Python commands behind the code literals; the all-non-default combination actually used by the round-trip; the CLI's final usage line, for the README (Phase 5).
- For the owner, once `data/base.bps` exists: `mkdir -p tmp/sd/Roms/SFC/ALttPR tmp/sd/App/ALttPR/history && tests/rando/build/rando_patch --base jp.sfc --sd tmp/sd --spoiler` shows a real name and record. That run is the owner's, not the agent's.
- Phase 3 step 6 can rely on: a record's `rom` is always a name this module makes (the reader enforces it), so RetroArch's `.srm` and save states are found from its base name (`<Mode>-<Goal>-<code>`); and a record without a spoiler key is a seed generated with the option off.
- The 24-bit code makes accidental collisions rare (about one in 16.7 million per pair); the suffix covers them and the deliberate repeat of a seed.
