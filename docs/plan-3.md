# Plan 3 — Zelda Randomizer: Link's Awakening DX next to ALttP

## 1. Goal

The Onion app generates seeds for two games on the console, with no PC and no network:

- *A Link to the Past*, as today (VT `219fcafd`, Japanese 1.0 ROM, `.sfc`).
- *Link's Awakening DX*, through a C99 port of **LADXR** (`daid/LADXR`, MIT), frozen at one commit. The app reads the English 1.0 GBC ROM and writes a patched `.gbc` that Onion's GBC core plays.

The app is renamed **Zelda Randomizer**. At each launch it opens a game-choice screen. Each game then has its own Generate and History tabs.

The goals behind every rule below:

1. **The goldens are the proof.** An agent cannot play the game. A LADX seed is right when the C port's output matches LADXR's for the same seed and settings, byte for byte, as with VT for ALttP.
2. **The same code runs everywhere.** The LADX library is pure C99 like the ALttP one: no `malloc`, no platform library, and the same code on the host, the Miyoo and qemu.
3. **No Nintendo ROM in the repository or in CI.** The LADX oracle needs the real ROM (see §2). It gets it from a temporary link that lives for the whole run, into a git-ignored folder, and nothing derived from it that contains Nintendo bytes is committed.
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
- **How the ROM reaches the oracle.** The owner pastes a temporary download link into the chat once, when launching `plan-pilot`. The link lives for the whole run: the owner deletes it after the plan is done. The run never stops on purpose; it converges. The rules:
  - The link is never written into a tracked file, the plan, a step note, a commit message or a PR. It lives only in the session's environment (`LADX_ROM_URL`).
  - Every session that needs the ROM downloads it into `tmp/ladx/` (git-ignored) and refuses it unless its SHA-1 is `d90ac17e…`.
  - `make check` and CI never need the ROM; only the oracle scripts and `rom-check` read it.
  - A link that stops working is a setup problem (a `setup:` line), not a bug.
  - Rejected: a private GitHub repository holding the ROM, the owner running the oracle locally, a token-protected URL, and a link limited to one owner-watched phase (it forced run-ending stops).
- **A defect in a ROM-derived artifact is fixed at the oracle.** When a failure's proven cause is a golden, a table in `data/ladx/`, an extraction fixture or an inventory side or group, the fix goes into the oracle scripts or `ladxr.patch`, and the artifacts are regenerated with the ROM in their own commit, the reason and evidence in the commit message and the step note. A ROM-derived file is never edited by hand, and a scope cut is never used for this.
- **Nothing in `data/ladx/` or in the LADX goldens contains Nintendo bytes.** Every byte of the output that comes from the source ROM, moved or not, is encoded as a reference to a source offset (like BPS SourceCopy), never as a literal. A check (Phase 2) fails on any literal run of 4 bytes or more that also appears in the source ROM. It runs where the ROM is, and its result goes in the step note.
- **Python's RNG, exactly.** `randomizer.py:29` seeds `random.Random(seed_bytes)`, and `randomizer.py:42` copies that state into the global `random` that some patches draw from. The C port reproduces CPython's MT19937, its seeding from bytes (version 2, SHA-512), and every method LADXR calls (`random`, `randrange`/`randint`, `choice`, `shuffle`, `sample`, …), each unit-tested against values CPython printed. Those values are committed fixtures, which need no ROM.
- **Determinism is proven, not assumed.** The oracle runs each case twice with different `PYTHONHASHSEED` values and requires identical output. If LADXR iterates over unordered sets or dicts of objects in a way that changes the output, `tests/ladx/oracle/ladxr.patch` fixes the order (as `vt.patch` does for VT), in its own commit, and the goldens are generated with it.
- **Settings tier 1 (this plan)**, from LADXR's `settings.py`:
  - Main: `logic`, `accessibility` and `race`. `forwardfactor` is fixed at LADXR's default 0.0 and not shown, as on LADXR's site (`www/index.html` hides it): 0.0 selects `RandomItemPlacer`, any positive value `ForwardItemPlacer` (`randomizer.py:55-56`), which is not ported.
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
  - The GBC folder, core and cache are subject to Phase 1's Onion research. On a mismatch the run adapts these paths to the facts found, and records the change and its reason in the step note and the PR.
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

- Goldens are the source of truth, and are never regenerated to get green. LADX goldens are generated in Phase 2, and regenerated only when the oracle, `ladxr.patch` or the case list changes on purpose, in their own commit with the reason.
- `make test` stays fast. Anything slow goes in a separate target called from `make check`.
- Never weaken a check. Never launch the game or an emulator. Never merge a pull request. Never work on or push a `claude/*` branch.
- Everything in the repository is in English. Comments follow `CLAUDE.md`'s rule.

### Out of scope

- Tiers 2 and 3 above, multiworld, plando, race-seed sharing and seed entry.
- A LADX-specific theme, and translations other than English.
- Any other Zelda game, and any firmware other than OnionOS.

## 3. Phases and steps

### Phase 1 — Rename and game choice, ROM-free work (unattended)

The owner pastes the ROM link when launching the run. Phase 1 does not use it.

