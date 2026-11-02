---
title: "Miyoo build and release zip (App/ALttPR/)"
description: "The owner has an app that builds for the PC and cross-links for ARM, but nothing they can copy to the Mini Flip's SD card: no stripped device binary, no launch.sh or config.json, no icon, no zip, and the committed base patch is checked only by hand."
status: needs-triage
---

## Problem Statement

The owner wants to copy one folder to their Miyoo Mini Flip's SD card and see
"ALttPR" in Onion's Apps list. Today they cannot:

- `make sdl-arm` cross-links the SDL app in Onion's toolchain, but only to
  prove it links; the binary is not stripped, not packaged, not checked as a
  hard-float executable, and not uploaded anywhere.
- There is no `launch.sh`, no `config.json` and no icon, so Onion would not
  even list the app (an `App/` folder counts only with a `config.json`,
  `docs/onion.md` §2).
- `data/base.bps` is now committed (commit `e13553b`, built by the owner), but
  it is checked only when someone types `make patch-header`; nothing in the
  fast loop or the net reads the real file, so a broken or replaced patch
  would ship silently. The plan's gate for this step asks for a host test that
  reads its header and proves the pipeline feeds it a source of the declared
  size (1,048,576 bytes) and expects the declared target size (2,097,152).
- CI cannot produce the release: there is no job that builds and uploads it.

## Solution

One command, `make package`, builds the device binary in Onion's pinned
toolchain container (reusing `make sdl-arm`), strips it, proves it is an ARM
32-bit EABI hard-float executable, re-checks the base patch, and writes a
reproducible release zip whose only top-level content is `App/ALttPR/`:

```
App/ALttPR/alttpr                       the SDL app, stripped, dynamically linked to Onion's SDL 1.2
App/ALttPR/launch.sh                    cd to its own folder, start alttpr, keep its stderr in a log
App/ALttPR/config.json                  label, icon, launch, description
App/ALttPR/icon.png                     the Apps-list icon
App/ALttPR/base.bps                     data/base.bps, byte for byte
App/ALttPR/DejaVuSansMono.ttf           data/'s font, byte for byte
App/ALttPR/DejaVuSansMono-LICENSE.txt   its licence, byte for byte
```

