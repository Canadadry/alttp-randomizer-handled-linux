# Owner to-do — plan 1 (ALttPR on Miyoo)

Branch `plan-1-onion-generator`. The run stopped at Phase 4 step 2 (Miyoo
package): `data/base.bps` is missing. Delete this file once every item is done.

## 1. Build and commit the base patch — blocking

On a PC, with the JP 1.0 ROM as `jp.sfc` (headerless, MD5
`03a63945398191337e896e5771f77173`), and `asar` and `flips` installed:

```sh
git clone https://github.com/KatDevsGames/z3randomizer
git -C z3randomizer checkout dcb0a2b42d14445f7994a0e9e4d63cbecf4b98d3
cp jp.sfc work.sfc && truncate -s 2097152 work.sfc
(cd z3randomizer && asar --fix-checksum=off LTTP_RND_GeneralBugfixes.asm ../work.sfc)
md5sum work.sfc        # must print edc01f3db798ae4dfe21101311598d44
flips --create --bps jp.sfc work.sfc base.bps
```

Then, in the repository:

```sh
git checkout plan-1-onion-generator
mkdir -p data && cp /path/to/base.bps data/base.bps
make patch-header BPS=data/base.bps   # must pass (1,048,576 -> 2,097,152 bytes)
git add data/base.bps
git commit -m "chore: add the base patch for build edc01f3d"
git push
```

Then confirm that `work.sfc`'s MD5 was `edc01f3d…`. The run resumes at Phase 4
step 2 (cross build, `App/ALttPR/` zip, CI job), then Phase 5 step 2 (README),
then opens the pull request.

## 2. Run the `sdl` CI job

GitHub → Actions → **check** → **Run workflow**. It builds the SDL app for the
PC and for ARM and uploads the PC binary (`alttpr-pc`). Report whether it is
green.

## 3. Check the Mini Flip (end of `docs/onion.md`)

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

## 4. Hand checks once the zip exists (plan §4)

- The app runs on the Mini Flip, generates a seed, and the ROM boots and plays
  to at least the first dungeon.
- Time one generation with `item_placement-basic`'s settings (Generate pressed
  → "Seed Ready" screen); the estimate is ≤ 3.4 s.
- Two seeds keep separate saves.
- Deleting a seed from History removes the ROM, its save and its states.
- The PC build looks right: `make sdl-pc`, then
  `tests/rando/build/sdl/alttpr --sd tmp/sd --font data/DejaVuSansMono.ttf`.

## 5. Decisions left to the owner (optional)

- The **Spoiler** option is not remembered: it starts off at every launch,
  while the other settings are restored. Say if it should be remembered.
- The PC build was not checked on macOS.

## 6. Clean-up (optional)

`rm -rf /tmp/test_history_*` on the session machine (three trees left by
failed test runs; the permission check refused the agent's delete).
