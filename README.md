# ALttPR on Miyoo

An on-device *A Link to the Past Randomizer* seed generator for the **Miyoo
Mini Flip running OnionOS**: an Onion app, driven by the d-pad, that reads
your Japanese 1.0 ROM, generates a seed with every alttpr.com setting, and
writes a patched `.sfc` you play with Onion's own SNES core. One seed is one
ROM, so each seed keeps its own save. A History tab lists the seeds and
deletes a seed's ROM, save and save states. A PC build (SDL) shows the same
app on Linux and macOS.

The generator is a C port of VT (`sporchia/alttp_vt_randomizer`, frozen at
`219fcafd`), checked against VT itself. No Nintendo ROM is ever in this
repository.

Status: usable. The checks only the owner can make on a real Mini Flip are
listed in `notes/owner-todo.md`.

## Install

### What you need

- **A Miyoo Mini Flip on Onion `v4.5-dev` or later.** Only that branch knows
  the Flip (device 285); Onion's `main` does not. To check a card, run on the
  device:

  ```sh
  cat /mnt/SDCARD/.tmp_update/onionVersion/version.txt
  cat /tmp/deviceModel    # 285 means a Flip
  ```

  The facts behind this are in [`docs/onion.md`](docs/onion.md).
- **Your own ROM of *Zelda no Densetsu: Kamigami no Triforce*, Japanese
  1.0**, headerless: exactly 1,048,576 bytes, MD5
  `03a63945398191337e896e5771f77173`. The game you play is in English. A US
  ROM is not supported. A headered ROM (512 bytes longer) is refused as the
  wrong size; the app does not strip the header.

The ROM never leaves your card. Neither this repository nor the release zip
holds a Nintendo ROM: they carry only the base patch (`base.bps`), which the
app applies to your ROM.

### Steps

