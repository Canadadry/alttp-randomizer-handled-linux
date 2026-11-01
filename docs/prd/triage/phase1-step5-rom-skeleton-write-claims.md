---
title: "rando_rom skeleton and claims by write group"
description: "Nothing in the C port writes a ROM byte, and the e2e net cannot compare a write log against the golden's rom_writes.txt, so steps 6 to 11 have no module to port the writes into and no claim to prove them by."
status: needs-triage
---

## The step (Plan 1, Phase 1, step 5, copied from `docs/plan-1.md`)

> **`rando_rom` skeleton and claims by write group.** The `rando_rom` module writes through a caller-supplied write callback (the host logs writes, the app writes into a buffer), with no allocation. `claims.txt` gains write-log claims by setter group, the way it claims trace stages: a case claims the log prefix up to a group. The groups follow `writeToRom`'s call order, where texts, credits and SRAM are only buffered until `writeCredits`, `writeText` and `writeInitialSram` at the end: items and locations, settings, credits, text, initial SRAM, the trailing `setTotalItemCount`, then the heart beep (`setHeartBeepSpeed`, written by the oracle after `writeToRom`). `e2e` prints one `DIVERGENT <case>` line per case whose claimed prefix differs. Verified by the `selftest` target covering a synthetic divergence in a claimed group, and a pass on an unclaimed one. The same step updates `skill: check-e2e` for the write logs and `e2e-combos`: it ranks a write-log failure by the earliest differing write group, and gives the first differing `rom_writes.txt` line (address, bytes, Rom method) as the bug's reproduction; verified by running it on that synthetic divergence.

Step 1.1's text adds one duty here: the `e2e-arm` target "compares write logs too from step 5, when the write-log claims start".

## Facts at `421d3ac` (checked)

