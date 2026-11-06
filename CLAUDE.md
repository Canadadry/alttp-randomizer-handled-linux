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
- `app/` is the Onion app's own code. `app/core/` is the UI core: pure C99
  like the library, built with only `alttp/include` and itself on the include
  path, no SDL; the app opens on its game-choice screen (`ui_games.c`), A
  opens a game (ALttP's Generate and History tabs, or LADX's placeholder),
  B goes back to it, and B there quits, so every key sequence of a test
  that means ALttP starts with `a`. `app/sd/` is the SD-card layer
  (`record.c`, `seed_save.c`); its paths live in `rando_seed_save.h`: the
  last game opened in `App/ZeldaRando/settings.json` (`last_game.c`), and
  per game ALttP's `base.sfc`, `base.bps`, `history/` and `settings.json` in
  `App/ZeldaRando/alttp/`, its ROMs in `Roms/SFC/ZeldaRando/`.
  `app/sdl/` is the SDL 1.2 renderer and input layer (key map, grid view,
  theme renderer `sdl_render.c`, frame loop). `app/core` uses the library;
  `app/sd` the library, `alttp/host` and `app/core` (it hands the core its
  runners and stores); `app/sdl` the library, `app/core` and `app/theme`;
  nothing in `alttp/` includes `app/`. Its tests live
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
- `tests/alttp/` holds its tests: `unit/test_<module>.c`, one binary per
  module, registered in `tests/alttp/Makefile`'s `UNIT_TESTS`, failing through
  `unit/support.h`'s `test_fail`.
- `tests/alttp/cases/` holds the e2e cases, one directory each with its VT
  golden; `tests/alttp/oracle/` the PHP VT oracle (`gen_golden.sh`, `vt.patch`)
  and the fixtures it writes.
