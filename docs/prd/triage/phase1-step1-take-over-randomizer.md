---
title: "Take over the randomizer from alttp-ds"
description: "The repository has only a bootstrap skeleton; it needs the alttp-ds generator, oracle, goldens and checks, proven equivalent here on the host, under the oracle and on the Onion toolchain."
status: needs-triage
---

## Problem Statement

This repository (ALttPR on Miyoo) holds only a bootstrap skeleton: a version
module and its one unit test. The C port of VT, its oracle, its 44 golden
cases and the scripts that diff them already exist and are green in
`Canadadry/alttp-ds`, but there they live next to a Nintendo DS port, are
checked by DS tooling, and have never been built for the Miyoo Mini's ARM
Cortex-A7 nor run outside the DS repository. Every later step of plan 1
(write log, ROM writer, app) builds on this code, and every later budget is
measured against it. Until it is here, verified the same way here, and
measured, no later step can start.

## Solution

Copy the randomizer and its tests from `Canadadry/alttp-ds` (default branch,
commit `8d772553f7b9a387d35e072daa9926cc07f9b62f`) over the skeleton, wire
its checks into this repository's net, adjust the texts that still name the
DS port, and prove the take-over three ways:

- on the host: `make test` and `make check` green, `e2e` with 44 PASS and no
  `DIVERGENT`, and per-case times measured (the reference for every later
  ±10% budget);
- against the oracle: the unchanged VT oracle, run at the pinned VT commit,
  rewrites every golden and fixture byte for byte;
- on the target CPU: the generator cross-built with the toolchain Onion
  itself builds with, run under qemu-arm user mode, passes every claimed
  e2e case.

## User Stories

1. As the owner, I want the alttp-ds generator in this repository, so that the Miyoo app has a generator proven against VT from day one.
2. As the owner, I want the upstream commit named in the commit message, so that I can tell exactly which version was taken over.
3. As the owner, I want only the randomizer, its tests and two skills copied, so that no DS code (platform, vendor submodule, build script, DS tools) enters this repository.
4. As the owner, I want the bootstrap skeleton (version module and its test) gone, so that nothing in the tree is a placeholder.
5. As an agent, I want `make test` to stay the seconds-long fast loop, so that the TDD cycle stays fast.
6. As an agent, I want `make test`'s root recipe unchanged, so that the fast loop's definition in `CLAUDE.md` stays true.
7. As an agent, I want `make check` to run `logic`, `selftest` and `e2e` in addition to `test` and `lint`, so that a change that breaks a golden, the oracle fixtures or the diff tooling turns the net red.
8. As an agent, I want `make -k check` to report every failing target at once, so that I see all the failures of a change in one run.
9. As an agent, I want `lint` to keep building the whole library as strict ISO C99 with `-pedantic-errors`, so that no compiler extension slips into `rando/`.
10. As an agent, I want `lint` to cover the taken-over sources (including `regions/`), so that the C99 rule applies to the real generator, not to the skeleton.
11. As an agent, I want `make -C tests/rando e2e` to print one line per case and no `DIVERGENT`, so that I know the take-over changed no behavior.
12. As an agent, I want `make -C tests/rando measure` to work here, so that every later budget check has a reference.
13. As a later step's agent, I want the reference per-case times recorded with the commit that is the reference, so that I can re-measure that commit from a throwaway worktree and compare on the same machine.
14. As the owner, I want proof that the oracle, unchanged, reproduces the committed goldens and fixtures, so that a later golden regeneration that differs can only be due to a deliberate change.
15. As the owner, I want the PHP, composer and VT versions of that run recorded, so that a future mismatch can be traced to the setup.
16. As an agent, I want the oracle's VT checkout to stay git-ignored, so that running the oracle never dirties the tree.
17. As the owner, I want the generator cross-built with the toolchain Onion builds with, so that the code that will run on the Miyoo is the code that was tested.
18. As the owner, I want that toolchain identified from Onion's own sources, with links, so that the choice is checkable and reused by Phase 3 step 1 and Phase 4.
19. As the owner, I want the toolchain pinned by an immutable reference, so that a toolchain update upstream cannot silently change a build.
20. As an agent, I want a make target that cross-builds the generator and runs the e2e cases under qemu-arm, so that I can check the ARM build without a console.
21. As an agent, I want that target outside `test`, `e2e` and `check`, so that the fast loop and the net never need docker or qemu.
22. As an agent, I want that target to say plainly when docker or qemu-arm is missing, so that a missing environment is recognized as a setup problem and never sent to fix-bug.
23. As an agent, I want that target to reuse the same e2e driver, claims and cases as the host `e2e`, so that from step 5 it compares write logs too with no change of its own.
24. As an agent, I want that target to print what it ran (the binary's architecture and the qemu version), so that a pass cannot come from accidentally running the host binary.
25. As a later step's agent, I want the toolchain runner to be one reusable piece, so that Phase 2's host tests under qemu and Phase 4's app build use the same pinned toolchain.
26. As an agent, I want `CLAUDE.md` § Checks to list `logic`, `selftest`, `e2e` and the cross target, so that the instructions match the Makefiles.
27. As an agent, I want the `check-e2e` skill to describe this repository's flow (it is run as a plan `skill:` step; per-PRD checking is `check-net`'s job), so that I do not call it in the wrong place.
28. As an agent, I want the `rework-comments` skill scoped to this repository's directories, so that it never looks for `platform/nds/` or `vendor/zelda3`.
29. As the owner, I want no Nintendo ROM needed by any of this, so that the repository stays legal and CI stays ROM-free.
30. As the owner, I want CI's existing `make -k check` job to run the new net targets with no extra setup, so that the manual CI run stays meaningful.

