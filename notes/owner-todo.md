# Owner to-do — plan 1 (ALttPR on Miyoo)

Branch `plan-1-miyoo-package` (not `plan-1-onion-generator`). Delete this
file once every item is done.

## 1. Build and commit the base patch — done, one confirmation left

`data/base.bps` is committed (`e13553b`) and `make test` checks it (sizes
1,048,576 -> 2,097,152, source CRC32 `3322EFFC`). Left: confirm that the
patched `work.sfc`'s MD5 was `edc01f3db798ae4dfe21101311598d44`; the agent
cannot check it without the ROM.

## 2. Run the `sdl` CI job

GitHub → Actions → **check** → **Run workflow**. It builds the SDL app for the
PC and for ARM and uploads the PC binary (`alttpr-pc`). Report whether it is
green.

## 3. Run the `package` CI job

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
  `tests/rando/build/sdl/alttpr --sd tmp/sd --font data/DejaVuSansMono.ttf`.

## 6. Decisions left to the owner (optional)

- The **Spoiler** option is not remembered: it starts off at every launch,
  while the other settings are restored. Say if it should be remembered.
- The PC build was not checked on macOS.

## 7. Clean-up (optional)

`rm -rf /tmp/test_history_*` on the session machine (three trees left by
failed test runs; the permission check refused the agent's delete).
