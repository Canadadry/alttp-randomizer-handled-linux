---
title: "README: install, use and deletion behavior"
description: "A Mini Flip owner cannot install, run or trust the History delete of ALttPR on Miyoo from the README, which today only describes development."
status: needs-triage
---

## Problem Statement

The README tells a developer how to run the tests, but tells the person who
owns a Miyoo Mini Flip nothing they need: which Onion build the app needs,
what to copy to the SD card and where, where their Japanese 1.0 ROM goes and
under which name, how to drive the two tabs with the buttons, where a
generated ROM lands and how to make it show in the SNES list, and, above all,
what "Delete" in History really removes and what it leaves. Its status line
still says "being built". Without this page the owner has to read `docs/`,
the plan and the code to use the app, and a wrong guess about delete costs a
save.

## Solution

`README.md` gains three user sections, written from what the committed code
does, between the opening description and the existing **Development**
section:

1. **Install**: the requirements (a Miyoo Mini Flip on a Flip-aware Onion,
   `v4.5-dev` or later; a headerless Japanese 1.0 ROM with its MD5), then:
   put the app folder `App/ALttPR/` from the release zip at the root of the
   SD card, and copy the ROM to `App/ALttPR/base.sfc`. How to update (copy a
   newer `App/ALttPR/` over the old one; `base.sfc`, `history/` and
   `settings.json` stay). How to remove the app.
2. **Use**: start ALttPR from Onion's Apps list; the Generate tab (each row,
   the buttons, Spoiler in History, Generate Seed, Start), the Seed Ready
   screen (the ROM name, saved in `Roms/SFC/ALttPR/`), playing it from the
   SNES list with Onion's own SNES core, refreshing the list if the ROM does
   not show, one seed = one ROM = its own save; the History tab (newest
   first, details with settings and the kept spoiler); the error messages and
   what to do (missing or wrong `base.sfc`, base patch, SD card).
3. **Deleting a seed**: what History's delete removes, in which order, what it
   never touches, and what happens when a ROM is deleted by hand.

The **Status** line changes from "being built" to what is true at the end of
the plan (usable, with the owner's device checks pending in
`notes/owner-todo.md` while that file exists). The **Development** section
keeps its content and gains the package target and the PC try-out command.

## User Stories

