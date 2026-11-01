# Phase 2 warn-review: warnings left open

Raised by the end-of-Phase-2 `warn-review` (`fc77e82..f7e12bb`). Plan review folded the settings list, the uint32 parser's home, `RandoHeartBeep`'s header, the BPS name table and the device-bound host code into Phase 3 steps 2, 4 and 5, and fixed `make test`'s `rando_patch` dependency; the rest stays open, guarded by the goldens and the round-trip tests.

- `record.c`'s JSON parser beside `case_reader.c`'s (a third is prevented: step 3.4 saves settings through the record).
- `record.c` decodes the name code again and inlines its length 6 (also `name.c`); the date ranges are written twice in `record.c` — unless step 3.4's settings-table chunk touches them.
- More little-endian readers (`bps.c`, `md5.c`, `patch_cli.c`); `rando_assemble.h` includes the `rando.h` hub.
- Tests redefine `0x100000`/`0x200000` beside `RANDO_ROM_SOURCE_SIZE`/`RANDO_ROM_SIZE` (`test_patch_cli.c`, `test_assemble.c`, `synthetic_base.h`, `test_patch_header.c`); four BPS writers in tests; repeated read/write-file helpers; the 4 MiB patch cap in two places.
- `rando_seed_save.h`'s SD path constants are unused until step 3.5, and checked by step 3.1 first (on purpose).
- `patch_cli.c` vs `rando_patch.h`/`main/rando_patch.c` naming (may ride step 3.2's move).
- **For Phase 5 `rework-comments`:** `test_name.c:94` cites "The PRD's worked example"; `test_record.c:188` cites "The plan's check".