1. **Rename `rando/` to `alttp/` and `tests/rando/` to `tests/alttp/`.** This is a mechanical step: move the files, then update the paths in the Makefiles, scripts, skills (`check-e2e`, `fix-bug`, `plan-pilot`, …), CI workflow, `CLAUDE.md` and README. No behaviour changes, and no golden is touched. Verified by `make check` and `make sdl-pc` green before and after, with the same e2e output, and a grep for any `rando/` path token (relative ones such as `../../rando` included) over tracked files, minus `docs/plan-*.md` and `notes/`, finding nothing; any match that must stay is listed with its reason in the step note.
2. **Additive SDL golden write.** `goldens.sh` gains a mode that writes goldens only for states that have none, never touching an existing one (still rendered twice, refused unless byte-identical). Verified by its self-proof on planted files: an existing golden left byte-identical, and a refusal when a requested state already has one.
3. **Rename the app to Zelda Randomizer.** `App/ZeldaRando/`, the label, binary `zrando`, `ZeldaRando.zip`, `launch.sh`, `build_zip.py` and its self-test, the user-visible strings (`ui_strings_en.c`), the UI snapshots, the README and `docs/onion.md`'s app-folder facts. Verified by `make check`, `make sdl-pc` (goldens rewritten only if a visible string changed, in this commit, with the reason stated), and `make package` producing a zip whose only top-level folder is `App/ZeldaRando/`. A changed golden is rewritten by removing only that golden and writing it back with step 2's additive mode; `goldens.sh --compare` keeps every other golden byte-identical, and the step note lists the rewritten states with their reason.
4. **Per-game SD layout for ALttP.** Move ALttP's files to `App/ZeldaRando/alttp/` and its ROMs to `Roms/SFC/ZeldaRando/`, with the advice strings updated. `sd_fixture` and `smoke_sdl.sh` follow the new layout in this step. Verified by the `app/sd` unit tests, the UI snapshots of the base-missing and saved-in screens, and `make sdl-pc` (any golden whose visible path changed rewritten in this commit, with the reason stated). A changed golden is rewritten by removing only that golden and writing it back with step 2's additive mode; `goldens.sh --compare` keeps every other golden byte-identical, and the step note lists the rewritten states with their reason.
5. **Onion facts for GBC**, added to `docs/onion.md` with links to Onion's sources, as §6 does for SNES: the `Emu/GBC` package (its `rompath` and default core), whether `Roms/GBC/ZeldaRando/` is listed, the cache files to delete, and the core's `.srm` and save-state paths. Verified by each fact citing a source file, and by §2's GBC paths checked against them. On a mismatch, §2's GBC paths are adapted to the facts (in the plan's step note and the code that follows), with the reason.
6. **Game-choice screen in the UI core.** A new screen comes before the tabs, listing "A Link to the Past" and "Link's Awakening DX", with the cursor on the last game used. A opens the game, B in a game returns to the choice screen, and B on the choice screen quits. Until Phase 4, the LADX entry opens a Generate tab whose only row is an "unavailable" notice through the string table. Verified by UI snapshots (first launch, return with B, last game remembered, quit) and `ui_cli` key sequences. The SDL goldens and `sdl/golden_states.txt` gain the choice screen through step 2's additive mode, on the new state only. Any existing golden whose pixels change (for example a help line gaining B) is rewritten by removing only that golden and writing it back with step 2's additive mode; `goldens.sh --compare` keeps every other golden byte-identical, and the step note lists the rewritten states with their reason.
7. **ROM guard** (§2), in `make test`, proving itself on planted files. Verified by the planted-file runs and `make test` green.
8. **ROM-free oracle work.** LADXR runs its logic and fill without a ROM (its own CI uses `SKIP_ROM_GENERATION`, and `randomizer.py` calls `generateRom` only after the fill). This step:
   - writes `tests/ladx/oracle/setup.sh`, which clones LADXR at `4325207e…` into `tests/ladx/oracle/ladxr/` (git-ignored) and records the Python version in the step note;
   - produces the ROM-free fixtures: CPython RNG values for every method LADXR calls, with several seeds; LADXR's logic and location tables for the logic unit tests (like ALttP's `extract_logic`); the item pool per setting; and LADXR's settings and values list, with its own rules on incompatible combinations;
   - runs the baseline and a handful of settings in `SKIP_ROM_GENERATION` mode twice, with `PYTHONHASHSEED=0` and `=1`, comparing the placement and the RNG trace; the second run uses a fresh clone in another directory, so directory-listing order varies too. Any difference is fixed in `ladxr.patch` (§2), in its own commit, with each change explained;
   - audits the frozen sources for `os.listdir`, `glob` and `os.walk`: each one on the tier-1 path (for example `patches/endscreen.py`'s unsorted `cats` list, fed to `random.choice`) is made order-stable in `ladxr.patch`, in its own commit, or justified in the step note. The same audit covers iteration over sets, or dicts keyed by objects, that hash by identity (no `__hash__`, so `PYTHONHASHSEED` does not vary their order) wherever the order reaches the RNG, the placement, a ROM write or the spoiler JSON; the known instance is `spoilerLog.py`'s `accessibleItems`, built from `Explorer`'s visited set. Phase 2 step 2's double run then re-checks the patched paths.

   Verified by the fixtures being byte-identical on two runs, and by the double run matching.
9. **Phase 2's ROM-free tooling, self-proven.** Write and prove on planted synthetic inputs: the Nintendo-bytes check (planted literal runs), the read-trace instrumentation (planted reads, one per channel), `fetch_rom.sh`'s SHA-1 refusal (a planted wrong file), Phase 2's push gate as a pre-push hook chained with `no_claude_branch.sh` (it reads the link from the environment, never a file, and refuses a planted link in a commit, a planted literal run against a planted ROM and a planted stray untracked file, while a clean push passes) and the script that diffs the inventory against `grep` of the frozen sources. Commit a static draft of `notes/ladx-inventory.md`: `generateRom`'s call list, each call's setting dependencies and RNG use, with the grep-diff script passing on it. Phase 2 then only calibrates, traces and decides on real data. Verified by each tool's planted-input self-proof and the grep-diff script passing on the draft.
10. skill: check-net — the whole net after Phase 1.

### Phase 2 — LADXR oracle, with the ROM (unattended)

Questions go to `guideline` as in the rest of the run; each decision is recorded in the step note and listed in the PR.

A resumed session first checks out the run's branch and confirms the last step note and `progress.md` are there. From Phase 2 on, at every session start, the session re-runs `fetch_rom.sh` and `setup.sh` (both idempotent) before the next step, checks the printed SHA-1, and re-arms the push-gate hook (Phase 1 step 9), until Phase 4 step 8 disarms it for good. The hook enforces, before every push until the end of the run, whichever skill pushes: `git log -p` of the branch is searched for the link (`grep -F`), and the Nintendo-bytes check (§2, calibrated in step 1) runs on every file the push adds or changes, anywhere in the repository; a link match, a literal run, or an untracked file outside the git-ignored paths (for example a stray `LADXR_<seed>.*` LADXR wrote into the working directory) blocks the push.

1. **Fetch and check the ROM.** `tests/ladx/oracle/fetch_rom.sh` reads the URL from its argument or from `LADX_ROM_URL`, never from a file. It downloads into `tmp/ladx/base.gbc`, refuses any SHA-1 but `d90ac17e…`, and prints only the SHA-1. If the link's host is blocked by the network policy, the session requests it through the environment tools (the owner approves the prompt). Verified by the printed SHA-1, and by `git status` showing nothing new outside `tmp/`.

   Right after the SHA-1 check, it calibrates the Nintendo-bytes check (§2) before anything depends on it. The check runs on Phase 1's committed text and fixtures, on a sample English file standing in for notes and READMEs, and on the baseline's raw output diff, split into seed-dependent writes and settings-only patch bytes. The hit counts and examples go in the step note. LADX text is plain ASCII, so hits are expected. A hit the check cannot clear without the C side needing the ROM is settled here (a `guideline` decision recorded in the step note), before step 3 and step 5's format. Then it installs and arms the push-gate hook, verified by one refused planted push.
2. **Oracle and full-generation determinism.** `setup.sh` (Phase 1 step 8) is re-run. `gen_golden.sh` runs LADXR on a case: settings, plus a seed as hex bytes, with the ROM. Every case runs twice, with `PYTHONHASHSEED=0` and `=1` and the second run on a fresh clone in another directory, and must give identical output; this now covers the ROM-writing part too. Any difference is fixed in `ladxr.patch` (§2), with each change explained in the commit message. Verified by the double run on the baseline and a handful of settings cases (step 4 then extends it to every case).
3. **Inventory.** `notes/ladx-inventory.md` lists every call of `generator.generateRom` and of what `randomizer.py` does after the fill. For each one it gives:
   - the settings it depends on, and whether it draws from the RNG or reads the placement;
   - every ROM **read** that influences an output (beyond `ROM.patch`'s old-byte check), for example the enemizer's entity reads, the text table and room parsing;
   - its side: precomputed table, or ported to C.

   The spoiler log is its own seed-dependent group: `spoilerLog.SpoilerLog` reads the seed, the world setup and every item back from the output ROM, so its reads are covered by the read trace and it comes after the groups whose writes it reads.

   A seed-dependent call that reads ROM content to decide gets an extraction: the data it reads becomes a committed fixture when it holds no Nintendo bytes (for example offsets or counts). Otherwise, it becomes a scope cut (§2) recorded in `notes/plan3-scope-cuts.md`. The `ROMWithTables.save` re-serialization (texts, entities, rooms, room sprite data, `rom.patch(0x00, 0x0DFA, …)`) is analysed here: what C has to reproduce, and what is a table. Verified by:
   - every `generateRom` call and every `ROMWithTables` table appearing in the inventory, checked by a script that diffs the inventory against `grep` of the frozen sources;
   - a read trace: the oracle logs every read outside `ROM.patch`'s old-byte check, of the ROM's bytes (indexing, slicing, iteration, search, whole-bank sums such as the header checksum) and of the `ROMWithTables` parsed tables (texts, entities, rooms, room sprite data, backgrounds, parsed before any group runs), tagged with the inventory group running. It first proves itself on planted reads, one per channel, each reported with its group. A script fails on any read in a seed-dependent group that its inventory entry does not cover (extraction fixture, table or recorded scope cut), on the baseline and the settings cases at hand;
   - every seed-dependent group naming the ROM-free evidence its Phase 3 sub-step will be claimed against: its golden write-log group, plus the fixtures it needs. A group with no such evidence becomes an extraction or a scope cut here (§2), before goldens and tables are generated.
4. **Cases.** `tests/ladx/cases/` gets one case per tier-1 setting value (the other settings at LADXR's defaults), plus a baseline. `tests/ladx/combos/` gets combinations: at least every `logic` × `dungeon_keys`, every `goal` × `accessibility`, every `hardmode` × `hpmode`, and the keysanity-everything case. Each case has a fixed seed. Verified by the case list covering every tier-1 value (a script compares it with §2's list).
5. **Golden format and goldens.** Every case's golden holds:
   - the placement (location to item), recorded by the oracle right after `item_placer.run` from the logic objects, not read from the ROM, for every case, `race` included;
   - the spoiler JSON, except on `race` cases, where LADXR writes none;
   - the output as a write log against the source ROM, in LADXR's write order, every write tagged with its group from step 3's inventory (as ALttP's `rom_writes.txt` tags its writer), encoded with source references (§2), in a format the C side can produce without the ROM;
   - the SHA-1 of the whole output ROM, for the full check (step 9);
   - the RNG trace by stage, like ALttP's `trace.txt`, so the fill can be claimed before the writes.

   The format and its "comparable without the ROM" property are documented in `tests/ladx/README.md`.

   Verified by:
   - two `gen_golden.sh` runs, one with `PYTHONHASHSEED=0` and one with `=1` on a fresh clone, giving identical files for every case and combo;
   - decoding each golden's write log against the real ROM giving exactly that golden's output SHA-1, for every case and combo (only possible while the ROM is here);
   - every write of every golden carrying a tag, and the set of tags equal to the inventory's groups;
   - every literal-or-reference choice in a seed-dependent group's write log being reproducible without the ROM: the C-side encoding rule documented in `tests/ladx/README.md` gives the golden's exact encoding on that group's own writes;
   - step 3's read-trace check passing on every case and combo;
   - each inventory group, edits made through `ROMWithTables` included, being claimable on its own from the golden, independently of the groups before it;
   - the Nintendo-bytes check (§2) printing `0 literal runs` for every golden.
6. **Patch tables.** `tests/ladx/oracle/gen_tables.sh` writes `data/ladx/`:
   - one table per settings-only patch group of the inventory, keyed by the setting values it depends on;
   - its layer of fixed patches applied whatever the settings;
   - the order the C side must apply them in.

   Verified by:
   - a host replay, Python on the oracle side: applying the tables to the real ROM, plus each case's seed-dependent writes taken from its golden write log, gives exactly the case's output SHA-1, for every case and combo. This is the proof that the tables compose.
   - a random sample: LADXR run live on a fixed, recorded sample of random tier-1 setting combinations, each with a fixed seed (sample size and generator in the step note); for each, the tables chosen by their keys plus that run's own seed-dependent writes give LADXR's output SHA-1. A mismatch means a table key misses a dependency (the assembler's global constants, `generator.py:80-89`, and cross-conditions such as `hardmode == 'ohko'` make this likely): widen the key, regenerate, and repeat until the whole sample passes. Each sample combination is then committed as a combo case in `tests/ladx/combos/` with its golden, under step 5's checks, so Phase 3 claims it too;
   - the Nintendo-bytes check printing `0 literal runs` for `data/ladx/`.
   - the total size of `data/ladx/` in the step note.
7. **Extraction fixtures.** Every extraction fixture the inventory names (the ROM-free fixtures were made in Phase 1 step 8). Verified by Phase 1 step 8's fixture generation re-run with the final `ladxr.patch` giving files byte-identical to the committed ones, with the Python version compared to the one Phase 1 recorded (any difference is regenerated in its own commit, with the reason in the step note); by the fixtures being byte-identical on two runs, and a script checking that each fixture the inventory names exists.
8. **Nintendo-bytes check over everything committed.** The check (§2) runs on every file Phase 2 commits, anywhere in the repository, fixtures included. Verified by it printing `0 literal runs`.
9. **Full-ROM check target.** `make -C tests/ladx rom-check LADX_ROM=<file>`, outside the net, runs the C generator on every case with the real ROM and compares each output's SHA-1 with the golden's. It is written now and used in this phase only on its self-test, a planted SHA-1 mismatch: the C generator does not exist yet. Phase 3 runs it for real. Verified by its self-test.
10. **Split Phase 3 step 6.** Run a plan-review with `notes/ladx-inventory.md` as input. Its accepted split replaces Phase 3 step 6 with one numbered sub-step per seed-dependent inventory group, each with its own claims and unit tests, committed with this step's note. The sub-steps are ordered so that each one's claim can be reached with the sub-steps before it, not by `generateRom`'s order. They are numbered `6.1 … 6.n` under step 6 (steps 7 to 10 keep their numbers); each runs as its own plan step, with its own PRD and `### Step 3.6.<k>` note; step 6 itself gets a one-line closing note once every sub-step has one, and no PRD. Verified by Phase 3 step 6 listing every seed-dependent group of the inventory, in an order with that property.

### Phase 3 — LADX library, host only (unattended)

The ROM stays in `tmp/ladx/` for the oracle and `rom-check`; `make check` never reads it. A failure proven to come from a Phase 2 artifact is fixed at the oracle and the artifact regenerated (§2), most likely at step 7's net-effect claim and step 9.

1. **Python RNG in C.** `ladx/src/pyrandom.c`: MT19937, seeding from bytes (SHA-512, so a SHA-512 module tested against FIPS 180-4 vectors), and every method of the inventory. This step also extends `make unit-arm` to `tests/ladx`'s unit tests, adds LADX's unit tests to the root `make test`, and extends `make lint` (`-std=c99 -pedantic-errors`) to `ladx/`, verified by `make lint` building the `ladx/` sources. Verified by `tests/ladx/unit/test_pyrandom.c` against Phase 1 step 8's fixtures, under `make test` and `make unit-arm`.
2. **Settings table.** Tier-1 settings and values as one library table, with LADXR's names, and the tier-2 and tier-3 values as unsupported pairs with their reasons. Verified by a unit test against `settings.py`'s list (a fixture from Phase 1 step 8) and the unsupported list against §2.
3. **Logic and locations.** Port `logic/` and `locations/` (tier 1 only: `overworld=normal`, no entrance shuffle). Verified by unit tests replaying Phase 1 step 8's logic fixtures, through `make logic`, which this step extends to LADX.
4. **Item pool and fill.** Port `itempool.py` and `randomizer.py`'s fill, with the RNG trace. Verified by `e2e` claims at the `fill` stage on every case (placement record and RNG trace equal to the golden's; the spoiler, which LADXR builds by reading the finished ROM back, is claimed later, in step 6), and by `make -C tests/ladx e2e` and `e2e-combos` printing no `DIVERGENT`. A `selftest` case proves a planted divergence prints `DIVERGENT`. This step wires LADX's `selftest`, `e2e` and `e2e-combos` into the root `make check`, verified by `make check` printing the LADX lines. This step also adds `make -C tests/ladx measure` for the run stopped after `fill`, with its Cortex-A7 projection in the step note (step 10 extends it to the whole pipeline).
5. **Table application.** Apply `data/ladx/` for a given settings set, in the order recorded, with no allocation. Verified by unit tests on synthetic tables, and by the write-log claims for the settings-only groups reaching every case. This first write-log step also self-proves the LADX e2e driver's claims on planted write logs: a divergence inside a claimed group prints `DIVERGENT`; one only in an unclaimed group, before or after it, does not; a literal-for-reference swap with the same bytes does; and so does a write with the wrong group tag.
6. **Seed-dependent writes**, one inventory group per step, in an order where each claim is reachable with the steps before it (items, texts and hints, goal, chests and entrances, bosses and minibosses, enemizer, frame counter, title-screen seed, `ROMWithTables.save`'s re-serialization). Each step's claims reach its group on every case, with unit tests per module. Phase 2 step 10 replaces this step with one numbered sub-step per inventory group.
7. **Assembly pipeline.** `ladx_assemble`: check the source SHA-1, apply the tables, generate, write the ROM, and fix the GB header and global checksums if LADXR does. A host CLI `ladx_patch --base <file> --out <file> [settings]`. Verified by a host test on a synthetic ROM and synthetic tables, covering the order and every error path. Also verified, on every case and combo, by a claim on the net effect of the C pipeline's whole write log against the golden's: the final value of every written output offset, as a literal or a source reference, so no ROM is needed; a planted overlap-order swap prints `DIVERGENT`. Finally, `make -C tests/ladx rom-check` runs against the built `ladx_patch` with a synthetic `LADX_ROM` (every case reports the wrong-base refusal as a refusal, not as a SHA-1 mismatch, and its planted-mismatch self-test still passes), then with the real ROM in `tmp/ladx/`: every case's and combo's output SHA-1 equals its golden's.
8. **LADX-aware debugging skills.** `check-e2e` (and the LADX-blind parts of `fix-bug` and `help-me-debug`) take the game's test directory and cover LADX: its `e2e` and `e2e-combos` commands, its single-case repro, its ranking (placement and RNG trace first, then the write log in inventory-group order) and where its RED unit tests go. Verified by a planted LADX divergence that the skill's documented first commands print as the top failure.
9. skill: check-e2e — on `tests/ladx`: every LADX case and combo, write logs included, then `rom-check` with the real ROM. A green ALttP net alone does not complete this step.
10. **Measure.** `make -C tests/ladx measure`, with the Cortex-A7 projection against the 10 s line in the step note. Above it, record the profile and the fix in `notes/`. Verified by the step note.

### Phase 4 — LADX in the app (unattended)

1. **LADX SD side.** Base check, generation runner, output name, History records and deletion of the ROM, `.srm` and save states, using Phase 1 step 5's paths. Verified by `app/sd` unit tests on a directory used as the SD card.
2. **LADX Generate tab.** The grid from the settings table, with unsupported values greyed with their reason, the spoiler row and the Generate action. It uses the existing grid and generation flow, with step 1's runners from `app/sd`. Verified by UI snapshots and `ui_cli` key sequences: initial, each group, a greyed value, generating, ready, base missing, base wrong.
3. **SDL.** The LADX states join `sdl/golden_states.txt` as new goldens (Phase 1 step 2's additive mode, new states only) and the `--draw` run with `themes/alttp`. Verified by `make sdl-pc` and `make sdl-arm`.
4. **Package.** `make package` ships `data/ladx/` in `App/ZeldaRando/ladx/`, `make e2e-arm` covers LADX, and `make unit-arm` still covers it (since Phase 3 step 1). Verified by both targets, and by the zip's file list in the step note.
5. **README and screenshots.** The README covers both games, where to put each base ROM, the new folder name and the old `App/ALttPR/` line. `make screenshots` adds the choice screen and one LADX screen. Verified by the README's paths matching the code's (a `grep` in the step note).
6. skill: check-net — the whole net.
7. skill: rework-comments — `ladx/` and the new `app/` code.
8. **Close the ROM window.** In this order: the link search over the run branch's whole `git log -p` finds nothing; `python3 -I tests/ladx/oracle/push_gate.py --uninstall` restores the plain `no_claude_branch.sh` hook; delete `tmp/ladx/`. The step note tells the owner the link can now be deleted. Verified by the empty search, the pre-push hook byte-identical to what `no_claude_branch.sh --install` writes, `tmp/ladx/` gone, and the step-note push going through.

## 4. Done when

- `make check` is green, with LADX's `test`, `lint`, `logic`, `selftest`, `e2e` and `e2e-combos` included, and no `DIVERGENT` line.
- `make sdl`, `make package`, `make e2e-arm` and `make unit-arm` are green.
- `make -C tests/ladx measure` projects under ~10 s on the Cortex-A7, or the overrun is recorded in `notes/` and listed in the PR.
- `notes/plan3-scope-cuts.md` lists every tier-1 value moved to unsupported, each with its reason, and the PR lists them too.
- `make -C tests/ladx rom-check` with the real ROM is green on every case and combo (Phase 3 step 9's note). After Phase 4 step 8 the owner deletes the ROM link; playing a seed on the device is the owner's own check.

## 5. Open questions

- None from the grilling. Phase 2's inventory may surface seed-dependent code that reads Nintendo data. Each such case becomes either an extraction or a scope cut, and the owner reviews it at the PR.

## 6. Step notes

### Step 1.1

`git mv rando alttp` and `git mv tests/rando tests/alttp` (827 renames, 811 pure); path tokens updated in the root and `tests/alttp` Makefiles, `measure.sh`, `gen_golden.sh`, CI, `.gitignore`, `targets/mac/Makefile`, `CLAUDE.md`, README, `docs/onion.md`, `docs/toolchain.md`, the `check-e2e` and `rework-comments` skills, and three unit-test comments. Identifiers (`rando_*`, `RANDO_*`, `librando*.a`, `test-rando`) unchanged. e2e + e2e-combos output identical before and after (76 lines); `make check` and `make sdl-pc` green (13 goldens pixel-identical). SDL dev packages installed with apt for `make sdl-pc`. The path-token grep keeps 8 matches, each on purpose: `CLAUDE.md:16` names upstream `alttp-ds`'s own folders; `tests/alttp/Makefile:363` and `measure_compare.sh:132,134` point into the frozen `40314c3` reference worktree, which predates the rename; `measure.sh:41,42,69,76` match the shell variable `$rando` (its value is now `$root/alttp`), not a directory.

### Step 1.2

`goldens.sh --add DIR STATE...` and `make sdl-goldens-add STATES="..."` (root and `tests/alttp`): self-proof on planted files first (adds a new golden leaving others byte-identical; refuses an existing state; refuses a mixed request writing nothing; refuses differing runs with `NONDETERMINISTIC <name>`), then unknown/duplicate names, existing goldens, the `environment.txt` fingerprint, two renders of only the requested states, and an all-or-nothing write. `--write` shares the run-comparison helper, output unchanged. Verified: adding `generate_top` is refused with the committed goldens untouched; on a copy with that PNG removed, the add re-creates it byte-identical and `--compare` reports 13 states pixel-identical; `make check` and `make sdl-pc` green. `CLAUDE.md` documents the target and the one-golden rewrite procedure.

### Step 1.3

App renamed: `App/ZeldaRando/` (still flat; the per-game split is step 1.4), `config.json` label "Zelda Randomizer", binary `zrando`, `zrando.log`, `ZeldaRando.zip`, CI artifacts `zrando-pc`/`zrando-miyoo`. Advice strings, three failure snapshots, mockup 04 and the `generation_failed` golden follow; that golden was the only `DIFFERENT` one and was rewritten alone (deleted, then `sdl-goldens-add`), the other 12 byte-identical. `make check`, `make sdl-pc`, `make mockups` and `make theme-art` green. `make package` printed `setup:` (no docker daemon here); fallback evidence: `test_package.py` 27 tests and `build_zip.py` on a stand-in binary (one top-level `App/ZeldaRando/`, same bytes on two builds). Deviation: the PRD's three chunks landed as one commit (`smoke_sdl.sh` and `CLAUDE.md` span them). `docs/onion.md`'s quoted plan-1 "§2 text" cells now say `App/ZeldaRando`. Remaining `alttpr` matches: `alttpr.com`, VT's in-game texts, `Roms/SFC/ALttPR` (step 1.4) and the README's line that an old `App/ALttPR/` can be deleted.

### Step 1.4

ALttP's SD files moved to `App/ZeldaRando/alttp/` (`base.sfc`, `base.bps`, `history/`, `settings.json`) and its ROMs to `Roms/SFC/ZeldaRando/`, built from new constants `RANDO_SD_APP_DIR` and `RANDO_SD_ALTTP_DIR`; `test_app_gen` pins every path to its literal and checks by literal path where a generation writes (and that a flat `App/ZeldaRando/base.sfc` is reported missing). `App/ZeldaRando/settings.json` is left for step 1.6. Deviation from the advice wording: "Needs App/ZeldaRando/alttp/base.sfc" drops "(JP 1.0)" to fit `UI_STR_LEN_MAX` 38; the wrong-ROM line still says JP 1.0. The zip ships `App/ZeldaRando/alttp/base.bps`. `sd_fixture`, `smoke_sdl.sh`, mac target, README, `docs/onion.md`, `docs/toolchain.md`, `CLAUDE.md` follow. Goldens `seed_ready` and `generation_failed` were the only `DIFFERENT` ones, rewritten alone through `sdl-goldens-add`; then 13 pixel-identical. Screenshot `seed_ready.png` rewritten; mockups 03/04 updated (`make mockups`: no MISMATCH; `make theme-art`: 14 pixel-identical). `make check`, `make sdl-pc` green; `make package` `setup:` (no docker), fallback zip listing as expected.

### Step 1.5

`docs/onion.md` gains section 11 (GBC list, saves and states), cited at the pinned Onion commits `07505ea5`/`2bab767e`: `Emu/GBC` (`rompath ../../Roms/GBC`, `extlist bin|dmg|gb|gbc|zip|7z`) starts Gambatte, Onion's fork `schmurtzm/gambatte-libretro@04e73b82` (newly pinned; the shipped binary's `04e73b8` is not in upstream); `library_name` `Gambatte`, no save-path override; saves `Saves/CurrentProfile/saves/Gambatte/<base>.srm` (RetroArch also registers `<base>.rtc`, written only for RTC cartridges; LADX DX being type 0x1B, `.srm` only, is an inference); states under `states/Gambatte/`; cache `Roms/GBC/GBC_cache6.db` and `GBC_cache2.db`. Every §2 GBC path matches: no adaptation. Consequences G1–G7 for Phase 4 (delete also tries `<base>.rtc`; README tells users to install the GBC list with the Package Manager, which is not preinstalled — `Emu/SFC` neither), owner device checks 7–9. Verified: 37 links and 40 quotes checked against local clones; `make check` green.

### Step 1.6

New UI-core screen `ui_games` ("Choose a Game": A Link to the Past, Link's Awakening DX; help "A Open", "B Quit"); `ui_app` has three views and starts on it. B on Generate or the History list returns to it, B on it quits, Menu quits anywhere; ALttP state is kept between visits and reopening lands on Generate. LADX opens a placeholder Generate tab ("Not available yet", "B Back"). Last game remembered by `app/sd/last_game.c` in `App/ZeldaRando/settings.json` (`{"last_game":...}`), written only when the opened game differs; missing/invalid file → cursor on ALttP. No B hint added to ALttP's tabs (allowed, not required; keeps the 13 goldens pixel-identical). Every key sequence (`smoke_ui_cli`, `test_app_shell`, `smoke_sdl.sh`, `golden_states.txt`, `mockup_states.txt`) gains a leading `a`; `generating` and mockup `02` stop after 5 frames instead of 3 (the extra press draws two frames, same picture). New golden `game_choice` via `sdl-goldens-add`; `make sdl-pc` 14 states pixel-identical; `make mockups` no MISMATCH; `make check` green. Deviation: the PRD's three chunks landed as one commit (key sequences span them).

### Step 1.7

`tests/rom_guard.py` (stdlib, `python3 -I`) joins `make test` through a root `rom-guard` target: it reads git's index and staged blobs and refuses any `.gb`/`.gbc`/`.sfc`/`.smc` path (any case, any depth) and any blob that is the LADX ROM (SHA-1 `d90ac17e…`) or the JP ALttP ROM. Deviation from §2: the ALttP ROM is matched by MD5 `03a63945…` (also after a 512-byte copier header), because the repository records only its MD5. Self-proof on synthetic throwaway repos (9 planted cases caught, a clean repo passes, an ignored untracked `.sfc` not reported); four mutants each caught. `.gitignore` gains `*.gb`, `*.gbc`. Real tree: `rom_guard: 979 tracked files, no ROM`, ~0.5 s. `make check` green.

### Step 1.8

`tests/ladx/oracle/`: `setup.sh` pins LADXR `4325207e` and applies `ladxr.patch` (recorded: Python 3.13.18); `ladx_oracle.py` (helpers; `TracingRandom` overriding `random` and `getrandbits`, tracing each outer call with its MT word count); `gen_fixtures.sh [--check]` renders the fixtures twice (hash seed 0 on the setup checkout, 1 on a fresh clone) and compares: 6 files byte-identical — `pyrandom_vectors.txt` 84 KB, `settings.txt` 10 KB, `settings_validate.txt` 100 KB, `logic_graphs.txt` 1.5 MB (five `logic` baselines), `logic_vectors.txt` 3.8 MB (445 configurations, graph SHA-512 + explorer vectors), `item_pools.txt` 1.2 MB (279 cases with MT word positions) — 6.6 MB, above the PRD's 2–3 MB estimate. `tier1.txt` lists tier-1 settings (forwardfactor excluded). `oracle_run.py` runs LADXR in `SKIP_ROM_GENERATION` mode, hooked from outside; `determinism.sh`: 12 cases identical across hash seeds and checkouts. Order audit (`audit_order.py`, `order_audit.txt`): 61 sites, all audited. `ladxr.patch` holds two fixes, each its own commit: `endscreen.py`'s `cats` listing sorted; `spoilerLog.py`'s `accessibleItems` in `location_list` order. No other fix needed. Finding: LADXR crashes on some `goal=maze` seeds (`rnd.choice([])` in `patches/maze.py`), recorded as an outcome; the C port must reproduce that refusal. `test_oracle.py` (37 tests, no LADXR, no network) joins `make test` through `test-ladx-oracle`. `CLAUDE.md` describes the oracle tooling. `make check` green.

### Step 1.9

Phase 2's ROM-free tooling in `tests/ladx/oracle/`, each self-proven on planted inputs: `nintendo_bytes.py` (literal runs ≥ `--min` 4 of the ROM in raw bytes and hex-digit runs at both alignments; synthetic-ROM self-proof; 13 tests); `fetch_rom.sh` (link from argument or `LADX_ROM_URL`, `.part` download, SHA-1 `d90ac17e…` or refused; `file://` self-proof); `push_gate.py` (pre-push: link in `git log -p --text`, literal runs in every uploaded blob, untracked files outside `docs/prd/`, fails closed without link or ROM; `--install` chains after `no_claude_branch.sh` once its 12-push self-test passes, `--uninstall` restores; not armed in Phase 1); `no_claude_branch.sh --install` now leaves a chain alone; `read_trace.py` (7 read channels on ROM banks and `ROMWithTables` tables, tagged by inventory site, `ROM.patch` reads and writes excluded; self-proof with LADXR's real `ROM`/`PointerTable` on a synthetic ROM; `--blind-spots`: 52 buffer-protocol sites to review); `inventory_check.py` (diffs the inventory against the sources; 7 planted inventories). Static draft `notes/ladx-inventory.md`: 214 rows in 6 sections (`ROM.save` and `ROM.fixHeader` are separate sections, keyed like the read trace); draft sides: 37 none, 36 unused, 94 table, 45 C, 2 `?` (`changeEntrances`, `addFrameCounter`); checker: `214 rows in 6 sections match 4325207e`. Finding: the global `random` reaches the ROM through `updateEndScreen` and `reduceMessageLengths → utils.randomNumber` (not `addHints`). The draft quotes LADXR hex literals; Phase 2 step 1's calibration covers it. `CLAUDE.md` ground rule rewritten for the link. `make test` ~11 s, `make check` green.

### Step 1.10

`check-net`: `make -k check` green on the first run (lint 75 alttp/ + 21 app/ sources, logic 12, selftest passed, e2e 50 + 23 pass, 0 divergent), no `fix:` commit; `make sdl-pc` 14 goldens pixel-identical. End of Phase 1: `warn-review` flagged duplicated constants (SD paths in strings, `build_zip.py` and the C macros; the LADX ROM SHA-1 and `tmp/ladx` paths across oracle scripts), `last_game.c` repeating `last_settings.c`'s save logic, `UiApp` shaped around ALttP, and the two hook installers matching each other's text. `plan-review` turned the hook warning into one accepted method change: Phase 4 step 8 now uninstalls the fail-closed push gate before deleting `tmp/ladx/` (else the run's last pushes are refused), and the Phase 2 re-arm stops there. The other warnings were judged not to change the method (pinned by tests and snapshots; `UiApp` left to Phase 4 step 2's PRD).

### Step 2.1

ROM: the owner's Dropbox link is blocked by the environment's network policy (proxy 403); the run uses the owner's upload of the same ROM through a `file://` URL in `LADX_ROM_URL` (never written to a file). `fetch_rom.sh` printed `d90ac17e…`. Calibration (throwaway, `tmp/calib/`): Phase 1's 4-byte rule flagged 267,523 runs in Phase 1's changed files plus tracked PNG/BPS/TTF (9.0 MB) and 11,078 in the English docs — ordinary words, counting sequences, font tables. Decision (this step's): the check is scoped by path — strict (`data/ladx/`, `tests/ladx/cases/`, `tests/ladx/combos/`) keeps 4 bytes in every view; general uses 16 raw (one LADX text-box line) and 8 for hex and a new `bytelist` view; clearances counted, never silent: `trivial` (counting run), `low-info` (≤3 distinct values, general only), `ladxr-public` (bytes in LADXR's own text files at the pin); a `computed` clearance for steps 5–6 is reserved (TODO in the check). After: English docs 0 literal runs; Phase 1 sample 2 literal runs (17 and 16 bytes of English prose in `.claude/skills/rework-comments/SKILL.md` and a `test_ui_generate.c` comment), kept as coincidences (threshold not fitted to them; already on origin, reword if a later push touches them); 1,862 trivial, 65 low-info, 120 ladxr-public cleared. LADXR baseline output (seeds 02–04; seed 01 has no valid result): 122,088 changed bytes, 35,478 seed-dependent and 86,610 settings-only; under the strict scope most are literals → step 5's format must encode them as source references. Push gate on the same policy (16 planted pushes) and armed: `.git/hooks/pre-push` chains `no_claude_branch.sh` and `push_gate.py`; a planted 32-byte ROM run pushed to a throwaway local bare repo was refused through the real hook, a clean push passed; all throwaway refs and objects removed. Gate run on a 10-blob push: 1.4 s, ~170 MB. From now on every push exports `LADX_ROM_URL`.

### Step 2.2

`oracle_run.py --rom` runs full generation per case with the case's 16-byte seed (LADXR's `seed` setting left empty, as an untyped seed on the device; `-s seed=` would also write the seed into the short-settings string) and writes `run.txt` (placement after the fill, RNG trace by stage, ROM and spoiler SHA-1, outcome), `out.gbc`, `spoiler.json`; `--plain` runs LADXR's own `main.main` with only `os.urandom` returning the seed. `gen_golden.sh` writes cases under `tmp/` only (step 5 lifts this). `determinism.sh --full`: A (hash seed 0, setup checkout) vs B (hash seed 1, fresh clone, run from a tagged throwaway git work directory) vs plain CLI, comparators self-proven on planted directories. The audit gained environment reads (`inert` verdict). Red evidence: every successful case's ROM differed (13,481–14,438 bytes from `0x14e`) because `patches/titleScreen.py`'s `setRomInfo` writes `git describe`'s answer onto the title screen; fixed in `ladxr.patch` (version `""`, what untagged runs produced, baseline SHA-1 unchanged), own commit. After: 19 cases identical with the ROM (7 new cases for the ROM-writing code: enemies, music random/shifted/off, text modes, hard modes, superweapons, evilshop, bowwow, quickswap, palette), driver equal to LADXR's CLI; audit 69 sites, all audited; fixtures unchanged. Finding: LADXR also crashes on some seeds with `UnicodeEncodeError` in `reduceMessageLengths`/`getMarinStartingText` (seed-dependent, default text mode); like the `goal=maze` crash, the C port must refuse the same seeds. Full generation ~0.6 s per case.
