---
title: "BPS applier in C"
description: "The library cannot apply the base patch (a BPS file) to the JP ROM, so Phase 2's pipeline has no way to build the base-patched ROM that every seed write goes into."
status: needs-triage
---

## The step (Plan 1, Phase 2, step 1, copied from `docs/plan-1.md`)

> **BPS applier in C** (no allocation, caller buffers, source and target CRC32 checks). Verified by unit tests on small hand-built BPS files covering every BPS action and each CRC failure.

Context the step relies on, from the plan:

- §2: the base patch is z3randomizer's compiled ASM and English text, applied before any seed write, committed as `data/base.bps` and shipped in the app. Applying it to the JP 1.0 ROM must give MD5 `edc01f3d…` (2,097,152 bytes).
- Step 0.1's note: `data/base.bps` is **not committed** yet (the user builds it with flips). Its source is the **unresized 1 MiB JP ROM**, because VT's browser applies the patch to `originalData` before `resize`, and `bps.ts` `applyPatch` rejects any source whose CRC32 differs from the patch's.
- Phase 2 step 2 (later) checks the source MD5, applies the patch to the 1 MiB ROM into a 2 MiB target, checks MD5 `edc01f3d…`, then runs the seed writes. Phase 2 step 2 and Phase 4 step 2 both need a header reader: a test that the pipeline feeds a source of the declared size (1,048,576) and expects the declared target size (2,097,152).
- Ground rules: `rando/` stays pure C99, no `malloc`, no platform library, built by host gcc and the Onion cross compiler; no Nintendo ROM in the repository or in CI.

## Facts (checked at HEAD)

### VT's reference applier (`resources/js/bps.ts`, VT `219fcafd`)

- `setPatch`: the first four bytes, read as a little-endian `uint32`, must equal `827543618` (`"BPS1"`), else "Not a valid patch file". Then three BPS numbers: source size, target size, metadata length; the metadata (if any) is `JSON.parse`d; the actions start right after it. The last 12 bytes are three little-endian 32-bit values: source CRC32, target CRC32, patch CRC32. The patch CRC32 is checked here, over every byte but the last four: "Patch checksum incorrect".
- `setSource` computes the source's CRC32; `applyPatch` throws "Source checksum incorrect" when it differs from the footer's. **VT never compares the source size with the header's**, and never compares the target size with the header's: it sums the actions' lengths to size its output.
- Actions run while `seek < patch.length - 12`. One BPS number per action: `type = n & 3`, `length = (n >> 2) + 1`.
  - `0` SourceRead: `length` bytes from the source at the **current output offset**.
  - `1` TargetRead: `length` literal bytes from the patch.
  - `2` SourceCopy: a BPS number `d`; `sourceRelativeOffset += (d & 1 ? -1 : 1) * (d >> 1)`; then `length` bytes copied one at a time from the source, the offset advancing by one per byte. The offset persists across SourceCopy actions.
  - `3` TargetCopy: the same with `targetRelativeOffset`, copied one byte at a time from the output already written, so a copy that overlaps its own output repeats a pattern (RLE). Persists across TargetCopy actions.
- Then the output's CRC32 must equal the footer's target CRC32: "Target checksum incorrect".
- `decodeBPS` (the BPS number): `number = 0, shift = 1`; per byte `x`: `number += (x & 0x7F) * shift`; stop if `x & 0x80`; else `shift <<= 7; number += shift`. Encodings: `80` = 0, `81` = 1, `FF` = 127, `00 80` = 128, `7F 80` = 255, `00 7F BE` = 1,048,576, `00 7F FE` = 2,097,152. VT has no guard against a number that never ends or overflows (its own `@todo`), nor against reads outside the source or target (a JS out-of-range read gives `undefined`, stored as 0).
- CRC32 is zlib's (`@stardazed/zlib`): reflected polynomial `0xEDB88320`, initial and final XOR `0xFFFFFFFF`. Check values: `""` → `0x00000000`, `"a"` → `0xE8B7BE43`, `"123456789"` → `0xCBF43926`.

### The C library today

