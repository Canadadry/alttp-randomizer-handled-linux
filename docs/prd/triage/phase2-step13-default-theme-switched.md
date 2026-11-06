---
title: "Default theme switched"
description: "The release zip still ships the phase-1 `classic` grid look as the app's default theme, so a player who installs ALttPR on the Mini Flip never sees the alttp look built from the owner's mockups, and the plan's goal (the look changed by a theme edit alone) has never been shown end to end."
status: needs-triage
---

## Problem Statement

Steps 8 to 12 built `themes/alttp/`: the mockups' zones and fonts in Press
Start 2P, `frame.png` and thirteen sprites made from the mockups' own shapes,
a help line per value, every 01–08 capture matching its mockup's layout. But
the release still ships the old look: `make package` copies `PKG_THEME`, which
is `themes/classic`, into the zip as `App/ALttPR/themes/default/`, and the
app on the device reads its look from there. A player who unzips
`ALttPR.zip` on the Mini Flip gets the 40×15 DejaVu grid the owner called
"very, very ugly".

The package self-test (`tests/rando/test_package.py`) pins that: its
`RealPackage` case builds the zip from `themes/classic` and checks the zipped
theme against it byte for byte, so it says nothing about the theme `make
package` really ships, and would stay green if the two drifted apart.

The docs say the same old thing: `README.md` says the zip ships `classic`
and tells the owner to look at the PC build with `--theme themes/classic`;
`CLAUDE.md` names `PKG_THEME` as `themes/classic`; the root `make help` line
of `package` too; `notes/owner-todo.md` asks the owner to check the PC look
with `classic`.

And the plan's goal, "changing the look means editing the theme folder and
relaunching the app: no rebuild, and no test of the net has to change", has
been relied on by every step but never shown once on the finished theme.

## Solution

`PKG_THEME` becomes `themes/alttp`: `make package` ships the alttp theme
(its `theme.ini`, `frame.png`, the thirteen sprite PNGs, `PressStart2P.ttf`
and its licence) as `App/ALttPR/themes/default/`. `classic` stays in the
repository as the SDL tests' theme: the 13 goldens, the smoke test and the
classic tests keep running on it, untouched.

