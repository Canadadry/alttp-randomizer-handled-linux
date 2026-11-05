# Phase 4 warn-review: warnings left open

Raised by the end-of-Phase-4 `warn-review` (steps 4.1–4.3: `app/sdl/`, `app/main/sdl_main.c`, `app/package/`, the SDL and package tests, `measure*`, the cross targets, CI); no ground-rule violation was found. Plan review judged none of them a risk to step 5.2 (README only) or to §4, and left all open:

- `sdl_main.c` literals: key repeat 300/50 ms (`:131`), the `--frames` cap 1000000 (`:88`), the grid's 2-column margin (`:123`, also `test_sdl_loop.c` and `UI_STR_LEN_MAX`), 640×480 apart from the grid defaults (`:17`, also `test_sdl_view.c`, `smoke_sdl.sh`).
- The font file name (`DEFAULT_FONT`, `build_zip.py`, `test_package.py`, Makefiles) and the `App/ALttPR/` layout and `base.bps` name (`build_zip.py`, `config.json`'s icon path, `test_package.py`, `smoke_sdl.sh`) restated beside `RANDO_SD_*`; checked consistent at the end of Phase 4.
- `sdl_main.c:119`: a failed `getcwd` is reported as "SD root does not fit" (shell init itself fails only on root length).
- `tests/rando/Makefile`: each cross target (`unit-arm`, `e2e-arm`, `sdl-arm`) rebuilds every source through its own inline loop.
- `measure.sh`: still reports the Nintendo DS projection and a devkitARM build (`:181`–`:227`, `:486`); budgets scattered (`:19`, `:284`–`:290`, one in awk); golden-derived expectations inline (2505 draws, per-stage table `:323`–`:325`, 716-byte `RandoItems` `:257`); the largest file in scope, with only runtime self-checks and no test in a net.
- The reference commit `40314c3` repeated (`tests/rando/Makefile:51`, `measure_compare.sh:13`, root `Makefile` `:21` and `:83`, `CLAUDE.md`). It and `52186a8` are not in this branch's history: reachable only through `refs/pull/2/head` or `origin/plan-1-onion-generator`; `make measure-compare` needs `git fetch origin refs/pull/2/head` first, and that branch should not be deleted while it is the only named ref.
- Root `Makefile` exposes `measure-compare` but not `measure`, and its help omits `measure`.
- **Deliberate deviation:** `tests/rando/Makefile:152` keeps a comment that carries a trap (the image's `sdl-config` and `pkg-config` are x86), against `CLAUDE.md`'s Makefile comment rule; kept by step 5.1.