- `tests/ladx/oracle/` holds the LADX oracle: LADXR pinned at `4325207e`,
  cloned (never vendored) into `tests/ladx/oracle/ladxr/` (git-ignored) and
  patched with `ladxr.patch` (order fixes only); the Python scripts that run
  it without a ROM (shared helpers `ladx_oracle.py`, unit-tested by
  `test_oracle.py`); Phase 2's ROM-free tools, each unit-tested by its
  `test_<tool>.py` (`nintendo_bytes.py`, the Nintendo-bytes check;
  `push_gate.py`, Phase 2's push gate; `read_trace.py`, the ROM read
  trace; `inventory_check.py`, the grep-diff of `notes/ladx-inventory.md`,
  the static draft of Phase 2's inventory of `generateRom`), and
  `fetch_rom.sh`, the ROM fetcher, proved by its own run; the
  hand-written inputs `tier1.txt`,
  `determinism_cases.txt` and `order_audit.txt`; and the six fixtures they
  write for the C port (`pyrandom_vectors.txt`, `settings.txt`,
  `settings_validate.txt`, `logic_graphs.txt`, `logic_vectors.txt`,
  `item_pools.txt`), each describing its format in its `#` header.

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
  own objects, with its smoke test `unit/smoke_ui_cli.c`) plus
  `test-template` (the tooling under `.claude/`) plus `rom-guard`
  (`tests/rom_guard.py`, git and python3 only): it fails, one
  `rom_guard: <path>: <reason>` line per hit, when git's index (the staged
  blob, not the working tree) holds a `.gb`, `.gbc`, `.sfc` or `.smc` path in
  any case, or a blob that is the LADX English 1.0 ROM (SHA-1) or the ALttP
  JP 1.0 ROM (MD5, with or without a copier header), whatever its name; it
  proves itself first on planted files in throwaway repositories; no git or
  no work tree gives a `setup:` line; else `rom_guard: <n> tracked files, no
  ROM`; plus `test-ladx-oracle` (`unittest discover` of
  `tests/ladx/oracle/test_*.py` under `python3 -I`: the LADX oracle's helpers
  and Phase 2's tools, no LADXR, no ROM, no network).
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
  `python3 tests/ladx/oracle/audit_order.py <ladxr dir>` checks every
  directory listing and set of LADXR's sources against `order_audit.txt`
  (one `UNAUDITED` or `STALE` line each, else `order audit: <n> sites, all
  audited`). `extract_logic.py --graph <config> --out <dir> <ladxr dir>`
  writes one configuration's full logic graph, to diff against a
  `logic_vectors.txt` digest. Run every script with `python3` without `-I`
  or `-E` (both drop `PYTHONHASHSEED`).
- The Nintendo-bytes check, outside the net (python3 and git):
  `python3 -I tests/ladx/oracle/nintendo_bytes.py [--rom <file>] [--ladxr
  <dir>] [--scope strict|general] [--min <n>] [--verbose] <file-or-dir>...`
  proves itself first on planted files against a planted synthetic ROM and
  corpus, one per scope, view and clearance (`nintendo-bytes: self-test, 34
  planted files as expected`, else `self-test failed: <case>`, exit 1). Then
  it prints one `LITERAL <path> <view> @<offset> <length> bytes <scope>`
  line per uncleared run of a file that also occurs in the ROM (`--rom`,
  default `tmp/ladx/base.gbc`), never the bytes, then `nintendo-bytes: <n>
  files, rom <sha1>, ladxr <short>, <k> literal runs, cleared <t> trivial,
  <l> low-info, <p> ladxr-public`; exit 1 when `k > 0`. Scope by
  repository-relative path: `strict` under `data/ladx/`,
  `tests/ladx/cases/`, `tests/ladx/combos/` (the ROM-derived artifacts;
  step 7 adds its fixture paths to `STRICT_PREFIXES`), `general` for any
  other path or a file outside the repository (`--scope` overrides). Views:
  `raw`; `hex` (maximal hex-digit runs, both nibble alignments); `bytelist`
  (lists of `0xNN`/`$NN` tokens separated by commas and whitespace).
  Minimum contiguous match: strict 4 bytes in every view; general raw 16,
  hex and bytelist 8. Cleared runs are counted, listed by `--verbose` as
  `CLEARED <path> <view> @<offset> <length> bytes <category>`: `trivial`
  (an arithmetic progression mod 256, constant runs included), `low-info`
  (at most 3 distinct values, general scope only), `ladxr-public` (the
  whole run occurs in a text file, valid UTF-8 without NUL, tracked by the
  LADXR checkout at `4325207e`, or in its decoded hex-digit runs; binary
  files left out). `--min <n>` is an experimental override of every
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
  uploads, intermediate commits included, holds a literal run of the ROM
  (`<path> (blob <sha>): literal run of <n> bytes at @<offset> (<view>)`),
  or when an untracked file sits outside the git-ignored paths (`docs/prd/`
  allowed, but searched for the link); it fails closed (no `LADX_ROM_URL`,
  or no `tmp/ladx/base.gbc` of SHA-1 `d90ac17e…`: `push-gate: setup: …,
  push refused`), never echoes the link or the bytes, else prints
  `push-gate: ok, …`. `--self-test` makes 12 planted pushes through the
  installed chain in a planted clone with a planted ROM and link
  (`push-gate: self-test, 12 planted pushes as expected`, else one
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
  `index`, `slice`, `iter`, `search`, `compare`, `convert`, `table`; writes
  never) as `<group>\t<site>\t<caller>\t<channel>\t<target>\t<detail>`,
  the site `<file> <function>\t<line>[ #<k>]` of the innermost section frame
  (`#<k>` on the k-th occurrence of that line text in the function).
  `python3 -I tests/ladx/oracle/read_trace.py --self-test [--ladxr <dir>]`
  runs one planted read per channel, a planted parse, `rom.patch` and writes
  through LADXR's real `ROM` and `PointerTable` on 64 synthetic banks
  (`read-trace: self-test, 7 channels traced, patch and writes excluded`,
  else one `self-test failed: <case>` line each, exit 1); `--blind-spots`
  lists the buffer-protocol lines a Python trace cannot see (`BLIND
  <file>\t<line>`, then `read-trace: <n> buffer-protocol sites to review`).
- The inventory check, outside the net (python3 and the LADXR checkout, else
  `setup: inventory: no LADXR checkout in <dir>, run
  tests/ladx/oracle/setup.sh`, exit 2; no ROM): `python3 -I
  tests/ladx/oracle/inventory_check.py [--ladxr <dir>] [--inventory <file>]
  [--print]` proves itself first on a planted tree and inventory
  (`inventory: self-test, 7 planted inventories as expected`, else one
  `self-test failed: <case>` line each, exit 1), then extracts every
  statement of `generator.py generateRom`, `randomizer.py
  Randomizer.__init__` after the fill, `romTables.py ROMWithTables.__init__`
  and `.save`, `rom.py ROM.save` and `ROM.fixHeader` (keyed by stripped
  line, ` #<k>` on a repeat, as `read_trace.py`'s sites) with the settings,
  `args`, RNG and placement each names, and diffs them against
  `notes/ladx-inventory.md`'s tables (default): one `MISSING`, `STALE`,
  `MISMATCH <section>: <call>: <column> inventory <a>, source <b>` or `BAD`
  (bad `Side (draft)`, empty `Group (draft)`, malformed table) line each,
  exit 1, else `inventory: <rows> rows in 6 sections match <commit>`.
  `--print` writes the extracted tables, draft columns kept, to refresh the
  draft by hand.
- `make ui-cli` builds `ui_cli` alone; look at the app with
  `tests/alttp/build/ui_cli --sd <dir> --every <keys>` (`ui_cli` with no
  argument prints its usage). `--seed`, `--date`, `--source-md5` and
  `--base-md5` fix a run; `--unsupported <setting>=<value>,...` is a
  test-only override of the library's unsupported pairs. The SDL app
  `zrando` takes the same keys and flags (one shared module,
  `app/sd/rando_fixed_run.c`), plus `--shot-text <file>`; with keys, its run
  ends after the last one.
- `make check` = `test` plus `lint` (`alttp/` and `app/` built with
  `-std=c99 -pedantic-errors`, except `app/sdl/` and `sdl_main.c`, which
  `make sdl` builds that way), `logic` (unit tests replaying the oracle fixtures),
  `selftest` (the e2e driver's own self-test), `e2e` (every case diffed
  against its golden, one `DIVERGENT <case>` line per failure) and
  `e2e-combos` (every combination case diffed against its golden, kept out
  of `e2e`). Run
  `make -k check` to see every failure at once.
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
  `tests/alttp`, `--fixtures` included for `LOGIC_TESTS`. Both need docker
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
  flipped pixel, a removed golden and an extra one, then plays the 14 states of
  `sdl/golden_states.txt` (each on a fresh SD from `sd_fixture`, drawn
  through `--theme themes/classic`) and prints
  one `DIFFERENT`, `MISSING` or `EXTRA` line per mismatch (each BMP
  capture's decoded pixels against its lossless PNG golden), else
  `goldens: 14 states pixel-identical`. The goldens in
  `tests/alttp/sdl/goldens/` (PNGs) are pinned to the Claude Code cloud container
  (Ubuntu 24.04.5, sdl12-compat 1.2.68 on SDL2 2.30.0, SDL_ttf 2.0.11,
  FreeType 2.13.2), fingerprinted in `goldens/environment.txt`; on another
  host it fails with `setup: goldens pinned to <fp>, this host is <fp>`, a
  setup problem that verifies nothing, not a bug. Then it draws the same 14
  states with `themes/alttp` (`goldens.sh --draw`, the theme in the
  Makefile's `DRAW_THEME`): no comparison and no state strings, each capture
  must be a 640×480 picture of more than one colour; it writes them to
  `tests/alttp/build/sdl/theme-states/alttp/` and prints
  `goldens: 14 states drawn with alttp in <dir>`.
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
  `gen_fixtures.sh`, `determinism.sh` and `audit_order.py` run only to
  regenerate or re-prove its fixtures; `nintendo_bytes.py` needs the ROM,
  which only Phase 2 holds, and the LADXR checkout, `fetch_rom.sh` fetches it, and `push_gate.py`
  guards Phase 2's pushes, `read_trace.py` traces LADXR's ROM reads and
  `inventory_check.py` diffs the inventory against LADXR (their tests run in
  `make test`). The SDL build (`make sdl`,
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
  every push.
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
