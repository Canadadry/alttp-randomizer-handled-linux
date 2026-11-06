# ALttPR on Miyoo

An on-device *A Link to the Past Randomizer* seed generator for the **Miyoo
Mini Flip running OnionOS**: an Onion app, driven by the d-pad, that reads
your Japanese 1.0 ROM, generates a seed with every alttpr.com setting, and
writes a patched `.sfc` you play with Onion's own SNES core. One seed is one
ROM, so each seed keeps its own save. A History tab lists the seeds and
deletes a seed's ROM, save and save states.

<table>
  <tr>
    <td><img src="docs/screenshots/generate_top.png" width="320" alt="Seed Settings"><br>Seed Settings</td>
    <td><img src="docs/screenshots/seed_ready.png" width="320" alt="Seed Ready"><br>Seed Ready</td>
  </tr>
  <tr>
    <td><img src="docs/screenshots/history_list.png" width="320" alt="History"><br>History</td>
    <td><img src="docs/screenshots/history_detail_spoiler.png" width="320" alt="Seed details"><br>Seed details</td>
  </tr>
</table>

The generator is a C port of VT (`sporchia/alttp_vt_randomizer`, frozen at
`219fcafd`), checked against VT itself. No Nintendo ROM is ever in this
repository.

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
  ROM is not supported, and a headered ROM (512 bytes longer) is refused.

The ROM never leaves your card. The release zip holds only the base patch
(`base.bps`), which the app applies to your ROM.

### Steps

1. **Get `ALttPR.zip`.** There is no GitHub Release. Either run the CI job
   that builds it (GitHub → Actions → **check** → **Run workflow**; when its
   `package` job is green, download the run's artifact `alttpr-miyoo`, which
   GitHub wraps in a zip of its own, so unzip it once to get `ALttPR.zip`),
   or build it yourself with `make package` (needs docker).
2. **Unzip `ALttPR.zip` at the root of the SD card.** Its only top-level
   folder is `App/ALttPR/`.
3. **Copy your ROM next to the app as `App/ALttPR/base.sfc`** (lower-case
   name).
4. Start **ALttPR** from Onion's Apps list.

### Update and remove

To update, unzip a newer `ALttPR.zip` at the root of the card, over the old
`App/ALttPR/`: your ROM, History and last settings stay.

To remove, delete `App/ALttPR/`. The generated ROMs in `Roms/SFC/ALttPR/`
and their saves stay on the card: delete the seeds from History first, or
remove those files by hand.

## Use

The app has two tabs, **Generate** and **History**; L1 and R1 switch between
them. Start, Select and Menu act when released, and not when pressed
together with another button, so Onion's own combinations do not trigger
them.

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

