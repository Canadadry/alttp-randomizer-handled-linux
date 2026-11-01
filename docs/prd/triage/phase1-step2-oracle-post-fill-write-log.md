---
title: "The oracle runs the post-fill code and records the write log"
description: "The VT oracle stops right after the fill, so the goldens prove nothing about the texts, credits, hints and ROM writes that make up the seed the user plays."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 2, copied from `docs/plan-1.md`)

> **The oracle runs the post-fill code and records the write log.**
> - `tests/rando/oracle/vt.patch` turns `setTexts`, `randomizeCredits` and `HintService::applyHints` back on (today it comments them out). It labels their PRNG draws, and the draws `writeToRom` itself makes (`setDiggingGameRng`, `writeRNGBlock`, the `fy_shuffle` calls in `Location::getHint` and `Location/Prize.php`), as new stages in `stages.txt`.
> - `oracle_case.php` runs `World::writeToRom` on a sourceless `Rom`, with the case's heart beep. It records every write as `cases/<case>/rom_writes.txt`: one `<hex address> <hex bytes> <Rom method>` line per write, in call order, tagged by the backtrace the same way `prng_stage` tags draws. `updateChecksum` is excluded.
> - Texts, credits and initial SRAM reach the ROM as a few large writes, so the oracle also logs, per case, the setter calls that fill those buffers (key and bytes, in call order), and writes one seed-independent fixture: the encoded default text table.
> - The case format gains a hints key and a heart-beep key; their defaults are VT's `on` and `half`.
> - Regenerate the goldens in their own commit.
> - The step note states the oracle's `writeToRom` `$save` value, and lists every ROM write between VT's generation entry point and the downloaded file that the log does not capture (to start: `setSeedString` and `setStartScreenHash` under `$save`, `RandomizerController::prepSeed`'s tournament writes, `resources/js/rom.js` `save()`'s quickswap, music volume, MSU-1 resume and reduce-flashing writes). Each gets one outcome: added to the log; reproduced in C outside the log with a unit test checked against VT's source (step 11); or left out, with its reason.
>
> Verified by:
> - `gen_golden.sh` is reproducible (two runs give identical files);
> - `make -C tests/rando e2e` stays green, with the existing claims held at their current stage (`fill`) instead of `all`.
> - a `fill` claim still compares what the fill produces (the spoiler's location sections and the attempts, as checks `stages.txt` assigns to `fill`), proven by a `selftest` case where a synthetic placement divergence under a `fill` claim prints `DIVERGENT`; step 3's "green" includes these checks.

## Hypotheses about step 1's outcome

Step 1 (taking over `rando/` and `tests/rando/` from `Canadadry/alttp-ds`) is not done when this PRD is written. The PRD was written against `alttp-ds` at `8d772553f7b9a387d35e072daa9926cc07f9b62f` and assumes the following. If one of them does not hold when the step starts, the implementer re-reads the affected decision before building on it.

- **H-1-1**: Step 1 copies `alttp-ds`'s `tests/rando/` unchanged, to the same paths. That covers the oracle (`vt.patch`, `oracle_case.php` as the shared case loader, `gen_case.php` as the per-case driver, `gen_golden.sh`, `dump_pools.php`, `extract_logic.php`, `prng_check.php` and their fixtures), `cases.sh`, `stages.txt` (ten stages, `crystals` … `fill`, and five checks), `claims.txt`, `run_e2e.sh`, `selftest.sh`, `measure.sh`, and the 44 cases (`config.json`, `seed`, `attempts`, `trace.txt`, `spoiler.json`).
- **H-1-2**: `rando/` is copied unchanged:
  - the `RandoStage` enum mirrors `stages.txt`'s `stage` lines, and `test_generate`'s stage-table test enforces it;
  - `rando_generate_until` stops with `RANDO_INCOMPLETE` on the first attempt that completes the named stage, before `isWinnable`;
  - on a stage stop, the host tool `rando_gen` writes `trace.txt`, `spoiler.json` (with no playthrough) and `pool.txt`, but no `attempts`;
  - the host case reader rejects every `config.json` key outside the eleven options.
- **H-1-3**: `test_generate`'s baseline tests assume the golden trace ends with the fill: the baseline trace has 805 lines, `check_golden_stop` is called with `GOLDEN_END`, and the retry-identity test compares the whole golden trace. `test_case_reader` and `test_spoiler_json` read every case's golden.
- **H-1-4**: `claims.txt` claims all 44 cases at `all`, `make -C tests/rando e2e` prints no `DIVERGENT` line, and every case's `attempts` is `0`.
- **H-1-5**: The root `make check` runs `test`, `lint`, `logic`, `selftest` and `e2e` (step 1 adds the last three). `tests/rando/Makefile` has the targets `test`, `logic`, `selftest`, `e2e`, `measure` and `lint`.
- **H-1-6**: The unchanged oracle reproduces the committed goldens in this environment. Step 1's note records the PHP 8.3.x, composer and VT versions. `composer install` can reach its registry, and `VT_DIR` can point at a throwaway clone under `tmp/`.
- **H-1-7**: The `check-e2e` skill is copied unchanged. This step does not edit it; step 5 does.
- **H-1-8**: This repository's `.gitignore` may not ignore `tests/rando/oracle/vt/`. This PRD always runs the oracle with `VT_DIR` under `tmp/`, so the outcome does not depend on it.

## Problem Statement

The owner wants every ROM the console produces to match what alttpr.com (VT at `219fcafd`) would produce for the same settings and PRNG. An agent cannot play the game, so the goldens are the only proof the ROM is right. Today the oracle stops right after the fill:
- `vt.patch` comments out `setTexts`, `randomizeCredits` and `applyHints`;
- `writeToRom` never runs.

As a result, no golden says what the texts, credits, hints, item writes, settings, initial SRAM and RNG block of a seed should be. Phase 1 steps 4 to 11 port exactly that code into C, and they have nothing to compare against. The case format also cannot say "hints off" or pick a heart beep, which are two settings the app will offer.

## Solution

The oracle runs VT's full generation path: randomize with the post-fill code back on, then `writeToRom`, then the heart beep. It records:
- the extra PRNG draws, as new labelled stages in each case's trace;
- every ROM write, tagged with the VT method that made it, in a new per-case golden;
- the setter calls that fill the text, credits and SRAM buffers, in a second per-case golden;
- the encoded default text table, as one fixture.

The goldens are regenerated once, in their own commit. The C generator does not port anything new yet, so every claim moves from `all` to `fill`. The harness is extended so that a `fill` claim still compares everything the fill decides: every spoiler section, and the attempt count. That way, no comparison made today is lost. The step note says what the oracle passes as `$save`, and lists every write between VT's entry point and the downloaded file that the log does not hold. Each such write gets one outcome.

## User Stories

1. As the owner, I want the goldens to cover the post-fill code, so that a ROM's texts, credits and hints are proven against VT, not guessed.
2. As the owner, I want the goldens to hold every ROM write VT makes for a seed, so that later steps can prove the produced ROM byte for byte.
3. As the owner, I want the write log to need no Nintendo ROM, so that the repository and CI stay legal.
4. As the owner, I want `make check` to stay green through this step, so that I know nothing the C port already matches was lost.
5. As a porting agent, I want each post-fill PRNG draw labelled with its VT stage, so that I can port and claim the draws one stage at a time in step 4.
6. As a porting agent, I want the new stages listed in `stages.txt` in VT's draw order, so that `run_e2e.sh` accepts the regenerated traces and a claim can name each new stage.
7. As a porting agent, I want the `RandoStage` enum to list the new stages, so that `rando_gen` accepts a claim at any stage of the table.
8. As a porting agent, I want the `fy_shuffle` draws of `Location::getHint` counted as `applyHints` draws, so that the stage order stays monotonic inside an attempt.
9. As a porting agent, I want the Prize music `fy_shuffle` draws (made when `rom.mapOnPickup` is on) labelled as their own stage, so that the `mc`, `mcs` and `full` dungeon-item cases show where those draws come from.
10. As a porting agent, I want the `setDiggingGameRng` and `writeRNGBlock` draws labelled under those names, so that the write log and the trace name the same setter.
11. As a porting agent, I want one `rom_writes.txt` line per write, giving address, bytes and the VT method, so that I can find the first write that differs.
12. As a porting agent, I want writes tagged by the method `writeToRom` calls (the same backtrace rule `prng_stage` uses), so that step 5 can group writes into items, settings, credits, text, SRAM and the trailing item count.
13. As a porting agent, I want the large text, credits and SRAM writes explained by their setter calls, so that a difference in a 30 KB text write points to one key.
14. As a porting agent, I want the encoded default text table as a seed-independent fixture, so that step 9 can test the encoder and default table without any seed.
15. As a porting agent, I want the oracle to check by itself that its setter log rebuilds the text and SRAM writes, so that a missed setter is caught when the golden is made, not three steps later.
16. As a porting agent, I want the oracle to check that its tagged log equals VT's own `getWriteLog()`, so that no write escapes the golden.
17. As a porting agent, I want the heart beep written by VT's own `setHeartBeepSpeed` and logged, so that step 11 has a golden byte for each heart-beep value.
18. As a case author (step 3), I want an optional `hints` key (`on`/`off`) in `config.json`, so that a case can turn hints off.
19. As a case author (step 3), I want an optional `heart_beep` key (`off`, `half`, `quarter`, `double`, `normal`), so that a case can pick the heart-beep byte.
20. As a case author, I want both keys to default to VT's `on` and `half`, so that the 44 existing cases need no edit.
21. As a case author, I want the oracle and the C case reader to reject an unknown value of either key, so that a typo cannot silently fall back to the default.
22. As a case author, I want the C case reader to keep rejecting any other unknown key, so that the format stays strict.
23. As the net (`make check`), I want `gen_golden.sh` to stay out of `make test` and `make check`, so that the fast loop needs no PHP and no network.
24. As the net, I want two `gen_golden.sh` runs to give identical files, so that a golden is a fact, not one run's accident.
25. As the net, I want the regenerated goldens to differ from the old ones only where this step says they will, so that an unexpected change in VT's behaviour is caught before it is committed.
26. As the net, I want `spoiler.json` and `attempts` byte-identical across the regeneration, so that the fill the C port already matches is unchanged.
27. As the net, I want every claim held at `fill`, so that `e2e` stays green while the C generator lacks the post-fill draws.
28. As the net, I want a `fill` claim to compare every spoiler section and the attempt count, so that moving from `all` to `fill` gives up no comparison except the trace after the fill.
29. As the net, I want a `fill` claim to stop the C generator on the winning attempt, not on attempt 0, so that the attempts and final placement it compares are the right ones when a case retries.
30. As the net, I want the `pool` check due only to a claim at its own stage, so that a `fill` claim is not failed by the pre-fill pool the fill consumes.
31. As the net, I want a selftest where a synthetic placement divergence under a `fill` claim prints `DIVERGENT`, so that the `fill` checks are proven to catch a wrong fill.
32. As the net, I want selftests for the new `attempts` check (pass, differ, missing), so that its messages are fixed and its failures are not silent.
33. As the net, I want `e2e` no slower, so that the CLAUDE.md budget holds. A `fill` stop now does the same work an `all` run did.
34. As a reviewer, I want the step note to state `$save = false` and why, so that the missing seed-database writes are a known choice.
35. As a reviewer, I want every write between `prepSeed` and the downloaded file that the log lacks listed with one outcome, so that step 11 and Phase 2 know what to reproduce and what to ignore.
36. As a reviewer, I want each "left out" outcome to carry its reason, tied to §2 of the plan (no tournament, no Enemizer, no sharing, cosmetics at VT's default), so that it can be checked against the plan.
37. As a reviewer, I want the goldens in a commit of their own, so that the regeneration can be inspected and reverted alone.
38. As a future agent, I want the oracle to throw on any draw outside `randomize` with no label, as it does today, so that a new VT draw site cannot be mislabelled silently.
39. As a future agent, I want the oracle to throw on any ROM write it cannot tag, so that a new write site cannot enter the golden untagged.
40. As the owner, I want VT's source left unmodified except through `vt.patch`, so that VT stays frozen at `219fcafd` and the oracle's edits stay reviewable in one file.

## Implementation Decisions

### Oracle: the post-fill code and its labels

- `vt.patch` restores VT's `Randomizer::randomize` tail exactly (`setTexts` and `randomizeCredits` per world, then `HintService::applyHints`). Their draws are labelled by the existing backtrace rule: the function `randomize` calls directly, so `setTexts`, `randomizeCredits` and `applyHints`.
- `Location::getHint` is only called from inside `applyHints` (`HintService`, `LocationCollection::getHint`), so its `fy_shuffle` draws are `applyHints` draws. They get no stage of their own.
  - Reason: a separate stage would interleave with `applyHints` (`applyHints`, `getHint`, `applyHints`…). `run_e2e.sh` refuses an attempt-0 golden whose stages go backwards in the table.
  - This departs from the plan text, which places `getHint` among `writeToRom`'s draws; VT's source shows that it is not.
- The draws `writeToRom` makes run outside `randomize`. `vt.patch` gives each one an explicit trace label that is set right before the draw site and cleared right after, as the oracle already does for `crystals`:
  - `writeItem` for the Prize music `fy_shuffle` (only when `rom.mapOnPickup` is on);
  - `setDiggingGameRng` for its `get_random_int(1, 30)`;
  - `writeRNGBlock` for the closure's 1024 draws.

  Any other unlabelled draw outside `randomize` still throws, so a new draw site cannot be mislabelled.
- `stages.txt` gains, after `fill` and in VT's draw order: `setTexts`, `randomizeCredits`, `applyHints`, `writeItem`, `setDiggingGameRng`, `writeRNGBlock`.
- Each attempt's trace gains its post-fill draws, failed attempts included, whenever `randomize` returned. The `writeToRom` draws appear only once, numbered as the winning attempt's, after its other lines.
- The oracle keeps its retry loop. On the winning attempt only, after `isWinnable`, it runs `writeToRom`, then the heart beep, then `getSpoiler`.
  - VT's `prepSeed` calls `writeToRom` before `isWinnable`. The two orders give the same draws: `isWinnable` makes none, since today's oracle runs it unlabelled outside `randomize`, which would throw on any draw.
  - A losing attempt's `writeToRom` would be discarded anyway.
- `writeToRom` runs with `$save = false`. `true` saves a seed record to a database and writes a hash derived from that record. The oracle has no database, and the device has no seed sharing (§2).

### Oracle: the write log

- The oracle builds a sourceless `Rom` (`new Rom()`, with no file), as the plan's ground rule allows. VT's `Rom` deletes its own temporary file when it is destroyed.
- The logging lives in the oracle's own PHP, not in VT's source. It uses:
  - a subclass of `Rom` whose `write` records each logged write;
  - logging subclasses of `Text`, `Credits` and `InitialSram`, installed in that `Rom` after its constructor has run. The private `text` and `credits` properties are set by reflection; `initial_sram` is public.
- `cases/<case>/rom_writes.txt` (a new golden) has one line per `Rom::write` with `$log = true`, in call order: `<address> <bytes> <method>`.
  - `<address>` is 6 uppercase hex digits.
  - `<bytes>` is the written bytes as contiguous uppercase hex pairs.
  - `<method>` is the tag.

  The file ends with a newline. `updateChecksum` is never called, so it never appears. `applyPatch`'s unlogged writes are never made.
- The tag rule mirrors `prng_stage`. The anchor is the innermost `World::writeToRom` frame. The tag is the function of the first frame below it that is neither a `Collection` method nor a closure.

  | Write | Tag |
  |---|---|
  | A Rom setter called by `writeToRom` | the setter (`setGoalIcon`, `writeText`, `setTotalItemCount`…) |
  | An item write | `writeItem` |
  | Prize packs | `writePrizePacksToRom` |
  | Escape fills | `setEscapeFills` |
  | A direct `$rom->write` in `writeToRom`'s body | `writeToRom` |
  | The heart beep, made outside `writeToRom` | `setHeartBeepSpeed`, its own write label |

  A write with no anchor and no label throws.
- The heart beep: after `writeToRom`, the oracle calls VT's `Rom::setHeartBeepSpeed(<case heart_beep>)`. Its single write at `0x180033` is the last line of `rom_writes.txt`.
  - This follows the browser's order: the patch first, then `rom.js` `setHeartSpeed`, which has the same address and mapping.
  - `writeToRom` itself never writes `0x180033`.
- Self-check: after the run, the oracle's tagged list must equal VT's own `Rom::getWriteLog()`, entry for entry (address and bytes). Otherwise the case dies. This proves no write escaped the subclass.

### Oracle: the buffer setter log and the default text fixture

- `cases/<case>/rom_buffers.txt` (a new golden) has one line per buffer setter call, in call order, starting after the `Rom` constructor:
  - `text <id> <hex>`: after every `Text::setString` or `setStringRaw`. `<hex>` is the encoded bytes now stored for `<id>`, or `-` when they are empty. This covers `Rom::setText` and the Rom setters that call `$this->text->setString` directly.
  - `credit <scene> <line> <align> <hex>`: after every `Credits::updateCreditLine` call that VT applies. A call that returns `false` is not logged. `<hex>` is the line's text bytes as stored, after VT's 32-byte cut.
  - `sram <method> <changes>`: after every outermost public `InitialSram` setter call, so a setter calling another setter logs once. `<changes>` is `<hex index>=<hex value>` pairs for the bytes the call changed, comma-separated in index order, or `-` when nothing changed.
- `tests/rando/oracle/text_default.txt` (a new fixture, seed-independent) has one line per text id, in the table's order, as the `Rom` constructor leaves it (`Text::en()` then `removeUnwanted()`): `<id> <hex>`, using the same encoding as above.
- Self-checks that make a case die on a mismatch:
  - the default table with the case's `text` lines applied in order, concatenated in table order, must equal the bytes of the `writeText` write;
  - `InitialSram`'s constructor bytes with the case's `sram` changes applied must equal the bytes of the `writeInitialSram` write.
  - Credits get no self-check: their conversion is ported and proven in step 8.
- `gen_golden.sh` writes `text_default.txt` once per run, into the oracle directory or under `--out`, next to the other fixtures.

### Case format

- `config.json` may also hold `hints` (`on` | `off`) and `heart_beep` (`off` | `half` | `quarter` | `double` | `normal`). Both are optional, with the defaults `on` and `half` (VT's `spoil.Hints` request default and `rom.js`/VT's default heart speed). An unknown value, a non-string or a duplicate is an error. Every other unknown key is still an error.
- The oracle's case loader passes `hints` as `spoil.Hints` to `World::factory`, and `heart_beep` to `setHeartBeepSpeed`.
- The host C case reader accepts the same two keys and stores them in `RandoCase`:
  - a hints flag and a heart-beep enum declared in the host case header;
  - `RandoOptions` is unchanged. Whether hints becomes a twelfth option is a later step's call.

  `rando_case_parse_config` fills a `RandoCase`'s options, hints and heart beep, and leaves seed and attempts alone. Its callers are updated. The contract comment states the defaults.
- The 44 existing `config.json` files are not edited. They take the defaults.

### C generator: stops at or after the fill

- `RandoStage` gains the six new stages, in table order, after `RANDO_STAGE_FILL`. The stage-name table follows, so `test_generate`'s stage-table test passes. None of the six is ported, and the C generator never emits them.
- `rando_generate_until`'s contract:
  - For a stage before `fill`, it is unchanged: `RANDO_INCOMPLETE` right after that stage, on the first attempt that completes it.
  - For `fill` or any later stage, it runs attempts until one completes the fill and `isWinnable` holds. It then stops with `RANDO_INCOMPLETE` right after that stage on that winning attempt. Unported stages after the fill complete with no draws.
  - A losing attempt retries, as in a full run. `gen->attempt` is the winning attempt.
  - The header comment states this.
- `rando_gen`: on a stop at `fill` or later, it also writes `attempts` and the spoiler with its playthrough, since the winning attempt is known. `pool.txt` is still written on every `RANDO_INCOMPLETE`. The exit codes are unchanged.

### Harness: `run_e2e.sh` and `stages.txt`

- A new check kind, `check <stage> attempts`, with no argument. It compares the C output's `attempts` with the golden's. Its failure lines, after the claim's `DIVERGENT <case>` line, are:
  - `  <stage>: attempts:` then `    golden: <g>` and `    c: <c>`;
  - `    c wrote no attempts` when the file is missing.

  A malformed line is a table error (exit 2), as with the other kinds.
- The `pool` check is due only to a claim at the very stage it names, no longer to every later claim.
  - Reason: it compares the pool as `prepareWorld` leaves it. The C pools are not consumed by the fill, so after the fill `pool.txt` no longer describes an unplaced pool. The fill's section checks cover the placement.
  - No claim ever relied on the old rule: every claim was `all`, which runs no due check.
- `stages.txt` assigns to `fill`:
  - a `section` check for every spoiler section that no earlier stage checks: the 16 region sections (`Castle Tower`, `Dark Palace`, `Dark World`, `Death Mountain`, `Desert Palace`, `Eastern Palace`, `Ganons Tower`, `Hyrule Castle`, `Ice Palace`, `Light World`, `Misery Mire`, `Skull Woods`, `Swamp Palace`, `Thieves Town`, `Tower Of Hera`, `Turtle Rock`), plus `Equipped`, `Special`, `meta` and `playthrough`;
  - `check fill attempts`.

  With `Bosses` and `Shops` checked earlier, a `fill` claim compares all of `spoiler.json` and `attempts`. Only the trace after the fill is no longer compared. The plan asked for the location sections; the other sections are added so that leaving `all` loses no comparison.
- `claims.txt`: every `<case> all` becomes `<case> fill`. The header comment is unchanged, since it already describes stage claims.

### Step note (in the plan's §6, "Step 1.2")

The note gives:
- the PHP, composer and VT versions of the regeneration run;
- the reproducibility evidence: the two `--out` runs, and `diff -r` empty;
- the oracle's `$save` (`false`, and why);
- the inventory below. The implementer re-reads `prepSeed`, `World::writeToRom`, `resources/js/rom.js`, `romSettings.js`, `VTRomSettings.vue`, `VTSpriteSelect.vue` and the generate view's download flow to confirm it is complete. Any write found beyond it gets one outcome by the same rules.

| Write | Outcome |
|---|---|
| `setSeedString` (`0x7FC0`, 21 bytes) under `$save` | left out: its value is the alttpr.com seed-record hash; the device has no seed database and no sharing (§2) |
| `setStartScreenHash` under `$save` | left out, same reason |
| `prepSeed` Enemizer writes | left out: Enemizer is out of scope (§2) |
| `prepSeed` `setTournamentType` / `rummageTable` | left out: tournament mode is out of scope; the oracle's `tournament` is `false` |
| `rom.js` `save()` `setQuickswap` (`0x18004B` = `0x00`, quickswap default off, non-tournament) | reproduced in C (step 11) |
| `setMusicVolume` (build `2024-02-18` > `2019-08-01`: `0x18021A` = `0x00`, music on) | reproduced in C (step 11) |
| `setMSU1Resume` (default on: no write) | left out: writes nothing at VT's default |
| `setReduceFlashing` (`0x18017F` = `0x00`, default off) | reproduced in C (step 11) |
| palette shuffle and SFX shuffle (default off: no write) | left out: write nothing at VT's default; rejected by the user |
| load-time `setHeartSpeed` | added to the log (the oracle's `setHeartBeepSpeed` line) |
| load-time `setMenuSpeed` (`normal`: `0x180048` = `0x08`, `0x6DD9A` = `0x11`, `0x6DF2A` = `0x12`, `0x6E0E9` = `0x12`) | reproduced in C (step 11) |
| load-time `setHeartColor` (`red`; the branch depends on the base-patched ROM's byte `0x7FE2`) | reproduced in C (step 11); the note records both branches' bytes, and which one the base patch selects once Phase 0 delivers it |
| default sprite (`VTSpriteSelect` applies Link, built from the unpatched JP ROM: gfx at `0x80000`, palette at `0xDD308`, gloves at `0xDEDF5`, author line at `0x118002` and `0x118020` under its header guard) | reproduced in C (step 11), from the JP ROM the device holds as `base.sfc` |
| `updateChecksum` | excluded from the log by the plan; reproduced in C (step 11) |

## Implementation Chunks

1. **Case format keys.** Add the optional `hints` and `heart_beep` keys to the oracle's case loader and to the C case reader (`RandoCase` fields, defaults, value validation), with `test_case_reader` tests. No golden changes. `make check` is green.
2. **`run_e2e.sh`: the `attempts` check and the `pool` due rule.** Add the new check kind and its messages, and limit `pool` to claims at its own stage. Add synthetic selftests:
   - `attempts` passes, differs, and is missing;
   - a malformed `attempts` line;
   - a `pool` check not due to a later claim, and still due at its own stage.

   `stages.txt` and `claims.txt` are untouched.
3. **C stage table and the stop on the winning attempt.** Add the six stages to `stages.txt` (`stage` lines only) and to `RandoStage`. Apply the new `rando_generate_until` contract for `fill` and later stages, and have `rando_gen` write `attempts` and the playthrough on such a stop. Update `test_generate`:
   - add the stop-semantics tests listed under Testing Decisions;
   - generalise the baseline golden tests (below), so that they pass on today's goldens and on the regenerated ones.
4. **`fill` claims.** Add the `fill` section and `attempts` checks to `stages.txt`, and change every claim to `fill`. Add the real-table selftest: a synthetic case, under the committed `stages.txt`, with one region-section placement changed, prints `DIVERGENT` under a `fill` claim. `make -C tests/rando e2e` prints no `DIVERGENT` line on today's goldens.
5. **The oracle runs the post-fill code and logs.** Make the `vt.patch` changes (the three calls back on, the three write-site labels), and add the logging `Rom`, `Text`, `Credits` and `InitialSram`, the tag rule, the heart beep, the self-checks, `rom_buffers.txt`, `text_default.txt` and the `gen_golden.sh` wiring. Verify:
   - with two `gen_golden.sh --out tmp/<a|b>` runs over every case (`VT_DIR` under `tmp/`), `diff -r` is empty;
   - a `diff -r` against the committed tree shows only the expected changes listed under Further Notes.

   Commit the oracle only, no golden.
6. **Regenerate the goldens** with `gen_golden.sh` over every case, in their own commit: case goldens and oracle fixtures only. Then `make check` is green, and `make -C tests/rando e2e` prints no `DIVERGENT` line.
7. **Step note.** Write the §6 "Step 1.2" note in the plan file, with the content above.

## Testing Decisions

- A good test drives the public interface and checks observable output: `run_e2e.sh`'s stdout and exit status, `rando_gen`'s files, `rando_generate_until`'s status, `gen->attempt` and trace. It never inspects internals.
- **`selftest.sh`** (prior art: its `gate` and `fake` helpers, and the synthetic `alpha`/`beta`/`gamma` tables):
  - `attempts` check: pass; differ, with the exact message; C wrote no `attempts`; malformed table line, exit 2.
  - `pool` due rule: a claim after the `pool` check's stage passes with a `pool.txt` that would fail the check; a claim at that stage still fails on it.
  - Real-table proof (the plan's verification): with `RANDO_STAGES` left at the committed `stages.txt`, a synthetic case claimed at `fill` passes when the fake C output equals the golden. It prints `DIVERGENT` with the section's first differing line when one region-section entry is changed. It prints `DIVERGENT` on `attempts` when only the attempts differ.
- **`test_generate`** (prior art: `check_baseline_fill`, `check_retry_identity`, the `RANDO_FAULTS` hooks):
  - With `RANDO_FAULT_IS_WINNABLE` on the first two attempts, `rando_generate_until(…, RANDO_STAGE_FILL)` returns `RANDO_INCOMPLETE` with `gen->attempt == 2`. The same faults with `RANDO_STAGE_PREPARE_WORLD` stop at attempt 0.
  - `rando_generate_until(…, RANDO_STAGE_WRITE_RNG_BLOCK)` on baseline returns `RANDO_INCOMPLETE` at attempt 0, with the same 805 draws as the fill stop.
  - The stage-table test now covers sixteen stages.
  - Generalised golden tests. This is a deliberate change: the goldens gain post-fill draws on purpose.
    - `check_golden_stop` accepts a stop at the fill when the golden's next line, if any, belongs to a stage ranked after `fill`.
    - The retry-identity test compares the C retry against the golden's lines up to its last `fill` line.
    - The 805-line counts stay as they are, because they count C draws.
- **`test_case_reader`** (prior art: `check_config_accepted`, `check_config_rejected`, `check_config_refused`):
  - both keys absent gives the defaults;
  - each value of each key is accepted;
  - an unknown value, a non-string or a duplicate is refused, with a message naming the key;
  - an extra unknown key is still refused;
  - every committed case still reads.
- **The oracle** is not in the net (CLAUDE.md). It proves itself when goldens are made: by its self-checks (the tagged log equals `getWriteLog()`; the text and SRAM setter logs rebuild their writes; an untagged write or unlabelled draw throws) and by two runs giving identical files.
- `lint` covers the `rando/` changes (C99, `-pedantic-errors`).

## Out of Scope

- Porting any post-fill draw or ROM write into C (steps 4 and 6 to 11). The new stages exist in C only as names.
- New cases: hints off, heart-beep values, combinations (step 3).
- Write-log claims, comparing `rom_writes.txt` in `e2e`, write groups, and the `check-e2e` skill update (step 5).
- The qemu-arm run comparing write logs (from step 5).
- Checksum, heart beep and the "reproduced in C" writes of the step note (step 11).
- Making hints a twelfth `RandoOptions` setting.
- Any Enemizer, tournament, entrance or multiworld path, and `$save = true`.
- The `e2e` time budget measurement (steps 1 and 3 own it). This step adds no work to an `e2e` run beyond what an `all` claim did.

## Further Notes

- **Expected regeneration diff** (chunk 5's check, and chunk 6's content). Any other difference stops the step for investigation. It is not committed as is: a change in `spoiler.json` or `attempts` would mean the post-fill code alters the fill's outcome, for example `applyHints` throwing inside the attempt loop. That would change what the C port must do. The expected changes are:
  - every case's `trace.txt` gains lines after the fill (`setTexts`, `randomizeCredits`, `applyHints`; `writeItem` for the `mc`, `mcs` and `full` cases; `setDiggingGameRng`, `writeRNGBlock`);
  - every case gains `rom_writes.txt` and `rom_buffers.txt`;
  - the oracle gains `text_default.txt`;
  - every `spoiler.json` and `attempts`, every logic and pool fixture, and every `config.json` and `seed` stay byte-identical.
- **Size.** The text write is up to `0x7355` bytes, so each `rom_writes.txt` is about 70 KB, and the 44 cases add about 3 MB. This is accepted: the goldens are the source of truth. The setter log and default fixture let a reader avoid diffing the big line.
- **For the plan pilot.** The heart-beep line is the last line of `rom_writes.txt`, after `setTotalItemCount`. Steps 5 and 10 plan claims "up to setTotalItemCount" and "the whole write log", and step 11 ports the heart beep. So step 10's "whole log" includes a write that step 11 ports. Step 5's group list should end with a trailing `setHeartBeepSpeed` group, and step 10 should claim up to `setTotalItemCount`. This PRD does not change those steps; it flags the ordering.
- **For step 4.** VT draws the post-fill stages on every attempt that finishes `randomize`, losing ones included, and draws `writeToRom`'s stages only on the winning one. The C port must match both to claim a later stage on a retrying case.
- No `guideline` agent was consulted: every open point was settled from the plan, VT's source at `219fcafd`, and `alttp-ds` at `8d772553`, and is recorded above.
