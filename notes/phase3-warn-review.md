# Phase 3 warn-review: warnings left open

Raised by the end-of-Phase-3 `warn-review` (`7734cf0..6c459cf`); no ground-rule violation was found. Plan review folded the app/sd→app/core layering into a CLAUDE.md wording fix, the unscanned `app/main` into `no_sdl.sh`, and the SD root into the SDL main (all Phase 4 step 1). Left open, guarded by the snapshots, goldens and SD-tree tests:

- `options.c` built on `rando_settings.h` (which includes `rando_options.h`); a dummy heart beep passed to the settings API (`options.c:121`, `:136`).
- `ui_strings.h` and `ui_history.h` include `ui_grid.h`.
- The tab header built in three places (`ui_generate.c`, `ui_genflow.c`, `ui_history.c`) beside `UiAppTab`; `add_text`/`move_to`/`clamp`/`wrap` duplicated across core modules; the buffer size 96 repeated.
- Three sources of defaults (`ui_generate_init_defaults` hard-codes HALF; `last_settings.c`; `rando_options_init` beside the table's default column) — a risk if a default ever changes.
- `UiHistoryDate` copies `RandoRecordDate` (forced by layering). **Ground-rule deviation:** the History date format "YYYY-MM-DD HH:MM" is fixed in code, not in the translation table; only English ships and `ui_str_fill` takes one `{}`, so a format id needs a multi-argument fill first.
- app/sd paths: duplicated `join`, paths built by hand, `"Roms"`/`"App"` and extensions inline beside `RANDO_SD_*`; `RANDO_SD_DEFAULT_ROOT` apart; every SD constant in `rando_seed_save.h`; `last_settings.c`'s own `PATH_MAX_LEN` 1024; `history.c`'s unexplained `+ 512`.
- `record.c` (674 lines) holds the History spoiler outliner (on-screen layout in the record module).
- `rando_out.h` reads inputs and draws seeds; `rando_parse_uint32` sits in the settings header.
- `app/sd` header names (`rando_app_gen.h`, …) differ from their sources (`app_gen.c`, …).
- `tests/rando/Makefile`: every new unit test is listed twice (`UNIT_TESTS` and the `LOGIC_TESTS` filter-out).
- `no_literals.py` does not scan `app/main`: the mains' usage and error lines are English stderr literals by design.
- **For Phase 5 `rework-comments`:** `rando_seed_save.h:48` — `rando_seed_save`'s contract comment now sits above `rando_sd_remove_list_caches`.
