# Phase 1 warn-review: warnings left open

The end-of-Phase-1 `warn-review` (code written since `40314c3`) raised these; plan review folded the ROM-size constants and the heart-beep/hints names into Phase 2 step 2, and left the rest open: VT is frozen and all 73 whole-write-log goldens guard this code.

- **Headers:** `rando.h` is a hub reached by `rando_rom.h`, `rando_text.h`, `rom_emit.h`; `text_strings.h` and `rom_credits_data.h` take table sizes from `rando_postfill.h`, which mixes alternative counts, hint lookups and the stage API; `text_strings.h` holds hint names and tablet/pedestal texts; `rando/src` is flat.
- **Duplicated rules:** alias→target (`hints.c`, `rom_credits.c`, `rom_items.c`, `rom_sram.c`); KeyGK substitution (`hints.c`, `rom_items.c`, `text_seed.c`); not-prize/fountain/medallion/trade (`hints.c`, `rom_sram.c`); music regions (`postfill.c` vs `rom_items.c`); prize class test; empty-as-Nothing (four files); little-endian packing (four files); ASCII case helpers (three files).
- **Repeated constants:** 63 drops, `0x11`/`0x16` music, `0x65` alternate bow, inline addresses in `rom_cosmetics.c`, raw SRAM offsets in `rom_sram.c`.
- `rom_settings.c`'s pass-through `emit`; `run_e2e.sh` holds three large awk programs.
- **For Phase 5 `rework-comments`:** unchecked claims in comments (`text_seed.c` disjoint setters, `rom_settings.c` single-valued keys, `rando_config.h` `'off'`, `rando_rom.h` browser writes complete); rule violations (`rando_rom.h` cites "step 1.2's order", `test_rom_items.c` "Handed over by step 5").
- **Rule deviation, not fixable without rewriting pushed history:** commits `41966a0`, `0295613`, `52a365e` changed `gen_golden.sh` in the same commit as the new fixture they produce (`hint_tables`, `credit_tables`, `text_tables`); each added a fixture only, every existing golden byte-identical by `--out` runs.