- No CRC32, MD5 or BPS code anywhere in `rando/` or `tests/rando/` (`git grep -i 'crc\|md5'` is empty).
- Public headers are `rando/include/rando_*.h`, sources `rando/src/*.c`; `tests/rando/Makefile` picks every source up by wildcard, so a new source joins the unit build and `lint` (`-std=c99 -pedantic-errors`) with no Makefile edit.
- Unit tests: one binary per module, listed in `UNIT_TESTS`; `LOGIC_TESTS` is `UNIT_TESTS` minus an explicit filter-out list of tests with no fixture mode (`make logic` runs every other one with `--fixtures`, and `test_fixtures_mode` exits 2 on an argument it doesn't know). Failures go through `support.h`'s `test_fail`.
- `rando_rom.h` holds `RandoRomBuffer` (caller-owned bytes and size); the library keeps no mutable static state and does not allocate.

## Hypotheses on steps not done yet

Phase 1 steps 11 and 12 are not done when this PRD is written. This step does not depend on their outcome except for the library's layout and test conventions. Before chunk 1, the implementer checks each hypothesis against the tree and records in the step note whether it held; one that doesn't hold is handled by the rule given with it.

- **H-1.11-1**: step 11 (`docs/prd/triage/phase1-step11-checksum-heart-beep.md`) added no CRC32, MD5 or BPS code to `rando/`. *If it did*, reuse its CRC32 if it is zlib's CRC32 behind a public header, and drop this step's own; anything else is left alone.
- **H-1.11-2**: `rando/` is still pure C99, with no allocation, caller-owned buffers, no mutable static state, and new sources picked up by `tests/rando/Makefile`'s wildcard. Step 11 adds `test_rom_checksum` and `test_rom_cosmetics` to `UNIT_TESTS` (and possibly to the `LOGIC_TESTS` filter-out list); this step only appends to those lists. *If the registration convention changed*, follow the new one.
- **H-1.12-1**: step 12 (`skill: check-e2e`) changed no public interface this step uses (it uses none of the generator's). *If it changed the test registration*, follow it; nothing else in this PRD depends on step 12.

## Problem Statement

The user's console has only the Japanese 1.0 ROM. To make a seed, the app must first turn that ROM into the base-patched 2 MiB ROM that alttpr.com writes every seed into, and the only form the base patch ships in is a BPS file (`data/base.bps`, once the user builds it). The C library has no way to read or apply a BPS file, and no CRC32 to tell a wrong ROM or a damaged patch from a good one. Without it, Phase 2's pipeline cannot start, and a wrong input would give a broken game instead of a clear error.

## Solution

A small, pure C99 module in the randomizer library applies a BPS patch from caller buffers into a caller buffer, with no allocation. It reads the header (declared source and target sizes, metadata span, the three CRC32s) and checks the patch's own CRC32; it rejects a source of the wrong size or the wrong CRC32, a target buffer too small, every malformed or out-of-range action, and a result whose CRC32 differs from the patch's; on success it reports the target size. Each failure has its own result code so the app can show a precise message later. A header-only call lets Phase 2 and Phase 4 check `data/base.bps`'s declared sizes without applying it. CRC32 (zlib's) is public too, for those checks. Everything is proven on small BPS files built by hand in the tests, with no ROM.

## User Stories

1. As the owner, I want the app to apply the base patch on the console itself, so that I only ever copy my JP ROM and never a pre-patched one.
2. As the owner, I want a clear error when my ROM is not the expected JP 1.0 (wrong CRC32 for the patch), so that I know to replace `base.sfc` rather than get a broken game.
3. As the owner, I want a clear error when the shipped patch is damaged (bad patch CRC32), so that a corrupted SD-card copy is caught before any ROM is written.
4. As the owner, I want a patched ROM to be checked against the patch's target CRC32, so that a ROM written to my SD card is exactly the one the patch describes.
5. As the owner, I want the patch applied the same way alttpr.com's browser does, so that the base ROM matches VT's and every seed write lands on the right bytes.
6. As the app (Phase 2 step 2), I want to apply the patch from a 1 MiB source buffer into a 2 MiB target buffer that I own, so that no memory is allocated on the console.
7. As the app, I want one call that applies and checks everything, so that I cannot forget a CRC check.
8. As the app, I want a distinct result for each failure (not a BPS file, malformed, patch CRC, source size, source CRC, target too small, target CRC), so that I can show a precise message to the user later.
9. As the app, I want the produced size returned on success, so that I can check it equals the 2 MiB I expect.
10. As the app, I want the produced size to be 0 on failure and the target's bytes declared unusable, so that I never write a half-patched ROM.
11. As the app, I want a header-only call that returns the declared source and target sizes, so that I can reject a patch whose sizes don't fit my buffers before reading the ROM.
12. As the Phase 4 gate, I want to read `data/base.bps`'s header (sizes 1,048,576 → 2,097,152 and source CRC32) with the library itself, so that the gate checks the real shipped file with the shipped code.
13. As the app, I want the header call to check the patch's own CRC32, as VT's `setPatch` does, so that a damaged patch is caught at the first look.
14. As the app, I want the metadata span reported but not parsed, so that the library needs no JSON parser and a metadata string never makes it fail.
15. As the app, I want a public CRC32 identical to zlib's, so that Phase 2 and Phase 4 can check a ROM or a patch against the CRC32s they record.
16. As a porting agent, I want SourceRead to copy from the source at the current output offset, as VT does, so that the semantics match the reference.
17. As a porting agent, I want TargetRead to copy literal bytes from the patch, so that new data (the compiled ASM, the English text) reaches the target.
18. As a porting agent, I want SourceCopy to move a persistent relative offset by a signed delta before copying, so that flips' patches, which reuse the offset across actions, apply correctly.
19. As a porting agent, I want TargetCopy to copy byte by byte from the output, so that an overlapping copy repeats a pattern as BPS's RLE requires.
20. As a porting agent, I want SourceCopy and TargetCopy offsets kept separately, so that one never moves the other.
21. As a porting agent, I want BPS numbers decoded exactly as VT's `decodeBPS`, including the `+shift` step, so that multi-byte sizes and offsets decode to the same values.
22. As a security-minded owner, I want every read and write bounded by its buffer, so that a hostile or damaged patch can never read or write outside memory.
23. As a security-minded owner, I want a BPS number that never ends before the footer, or that overflows, rejected, so that the loop VT warns about (`@todo … while(true)`) cannot hang or wrap in C.
24. As a security-minded owner, I want an action that would write past the declared target size rejected, so that the target buffer can never overflow.
25. As a security-minded owner, I want a SourceRead past the source's end, and a SourceCopy offset outside the source, rejected, so that no undefined bytes are read (VT silently writes 0 there).
26. As a security-minded owner, I want a TargetCopy that reads at or past the bytes already written rejected, so that no uninitialised byte is copied.
27. As the app, I want a patch whose actions produce fewer bytes than the declared target size rejected, so that a short patch cannot pass with stale bytes at the end.
28. As the app, I want the source size checked against the header before its CRC32, so that a 2 MiB copy passed by mistake gives "source size", not a CRC mismatch.
29. As the app, I want the checks in VT's order (magic, header, patch CRC, source, actions, target CRC), so that the error for a given bad input is the one VT would raise first.
30. As the library, I want the module pure C99 with no allocation and no mutable static state, so that it builds with host gcc, under `-pedantic-errors`, and with the Onion cross compiler.
31. As the library, I want the CRC32 table constant (no lazy initialisation), so that the code holds no mutable static state.
32. As the net, I want every action type covered by a hand-built BPS whose expected output is spelled out, so that each action is proven without a ROM.
33. As the net, I want each CRC failure (patch, source, target) covered by its own hand-built patch, so that each check is proven to fire.
34. As the net, I want each structural failure covered by its own hand-built patch, so that each bound is proven to hold.
35. As the net, I want at least one complete patch written as a literal byte array with CRC32s computed outside the C, so that a bug shared by the test's encoder and the applier cannot pass.
36. As the net, I want CRC32 checked against zlib's published values, so that the CRC is proven independently of the BPS code.
37. As the net, I want a header test that decodes `00 7F BE` / `00 7F FE` as 1,048,576 / 2,097,152, so that the sizes Phase 2 and Phase 4 rely on are decoded right.
38. As the net, I want the tests to run in milliseconds in `make test`, so that the fast loop stays fast.
39. As a legal repository, I want every test on synthetic bytes, never a Nintendo ROM or the real base patch, so that the repository and CI stay clean.
40. As a later agent, I want the module's contracts (who owns which buffer, what is written on failure, what overlaps are forbidden) stated in its header, so that Phase 2 step 2 can call it without reading the source.

## Implementation Decisions

- **New module, BPS**: a public header `rando_bps.h` and one source in `rando/src/`. It depends on nothing else in the library (not on `rando_rom.h`: plain pointers and sizes, so it can be used before any `RandoRomBuffer` exists).
- **CRC32 is its own tiny public module** (`rando_crc32.h`, one source), used by the BPS module and by the later pipeline and gate: `uint32_t rando_crc32(const uint8_t *bytes, size_t size)`. zlib's CRC32 (reflected `0xEDB88320`, init and final XOR `0xFFFFFFFF`); `size == 0` returns 0 and `bytes` may then be NULL. A `const uint32_t[256]` table written in the source (no runtime init, no mutable static); the table is checked by the tests' known values, not by a generator in the build. (H-1.11-1 governs reuse if step 11 already brought one.)
- **Interface** (shape decided here; names may be adjusted to the tree's style):

  ```c
  typedef enum RandoBpsResult {
  	RANDO_BPS_OK,
  	RANDO_BPS_NOT_BPS,         /* shorter than 4 bytes, or not "BPS1" */
  	RANDO_BPS_MALFORMED,       /* any structural fault: see below */
  	RANDO_BPS_PATCH_CRC,
  	RANDO_BPS_SOURCE_SIZE,
  	RANDO_BPS_SOURCE_CRC,
  	RANDO_BPS_TARGET_TOO_SMALL,
  	RANDO_BPS_TARGET_CRC,
  } RandoBpsResult;

  typedef struct RandoBpsHeader {
  	size_t source_size, target_size;
  	size_t metadata_offset, metadata_size; /* within the patch; metadata not parsed */
  	size_t actions_offset;                 /* actions run up to patch_size - 12 */
  	uint32_t source_crc, target_crc, patch_crc;
  } RandoBpsHeader;

  RandoBpsResult rando_bps_read_header(const uint8_t *patch, size_t patch_size, RandoBpsHeader *out);

  RandoBpsResult rando_bps_apply(const uint8_t *patch, size_t patch_size,
  			       const uint8_t *source, size_t source_size,
  			       uint8_t *target, size_t target_capacity, size_t *target_size);
  ```

- **Contracts** (stated in the header): every buffer is the caller's; nothing is allocated, kept or freed. `patch` and `source` are only read. `target` must not overlap `patch` or `source`. `rando_bps_read_header` fills `*out` only on `RANDO_BPS_OK`. `rando_bps_apply` sets `*target_size` to the declared target size on `RANDO_BPS_OK` and to 0 on any failure; on failure, `target[0..target_capacity)` may have been partly written and must be discarded (it is written directly into `target`, with no intermediate buffer — a third buffer would cost 2 MiB; `source` stays unchanged and is needed after the apply). Bytes of `target` past the declared target size are never written.
- **Check order** (VT's, with the C's extra guards at the earliest point they can be checked): magic → header numbers, metadata span and footer fit → patch CRC32 (over `patch[0..size-4)`) — that ends `rando_bps_read_header`, which `rando_bps_apply` calls first → source size equals the declared size → source CRC32 → declared target size ≤ `target_capacity` → actions → produced length equals the declared target size → target CRC32 over the declared target size.
- **Deviations from VT, on purpose** (each a reject where VT would read garbage or loop; none changes the result of a well-formed patch such as flips produces):
  - the source size must equal the header's (VT checks only the CRC32); a matching CRC32 with another size is not a real input, and Phase 2/4 rely on the declared sizes;
  - the declared target size is authoritative: the actions must produce exactly that many bytes (VT sizes its output from the actions);
  - every out-of-range access is `RANDO_BPS_MALFORMED`.
- **`RANDO_BPS_MALFORMED` covers**: the patch shorter than magic + three numbers + 12-byte footer; a BPS number not terminated before the actions' end (or, in the header, before the footer); a BPS number above `SIZE_MAX` or one whose decoding would overflow (`uint64_t` arithmetic, checked before each add and shift); metadata running into the footer; an action length overflowing; an action that would write past the declared target size; a TargetRead whose literal bytes run into the footer; a SourceRead whose range passes the source's end; a SourceCopy whose offset, after the delta, is negative or whose range passes the source's end; a TargetCopy whose offset is negative or not strictly below the current output offset at its start; fewer bytes produced than declared. One code is enough for these: the user-facing message for all of them is "the patch is damaged"; the tests tell them apart by building one patch per fault.
- **Decoding** follows `decodeBPS` exactly, including the `number += shift` after each continuation byte. Relative offsets are `int64_t`-checked: `delta = d >> 1`, negative when `d & 1`.
- **Metadata** is skipped, never parsed or validated (VT `JSON.parse`s it; base patches from flips carry none or a short string). Reported as an offset and size for whoever wants it.
- **Footer CRCs** are read little-endian as `uint32_t` (VT reads them as `Int32` and compares them with `crc32`'s result; with zlib's unsigned result the comparison is the same bit pattern).
- **No result-to-string function** in the library: user-facing text belongs to the UI's translation table (Phase 3), and the host CLI of Phase 2 step 2 maps codes itself.
- **Speed**: one pass over the patch for the actions (no pre-pass to size the output: the size comes from the header), one CRC32 over the source and one over the target. For the real patch that is ~3 MiB of CRC32, a few milliseconds on the host.

## Implementation Chunks

1. **CRC32 and the header reader.** The CRC32 module and its tests; `RandoBpsResult`, `RandoBpsHeader` and `rando_bps_read_header` (magic, the three numbers with overflow and termination guards, metadata span, footer fit, the three CRC32s, the patch CRC32 check); `test_bps` registered. Green: the CRC32 known values, the header tests, the header error tests.
2. **`rando_bps_apply`.** Source size and CRC32, target capacity, the four actions with their bounds, the produced-length check and the target CRC32. Green: one test per action and per fault below, and the literal-bytes patch.

## Testing Decisions

- **Good tests** go through the public interface only (`rando_crc32`, `rando_bps_read_header`, `rando_bps_apply`) and check results, reported sizes and output bytes, never a static function. One behaviour per test, named after it.
- **New binaries** `test_crc32` and `test_bps`, in `UNIT_TESTS` and in the `LOGIC_TESTS` filter-out list (no fixture mode; H-1.11-2). Prior art: `test_rom`'s buffer-writer and `check_buffer` tests (exact bytes, every other byte unchanged), `test_prng`'s fixed vectors; failures through `test_fail`.
- **Patch construction:** a small builder inside `test_bps` (append magic, a BPS number per the encoding above, metadata, actions, then the footer with `rando_crc32`), plus setters to corrupt one footer CRC after the fact. Because the builder shares `rando_crc32` and the number encoding with the code under test, **at least one complete patch is a literal byte array** (all four actions, a negative SourceCopy delta, an overlapping TargetCopy, a non-empty metadata string) whose three CRC32s were computed with Python's `zlib.crc32` when the test was written; the command goes in the commit message. Its expected output is spelled out as a literal too.
- **`test_crc32`:** `""` → `0`, `"a"` → `0xE8B7BE43`, `"123456789"` → `0xCBF43926`, and one longer synthetic buffer (e.g. 256 bytes `0..255`) against a value computed with Python's `zlib.crc32`, pasted as a literal.
- **`test_bps`, header:** sizes decoded from `80` (0), `81` (1), `FF` (127), `00 80` (128), `7F 80` (255), and `00 7F BE` / `00 7F FE` as 1,048,576 / 2,097,152 (the base patch's declared sizes, on a synthetic header with no actions needed to apply); metadata offset and size for an empty and a non-empty metadata; footer CRCs read back; `NOT_BPS` for a 3-byte input and for a wrong magic; `MALFORMED` for a patch cut inside a header number, for a header number that runs into the footer, for an overflowing number (e.g. eleven `00` bytes then `80`), for metadata running into the footer, for a patch shorter than the minimum; `PATCH_CRC` for one flipped footer bit and for one flipped body byte.
- **`test_bps`, apply — one test per action**, each on a few-byte source with every output byte checked: SourceRead (output offset, not a cursor of its own); TargetRead (literal bytes); SourceCopy with a positive delta, then a negative one (the offset persists and advances by the length copied); TargetCopy with a non-overlapping copy and with an overlapping one (`length` > distance, a repeated pattern); SourceCopy and TargetCopy offsets interleaved, proving they are independent; an empty target (declared size 0, no actions). The reported size equals the declared size, and bytes of `target` past it are unchanged.
- **`test_bps`, apply — one test per fault**, each checking the exact code and `*target_size == 0`: `SOURCE_SIZE` (one byte short, one byte long); `SOURCE_CRC` (right size, one byte changed); `TARGET_TOO_SMALL` (capacity one below the declared size); `TARGET_CRC` (a valid patch whose footer target CRC32 is changed and the patch CRC32 recomputed); `MALFORMED` for an action number cut by the footer, a TargetRead running into the footer, an action writing past the declared size, actions producing one byte too few, a SourceRead past the source's end, a SourceCopy offset going negative, a SourceCopy range past the source's end, a TargetCopy offset going negative, a TargetCopy starting at the current output offset; and every header fault again through `rando_bps_apply` (it returns the header's code).
- **Order test:** a patch with both a bad source CRC and a bad target CRC gives `SOURCE_CRC`; a wrong-size source with a bad patch CRC gives `PATCH_CRC`.
- **Speed:** buffers of at most a few KiB; `make test` grows by two binaries running in milliseconds. `lint` covers the new sources by wildcard.
- **Not tested here:** the real `data/base.bps` (absent, step 0.1) and any ROM; Phase 2 step 2 and Phase 4 step 2 test the real sizes and the MD5.

## Out of Scope

- The ROM assembly pipeline: source MD5 (`03a63945…`), resize, the target MD5 (`edc01f3d…`), seed writes, checksum, the `.sfc` and the `rando_patch` CLI (Phase 2 step 2). No MD5 in this step.
- The header-size test on `data/base.bps` and on its synthetic stand-in in the pipeline (Phase 2 step 2, Phase 4 step 2); this step only provides `rando_bps_read_header`.
- Creating BPS files (VT doesn't either: `createPatch` throws); other patch formats (IPS, UPS).
- Parsing or validating the metadata JSON.
- File reading: the module takes bytes; the host and app read files.
- Any change to the generator, `rando_rom`, the goldens, the oracle or `check-e2e`.

## Further Notes

- Step note for this step: the hypotheses' outcomes (H-1.11-1, H-1.11-2, H-1.12-1), the Python command that produced the literal CRC32s, and the deviations from VT listed above (source size, authoritative target size, bounds), so Phase 2 step 2 knows a patch VT would accept with garbage reads is refused here.
- For Phase 2 step 2: call `rando_bps_apply(base_bps, size, jp_rom, 0x100000, rom, 0x200000, &n)` on the **unresized** 1 MiB JP ROM (step 0.1's note), then check `n == 0x200000` before the MD5. The 2 MiB target buffer is the one `rando_rom_write` and step 11's functions then write into.
- When the user commits `data/base.bps`, `rando_bps_read_header` on it should give source 1,048,576, target 2,097,152, and a source CRC32 equal to the JP 1.0 ROM's (Phase 0's header checks); that check belongs to Phase 4 step 2's gate.


## Plan review addition (after Phase 1)

The plan's Phase 2 step 1 now requires the target to be its own buffer and one more per-action test: a SourceCopy back to a source offset below the current output offset, after a TargetRead wrote different bytes there; the output must hold the source's bytes.
