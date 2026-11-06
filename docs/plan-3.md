# Plan 3 — Zelda Randomizer: Link's Awakening DX next to ALttP

## 1. Goal

The Onion app generates seeds for two games on the console, with no PC and no network:

- *A Link to the Past*, as today (VT `219fcafd`, Japanese 1.0 ROM, `.sfc`).
- *Link's Awakening DX*, through a C99 port of **LADXR** (`daid/LADXR`, MIT), frozen at one commit. The app reads the English 1.0 GBC ROM and writes a patched `.gbc` that Onion's GBC core plays.

The app is renamed **Zelda Randomizer**. At each launch it opens a game-choice screen. Each game then has its own Generate and History tabs.

The goals behind every rule below:

1. **The goldens are the proof.** An agent cannot play the game. A LADX seed is right when the C port's output matches LADXR's for the same seed and settings, byte for byte, as with VT for ALttP.
2. **The same code runs everywhere.** The LADX library is pure C99 like the ALttP one: no `malloc`, no platform library, and the same code on the host, the Miyoo and qemu.
3. **No Nintendo ROM in the repository or in CI.** The LADX oracle needs the real ROM (see §2). It gets it only inside one interactive phase, from a temporary link, and nothing derived from it that contains Nintendo bytes is committed.
4. **Controller-first**, as today: every screen is driven by the d-pad and buttons, in the style of Onion's menus.

## 2. Rules

### Locked decisions

- **LADXR, not Z4Randomizer.** `CrystalSaver/Z4Randomizer` (the owner's first idea) was rejected:
  - it ships only a built React/Electron web app, with no sources and no README;
  - it has no licence, so porting it, even from the minified JS, is copying code without the right to;
  - it stopped in June 2020 at v0.68.

  LADXR is the reference LADX randomizer: MIT, maintained, used for races and by Archipelago. It is Python, about 27,000 lines.
- **LADXR is frozen** at `daid/LADXR` `4325207e4e55076e05f209fbdf16b860a90e42ef` (master, 2026-11-04), with no tracking of later updates. Tag `v14` (`943dae6`, 2025-10-24) was rejected: it lacks 85 commits, including the "Logic 2026-08" logic, logic fixes ("certain checks could never have dungeon items", "one way entrance requirements could clobber others") and the `notextbox` text mode. daid.github.io/LADXR, the site players use, runs master.
- **Base ROM: *Link's Awakening DX*, English v1.0**, SHA-1 `d90ac17e9bf17b6c61624ad9f05447bdb5efc01a` (LADXR's `README.md`). The app checks the SHA-1 and shows advice when it differs or the file is missing, like ALttP's `base.sfc` advice.
- **Hybrid port.** This is option (b) of the grilling:
  - The **seed-dependent part** is ported to C99: Python's RNG, the logic, the item pool, the fill, and every ROM write that depends on the placement or on the RNG (item writes, hints and other texts, entrances, bosses and minibosses, goal, title-screen seed, frame counter count, …).
  - The **settings-only part** is precomputed off-device by the frozen oracle. That means the assembled ASM patches and the fixed `rom.patch` calls each setting value enables. It is stored as committed **patch tables** (`data/ladx/`), which the C side applies.
  - Which side each patch call of `generator.generateRom` falls on is decided by Phase 2's inventory, not by guess.
  - Rejected: (a) a full C99 port, LADXR's assembler included, which would cost about as much as the whole ALttP port; and (c) shipping a Python 3 interpreter for armhf and running LADXR as is, which would be several MB, probably about a minute per seed on the Cortex-A7, and outside this repository's C99 rules.