The rows and their values, in the order Right steps through them; the
default is in bold. They mean what
[alttpr.com's options](https://alttpr.com/en/options) mean.

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

- **The seed is always random**: there is no seed entry and no seed sharing.
- **Your last settings are remembered** when you press Generate, and
  restored at the next launch.
- **Spoiler in History** starts off at every launch. It never changes the
  ROM: it only decides whether the seed's item locations are kept in its
  History record.

### Generating and playing a seed

Press **Generate Seed** (or Start). A **Generating Seed** screen shows for a
few seconds, then **Seed Ready** gives the ROM's name, for example
`Open-Ganon-1CA10A.sfc` (world state, goal, six hex digits; `-2`, `-3`… if
the name is taken).

Quit the app and play the ROM from Onion's **SNES** list, folder `ALttPR`,
with the default SNES core (Beetle Supafaust). If the ROM is not in the
list, highlight SNES in the Games tab and press **Select › Refresh all
roms**. Each seed keeps its own save and save states, so you can play
several seeds in parallel.

### When generation fails

The **Generation Failed** screen shows what went wrong and what to do.

| Message | What to do |
|---------|------------|
| Base ROM not found, Base ROM could not be read | Copy your JP 1.0 ROM to `App/ALttPR/base.sfc` |
| Base ROM has the wrong size | The file is not 1,048,576 bytes: use a headerless JP 1.0 ROM |
| Base ROM is not the JP 1.0 ROM | A US ROM or another version: use the JP 1.0 ROM |
| Base patch not found, could not be read, is too large, is for another ROM, gave the wrong ROM; Patch refused: … | `App/ALttPR/base.bps` is missing or damaged: reinstall `App/ALttPR` from the zip |
| Could not create the ROM folders, Seed could not be saved, History record write failed, SD card write failed | Check that the SD card is writable (not full, not read-only) |
| Generator gave up on this seed, ROM could not be built, No free file name for this seed | Press A and generate again |

If the app does not start, or quits at once, `App/ALttPR/alttpr.log`
(rewritten at each launch) says why; include it when you report a problem.

### History tab

**Generated Seeds** lists the seeds, newest first, with the selected seed's
date at the bottom. The details show the date, the seed number, every
setting, then the spoiler if it was kept.

| Screen | Button | Does |
|--------|--------|------|
| List | Up / Down, L2 / R2 | Move, page up / down |
| List | A | Open the seed's details |
| Details | Up / Down, L2 / R2 | Scroll a line, a page |
| Details | B | Back to the list |
| List, Details | X | Delete the seed (asks first) |
| Confirmation | A / B | Delete / Keep |
| List, Details | L1 / R1 | Switch to the Generate tab |
| List | B | Quit the app |
| Any | Menu | Quit the app |

History lists only seeds whose ROM is still in `Roms/SFC/ALttPR/`: if you
delete a ROM by hand, its seed is hidden, but its save and states stay on
the card.

## Deleting a seed

Press X on a seed in History, then **A** to delete (B keeps). There is no
undo. Delete removes the ROM, its core override if you made one, its save
and save states in every Onion profile (Current, Main, Guest), its History
record, and the SNES list's cache so the list forgets the ROM.

It never touches other seeds (a `-2` sibling included), other folders of
`Roms/SFC/`, saves made with another core, nor RetroArch's history and
Onion's Recents.

**Warning: a copy shares the save.** RetroArch names saves by the ROM's file
name only. If you copied a generated ROM elsewhere in `Roms/SFC/` under the
same name, Delete removes that copy's save and states too.

If a delete is only partial ("ROM could not be deleted", "Some save files
were not deleted", "History record was not deleted"), the seed stays in the
list while the app runs: press X again to finish.

## Development

Needs only `gcc`, `make` and `python3`.

```bash
make test
```

```bash
make check
```

`make test` is the fast loop; `make check` is everything that must be green
before a push. To see the app on a PC (needs `libsdl1.2-dev`,
`libsdl-ttf2.0-dev` and `libsdl-image1.2-dev`):

```bash
make sdl-pc
```

```bash
tests/rando/build/sdl/alttpr --sd tmp/sd --theme themes/alttp
```

`tmp/sd` stands for the SD card: put your ROM at `tmp/sd/App/ALttPR/base.sfc`
and a copy of `data/base.bps` beside it. The keyboard follows Onion's button
map: arrows for the d-pad, Space / Left Ctrl / Left Shift / Left Alt for
A / B / X / Y, E / T for L1 / R1, Tab / Backspace for L2 / R2, Enter /
Right Ctrl / Esc for Start / Select / Menu. `make screenshots` redraws the
pictures above into `docs/screenshots/`.

Every other target (cross build, release zip, themes, mockups) and the
project's rules are in [`CLAUDE.md`](CLAUDE.md); `make help` lists the
targets.

### Working with Claude

Run `/grill-me` to be interviewed about a plan, `/plan-write` to turn it
into `docs/plan-<N>.md`, then `/plan-converge <plan>` and
`/plan-pilot <plan>`, each in a **new session**: the plan file is all they
know. `/fix-bug <symptom>` fixes one bug end to end. Follow a running plan
in `progress.md` at the root of its branch.
