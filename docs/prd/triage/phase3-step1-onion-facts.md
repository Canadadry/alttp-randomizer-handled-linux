---
title: "Onion facts for the app"
description: "Phase 3 builds an Onion app whose folder, screen, buttons, ROM list and save deletion all depend on how OnionOS and its RetroArch really behave, and none of that is recorded or checked against the plan's SD-card paths yet."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 1, copied from `docs/plan-1.md`)

> 1. **Confirm the Onion specifics from Onion's sources** (`OnionUI/Onion`) and record them in `docs/` with links:
>    - the app folder layout (`App/<Name>/config.json`, `launch.sh`, icon);
>    - the toolchain (the Miyoo Mini cross toolchain Onion builds with), and the SDL version and video setup Onion apps use;
>    - the screen resolution on the Mini Flip;
>    - the button mapping;
>    - the exact RetroArch save and save-state paths for the default SNES core (e.g. under `Saves/CurrentProfile/`).
>
>    Verified by: each item cites the Onion source file, and each §2 path and layout (`App/ALttPR/`, `Roms/SFC/ALttPR/`, the `.srm` and save-state locations) is checked against these facts; a mismatch stops the step and asks the user before steps 4 and 6.

Context the step relies on, from the plan:

- §2 *Files on the SD card*: the base ROM at `App/ALttPR/base.sfc`; generated ROMs at `Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc`, "a subfolder of Onion's SNES list"; history records at `App/ALttPR/history/<code>.json`.
- §2 *History tab*: delete removes the ROM, "its RetroArch `.srm` save and its RetroArch save states"; a ROM deleted by hand disappears from History at the next start.
- §2 *Locked decisions*: the user plays the ROM "from Onion's SNES list with Onion's own SNES core"; the user checked that an alttpr.com ROM boots, has sound and keeps its save with Onion's default SNES core.
- §2 *UI*: two tabs, in the style of Onion's menus; the UI core is a pure C state machine drawn by an SDL renderer (Miyoo and PC) and a text renderer.
- Phase 1 step 1 did the toolchain item already (step 1.1 note): `docs/toolchain.md`, titled "Onion platform facts", cites `OnionUI/Onion` commit `07505ea58c7bba698d6b9220ff43946a43cac76b` (2026-01-21) for the docker image, Onion's CI, the miyoomini `CFLAGS`, the image digest and its gcc 8.3.0 / glibc 2.28. It is referenced from `CLAUDE.md`, `README.md`, `onion-toolchain.sh` and `tests/rando/Makefile`.
- Consumers of these facts: Phase 3 step 2 (the screen model's grid and its relation to the pixel resolution), step 4 (the Generate tab's keys), step 5 (where the ROM is written and how it shows up in Onion's list), step 6 (which files a delete removes), Phase 4 steps 1–2 (SDL renderer, key mapping, packaging).

## Hypotheses about Phase 2 step 3 (not done when this PRD was written)

Phase 2 step 3 (output naming and history record, PRD `docs/prd/triage/phase2-step3-naming-history-record.md`) is not done. This step is fact-finding and mostly checks §2's text, so it rests on only two assumptions about that step's outcome:

- **H-2.3-1** (one place for the SD paths): step 2.3 spells the two SD-card directories once, as public constants of its save module (`Roms/SFC/ALttPR` and `App/ALttPR/history`, relative to the SD root), as its PRD decides "so Phase 3 step 1 checks them against Onion's facts in one place". *If it holds*, chunk 2's table names those constants next to §2's text. *If it fails* (spelled in several places, or not at all), chunk 2 checks §2's text and lists every place the committed code spells either path (a `git grep` at HEAD), each checked the same way.
- **H-2.3-2** (the ROM name's characters): a generated ROM is named `<Mode>-<Goal>-<code>.sfc` or `<Mode>-<Goal>-<code>-<n>.sfc`, with only ASCII letters, digits and `-`, and fits the name module's maximum length. *If it holds*, chunk 2 checks that RetroArch derives the `.srm` and state names from that base name unchanged. *If it fails* (another character, a longer name), chunk 2 checks the actual rule from step 2.3's step note against RetroArch's save naming, Onion's subfolder rule and FAT; a character RetroArch or Onion rewrites or refuses is a mismatch row.

## Problem Statement

The plan describes an Onion app on a Miyoo Mini Flip: a folder under `App/`, ROMs in a subfolder of the SNES list, and a History tab that deletes a seed's ROM, save and save states. Every one of those rests on how OnionOS, its menu and its RetroArch behave, and so far the repository records only the toolchain. Without the rest, the next steps would guess: the screen model could pick a grid that does not fit the screen, the Generate tab could use keys Onion's menus use differently, a generated ROM could be invisible in Onion's list, and, worst for the user, History's delete could miss the save and states (leaving the next seed with the same name to load a stale state) or remove the wrong files. The plan's SD-card paths were written from memory; nobody has checked them against Onion's sources.

While writing this PRD, two facts already showed that the check is not a formality:

- Onion's `main` at the pinned commit `07505ea5` does not know the Mini Flip: its device model has only the Mini (283) and the Mini Plus (354). Mini Flip support (device 285, lid sensor, "MainUI 354 binaries … 640x480") is commit `9bf8ccd3` (2026-02-07, PR #1860), on branch `v4.5-dev` only (head `2bab767e4a17b1b489f02c9fb6af8dde310d4c3e` on 2026-11-01, when `main` was still `07505ea5`).
- The SNES save folder name is ambiguous inside Onion's own files: its saves README says SFC saves go under `Beetle Supafaust`, while its shipped core config folder is `Supafaust`. Only RetroArch's and the core's sources settle which one the `.srm` and states really use.

## Solution

One new document, `docs/onion.md`, records what the app needs from Onion, each fact with a permalink to the source line that proves it, at a pinned commit. It reuses `docs/toolchain.md` for the cross toolchain (linked, not copied) and adds the rest: the app folder and its `config.json`, the SDL API, version and video mode Onion apps use, the Mini Flip's screen and the resolution an app gets, the button codes and the keys Onion keeps for itself, the SNES list and its subfolder rule and cache, the default SNES core, and the exact `.srm` and save-state paths and names RetroArch writes for a ROM in `Roms/SFC/ALttPR/`.

Then the document checks each of §2's paths and layouts against those facts, one row each, with a verdict. If every row matches, the step ends with the facts committed and the later steps build on them. If any row does not, the run stops and the owner is asked, with the facts, the cited source and the options, before the Generate and History steps are built on a wrong path.

## User Stories

1. As the player, I want the app to appear in Onion's Apps list with its name and icon, so that I can start it like any other Onion app.
2. As the player, I want the app's own files (base ROM, history, settings) to stay inside its folder, so that I install and remove it by copying one folder.
3. As the player, I want a generated ROM to appear in Onion's SNES list under an `ALttPR` folder, so that I find my seeds in one place.
4. As the player, I want a newly generated ROM to show in the list without guessing a hidden refresh, so that I can play it right after generating.
5. As the player, I want each seed's RetroArch save to be its own file, so that two seeds keep separate saves.
6. As the player, I want deleting a seed in History to remove exactly that seed's save and save states, so that a later seed never loads a stale state and no other game loses its save.
7. As the player on a Mini Flip, I want the app drawn at the resolution the Flip actually gives an app, so that nothing is cut or blurred.
8. As the player, I want the app's keys to mean what they mean in Onion's own menus (confirm, back, tabs), so that I don't relearn the controls.
9. As the player, I want the keys Onion keeps for itself (menu, power, volume, lid) to keep working while the app runs, so that the console behaves as usual.
10. As the owner, I want every fact cited from Onion's source at a pinned commit, so that I can check it and see when Onion changes it.
11. As the owner, I want to know which Onion branch supports my Mini Flip, so that I know which build the app is checked against.
12. As the owner, I want a plain table of §2's paths with a verdict each, so that I see at a glance whether the plan's file layout holds.
13. As the owner, I want the run to stop and ask me when a §2 path does not match Onion, so that I decide before the Generate and History steps are built on it.
14. As the owner, I want that stop to give the fact, its link and the options, so that I can answer without redoing the research.
15. As the owner, I want facts that Onion's sources do not settle marked as such, with what the app assumes until I check on the device, so that no guess passes for a fact.
16. As the owner, I want the device checks I must make listed in the step note, so that I do them once, together.
17. As the screen-model author (Phase 3 step 2), I want the pixel resolution an app gets and Onion's own list metrics if its sources give them, so that the character grid can be sized from facts.
18. As the Generate tab author (step 4), I want the input codes of every button as an SDL 1.2 key and as a Linux input code, so that the state machine's keys map one to one.
19. As the Generate tab author, I want Onion's own menu conventions (which button confirms, which goes back, which change page or tab), cited from an Onion menu app, so that the app follows them.
20. As the generation-flow author (step 5), I want to know how Onion's SNES list discovers ROMs in a subfolder (depth, name rules, extensions, cache file), so that a new ROM is listed.
21. As the History author (step 6), I want the `.srm` path and every save-state file name RetroArch can write for a ROM (numbered slots, auto state, compression, thumbnails), so that delete removes all of them.
22. As the History author, I want the other per-ROM files Onion keeps (recent list, game-switcher screenshot, play activity) recorded, so that step 6 can decide what else a delete touches.
23. As the History author, I want Onion's profile mechanism recorded (what `Saves/CurrentProfile` is, and the guest profile), so that delete looks where the saves really are.
24. As the SDL renderer author (Phase 4 step 1), I want the SDL API and version Onion apps link against, the video call they make and where the device library comes from, so that the renderer cross-compiles against what the device has.
25. As the packaging author (Phase 4 step 2), I want `config.json`'s keys and value rules and `launch.sh`'s conventions (working directory, environment), so that the release zip's app starts.
26. As the packaging author, I want to know whether `config.json`'s icon may be a file inside the app folder, so that the icon ships in `App/ALttPR/`.
27. As the packaging author, I want the full-resolution opt-in recorded (the marker Onion checks before switching an app to the larger mode), so that the app gets the resolution it is designed for.
28. As the porting agent, I want the toolchain facts reused from `docs/toolchain.md`, re-checked on the Flip branch, so that there is one source for them.
29. As a later agent, I want each fact to say whether it holds on both Onion branches, so that I know which ones the Flip branch changes.
30. As a later agent, I want the RetroArch and core sources pinned too (the RetroArch build Onion ships, the core's commit), so that the save naming can be re-checked.
31. As the net, I want this step to change no code and no check, so that `make test` and `make check` stay as they are.
32. As a reviewer, I want every link to resolve to the cited line at its commit, so that a citation cannot be a guess.

## Implementation Decisions

### The document

- **New file `docs/onion.md`, titled "Onion facts"**, for everything but the toolchain. `docs/toolchain.md` keeps the toolchain item unchanged (it is referenced from four places; renaming it would be churn): `docs/onion.md` links to it, and `docs/toolchain.md`'s title becomes "Onion toolchain facts" with one line pointing to `docs/onion.md`. Nothing else in `docs/toolchain.md` changes, except the re-check line below.
- **Style: `docs/toolchain.md`'s.** Bold one-line fact, then the evidence (the quoted value or a short quoted line), then a GitHub permalink to the file and lines at a full commit hash. No fact from memory, from a forum or from a non-source web page; Onion's own `website/docs` pages count as Onion's sources, but a code or config line wins when both exist.
- **Two pinned Onion commits.**
  - `main` at `07505ea58c7bba698d6b9220ff43946a43cac76b`, the existing pin.
  - `v4.5-dev` at `2bab767e4a17b1b489f02c9fb6af8dde310d4c3e`, the branch carrying Mini Flip support (`9bf8ccd3`, PR #1860).
  - Every fact is cited at `07505ea5` and checked unchanged at `2bab767e`. A fact only the Flip branch has (device id 285, its detection, the lid key, its resolution handling) is cited at `2bab767e`. A fact that differs between the two is recorded for both, flagged "differs on v4.5-dev".
  - If either branch head has moved when the step runs, the implementer keeps these two hashes (stable citations) and records the newer heads in the step note.
- **Other pinned sources**, when Onion's own files do not settle a fact:
  - RetroArch's save naming comes from the RetroArch build Onion ships: `third-party/RetroArch-patch` is a submodule at `f9e959f7445d2ba0a4dd6279da41a095163767f2` (`OnionUI/RetroArch-patch`), `RA_SUBVERSION` in Onion's `Makefile`. The implementer follows it to the RetroArch source it builds, and cites that source at its commit.
  - The SNES core's library name comes from its own source at the commit its shipped binary names (the binary's version string, e.g. `1.29.0 75c658c`), with the shipped binary itself as the Onion-side evidence.
  - Clones go under `tmp/`; the Onion clone already at `tmp/onion` (`07505ea5`, with `origin/v4.5-dev` fetched) is used if present.
- **Sections of `docs/onion.md`**, one per plan item plus what the §2 check needs:
  1. *Device and Onion version.* Which commit supports which model; how Onion detects the Mini Flip; where Onion's version string lives on the SD card (so the owner can say which build they run).
  2. *App folder.* How Onion finds apps (directory scan of `App/`, a folder counts only with a `config.json`); the `config.json` keys Onion reads or writes (`label`, `icon`, `iconsel`, `launch`, `description`), and whether `icon` may be relative to the app folder; how `launch.sh` is run (working directory, environment such as `HOME` and `LD_LIBRARY_PATH`, the "app launch" detection in `runtime.sh`); what Onion does to `App/` folders on its own (the app sorter moves whole folders, icon packs rewrite `config.json`), and whether an app's data inside its folder survives that.
  3. *SDL and video.* The SDL API Onion apps use (the `SDL/SDL.h` include, `-lSDL`, `-lSDL_ttf`, `-lSDL_image`), its version (from the headers the pinned toolchain image provides, read through `onion-toolchain.sh` and cited by image digest and header path; the image's own source repository if it names the package), the video call Onion apps make (mode, depth, flags) and where the device's SDL library comes from. Where SDL2 exists on the card (bundled inside some packages only) is recorded too, because the PC build uses SDL2 (`CLAUDE.md`): this step records it, and Phase 4 step 1 decides.
  4. *Screen.* The physical resolutions Onion's sources know, how `runtime.sh` detects the panel, the resolution an app gets by default, and the full-resolution opt-in (the marker file and its conditions). For the Mini Flip: what the Flip branch's sources state. If they do not state the Flip panel's resolution, the section says so, gives the resolution an app is given regardless (from the code path, cited), and lists the panel check as a device check for the owner.
  5. *Buttons.* For every button, its Linux input code (`keymap_hw.h`) and SDL 1.2 key (`keymap_sw.h`), including the Flip's lid key. Which keys Onion's key monitor takes while an app runs (menu, power, volume, combinations), cited from `keymon`. Onion's own menu convention (confirm, back, page or tab keys), cited from one Onion list app such as Tweaks.
  6. *SNES list.* The default SNES emulator entry (its `config.json`: `rompath`, `extlist`, launch line with the core). The subfolder rule (one level deep, no apostrophe, `miyoogamelist.xml` caveat). The list cache file and how Onion's own scripts force a refresh. Whether a ROM written while the app runs appears without a refresh.
  7. *Saves and states.* `savefile_directory`, `savestate_directory` and the `sort_*`, `*_in_content_dir`, `savestate_auto_*`, `savestate_max_keep`, compression and thumbnail settings from Onion's shipped `retroarch.cfg`, and whether Onion forces that config at launch. Then, from RetroArch's source:
     - the folder name used under sorting: library name or `.info` core name, which settles `Supafaust` against `Beetle Supafaust`;
     - how a content base name becomes the `.srm` and state names: extension stripping, subfolder of the content ignored or not;
     - every state file name the settings above can produce (`.state`, numbered slots, `.state.auto`, any thumbnail or compressed variant).
     Ends with the worked example for `Roms/SFC/ALttPR/Open-Ganon-1CA10A.sfc`: the full path of its `.srm` and of each state file.
  8. *Other per-ROM files.* What else Onion keeps per ROM (recent list entry, game-switcher screenshot and its naming, play activity), each one line with its path rule. Recorded only: step 6 decides what a delete touches.
  9. *Profiles.* What `Saves/CurrentProfile` is, and where the main profile's saves are while the guest profile is active.
  10. *Checked against the plan* (chunk 2).
- **Toolchain re-check.** `docs/toolchain.md`'s facts are re-checked at `2bab767e`: same `TOOLCHAIN`, same miyoomini flags. One line records the result; a difference is recorded, not acted on.

### The §2 check and the stop

- **The table** (section 10): one row per §2 path or layout:
  - `App/ALttPR/` as an Onion app (`config.json`, `launch.sh`, icon);
  - `App/ALttPR/base.sfc`;
  - `App/ALttPR/history/`;
  - `Roms/SFC/ALttPR/` as a folder of the SNES list played by the default core;
  - the ROM name of H-2.3-2 as a content name;
  - the `.srm` location;
  - the save-state location.

  Each row gives §2's text, the step-2.3 constant (H-2.3-1), the facts it rests on (links into sections 1–9), and a verdict: `MATCH`, `MATCH, with a consequence` (the path works, but a later step must do something: refresh the list cache, remove every numbered state), or `MISMATCH`. The consequences are listed under the table, each with the step that owns it (4, 5, 6 or Phase 4).
- **What counts as a mismatch**: a §2 path or layout Onion or RetroArch does not support as §2 states it. For example:
  - Onion would not list `App/ALttPR/` as an app;
  - the SNES list would not show `Roms/SFC/ALttPR/` or would not play it with its default core;
  - RetroArch's `.srm` or states could not be found from the ROM's name alone (saved next to the content, or named from something other than its base name);
  - the ROM name would be rewritten or refused;
  - a §2 path behaves differently on the two pinned branches.

  Not a mismatch: anything §2 does not state. The exact save folder, the resolution, the keys, the SDL version and the missing Mini Flip support on `main` are recorded as facts and consequences for later steps.
- **How the stop is reported** (the plan: "a mismatch stops the step and asks the user before steps 4 and 6"):
  - Chunk 2's implementer writes the table whatever the verdicts. On any `MISMATCH` it reports `DONE: MISMATCH` followed by the rows. It never raises a mismatch as a `QUESTION:`: a `guideline` agent always answers, and the plan reserves this decision for the user.
  - Chunk 2's verifier then reports `FAILED: §2 mismatch, the owner decides before Phase 3 steps 4 and 6`, followed by, per row: §2's text, the Onion or RetroArch fact with its link, and the options (change §2's path to the one Onion supports, or keep it with a stated workaround). `FAILED:` stops `prd-autopilot`, which commits nothing for chunk 2 and leaves this PRD in triage, and `plan-pilot` reports the stop to the user.
  - Chunk 1's facts are already committed by then. Chunk 2's table stays in the working tree for the owner to read.
  - Resuming: the owner decides (a plan edit, or "keep §2"), and the run resumes on chunk 2, whose table then records the decision on the row.
  - With no `MISMATCH`, chunk 2 is committed like any chunk, and the step note lists the consequences.
- **Unsettled facts are not mismatches.** A fact the pinned sources do not settle (for example the Flip panel's physical resolution) is written as "not stated in the sources", with the cited code path that bounds it and the assumption later steps use. It becomes a device check in the step note, not a stop, unless a §2 row depends on it. A row that depends on it is a `MISMATCH` row, because the plan asks the user.

### What does not change

- No code, no test, no `Makefile`, no `CLAUDE.md`, no `README.md`, no plan edit (the step note and any plan change are `plan-pilot`'s). `make test` and `make check` are untouched and stay green.
- No decision for a later step: the grid size, the key bindings, the delete list, the cache refresh and the SDL choice are recorded as facts and consequences, and decided in their own steps.

## Implementation Chunks

1. **Onion facts.** `docs/onion.md` sections 1–9 with every citation; the title and pointer line in `docs/toolchain.md`, and its re-check line at `2bab767e`. Done when every plan item and every fact listed above has a cited entry, or an explicit "not stated in the sources" entry, and every link resolves to the quoted line at its commit.
2. **Checked against the plan.** Section 10: the table, the verdicts, the consequences with their owning steps, and the device checks for the step note. With a `MISMATCH`, this chunk is the stop (see *How the stop is reported*). Without one, it is committed.

## Testing Decisions

- **No automated test.** This step writes documentation only; the `tdd` skill's red-green cycle has nothing to drive, and adding a docs linter to `make test` would be a new check the plan does not ask for. `make test` and `make check` must stay green, unchanged.
- **What the verifier checks, per chunk** (in place of a test):
  - the diff touches only `docs/onion.md`, plus `docs/toolchain.md` in chunk 1;
  - every link is a permalink at a full commit hash: one of the two Onion commits, or a pinned RetroArch or core commit;
  - a sample of at least one link per section is re-opened at its commit, for example with `git -C tmp/onion show <commit>:<path>` and the cited lines, to confirm the quoted text is there;
  - every plan item has an entry;
  - in chunk 2, every §2 row has a verdict and every `MATCH, with a consequence` names its step.

  A broken link or a fact without a source is `FAILED:` with the line.
- **Prior art:** `docs/toolchain.md` (step 1.1), whose facts were cited the same way and checked by the step's verifier.
- **Device checks are the owner's** (ground rule: never launch the game or an emulator). The step note lists them, for example:
  - the Onion version running on the Flip;
  - the panel resolution an app gets;
  - an existing ROM's `.srm` folder name under `Saves/CurrentProfile/saves/`.

## Out of Scope

- Any code: the screen model, the key mapping, the History delete, the list-cache refresh, the SDL renderer and packaging are later steps (Phase 3 steps 2–7, Phase 4).
- Changing §2 or any plan step: a mismatch goes to the owner; a plan edit is the owner's or `plan-pilot`'s.
- Running anything on the device or in an emulator, or building Onion.
- Supporting Onion branches other than the two pinned commits, firmware other than OnionOS, or SNES cores other than Onion's default one. Other cores' save folders are not recorded.
- Deciding SDL 1.2 against SDL2 for the device build (Phase 4 step 1).

## Further Notes

Starting points found while writing this PRD. They are leads for the implementer, to be re-read and cited, never copied unverified:

- App scan: `src/common/utils/apps.h` (`/mnt/SDCARD/App`, `config.json` required, `label` read). Examples: `static/packages/App/*/App/*/config.json` and `launch.sh` (the icon given as an absolute path or as `../../Icons/...`, relative to the app folder; `cd $(dirname "$0")`). App sorter: `static/build/.tmp_update/script/app_sorter.sh`. Icon packs: `src/common/utils/apply_icons.h`.
- SDL: `src/common/system/keymap_sw.h` includes `SDL/SDL.h`; app `Makefile`s link `-lSDL -lSDL_image -lSDL_ttf`; `src/batteryMonitorUI/batteryMonitorUI.c` calls `SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE)`. The repository's `include/` has no SDL version header, so the version comes from the toolchain image. SDL2 shows up only bundled, for example under `static/build/.tmp_update/lib/parasyte/` and inside some packages.
- Screen: `src/common/system/display.h` (`DEFAULT_WIDTH` 640×480 under `PLATFORM_MIYOOMINI`); `static/build/.tmp_update/runtime.sh` `get_screen_resolution` (reads `/proc/mi_modules/fb/mi_fb0`, 752×560 only with a firmware date check, else 640×480, written to `/tmp/screen_resolution`) and `get_full_resolution_path` (an app's `full_resolution` marker).
- Device: `src/common/system/device_model.h` (283, 354 on `main`; 285 added on `v4.5-dev`); `runtime.sh` `main()` (detection by `axp` on `main`; by the hall sensor or `/dev/input/event1` on `v4.5-dev`).
- Buttons: `src/common/system/keymap_hw.h` (A `KEY_SPACE`, B `KEY_LEFTCTRL`, X `KEY_LEFTSHIFT`, Y `KEY_LEFTALT`, L1 `KEY_E`, R1 `KEY_T`, L2 `KEY_TAB`, R2 `KEY_BACKSPACE`, Select `KEY_RIGHTCTRL`, Start `KEY_ENTER`, Menu `KEY_ESC`; `v4.5-dev` adds the lid as `KEY_SLEEP`); `src/keymon/keymon.c` for the keys Onion takes; `src/tweaks/tweaks.c` for a menu's A, B and Select.
- SNES: `static/packages/Emu/Nintendo - SNES (Beetle Supafaust)/Emu/SFC/config.json` (`rompath` `../../Roms/SFC`, `extlist` `sfc|smc|fig|bs|st|zip|7z`) and `launch.sh` (`mednafen_supafaust_libretro.so`); `website/docs/04-emulators/index.mdx` (subfolders one level deep, no apostrophe); the list cache `<rompath>/SFC_cache6.db`, removed by `static/build/.tmp_update/script/reset_list.sh`.
- Saves: `static/configs/RetroArch/.retroarch/retroarch.cfg` (`savefile_directory` `/mnt/SDCARD/Saves/CurrentProfile/saves`, `savestate_directory` `.../states`, `sort_savefiles_enable` and `sort_savestates_enable` true, `*_by_content_enable` false, `*_in_content_dir` false, `savestate_auto_index`, `savestate_auto_save` and `savestate_auto_load` true, `savestate_max_keep` 10, `savestate_file_compression` true, `savestate_thumbnail_enable` false); `runtime.sh` calls `force_retroarch_cfg` before a game. The folder-name conflict: `static/configs/Saves/CurrentProfile/states/README.txt` says `SFC Beetle Supafaust`, while `static/configs/Saves/CurrentProfile/config/Supafaust/` and the core binary's strings say `Supafaust`; the `.info` file's `corename` is `Beetle Supafaust`.
- Other per-ROM files: `src/gameSwitcher/gs_model.h` (`romScreens`, `content_history.lpl`), `src/playActivity/playActivityDB.h`. Profiles: `GUEST_DIR` in `src/common/utils/apply_icons.h`.
- Step note for this step: the outcome of H-2.3-1 and H-2.3-2; the Onion heads seen on the day; every `MATCH, with a consequence` and its owning step; the device checks for the owner; and, after a stop, the owner's decision per mismatch row.