1. **Get `ALttPR.zip`.** There is no GitHub Release. Either run the CI job
   that builds it (GitHub → Actions → **check** → **Run workflow**; when its
   `package` job is green, download the run's artifact `alttpr-miyoo`, which
   GitHub wraps in a zip of its own, so unzip it once to get `ALttPR.zip`),
   or build it yourself with `make package` (see
   [Development](#development)).
2. **Unzip `ALttPR.zip` at the root of the SD card.** Its only top-level
   folder is `App/ALttPR/`, so this is the whole copy.
3. **Copy your ROM next to the app as `App/ALttPR/base.sfc`** (lower-case
   name).
4. Put the card back in the Flip and start **ALttPR** from Onion's Apps list.

The app creates what else it needs by itself at the first generation:
`Roms/SFC/ALttPR/` (the generated ROMs) and `App/ALttPR/history/` (the
History records). It writes `App/ALttPR/settings.json` (your last settings)
when you press Generate, and `App/ALttPR/alttpr.log` at each launch.

### Update

Unzip a newer `ALttPR.zip` at the root of the card again, over the old
`App/ALttPR/`. The zip holds no `base.sfc`, `history/` or `settings.json`,
so your ROM, your History and your last settings stay.

### Remove

Delete the `App/ALttPR/` folder. That also removes `base.sfc`, the History
records and the last settings. The generated ROMs in `Roms/SFC/ALttPR/` and
their saves and states stay on the card: delete the seeds from History before
removing the app, or delete those files by hand.

## Use

Start **ALttPR** from Onion's Apps list. The app has two tabs, **Generate**
and **History**; L1 and R1 switch between them.

Start, Select and Menu act when released, and not when pressed together with
another button, so Onion's own button combinations do not trigger them.

### Generate tab

The **Seed Settings** screen lists every alttpr.com setting, then
**Spoiler in History**, then **Generate Seed**.

| Button | Does |
|--------|------|
| Up / Down | Move to the previous / next row (wraps around) |
| Left / Right | Change the row's value (wraps around) |
| A | Next value; on **Generate Seed**, generate |
| Y | Put the row back to its default |
| L2 / R2 | Page up / down |
| Start | Generate, from any row |
| L1 / R1 | Switch to the History tab |
| B or Menu | Quit the app |

The rows and their values, in the order Right steps through them, as the app
shows them; the default is in bold. They mean what alttpr.com's options
mean; see [alttpr.com's options page](https://alttpr.com/en/options).

| Row | Values |
|-----|--------|
| Glitches | **None**, Overworld Glitches, Hybrid Major Glitches, Major Glitches, No Logic |
| Item Placement | **Advanced**, Basic |
| Dungeon Items | **Standard**, Map/Compass, Map/Compass/Keys, Keysanity |
| Accessibility | **100% Inventory**, 100% Locations, Beatable |
| Goal | **Defeat Ganon**, Fast Ganon, All Dungeons, Master Sword Pedestal, Triforce Hunt, Ganonhunt, Completionist |
| Tower Crystals | 0 to 6, **7**, Random |
| Ganon Crystals | 0 to 6, **7**, Random |
| World State | **Open**, Standard, Inverted, Retro |
| Swords | **Randomized**, Assured, Vanilla, Swordless |
| Item Pool | **Normal**, Hard, Expert, Crowd Control |
| Item Functionality | **Normal**, Hard, Expert |
| Hints | **On**, Off |
| Heart Beep | Off, **Half Speed**, Quarter Speed, Double Speed, Normal Speed |
| Spoiler in History | **Off**, On |

- **The seed is always random.** There is no seed entry and no seed sharing.
- **Your last settings are remembered**: they are saved when you press
  Generate and restored at the next launch. If the saved file cannot be
  read, the app starts with the defaults and says "Saved settings
  unreadable: defaults".
- **Spoiler in History** is off by default and starts off at every launch
  (it is not remembered). It never changes the ROM and is never written
  next to it: it only decides whether the seed's item locations are kept in
  its History record.

### Generating a seed

Press **Generate Seed** (or Start). The app checks your base ROM at this
moment, not at launch, so it starts even without `base.sfc`. A
**Generating Seed** screen ("This takes a few seconds") shows while it
works; there is no progress bar, and no button acts until it is done.

Then the **Seed Ready** screen shows "Saved in Roms/SFC/ALttPR as:" and the
ROM's name, "Spoiler kept in History" or "Spoiler not kept", and "Play it
from the SNES list". A or B goes back to the settings; Menu quits.

A ROM is named `<Mode>-<Goal>-<code>.sfc`, for example
`Open-Ganon-1CA10A.sfc`: the world state, the goal, and six hex digits
drawn from the seed and the settings. If that name is taken, the app adds
`-2`, `-3`… (`Open-Ganon-1CA10A-2.sfc`).

### Playing a seed

Quit the app and play the ROM from Onion's **SNES** list, folder `ALttPR`,
with Onion's default SNES core (Beetle Supafaust). The app is not involved
in playing.

The app clears the SNES list's cache after each generation so the list picks
up the new ROM. If the Seed Ready screen says "Refresh the SNES list to see
it", or the ROM is not in the list, highlight SNES in the Games tab and press
**Select › Refresh all roms**.

One seed is one ROM, and RetroArch names saves after the ROM's file name, so
each seed keeps its own save and save states: you can play several seeds in
parallel.

### When generation fails

The **Generation Failed** screen shows what went wrong and what to do.

| Message | What to do |
|---------|------------|
| Base ROM not found | Copy your JP 1.0 ROM to `App/ALttPR/base.sfc` (the screen says "Needs App/ALttPR/base.sfc (JP 1.0)") |
| Base ROM could not be read | Same: copy the ROM again |
| Base ROM has the wrong size | The file is not 1,048,576 bytes: use a headerless JP 1.0 ROM |
| Base ROM is not the JP 1.0 ROM | Its MD5 is not `03a63945…`: a US ROM or another version; use the JP 1.0 ROM |
| Base patch not found, Base patch could not be read, Base patch is too large, Patch refused: …, Base patch is for another ROM, Base patch gave the wrong ROM | `App/ALttPR/base.bps` is missing or damaged: "Reinstall App/ALttPR from the zip" |
| Could not create the ROM folders, Seed could not be saved, History record write failed, SD card write failed | "Check that the SD card is writable" (not full, not read-only) |
| Generator gave up on this seed, ROM could not be built, No free file name for this seed | "Press A and generate again" |

If the app does not start, or quits at once, `App/ALttPR/alttpr.log`
(rewritten at each launch) says why; include it when you report the problem.

### History tab

**Generated Seeds** lists the seeds, newest first, by ROM name; the bottom
line shows the selected seed's date ("Generated 2026-11-02 14:05"). With no
seed yet it says "No seeds yet".

| Screen | Button | Does |
|--------|--------|------|
| List | Up / Down | Move (wraps around) |
| List | L2 / R2 | Page up / down |
| List | A | Open the seed's details |
| List | X | Delete the seed (asks first) |
| Details | Up / Down | Scroll one line |
| Details | L2 / R2 | Scroll one page |
| Details | B | Back to the list |
| Details | X | Delete the seed (asks first) |
| Confirmation | A | Delete |
| Confirmation | B | Keep, and go back |
| List, Details | L1 / R1 | Switch to the Generate tab |
| List | B | Quit the app |
| Any | Menu | Quit the app |

The details show the date, the seed number, every setting, Spoiler in
History, then the spoiler (item locations) if it was kept, or "No spoiler
kept for this seed".

History lists only seeds whose ROM is still in `Roms/SFC/ALttPR/`; a seed
whose ROM is gone is hidden (see [below](#deleting-a-rom-by-hand)). Notices
at the bottom may say "History folder could not be read", "Some History
records are unreadable", or "Showing the newest 256 seeds" (older ones are
not listed).

Each record is `App/ALttPR/history/<code>.json`: the settings, the date, the
seed, and the spoiler when it was kept.

## Deleting a seed

Press X on a seed in History (the list or its details). The confirmation
says "Delete <code>?", the ROM name, "Removes the ROM, its save and states"
and "This cannot be undone". **Only A deletes; B keeps.** There is no undo.

### What Delete removes, in order

1. The ROM, `Roms/SFC/ALttPR/<name>.sfc`.
2. Its per-ROM core override, `Roms/SFC/ALttPR/.game_config/<name>.cfg`, if
   you made one.
3. In each of `Saves/CurrentProfile`, `Saves/MainProfile` and
   `Saves/GuestProfile` that exists: the save
   `saves/Supafaust/<name>.srm`, and every file of `states/Supafaust/` whose
   name starts with `<name>.state` (every slot `.state`, `.state1`…, the auto
   state `.state.auto`, and their `.png` thumbnails).
4. The History record, `App/ALttPR/history/<code>.json`.
5. The SNES list's cache, `Roms/SFC/SFC_cache6.db` and `SFC_cache2.db`, so
   the list forgets the ROM. If the cache cannot be removed, the app says
   "Seed deleted: refresh the SNES list": refresh it as in
   [Playing a seed](#playing-a-seed). Otherwise it says "Seed deleted".

`<name>` is the ROM's file name without `.sfc`, for example
`Open-Ganon-1CA10A`.

### What Delete never touches

- **Other seeds**, including a `-2` sibling: `Open-Ganon-1CA10A-2.state`
  does not start with `Open-Ganon-1CA10A.state`.
- Any other folder of `Roms/SFC/`.
- Saves made with **another core**: if you changed this ROM's core, its save
  is under that core's folder, not `Supafaust`; delete it by hand.
- RetroArch's history, Onion's Recents, game-switcher screenshots and play
  activity. They may still list the deleted seed.

**Warning: a copy shares the save.** RetroArch names saves by the ROM's file
name only, whatever its folder. If you copied a generated ROM elsewhere in
`Roms/SFC/` under the same file name, that copy uses the same save and
states, and Delete removes them too.

### When a delete is only partial

- "ROM could not be deleted": nothing else was touched; the seed stays in
  the list and you can press X again.
- "Some save files were not deleted" or "History record was not deleted":
  the ROM is gone, the record is kept, and the seed stays in the list while
  the app runs, so you can press X again to finish. After the app restarts
  (or after a new generation) the seed is hidden because its ROM is gone;
  the leftover files then have to be removed by hand.

### Deleting a ROM by hand

If you delete a generated ROM with Onion's file manager, the seed disappears
from History at the next start; its record stays in `App/ALttPR/history/`,
and the seed reappears if you put the ROM back under the same name. Its save
and states stay on the card: only History's Delete removes them.

### Profiles

Seeds, History and `base.sfc` are shared by all of Onion's profiles; saves
are per profile. Delete clears the seed's save and states in every profile
folder present (Current, Main, Guest), so switching to or from Guest Mode
holds no leftover save of a deleted seed.

## Development

Needs only `gcc`, `make` and `python3`; nothing to install.

```bash
make test
```

```bash
make check
```

`make test` is the fast loop (unit tests); `make check` is everything that
must be green before a push.

`make check` (through `e2e`) also builds the host CLI `tests/rando/build/rando_patch`, which
writes a seeded ROM from your own headerless JP 1.0 ROM and the base patch:

```bash
tests/rando/build/rando_patch --base <jp.sfc> --out <file> [--bps <patch>] [--seed <n>] [key=value ...]
```

The keys are the case format's (`goal=triforce-hunt`, `hints=off`,
`heart_beep=quarter`, ...); a missing key keeps VT's default, `--bps`
defaults to `data/base.bps` and a missing `--seed` is drawn at random.

`make e2e-arm` cross-builds the generator with Onion's own toolchain (a
pinned docker image, see `docs/toolchain.md`) and runs the e2e cases on it
under `qemu-arm`; `make unit-arm` does the same with every unit test. Both
need docker and `qemu-user`, and are not part of `make check`.

`make package` builds the Miyoo release: it runs `make sdl-arm`, strips the
binary with Onion's toolchain, checks it with `file` and `readelf` (32-bit
ARM EABI5, hard-float, dynamically linked against `libSDL-1.2.so.0` and
`libSDL_ttf-2.0.so.0`), re-checks `data/base.bps`, and writes
`tests/rando/build/package/ALttPR.zip`; the same commit gives the same zip
bytes. It needs docker and is not part of `make check`. The zip's
`App/ALttPR/` holds `alttpr`, `launch.sh`, `config.json` and `icon.png`
(from `app/package/`), and `base.bps`, `DejaVuSansMono.ttf` and
`DejaVuSansMono-LICENSE.txt` (from `data/`). How `data/base.bps` was built
is in step 0.1's note of `docs/plan-1.md`.

CI (`.github/workflows/check.yml`) runs only by hand (**Run workflow**): its
job `check` runs `make -k check`, its job `sdl` runs `make sdl` and uploads
the PC binary as the artifact `alttpr-pc`, and its job `package` runs
`make package` then `make e2e-arm` and uploads the zip as the artifact
`alttpr-miyoo`.

To try the app on a PC before copying it (needs `libsdl1.2-dev` and
`libsdl-ttf2.0-dev`):

```bash
make sdl-pc
tests/rando/build/sdl/alttpr --sd tmp/sd --font data/DejaVuSansMono.ttf
```

`tmp/sd` stands for the SD card. To generate there, put your ROM at
`tmp/sd/App/ALttPR/base.sfc` and a copy of `data/base.bps` at
`tmp/sd/App/ALttPR/base.bps`. The keyboard follows Onion's button map:

| Button | Key |
|--------|-----|
| d-pad | Arrow keys |
| A / B / X / Y | Space / Left Ctrl / Left Shift / Left Alt (also a / b / x / y) |
| L1 / R1 | E / T |
| L2 / R2 | Tab / Backspace (also Page Up / Page Down) |
| Start / Select / Menu | Enter / Right Ctrl / Esc |

## Working with Claude

### Skills you run

| Skill | Use it to |
|-------|-----------|
| `/grill-me` | Get interviewed about a plan or design until every decision is made |
| `/plan-write` | Turn that conversation into `docs/plan-<N>.md` |
| `/plan-converge <plan>` | Sharpen the plan with review rounds until they come back empty |
| `/plan-pilot <plan>` | Implement the whole plan unattended, step by step, then open the PR |
| `/fix-bug <symptom>` | Fix one bug: proven cause, failing test, minimal fix, independent check |

Run `plan-converge` and `plan-pilot` each in a **new session**: the plan file
is all they know, so everything decided while grilling must be in it.

### Skills the others call

`to-prd` (one PRD per plan step), `prd-autopilot` (implements the PRD queue),
`tdd` (red-green-refactor), `check-net` (runs `make check`, drives failures
back to green), `help-me-debug` (proves a bug's cause), `git-commit`,
`plan-review`, `warn-review` (design smells at each phase end), `guideline`
(the stand-in human during unattended runs).

Follow a running plan by reading `progress.md` at the root of its branch.