- **The LADX oracle needs the real ROM.** `ROM.patch` (`rom.py:13-28`) checks the old bytes before writing, and `ROMWithTables` parses and re-serializes texts, entities and rooms (`romTables.py:182-212`). LADXR therefore fails or diverges on a synthetic ROM. Running LADXR patched to skip those checks on a synthetic ROM was rejected: its output would not be the real ROM's, so the golden would prove nothing.
- **How the ROM reaches the oracle.** The owner pastes a temporary download link into the chat at the start of Phase 2's session. The rules:
  - The link is never written into a tracked file, the plan, a step note, a commit message or a PR.
  - The ROM is downloaded into `tmp/` (git-ignored), and refused unless its SHA-1 is `d90ac17e…`.
  - At the end of Phase 2 the owner deletes the link.
  - Every later phase runs without the ROM. Regenerating LADX goldens or tables later (tier 2, a fix) needs a new link. That is accepted.
  - Rejected: a private GitHub repository holding the ROM, the owner running the oracle locally, and a token-protected URL.
- **Nothing in `data/ladx/` or in the LADX goldens contains Nintendo bytes.** Every byte of the output that comes from the source ROM, moved or not, is encoded as a reference to a source offset (like BPS SourceCopy), never as a literal. A check (Phase 2) fails on any literal run of 4 bytes or more that also appears in the source ROM. It runs where the ROM is, and its result goes in the step note.
- **Python's RNG, exactly.** `randomizer.py:29` seeds `random.Random(seed_bytes)`, and `randomizer.py:42` copies that state into the global `random` that some patches draw from. The C port reproduces CPython's MT19937, its seeding from bytes (version 2, SHA-512), and every method LADXR calls (`random`, `randrange`/`randint`, `choice`, `shuffle`, `sample`, …), each unit-tested against values CPython printed. Those values are committed fixtures, which need no ROM.
- **Determinism is proven, not assumed.** The oracle runs each case twice with different `PYTHONHASHSEED` values and requires identical output. If LADXR iterates over unordered sets or dicts of objects in a way that changes the output, `tests/ladx/oracle/ladxr.patch` fixes the order (as `vt.patch` does for VT), in its own commit, and the goldens are generated with it.
- **Settings tier 1 (this plan)**, from LADXR's `settings.py`:
  - Main: `logic`, `accessibility`, `race`, and `forwardfactor`, limited to a finite list of values taken from LADXR's own web UI (`www/`), with the list and its source in the step note.
  - Items, Dungeon Items, Gameplay: every setting and value.
  - Special: `evilshop`, `bowwow`, `owlstatues`, `keyholesanity`, `superweapons`.
  - User options: `quickswap`, `textmode`, `lowhpbeep`, `noflash`, `nagmessages`, `follower`, `linkspalette`, and `music` restricted to default, `random`, `off` and `shifted`.
  - The seed is never typed; it is always random.
