---
title: "Port the heart beep, the checksum and the browser's default writes"
description: "The C ROM writer writes no heart beep, cannot fix a ROM's checksum, and leaves out the writes alttpr.com's browser makes at download, so no case claims its whole write log and Phase 2 has nothing to finish a ROM with."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 11, copied from `docs/plan-1.md`, as amended by plan review)

> **Checksum and heart beep.** Port `Rom::updateChecksum` and `setHeartBeepSpeed`. Verified by a unit test of the checksum algorithm on a synthetic 2 MiB buffer whose expected sum is computed independently, and by every case claiming the whole write log, heart-beep group included (the step 3 cases cover each heart-beep value). Every write step 2's note marks "reproduced in C" has its unit test here.

Step 1.2's note marks these as "reproduced in C, step 11, applied in that order by Phase 2 step 2":

> `setQuickswap` (`0x18004B`=`00`), `setMusicVolume` (`0x18021A`=`00`), `setReduceFlashing` (`0x18017F`=`00`), `setMenuSpeed` normal (`0x180048`=`08`, `0x6DD9A`=`11`, `0x6DF2A`=`12`, `0x6E0E9`=`12`), `setHeartColor` red (branch on base-patched byte `0x7FE2`), default Link sprite (from the JP ROM: gfx `0x80000`, palette `0xDD308`, gloves `0xDEDF5`, author `0x118002`/`0x118020`), `updateChecksum`

Applying them to a real ROM is Phase 2 step 2's pipeline, not this step.

## Facts at `8a9cf17` (checked)

### The heart-beep group