1. As a Mini Flip owner, I want to know first which Onion build the app needs, so that I do not install it on a `main` build that does not know the Flip.
2. As a Mini Flip owner, I want a command to check my Onion version and device id (`version.txt`, `/tmp/deviceModel` = 285), so that I can tell whether my card qualifies.
3. As a Mini Flip owner, I want to know exactly which ROM I need (Japanese 1.0, *Zelda no Densetsu: Kamigami no Triforce*, headerless, 1,048,576 bytes, MD5 `03a63945398191337e896e5771f77173`), so that I do not waste time with a US ROM or a headered dump.
4. As a Mini Flip owner, I want to be told that a US ROM is not supported and a headered ROM is refused, so that I know why my ROM fails.
5. As a Mini Flip owner, I want to know where to get the release zip, so that I can start the install.
6. As a Mini Flip owner, I want to know that the zip holds an `App/ALttPR/` folder to put at the root of the SD card, so that Onion lists the app.
7. As a Mini Flip owner, I want to know the exact file name and place of the ROM (`App/ALttPR/base.sfc`, lower-case), so that the app finds it.
8. As a Mini Flip owner, I want to know that the ROM never leaves my card and that the repository and the zip carry no Nintendo ROM, only the base patch, so that I understand why I must supply it.
9. As a Mini Flip owner, I want to know what the app creates on the card by itself (`Roms/SFC/ALttPR/`, `App/ALttPR/history/`, `App/ALttPR/settings.json`), so that I am not surprised by new folders.
10. As a Mini Flip owner, I want to know how to update the app without losing my History, last settings and `base.sfc`, so that an update is safe.
11. As a Mini Flip owner, I want to know how to remove the app and what removing `App/ALttPR/` leaves behind (generated ROMs and their saves), so that I can clean up fully or keep my seeds.
12. As a player, I want to know where to find the app (Onion's Apps list, its label), so that I can start it.
13. As a player, I want a table of the buttons on each screen (Up/Down move, Left/Right change, A, Y default, L2/R2 page, Start generate, L1/R1 change tab, B, X, Menu), so that I can drive the app without trying every key.
14. As a player, I want the Generate rows listed with what they mean in alttpr.com terms (glitches, item placement, dungeon items, accessibility, goal, tower and Ganon crystals, world state, swords, item pool, item functionality, hints, heart beep), so that I can pick a seed like on alttpr.com.
15. As a player, I want to know that the seed is always random, that there is no seed entry or sharing, and that the last settings are remembered, so that I know what to expect.
16. As a player, I want to know that "Spoiler in History" is off by default, starts off at every launch, never changes the ROM, and only decides whether item locations are kept in History, so that I choose it knowingly.
17. As a player, I want to know that generation takes a few seconds and shows a "Generating Seed" screen, so that I do not think the console froze.
18. As a player, I want to know where the generated ROM goes and how it is named (`Roms/SFC/ALttPR/<Mode>-<Goal>-<code>.sfc`, `-2`… on a name clash), so that I recognise it in the SNES list.
19. As a player, I want to know that I play the seed from Onion's SNES list (folder `ALttPR`) with Onion's default SNES core, so that I do not look for it inside the app.
20. As a player, I want to know what to do when a new ROM does not show in the SNES list (Refresh the list from the SNES entry: Select › Refresh all roms), so that I can find it.
21. As a player, I want to know that each seed keeps its own save and save states, so that I can play many seeds in parallel.
22. As a player, I want to know what each failure message means and its fix (Base ROM not found, not the JP 1.0 ROM, wrong size, base patch missing or refused, SD card write failed, generator gave up), so that I can recover alone.
23. As a player, I want to know that the base ROM is checked when I press Generate, so that I am not surprised that the app starts without it.
24. As a player, I want to know what the History tab shows (newest first, date, settings, the spoiler if kept, scrollable), so that I can look back at a seed.
25. As a player, I want to know that History lists only seeds whose ROM is still in `Roms/SFC/ALttPR/`, so that I understand why a seed vanished.
26. As a player, I want to know exactly what Delete removes: the ROM, its per-ROM core override in `Roms/SFC/ALttPR/.game_config/`, its `.srm` save and every save state (any slot, auto, thumbnails) under `Saves/<profile>/saves|states/Supafaust/`, in every profile folder present (Current, Main, Guest), then the History record, then the SNES list cache, so that I know nothing of that seed stays playable.
27. As a player, I want to know that Delete asks for confirmation (only A deletes, B keeps) and cannot be undone, so that I do not lose a save by mistake.
28. As a player, I want to know what Delete never touches (other seeds, including a `-2` sibling; any other `Roms/SFC/` folder; saves made with another core; RetroArch's history, Onion's Recents, game-switcher screenshots, play activity), so that I know what may still show the deleted seed.
29. As a player, I want to be warned that a hand-made copy of a generated ROM with the same file name elsewhere in `Roms/SFC/` shares its save, which Delete removes, so that I do not lose it.
30. As a player, I want to know that a ROM deleted by hand with Onion's file manager simply disappears from History, its record kept, and reappears if the ROM is put back, and that its save and states then stay on the card, so that I know the difference with Delete.
31. As a player, I want to know what happens when a delete is only partial ("Some save files were not deleted", "ROM could not be deleted"): the record stays and I can retry, so that I can finish it.
32. As a Guest Mode user, I want to know that seeds and History are shared between profiles and that Delete clears every profile's save of that seed, so that switching profiles holds no surprise.
33. As the owner, I want a one-line way to try the app on my PC before copying it (`make sdl-pc`, then the `alttpr` command with `--sd` and `--font`), so that I can check the rendering first.
34. As the owner, I want the Development section to name the target that builds the Miyoo zip and the CI job that uploads it, so that I can produce the zip myself.
35. As a reader of the README, I want every path, button and message in it to match the shipped app, so that I can trust it.
36. As a reviewer, I want each README claim checkable against plan §2 and the committed code, with any difference from §2 stated, so that the review step can pass or fail it.

## Implementation Decisions

- **One file changes: `README.md`.** No code, no test data, no other doc.
  `docs/onion.md`, the plan and `notes/` are linked, not copied.
- **Order of sections:** title and description (kept, with the status line
  updated), **Install**, **Use**, **Deleting a seed**, then the existing
  **Development** and **Working with Claude** sections unchanged except as
  below.
- **The README follows the committed code, not the plan's wording, where
  they differ**, and says so in Further Notes for the reviewer. Known
  differences: §2 says the base ROM's MD5 is checked "at start"; the app
  checks it when Generate is pressed (the runner reads `App/ALttPR/base.sfc`
  then; the shell does not read it at start). The README states the real
  behavior. §2's "deletes the ROM, `.srm` and save states" is narrower than
  the shipped delete (also `.game_config`, every profile, the list cache):
  the README states the shipped set.
- **Paths are written as the code spells them**: `App/ALttPR/base.sfc`,
  `App/ALttPR/base.bps`, `App/ALttPR/history/`, `App/ALttPR/settings.json`,
  `Roms/SFC/ALttPR/`, `Roms/SFC/ALttPR/.game_config/`,
  `Saves/{CurrentProfile,MainProfile,GuestProfile}/{saves,states}/Supafaust/`,
  `Roms/SFC/SFC_cache6.db` and `SFC_cache2.db`. Every one is checked with
  `git grep` against the SD constants of the SD layer before commit.
- **Requirements stated:** Onion `v4.5-dev` or later (the only branch that
  knows the Flip, device 285), with the two checking commands of
  `docs/onion.md` device check 1; the JP 1.0 ROM, headerless, 1,048,576
  bytes, MD5 `03a63945398191337e896e5771f77173`; a US ROM unsupported; a
  headered ROM refused as a wrong size, not stripped.
- **Install steps** (numbered): get `ALttPR.zip`, either from the manual
  CI job `package` (GitHub → Actions → **check** → Run workflow → the run's
  artifact `alttpr-miyoo`, which GitHub wraps in its own zip, so it is
  extracted twice) or locally with `make package`; unzip `ALttPR.zip` at the
  SD card's root (its only top-level folder is `App/ALttPR/`); copy the ROM
  next to the app as `App/ALttPR/base.sfc`; put the card back and start
  **ALttPR** from Apps (the `config.json` label). There is no GitHub
  Release; the README does not promise one. The README says the app creates
  `Roms/SFC/ALttPR/` and `App/ALttPR/history/` itself at the first generation
  (the runner does, C3), so the user creates nothing.
- **Update and removal:** updating = copying a newer `App/ALttPR/` over the
  old one (unzip `ALttPR.zip` at the root again); the zip holds no
  `base.sfc`, `history/` or `settings.json`, so those stay. Removal = deleting `App/ALttPR/`; generated ROMs
  in `Roms/SFC/ALttPR/` and their saves stay until deleted (from History
  before removal, or by hand).
- **Use, Generate tab:** a button table built from the core's key handling:
  Up/Down move (wrap), Left/Right change (wrap), A changes or generates on
  "Generate Seed", Y resets a row to its default, L2/R2 page, Start generates
  from any row, L1/R1 switch tabs, B or Menu quit. Rows named with the
  app's English labels (Glitches, Item Placement, Dungeon Items,
  Accessibility, Goal, Tower Crystals, Ganon Crystals, World State, Swords,
  Item Pool, Item Functionality, Hints, Heart Beep, Spoiler in History),
  each with its values in one short line or a compact table; the README does
  not re-explain alttpr.com's settings, it links alttpr.com's options page.
  Last settings remembered except the spoiler option (starts off each
  launch, owner decision pending in `notes/owner-todo.md`). Seed always
  random, no seed entry, no sharing.
