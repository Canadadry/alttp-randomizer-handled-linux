# Owner to-do — plan 1 (ALttPR on Miyoo)

Branch `plan-1-miyoo-package` (not `plan-1-onion-generator`). Plan 1 was
closed with the plan's section 4 owner checks waived, not done: §1's MD5
confirmation, §4 and §5 are still open (§2 and §3 done since). Delete this file once every
item is done.

## 1. Build and commit the base patch — done, one confirmation left

`data/base.bps` is committed (`e13553b`) and `make test` checks it (sizes
1,048,576 -> 2,097,152, source CRC32 `3322EFFC`). Left: confirm that the
patched `work.sfc`'s MD5 was `edc01f3db798ae4dfe21101311598d44`; the agent
cannot check it without the ROM.

## 2. Run the `sdl` CI job — done

Green on 2026-11-05 at `5075e75` (run https://github.com/Canadadry/alttp-randomizer-handled-linux/actions/runs/42024695341), artifact `alttpr-pc`.

GitHub → Actions → **check** → **Run workflow**. It builds the SDL app for the
PC and for ARM and uploads the PC binary (`alttpr-pc`). Report whether it is
green.

## 3. Run the `package` CI job — done

Green on 2026-11-05 at `5075e75` (same run: `make package`, `make e2e-arm`,
`check` job too), artifact `alttpr-miyoo`, kept until 2027-02-03.

GitHub → Actions → **check** → **Run workflow**. Its `package` job builds the
Miyoo release with `make package`, runs `make e2e-arm` (the cross-built
generator on every e2e case under qemu-arm) and uploads the zip as
`alttpr-miyoo`. Report whether it is green. Locally, `make package` (docker
needed) writes the same zip to `tests/rando/build/package/ALttPR.zip`.

To install it for the plan §4 hand checks (item 5): unzip `ALttPR.zip` at the SD card's
root (it holds only `App/ALttPR/`), and put the JP 1.0 ROM next to the app as
`App/ALttPR/base.sfc`.

## 4. Check the Mini Flip (end of `docs/onion.md`)

The device needs an Onion **`v4.5-dev`** build: only that branch knows the
Flip, `main` does not.

1. `cat /mnt/SDCARD/.tmp_update/onionVersion/version.txt` and
   `cat /tmp/deviceModel` (285 means a Flip).
2. `cat /tmp/screen_resolution`; does `/tmp/new_res_available` exist?
3. Is an existing SNES ROM's save folder named `Supafaust`
   (`Saves/CurrentProfile/saves/Supafaust/`)?
4. Does the card's `retroarch.cfg` keep Onion's save and state values (sort by
   core, auto-index), with no override in `config/Supafaust/`?
5. Does a ROM added under `Roms/SFC/ALttPR/` show after a list refresh (or once
   `Roms/SFC/SFC_cache6.db` is deleted)?
6. Is an icon stored inside `App/ALttPR/` shown in the Apps list?
7. After deleting a seed from History, what do Recents and the game switcher
   show?

## 5. Hand checks with the zip installed (plan §4)

- The app runs on the Mini Flip, generates a seed, and the ROM boots and plays
  to at least the first dungeon.
- Time one generation with `item_placement-basic`'s settings (Generate pressed
  → "Seed Ready" screen); the estimate is ≤ 3.4 s.
- Two seeds keep separate saves.
- Deleting a seed from History removes the ROM, its save and its states.
- The PC build looks right: `make sdl-pc`, then
  `tests/rando/build/sdl/alttpr --sd tmp/sd --theme themes/alttp`.
- The app on the Mini Flip draws the alttp look: the notched frame, the
  sprites (triforces, arrows, buttons), Press Start 2P and the help line under
  the list. Nothing in the net runs the SDL app on the device.
- On the Mac: run `make -C targets/mac run` and report whether it builds
  (SDL_image is now built from source there, a recipe the agent checked on
  Linux only) and whether it draws the app.

## 6. Decisions left to the owner (optional)

- The **Spoiler** option is not remembered: it starts off at every launch,
  while the other settings are restored. Say if it should be remembered.
- The PC build was not checked on macOS.

## 7. Clean-up (optional)

`rm -rf /tmp/test_history_*` on the session machine (three trees left by
failed test runs; the permission check refused the agent's delete).

# Plan 2 — the alttp theme's look

## 8. Judge the alttp theme against your mockups

Run `make theme-compare` (needs `make sdl-pc`'s packages, node and
Playwright's Chromium). It writes nine images to
`tests/rando/build/theme-compare/`:

- `00_settings_help.png`
- `01_settings_greyed_error.png`
- `02_generating.png`
- `03_seed_ready.png`
- `04_generation_failed.png`
- `05_history_list.png`
- `06_history_empty.png`
- `07_seed_detail_spoiler.png`
- `08_confirm_delete.png`

Each one has three panels, left to right: your **mockup** rendered by
Chromium, the **app**'s capture with `themes/alttp`, and the **diff** (the
mockup in dim grey where both are equal, magenta where they differ).

What the measures say (step 2.12's last pass):

- 01–08: all three criteria hold. No pixel differs outside the texts
  (frame, bands, sprites and title rule are exact), every text is at its
  mockup position (offset 0), and every text's colour is found. What is
  left in the diff panel is glyph edges: SDL_ttf and Chromium antialias
  Press Start 2P differently.
- 00 is judged on its frame, tabs, title and help line only, because its
  rows and footer are the 16 px variant and the app plays the 12 px theme.
  Its frame and tabs are exact. Its title and title rule sit 1 px lower in
  the mockup (baseline 83, rule at 78) than in 01–08 (82, 77); one theme
  serves every screen, so the theme follows 01–08. The help line keeps the
  message zone's place (x 30, baseline 412) instead of 00's x 34, baseline
  418, as plan 2 decided; x 34 would cut the help to 57 characters.

The question: does the look match your mockups, and which of the
differences matter to you?