- **Unsupported in tier 1**, shown greyed in the grid with a reason through the existing unsupported-pairs mechanism:
  - every `overworld` value other than `normal`, and `dungeonchainlength`;
  - every `entranceshuffle` value other than `none`, plus `randomstartlocation`, `dungeonshuffle`, `shufflejunk`, `shuffleannoying` and `shufflewater`;
  - `shopsanity`;
  - `gfxmod` (third-party fan sprite packs in LADXR's `gfx/`, with unclear licences);
  - `music=ffa` (imports Final Fantasy music).
- **Later tiers, out of this plan.**
  - Tier 2, its own plan: the Entrances group.
  - Tier 3, maybe never: the alternative overworlds (`dungeondive`, `nodungeons`, `dungeonchain`, `alttp`, `random`, with `mapgen` and `dungeongen`), `shopsanity`, `gfxmod` and `music=ffa`.
- **A tier-1 value too costly to port** may move to "unsupported" only with its reason recorded in `notes/plan3-scope-cuts.md` and listed in the plan's PR. The owner reviews every such cut. Nothing is dropped silently.
- **One app, renamed.**
  - The Onion folder is `App/ZeldaRando/`, `config.json` `"label": "Zelda Randomizer"`, the binary `zrando`, and the release zip `ZeldaRando.zip`.
  - Nobody has installed the ALttPR release yet, so there is no migration code. The README says only that an old `App/ALttPR/` can be deleted.
  - Rejected: a second Onion app for LADX, and keeping the "ALttPR" name or only its visible label.
- **SD layout per game.**
  - `App/ZeldaRando/alttp/` holds `base.sfc`, `base.bps`, `history/` and `settings.json`.
  - `App/ZeldaRando/ladx/` holds `base.gbc`, the patch tables, `history/` and `settings.json`.
  - Generated ROMs go to `Roms/SFC/ZeldaRando/` and `Roms/GBC/ZeldaRando/`.
  - The GBC folder, core and cache are subject to Phase 1's Onion research. A mismatch stops the step and asks the owner.
- **Game-choice screen.**
  - It is shown at every launch, with the cursor on the last game used (remembered in `App/ZeldaRando/settings.json`).
  - A opens the game's Generate and History tabs. B from those tabs returns to the choice screen. B on the choice screen quits.
  - Both games are always selectable. A missing or wrong base ROM is reported inside the game's Generate tab.
  - Rejected: three tabs ALttP/LADX/History, a "Game" row at the top of Generate, and a skip-to-last-game shortcut.
- **Same conventions as ALttP for LADX**:
  - a random seed per generation;
  - a readable ROM name built from the main settings and a hex code, with the same collision rule;
  - a History record with the settings, the date, and the spoiler only when the spoiler option was on (default off);
  - the spoiler is LADXR's JSON spoiler log, part of each case's golden.
- **Time budget**: the same as ALttP's, about 10 s projected on the Cortex-A7. `make -C tests/<game> measure` covers LADX.
- **Theme**: LADX screens use the shipped `themes/alttp`. A LADX theme is a separate plan if the owner wants one.
- **Code layout.**
  - `rando/` becomes `alttp/` and `tests/rando/` becomes `tests/alttp/`, in one mechanical step.
  - LADX lives in `ladx/` (`include/`, `src/`, `host/`) and `tests/ladx/` (`unit/`, `cases/`, `oracle/`), with the same rules as `alttp/`. `app/` uses both, and nothing in `alttp/` or `ladx/` includes the other or `app/`.
  - A `common/` library is created only when a piece of code has two real users (for example BPS or SHA-1). It is never created speculatively.
- **ROM guard.** A check in `make test` fails if git tracks a file whose SHA-1 is `d90ac17e…` or the JP ALttP ROM's, or any `.gb`, `.gbc`, `.sfc` or `.smc` file. It proves itself on planted files first.
- **`CLAUDE.md`** is updated in the same steps as the code it describes: the app's name, the layout, the checks, and the "No Nintendo ROM" ground rule rewritten for the LADX oracle's temporary link.

### Inherited guardrails (`CLAUDE.md`)

- Goldens are the source of truth, and are never regenerated to get green. LADX goldens are generated only in Phase 2, or later with a new link, in their own commit.
- `make test` stays fast. Anything slow goes in a separate target called from `make check`.
- Never weaken a check. Never launch the game or an emulator. Never merge a pull request. Never work on or push a `claude/*` branch.
- Everything in the repository is in English. Comments follow `CLAUDE.md`'s rule.

### Out of scope

- Tiers 2 and 3 above, multiworld, plando, race-seed sharing and seed entry.
- A LADX-specific theme, and translations other than English.
- Any other Zelda game, and any firmware other than OnionOS.

## 3. Phases and steps

### Phase 1 — Rename and game choice, without the ROM (unattended)

1. **Rename `rando/` to `alttp/` and `tests/rando/` to `tests/alttp/`.** This is a mechanical step: move the files, then update the paths in the Makefiles, scripts, skills (`check-e2e`, `fix-bug`, `plan-pilot`, …), CI workflow, `CLAUDE.md` and README. No behaviour changes, and no golden is touched. Verified by `make check` and `make sdl-pc` green before and after, with the same e2e output, and `git grep -n 'tests/rando\|rando/include\|rando/src'` empty outside git history.
2. **Rename the app to Zelda Randomizer.** `App/ZeldaRando/`, the label, binary `zrando`, `ZeldaRando.zip`, `launch.sh`, `build_zip.py` and its self-test, the user-visible strings (`ui_strings_en.c`), the UI snapshots, the README and `docs/onion.md`'s app-folder facts. Verified by `make check`, `make sdl-pc` (goldens rewritten only if a visible string changed, in this commit, with the reason stated), and `make package` producing a zip whose only top-level folder is `App/ZeldaRando/`.
3. **Per-game SD layout for ALttP.** Move ALttP's files to `App/ZeldaRando/alttp/` and its ROMs to `Roms/SFC/ZeldaRando/`, with the advice strings updated. Verified by the `app/sd` unit tests and the UI snapshots of the base-missing and saved-in screens.
4. **Onion facts for GBC**, added to `docs/onion.md` with links to Onion's sources, as §6 does for SNES: the `Emu/GBC` package (its `rompath` and default core), whether `Roms/GBC/ZeldaRando/` is listed, the cache files to delete, and the core's `.srm` and save-state paths. Verified by each fact citing a source file, and by §2's GBC paths checked against them. A mismatch stops the step and asks the owner.
5. **Game-choice screen in the UI core.** A new screen comes before the tabs, listing "A Link to the Past" and "Link's Awakening DX", with the cursor on the last game used. A opens the game, B in a game returns to the choice screen, and B on the choice screen quits. Until Phase 4, the LADX entry opens a Generate tab whose only row is an "unavailable" notice through the string table. Verified by UI snapshots (first launch, return with B, last game remembered, quit) and `ui_cli` key sequences. The SDL goldens and `sdl/golden_states.txt` gain the choice screen through `make sdl-goldens` on a new state only. Existing goldens change only through the moved keys, in this commit, with the reason stated.
6. **ROM guard** (§2), in `make test`, proving itself on planted files. Verified by the planted-file runs and `make test` green.
7. skill: check-net — the whole net after Phase 1.

### Phase 2 — LADXR oracle, with the ROM (interactive: the owner pastes the link at the start)

This phase runs in one session the owner opens and watches. Its first action is to ask for the link if it is not in the chat. Its last step stops and asks the owner to delete the link.

1. **Fetch and check the ROM.** `tests/ladx/oracle/fetch_rom.sh` reads the URL from its argument or from `LADX_ROM_URL`, never from a file. It downloads into `tmp/ladx/base.gbc`, refuses any SHA-1 but `d90ac17e…`, and prints only the SHA-1. If the link's host is blocked by the network policy, the session asks the owner to allow it (the environment tools). Verified by the printed SHA-1, and by `git status` showing nothing new outside `tmp/`.
2. **Pinned oracle and determinism.** `tests/ladx/oracle/setup.sh` clones LADXR at `4325207e…` into `tests/ladx/oracle/ladxr/` (git-ignored), and records the Python version in the step note. `gen_golden.sh` runs LADXR on a case: settings, plus a seed as hex bytes. Every case runs twice with `PYTHONHASHSEED=0` and `=1`, and must give identical output. Any difference is fixed in `ladxr.patch` (§2), with each change explained in the commit message. Verified by the double run on every case of step 4.
3. **Inventory.** `notes/ladx-inventory.md` lists every call of `generator.generateRom` and of what `randomizer.py` does after the fill. For each one it gives:
   - the settings it depends on, and whether it draws from the RNG or reads the placement;
   - every ROM **read** that influences an output (beyond `ROM.patch`'s old-byte check), for example the enemizer's entity reads, the text table and room parsing;
   - its side: precomputed table, or ported to C.

   A seed-dependent call that reads ROM content to decide gets an extraction: the data it reads becomes a committed fixture when it holds no Nintendo bytes (for example offsets or counts). Otherwise, it becomes a scope cut (§2) recorded in `notes/plan3-scope-cuts.md`. The `ROMWithTables.save` re-serialization (texts, entities, rooms, room sprite data, `rom.patch(0x00, 0x0DFA, …)`) is analysed here: what C has to reproduce, and what is a table. Verified by every `generateRom` call and every `ROMWithTables` table appearing in the inventory, checked by a script that diffs the inventory against `grep` of the frozen sources.
4. **Cases.** `tests/ladx/cases/` gets one case per tier-1 setting value (the other settings at LADXR's defaults), plus a baseline. `tests/ladx/combos/` gets combinations: at least every `logic` × `dungeon_keys`, every `goal` × `accessibility`, every `hardmode` × `hpmode`, and the keysanity-everything case. Each case has a fixed seed. Verified by the case list covering every tier-1 value (a script compares it with §2's list).
5. **Golden format and goldens.** Every case's golden holds:
   - the spoiler JSON;
   - the output as a write log against the source ROM: every offset LADXR's final ROM differs at, or writes, encoded with source references (§2), in a format the C side can produce without the ROM;
   - the SHA-1 of the whole output ROM, for the later full check (step 8);
   - the RNG trace by stage, like ALttP's `trace.txt`, so the fill can be claimed before the writes.

   The format and its "comparable without the ROM" property are documented in `tests/ladx/README.md`. Verified by two `gen_golden.sh` runs giving identical files, and by the Nintendo-bytes check (§2) printing `0 literal runs` for every golden.
6. **Patch tables.** `tests/ladx/oracle/gen_tables.sh` writes `data/ladx/`:
   - one table per settings-only patch group of the inventory, keyed by the setting values it depends on;
   - its layer of fixed patches applied whatever the settings;
   - the order the C side must apply them in.

   Verified by:
   - a host replay, Python on the oracle side: applying the tables to the real ROM, plus each case's seed-dependent writes taken from LADXR, gives exactly the case's output SHA-1, for every case and combo. This is the proof that the tables compose.
   - the Nintendo-bytes check printing `0 literal runs` for `data/ladx/`.
   - the total size of `data/ladx/` in the step note.
7. **Fixtures that need no ROM.** CPython RNG values for every method LADXR calls, with several seeds; LADXR's logic and location tables dumped for the logic unit tests (like ALttP's `extract_logic`); and the item pool per setting. Verified by the fixtures being byte-identical on two runs.
8. **Full-ROM check target.** `make -C tests/ladx rom-check LADX_ROM=<file>`, outside the net, runs the C generator on every case with the real ROM and compares each output's SHA-1 with the golden's. It is written now, and used in this phase only on its self-test: a planted SHA-1 mismatch. The C generator does not exist yet. Later, the owner runs it locally, or in a session with a new link. Verified by its self-test.
9. **Stop.** Commit everything, push, and stop with: "Phase 2 done: please delete the ROM link now." Delete `tmp/ladx/`. The step note says what the owner must do and that every later phase runs without the ROM.

### Phase 3 — LADX library, host only (unattended, no ROM)

1. **Python RNG in C.** `ladx/src/pyrandom.c`: MT19937, seeding from bytes (SHA-512, so a SHA-512 module tested against FIPS 180-4 vectors), and every method of the inventory. Verified by `tests/ladx/unit/test_pyrandom.c` against step 7's fixtures, under `make test` and `make unit-arm`.
2. **Settings table.** Tier-1 settings and values as one library table, with LADXR's names, and the tier-2 and tier-3 values as unsupported pairs with their reasons. Verified by a unit test against `settings.py`'s list (a fixture from step 7) and the unsupported list against §2.
3. **Logic and locations.** Port `logic/` and `locations/` (tier 1 only: `overworld=normal`, no entrance shuffle). Verified by unit tests replaying step 7's logic fixtures, through `make logic`.
4. **Item pool and fill.** Port `itempool.py` and `randomizer.py`'s fill, with the RNG trace. Verified by `e2e` claims at the `fill` stage on every case (spoiler locations and trace equal to the golden), and by `make -C tests/ladx e2e` and `e2e-combos` printing no `DIVERGENT`. A `selftest` case proves a planted divergence prints `DIVERGENT`.
5. **Table application.** Apply `data/ladx/` for a given settings set, in the order recorded, with no allocation. Verified by unit tests on synthetic tables, and by the write-log claims for the settings-only groups reaching every case.
6. **Seed-dependent writes**, one inventory group per step, in `generateRom`'s order (items, texts and hints, goal, chests and entrances, bosses and minibosses, enemizer, frame counter, title-screen seed, `ROMWithTables.save`'s re-serialization). Each step's claims reach its group on every case, with unit tests per module. Split this step into one numbered sub-step per inventory group when the plan is converged.
7. **Assembly pipeline.** `ladx_assemble`: check the source SHA-1, apply the tables, generate, write the ROM, and fix the GB header and global checksums if LADXR does. A host CLI `ladx_patch --base <file> --out <file> [settings]`. Verified by a host test on a synthetic ROM and synthetic tables, covering the order and every error path.
8. skill: check-e2e — every LADX case and combo, write logs included.
9. **Measure.** `make -C tests/ladx measure`, with the Cortex-A7 projection against the 10 s line in the step note. Above it, record the profile and the fix in `notes/`. Verified by the step note.