- 73 goldens hold a `rom_writes.txt`: 50 in `cases/`, 23 in `combos/`. Each line is `%06X <HEX> <tag>` (`gen_case.php`), newline-terminated, with no empty write (`oracle_rom.php` throws on one). Lines run from 411 to 617 per case. The longest line (the text block) holds 15,199 bytes, so about 30 KB of hex: longer than `support.h`'s `FIXTURE_LINE_MAX` (8,192).
- The tag is the function `World::writeToRom` called (closures and `Collection` frames skipped), `writeToRom` itself for a write from its body (`0x45C42`, Byrna's magic), or the oracle's label (`setHeartBeepSpeed`, written after `writeToRom`).
- Mapping each tag to the plan's seven groups (`writeItem` → items; `writeCredits` → credits; `writeText` → text; `writeInitialSram` → initial SRAM; `setTotalItemCount`; `setHeartBeepSpeed` → heart beep; every other tag → settings), **all 73 logs run through the groups in that order, and every group is non-empty in every log**:
  - items: 315 lines in every case (`writeItem` only);
  - settings: 90 to 296 lines. Besides the `Rom` setters, it holds `setEscapeFills` (the `World` method, standard state only), the bare `writeToRom` write, `setDiggingGameRng`, `writeRNGBlock`, `writePrizePacksToRom`, `setMapRevealSahasrahla`/`setMapRevealBombShop` (map-on-pickup) and `enableTriforceTurnIn`;
  - credits: 2 lines; text, initial SRAM, `setTotalItemCount`, heart beep: 1 line each, in that order, at the end.
- VT's `writeToRom` (`tmp/vt`, `app/World.php` from line 1065) confirms that order: `setText`/`setCredit` only buffer, the item loop comes first, then the setters, and `writeCredits`, `writeText`, `writeInitialSram` and `setTotalItemCount` close it. The `override_patch` branch (entrance shuffle) is out of scope (plan §2).
- `run_e2e.sh` reads `stages.txt` and a claims file. A claim is `<case> <stage>` or `<case> all`: an `all` claim runs the generator with no stage and compares `attempts`, the whole trace and `spoiler.json`. A claims-file or table error exits 2 before any case runs; the detail lines of a divergence follow its `DIVERGENT` line. `e2e-combos` runs the same script with `RANDO_CASES` and `RANDO_CLAIMS` pointed at `combos/` and `combo_claims.txt`; `e2e-arm` runs it with `RANDO_GEN` pointed at a qemu-arm wrapper over the cross build, on `cases/` only.
- `rando_gen <case_dir> <out_dir> [<stage>]` writes `trace.txt`, `attempts` (on a win), `spoiler.json` and `pool.txt` (on a stop). It writes no ROM log.
- `RandoHeartBeep` (off, half, quarter, double, normal) lives in the host header `rando_case.h`; the case reader fills `RandoCase.heart_beep`, default half.
- `check-e2e` runs only `make -C tests/rando e2e`, ranks failures by the earliest differing trace draw, and knows no write log.

## Step 4's outcome (hypotheses)

Step 4 ("Port the post-fill draws") is not done. Its PRD is `docs/prd/triage/phase1-step4-post-fill-draws.md`. Every decision below that depends on what step 4 leaves behind cites one of these hypotheses. Before chunk 1, the implementer checks each one against the tree and records in the step note whether it holds. A hypothesis that does not hold is handled by the rule given with it.

- **H-4-1**: every claim of `claims.txt` and `combo_claims.txt` is `<case> all`, and `make check` is green. If some case is still at a stage, it simply gets no write-log claim (none is added in this step anyway); nothing here waits on it.
- **H-4-2**: `RandoGen` holds the post-fill choices by value (`RandoPostFill`, `gen->post`), next to the world, the options and the config, so one `const RandoGen *` carries everything `writeToRom` reads. If step 4 put them elsewhere, `rando_rom_write` takes whatever const struct holds them, by pointer, beside the generator: the rest of this PRD is unchanged.
- **H-4-3**: `rando_generate` (and `rando_generate_until` with `RANDO_STAGE_NONE`) returns `RANDO_COMPLETE` after the last post-fill stage, with the world as VT's `writeToRom` reads it (prize-pack drops assigned). The ROM writer never draws, so it takes no PRNG.
- **H-4-4**: hints are `RandoOptions.hints`, `RandoCase.hints` is gone, and `RandoHeartBeep` is still in `rando_case.h`. If step 4 moved the heart beep elsewhere in `rando/include/`, the move decided below is already done.
- **H-4-5**: every golden and fixture is byte-identical to `421d3ac`'s, except a new seed-independent oracle fixture (`hint_tables.txt`). `stages.txt`'s stages, `run_e2e.sh`'s claim semantics and `rando_gen`'s arguments are unchanged.
- **H-4-6**: no `rando_rom` module, write callback or `rom_writes.txt` writer exists in the C port (step 4's out of scope).
- **H-4-7**: the C post-fill result holds what VT computes before writing (the alternate-bow location, the drops), so steps 6 to 11 can fill the skeleton's group functions without changing its interface. Not checked here: a missing field is added by the step that needs it.

## Problem Statement

The owner will play ROMs that the console patches. A ROM is right only if every byte VT writes for that seed is written, at the same address, by the C port. Today:
- the C port has no ROM writer at all, so steps 6 to 11 have nowhere to put the writes they port;
- the goldens have held each case's write log (`rom_writes.txt`) since step 2, but the net never reads it, so a port of the item writes, say, could be wrong and `make check` would stay green;
- a write log has about 400 to 600 writes. Claiming it only whole would leave every step from 6 to 10 unclaimable until step 11, the way the trace would be unclaimable without stage claims;
- the agent that fixes a write-log divergence (`check-e2e`) does not know write logs exist, nor `e2e-combos`.

## Solution

- A new `rando_rom` module in the randomizer library writes a generated seed's ROM writes through a callback the caller supplies, group by group in `writeToRom`'s order, allocating nothing. In this step every group is an empty function: the skeleton writes nothing, and steps 6 to 11 fill one group each. The library also gives a ready buffer writer (the app's case: write into a 2 MiB ROM buffer, refusing out-of-bounds writes); the host gives a log writer that prints the oracle's `rom_writes.txt` format.
- `rando_gen` writes `rom_writes.txt` on every complete run (empty in this step).
- A new table, `tests/rando/write_groups.txt`, names the seven write groups in order and the tags each one owns.
- A claim gains an optional third field: `<case> all <group>` claims the golden's write log up to the last write of that group. `run_e2e.sh` compares it after the existing `all` checks and prints `DIVERGENT <case>` with the first differing write, its group and both lines. The last group claims the whole log, extra writes included.
- `selftest` proves both halves: a divergence inside a claimed group is `DIVERGENT`, one in a later, unclaimed group passes.
- `check-e2e` runs `e2e` and `e2e-combos`, ranks write-log failures by their earliest differing group, and hands `fix-bug` the first differing `rom_writes.txt` line as the reproduction.
- `e2e-arm` compares write logs through the same claims, with no change of its own needed; the step runs it once and records the result.

No real case gains a write-log claim in this step: the skeleton writes nothing, so no group would pass. Step 6 adds the first ones.

## User Stories

1. As the owner, I want every ROM byte the console writes to be checked against VT's write log, so that a seed I play is the seed VT would have patched.
2. As the owner, I want the ROM writer to run unchanged on the Miyoo and on the host, so that what the host tests prove is what the console does.
3. As the owner, I want the ROM writer to allocate nothing, so that it runs within the console's fixed memory like the rest of the generator.
4. As the app (Phase 2), I want to hand the ROM writer a callback that writes into my ROM buffer, so that I patch the ROM in place with no copy.
5. As the app, I want a ready buffer writer that refuses a write outside the buffer, so that a wrong address fails the generation instead of corrupting memory.
6. As the app, I want the ROM writer to stop at the first write my callback refuses and report it, so that I can show a write failure instead of a half-patched ROM.
7. As the host generator, I want a callback that logs each write in the oracle's `rom_writes.txt` format, so that the e2e net diffs the C log against the golden line by line.
8. As the host generator, I want `rando_gen` to write `rom_writes.txt` on every complete run, so that every `all` case has a log to compare.
9. As the host generator, I want each write to carry the VT method name the oracle tags it with, so that the C log's lines equal the golden's, tag included.
10. As a porting agent (steps 6 to 11), I want one empty function per write group, called in `writeToRom`'s order, so that I port one group per step without touching the others.
11. As a porting agent, I want the heart beep to be an input of the ROM writer, so that step 11 writes the heart-beep group from the case's value without changing the interface.
12. As a porting agent, I want the ROM writer to read the generator's state as const and take no PRNG, so that no write can draw or change the seed.
13. As a porting agent, I want to claim a case's write log up to a group, so that step 6 proves the items group before the settings exist.
14. As a porting agent, I want a claim on a group to compare every earlier group too, so that a later port cannot break an earlier group unnoticed.
15. As a porting agent, I want writes after the claimed group ignored, so that a half-ported later group does not fail an earlier claim.
16. As a porting agent, I want the last group's claim to compare the whole log, extra C writes included, so that the final claim proves the ROM gets exactly VT's writes.
17. As a porting agent, I want the divergence detail to give the write's number, its group and both lines (address, bytes, method), so that I see at once which setter wrote what.
18. As a porting agent, I want a C log shorter than the claim reported as such, with the number of writes needed, so that a missing write is not mistaken for a wrong one.
19. As a porting agent, I want a C log with no `rom_writes.txt` reported as such, so that a crash before the writer is told apart from a wrong write.
20. As a porting agent, I want the trace, attempts and spoiler checked before the write log, so that a write-log divergence is never reported when the seed itself already differs.
21. As a porting agent, I want write-log claims in `combo_claims.txt` too, so that the combinations guard the writes as they guard the trace.
22. As the net, I want the group table to be a file the harness reads, as `stages.txt` is, so that the claim names and the tag mapping are data, checked once, and not buried in awk.
23. As the net, I want every claimed golden's write log checked against the group order at every run, so that a wrong table fails `make check` before any claim relies on it.
24. As the net, I want a claim on an unknown group, on a group the golden has no write in, or on a group beside a stage claim, refused with exit 2 and a file and line, so that a bad claim is a setup error, never a silent pass.
25. As the net, I want every malformed table line, duplicate group, tag owned by two groups, and a table with no default group or two, refused with exit 2, so that the mapping of every tag is unambiguous.
26. As the net, I want `selftest` to cover a synthetic divergence in a claimed group, and a pass on a divergence in an unclaimed one, so that the write-log gate is proven without a real ROM port.
27. As the net, I want `selftest` to run the committed `write_groups.txt` on a synthetic log, so that the real table, not only a test table, is proven to split groups where VT does.
28. As the net, I want `make test` unchanged in time and `e2e` slower only by the write-log comparison, so that the fast loop stays fast.
29. As the net, I want every golden and fixture untouched, so that the source of truth does not move.
30. As the net, I want `rando/` to stay pure C99 with no `malloc`, built by `lint`, so that the new module obeys the same rules as the generator.
31. As the cross build (`e2e-arm`), I want the write logs compared through the same claims under qemu-arm, so that the ARM build is held to the same writes as the host build from step 6 on.
32. As `check-e2e`, I want to run `e2e` and `e2e-combos`, so that a combination's divergence is handled by the same loop.
33. As `check-e2e`, I want a seed-level failure (trace, attempts, spoiler) ranked before any write-log failure, so that I never debug a write whose input already differs.
34. As `check-e2e`, I want write-log failures ranked by the earliest differing group, then the earliest differing write, so that I fix the group every later one depends on first.
35. As `check-e2e`, I want the bug report's reproduction to be the first differing `rom_writes.txt` line, golden and C, with the command that replays the case, so that `fix-bug` starts from the exact write.
36. As `check-e2e`, I want the fix's red test to be a `test_rom_*` unit test on a hand-built world, never a golden read whole, so that the dev loop catches the next such bug in milliseconds.
37. As a reviewer, I want the step note to record which step-4 hypotheses held, the group table's check on all 73 goldens, the `check-e2e` dry run and the `e2e-arm` result, so that I can check the step without rerunning it.

## Implementation Decisions

### The `rando_rom` module (new public header in `rando/include/`, source in `rando/src/`)

- **The writer.** A caller-owned struct holding a callback and its context:
  - the callback takes the context, the PC address, a pointer to the bytes, their count (never 0) and the method tag, and returns `bool`: false refuses the write;
  - the method tag is a static string the module owns: the oracle's tag for that write (`writeItem`, `setGameState`, `writeToRom`, `setHeartBeepSpeed`, …). Its lifetime is the program's; the callback must not free it.
- **The entry point.** `rando_rom_write(const RandoGen *gen, RandoHeartBeep heart_beep, const RandoRomWriter *writer)` returns `bool`: true when every write was accepted; false at the first refused write, after which it writes nothing more. It never draws and never changes the generator's state (H-4-2, H-4-3). Its precondition: `gen` is the result of a `RANDO_COMPLETE` generation.
- **The groups.** The source holds one static function per group, called in this order: items, settings, credits, text, initial SRAM, `setTotalItemCount`, heart beep. In this step each is empty, so `rando_rom_write` makes no call to the callback and returns true. Every write a later step adds goes through one internal emit helper, so the refusal rule holds for every group. The header's contract names this order and states that each group's writes come in VT's call order.
- **No C group enum.** The group order lives in the call sequence in C and in `write_groups.txt` for the harness. Nothing in C would read an enum, so none is added (unlike `RandoStage`, which the trace labels draws with).
- **The heart beep moves.** `RandoHeartBeep` moves from the host's `rando_case.h` to the new public header, unchanged (same names, same order); `rando_case.h` includes it. Step 11 writes the heart-beep group from it; the skeleton only accepts it.
- **The buffer writer.** The header also gives `RandoRomBuffer` (a caller-owned byte pointer and size) and a callback for it: it copies the bytes when `address + count <= size`, and refuses otherwise, copying nothing. The app (Phase 2) passes a 2 MiB buffer.
- No `malloc`, no `stdio`, no platform header: `lint` builds the new source with `-pedantic-errors`.

### The host log writer (`rando/host/`, beside the trace file)

- A log writer in `rando_out`, shaped like `RandoTraceFile`: open `<out_dir>/rom_writes.txt`, a `RandoRomWriter` callback whose context is the open file, close (false when any write or the close failed). Each write is one line: the address as `%06X`, a space, each byte as two uppercase hex digits, a space, the tag, `\n`. That is `gen_case.php`'s format exactly; a zero-byte write is refused (the oracle never logs one).
- The bytes are printed one at a time, so no line buffer limits a write's size (the text write is about 15 KB).

### `rando_gen`

- On `RANDO_COMPLETE` only, after `spoiler.json`, `rando_gen` opens the log writer, calls `rando_rom_write` with the case's heart beep, and closes the log. A refused write or a failed close prints `rando_gen: <path>: write failed` and exits `EXIT_INPUT`, as the other output files do.
- A stop at a stage writes no `rom_writes.txt`: the write log exists only for a complete generation, as in the oracle.
- In this step every complete run writes an empty `rom_writes.txt`.

### The write-group table (`tests/rando/write_groups.txt`, new)

- Comment lines as in `stages.txt`, then one line per group, in order: `group <name> <tag>...`, or `group <name> *` for the one default group, which owns every tag no other line names. The committed table:

  ```
  group items writeItem
  group settings *
  group credits writeCredits
  group text writeText
  group sram writeInitialSram
  group setTotalItemCount setTotalItemCount
  group heart_beep setHeartBeepSpeed
  ```

  The names are the plan's own words: step 10 claims "the `setTotalItemCount` group", step 11 "the heart-beep group" (`heart_beep`, as the case key).
- Rules, each a table error (exit 2, `<file>:<line>: <reason>`): a line that is not `group` with a name and at least one tag; a duplicate group; a tag named twice; `*` beside another tag; no default group, or two.
- `RANDO_GROUPS` overrides its path, as `RANDO_STAGES` does for `stages.txt`.

### Write-log claims (`run_e2e.sh`, `claims.txt`, `combo_claims.txt`)

- **Syntax.** A claim line has two or three fields: `<case> <stage>`, `<case> all`, or `<case> all <group>`. Each claims file's header comment gains the third form, in one sentence.
- **Claim errors** (exit 2, before any case runs, with `<claims file>:<line>:` as today): four or more fields (`malformed line`); an unknown group (`unknown write group: <g>`); a group beside a stage claim (`write group <g> needs an all claim`: the write log exists only for a complete run); a group with no write in the case's golden (`<g> makes no write in <case>`); a claimed group whose golden has no `rom_writes.txt` (`<case>: cannot read <path>`).
- **Golden check.** For every claimed case, with or without a write claim, whose golden has a `rom_writes.txt`, each line's tag is mapped to its group and the groups must be in table order; otherwise `<case>: golden write group <g> out of table order` and exit 2. This runs the committed table against every claimed golden on every `e2e` and `e2e-combos` run, so a wrong table fails `make check` in this step already, before any write claim exists. Its cost is reading about 50 files of 600 lines: negligible against the generation.
- **Comparison**, for an `all <group>` claim, after the existing `attempts`, trace and `spoiler.json` checks pass (a failure there is reported, and the write log is not compared):
  - N is the number of the golden's last line whose group is the claimed one or earlier.
  - The C log's first N lines must equal the golden's first N lines, with the same line endings, as the trace prefix is compared.
  - For the last group of the table, the C log must have exactly N lines too: an extra write changes the ROM, so it is a divergence. For any other group, lines after N are ignored, as a longer C trace is.
- **Detail lines**, after `DIVERGENT <case>`, in the style of the existing ones:
  - `  c wrote no rom_writes.txt`;
  - `  rom_writes.txt: c ends after <k> writes, claim needs <N>`;
  - `  rom_writes.txt: <group>: first differing write <n>:` then `    golden: <line>` and `    c: <line>`, where the group is the golden line's (or, for an extra C write, the claimed last group) and an absent line reads `<end>`;
  - `  rom_writes.txt: line endings differ`.
- **PASS line.** A write-claimed case prints `PASS <case> (all <group>)`; every other output line is unchanged.
- The generator is still run with no stage for an `all` claim, write group or not.

### `check-e2e` (`.claude/skills/check-e2e/SKILL.md`)

- **Step 1** runs `make -C tests/rando e2e` and `make -C tests/rando e2e-combos`; green means both are.
- **Step 2, ranking**, one pool of `DIVERGENT` lines from both runs:
  1. `baseline`, if red;
  2. a failure in the trace, `attempts` or `spoiler.json` (the seed itself), earliest differing draw first (attempt, then draw), as today;
  3. a write-log failure, earliest differing group first in `write_groups.txt` order, then the lowest write number; a short or missing log ranks at the group of the first missing write;
  4. on a tie, `e2e` before `e2e-combos`, then case order.
- **Step 3, bug report.** For a write-log failure, the reproduction is the first differing `rom_writes.txt` line, golden and C (address, bytes, Rom method), with its group, verbatim from the detail, and the replay command: `tests/rando/run_e2e.sh <case>` for `cases/`, or `RANDO_CASES=tests/rando/combos RANDO_CLAIMS=tests/rando/combo_claims.txt tests/rando/run_e2e.sh <case>` for `combos/`. The constraint list gains: the RED test is a `test_rom_*` unit test on a hand-built world through a recording writer, never a whole golden log; never a new case in `cases/` or `combos/`.
- **Step 4, commit gate**: point 4 runs both `e2e` and `e2e-combos`.
- Nothing else in the skill changes: the attempt count, the gate and the outputs stay as they are.
- **Its verification** is a dry run, recorded in the step note: a synthetic case set under `tmp/` (selftest's fake generator pattern), with one write-claimed case diverging in `settings` and one in `text`, plus one seed-level divergence, run through `run_e2e.sh` with `RANDO_CASES`, `RANDO_CLAIMS` and `RANDO_GEN`. The skill's steps 1 to 3 are applied to that output, stopping before `fix-bug` is invoked: it must pick the seed-level failure first and, once that one is removed, the `settings` one over the `text` one, and its bug report must quote the first differing line. The note keeps both bug reports.

### `e2e-arm`

- It needs no change of its own: it runs `run_e2e.sh` with `RANDO_GEN` pointing at the cross build, so a write-log claim in `claims.txt` is compared under qemu-arm exactly as on the host. The cross build picks up the new library and host sources through the existing wildcards.
- `docs/toolchain.md`'s `e2e-arm` paragraph says that its comparison includes the write-group claims.
- The step runs `make e2e-arm` once, so that the new sources are proven to cross-build and `rando_gen` writes its (empty) log under qemu-arm. Without docker or `qemu-user` (a `setup:` line), the step note records it as a setup problem and the step goes on.

### Step note

The plan's §6 gains a "Step 1.5" note: which H-4 hypotheses held; the group table and its golden check passing on all 73 goldens; the `check-e2e` dry run's two bug reports; the `e2e-arm` result or its `setup:` line. No `measure` run: step 5's text sets no budget, and the skeleton adds no work to a generation.

## Implementation Chunks

1. **The `rando_rom` skeleton and its writers.**
   - The new public header and source: the writer struct, `rando_rom_write` with its seven empty group functions and the emit helper, the buffer writer, and `RandoHeartBeep` moved in from `rando_case.h`.
   - The host log writer in `rando_out`, and `rando_gen` writing `rom_writes.txt` on a complete run.
   - `test_rom`, registered in `UNIT_TESTS`.
   - `make check` stays green: no claim moves, and the harness does not read the new file yet.
2. **Write groups in the harness.**
   - `write_groups.txt`, the `RANDO_GROUPS` override, the claim syntax and errors, the golden check, the comparison and its detail lines in `run_e2e.sh`, and the claims files' header comments.
   - `selftest`'s new gate cases (see Testing Decisions).
   - `make check` green, with the golden check now running the committed table over every claimed case of both files.
3. **`check-e2e`, `e2e-arm` and the step note.**
   - The skill's changes, its dry run under `tmp/`, the `docs/toolchain.md` sentence, the one `make e2e-arm` run, and §6 "Step 1.5".

## Testing Decisions

- A good test drives a public interface and checks what comes out: the bytes in a buffer, the lines of a log, `run_e2e.sh`'s stdout, stderr and exit status. No test inspects a static group function.
- **`test_rom`** (new, in `UNIT_TESTS`; not in `LOGIC_TESTS`, it has no fixture). Prior art: `test_generate` for driving a whole generation on `baseline`'s options; `test_spoiler_json` for checking host output text.
  - `rando_rom_write` on a completed `baseline` generation, through a recording writer, makes no callback call and returns true, and leaves the generator's bytes unchanged (compared against a copy taken before). Step 6 replaces the "no call" expectation, deliberately.
  - The buffer writer: a write inside a small buffer lands at its address and touches nothing else; a write ending exactly at the end is accepted; one past the end, or at an address beyond it, is refused and changes nothing.
  - The log writer, on a `tmpfile()`: a one-byte write, a multi-byte write and a write longer than `FIXTURE_LINE_MAX` give the exact `%06X <HEX> <tag>\n` lines, uppercase; a zero-byte write is refused.
  - The refusal rule (no write after a refused one) has nothing to call in this step. Its test lands with the first group that writes (step 6, which the header's contract binds).
- **`selftest`**, new gate cases, each with a synthetic golden `rom_writes.txt` and a fake C one, built on the existing `gate` helper and a small test table (two or three groups):
  - **the plan's two:** a divergence inside the claimed group is `DIVERGENT`, with the group, the write number and both lines; a divergence in a later, unclaimed group passes;
  - a divergence in an earlier group under a later claim names the earlier group;
  - a short C log, a missing C log, a last-group claim with one extra C write, and a log without its last newline;
  - a seed-level failure (spoiler) under a write claim is reported alone, the log not compared;
  - claim errors: four fields, an unknown group, a group on a stage claim, a group with no write in the golden, a write claim on a golden with no `rom_writes.txt`;
  - table errors: a malformed line, a duplicate group, a tag in two groups, `*` with another tag, no default, two defaults; a golden out of table order, refused even when the case has no write claim;
  - a write claim still runs the generator with no stage argument (the generator-arguments check);
  - **the committed table**, as the "fill claim, real table" cases do for `stages.txt`: a synthetic log with one write per committed group, where a `settings` claim passes with a changed `writeText` line and is `DIVERGENT` with a changed `setGameState` line.
- `selftest discovery` (in `make test`) is unchanged.
- **`e2e` and `e2e-combos`** must stay green: every claimed golden passes the golden check with the committed table. No real write claim is added.
- **`lint`** covers the new library source.
- `make test` stays fast: `test_rom` runs one generation, as `test_generate` already does many.

## Out of Scope

- Any ROM write: items and locations (step 6), settings and flags (step 7), credits (step 8), text, dialog and hints (step 9), initial SRAM and `setTotalItemCount` (step 10), the checksum and the heart-beep write (step 11). Each group function stays empty.
- Any real write-log claim in `claims.txt` or `combo_claims.txt`: none would pass yet. Step 6 adds the first.
- `rom_buffers.txt` and `text_default.txt`: steps 8 to 10 use them; nothing in this step reads them.
- The writes outside the log (step 2's note: quickswap, music volume, menu speed, heart color, default sprite, checksum), reproduced in C in step 11 and applied by Phase 2.
- The BPS applier, the ROM assembly and the `.sfc` file (Phase 2).
- Running `check-e2e` over the whole case list (step 12). Adding `e2e-combos` to `e2e-arm` (step 1.1 and Phase 4 run it on the e2e cases only).
- Any change to the goldens, the oracle, `stages.txt` or the trace-claim semantics.

## Further Notes

- **Why a table file, not more awk.** The tag-to-group mapping is data that steps 6 to 11 may need to read (for instance a test that a ported group's writes carry its tags). `stages.txt` set the pattern: names and order in a file, the harness and the tests reading it.
- **Why groups by tag, with a default.** The settings group holds about 60 different tags, some only under one mode (`setEscapeFills`, `setMapRevealSahasrahla`, `enableTriforceTurnIn`). Naming them all would make the table fail on the next mode a combination exposes. The six other groups are each one tag, and the golden check proves, on every run, that the default never swallows a line out of order.
- **Why the last group compares the whole log.** Phase 2 applies exactly the logged writes. An extra C write that VT does not make is as wrong as a missing one, and only the final claim can see it.
- **Why the write log needs an `all` claim.** The log is written only by a complete generation, in the oracle and in `rando_gen`. A write claim beside a stage claim could never be compared, so it is refused rather than passed silently.
- **Hand-over to step 6.** Step 6 adds the first write: the refusal-rule test, the "no call" expectation of `test_rom` rewritten, and its claims (`<case> all items`). A golden line can be longer than `FIXTURE_LINE_MAX`: a `test_rom_*` test that ever reads one needs its own reader, but `check-e2e`'s rule already keeps those tests on hand-built worlds.
- **No `guideline` agent was consulted.** Every point was settled from the plan, VT at `219fcafd` (`tmp/vt`), the code and the 73 goldens at `421d3ac`, steps 1.1 to 1.3's notes and step 4's PRD, and each decision is recorded above.