## Implementation Decisions

- **Source of the copy**: `Canadadry/alttp-ds` default branch at `8d772553f7b9a387d35e072daa9926cc07f9b62f` (a local shallow clone exists at `/home/user/canadadry/alttp-ds`; re-clone if missing). The commit message of the take-over names that commit.
- **What is copied, verbatim**: the library's public headers, sources (including the regions subdirectory and the private rules header) and host-side code (case reader, spoiler JSON, playthrough, output, and the generator's `main`); under the randomizer tests: every unit test and the shared test support, all 44 cases, the oracle (PHP scripts, the VT patch, the logic, pool and PRNG fixtures, `gen_golden.sh`), the scripts (`run_e2e.sh`, `selftest.sh`, `cases.sh`, `measure.sh`), the measure program and its shim, `claims.txt` and `stages.txt`; and the skills `check-e2e` and `rework-comments` with their support files. The measure program's sources are not named in the plan's list but `make measure` cannot run without them, so they come too.
- **What is not copied**: `platform/`, `vendor/`, `.gitmodules`, `build.py`, `tools/`, the DS root `Makefile`, the DS `.github/` workflow, DS images and icons, and every other skill (this repository has its own, newer, versions). This repository's pre-push hook replaces `tools/branch_guard.sh`.
- **Skeleton removed**: the version header, its source and its unit test are deleted, and no longer registered.
- **Randomizer tests Makefile**: alttp-ds's version replaces the skeleton's, plus the skeleton's `lint` target added back: every library source compiled with the same flags as the unit build (which include `-DRANDO_FAULTS`) plus `-pedantic-errors`, into its own object directory, printing how many sources it built. Checked in exploration: all taken-over library sources pass it, with and without `-DRANDO_FAULTS`. Its `test` target stays alttp-ds's (13 unit tests plus the discovery self-test).
- **Root Makefile**: new delegating targets `logic`, `selftest` and `e2e`; `check` becomes `test lint logic selftest e2e`; `test` is unchanged; help lines for each new target. The cross target (below) is also a root target, not part of `check`.
- **`.gitignore`**: adds the oracle's default VT checkout directory under the randomizer tests' oracle; nothing else (in particular `*.bps` is not ignored: `data/base.bps` is to be committed later).
- **`measure.sh` unchanged**: it still reports DS-era figures (an optional devkitARM build, a `ds.time.projected` line, absolute budgets marked "over"). Its per-case `host-O2.<case>.time` lines are what this plan uses; retargeting its ARM estimate to the Cortex-A7 is Phase 4 step 3's job. Keeping it byte-identical keeps the reference measurement comparable. Observed in exploration: 44 cases complete, median per case 4–5 ms, max about 18 ms (item_placement-basic), whole run about 17 s; it exits 0 whatever the figures.
- **The reference commit**: the code take-over commit (chunk 1) is "step 1's commit" for every later ±10% budget. Its message records the `e2e` summary line and, for each case, the `host-O2.<case>.time` figure from `make -C tests/rando measure`, plus the machine's `nproc` and CPU model. Later chunks of this PRD do not change the library, the generator or the e2e driver.
- **Oracle reproduction**: `gen_golden.sh --out tmp/<dir>` over every case, with `VT_DIR` pointing at a throwaway VT checkout under `tmp/` (a clone at the pinned commit is at `tmp/vt`; the script resets, cleans, patches and composer-installs it itself, so use a copy or let it own `tmp/vt`). Then a recursive comparison: every case directory against the committed case, and every fixture file it writes against the committed oracle fixture (`prng_vectors.txt` is not written by the script, only checked by `prng_check.php`). Identical → the versions line the script prints (`php …, Composer version …, vt …`) and "identical, N cases, M fixtures" go in that chunk's commit message. A difference is a setup problem: resolve it (PHP extension, composer lock), never edit a golden; if it cannot be resolved, report `QUESTION:` before the step ends. Observed in exploration: PHP 8.3.6, Composer 2.8.12, VT `219fcafd…`; the run on `baseline` alone took about 5 minutes (the logic and pool fixtures are regenerated over all cases each time) and its case files and all ten fixtures were identical.
- **Onion toolchain, from Onion's sources** (`OnionUI/Onion`, commit `07505ea58c7bba698d6b9220ff43946a43cac76b`, 2026-01-21): the root `Makefile` sets `TOOLCHAIN := aemiii91/miyoomini-toolchain:latest` and builds through `docker run … $(TOOLCHAIN)` (`with-toolchain`); `.github/workflows/build.yml` (and the release workflows) run in the container `aemiii91/miyoomini-toolchain:latest`, sourcing `/root/.bashrc` for `CROSS_COMPILE`; `src/common/config.mk` adds, for `PLATFORM=miyoomini`, `-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve`. The image's `CROSS_COMPILE` is `/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-`, a GCC 8.3.0 (Arm A-profile 8.3-2019.03) with glibc 2.28.
- **Pinning**: the image is pinned by digest `aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132` (what `:latest` resolved to on 2026-11-01; image created 2026-01-20), never by tag.
- **The toolchain record**: a new document under `docs/` records the toolchain item exactly as Phase 3 step 1 will cite its items: each fact with a link to the Onion source file at the commit above, plus the digest and compiler version. Phase 3 step 1 adds its other items to the same document.
- **The toolchain runner**: one script at the repository root level (shared by the randomizer tests now and the app build later) that runs a given command inside the pinned image with the repository mounted at a fixed path and the toolchain's `CROSS_COMPILE` exported. It is the only place that names the digest. It pulls the image when absent. It runs as root in the container (the image's environment lives in `/root`). When the docker client is missing or the daemon does not answer, it exits non-zero with one line starting `setup:` naming what is missing.
- **The cross target** (`e2e-arm`, at the root and in the randomizer tests' Makefile): through the runner, builds the generator from the same sources and flags as the host `e2e` generator (`-std=c99 -Wall -Wextra -Werror -DRANDO_FAULTS -O2`) plus Onion's miyoomini flags above and `-static`, into the randomizer tests' build directory; checks with `file` that it is a 32-bit ARM EABI executable and prints that line and `qemu-arm --version`; then runs the e2e driver with `RANDO_GEN` pointing at a generated wrapper that execs `qemu-arm <binary> "$@"` (the driver runs `RANDO_GEN` as one word, so a wrapper is required). Its pass/fail and output are the driver's. Missing `qemu-arm` → the same `setup:` failure. Static linking avoids extracting the image's sysroot for `qemu-arm -L`; both were checked in exploration and pass all 44 cases (about 2 s under qemu).
- **Docker in this container**: the daemon is not running at session start; `dockerd` started in the background works (it pulled the image through the proxy in about 35 s). That is a setup step, not part of the target. No fallback to Ubuntu's `arm-linux-gnueabihf-gcc`: it is not Onion's toolchain (newer GCC and glibc), and the plan asks for Onion's.
- **Not in CI yet**: the cross target is not added to the workflow in this step; Phase 4 step 2 adds the cross-build CI job. CI's existing `make -k check` job runs the new net targets as is (they need only gcc, make, sh and awk).
- **`CLAUDE.md`**: § Checks lists `make check` = `test` + `lint` + `logic` (unit tests replaying the oracle fixtures) + `selftest` (the e2e driver's own self-test) + `e2e` (every case diffed against its golden, one `DIVERGENT <case>` line per failure); names `make -C tests/rando measure` (outside the net, the budget reference) and `make e2e-arm` (outside the net: needs docker and qemu-user; a missing one is a setup problem). § Layout adds `rando/host/`, `tests/rando/cases/` and `tests/rando/oracle/`. Nothing else in it changes.
- **Skill texts**: `check-e2e`'s description and opening say it is run by plan-pilot for a `skill: check-e2e` step (here `check-net` runs after each PRD, and `e2e` is part of `make check`); the rest of its loop is kept. `rework-comments`' scope becomes `rando/` and `tests/` (and any directory later steps add), its `cloc` command matches, and its mentions of `platform/nds/` and `vendor/zelda3` go; its anecdotes about "the reference project" stay (they are not DS-specific instructions). A final grep of the copied files and `CLAUDE.md` for `nds`, `devkit`, `platform/`, `vendor/zelda3`, `branch_guard`, `build.py`, `ARM9` and `alttp-ds` finds only provenance mentions and `measure.sh`'s DS figures (decided above).
- **README**: the Development section mentions `make e2e-arm` and what it needs; nothing else.

## Implementation Chunks

1. **Code and net take-over** (the reference commit). Copy the library, host code and randomizer tests (with the measure program), remove the skeleton, merge the randomizer tests' Makefile with the `lint` target, extend the root Makefile and `check`, `.gitignore`, `CLAUDE.md` § Checks and § Layout. Done when `make test`, `make -k check` and `make -C tests/rando e2e` (44 PASS, no `DIVERGENT`) are green, and `make -C tests/rando measure` has run. The commit message names the upstream commit and carries the `e2e` line and the per-case times.
2. **Skills and oracle reproduction.** Copy `check-e2e` and `rework-comments`, adjust their texts, run the DS-name grep. Run the oracle over every case into `tmp/` and compare (no repository change from that part). The commit message carries the versions line and the comparison result. Done when the comparison is identical and `make test` is green.
3. **Onion toolchain and the cross target.** The toolchain document, the toolchain runner, `e2e-arm` in both Makefiles, the `CLAUDE.md` and README lines. Done when `make e2e-arm` prints an ARM executable line, the qemu version and `e2e: 44 pass, 0 divergent, 0 pending`; when it fails with a `setup:` line with docker stopped or `qemu-arm` off `PATH`; and `make check` still does not need docker.

## Testing Decisions

- A good test here checks external behavior: the generator's output against VT's goldens, the oracle's output against the committed files, the ARM binary's output against the same goldens. No test inspects how the code was copied.
- No new unit test is written for the taken-over code: it arrives with its 13 unit tests, the `logic` fixture replay, the driver `selftest` and the 44 e2e cases, which together are the proof. They must pass unchanged; none is edited, skipped or loosened. The skeleton's version test is deleted with the module it tested (a deliberate change, stated in the commit).
- `lint` is the test of the pure-C99 rule on the new sources.
- The cross target is verified by running it (all claimed cases pass under qemu) and by its two setup failure paths (docker unavailable, qemu-arm missing), checked by hand once in chunk 3. Its architecture check guards against a false pass on the host binary.
- The oracle reproduction is a one-off verification, deliberately not a target: the oracle stays outside the net (`CLAUDE.md`).
- Prior art: alttp-ds's own `tests/rando` suites (`run_e2e.sh` with `RANDO_GEN`, `selftest.sh`), and this repository's `make check` and `test-template`.

## Out of Scope

- Any change to the generator's behavior, the goldens, `claims.txt`, `stages.txt`, the oracle or `vt.patch` (step 2 onward).
- The write log, the post-fill draws, `rando_rom`, `e2e-combos` (steps 2 to 5).
- Retargeting `measure.sh`'s ARM estimate to the Cortex-A7 (Phase 4 step 3).
- The other Onion facts (app layout, SDL, resolution, buttons, save paths) — Phase 3 step 1; only the toolchain item is done here.
- Phase 2's host tests under qemu, the app's cross build and packaging, and any CI job for the cross target (Phase 4).
- Turning on CI's push and pull-request triggers.
- Anything about alttp-ds itself or its DS port.

## Further Notes

- Facts checked during exploration on this machine: alttp-ds at `8d772553` gives `make -C tests/rando test` green (13 unit tests and the discovery self-test), `logic` (6 fixture replays) and `selftest` green, `e2e` 44 pass in about 4 s; the Onion toolchain cross-build with `-static` passes all 44 cases under qemu-arm 8.2.2.
- The step note (written by plan-pilot) needs, from the commit messages: the reference per-case times and machine, the oracle versions and comparison result, and the toolchain digest and compiler version.
- When later steps re-measure the reference, they check out the chunk 1 commit in a throwaway worktree under `tmp/` and run `make -C tests/rando measure` there and in the current tree in the same session.
- The cross target's driver is the host `e2e` driver, so step 5's write-log claims reach it automatically; steps 12, the end of Phase 2 and Phase 4 step 2 rerun it.