- `tests/rando/write_groups.txt` ends with `group setTotalItemCount setTotalItemCount`, then `group heart_beep setHeartBeepSpeed`, the last group. A claim of the last group claims the whole log, and any extra C write is `DIVERGENT` (step 1.5 note).
- Every one of the 73 goldens (50 `cases/`, 23 `combos/`) ends with `180196 D800 setTotalItemCount`, then one `180033 <byte> setHeartBeepSpeed` line, the last line of the log.
- The byte, across the 73 goldens: `40` in 69 (`half`, the case reader's default), `00` in `heart_beep-off`, `80` in `heart_beep-quarter`, `10` in `heart_beep-double`, `20` in `heart_beep-normal`.
- The oracle (`gen_case.php`) calls `$rom->setHeartBeepSpeed($config['heart_beep'])` after `writeToRom`.
- **VT** (`tmp/vt`, `app/Rom.php` `setHeartBeepSpeed(string)`): `off` → `0x00`, `half` → `0x40`, `quarter` → `0x80`, `double` → `0x10`, `normal` and anything else → `0x20`; one `write(0x180033, pack('C', $byte))`. `resources/js/rom.js` `setHeartSpeed` writes the same byte at the same address.
- **The C side:** `RandoHeartBeep` (`rando_rom.h`) lists `OFF, HALF, QUARTER, DOUBLE, NORMAL`, in the case reader's order. `rando_rom_write` takes it and passes it to the empty static `write_heart_beep` (`rom.c`, `TODO step 11`), the last slot. `rando_gen` already passes the case's `heart_beep`. Groups write through `rando_rom_emit` (private `rom_emit.h`), which stops at the first refused write.

### `Rom::updateChecksum` (`app/Rom.php`) and `rom.js` `updateChecksum`

- PHP: `$sum = 0x1FE`; for every byte of the `Rom::SIZE` (2,097,152) bytes, skip offsets `0x7FDC`–`0x7FDF`, add the byte; `checksum = sum & 0xFFFF`, `inverse = checksum ^ 0xFFFF`; `write(0x7FDC, pack('S*', $inverse, $checksum))`, little-endian on every host VT runs on.
- JS (the code that makes the downloaded file): the same sum over the whole `u_array` (2 MiB after `resize(2)`), the same skip, `(sum + 0x1FE) & 0xFFFF`, then `[inverse & 0xFF, inverse >> 8, checksum & 0xFF, checksum >> 8]` at `0x7FDC`. Its `reduce` has no initial value, so index 0 is the start value; index 0 is outside the skip, so the result is the same.
- So the four header bytes never feed the sum, and `0x1FE` stands for their value once fixed (`lo(inverse) + lo(checksum) = 0xFF`, same for the high bytes). A fixed ROM therefore sums, all bytes included, to `checksum` modulo `0x10000`, and running it twice gives the same bytes.
- **Expected bytes at `0x7FDC`–`0x7FDF`**, computed for this PRD by a separate Python script of the rule above, never from C:

| Synthetic 2 MiB buffer | Bytes at `0x7FDC` |
|---|---|
| all `00` | `01 FE FE 01` |
| all `FF` | `FD 01 02 FE` |
| byte `i` = `i & 0xFF`, then `0x7FDC`–`0x7FDF` set to `12 34 56 78` | `77 01 88 FE` |
| byte `i` = `((i * 7) ^ (i >> 11)) & 0xFF`, then `0x7FDC`–`0x7FDF` set to `AA BB CC DD` | `43 FE BC 01` |

  The third one also has a closed form a test can recompute: `8192 × 32640 − (0xDC + 0xDD + 0xDE + 0xDF) + 0x1FE = 267,386,504`, low 16 bits `0xFE88`, inverse `0x0177`.

### The browser's writes at VT's defaults (`resources/js/rom.js`)

The spoiler's `meta.build` is `Rom::BUILD` = `2024-02-18`, which picks every build-gated branch below.

- **`setQuickswap(false)`** (`save()`; `!tournament`): `0x18004B = 0x00`.
- **`setMusicVolume(true)`** (`musicOn`, build `> "2019-08-01"`): `0x18021A = !enable ? 0x01 : 0x00` → `0x00`.
- **`setReduceFlashing(false)`** (build `>= "2021-05-04"`): `0x18017F = 0x00`.
- **`setMenuSpeed("normal")`**: `0x180048 = 0x08`, `0x6DD9A = 0x11`, `0x6DF2A = 0x12`, `0x6E0E9 = 0x12` (`fast` false).
- **`setHeartColor("red")`** reads `version = arrayBuffer[0x7FE2]` (the base-patched ROM):
  - `version <= 0x03 || version == 0xFF` (old builds, vanilla): `0x24` at the ten addresses `0x6FA1E`, `0x6FA20`, … `0x6FA30` (step 2), and `0x65561 = 0x05`;
  - otherwise: `0x187020 = 0x00` (`HUDHeartColors_index`, red).
  - z3randomizer `dcb0a2b4` (`LTTP_RND_GeneralBugfixes.asm`) puts `dw !ROM_VERSION_LOW` (1) at `$80FFE0` and `dw !ROM_VERSION_HIGH` (4) at `$80FFE2`, PC `0x7FE2`, and VT's `UpdateBuildRecord` passes no `-D`. So the base-patched byte should be `0x04`, the second branch. Unverifiable here: `data/base.bps` is not committed (step 0.1). The C branches on the buffer's byte, as VT does.
- **The default Link sprite.** At ROM load, when local storage has no `vt_sprites.001.link.1.zspr`, `rom.js` builds it from `originalData` (the loaded JP ROM, header stripped, before `resize`): a 66-byte (`0x42`) ZSPR header, then `0x7000` bytes from `0x80000`, `120` from `0xDD308`, `4` from `0xDEDF5`. The sprite selector loads it through `parseSprGfx` → `parseZsprGfx`:
  - header: gfx offset `0x42`, palette offset `0x7042`; the two UTF-16 strings (`Link`, `Nintendo`) are skipped and the ASCII author `Nintendo` is read;
  - author: `center("Nintendo".substring(0, 28), 28).toUpperCase()`. With `center-align` 1.0.1 (VT's `package-lock.json`), a one-line string and a numeric width give `floor((28 − 8) / 2) = 10` spaces on **both** sides: 28 characters, `"          NINTENDO          "`;
  - author bytes per character (`[hi, lo]`): space `9F 9F`, `N` `6A 90`, `I` `65 8B`, `T` `70 96`, `E` `61 87`, `D` `60 86`, `O` `6B 91`;
  - the author is written only when the current ROM has `0x118000 = 0x02`, `0x118001 = 0x37`, `0x11801E = 0x02`, `0x11801F = 0x37`: then 28 hi bytes at `0x118002` and 28 lo bytes at `0x118020`. z3randomizer's `stats/credits.asm` at `$A38000` (PC `0x118000`) starts with `db 2`, `db 55` before each of the two lines, so the base-patched ROM should pass the guard;
  - then gfx: `0x7000` bytes at `0x80000`; palette: `120` bytes at `0xDD308`; gloves: `4` bytes at `0xDEDF5`, each the JP ROM's own bytes at the same offsets. So the sprite copies three JP ROM ranges back over the base-patched ROM.
- **Not here:** `setMSU1Resume(true)` and palette and SFX shuffle write nothing at VT's defaults (step 1.2 note).
- **No golden write** of the 73 overlaps any of these addresses or ranges, nor `0x7FC0`–`0x7FFF`: they are disjoint from the write log, so their order against it does not matter, and only the checksum must come last.

### The C side

- `RandoRomBuffer` (caller-owned bytes and size) and `rando_rom_buffer_write` (bounds-checked copy) are public in `rando_rom.h`. The library keeps no mutable static state and does not allocate.
- `tests/rando/Makefile`: `UNIT_TESTS` lists the unit binaries; `LOGIC_TESTS` is `UNIT_TESTS` filtered by a list of binaries with no `--fixtures` mode (`test_rom` is one).
- `test_rom` holds the baseline write-order expectation and the buffer and log writer tests. Its recorder already treats an unknown tag as a settings write.

## Step 10's outcome (hypotheses)

Step 10 (`docs/prd/triage/phase1-step10-initial-sram-total-item-count.md`) is not done at `8a9cf17`: every claim reads `<case> all text`, and `write_initial_sram` and `write_total_item_count` are still empty statics. Every decision below that depends on what step 10 leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it held. A hypothesis that doesn't hold is handled by the rule given with it.

- **H-10-1**: step 10 is finished: every line of `claims.txt` (50) and `combo_claims.txt` (23) is `<case> all setTotalItemCount`, and `make check` is green.
  - *If a case is still below `all setTotalItemCount`*, this step can't claim `all heart_beep` for it. It stops and reports that case instead of claiming around it, since the step text requires every case.
  - Chunks 2 and 3 need no claim and may go ahead.
- **H-10-2**: the heart-beep slot is still the empty static `write_heart_beep` (`TODO step 11`) in `rom.c`, called last by `rando_rom_write`, after the `setTotalItemCount` group. Step 10 put the SRAM and count groups in their own private source.
  - *If step 10 moved or renamed it*, the heart beep takes whatever slot comes after `setTotalItemCount`, still last.
- **H-10-3**: `test_rom`'s baseline expectation reads "items, settings, two `writeCredits`, one `writeText`, one `writeInitialSram`, one `setTotalItemCount`", and step 10's PRD says step 11 rewrites it. This step rewrites it on purpose.
- **H-10-4**: `test_rom_sram`'s shape case allows "nothing after `setTotalItemCount` but a heart-beep tag" (step 10 PRD), so a real heart-beep write keeps it green.
  - *If it instead requires `setTotalItemCount` to be the last write*, this step changes that one expectation on purpose, to "followed only by one `setHeartBeepSpeed` write", and says so in the commit.
- **H-10-5**: step 10 added no heart-beep, checksum or browser-default code, and left `RandoHeartBeep`, `rando_rom_write`'s signature, `RandoRomBuffer` and `rando_rom_buffer_write` unchanged.
  - *If it added any of these*, this step uses what is there as the single source and does not keep a second one.
- **H-10-6**: `write_groups.txt` (with `heart_beep` the last group), `run_e2e.sh`'s claim semantics, the goldens, the fixtures, `gen_golden.sh` and the oracle are unchanged by step 10.
- **H-10-7**: the `Makefile`'s `UNIT_TESTS` / `LOGIC_TESTS` structure is unchanged; step 10 added `test_rom_sram` to `UNIT_TESTS` and kept it in `LOGIC_TESTS` with a `--fixtures` mode.

## Problem Statement

The owner will play ROMs the console patches. Three things stand between today's C writer and a ROM alttpr.com would hand him:

- The low-health beep is never written, so the beep he picks (or the default) is whatever the base patch holds.
- Nothing fixes the ROM's checksum after the seed writes. Emulators and the SNES core warn on, or refuse, a bad header checksum, and the file would differ from VT's download anyway.
- alttpr.com's browser makes a handful of writes at download that are not in VT's write log: quickswap, music, flashing, menu speed, heart colour, and the default Link sprite taken from the player's own JP ROM. Without them the patched ROM keeps the base patch's bytes there; the sprite in particular would be the base patch's, not Link's.

The net can't catch any of this. No claim reaches the heart-beep line, and nothing in the repository tests a checksum or these writes.

## Solution

- **The heart-beep group.** `rando_rom_write` ends with one write of VT's byte for the chosen beep at `180033`, tagged `setHeartBeepSpeed`.
- **The checksum.** A new public function recomputes VT's SNES checksum over a caller's 2 MiB ROM buffer and writes the inverse and checksum at `7FDC`.
- **The browser's defaults.** A new public function writes, into a caller's 2 MiB base-patched ROM buffer, the browser's writes at VT's defaults, in step 1.2's order: quickswap off, music on, flashing not reduced, menu speed normal, heart colour red (by the buffer's version byte), and the default Link sprite copied from the caller's 1 MiB JP ROM, author guard included.
- **Proof:**
  - `test_rom` gains one unit case per heart-beep value; its baseline expectation gains the heart beep as the last write;
  - `test_rom_checksum`: the algorithm on synthetic 2 MiB buffers whose expected bytes were computed independently;
  - `test_rom_cosmetics`: every write of the browser's defaults, both heart-colour branches, both author-guard outcomes, on synthetic buffers;
  - every claim of `claims.txt` and `combo_claims.txt` becomes `<case> all heart_beep`, so `e2e` and `e2e-combos` compare all 73 write logs whole.

## User Stories

1. As the owner, I want the low-health beep of my seed to be the one I chose (off, half, quarter, double, normal), so that the game beeps as I asked.
2. As the owner who left the default, I want the beep at half, alttpr.com's default, so that the ROM matches what the site gives.
3. As the owner, I want a valid header checksum, so that the emulator loads the ROM without a bad-checksum warning.
4. As the owner, I want quickswap off, so that the item menu behaves as alttpr.com's default.
5. As the owner, I want the music on, so that the game has its soundtrack (the reason a previous project was rejected).
6. As the owner, I want flashing not reduced, as alttpr.com's default, so that the game looks as on the site.
7. As the owner, I want the normal menu speed, so that the menu slides in as alttpr.com's default.
8. As the owner, I want red hearts, so that the HUD looks as alttpr.com's default.
9. As the owner, I want Link's own sprite, palette and gloves from my JP ROM, so that Link looks like Link and not like the base patch's sprite.
10. As the owner, I want the credits to name the sprite's author, Nintendo, as alttpr.com does, so that the end credits match.
11. As the app (Phase 2), I want the heart beep inside `rando_rom_write`, so that the write log and the ROM get it the same way.
12. As the app, I want the browser's defaults as one call on my buffer, in step 1.2's order, so that the pipeline cannot apply them in a wrong order.
13. As the app, I want the checksum as its own call, so that I run it last, after every other write.
14. As the app, I want both calls to refuse a buffer of the wrong size, writing nothing, so that a wrong input never half-patches a ROM.
15. As the app, I want the sprite read from the unpatched JP ROM I pass, never from the patched buffer, so that the sprite is VT's default even though the base patch changed those bytes.
16. As a porting agent, I want the heart colour to branch on the buffer's own `0x7FE2` byte, as VT does, so that a different base build would still get VT's write.
17. As a porting agent, I want the sprite author written only when the buffer passes VT's four-byte guard, so that an older base build is never overwritten as VT avoids.
18. As a porting agent, I want the author bytes fixed by `center-align` 1.0.1's padding on both sides, so that the 28 credit cells are VT's and the trap is recorded.
19. As a porting agent, I want the checksum to skip `7FDC`–`7FDF` and start at `0x1FE`, so that it matches VT and is idempotent.
20. As a porting agent, I want an out-of-range `RandoHeartBeep` to fail an assertion, so that a bad caller is loud.
21. As the net, I want a unit case per heart-beep value, address and tag, so that the byte table is pinned beside the goldens.
22. As the net, I want the heart beep to be the very last write and to obey the refusal rule, so that the writer contract holds.
23. As the net, I want the checksum tested on synthetic 2 MiB buffers with expected bytes computed outside the C, so that the algorithm is proven without a ROM.
24. As the net, I want the checksum's sum property and idempotence checked, so that a wrong byte order or skip range is caught.
25. As the net, I want every browser-default byte, and the bytes just around each range, checked on synthetic buffers, so that no write lands one byte off.
26. As the net, I want both heart-colour branches (versions `00`, `03`, `FF` and `04`, `05`, `FE`) checked, so that the comparison is pinned at its edges.
27. As the net, I want the author guard checked when it passes and when each of its four bytes differs, so that the guard is exact.
28. As the net, I want every case and combination to claim `all heart_beep`, so that `make check` compares every write log whole.
29. As the net, I want `test_rom`'s baseline expectation rewritten on purpose, with the reason, so that no check is weakened silently.
30. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new sources obey the library rules.
31. As the net, I want `make test` to stay fast: a few 2 MiB passes, no generation beyond `test_rom`'s one, so that the dev loop stays seconds long.
32. As a reviewer, I want the step note to record which H-10 hypotheses held, the claims moved, the expected heart-colour branch and author guard, and any VT quirk settled, so that I can check the step without rerunning it.
33. As a legal repository, I want every test on synthetic buffers, never a Nintendo ROM, so that the repository and CI stay clean.

## Implementation Decisions

### Modules

- **The heart-beep group** stays in `rom.c`, replacing the empty `write_heart_beep` (H-10-2): a five-way table from `RandoHeartBeep` to `00 40 80 10 20`, one `rando_rom_emit` of one byte at `0x180033`, tagged `setHeartBeepSpeed`. A value outside the enum fails an assertion: the enum is C's, and VT's `default` branch only catches strings the C cannot produce. No new source: it is one byte.
- **The checksum** gets a new private source in `rando/src/` and one public function in `rando_rom.h`:
  - `bool rando_rom_update_checksum(RandoRomBuffer *rom)`;
  - contract: `rom->size` must be `0x200000` (VT's `Rom::SIZE`); otherwise it returns false and writes nothing. True after writing the four bytes at `0x7FDC`.
  - The sum is a `uint32_t` (2 MiB × 255 fits) masked at the end. A trap comment states the skip and the `0x1FE`.
- **The browser's defaults** get another private source in `rando/src/` and one public function in `rando_rom.h`:
  - `bool rando_rom_write_cosmetic_defaults(RandoRomBuffer *rom, const uint8_t *source, size_t source_size)`;
  - contract: `rom` is the 2 MiB base-patched ROM (`rom->size == 0x200000`); `source` is the JP ROM as loaded, header stripped and not resized (`source_size == 0x100000`), only read, not overlapping `rom`. On any other size it returns false and writes nothing. True after all writes.
  - Writes, in step 1.2's order: quickswap, music volume, reduce flashing, menu speed (4 bytes), heart colour, then the sprite (author if guarded, gfx, palette, gloves).
  - The heart colour reads `rom->bytes[0x7FE2]` at call time and branches on `<= 0x03 || == 0xFF`. A trap comment states VT reads the base-patched byte, and that the old branch also writes `0x65561`.
  - The author is two 28-byte constant tables (hi, lo), written only when the four guard bytes match. A trap comment states `center-align` 1.0.1 pads both sides, so the table is 10 blanks, `NINTENDO`, 10 blanks. No generic ZSPR parser or author encoder: the only sprite is the default one, and its header is a constant in VT.
  - gfx, palette and gloves are `memcpy`s from `source` at the same offsets. Writes go straight into the buffer (the size check already bounds them), not through `RandoRomWriter`: these functions must read the ROM, which a writer callback cannot.
- **Why the two buffer functions are not in `rando_rom_write`:** they are outside VT's write log (the log goldens would turn every case `DIVERGENT` on an extra write) and need the ROM's bytes, which `rando_rom_write`'s writer callback never sees. Phase 2 step 2 calls `rando_rom_write` with the buffer writer, then `rando_rom_write_cosmetic_defaults`, then `rando_rom_update_checksum`.
- **Why one call for the six browser writes, not six:** Phase 2 must apply them in step 1.2's order; one call makes the order the library's, and its unit tests still check each write.
- **Name:** "cosmetic defaults", after the plan's rule "every other cosmetic stays at VT's default". The menu speed, quickswap, music and flashing are cosmetics on alttpr.com's ROM settings panel.
- `rando_rom.h`'s contract for `rando_rom_write` already lists the heart beep; it is not changed. The two new declarations state their size contracts and what is returned on a wrong size.

### Claims

- Every line of `claims.txt` (50) and `combo_claims.txt` (23) becomes `<case> all heart_beep` (H-10-1). That claims the whole write log, extra C writes included.
- Nothing else moves in the harness, `write_groups.txt`, the goldens or the oracle (H-10-6). No fixture: the goldens hold every heart-beep value, and the browser writes and the checksum have no golden by design (plan §2).
- A case `DIVERGENT` at `all heart_beep` is a bug in this step; it is driven to green before the step ends, behind its own failing unit test first.

### Not done here

- No ROM is assembled, no BPS applied, no CLI added (Phase 2).
- No `measure` run: step 11 sets no budget.
- `e2e-arm` is not run (step 12 and Phase 4). If it runs anyway, the result goes into the note.
- No change to `check-e2e` or `rando_gen`: the case's heart beep already reaches `rando_rom_write`.

### Step note

The plan's §6 gains a "Step 1.11" note with:

- which H-10 hypotheses held, and how any that didn't was handled;
- the claims moved (`e2e` 50, `e2e-combos` 23 at `all heart_beep`);
- the two public functions and the order Phase 2 step 2 calls them in;
- the expected branches on the real base build, still unverified until `data/base.bps` exists: heart colour `0x7FE2 = 0x04` → `0x187020`; author guard `02 37 … 02 37` passes;
- VT quirks settled: `setMusicVolume`'s inverted flag (music on writes `00`); `center-align` 1.0.1's padding on both sides; the sprite copying JP bytes back over the base patch.

### No `guideline` agent

None was consulted. Every point was settled from:

- the plan (§2 rules, step notes 0.1–1.9);
- VT at `219fcafd` (`tmp/vt`: `app/Rom.php`, `resources/js/rom.js`, `resources/js/components/VTSpriteSelect.vue`, `VTRomSettings.vue`, `views/Randomizer.vue`, `package-lock.json`);
- z3randomizer `dcb0a2b4` (Composer's cache: `LTTP_RND_GeneralBugfixes.asm`, `tables.asm`, `stats/credits.asm`);
- `center-align` 1.0.1 and `align-text` 1.0.2 from the npm registry;
- the code and the goldens at `8a9cf17`, and step 10's PRD.

## Implementation Chunks

1. **The heart-beep group and the whole-log claims.**
   - `write_heart_beep` filled in (H-10-2).
   - `test_rom`: one case per beep value; the heart beep last and after `setTotalItemCount`; refusal of the heart-beep write returns false; baseline expectation rewritten (H-10-3); `test_rom_sram`'s shape case adjusted only if H-10-4 fails.
   - Every claim of both files moves to `all heart_beep` (H-10-1); `e2e` and `e2e-combos` green.
   - `make check` green.
2. **The checksum.**
   - The private source and `rando_rom_update_checksum`.
   - `test_rom_checksum` in `UNIT_TESTS` and in `LOGIC_TESTS`' filter-out list (no fixture mode).
   - `make check` green.
3. **The browser's defaults and the note.**
   - The private source and `rando_rom_write_cosmetic_defaults`.
   - `test_rom_cosmetics` in `UNIT_TESTS` and in the filter-out list.
   - The §6 "Step 1.11" note.
   - `make check` green.

## Testing Decisions

- **What a good test is here.**
  - It goes through the public interface only: `rando_rom_write` with a recording writer, `rando_rom_update_checksum` and `rando_rom_write_cosmetic_defaults` on caller buffers. It never calls a static function.
  - Expected bytes come from VT's source at `219fcafd` (and z3randomizer's, `center-align`'s), written by hand in the test or computed outside the C (the checksum table above), never from the C code.
  - Synthetic buffers only: static 2 MiB and 1 MiB arrays filled with a position pattern. No Nintendo ROM, no file read.
  - Prior art: `test_rom` (recorder, refusal, buffer writer, `check_buffer`'s "every other byte unchanged"), `test_rom_items` and `test_rom_settings` (one case per rule).
- **`test_rom`** (existing):
  - per value: `OFF` → `00`, `HALF` → `40`, `QUARTER` → `80`, `DOUBLE` → `10`, `NORMAL` → `20`; each one write, at `180033`, 1 byte, tagged `setHeartBeepSpeed`, the last call, right after the `setTotalItemCount` call;
  - a writer refusing the heart-beep write makes `rando_rom_write` return false;
  - the baseline expectation, rewritten on purpose because the heart beep now writes (H-10-3): items, settings, two `writeCredits`, one `writeText`, one `writeInitialSram`, one `setTotalItemCount`, one `setHeartBeepSpeed`, in that order, all inside 2 MiB, accepted by the buffer writer, generator unchanged.
- **`test_rom_checksum`** (new):
  - the four buffers of the Facts table give exactly their bytes at `7FDC`–`7FDF`; the third also against the closed form recomputed in the test;
  - every byte outside `7FDC`–`7FDF` is unchanged;
  - after the write, the sum of all 2 MiB bytes, header included, modulo `0x10000` equals the written checksum, and `lo(inverse) + lo(checksum) == 0xFF`, same for the high bytes;
  - running it twice gives the same bytes;
  - changing one byte outside the skip by `+1` changes the checksum by `+1`; changing a byte inside the skip changes nothing;
  - a buffer of 2 MiB − 1 and of 2 MiB + 1 returns false and leaves the buffer as it was.
- **`test_rom_cosmetics`** (new). The ROM buffer is filled with one pattern and the source with another, so a copied byte and a kept byte differ:
  - `18004B=00`, `18021A=00`, `18017F=00`, `180048=08`, `6DD9A=11`, `6DF2A=12`, `6E0E9=12`;
  - heart colour, new branch: with `7FE2` set to `04`, `05` and `FE`, `187020=00` and the eleven old-branch bytes unchanged;
  - heart colour, old branch: with `7FE2` set to `00`, `03` and `FF`, `6FA1E`, `6FA20`, … `6FA30` = `24`, `65561=05`, the odd bytes between them and `187020` unchanged;
  - sprite: `80000`–`86FFF`, `DD308`–`DD37F`, `DEDF5`–`DEDF8` equal the source's bytes; `7FFFF`, `87000`, `DD307`, `DD380`, `DEDF4`, `DEDF9` unchanged;
  - author guard passing (`118000=02`, `118001=37`, `11801E=02`, `11801F=37`): `118002`–`11801D` = ten `9F`, `6A 65 6A 70 61 6A 60 6B`, ten `9F`; `118020`–`11803B` = ten `9F`, `90 8B 90 96 87 90 86 91`, ten `9F`; the guard bytes themselves unchanged;
  - author guard failing, once per guard byte changed: `118002`–`11801D` and `118020`–`11803B` unchanged, the gfx, palette and gloves still copied;
  - every byte not named above is unchanged (one full-buffer comparison against a copy);
  - wrong sizes (ROM 1 MiB, ROM 2 MiB + 1, source 1 MiB − 1, source 2 MiB) return false and leave the ROM as it was;
  - `source` unchanged after the call.
- **`e2e` and `e2e-combos`**: the step's proof is all 73 claims at `all heart_beep`, green.
- **`lint`** builds the two new sources with `-std=c99 -pedantic-errors`.
- **Speed:** `make test` gains two binaries, each a few dozen passes over 2 MiB static buffers (milliseconds), and no generation.

## Out of Scope

- The ROM assembly pipeline: source MD5, resize, BPS, the order of the calls, writing the `.sfc` (Phase 2 steps 1–2).
- Checking the expected branches on the real base build (`0x7FE2`, the author guard): it needs `data/base.bps` (Phase 0, Phase 2).
- Any cosmetic other than VT's defaults: other beep-free settings, custom sprites (`.zspr`), palette or SFX shuffle, MSU-1 resume, heart colours other than red, other menu speeds (plan §2).
- A generic ZSPR parser or `center-align` port.
- `setSeedString`, `setStartScreenHash`, tournament writes (step 1.2 note: left out).
- Oracle changes: goldens, fixtures, `write_groups.txt`, claim semantics.
- Running `check-e2e` over the whole case list, and `e2e-arm` (step 12).

## Further Notes

- **Why the expected checksums are constants in the PRD**: the step asks for a sum "computed independently". A Python script of VT's rule produced them; the test hard-codes them, and recomputes one in closed form, so a C bug can't hide in a shared helper.
- **Why the browser's writes are trusted without a golden**: no oracle runs VT's JavaScript, and plan §2 says the ROM-dependent part is tested by its rule. The unit tests pin every byte against `rom.js`; the sprite's content is the user's own JP ROM, so a synthetic source proves the copy.
- **Where a divergence is likely:**
  - the heart-beep table order against `RandoHeartBeep` (`half` is index 1, the default);
  - the checksum's byte order (inverse first, little-endian);
  - the author's centring (a left-only padding would leave 10 cells unwritten and, in VT, throw).
- **For Phase 2 step 2:** call `rando_rom_write` with `rando_rom_buffer_write` on the 2 MiB base-patched buffer, then `rando_rom_write_cosmetic_defaults` with the 1 MiB JP ROM, then `rando_rom_update_checksum`. Nothing else may write after the checksum.