The package self-test reads the default theme from the same place `make
package` does (the Makefile's `PKG_THEME`), checks it is `themes/alttp`, and
checks the zipped theme against that folder byte for byte. A test can no
longer pass on one theme while the zip ships another.

`README.md`, `CLAUDE.md`, the root Makefile's help and `notes/owner-todo.md`
say that the zip ships `alttp`, where the theme lives (`themes/alttp/` in
the repository, `App/ALttPR/themes/default/` on the card), how to look at it
on the PC (`--theme themes/alttp`), how to compare it with the mockups (`make
theme-compare`), and that unzipping over an older install leaves the old
classic files in `themes/default/`, unused.

Then the goal is shown once by hand, on the built PC binary, with no
rebuild: one colour, one font size and one row height edited in
`themes/alttp/theme.ini`, one at a time, each drawn through the same binary
and seen to change the captures, each reverted, the reverted theme drawing
byte-identical captures to the first run, and `git status` clean at the end.
The record goes in the step notes.

## User Stories

1. As a player, I want the zip to ship the alttp look as the default, so that the app on my Mini Flip looks like the owner's mockups.
2. As a player, I want the notched frame, the triforces, the arrows and the button sprites on the device, so that the screens read like a Zelda menu.
3. As a player, I want Press Start 2P shipped with the theme, so that the device draws the mockups' font and not a missing-font error.
4. As a player, I want the font licence shipped beside the font, so that the zip respects the SIL OFL 1.1.
5. As a player, I want the help line under the list on the device, so that I know what each value does while I choose it.
6. As a player, I want the zip still to hold only `App/ALttPR/`, so that installing stays one unzip at the card's root.
7. As a player updating over an older install, I want to know that the old classic files left in `themes/default/` are unused, so that I neither worry about them nor need to clean them.
8. As a player updating over an older install, I want the new `theme.ini` to replace the old one, so that the update switches the look without any manual step.
9. As a player, I want my ROM, History and settings untouched by the new zip, so that switching the look costs nothing.
10. As the owner, I want `make package` to write the zip with the alttp theme, so that the CI artifact `alttpr-miyoo` is the new look.
11. As the owner, I want the same commit still to give the same zip bytes, so that a release is reproducible.
12. As the owner, I want the zip's binary checks (ARM EABI5, hard-float, NEEDED SDL, SDL_ttf, SDL_image) unchanged, so that the switch touches only the theme.
13. As the owner, I want `classic` kept in the repository, so that the phase-1 goldens still prove the engine.
14. As the owner, I want the README to say where the theme lives and how to compare it with my mockups, so that I can judge it without reading the code.
15. As the owner, I want the README's PC instructions to use `--theme themes/alttp`, so that what I look at on the PC is what the device shows.
16. As the owner, I want my to-do's PC look check to name the alttp theme, so that I check the look that ships.
17. As the owner, I want a hand check on the device of the new look in my to-do, so that I know the device draw is still mine to confirm.
18. As the owner, I want proof that a colour edit in `theme.ini` changes the screen with no rebuild, so that I trust the look can be iterated on.
19. As the owner, I want proof that a font size edit does the same, so that a bigger or smaller text is a theme edit.
20. As the owner, I want proof that a row height edit does the same, and changes the page size the app shows, so that going back to bigger rows stays a theme edit.
21. As the owner, I want the proof to show that no test of the net had to change, so that the visuals can be iterated on without touching tests.
22. As the owner, I want the proof recorded in the step notes, so that I can read it without rerunning it.
23. As a maintainer, I want the package self-test to read the shipped theme from the Makefile's `PKG_THEME`, so that the test and the zip can never name two different themes.
24. As a maintainer, I want a test that `PKG_THEME` names `themes/alttp`, so that switching the default back by accident turns `make test` red.
25. As a maintainer, I want a missing or duplicated `PKG_THEME` line refused by the self-test with one clear failure, so that a Makefile edit cannot silently disable the check.
26. As a maintainer, I want the zipped theme compared byte for byte with `themes/alttp/`, file list included, so that every PNG and the font ship exactly as committed.
27. As a maintainer, I want the self-test still to find no font loose in `App/ALttPR/`, so that fonts only ship inside the theme.
28. As a maintainer, I want `make test` to stay as fast as today, so that the fast loop stays fast.
29. As a maintainer, I want the SDL tests, the smoke test and the goldens to stay on `classic`, so that the pinned golden environment and the engine proof are not disturbed.
30. As a maintainer, I want `CLAUDE.md` and the root `make help` to name the shipped theme correctly, so that the next session's notes stay true.
31. As a maintainer, I want the demonstration's captures kept out of the repository, so that a throwaway check leaves nothing to commit.
32. As a maintainer, I want the demonstration's edits made on the real `themes/alttp/theme.ini`, so that it proves the folder the zip ships, not a copy.
33. As a maintainer, I want each demonstration edit to touch a key the loader and the app accept, so that the demonstration shows a change and not a refusal.
34. As a maintainer, I want the demonstration to avoid the keys the page, width and help bound are derived from (the list and help fonts), so that its font size edit does not double as a page change.
35. As a maintainer, I want the binary's digest recorded before and after the demonstration, so that "no rebuild" is a fact and not a claim.
36. As a maintainer, I want the reverted theme's captures byte-identical to the first ones, so that the revert is proven complete.
37. As a maintainer, I want `make check` and `make package` green after the switch, so that the step leaves the net and the release intact.

## Implementation Decisions

### Packaging

- **`PKG_THEME` becomes `themes/alttp`** (the variable of `tests/rando`'s
  Makefile, same relative form as today). Nothing else in `make package`
  changes: the strip, the `file`/`readelf` checks, `test_patch_header --file`
  and `build_zip.py`'s call stay as they are.
- **`build_zip.py` does not change.** It already ships every file of a flat
  theme folder and refuses subfolders, symlinks and hidden files;
  `themes/alttp/` is flat and holds 17 regular files (`theme.ini`,
  `frame.png`, 13 sprite PNGs, `PressStart2P.ttf`,
  `PressStart2P-LICENSE.txt`). `test_theme` already checks that every file
  of `themes/alttp/` is named by its `theme.ini` or is a font licence, so
  nothing stray can ship.
- **`launch.sh`, `config.json`, `icon.png`: unchanged.** The app finds
  `themes/default/` next to its binary (step 1.6), whatever theme is in it.
- **The CI workflow: unchanged.** Its `package` job runs `make package` and
  uploads the zip, which now carries alttp. (The `sdl` artifact uploads the
  PC binary alone, as step 1.6 noted; not this step's.)
- **Updating over an old install** leaves classic's `DejaVuSansMono.ttf`,
  its licence and `background.png` in `App/ALttPR/themes/default/`: the
  zip cannot delete files. Decided harmless and documented, not fixed: the
  app never lists the theme folder (it opens only the files `theme.ini`
  names) and the new `theme.ini` overwrites the old one. No cleanup step in
  `launch.sh`: the rules keep the app from touching anything but its own
  outputs, and a few hundred kB of dead files is not worth a startup delete.

### The package self-test

- **The default theme is read from the Makefile.** `test_package.py` reads
  `tests/rando/Makefile` (beside it), finds the single line
  `PKG_THEME := <path>`, and resolves `<path>` against its own directory
  (the Makefile's recipes run from `tests/rando/`, so the path means the
  same to both). No line, or more than one, is a test failure naming the
  Makefile. This replaces the hard-coded `CLASSIC` constant of
  `RealPackage`.
- **A test pins the choice**: the resolved `PKG_THEME` is the repository's
  `themes/alttp` (compared as real paths). This is the step's deliberate
  change of an expected value: the self-test used to expect `classic` byte
  for byte, it now expects `alttp`, because the plan's step 13 switches the
  default; stated in the commit message.
- **`RealPackage` builds from `PKG_THEME`** and its byte-for-byte test
  (renamed from "theme is classic" to "theme is the package's default"):
  the zipped theme's file list equals the folder's sorted listing, it holds
  `theme.ini`, and each file equals its source.
- The other `RealPackage` tests (config names the app, no loose font, icon
  is a PNG) and the whole `Builder` and `Launch` classes stay as they are:
  they run on planted inputs and do not depend on the default theme.
- The self-test stays standard-library Python, in-process, under `make
  test`: reading one Makefile line costs nothing.

### Docs

- **`README.md`**:
  - The `make package` paragraph: `themes/default/` is a copy of every file
    of `themes/alttp/` (its `theme.ini`, `frame.png`, the thirteen sprites,
    Press Start 2P and its licence).
  - The PC instructions and the `--shot` example use `--theme themes/alttp`
    ("on the PC, pass `--theme themes/alttp`").
  - The themes paragraph: `alttp` is the look the zip ships; `classic`, the
    40×15 grid look of phase 1, stays as the SDL tests' theme (the goldens
    are drawn with it).
  - A short "where the theme lives" statement, near the themes paragraph:
    `themes/alttp/` in the repository, shipped as
    `App/ALttPR/themes/default/`; changing the look is editing
    `themes/alttp/theme.ini` (or its PNGs, through `make theme-art-write`)
    and relaunching, with no rebuild; compare it with the mockups with
    `make theme-compare` (already documented below it: the paragraph points
    there rather than repeating it).
  - The **Update** section: unzipping over an older `App/ALttPR/` can leave
    files of the previous theme in `themes/default/`; they are unused and
    may be deleted.
- **`CLAUDE.md`**: the `themes/` layout line (classic is the phase-1 grid
  look kept as the SDL tests' theme; alttp is the shipped look); the owner's
  PC look command uses `--theme themes/alttp`; the `make package` line names
  `PKG_THEME` as `themes/alttp`. The goldens' lines keep `themes/classic`
  (they are drawn with it).
- **Root `Makefile` help** for `package`: `themes/alttp as themes/default`.
- **`notes/owner-todo.md`**: §5's PC look check runs `--theme themes/alttp`;
  §5 gains a hand check that the app on the Mini Flip draws the alttp look
  (frame, sprites, Press Start 2P, help line), since nothing in the net
  runs the SDL app on the device. §8 is unchanged.
- **`targets/mac/`**: its `run` target and its README run the app with
  `--theme themes/classic` as the look to try; both move to
  `--theme themes/alttp` (the mac build reads `--theme` like the PC one).
  Nothing else there changes.

### The demonstration (by hand, nothing committed)

Decided procedure, run after the switch and before the step's `make check`:

- **Build once, then never again until the end**: `make sdl-pc` (it builds
  `tests/rando/build/sdl/alttpr` and `tests/rando/build/sd_fixture`, and
  must print its two `goldens:` lines, not a `setup:` line, per the plan's
  rule that a `setup:` line blocks a visual check). Record the sha256 and
  mtime of `tests/rando/build/sdl/alttpr`. No `make` target runs between
  this and the last capture.
- **Captures**: each pass is `tests/rando/sdl/goldens.sh
  tests/rando/build/sdl/alttpr themes/alttp tests/rando/build/sd_fixture
  --draw tmp/theme-demo/<pass>` (run from the repository root with the
  paths adjusted as the script expects, or from `tests/rando/` with
  relative paths as the Makefile does): the existing binary plays the 13
  states of `golden_states.txt` with `--shot` and `--shot-text`, one `.bmp`
  and one `.txt` per state. Everything goes under `tmp/` (git-ignored).
- **Passes**: `base` (theme untouched); then, one edit at a time, each
  drawn and then reverted with `git checkout -- themes/alttp/theme.ini`
  before the next:
  1. **Colour**: `[title] colour` from `#78d8f8` to `#ff00ff` (only the
     `[title]` section's key; `[help] colour` holds the same value and must
     not be touched).
  2. **Font size**: `[font.ps2p_14] size` from `14` to `16`. This font draws
     the title and the notice rows. It is chosen because it feeds none of
     the derived numbers: the page and the wrap width come from the list
     font (`ps2p_12`, also the message and footer font) and the help bound
     from the help font (`ps2p_10`), which `test_theme` ties to
     `UI_STR_HELP_LEN_MAX`. 16 is a size the theme already uses (the tabs),
     so the loader (8..64) and Press Start 2P's monospace advance accept it,
     and every notice row still fits within `right = 610` (the longest,
     "Base ROM is not the JP 1.0 ROM", is 30 × 16 = 480 px from x 30).
  3. **Row height**: `[list] row_height` from `24` to `32`. The selected
     bands (`h = 24`) stay within a row, so the loader accepts it; the page
     becomes (386 − 98) / 32 = 9 instead of 12, which the app derives at
     launch and hands to the core.
  4. `reverted`: the untouched theme again.
- **What each pass must show** (each comparison with `cmp` against `base`,
  state by state; the PRD writer ran these passes on a copy of the theme
  and saw exactly this, so a different result is a finding to report, not
  to tune away):
  - colour: the `.bmp` of the 11 states with a normal-tone title differ;
    `generation_failed` and `history_confirm` (alert title) are identical;
    every `.txt` is identical (the text renderer has no colours);
  - font size: all 13 `.bmp` differ (title and its rule's start, notice
    rows); every `.txt` identical (the core's page and width did not move);
  - row height: the list states' `.bmp` differ and every `.txt` differs
    (the text grid is page + 4 lines: 15 instead of 18); the notice states'
    `.bmp` are identical (notices have their own row height);
  - reverted: all 26 files byte-identical to `base`.
- **At the end**: the binary's sha256 and mtime are those recorded at the
  start; `git status --porcelain` is empty and `git diff --stat` prints
  nothing (so nothing under `tests/`, nor anywhere else, changed). The
  implementer looks at one capture of each edit (for example
  `generate_top.bmp`) to confirm the change is the expected one (magenta
  title; bigger title with the rule starting further right; spaced-out list
  rows with 9 of them).
- **Recorded**, for the step note: the three edits (section, key, old and
  new value), the binary's sha256, the per-pass lists of differing files,
  the reverted pass's byte identity, and the empty `git status`. The
  implementer puts this in its report; plan-pilot writes it into the step
  note. No file of the demonstration is committed.

## Implementation Chunks

1. **Ship alttp as the default theme.** Test first in `test_package.py`:
   `PKG_THEME` read from the Makefile (one line, else a failure naming the
   Makefile), pinned to `themes/alttp`, and `RealPackage` built from it with
   the byte-for-byte check (red while `PKG_THEME` is classic). Then
   `PKG_THEME := ../../themes/alttp` and the root `make help` line. Verified
   by `make test`, then `make package` (docker; `dockerd` may need starting
   by hand, step 1.1's note): the zip's `App/ALttPR/themes/default/` lists
   the 17 alttp files, `build_zip.py --check` accepts it, two runs give the
   same bytes (sha256 recorded with the zip size; step 1.6's classic zip was
   447607 bytes). One `feat:` commit.
2. **Docs follow.** `README.md`, `CLAUDE.md`, `notes/owner-todo.md`, and
   `targets/mac/`'s run target and README, as decided above. One `docs:`
   commit.
3. **The demonstration and the net.** No code, no commit: the procedure
   above, then `make check` (and `make -k check` on a failure). The
   recorded facts go into the implementer's report for the step note.

## Testing Decisions

- A good test here checks what ships, not how the builder works inside: the
  package self-test asserts the zip's theme entries equal the default theme
  folder's files byte for byte, and that the default theme is the one the
  Makefile ships; it does not assert anything about the theme's content
  (that is `test_theme`'s and `test_sdl_alttp`'s job).
- **Tested**: `tests/rando/test_package.py` (the `PKG_THEME` reading and
  pinning, `RealPackage` on it). Prior art: its own `RealPackage` and
  `Builder` cases, `run_builder` in-process.
- **Not given a new test**: the docs (prose); the demonstration (a one-off
  proof by hand, as the plan asks: turning it into a net test would mean
  editing the shipped theme from a test); `build_zip.py` (unchanged, its
  self-test already covers a theme folder of any content).
- **Unchanged and still run**: `test_theme` (alttp loads, every file of the
  folder is named by `theme.ini`, fonts are byte copies of `data/`, the help
  bound), `test_sdl_alttp` and `make sdl-pc`'s draw of the 13 states with
  alttp, the classic goldens (13 byte-identical), the smoke test on classic.
- **The one expected value changed on purpose**: the self-test's default
  theme, classic → alttp, the switch this step exists for; said in the
  commit message.
- Verification of the step: `make test`; `make package` (docker); `make
  sdl-pc` (goldens, in the pinned container, not a `setup:` line); the
  demonstration; `make check`.

## Out of Scope

- Any change to `themes/alttp/` or `themes/classic/` content: the
  demonstration's edits are reverted, and the tuning ended at step 12.
- Moving the SDL tests, the smoke test or the goldens to alttp: classic
  stays their theme (plan rule), and the goldens are pinned to it.
- Running the SDL app on the ARM build (under qemu or otherwise): the plan
  never launches the game or an emulator; the device look is an owner check.
- Cleaning stale theme files on the card at launch or in the zip.
- A theme picker, user themes or hot reload (rejected by the plan).
- The CI `sdl` artifact shipping a theme beside the PC binary (step 1.6's
  open note).
- Turning on the CI triggers.

## Further Notes

- The demonstration's numbers were checked by the PRD writer on copies of
  `themes/alttp/` in a scratch folder with the existing build (13 states
  drawn per pass): colour 11 BMPs differ and no text; font size 13 BMPs and
  no text; row height 8 BMPs (the list states) and all 13 texts, the text
  grid 15 lines instead of 18; the untouched theme drawn twice is
  byte-identical. The real run edits `themes/alttp/theme.ini` itself.
- The zip gets smaller: alttp's folder is about 133 kB against classic's
  356 kB (DejaVu Sans Mono and its larger background go).
- `goldens.sh --draw` names its summary line after the theme folder
  (`alttp`) and writes a `goldens-work` folder beside the binary, under
  `tests/rando/build/`, git-ignored.
- Step 14 (check-net) follows: this step's `make check` is the first run of
  the net with alttp shipped.