### Phase 4 — LADX in the app (unattended, no ROM)

1. **LADX Generate tab.** The grid from the settings table, with unsupported values greyed with their reason, the spoiler row and the Generate action. It uses the existing grid and generation flow, with game-specific runners from `app/sd`. Verified by UI snapshots and `ui_cli` key sequences: initial, each group, a greyed value, generating, ready, base missing, base wrong.
2. **LADX SD side.** Base check, output name, History records and deletion of the ROM, `.srm` and save states, using Phase 1 step 4's paths. Verified by `app/sd` unit tests on a directory used as the SD card.
3. **SDL.** The LADX states join `sdl/golden_states.txt` as new goldens (`make sdl-goldens` on new states only) and the `--draw` run with `themes/alttp`. Verified by `make sdl-pc` and `make sdl-arm`.
4. **Package.** `make package` ships `data/ladx/` in `App/ZeldaRando/ladx/`, and `make e2e-arm` and `make unit-arm` cover LADX. Verified by both targets, and by the zip's file list in the step note.
5. **README and screenshots.** The README covers both games, where to put each base ROM, the new folder name and the old `App/ALttPR/` line. `make screenshots` adds the choice screen and one LADX screen. Verified by the README's paths matching the code's (a `grep` in the step note).
6. skill: check-net — the whole net.
7. skill: rework-comments — `ladx/` and the new `app/` code.

## 4. Done when

- `make check` is green, with LADX's `test`, `logic`, `selftest`, `e2e` and `e2e-combos` included, and no `DIVERGENT` line.
- `make sdl`, `make package`, `make e2e-arm` and `make unit-arm` are green.
- `make -C tests/ladx measure` projects under ~10 s on the Cortex-A7, or the overrun is recorded in `notes/` and listed in the PR.
- `notes/plan3-scope-cuts.md` lists every tier-1 value moved to unsupported, each with its reason, and the PR lists them too.
- The owner has deleted the ROM link (Phase 2 step 9). Running `make -C tests/ladx rom-check` on the owner's machine, and playing a seed on the device, are the owner's own checks.

## 5. Open questions

- None from the grilling. Phase 2's inventory may surface seed-dependent code that reads Nintendo data. Each such case becomes either an extraction or a scope cut, and the owner reviews it at the PR.

## 6. Step notes