- **Use, generation:** "Generating Seed" for a few seconds (no progress
  bar), then "Seed Ready" with the ROM name; the name scheme
  `<Mode>-<Goal>-<code>.sfc` with an example (`Open-Ganon-1CA10A.sfc`) and
  the `-2`… suffix. The ROM is played from the SNES list's `ALttPR`
  subfolder with Onion's default SNES core (Beetle Supafaust). If the screen
  says "Refresh the SNES list to see it", or the ROM is missing from the
  list, use Onion's Select › Refresh all roms on the SNES entry. One seed,
  one ROM, one save.
- **Use, errors:** a short table, one row per message the generation flow
  can show to a user who did not tamper with the app (base ROM not found /
  unreadable / wrong size / not the JP 1.0 ROM; base patch not found /
  refused / for another ROM / gave the wrong ROM → reinstall the app;
  folders, SD card write failed → check the card; generator gave up →
  generate again). Messages quoted exactly as the English string table
  spells them; checked with `git grep` against the table before commit.
- **Use, History tab:** newest first, "Generated <date>"; A opens details
  (date, seed, settings, spoiler or "No spoiler kept for this seed"), L2/R2
  scroll, B back; X deletes from the list or the details; notices for an
  unreadable folder, unreadable records, more than the shown cap.
