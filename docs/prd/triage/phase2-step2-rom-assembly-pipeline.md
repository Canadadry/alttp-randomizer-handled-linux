---
title: "ROM assembly pipeline"
description: "The library can generate a seed and log its ROM writes, but nothing turns the user's JP 1.0 ROM and the base patch into a finished, checksummed .sfc, so no playable ROM can be produced yet."
status: needs-triage
---

## The step (Plan 1, Phase 2, step 2, copied from `docs/plan-1.md`)

> **ROM assembly pipeline**: check the source MD5 (`03a63945…`), apply the base patch from the 1 MiB JP ROM into a separate 2 MiB buffer (the patch's declared target size is the resize), check MD5 `edc01f3d…`, generate, run `rando_rom`, apply the heart beep and the writes Phase 1 step 2's note marks "reproduced in C" (in the order that note gives), update the checksum, and write the `.sfc`. A host CLI `rando_patch --base <jp.sfc> --out <file> [settings]` does it end to end. As in VT's `rom.js`, the BPS applies to the unresized 1 MiB JP ROM, not a 2 MiB copy; that buffer is only read, and stays unchanged until `rando_rom_write_cosmetic_defaults` has copied the Link sprite from it. `[settings]` are the case format's keys and values (every `RandoOptionKey`, `hints`, `heart_beep`), parsed through the library's name tables; heart beep and hints get theirs in the library, and `case_reader.c` uses them instead of its own list. The ROM and source sizes (`0x200000`, `0x100000`) are one pair of public constants in `rando_rom.h`, used by the checksum, the cosmetic defaults, the pipeline and the header-size test. Verified by:
>    - a host test with a **synthetic** base ROM and a synthetic BPS, which checks the order of operations and each error path (wrong MD5, bad BPS, write failure);
>    - a test that the written ROM equals the base-patched buffer with exactly the logged writes and the "reproduced in C" writes applied, then the checksum;
>    - a test whose synthetic BPS writes different bytes over the sprite ranges (`0x80000+0x7000`, `0xDD308+120`, `0xDEDF5+4`) checks that the written ROM holds the synthetic source's bytes there;
>    - MD5 (a library module like step 1's CRC32) checked against RFC 1321's test-suite values and one longer synthetic buffer whose digest is computed with Python's `hashlib`;
>    - the pipeline takes the expected source and target digests as inputs, so the synthetic tests run its success path; one test checks that the CLI and the app pass §2's `03a63945…` and `edc01f3d…`;
>    - the header-size test that Phase 4 step 2's gate runs on `data/base.bps` is written here, and checked on a synthetic BPS header.

Context the step relies on, from the plan:

- §2: base ROM is the JP 1.0, MD5 `03a63945398191337e896e5771f77173`; the base-patched, resized ROM is VT's `Rom::HASH` `edc01f3db798ae4dfe21101311598d44` (2,097,152 bytes). Only the heart beep is a user cosmetic; every other cosmetic stays at VT's default. The seed is always random on the device. No Nintendo ROM in the repository or in CI.
- Step 0.1's note: `data/base.bps` does not exist; its source is the **unresized 1 MiB** JP ROM. Phase 4 step 2's gate waits on it and runs a host test that reads its header and checks the pipeline feeds it a 1,048,576-byte source and expects a 2,097,152-byte target.
- Step 1.2's note, the "reproduced in C" writes, in order: quickswap, music volume, reduce flashing, menu speed, heart colour red, default Link sprite (from the JP ROM), `updateChecksum`. The heart beep is in VT's write log (its last line).
- Step 1.11's note: `rando_rom_write` writes every group, heart beep last; `rando_rom_write_cosmetic_defaults` holds the browser defaults in that order (it reads the base-patched `0x7FE2` and the `02 37` author guards, and copies the sprite from the 1 MiB source); `rando_rom_update_checksum` is VT's. "Phase 2 calls `rando_rom_write`, then these, then the checksum."
- Phase 1 step 1 (`e2e-arm`): the end of Phase 2 runs Phase 2's host tests under qemu-arm too, so what this step adds to `rando/` and `rando/host/` must cross-build with the Onion toolchain (gcc 8.3, `-std=c99`).

## Facts (checked at HEAD `fc77e82`)

- `rando/include/rando_rom.h`: `RandoHeartBeep` (off, half, quarter, double, normal; no count member), `RandoRomWriteFn` / `RandoRomWriter`, `rando_rom_write(gen, heart_beep, writer)`, `RandoRomBuffer`, `rando_rom_buffer_write` (bounds-checked), `rando_rom_update_checksum` (false unless 2 MiB), `rando_rom_write_cosmetic_defaults(rom, source, source_size)` (false unless 2 MiB and 1 MiB; `source` only read, must not overlap `rom`).
- `rom_checksum.c` and `rom_cosmetics.c` each hold a private `#define ROM_SIZE 0x200000u`; `rom_cosmetics.c` also `SOURCE_SIZE 0x100000u`. No public size constant exists.
- `rando/include/rando.h`: `rando_generate(gen, options, seed, max_attempts, trace, trace_ctx)` → `RANDO_COMPLETE`, `RANDO_UNSUPPORTED`, `RANDO_GAVE_UP`. `RandoGen` is caller-owned (every host and test keeps it `static`). Under `RANDO_FAULTS` (set by `tests/rando/Makefile`'s `CFLAGS`), `gen->fault` / `fault_attempts` survive `rando_generate`'s entry, so a test can force `RANDO_GAVE_UP` (prior art: `test_generate`). Step 1.3: no option combination returns `RANDO_UNSUPPORTED`.
- `rando/include/rando_options.h`: `rando_options_init` (VT's default preset, hints on), `rando_options_set(options, key, value)` over the eleven `RandoOptionKey`s, `rando_option_key_find`, `rando_option_key_name`, `rando_option_value_name`. `hints` is a `RandoOptions` field "not a `RandoOptionKey`"; there is no name table for it nor for the heart beep in the library.
- `rando/host/case_reader.c`: its own `heart_beep_names[]` and a `strcmp` on `"on"`/`"off"` for hints (`set_case_key`); defaults `hints` on, `heart_beep` half; `test_case_reader` covers the case format.
- `rando/host/main/rando_gen.c`: the only host program; `static RandoGen gen`; watchdog `ORACLE_WATCHDOG 1000u`; exit codes 0 complete, 1 input, 2 usage, 3 incomplete, 4 unsupported, 5 gave up.
- Goldens: each of the 50 cases (`tests/rando/cases/`) and 23 combinations (`tests/rando/combos/`) holds `config.json`, `seed`, `attempts` and `rom_writes.txt` (`%06X <HEX> <method>`, longest line ~31 KB of hex, the whole write log, heart beep last). `test_rom_sram --fixtures` already reads `rom_writes.txt` (prior art for a reader).
- `tests/rando/Makefile`: library sources by wildcard (`rando/src/*.c`), host sources by wildcard (`rando/host/*.c`, not `main/`), one binary per `UNIT_TESTS` entry, `LOGIC_TESTS` = `UNIT_TESTS` minus an explicit filter-out list; `e2e-arm` cross-builds `rando_gen` with `$(RANDO_SRC) $(HOST_SRC)`, so every new host source joins the cross build. Unit tests that need files use `mkdtemp` under `/tmp` (`test_case_reader`) or `tmpfile()` (`test_rom`).
- No MD5 in the tree (`git grep -i md5` hits only docs and comments).

## Hypotheses on Phase 2 step 1 (not done yet)

Phase 2 step 1 (`docs/prd/triage/phase2-step1-bps-applier.md`) is not done when this PRD is written. Before chunk 1, the implementer checks each hypothesis against the tree and records in the step note whether it held; one that does not hold is handled by the rule given with it.

- **H-2.1-1**: a public `rando_bps.h` exists with `RandoBpsResult` (`RANDO_BPS_OK`, `NOT_BPS`, `MALFORMED`, `PATCH_CRC`, `SOURCE_SIZE`, `SOURCE_CRC`, `TARGET_TOO_SMALL`, `TARGET_CRC`), `RandoBpsHeader` (`source_size`, `target_size`, the three CRC32s, metadata span) and `rando_bps_read_header(patch, patch_size, &header)` / `rando_bps_apply(patch, patch_size, source, source_size, target, target_capacity, &target_size)`. *If names or codes differ*, use the tree's; this PRD's pipeline only forwards the BPS code it gets and never interprets its values beyond `OK`.
- **H-2.1-2**: `rando_bps_apply` writes straight into the caller's `target` (no third buffer), requires `source_size` equal to the declared size, never writes past the declared target size, leaves `source` unchanged, requires `target` not to overlap `source` (the plan-review addition), and sets `*target_size` to the declared size on `OK`, 0 otherwise. *If it accepts a target larger than declared but needs exact equality instead*, pass exactly `RANDO_ROM_SIZE`, which the pipeline does anyway.
- **H-2.1-3**: `rando_bps_read_header` checks magic, structure and the patch CRC32, and fills the header only on `OK`. *If it does not check the patch CRC32*, the pipeline's header check still forwards whatever it returns, and `rando_bps_apply` catches a damaged patch later; the order tests below then expect the code `rando_bps_apply` returns.
- **H-2.1-4**: a public `rando_crc32.h` exists (`rando_crc32(bytes, size)`), one function behind one header and one source with a constant table. The MD5 module of this step follows that shape. *If CRC32 lives elsewhere*, MD5 still gets its own header and source.
- **H-2.1-5**: step 1 added no ROM-size constant, no MD5, and did not touch `rando_rom.h`, `case_reader.c` or the option name tables. *If it added size constants*, reuse them under the names this PRD gives (rename if needed) and keep a single pair.
- **H-2.1-6**: step 1 registered `test_crc32` and `test_bps` in `UNIT_TESTS` and in the `LOGIC_TESTS` filter-out list, and did not change the registration convention. *If it changed*, follow the new one.

## Problem Statement

The owner wants to play a randomizer seed made from their own JP 1.0 ROM. At this point the C library can generate a seed that matches VT draw for draw and produce VT's exact write log, and it has VT's checksum and the browser's default cosmetics, but nothing chains them: nothing checks that the ROM is the right one, applies the base patch into a 2 MiB image, verifies the image is VT's base build, lays the seed's writes and the defaults over it, fixes the checksum and writes a `.sfc`. Without that, no playable file exists, a wrong ROM or a damaged patch would go unnoticed, and the app of Phase 3 has nothing to call. The owner also needs a way to do it on a PC, from the command line, to check a ROM before the app exists.

## Solution

One library call, the **assembly pipeline**, takes the JP ROM bytes, the base patch bytes, the two expected MD5 digests, the settings (options, hints, heart beep), a seed and caller-owned working memory, and runs every step in a fixed order: source size and MD5, patch header sizes, BPS apply into a separate 2 MiB buffer, base MD5, generation, VT's write log, the browser defaults (the Link sprite read from the still-unchanged JP ROM), the checksum, then hands the finished 2 MiB image to a caller-supplied output function. Each failure has its own status, so the CLI now and the app later show a precise message. It allocates nothing and touches no file. A small MD5 module, public ROM and source size constants, and library name tables for hints and heart beep support it.

On top, a host CLI, `rando_patch --base <jp.sfc> --out <file> [--bps <patch>] [--seed <n>] [key=value ...]`, reads the files, parses settings with the library's own name tables, calls the pipeline with §2's digests and writes the `.sfc`. A header check, shared by the pipeline and by Phase 4's gate, confirms a patch declares 1 MiB → 2 MiB. Everything is proven on synthetic ROMs and patches, and the written ROM is checked against the goldens' own write logs.

## User Stories

1. As the owner, I want one command on my PC that turns my JP 1.0 ROM into a playable randomizer `.sfc`, so that I can try a seed before the console app exists.
2. As the owner, I want the command to refuse a ROM that is not the JP 1.0 (wrong MD5), with a message saying so, so that I never play a broken game.
3. As the owner, I want a ROM of the wrong size (a US ROM, a headered dump, an already-resized copy) refused with a size message, so that I know what is wrong with my file.
4. As the owner, I want a damaged or wrong base patch refused with a clear message, so that a bad SD-card copy never produces a ROM.
5. As the owner, I want a patch built for another source size (e.g. made from the 2 MiB resized ROM) refused before anything is applied, so that the patch I built with flips is checked the way step 0.1's note says it must be made.
6. As the owner, I want the base-patched image checked against VT's base build MD5 `edc01f3d…`, so that every seed is written over exactly the ROM alttpr.com writes over.
7. As the owner, I want every setting alttpr.com offers, plus hints and heart beep, settable on the command line with the same names as the case files, so that I don't learn a second vocabulary.
8. As the owner, I want unspecified settings to take VT's defaults (and heart beep half, hints on), so that a plain command makes a standard seed.
9. As the owner, I want an unknown setting name or value refused with the offending word in the message, so that a typo does not silently give another seed.
10. As the owner, I want a setting given twice refused, so that I never wonder which one won.
11. As the owner, I want a random seed by default and the seed printed, so that every run is a new game and I can still regenerate one.
12. As the owner, I want `--seed <n>` to make a run reproducible, so that I can compare two builds of the tool.
13. As the owner, I want the base patch path to default to the repository's `data/base.bps`, with `--bps` to point elsewhere, so that the usual command is short.
14. As the owner, I want a write failure on the output file reported and no half-written `.sfc` left behind, so that a full disk never leaves a broken ROM in my list.
15. As the owner, I want the output ROM's internal checksum correct, so that emulators and tools don't flag it.
16. As the owner, I want the default Link sprite in my ROM to be the JP ROM's own graphics, as on alttpr.com, so that Link looks right.
17. As the owner, I want the browser defaults (quickswap off, music on, flashing normal, menu speed normal, red hearts) in every ROM, so that it plays like an alttpr.com download with default cosmetics.
18. As the owner, I want my chosen heart beep written, so that the one cosmetic I asked for takes effect.
19. As the owner, I want my JP ROM file never modified, so that I can generate seeds forever from the same file.
20. As the console app (Phase 3), I want one library call that does everything from bytes to a finished image, so that the app cannot forget or misorder a step.
21. As the console app, I want the pipeline to allocate nothing and use working memory I own, so that it runs on the Miyoo with static buffers.
22. As the console app, I want a distinct status for each failure (source size, source MD5, patch, patch sizes, base MD5, unsupported, gave up, output), so that each maps to its own translated message.
23. As the console app, I want the BPS result code alongside a patch failure, so that a detailed reason is available for a log line.
24. As the console app, I want the output delivered through a function I supply, so that I can write to the SD card my own way and report its failure.
25. As the console app, I want the generation's trace hook passed through, so that Phase 3 step 5 can show progress per generator stage.
26. As the console app (Phase 2 step 3), I want the completed generation left in the working memory after success, so that I can name the ROM and write the history record and spoiler from it.
27. As the console app, I want the expected digests as inputs, with the §2 values available from the library as one shared default, so that the app and the CLI cannot drift to different values.
28. As the console app, I want the source and ROM sizes as public constants, so that my buffers and checks use the same numbers as the library.
29. As the Phase 4 gate, I want a header check that says whether a patch declares a 1,048,576-byte source and a 2,097,152-byte target, using the same constants the pipeline feeds it, so that `data/base.bps` is checked with the shipped code.
30. As the Phase 4 gate, I want a make target that runs that check on a file path, so that the gate is one command once `data/base.bps` exists.
31. As the history record (step 3) and the UI (Phase 3), I want hints and heart beep to have name tables in the library like the options, so that every place that shows or stores a setting uses the same strings.
32. As the case reader, I want to use the library's hints and heart-beep names instead of my own list, so that the case format and the CLI cannot diverge.
33. As a porting agent, I want the pipeline's order of operations fixed and documented in its header, so that nobody moves the checksum before the defaults or the sprite copy after the source is reused.
34. As a porting agent, I want the source MD5 checked before the patch is looked at, so that a wrong ROM is reported as a wrong ROM, not as a patch error.
35. As a porting agent, I want the base MD5 checked before the generator runs, so that no generation time is spent on a bad base.
36. As a porting agent, I want generation failures (gave up, unsupported) to stop before any output, so that no ROM without a seed is ever delivered.
37. As the net, I want a synthetic base ROM and synthetic BPS to drive the pipeline's success path, so that it is tested without any Nintendo data.
38. As the net, I want each error path tested on its own, with proof that later steps did not run (no generator draw, no output call), so that the order is proven, not assumed.
39. As the net, I want the written ROM compared byte for byte against the patched base with a golden `rom_writes.txt` replayed, then the defaults, then the checksum, so that the ROM is proven to carry exactly VT's writes.
40. As the net, I want every case and combination replayed that way in `make logic`, so that every setting combination's ROM is checked.
41. As the net, I want a synthetic patch that writes other bytes over the sprite ranges to come out with the source's bytes there, so that the sprite is proven to be read from the untouched JP ROM.
42. As the net, I want the source buffer proven byte-identical after the call, so that the "only read" contract is checked.
43. As the net, I want MD5 checked against RFC 1321's seven test-suite values, block-boundary lengths and a longer buffer from Python's `hashlib`, so that the digest is proven independently of the pipeline.
44. As the net, I want one test to check that the CLI's default request carries §2's two digests, so that a typo in a constant cannot pass.
45. As the net, I want the CLI tested end to end on synthetic files (with test digests), including a refused output path, so that file handling and exit codes are proven.
46. As the net, I want `make test` to stay a few seconds, so that the fast loop stays fast.
47. As the net, I want the case reader's existing tests to stay green unchanged after it switches to the library's name tables, so that the refactor is proven behaviour-neutral.
48. As the net, I want the new library and host code to build under `lint` and the Onion cross compiler, so that the end of Phase 2 can run these tests under qemu-arm.
49. As a legal repository, I want no test to need the real ROM or `data/base.bps`, so that CI stays clean and runs without them.

## Implementation Decisions

### Library additions (pure C99, no allocation, no mutable static state)

- **Size constants in `rando_rom.h`**: `RANDO_ROM_SIZE` (`0x200000u`) and `RANDO_ROM_SOURCE_SIZE` (`0x100000u`). `rom_checksum.c` and `rom_cosmetics.c` drop their private defines and use these; their documented contracts are unchanged. Unit tests keep their own literal sizes (an independent check), except where a test checks the constants themselves.
- **Heart-beep names in `rando_rom.h`**: `RANDO_HEART_BEEP_COUNT` appended to `RandoHeartBeep`; `const char *rando_heart_beep_name(RandoHeartBeep)` (asserts the range) and `bool rando_heart_beep_find(const char *name, RandoHeartBeep *out)` (false and `*out` untouched on an unknown name). Names: `off`, `half`, `quarter`, `double`, `normal`, as the case format.
- **Hints names in `rando_options.h`**: `const char *rando_hints_name(bool hints)` (`on` / `off`) and `bool rando_hints_find(const char *name, bool *out)` (same contract). `rando_options_set` still never sets `hints`.
- **`case_reader.c`** uses these two `find`s in place of its own list and `strcmp`s; error messages and defaults stay byte-identical (`test_case_reader` unchanged).
- **MD5 module** (`rando_md5.h`, one source): `#define RANDO_MD5_SIZE 16` and `void rando_md5(const uint8_t *bytes, size_t size, uint8_t digest[RANDO_MD5_SIZE])`; `bytes` may be NULL when `size` is 0. One-shot over a buffer (the pipeline never streams), RFC 1321's algorithm with constant tables (the 64 sine constants and shift amounts as `const` arrays), lengths up to `SIZE_MAX` bytes, the bit length encoded as a 64-bit little-endian count. No hex formatting in the library.
- **Assembly module** (`rando_assemble.h`, one source). Interface (shape decided here; names may be adjusted to the tree's style):

  ```c
  typedef enum RandoAssembleStatus {
  	RANDO_ASSEMBLE_OK,
  	RANDO_ASSEMBLE_SOURCE_SIZE,  /* source is not RANDO_ROM_SOURCE_SIZE bytes */
  	RANDO_ASSEMBLE_SOURCE_MD5,   /* not the expected JP ROM */
  	RANDO_ASSEMBLE_PATCH,        /* the BPS module refused it: see *bps */
  	RANDO_ASSEMBLE_PATCH_SIZES,  /* a valid BPS declaring other sizes */
  	RANDO_ASSEMBLE_BASE_MD5,     /* patched image is not the expected base build */
  	RANDO_ASSEMBLE_UNSUPPORTED,
  	RANDO_ASSEMBLE_GAVE_UP,
  	RANDO_ASSEMBLE_OUTPUT,       /* the output function returned false */
  } RandoAssembleStatus;

  /* Returning false reports a failed write. rom is only valid during the call. */
  typedef bool (*RandoAssembleOutputFn)(void *ctx, const uint8_t *rom, size_t size);

  typedef struct RandoAssembleRequest {
  	const uint8_t *source; size_t source_size;   /* the JP ROM, header stripped */
  	const uint8_t *patch;  size_t patch_size;    /* the base BPS */
  	uint8_t source_md5[RANDO_MD5_SIZE];
  	uint8_t base_md5[RANDO_MD5_SIZE];
  	RandoOptions options;                         /* hints included */
  	RandoHeartBeep heart_beep;
  	uint32_t seed;
  	uint32_t max_attempts;                        /* rando_generate's; 0 unbounded */
  	RandoTraceFn trace; void *trace_ctx;          /* may be NULL */
  	RandoAssembleOutputFn output; void *output_ctx;
  } RandoAssembleRequest;

  typedef struct RandoAssembleWork {
  	uint8_t rom[RANDO_ROM_SIZE];
  	RandoGen gen;
  } RandoAssembleWork;

  extern const uint8_t rando_assemble_jp10_md5[RANDO_MD5_SIZE];  /* 03a63945… */
  extern const uint8_t rando_assemble_base_md5[RANDO_MD5_SIZE];  /* edc01f3d… */

  /* Fills §2's digests, VT's default options (hints on), heart beep half, max_attempts
   * RANDO_ASSEMBLE_MAX_ATTEMPTS; every pointer NULL, sizes and seed 0. */
  void rando_assemble_request_init(RandoAssembleRequest *request);

  RandoAssembleStatus rando_assemble_check_patch(const uint8_t *patch, size_t patch_size, RandoBpsResult *bps);

  RandoAssembleStatus rando_assemble(const RandoAssembleRequest *request, RandoAssembleWork *work,
  				   RandoBpsResult *bps);
  ```

- **`rando_assemble_request_init` is the one place the §2 digests enter a request.** The CLI uses it, and the app (Phase 3/4) must too; tests override the digests after it for synthetic inputs. `RANDO_ASSEMBLE_MAX_ATTEMPTS` is 1000, `rando_gen`'s watchdog (every golden wins at attempt 0; 1000 bounds a pathological seed on the console).
- **Order of operations in `rando_assemble`** (stated in the header; each step runs only when every earlier one succeeded):
  1. `source_size == RANDO_ROM_SOURCE_SIZE`, else `SOURCE_SIZE`.
  2. MD5 of the source equals `source_md5`, else `SOURCE_MD5`.
  3. `rando_assemble_check_patch`: `rando_bps_read_header` (a refusal → `PATCH`), then declared source size `RANDO_ROM_SOURCE_SIZE` and target size `RANDO_ROM_SIZE`, else `PATCH_SIZES`.
  4. `rando_bps_apply(patch, source, work->rom, RANDO_ROM_SIZE)`; a refusal → `PATCH` (with the BPS code), including a source CRC32 mismatch on a source whose MD5 matched the caller's digest (only reachable with a wrong patch).
  5. MD5 of `work->rom` equals `base_md5`, else `BASE_MD5`.
  6. `rando_generate(&work->gen, &options, seed, max_attempts, trace, trace_ctx)`: `UNSUPPORTED` / `GAVE_UP` map to their statuses.
  7. `rando_rom_write(&work->gen, heart_beep, …)` through `rando_rom_buffer_write` on `work->rom` (heart beep is its last group, step 1.11). A refused write cannot happen (every address is below 2 MiB and the buffer is 2 MiB): asserted, no status.
  8. `rando_rom_write_cosmetic_defaults(work->rom, source, source_size)` — the source is still the unchanged JP ROM here. Its false return cannot happen after step 1: asserted.
  9. `rando_rom_update_checksum` (asserted true).
  10. `output(output_ctx, work->rom, RANDO_ROM_SIZE)`; false → `OUTPUT`.
- **Contracts** (in the header): every buffer is the caller's; nothing is allocated, kept or freed. `source` and `patch` are only read and must not overlap `work`. `*bps` (may be NULL) is set to the BPS module's code when a BPS call ran, `RANDO_BPS_OK` otherwise. On any status but `OK`, `work->rom` holds unspecified bytes and the output function was not called (except for `OUTPUT`, where it was called once). On `OK` and `OUTPUT`, `work->gen` holds the complete generation (for step 3's naming, record and spoiler) and `work->rom` the finished image. `output` must not be NULL.
- **The pipeline does no file I/O and no seed drawing**: the caller supplies the seed. On the device, the app draws it (Phase 3); on the host, the CLI does.
- **No headered ROMs**: a 512-byte copier header is not stripped; such a file fails `SOURCE_SIZE`, and the CLI's message says the file must be the headerless 1,048,576-byte JP 1.0. (VT's start page requires the headerless JP ROM; stripping is out of scope.)
- **No status-to-string function in the library**: user-facing text belongs to Phase 3's translation table; the CLI has its own English messages.

### Host additions

- **Host module** (`rando/host/`, header + source, joining `librando_host` by wildcard): the CLI's logic, testable without a process.
  - `RandoPatchArgs`: base path, out path, bps path (default `data/base.bps`, relative to the working directory), whether a seed was given and its value, and a `RandoAssembleRequest` prepared by `rando_assemble_request_init` with the parsed options, hints and heart beep.
  - `bool rando_patch_parse_args(int argc, char **argv, RandoPatchArgs *args, char error[…])`: `--base <path>` and `--out <path>` required; `--bps <path>` and `--seed <n>` (decimal uint32, the case reader's `rando_case_parse_uint32` rule) optional; every other argument is `key=value`. Keys: the eleven `RandoOptionKey` names through `rando_options_set`, `hints` through `rando_hints_find`, `heart_beep` through `rando_heart_beep_find`. Missing keys keep the request's defaults. Refused: an unknown key or value (message names it), a key given twice, a flag given twice or missing its value, an argument that is neither a known flag nor `key=value`, a missing `--base` or `--out`.
  - `int rando_patch_run(const RandoPatchArgs *args, FILE *out, FILE *err)`: reads the base file (refuses a file larger than `RANDO_ROM_SOURCE_SIZE` without reading the rest, so a 2 MiB or headered file reports its size) and the patch file (cap 4 MiB) into static buffers, draws a seed if none was given (4 bytes from `/dev/urandom` through `fopen`, falling back to `time` and `clock` mixed; C99 only, no POSIX call, so it cross-builds), calls `rando_assemble` with a file output function, and prints `seed <n>` and the output path to `out`. The output function opens `<out>` with `"wb"`, writes, closes; on any failure it removes the partial file. Exit codes follow `rando_gen`'s: 0 success, 1 input (unreadable file, size, MD5, patch, output), 2 usage, 4 unsupported, 5 gave up. Each status has one English message on `err`, prefixed `rando_patch: ` and naming the file it concerns; `PATCH` adds the BPS code's name.
  - The static working memory (`RandoAssembleWork`, source and patch buffers, ~7 MiB) lives in the host module, file scope, as `rando_gen` keeps its `RandoGen`.
- **CLI program** `rando/host/main/rando_patch.c`: `main` parses, prints usage and returns 2 on a parse error, else returns `rando_patch_run(&args, stdout, stderr)`. Built by `tests/rando/Makefile` as `build/rando_patch` (`-O2`, like `rando_gen`), a prerequisite of `test` so it always links. Usage text: `usage: rando_patch --base <jp.sfc> --out <file> [--bps <patch>] [--seed <n>] [key=value ...]`.
- **Header-size check for Phase 4's gate**: a test binary `test_patch_header` that, with no argument, runs its synthetic checks (in `make test`), and with `--file <path>` reads that file and checks `rando_assemble_check_patch` returns `OK`, printing the declared sizes and source CRC32 (the latter for the user to compare with Phase 0's header checks; not asserted here). A `patch-header` target in `tests/rando/Makefile` (`BPS ?= ../../data/base.bps`) runs it on a file, and the root `Makefile` forwards it; **it is not in `check`** (the file does not exist yet; Phase 4 step 2 wires it into its gate). The root `help` gains its line and `CLAUDE.md`'s "Not in the net" list names it.

## Implementation Chunks

1. **Constants and name tables.** `RANDO_ROM_SIZE`, `RANDO_ROM_SOURCE_SIZE` used by the checksum and the cosmetics; `RANDO_HEART_BEEP_COUNT`, `rando_heart_beep_name` / `find`; `rando_hints_name` / `find`; `case_reader.c` switched to them. Green: new name-table tests in `test_rom` and `test_options`; `test_case_reader`, `test_rom_checksum`, `test_rom_cosmetics` unchanged and green; `make check` green.
2. **MD5.** `rando_md5.h` and its source; `test_md5` registered (filtered out of `LOGIC_TESTS`). Green: RFC 1321 values, block-boundary and `hashlib` buffers.
3. **Patch header check and the gate target.** `rando_assemble.h` with the status enum, the digest constants, `rando_assemble_request_init` and `rando_assemble_check_patch` (no pipeline yet); `test_patch_header` (synthetic mode and `--file`), the `patch-header` targets, help and `CLAUDE.md` line. Green: the header tests; `make patch-header BPS=<synthetic file written under tmp/>` passes by hand, recorded in the step note.
4. **The pipeline.** `rando_assemble`; `test_assemble` with the synthetic success path, every error path and order test, the source-unchanged and sprite tests, the golden-replay test on one case in `test`, every case and combination with `--fixtures` (in `LOGIC_TESTS`). Green: `make test`, `make logic`.
5. **The CLI.** Host module, `rando_patch` program and its build rule, `test_patch_cli` (filtered out of `LOGIC_TESTS`). Green: `make test`, `make check`; `build/rando_patch` with no argument prints the usage and exits 2.

## Testing Decisions

- **Good tests** go through public interfaces only (`rando_md5`, the name-table functions, `rando_assemble_check_patch`, `rando_assemble`, `rando_patch_parse_args`, `rando_patch_run`) and check statuses, exit codes, messages' key words and output bytes, never a static function. One behaviour per test, named after it. Failures through `support.h`'s `test_fail`. Prior art: `test_rom_cosmetics` (static 2 MiB buffers, `expected` image compared byte for byte, sizes refused), `test_rom_checksum` (independently computed values pasted as literals), `test_case_reader` (`mkdtemp` tree, file errors), `test_generate` (forced `RANDO_GAVE_UP` through `gen.fault`), `test_rom_sram --fixtures` (reading every golden's `rom_writes.txt`).
- **New binaries**: `test_md5`, `test_patch_header`, `test_assemble`, `test_patch_cli` in `UNIT_TESTS`; all but `test_assemble` in the `LOGIC_TESTS` filter-out list. `test_assemble --fixtures` runs in `make logic`.
- **Name tables** (chunk 1): every heart-beep value's name round-trips through `find`; `find` refuses an unknown name, an empty string and a case variant (`Half`) and leaves `*out` unchanged; same for hints (`on`, `off`). The constants equal `0x200000` and `0x100000` (checked once, in `test_rom`).
- **`test_md5`**: RFC 1321 A.5's seven values (`""`, `"a"`, `"abc"`, `"message digest"`, the alphabet, `A–Z a–z 0–9`, eight times `"1234567890"`); lengths 55, 56, 63, 64, 65 and 119, 120 (padding boundaries) of a fixed pattern; one 1 MiB + 13 byte synthetic buffer (e.g. `(i * 31 + (i >> 9)) & 0xFF`); each non-RFC digest computed with Python's `hashlib.md5` when the test is written, pasted as a literal, the command in the commit message.
- **Synthetic inputs for the pipeline** (`test_assemble`, `test_patch_header`, `test_patch_cli`): a 1 MiB synthetic source pattern, and a small synthetic BPS built by a test-local encoder that declares 1 MiB → 2 MiB: one SourceRead of the whole source, a TargetRead of a short distinct pattern, a TargetCopy that repeats it to fill 2 MiB, footer CRCs from `rando_crc32` (H-2.1-4). Variants: TargetReads placing distinct bytes over the three sprite ranges; a patch declaring 2 MiB → 2 MiB; one declaring 1 MiB → 2 MiB − 1; one with a flipped body byte. The expected base image is built by the test from the same recipe in plain C loops (not through `rando_bps_apply`), and the request's digests are `rando_md5` of the synthetic source and of that expected image. The base image sets `0x7FE2` and the `02 37` author guards to fixed values so the cosmetic branch taken is known.
- **`test_assemble` — success and content**:
  - *Golden replay*: for the `baseline` case (in `test`) and every case and combination (`--fixtures`), read `config.json` / `seed` / `attempts` with `rando_case_read`, run the pipeline (`max_attempts` = attempts + 1), and compare the delivered image with: the expected base image, every `rom_writes.txt` line applied in order (a parser for `%06X <HEX> <method>`, independent of `rando_rom_write`), then `rando_rom_write_cosmetic_defaults` with the synthetic source, then `rando_rom_update_checksum`. Byte-for-byte equality over 2 MiB; on a difference, print the first differing address, both bytes, and the case name.
  - The output function is called exactly once, with size `RANDO_ROM_SIZE`; status `OK`; `*bps == RANDO_BPS_OK`; `work->gen` holds the case's seed and a complete generation (its `attempt` equals the golden's).
  - The delivered image's checksum is self-consistent (recomputing `rando_rom_update_checksum` on a copy changes nothing), proving the checksum ran last.
  - *Sprite*: with the sprite-overwriting patch, the delivered image holds the synthetic source's bytes at `0x80000..+0x7000`, `0xDD308..+120`, `0xDEDF5..+4`, and the patch's own bytes there differ from the source's (checked first, so the test proves the copy).
  - *Source unchanged*: the source buffer equals a copy taken before the call, after a success and after each failure.
  - *Trace pass-through*: a counting trace hook sees draws on success.
- **`test_assemble` — every error path, each checking the status, `*bps`, zero output calls (or one for `OUTPUT`), and zero trace calls for every status before generation**:
  - `SOURCE_SIZE`: source one byte short, one byte long, 2 MiB.
  - `SOURCE_MD5`: right size, one byte changed in the digest.
  - `PATCH`: a truncated patch (`*bps` the header code), a flipped body byte (`PATCH_CRC`, H-2.1-3), a source the digest accepts but the patch's source CRC32 rejects (digest set to that source's MD5; `*bps == RANDO_BPS_SOURCE_CRC`).
  - `PATCH_SIZES`: the 2 MiB → 2 MiB patch and the 1 MiB → 2 MiB − 1 patch.
  - `BASE_MD5`: a wrong base digest.
  - `GAVE_UP`: `work->gen.fault` set with `max_attempts` 1 (`RANDO_FAULTS`), output not called.
  - `OUTPUT`: an output function returning false.
  - `UNSUPPORTED`: unreachable through options today (step 1.3: none); not tested, noted in the step note.
- **Order tests**: wrong source MD5 *and* a damaged patch → `SOURCE_MD5`; wrong source size *and* wrong source MD5 → `SOURCE_SIZE`; `PATCH_SIZES` patch *and* wrong base digest → `PATCH_SIZES`; wrong base digest *and* a forced give-up → `BASE_MD5` with zero trace calls.
- **`test_patch_header`**: `rando_assemble_check_patch` on a synthetic 1 MiB → 2 MiB header gives `OK` (its numbers encoded `00 7F BE` / `00 7F FE`); 2 MiB → 2 MiB, 1 MiB → 1 MiB and 1 MiB + 1 → 2 MiB give `PATCH_SIZES`; a wrong magic gives `PATCH` with the BPS code. `--file` mode on a synthetic file it writes itself under `mkdtemp`, then on a missing path (exit 1). This is the test Phase 4 step 2's gate runs on `data/base.bps`.
- **`test_patch_cli`**:
  - Parsing: `--base`/`--out` only gives `rando_assemble_request_init`'s request, **whose digests equal `03a63945398191337e896e5771f77173` and `edc01f3db798ae4dfe21101311598d44`** (compared against hex strings in the test, the §2 check), bps path `data/base.bps`, no seed; every one of the 13 keys at a non-default value lands in the request; each refusal listed above gives false and a message naming the word.
  - Running: in a `mkdtemp` tree, synthetic `jp.sfc` and `base.bps`, digests overridden to the synthetic ones, `--seed 1` → exit 0, the output file is 2,097,152 bytes and equals what `rando_assemble` delivers for the same request, `seed 1` printed. A missing base file, a 2 MiB base, a wrong MD5, a damaged patch → exit 1 with the matching message; an output path in a missing directory → exit 1 and no file left; a forced give-up is not reachable through the CLI and is covered by `test_assemble`.
- **Speed**: `make test` runs the pipeline about fifteen times (each: ~3 MiB of MD5 and CRC32, one generation, a 2 MiB checksum), well under a second at `-O0`; the 73-case replay is in `make logic` only. `e2e`, `e2e-combos` and `measure` are untouched.
- **Not tested here**: the real `data/base.bps` and any real ROM (absent, and never in CI); the real `edc01f3d…` result, which is the user's confirmation (Phase 0) and Phase 4's gate.

## Out of Scope

- ROM naming (`<Mode>-<Goal>-<code>.sfc`), collisions, the history record and the spoiler file: Phase 2 step 3. The CLI writes to the path it is given and overwrites an existing file.
- The BPS applier and CRC32 themselves (Phase 2 step 1); any change to their behaviour.
- Stripping a copier header, US ROMs, other base builds.
- Any change to the generator, `rando_rom_write`, the cosmetic defaults' bytes, the checksum algorithm, the goldens, the oracle, `e2e` or `check-e2e`.
- Adding `patch-header` to `check` or to a CI job (Phase 4 step 2), and running the new tests under qemu-arm (the end of Phase 2).
- The app, its seed drawing and its SD-card writes (Phases 3–4); user-facing translated messages.
- Streaming MD5 or patching, and any memory-saving scheme beyond caller-owned buffers.

## Further Notes

- Step note for this step: the outcome of H-2.1-1 to H-2.1-6; the `hashlib` command for the MD5 literals; the hand run of `make patch-header` on a synthetic file; that `UNSUPPORTED` and a refused `rando_rom_write` are unreachable and asserted or untested; the CLI's exact usage line, for the README (Phase 5).
- The pipeline runs `rando_generate`, not `rando_generate_until`; no stop stage, no fault, no trace file. The CLI is not a replacement for `rando_gen`: it never writes `trace.txt`, `spoiler.json` or `rom_writes.txt`.
- For the user, once `data/base.bps` exists: `make -C tests/rando build/rando_patch && tests/rando/build/rando_patch --base jp.sfc --out tmp/seed.sfc` gives a real ROM; `make patch-header` checks the patch's declared sizes. That run is the user's, not the agent's (no ROM in the session).
- Memory on the Miyoo: the pipeline needs the 1 MiB source, the patch (~1 MiB expected), the 2 MiB image and a `RandoGen`; all static in the app, well within the device's RAM. Phase 4's performance step may measure it.
- The JP ROM's CRC32 printed by `test_patch_header --file` lets the user tick Phase 0's "source CRC32 equals the JP 1.0 ROM's" header check; the value is not hard-coded anywhere in this step.