The owner unzips it at the SD card's root, puts their JP 1.0 ROM next to it as
`App/ALttPR/base.sfc` (Phase 5 step 2's README will say so), and starts the
app. The same zip comes out of a manual CI job, `package`, as the workflow
artifact `alttpr-miyoo`; that job also runs the existing `make e2e-arm`
(the cross-built generator under qemu-arm on every e2e case, traces and write
logs against the goldens).

The gate becomes part of `make test`: `test_patch_header`'s normal run reads
the committed `data/base.bps`, checks its header (sizes, source CRC32 of the
JP 1.0 ROM, patch CRC32), that it fits the app's patch read cap, and runs the
real assembly pipeline on it with a synthetic 1 MiB source to show the
pipeline hands the patch a source of exactly the declared size and a target
buffer of exactly the declared size.

## User Stories

1. As the owner, I want one `make package` command, so that I get the release zip without knowing the toolchain's details.
2. As the owner, I want the zip to contain exactly one top-level folder, `App/ALttPR/`, so that unzipping it at the SD card's root puts the app where Onion looks.
3. As the owner, I want the app to show up in Onion's Apps list as "ALttPR", so that I can start it like any other Onion app.
4. As the owner, I want an icon in the Apps list, so that the app does not look broken next to Onion's own apps.
5. As the owner, I want the icon path in `config.json` to follow a form Onion's shipped apps use, so that it has the best chance to show before device check 6 confirms it.
6. As the owner, I want `launch.sh` to change to its own folder before starting the binary, so that the app finds its font and the SD root whatever directory Onion starts it from.
7. As the owner, I want the app's error output kept in a file inside `App/ALttPR/`, so that I can read why it failed to start on a device that has no visible console.
8. As the owner, I want `base.bps` shipped inside `App/ALttPR/`, so that the app can patch my ROM with no extra download.
9. As the owner, I want the font and its licence shipped next to the binary, so that the app renders text and the licence travels with the font as it requires.
10. As the owner, I want the zip not to contain any Nintendo ROM, so that the release stays legal to share.
11. As the owner, I want the zip not to contain a `history/` folder, a `settings.json` or a `base.sfc`, so that a fresh install never overwrites my seeds' history, my last settings or my ROM when I unzip an update over an old install.
12. As the owner, I want the binary stripped, so that the app takes little room on the SD card.
13. As the owner, I want `make package` to fail if the binary is not a 32-bit ARM EABI hard-float executable, so that a wrong-ABI build never reaches my device.
14. As the owner, I want `make package` to fail if `data/base.bps` does not declare the JP 1.0 → 2 MiB sizes and the JP 1.0 source CRC32, so that a wrong patch is never shipped.
15. As the owner, I want `make package` to stop with a `setup:` line when docker is missing, so that I can tell an environment problem from a bug.
16. As the owner, I want two `make package` runs on the same commit to give the same zip bytes, so that I can compare releases and trust that nothing random slipped in.
17. As the owner, I want the zip's `alttpr` and `launch.sh` entries marked executable, so that an unzip on a system that keeps permissions does not lose them.
18. As the owner, I want a manual CI job that builds the zip and uploads it as an artifact, so that I can download a release without building locally.
19. As the owner, I want that CI job to also run the cross-built generator under qemu-arm on the e2e cases, so that the release's ARM code is proven against VT's goldens in the same run.
20. As the owner, I want the CI job to stay manual (`workflow_dispatch`) like the others, so that I decide when CI turns on.
21. As the owner, I want `make test` to read the committed `data/base.bps`, so that a damaged or wrong patch turns the fast loop red immediately.
22. As the owner, I want the gate test to prove the pipeline feeds the patch a source of exactly 1,048,576 bytes and a target of exactly 2,097,152 bytes, so that the device never applies the patch to a resized ROM.
23. As the owner, I want the gate test to check that `data/base.bps` fits the app's patch read cap, so that the device's reader never refuses the patch I ship.
24. As the owner, I want `make patch-header BPS=<file>` to keep working on any BPS file, with the same checks as the gate, so that I can check a rebuilt patch before committing it.
25. As an agent, I want the packaging script to have its own self-test in `make test`, so that a layout regression shows without docker.
26. As an agent, I want the self-test to run `launch.sh` on the host against a stub binary, so that the folder change and log redirection are proven without the device.
27. As an agent, I want the self-test to validate `config.json` (valid JSON, required keys, launch file and icon file present in the package), so that a typo never hides the app on the device.
28. As an agent, I want the package step to compare the zip's entries against one exact expected list, so that an extra or missing file fails the build.
29. As an agent, I want `base.bps`, the font and the licence in the zip compared byte for byte against `data/`, so that the shipped copies are the committed ones.
30. As an agent, I want `make package` to reuse `make sdl-arm` and `onion-toolchain.sh` instead of a second cross build, so that there is one place that knows how to cross-build the app.
31. As an agent, I want the shipped binary built with the same flags as the binaries `e2e-arm` and `unit-arm` run under qemu, so that what was verified is what ships.
32. As the owner, I want CLAUDE.md, `docs/toolchain.md` and `notes/owner-todo.md` to describe `make package` and the new CI job, so that the next session and I know how to produce and check the release.
33. As the owner, I want my to-do list to drop the "build the base patch" item and gain "run the `package` CI job and report", so that what is left for me is accurate.

## Implementation Decisions

- **Gate test: the real patch joins `make test`.** `test_patch_header`'s normal run (no argument) additionally reads the committed base patch from the repository's `data/` (path relative to `tests/rando`, like the other tests' relative reads; `unit-arm` runs it from the same directory under qemu-arm) and checks:
  - the header declares source 1,048,576 and target 2,097,152 (`RANDO_ROM_SOURCE_SIZE`, `RANDO_ROM_SIZE`) through `rando_assemble_check_patch`, the function the pipeline itself calls;
  - the patch's own CRC32 and header parse (`rando_bps_read_header`);
  - the source CRC32 equals the JP 1.0 ROM's, `3322EFFC` — a test-local constant whose public source (the No-Intro entry for *Zelda no Densetsu - Kamigami no Triforce (Japan)*, the MD5 `03a63945…` ROM) the implementer cites in the commit message; not added to the library, since the BPS applier already checks the source CRC32 against the patch;
  - the file size is at most `RANDO_OUT_PATCH_MAX`, the cap the app's reader uses;
  - **the pipeline feeds it a source of the declared size:** `rando_assemble` runs on the real patch with a synthetic 1,048,576-byte source and injected digests (the source MD5 set to the synthetic source's, as the existing assemble tests do). It must stop at `RANDO_ASSEMBLE_PATCH` with `RANDO_BPS_SOURCE_CRC` — past the source-size check, past the declared-sizes check, inside the applier at the CRC comparison, which comes after the applier's own source-size check. A synthetic source one byte longer must stop at `RANDO_ASSEMBLE_SOURCE_SIZE`. The target buffer is `RandoAssembleWork.rom`, sized `RANDO_ROM_SIZE`, which the declared-sizes check already ties to the header.
  - A missing `data/base.bps` fails the test with a message naming the file and the plan's gate (no skip: the file is committed now).
- **`--file` mode** (`make patch-header BPS=<file>`) gains the same source-CRC32 and size-cap checks, so the owner's tool and the gate agree. `make patch-header` stays outside the net; CLAUDE.md's line "outside it until `data/base.bps` exists" becomes "checks any `BPS=<file>`; `make test` checks `data/base.bps`".
- **Package files** live in a new folder `app/package/`: `launch.sh`, `config.json`, `icon.png`, and the zip builder. Nothing there is C, so `lint`, `no_sdl.sh` and `no_literals.py` are untouched.
  - `launch.sh`: POSIX `sh`, LF line endings, `cd "$(dirname "$0")"` (Onion's packaged scripts do this even though Onion already `cd`s, `docs/onion.md` §2), then `exec ./alttpr 2>alttpr.log`. No `--sd` and no `--font`: the binary already takes the SD root from `rando_app_sd_root` of its working directory (two components up, else `/mnt/SDCARD`, consequence C2) and opens `DejaVuSansMono.ttf` from its working directory by default. No `LD_LIBRARY_PATH` change: Onion's `runtime.sh` already puts `/mnt/SDCARD/miyoo/lib` (its SDL 1.2) on it.
  - `config.json`: `{"label": "ALttPR", "icon": "/mnt/SDCARD/App/ALttPR/icon.png", "launch": "launch.sh", "description": "A Link to the Past Randomizer seeds"}` — consequence C1 of `docs/onion.md`: the absolute icon path is the shipped form, until device check 6 says whether a path inside the app folder shows.
  - `icon.png`: a committed 64×64 RGBA PNG, a simple Triforce drawn by a throwaway stdlib-Python script in `tmp/` (only the PNG is committed). Onion's sources do not state an app icon size; 64×64 is an assumption the owner can replace with nicer art without code changes, and device check 6 covers it.
  - The zip builder is a stdlib-only Python 3 script (no `zip` binary: the checks need only gcc, make and python3). Inputs: the binary, the three package files, and `data/`'s `base.bps`, font and licence; output: one zip. It writes entries in a fixed sorted order, with a fixed timestamp (1980-01-01, the zip epoch), deflate, Unix mode 0755 for `alttpr` and `launch.sh` and 0644 for the rest, explicit directory entries `App/` and `App/ALttPR/`. It refuses (non-zero exit, one line naming the file) a missing input, and after writing reads the zip back and checks the entry list against its one expected list and every copied file's bytes against its source. Run twice on the same inputs it gives identical bytes.
- **`make package`** (root target delegating to `tests/rando`, next to `sdl-arm`; outside `test`, `e2e` and `check`, like every docker target):
  1. runs `sdl-arm` (the existing cross build and link in the pinned toolchain container; no second build recipe);
  2. strips a copy of the binary with the toolchain's `${CROSS_COMPILE}strip` through `onion-toolchain.sh`;
  3. checks the stripped binary: `file` reports `ELF 32-bit LSB executable, ARM, EABI5` and `dynamically linked` with the interpreter `/lib/ld-linux-armhf.so.3` (the armhf loader; `file` 5.45 prints no "hard-float" word), and `readelf -h` reports `hard-float ABI` in the ELF flags (host binutils, already present wherever gcc is); `readelf -d` lists `libSDL-1.2.so.0` and `libSDL_ttf-2.0.so.0` as NEEDED. Any miss fails with the `file`/`readelf` line;
  4. runs `test_patch_header --file` on `data/base.bps` (the gate, again, at packaging time);
  5. builds the zip into the build directory under a fixed name (`ALttPR.zip` in a `package/` subfolder of `tests/rando/build`) and prints its path, its size and the `file` line.
  A missing docker prints `onion-toolchain.sh`'s existing `setup:` line.
- **Shipped flags**: the binary is `sdl-arm`'s, built with the same `CFLAGS` (test fault hooks `-DRANDO_FAULTS` included, inert unless a test arms them) plus `-O2` and Onion's miyoomini flags as `e2e-arm` and `unit-arm`, so the qemu-verified code and the shipped code share flags. Dropping `-DRANDO_FAULTS` from the release would create a configuration no check runs; not done.
- **The qemu-arm target is the existing `make e2e-arm`**, unchanged (cross-built generator, static, under qemu-arm, traces, attempts, spoiler and whole write logs against the goldens through the e2e claims). No new target duplicates it.
- **CI**: a new manual job `package` in the existing workflow (still `workflow_dispatch` only): checkout, `apt-get install qemu-user`, `make package`, `make e2e-arm`, upload the zip as artifact `alttpr-miyoo` (GitHub wraps it in its own download zip). The `sdl` job is unchanged.
- **Docs**: CLAUDE.md § Checks gains `make package` (what it builds, docker needed, outside the net, the CI job) and the corrected `patch-header` line; § Layout mentions `app/package/`. `docs/toolchain.md` "How this repository uses it" gains a `make package` bullet (strip, ABI checks, zip). `notes/owner-todo.md`: item 1 (build and commit the base patch) is marked done except the owner's MD5 confirmation if not yet given; a new item asks the owner to run the `package` CI job and report, and to install the zip for the §4 hand checks.
- **Not in the zip**: `base.sfc` (the user's ROM), `history/`, `settings.json` (both created or written by the app at run time, step 3.5), any README (Phase 5 step 2).

## Implementation Chunks

1. **Gate on the real patch.** Extend `test_patch_header` (normal run reads `data/base.bps`: sizes, source CRC32 `3322EFFC`, patch CRC, size cap, the pipeline-feeds-a-declared-size-source checks; `--file` gains the CRC and cap checks). CLAUDE.md's `patch-header` line. Verified by `make test` green, `make patch-header BPS=data/base.bps` green, and each new check seen failing once on a planted bad file (a copy with a changed source CRC32 in a temporary directory via `--file`).
2. **Package files and zip builder.** `app/package/launch.sh`, `config.json`, `icon.png`, the stdlib zip builder, and its self-test wired into `make test` (fake inputs in a temporary directory: exact entry list, byte-identical copies, modes, two runs identical, missing input refused; `launch.sh` run on the host from another directory against a stub `alttpr` that records its working directory and writes to stderr, checking the folder change and the log file; `config.json` parsed, keys present, launch file and icon basename in the package). Verified by `make test` green.
3. **`make package`, CI job and docs.** The root and `tests/rando` targets (reuse `sdl-arm`, strip, ABI checks, gate, zip), the `package` CI job, CLAUDE.md, `docs/toolchain.md`, `notes/owner-todo.md`. Verified locally (docker and qemu-user are available): `make package` produces the zip and prints the armhf `file` line and `hard-float ABI`; a second `make package` gives the same zip bytes (`cmp`); `unzip -l` (or Python's `zipfile`) shows exactly the seven files; `make e2e-arm` reports every case passing under qemu-arm; `make -k check` green. The step note asks the owner to run the `package` job.

## Testing Decisions

- Good tests here check what the owner gets, not how it is built: the zip's entry list and bytes, the binary's ABI as `file` and `readelf` report it, the patch's header as the pipeline reads it, and the pipeline's status on the real patch. No test inspects the Makefile's recipe text.
- **`test_patch_header`** (C, `make test`, `unit-arm`): prior art is its own synthetic-header tests and `test_assemble`'s injected-digest runs. The real-patch checks are external behaviour of `rando_assemble_check_patch` and `rando_assemble` on the committed file; no Nintendo ROM is used (the source is synthetic, which is why the pipeline is expected to stop at the source CRC32).
- **Zip builder self-test** (Python `unittest`, stdlib only, run with `python3 -I` from `make test` like `no_literals.py`): prior art `tests/rando/no_literals.py`'s planted-input self-proof and `.claude/hooks/tests`. It plants a missing input and an extra file to prove the checks bite. It must stay a fraction of a second; the clean `make test` time is measured before and after, numbers in the step note.
- **`launch.sh`** is tested on the host with a stub binary (prior art: `smoke_sdl.sh`'s temporary SD tree). The real device start is the owner's (§4).
- **The ARM binary** is checked by `make package` (ABI, NEEDED libraries) and, as code, by `make e2e-arm` and `make unit-arm` (same flags). The shipped SDL binary itself is not run under qemu: it needs the toolchain's sysroot libraries outside the container; recorded as a limit.
- No golden, snapshot or expected value is edited by this step.

## Out of Scope

- The README and install instructions (Phase 5 step 2).
- Performance on the device and Phase 4 step 3's numbers (done).
- Running the shipped SDL binary under qemu-arm, or on the device (the owner's §4 checks).
- Turning on CI's push and pull-request triggers (the owner's decision).
- A nicer icon, a `full_resolution` file, Onion icon-pack integration (`iconsel`).
- A version number in the zip name or the app; release tagging.
- Bundling SDL libraries (Onion ships SDL 1.2, step 4.1's decision).
- Removing `-DRANDO_FAULTS` from the release build.

## Further Notes

- The plan's gate is met by the session's facts: `data/base.bps` is committed (`e13553b`), and `make patch-header BPS=data/base.bps` prints source 1,048,576, target 2,097,152, source CRC32 `3322EFFC`. The missing half was the host test reading the committed file in the net; chunk 1 adds it.
- The owner's confirmation that the patch applied to their JP ROM gives MD5 `edc01f3d…` (Phase 0, §4) is still theirs to give; the agent cannot check it without the ROM. The app itself checks it at every generation (`RANDO_ASSEMBLE_BASE_MD5`).
- Step 1.11's note left one check for when `data/base.bps` exists (the heart-colour branch on `0x7FE2` and the author guard): it needs the patched image, so it still needs the ROM; not this step.
- `make sdl-arm` was run in this session to read the binary's markers: `file` prints `ELF 32-bit LSB executable, ARM, EABI5 version 1 (SYSV), dynamically linked, interpreter /lib/ld-linux-armhf.so.3, for GNU/Linux 3.2.0, with debug_info, not stripped`; `readelf -h` flags `0x5000400, Version5 EABI, hard-float ABI`; `readelf -A` `Tag_ABI_VFP_args: VFP registers`; NEEDED `libSDL-1.2.so.0`, `libSDL_ttf-2.0.so.0`, `libc.so.6`.
- Assumptions for the device checks: the icon shows with an absolute path (device check 6); the app gets 640×480 (device check 2).