- **Deleting a seed** (its own section, the plan's "deletion behavior"):
  - confirmation "Delete <code>?", "Removes the ROM, its save and states",
    "This cannot be undone"; A deletes, B keeps;
  - removed, in order: the ROM; `Roms/SFC/ALttPR/.game_config/<base>.cfg`;
    in each of `Saves/CurrentProfile`, `MainProfile`, `GuestProfile` that
    exists, `saves/Supafaust/<base>.srm` and every file of
    `states/Supafaust/` starting with `<base>.state` (`.state`, `.state1`…,
    `.state.auto`, `.png` thumbnails); then the History record; then the SNES
    list cache, so the list forgets the ROM (else "Seed deleted: refresh the
    SNES list");
  - never touched: other seeds (a `-2` sibling never matches), other
    `Roms/SFC/` folders, saves under another core's folder (a per-ROM core
    change keeps its save: delete it by hand), RetroArch's history, Onion's
    Recents, game-switcher screenshots, play activity (they may still list
    the deleted seed; the owner's device check 7 is pending);
  - warning: a hand-made copy with the same file name elsewhere in
    `Roms/SFC/` shares the save (RetroArch names saves by file name only), so
    Delete removes that copy's save too;
  - partial failure: if the ROM stays, nothing else is touched; if a save
    file stays, the record is kept and Delete can be retried;
  - deleting a ROM by hand (Onion's file manager): the seed disappears from
    History at next start, its record kept (put the ROM back and it
    reappears); its save and states stay on the card.
- **Profiles:** seeds, History and `base.sfc` are shared by all Onion
  profiles; saves are per profile; Delete clears every profile's.
- **Development section:** add `make package` (outside `make check`, needs
  docker: strips and ABI-checks the `sdl-arm` binary, re-checks
  `data/base.bps`, writes `tests/rando/build/package/ALttPR.zip`, the same
  bytes for the same commit) and the manual CI job `package` (`make
  package` then `make e2e-arm`, uploading the artifact `alttpr-miyoo`) next
  to `make e2e-arm`, and the PC try-out command
  (`make sdl-pc`, then `tests/rando/build/sdl/alttpr --sd tmp/sd --font
  data/DejaVuSansMono.ttf`, keyboard = Onion's map: arrows, Space A, Left
  Ctrl B, Left Shift X, Left Alt Y, E/T L1/R1, Tab/Backspace L2/R2, Enter
  Start, Right Ctrl Select, Esc Menu; a/b/x/y and Page Up/Down also work).
  Existing text otherwise unchanged.
- **Status line:** "Status: being built" becomes a line saying the app is
  usable and that the owner's device checks are listed in
  `notes/owner-todo.md`; if that file no longer exists at implementation
  time, the line drops the reference.
- **Link, don't copy:** Onion facts link `docs/onion.md`; the base patch's
  origin links the plan's step 0.1 note only as "how `data/base.bps` was
  built" in Development, not in Install (the user never builds it).
- **Style:** English, short sentences, controller words as printed on the
  Flip (A, B, X, Y, L1, R1, L2, R2, Start, Select, Menu), no emoji, no
  Nintendo ROM link or download hint.

## What Phase 4 step 2 shipped

Facts from the committed package (`app/package/`, `tests/rando/Makefile`'s
`package` target, `.github/workflows/check.yml`, `CLAUDE.md` § Checks); the
README uses them as written here. They replace this PRD's earlier
hypotheses H-4.2-1 to H-4.2-9 (outcomes in Further Notes).

- **The zip** is `ALttPR.zip`; its only top-level folder is `App/ALttPR/`,
  so unzipping it at the SD card's root is the whole copy step.
- **Local build:** `make package` (root `Makefile`, outside the net, needs
  docker) runs `sdl-arm`, strips the binary with Onion's toolchain, checks
  it with `file` and `readelf` (32-bit ARM EABI5, hard-float, dynamically
  linked, needs `libSDL-1.2.so.0` and `libSDL_ttf-2.0.so.0`), re-runs the
  base-patch header check on `data/base.bps`, and writes
  `tests/rando/build/package/ALttPR.zip`; the same commit gives the same
  zip bytes.
- **CI:** the manual (`workflow_dispatch`) job `package` of the `check`
  workflow runs `make package` then `make e2e-arm` and uploads the zip as
  the artifact `alttpr-miyoo`. No GitHub Release exists.
- **`App/ALttPR/` holds exactly:** `alttpr` (the stripped device binary),
  `launch.sh`, `config.json`, `icon.png`, `base.bps` (`data/base.bps`
  unchanged), `DejaVuSansMono.ttf` and `DejaVuSansMono-LICENSE.txt`.
- **`launch.sh`** does `cd "$(dirname "$0")"`, then runs `./alttpr` with no
  `--sd`, its error output going to `App/ALttPR/alttpr.log` (rewritten at
  each launch); the SD root is two folders up from `App/ALttPR/`, and the
  font is found by its default name in that folder.
- **`config.json`:** `label` `ALttPR`, `icon`
  `/mnt/SDCARD/App/ALttPR/icon.png`, `launch` `launch.sh`, `description`
  "A Link to the Past Randomizer seeds".
- **Not in the zip:** `base.sfc`, `history/`, `settings.json`,
  `alttpr.log`, `full_resolution`, anything outside `App/ALttPR/`. So an
  update keeps the user's ROM, History and last settings, and the app runs
  at 640×480.

The README mentions `alttpr.log` once, under errors: if the app does not
start or quits at once, that file says why (to report it).

## Testing Decisions

- **No automated test.** The change is prose in `README.md`; the plan
  verifies it "by review against §2". A test grepping README prose would
  freeze wording, not behavior, and nothing in `make test` or `make check`
  reads the README. `make check` must stay green (it is unaffected).
- **Review checklist** (done by the implementer before commit, results in
  the commit message):
  - every §2 item a user meets is in the README and agrees with it, or the
    difference is the code's real behavior and is stated (MD5 checked at
    Generate, the wider delete set): base ROM name and place, MD5 check and
    message, ROM folder and name scheme, History record place, spoiler
    option off by default and never in the ROM, History list/detail/delete
    with confirmation, delete's ROM/`.srm`/states, a hand-deleted ROM
    leaving History, the two tabs, English only, random seed, last settings
    remembered;
  - every path in the README `git grep`s to an SD constant or `docs/onion.md`;
  - every quoted on-screen text `git grep`s to the English string table;
  - every button in the button tables matches the core's key handling for
    that screen;
  - every make target, CI job, artifact and zip entry named exists in the
    committed `Makefile`s, workflow and `build_zip.py`;
  - the Markdown renders (headings, tables, code blocks) — checked by
    reading it, not by a tool.
- **Prior art:** the existing README's Development section and step notes'
  habit of quoting exact strings and paths.

## Out of Scope

- Phase 4 step 2's zip, make target, CI job, `launch.sh`, `config.json`,
  icon, `build_zip.py`: only described, never changed here.
- Any code, string-table or behavior change, including making the base ROM
  check run at start to match §2's wording, or remembering the spoiler
  option.
- Translating the README; screenshots or images of the app.
- Building the base patch (owner-only, step 0.1 note); `notes/owner-todo.md`
  edits; the plan's final step note and the pull request.
- Explaining alttpr.com's settings in depth (linked instead), racing,
  seed sharing, other firmwares.
- The **Working with Claude** section (unchanged).

## Further Notes

- For the reviewer: the README deliberately departs from §2's text in two
  places, both because the shipped code differs and the README must match
  the shipped app — the base ROM's MD5 is checked when Generate is pressed,
  not at start (failure: "Base ROM is not the JP 1.0 ROM", advice "Needs
  App/ALttPR/base.sfc (JP 1.0)"); and Delete removes more than §2 lists
  (`.game_config`, every profile, the list cache), as decided at step 3.6.
  Whether the start-time check should be added is not this step's call; a
  reviewer who wants it raises it separately.
- The step 3.6 note marks the same-name-copy warning (C7) "for the README":
  it must appear in **Deleting a seed**.
- Owner checks still pending (`notes/owner-todo.md` §3) bound some README
  sentences: the `Supafaust` folder (check 3), the list refresh (check 5),
  the icon (check 6), Recents and game switcher after delete (check 7). The
  README states the app's behavior and does not promise these outcomes on
  every card.
- Earlier hypotheses about Phase 4 step 2, checked against commits
  `4bd2782`, `5368106`, `8551386`: **H-4.2-1** partly (root only
  `App/ALttPR/` held; the name is `ALttPR.zip`, not `alttpr-miyoo.zip`);
  **H-4.2-2** false (`make package`, not `make miyoo`; the zip is at
  `tests/rando/build/package/ALttPR.zip`; outside the net and docker held);
  **H-4.2-3** partly (manual job, artifact `alttpr-miyoo`, no Release held;
  the job is `package`, not `miyoo`, and also runs `make e2e-arm`);
  **H-4.2-4** held (the icon is `icon.png`); **H-4.2-5** held (plus stderr
  to `alttpr.log`); **H-4.2-6**, **H-4.2-7**, **H-4.2-8**, **H-4.2-9** held.
- `make test` now checks `data/base.bps` itself (`4bd2782`); `make
  patch-header` checks any other patch. Install never asks the user to
  build or check the patch.
