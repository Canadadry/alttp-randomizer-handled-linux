# Onion facts

What the app needs from OnionOS, its menu and its RetroArch, each fact cited
from the source at a pinned commit; section 11 adds the Game Boy Color list
for plan 3's LADX seeds. The cross toolchain is in
[`docs/toolchain.md`](toolchain.md) and is not repeated here.

## Pinned sources

- **Onion `main`** at
  [`07505ea58c7bba698d6b9220ff43946a43cac76b`](https://github.com/OnionUI/Onion/tree/07505ea58c7bba698d6b9220ff43946a43cac76b)
  (2026-01-21). Every fact below is cited here unless marked otherwise.
- **Onion `v4.5-dev`** at
  [`2bab767e4a17b1b489f02c9fb6af8dde310d4c3e`](https://github.com/OnionUI/Onion/tree/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e)
  (2026-02-07), the branch with Mini Flip support. Both were still the branch
  heads on 2026-11-01.
- **How both are checked.** `git diff --stat 07505ea5 2bab767e` lists 19
  files. Of the files cited below, only these differ: `Makefile`,
  `src/common/system/device_model.h`, `src/common/system/keymap_hw.h`,
  `src/common/system/settings.h`, `src/keymon/keymon.c`, `src/tweaks/tweaks.c`,
  `static/build/.tmp_update/runtime.sh`,
  `static/dist/miyoo/app/.tmp_update/install.sh`. Any other cited file is
  byte-identical at both commits, so one link covers both. For the eight
  listed files, a fact gets a second link at `2bab767e` (the line numbers
  move), or is flagged **differs on v4.5-dev**.
- **RetroArch.** Onion builds it from the submodule `third-party/RetroArch-patch`,
  at
  [`OnionUI/RetroArch-patch@f9e959f7445d2ba0a4dd6279da41a095163767f2`](https://github.com/OnionUI/RetroArch-patch/tree/f9e959f7445d2ba0a4dd6279da41a095163767f2)
  on both Onion commits (`git ls-tree <commit> third-party/RetroArch-patch`;
  [`.gitmodules`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/.gitmodules#L10-L12)).
  That repository copies its own submodule `submodules/RetroArch`, pinned at
  [`libretro/RetroArch@69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576`](https://github.com/libretro/RetroArch/tree/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576)
  (2025-11-20, `PACKAGE_VERSION "1.22.2"`), applies `patches/*.patch` and builds with
  `Makefile.miyoomini`
  ([`Makefile` lines 37–49, 63–76](https://github.com/OnionUI/RetroArch-patch/blob/f9e959f7445d2ba0a4dd6279da41a095163767f2/Makefile#L37-L76);
  [`.gitmodules`](https://github.com/OnionUI/RetroArch-patch/blob/f9e959f7445d2ba0a4dd6279da41a095163767f2/.gitmodules#L1-L3)).
  No patch changes how save and state paths are built: `runloop_path_set_redirect`,
  `runloop_path_set_basename` and `fill_pathname_dir` appear in none of them,
  and `runloop_get_savestate_path` is only called (patch 00007).
- **The SNES core.** The shipped
  `static/build/RetroArch/.retroarch/cores/mednafen_supafaust_libretro.so`
  contains the strings `Supafaust` and `1.29.0 75c658c`, which are its
  `library_name` and its `library_version` (`MEDNAFEN_VERSION GIT_VERSION`).
  The source at that commit:
  [`libretro/supafaust@75c658cce454e58ae04ea252f53a31c60d61548e`](https://github.com/libretro/supafaust/tree/75c658cce454e58ae04ea252f53a31c60d61548e)
  (2022-12-12).
- **The GBC core.** The shipped
  `static/build/RetroArch/.retroarch/cores/gambatte_libretro.so` contains the
  strings `Gambatte` and `v0.5.0 04e73b8`, its `library_name` and its
  `library_version` (`"v0.5.0" GIT_VERSION`, `GIT_VERSION` being
  `git rev-parse --short HEAD`). `04e73b8` is not in
  `libretro/gambatte-libretro`: it is Onion's fork,
  [`schmurtzm/gambatte-libretro@04e73b8215601b61fd414c66ab20db5a99d65001`](https://github.com/schmurtzm/gambatte-libretro/tree/04e73b8215601b61fd414c66ab20db5a99d65001)
  (2023-04-12, "fix palette struct name", branch
  `advanced-color-organisation`), the fork Onion's GBC page names
  ([`06-nintendo-gbc.md` line 16](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/04-emulators/03-handhelds/06-nintendo-gbc.md#L16)).
  The binary was last changed by Onion commit
  [`d937d44f`](https://github.com/OnionUI/Onion/commit/d937d44f4beb4d682cf20ac50a062d6eab8c9880)
  (2023-05-29) and is the same file at both Onion commits.

## 1. Device and Onion version

- **`main` knows two models: the Mini (283) and the Mini Plus (354).**
  `#define MIYOO283 283`, `#define MIYOO354 354`
  ([`src/common/system/device_model.h` lines 7–8](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/device_model.h#L7-L8)).
  `runtime.sh` picks 354 when `axp 0` succeeds, 283 otherwise, and writes
  the id to `/tmp/deviceModel`
  ([`runtime.sh` lines 15–18](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L15-L18)).
  `main` has no Mini Flip id and no Flip detection.
- **Only `v4.5-dev` knows the Mini Flip (285).** It adds `#define MIYOO285 285`
  and `IS_MIYOO_PLUS_OR_FLIP()`
  ([`device_model.h` lines 7–13 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/common/system/device_model.h#L7-L13)),
  from commit
  [`9bf8ccd3`](https://github.com/OnionUI/Onion/commit/9bf8ccd3c4621bd543d6b22080ffdd8bbfa68a0e)
  ("initial Miyoo Mini Flip compatibility", PR #1860), plus the Flip MainUI
  binaries of
  [`edfc7a6b`](https://github.com/OnionUI/Onion/commit/edfc7a6bd87d06073ad977039da7bd3b69298343)
  (`static/build/.tmp_update/bin/MainUI-285-clean`, `MainUI-285-expert`).
- **Flip detection (v4.5-dev): the hall sensor, or a second input device.**
  `if [ -e /sys/devices/soc0/soc/soc:hall-mh248/hallvalue ] || [ -e /dev/input/event1 ]; then export DEVICE_ID=$MODEL_MMF`,
  then `axp 0` for 354, else 283
  ([`runtime.sh` lines 16–24 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L16-L24));
  the installer has the same test
  ([`install.sh` lines 145–155 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/dist/miyoo/app/.tmp_update/install.sh#L145-L155)).
  On the Flip, RetroArch is the Mini Plus build: `retroarch_miyoo354` is
  bind-mounted over `retroarch` for 285 and 354
  ([`runtime.sh` lines 115–120 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L115-L120);
  354 only on `main`:
  [lines 103–106](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L103-L106)).
  Both binaries come from the same RetroArch source (`MIYOO354=1` is a build flag,
  [RetroArch-patch `Makefile` lines 70–76](https://github.com/OnionUI/RetroArch-patch/blob/f9e959f7445d2ba0a4dd6279da41a095163767f2/Makefile#L70-L76)).
- **The Onion version is in `/mnt/SDCARD/.tmp_update/onionVersion/version.txt`.**
  The build writes `v$(VERSION)` there, and RetroArch's sub-version to
  `/mnt/SDCARD/RetroArch/onion_ra_version.txt`
  ([`Makefile` lines 74–76](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L74-L76),
  [line 185](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L185);
  same lines at `2bab767e`). **Differs on v4.5-dev**: `VERSION=4.4.0-beta-20260120`,
  `RA_SUBVERSION=1.22.2-1` on `main`
  ([`Makefile` lines 4–5](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L4-L5)),
  `VERSION=4.4.0-beta-20260117`, `RA_SUBVERSION=1.22.2` on `v4.5-dev`
  ([lines 4–5 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/Makefile#L4-L5)).
  A CI build may replace `VERSION` with `VERSION_OVERRIDE`
  ([lines 9–11](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L9-L11)),
  so the file on the card, not the branch, says which build runs.

## 2. App folder

- **The Apps list is drawn by MainUI, a binary**
  (`static/build/.tmp_update/bin/MainUI-*`), so its own scan is not in the
  sources. Onion's C code scans the same way wherever it lists apps (Tweaks,
  the menu-button shortcut): a directory under `/mnt/SDCARD/App/` counts only
  if it holds a `config.json`, and its name in lists is the `label` key
  ([`src/common/utils/apps.h` lines 25–35, 52–67](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/apps.h#L25-L67)).
  The app sorter treats a folder the same way (`if [ -f "$dir/config.json" ]`,
  [`app_sorter.sh` lines 3–7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/script/app_sorter.sh#L3-L7)).
- **`config.json` keys: `label`, `icon`, `iconsel`, `launch`, `description`.**
  Tweaks: `"label": "Tweaks"`, `"icon": "../../Icons/Default/app/tweaks.png"`,
  `"launch": "launch.sh"`, `"description": "System tweaks and tools"`
  ([`App/Tweaks/config.json`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Tweaks/App/Tweaks/config.json#L1-L6)).
  `launch` is read for the start command
  ([`apps.h` lines 126–130](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/apps.h#L126-L130));
  `icon` and `iconsel` are written by icon packs (below).
- **`icon` may be absolute or relative to the app folder.** Every packaged app
  uses either `"/mnt/SDCARD/Icons/Default/app/battery_monitor.png"`
  ([Battery Monitor](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Battery%20Monitor/App/BatteryMonitorUI/config.json#L3))
  or `"../../Icons/Default/app/..."`, which reaches `/mnt/SDCARD/Icons` only
  from `App/<Name>/`
  ([Guest Mode](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Guest%20Mode/App/Guest_Mode/config.json#L3)).
  No packaged app keeps its icon inside its own folder, so that case is
  **not stated in the sources**; an absolute path such as
  `/mnt/SDCARD/App/ZeldaRando/icon.png` follows a form Onion ships.
- **`launch.sh` runs from the app folder.** The start command is
  `cd <app dir>; chmod a+x ./<launch>; LD_PRELOAD=/mnt/SDCARD/miyoo/app/../lib/libpadsp.so ./<launch>`
  ([`apps.h` lines 132–138](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/apps.h#L132-L138)),
  and `runtime.sh` reads an app launch the same way (`cut -d' ' -f 2` of the
  command is the app folder,
  [`runtime.sh` lines 590–593](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L590-L593);
  [lines 604–607 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L604-L607)).
  Packaged scripts still `cd $(dirname "$0")` first
  ([Battery Monitor `launch.sh` line 3](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Battery%20Monitor/App/BatteryMonitorUI/launch.sh#L3)).
- **Environment.** `runtime.sh` exports
  `LD_LIBRARY_PATH="/lib:/config/lib:$miyoodir/lib:$sysdir/lib:$sysdir/lib/parasyte"`
  with `miyoodir=/mnt/SDCARD/miyoo`, `sysdir=/mnt/SDCARD/.tmp_update`
  ([`runtime.sh` lines 2–5](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L2-L5);
  same lines at `2bab767e`), and sets `HOME=/mnt/SDCARD/RetroArch/` without
  exporting it
  ([line 88](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L88);
  [line 100 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L100)).
  Onion's binaries are linked with `-Wl,-rpath=/mnt/SDCARD/.tmp_update/lib`
  ([`src/common/config.mk` lines 14, 54](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/config.mk#L14-L54)).
- **App or game: decided from the command line.** `check_is_game` treats a
  command as a game only if it contains `retroarch/cores`, `/../../Roms/` or
  `/mnt/SDCARD/Roms/`
  ([`runtime.sh` lines 278–280](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L278-L280);
  [lines 292–294 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L292-L294));
  an app launch ends with `set_prev_state "app"`
  ([line 581](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L581);
  [line 595 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L595)).
  While a process that is not MainUI, RetroArch, the game switcher, AdvanceMENU
  or Drastic runs, keymon's state is `MODE_APPS`
  ([`src/common/system/state.h` lines 89–102](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/state.h#L89-L102)).
- **What Onion does to `App/` folders on its own.**
  - The app sorter (Tweaks › Tools) moves every app folder whole to
    `/mnt/SDCARD/App.temp/` and back in label order
    ([`app_sorter.sh` lines 1–33](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/script/app_sorter.sh#L1-L33);
    [`src/tweaks/tools.h` lines 153, 158](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/tweaks/tools.h#L153-L158)):
    files inside the folder move with it, so app data inside `App/ZeldaRando/`
    survives, but its absolute path is briefly elsewhere.
  - Icon packs rewrite `icon` and `iconsel` in `config.json`, but only when
    the pack has an icon with the same base name as the current one
    (`<pack>/app/<basename>.png`); otherwise the file is left alone
    ([`src/common/utils/apply_icons.h` lines 132–179](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/apply_icons.h#L132-L179)).
    `cJSON_Print` rewrites the whole file, so a key Onion does not know
    survives only if cJSON keeps it (it does: the object is loaded and
    printed back).
  - An app may rewrite its own `config.json`: Guest Mode copies
    `data/configON.json` or `data/configOFF.json` over it
    ([Guest Mode `launch.sh` lines 23–34](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Guest%20Mode/App/Guest_Mode/launch.sh#L23-L34)).

## 3. SDL and video

- **Onion apps use SDL 1.2**: `#include <SDL/SDL.h>`, `<SDL/SDL_image.h>`,
  `<SDL/SDL_ttf.h>`
  ([`src/common/utils/sdl_init.h` lines 4–6](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/sdl_init.h#L4-L6)),
  linked with `-lSDL -lSDL_ttf -lSDL_image -lSDL_mixer -lSDL_rotozoom`
  ([`src/tweaks/Makefile` line 7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/tweaks/Makefile#L7)).
- **The versions are those of the toolchain image's sysroot.** In
  `aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132`
  (read through `onion-toolchain.sh`), under
  `/opt/miyoomini-toolchain/arm-linux-gnueabihf/libc/usr/include/SDL/`:
  `SDL_version.h` `SDL_MAJOR_VERSION 1`, `SDL_MINOR_VERSION 2`,
  `SDL_PATCHLEVEL 15` (lines 42–44); `SDL_ttf.h` 2.0.11 (lines 40–42);
  `SDL_image.h` 1.2.12 (lines 38–40). `SDL_config.h` enables
  `SDL_VIDEO_DRIVER_FBCON` and `SDL_VIDEO_DRIVER_DUMMY` (lines 265–266). The
  sysroot's `usr/lib/` has `libSDL-1.2.so.0.11.4`, `libSDL_ttf-2.0.so.0.10.1`,
  `libSDL_image-1.2.so.0.8.4`, `libSDL_mixer-1.2.so.0.12.0`. The image has no
  SDL2.
- **Video setup**: `SDL_InitDefault()` reads the framebuffer size, then calls
  `SDL_SetVideoMode(g_display.width, g_display.height, 32, SDL_HWSURFACE)`,
  draws into an `SDL_CreateRGBSurface(SDL_HWSURFACE, w, h, 32, 0, 0, 0, 0)`
  and enables key repeat `SDL_EnableKeyRepeat(300, 50)`
  ([`sdl_init.h` lines 17–31](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/sdl_init.h#L17-L31)).
  The size is `FBIOGET_VSCREENINFO`'s `xres` × `yres` of `/dev/fb0`
  ([`src/common/system/display.h` lines 83–92](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/display.h#L83-L92)).
  Older tools hard-code `SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE)`
  ([`src/batteryMonitorUI/batteryMonitorUI.c` line 66](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/batteryMonitorUI/batteryMonitorUI.c#L66)).
- **The device's SDL 1.2 comes with Onion**, in `/mnt/SDCARD/miyoo/lib/`
  (on `LD_LIBRARY_PATH`, section 2): `libSDL-1.2.so.0.11.4`, `libSDL.so`,
  `libSDL_image-1.2.so.0.8.4`, `libSDL_ttf-2.0.so.0.10.1`,
  `libSDL_mixer-1.2.so.0.12.0`
  ([`static/build/miyoo/lib/`](https://github.com/OnionUI/Onion/tree/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/miyoo/lib)),
  the same file names as the toolchain sysroot.
- **SDL2 on the card exists only bundled**: `libSDL2-2.0.so.0` in
  `/mnt/SDCARD/.tmp_update/lib/parasyte/`
  ([`static/build/.tmp_update/lib/parasyte/`](https://github.com/OnionUI/Onion/tree/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/lib/parasyte)),
  and inside two packages, Drastic (`Emu/NDS/libs/`) and PICO-8 standalone
  (`RApp/PICO-8/lib/`), each with `libSDL2_image-2.0.so.0` and
  `libSDL2_ttf-2.0.so.0`. Phase 4 step 1 decides between them.

## 4. Screen

- **Two panel sizes are known: 640×480 and 752×560.**
  `DEFAULT_WIDTH 640`, `DEFAULT_HEIGHT 480` under `PLATFORM_MIYOOMINI`
  ([`display.h` lines 14–20](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/display.h#L14-L20)).
- **Detection**: `get_screen_resolution` reads `Current TimingWidth=` from
  `/proc/mi_modules/fb/mi_fb0`, keeps 752×560 only if the firmware's
  `miyoo_version` is at least `202310271401` (then touches
  `/tmp/new_res_available`), else uses 640×480, and writes the result to
  `/tmp/screen_resolution`
  ([`runtime.sh` lines 713–742](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L713-L742);
  [lines 727–756 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L727-L756)).
- **An app gets 640×480 by default.** The framebuffer is switched to the panel
  size only when `/tmp/new_res_available` exists and the launched program's
  `full_resolution` file exists, and back to 640×480 when it exits
  ([`runtime.sh` lines 400–404, 420–423](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L400-L423);
  [lines 414–437 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L414-L437)).
- **The full-resolution opt-in for an app is the file `App/<Name>/full_resolution`**:
  for a command containing `/mnt/SDCARD/App/`, the marker is the app folder
  plus `/full_resolution`
  ([`runtime.sh` lines 586–593](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L586-L593);
  [lines 600–607 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L600-L607)).
  The only package that ships one is Drastic
  (`static/packages/Emu/Nintendo - DS (Drastic)/Emu/NDS/full_resolution`).
- **Mini Flip: the panel's resolution is not stated in the sources.** The
  Flip runs the same `get_screen_resolution` (no 285 case). The Flip commit
  says it "Uses MainUI 354 binaries (Clean/Expert) to maintain proper 640x480
  resolution compatibility with existing Onion themes"
  ([`9bf8ccd3` message](https://github.com/OnionUI/Onion/commit/9bf8ccd3c4621bd543d6b22080ffdd8bbfa68a0e)),
  which bounds what MainUI draws, not the panel. Whatever the panel, an app
  without `full_resolution` is given 640×480 by the code path above; that is
  the assumption later steps use. Device check below.
- **List metrics**: MainUI's list is a binary and Tweaks' list layout is
  theme-driven; no grid in characters is stated, so Phase 3 step 2 sizes its
  grid from the 640×480 pixels.

## 5. Buttons

| Button | Linux code (`keymap_hw.h`) | SDL 1.2 key (`keymap_sw.h`) |
|---|---|---|
| Up / Down / Left / Right | `KEY_UP` / `KEY_DOWN` / `KEY_LEFT` / `KEY_RIGHT` | `SDLK_UP` / `SDLK_DOWN` / `SDLK_LEFT` / `SDLK_RIGHT` |
| A | `KEY_SPACE` | `SDLK_SPACE` |
| B | `KEY_LEFTCTRL` | `SDLK_LCTRL` |
| X | `KEY_LEFTSHIFT` | `SDLK_LSHIFT` |
| Y | `KEY_LEFTALT` | `SDLK_LALT` |
| L1 | `KEY_E` | `SDLK_e` |
| R1 | `KEY_T` | `SDLK_t` |
| L2 | `KEY_TAB` | `SDLK_TAB` |
| R2 | `KEY_BACKSPACE` | `SDLK_BACKSPACE` |
| Select | `KEY_RIGHTCTRL` | `SDLK_RCTRL` |
| Start | `KEY_ENTER` | `SDLK_RETURN` |
| Menu | `KEY_ESC` | `SDLK_ESCAPE` |
| Power | `KEY_POWER` | `SDLK_FIRST` |
| Volume + / − | `KEY_VOLUMEUP` / `KEY_VOLUMEDOWN` | none |
| Lid closed (v4.5-dev only) | `KEY_SLEEP` | none |

Sources:
[`src/common/system/keymap_hw.h` lines 7–24](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/keymap_hw.h#L7-L24),
[line 25 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/common/system/keymap_hw.h#L25)
(`HW_BTN_LID_CLOSE KEY_SLEEP`),
[`src/common/system/keymap_sw.h` lines 4–22](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/keymap_sw.h#L4-L22).

- **The lid is not read as a key.** `HW_BTN_LID_CLOSE` is defined and used
  nowhere; keymon polls `/sys/devices/soc0/soc/soc:hall-mh248/hallvalue` on a
  285 and suspends, shuts down or does nothing per the Tweaks setting
  ([`keymon.c` lines 553–562, 1067–1096 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/keymon/keymon.c#L1067-L1096)).
  Whether the Flip's kernel also emits `KEY_SLEEP` to an app is not stated.
- **keymon reads `/dev/input/event0` without grabbing it**
  (`open("/dev/input/event0", O_RDONLY)`,
  [`keymon.c` line 456](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/keymon.c#L456);
  [line 610 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/keymon/keymon.c#L610)),
  so an app receives every key, including those keymon acts on. It grabs
  the device (`EVIOCGRAB`) only around a suspend or a MainUI menu press
  ([`src/keymon/input_fd.h` lines 109–117](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/input_fd.h#L109-L117);
  [`menuButtonAction.h` lines 384–389](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/menuButtonAction.h#L384-L389)).
- **What keymon does while an app runs** (`MODE_APPS`;
  [`keymon.c` switch, lines 546–811](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/keymon.c#L546-L811);
  [lines 721–986 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/keymon/keymon.c#L721-L986);
  the line numbers below are `07505ea5`'s, and the only change on v4.5-dev
  is the brightness combination noted):
  - Power: short press turns the screen off (or deep sleep), Menu + Power
    takes a screenshot, a hold of 7 repeat events deep-sleeps, a hold of
    `REPEAT_SEC(5)` forces a shutdown (lines 547–580).
  - Volume + / −: volume, both together mute, Menu + volume: brightness
    (lines 726–749, 765–792).
  - Select + Start + R1 / L1: CPU clock ±100 (lines 598–607).
  - Start + L2 / R2: audio boost down / up; Select + L2 / R2: brightness,
    skipped on the Mini Plus (lines 608–683), and on the Flip too on
    v4.5-dev (`IS_MIYOO_PLUS_OR_FLIP()`).
  - Select alone sends Menu only in MainUI (lines 581–593).
  - Menu: its single, double and long press act only in MainUI, a game or
    Drastic; for an app it does nothing
    ([`menuButtonAction.h` lines 288–325](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/menuButtonAction.h#L288-L325)).
    Menu + A held 2 s toggles the screen recorder (if enabled), Menu + B held
    2 s the blue-light filter (lines 813–836).
  - Shutdown with an app running: keymon removes the command and suspends
    the app's process before powering off
    ([`keymon.c` lines 242–245](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/keymon/keymon.c#L242-L245);
    [lines 343–346 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/src/keymon/keymon.c#L343-L346)).
- **Onion's menu convention**, from Tweaks (a list): Up/Down move, Left/Right
  change a value, A activates, B goes back one level and quits at the top,
  Menu released quits, Y resets the item, Select shows the item's help
  ([`src/tweaks/tweaks.c` lines 118–161](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/tweaks/tweaks.c#L118-L161);
  same lines at `2bab767e`). From the Package Manager (tabs): L1/R1 change
  tab with wrap-around, L2/R2 page by 7 rows, B back, Start applies
  ([`src/packageManager/keystateHandler.h` lines 23–40, 63–84, 117–144](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/keystateHandler.h#L23-L144)).

## 6. SNES list

- **The SNES entry is `Emu/SFC`, played with Beetle Supafaust.** Its
  `config.json`: `"label":"SNES"`, `"rompath":"../../Roms/SFC"`,
  `"extlist":"sfc|smc|fig|bs|st|zip|7z"`, `"launch":"launch.sh"`
  ([`config.json` lines 2–12](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/Emu/Nintendo%20-%20SNES%20%28Beetle%20Supafaust%29/Emu/SFC/config.json#L2-L12));
  `launch.sh` runs
  `HOME=/mnt/SDCARD/RetroArch/ $progdir/../../RetroArch/retroarch -v -L $progdir/../../RetroArch/.retroarch/cores/mednafen_supafaust_libretro.so "$1"`
  ([`launch.sh` line 7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/Emu/Nintendo%20-%20SNES%20%28Beetle%20Supafaust%29/Emu/SFC/launch.sh#L6-L7)).
  It is the only `Emu` package for `Roms/SFC`; the other SNES cores are `RApp`
  packages, and Onion's docs list `lr-mednafen-supafaust` first
  ([`24-nintendo-snes.md` line 10](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/04-emulators/02-consoles/24-nintendo-snes.md#L10)).
- **The extension list is case-sensitive**: the FAQ asks for `"extlist":"zip|ZIP"`
  to list upper-case files
  ([`03-faq/index.mdx` line 74](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/03-faq/index.mdx#L74)),
  so a ROM must end in lower-case `.sfc`.
- **A per-ROM core override can change the core.** Before a game,
  `runtime.sh` reads `<rom dir>/.game_config/<rom name>.cfg` and may rewrite
  the command with another core (lines 347 and 360–364 call it; 361 and
  374–378 at `2bab767e`)
  ([`runtime.sh` lines 458–494](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L458-L494);
  [lines 472–508 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L472-L508)).
  For `Roms/SFC/ZeldaRando/X.sfc` that file is `Roms/SFC/ZeldaRando/.game_config/X.cfg`;
  it exists only if the user changes the core for that ROM.
- **Subfolders: one level deep, no apostrophe, not with `miyoogamelist.xml`.**
  "Subfolders can be used inside your rom folders but ONLY one level deep
  (i.e. `Roms/GB/Hacks`)"; "Subfolders cannot have an apostrophe (`'`) in the
  name and browsing subfolders is not compatible with miyoogamelist.xml (all
  games will appear as one flat list)"
  ([`04-emulators/index.mdx` lines 50–52](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/04-emulators/index.mdx#L50-L52)).
  The SD card is FAT32
  ([`01-fresh-install.mdx` line 21](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/02-installation/01-fresh-install.mdx#L21)).
- **The list is cached in `<rompath>/<folder>_cache6.db`**, here
  `Roms/SFC/SFC_cache6.db`, an SQLite table `SFC_roms`
  ([`src/randomGamePicker/randomGamePicker.c` lines 123–132](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/randomGamePicker/randomGamePicker.c#L123-L132)).
  "If you add new roms to a system, highlight the system in the Games tab and
  press SELECT › Refresh all roms"
  ([`04-emulators/index.mdx` line 52](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/04-emulators/index.mdx#L52)),
  so a ROM written while the cache exists is not listed until the cache is
  rebuilt. Onion's own scripts force that by deleting the file:
  `rm -f "$romdir/$(basename "$romdir")_cache2.db"` and `_cache6.db`, after
  resetting the list position in `/tmp/state.json` and
  `/appconfigs/romwinidx.json`
  ([`reset_list.sh` lines 1–16](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/script/reset_list.sh#L1-L16));
  the netplay client does the same after copying a ROM
  ([`easy-netplay_client.sh` lines 302–303](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/script/netplay/easy-netplay_client.sh#L302-L303)).
  That a ROM in a subfolder lives in the parent's cache is an inference
  (Random Game reads only `<rompath>/<folder>_cache6.db` and "is not limited
  by subfolder",
  [`random-game.md` line 24](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/07-apps/01-included-in-onion/random-game.md#L24));
  MainUI's cache code is not in the sources.

## 7. Saves and states

- **Onion's `retroarch.cfg`**
  ([`static/configs/RetroArch/.retroarch/retroarch.cfg`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L2959-L3005),
  extracted to `/mnt/SDCARD/RetroArch/.retroarch/retroarch.cfg`, RetroArch's
  config under `HOME=/mnt/SDCARD/RetroArch/`):
  - `savefile_directory = "/mnt/SDCARD/Saves/CurrentProfile/saves"` (line 2959),
    `savestate_directory = "/mnt/SDCARD/Saves/CurrentProfile/states"` (2964);
  - `sort_savefiles_enable = "true"`, `sort_savestates_enable = "true"`,
    `sort_savefiles_by_content_enable = "false"`,
    `sort_savestates_by_content_enable = "false"` (3000–3003);
  - `savefiles_in_content_dir = "false"`, `savestates_in_content_dir = "false"`
    (2960, 2968);
  - `savestate_auto_index = "true"`, `savestate_auto_load = "true"`,
    `savestate_auto_save = "true"`, `savestate_max_keep = "10"`,
    `savestate_file_compression = "true"`, `savestate_thumbnail_enable = "false"`
    (2961–2967), `state_slot = "0"` (3005);
  - `auto_overrides_enable = "true"`, `config_save_on_exit = "false"`
    (lines [37](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L37), [59](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L59)).
- **Onion does not force these at launch.** `force_retroarch_cfg` patches only
  `network_cmd_enable = "true"`
  ([`runtime.sh` lines 447–456](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L447-L456);
  [lines 461–470 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L461-L470)),
  and an install without reset extracts configs with `7z x -aos` ("skip
  existing files")
  ([`install.sh` lines 479–487](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/dist/miyoo/app/.tmp_update/install.sh#L479-L487);
  [lines 487–495 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/dist/miyoo/app/.tmp_update/install.sh#L487-L495)).
  A card's own `retroarch.cfg` (or a core override in
  `Saves/CurrentProfile/config/Supafaust/`) can therefore differ from these
  values; the shipped `Supafaust.cfg` is empty. Device check below.
- **The save folder is the core's `library_name`, `Supafaust`.** With
  `sort_savefiles_enable`, RetroArch appends `sysinfo->library_name` to the
  save directory, and likewise for states
  ([RetroArch `runloop.c` lines 8241–8300](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L8241-L8300)),
  and the core sets `info->library_name = "Supafaust"`
  ([supafaust `libretro.cpp` line 303](https://github.com/libretro/supafaust/blob/75c658cce454e58ae04ea252f53a31c60d61548e/libretro.cpp#L303)).
  The `.info` file's `corename = "Beetle Supafaust"`
  ([`mednafen_supafaust_libretro.info` line 5](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/RetroArch/.retroarch/cores/mednafen_supafaust_libretro.info#L5))
  is not used for the folder. Onion's README files saying `SFC  Beetle Supafaust`
  ([`saves/README.txt` line 36](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/saves/README.txt#L36),
  [`states/README.txt` line 36](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/states/README.txt#L36))
  are wrong for this core; Onion's own core-config folder is
  `Saves/CurrentProfile/config/Supafaust/`
  ([`Supafaust.cfg`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/config/Supafaust/Supafaust.cfg)),
  which agrees.
- **The base name is the file name without its last extension, folders
  dropped.** `runloop_path_set_basename` copies the content path and cuts it
  at the last `.`
  ([`runloop.c` lines 8099–8139](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L8099-L8139));
  the `.srm` and `.state` names are `fill_pathname_dir(dir, basename, ext)`
  ([lines 8355–8375](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L8355-L8375)),
  which appends `path_basename()` (the part after the last slash) to the
  directory
  ([`libretro-common/file/file_path.c` lines 420–427, 665–673](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/libretro-common/file/file_path.c#L420-L427)).
  The content's subfolder is used only with `sort_*_by_content_enable`
  ([`runloop.c` lines 8205–8209, 8253–8258](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L8205-L8258)),
  off in Onion's config. So `ZeldaRando/` is ignored, and two ROMs with the same
  file name in different SNES folders would share one save. Before launch,
  `runtime.sh` resolves the ROM path with `realpath`
  ([`runtime.sh` lines 335–337](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L335-L337);
  [lines 349–351 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L349-L351)),
  which does not change the file name.
- **Every state file name** (`.state` is `FILE_PATH_STATE_EXTENSION`,
  `.srm` `FILE_PATH_SRM_EXTENSION`, `.auto`, `.png`,
  [`file_path_special.h` lines 39–51](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/file_path_special.h#L39-L51)):
  - slot 0: `<base>.state`; slot N > 0: `<base>.stateN`; slot −1 (auto):
    `<base>.state.auto`
    ([`runloop.c` lines 7622–7644](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L7622-L7644));
  - auto save on exit and auto load on start use `<base>.state.auto`
    ([`command.c` lines 1367–1381, 1468–1492](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/command.c#L1367-L1381));
  - with `savestate_auto_index`, each save goes to the next slot in 0…10
    (`for (i = 0; i <= savestate_max_keep; i++)`), deleting the oldest in
    range with its `.png`; slots above 10 chosen by hand stay
    ([`command.c` lines 1518–1680](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/command.c#L1518-L1680),
    [1760–1783](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/command.c#L1760-L1783));
  - the slot scan matches files whose name starts with `<base>.state`
    (`string_starts_with(elem_base, state_base)`,
    [`command.c` lines 1570–1580](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/command.c#L1570-L1580)):
    `Open-Ganon-1CA10A-2.state` does not start with `Open-Ganon-1CA10A.state`,
    so the `-<n>` names never collide;
  - a thumbnail `<state file>.png` (e.g. `<base>.state3.png`,
    `<base>.state.auto.png`) is written only with `savestate_thumbnail_enable`
    ([`runloop.c` lines 6923–6936](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L6923-L6936);
    [`tasks/task_save.c` line 1170](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/tasks/task_save.c#L1170)),
    off in Onion's config;
  - compression sets a flag on the same file, with no extra extension
    ([`tasks/task_save.c` lines 1181–1184](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/tasks/task_save.c#L1181-L1184)).
- **Worked example: `Roms/SFC/ZeldaRando/Open-Ganon-1CA10A.sfc`**, base name
  `Open-Ganon-1CA10A`, with Onion's shipped config:
  - save: `/mnt/SDCARD/Saves/CurrentProfile/saves/Supafaust/Open-Ganon-1CA10A.srm`;
  - states, in `/mnt/SDCARD/Saves/CurrentProfile/states/Supafaust/`:
    `Open-Ganon-1CA10A.state`, `Open-Ganon-1CA10A.state1` …
    `Open-Ganon-1CA10A.state10` (and any higher slot picked by hand),
    `Open-Ganon-1CA10A.state.auto`, and each one's `.png` if thumbnails are
    turned on; all of them start with `Open-Ganon-1CA10A.state`.

## 8. Other per-ROM files

Recorded only; Phase 3 step 6 decides what a delete touches.

- **RetroArch history**: `/mnt/SDCARD/Saves/CurrentProfile/lists/content_history.lpl`
  ([`retroarch.cfg` lines 64–65](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L64-L65);
  [`src/common/system/settings.h` lines 16–17](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/settings.h#L16-L17),
  same lines at `2bab767e`),
  one entry per played ROM path; the game switcher reads it.
- **MainUI's Recents**: `/mnt/SDCARD/Roms/recentlist.json`
  (`recentlist-hidden.json` when hidden)
  ([`settings.h` lines 18–19](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/settings.h#L18-L19)).
- **Game-switcher screenshot**:
  `/mnt/SDCARD/Saves/CurrentProfile/romScreens/<hash>.png`, `<hash>` the
  decimal `FNV1A_Pippip_Yurii` of the ROM path as recorded in the history
  ([`src/gameSwitcher/gs_romscreen.h` lines 45–47](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/gameSwitcher/gs_romscreen.h#L45-L47);
  [`src/common/system/state.h` lines 303–313](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/system/state.h#L303-L313)).
- **Play activity**: rows of table `rom` (`file_path` relative to `Roms/`)
  in `/mnt/SDCARD/Saves/CurrentProfile/play_activity/play_activity_db.sqlite`
  ([`src/playActivity/playActivityDB.h` lines 19, 88](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/playActivity/playActivityDB.h#L19-L88)),
  started by `playActivity start "$rompath"`
  ([`runtime.sh` line 381](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/runtime.sh#L381);
  [line 395 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/build/.tmp_update/runtime.sh#L395)).
- **Per-ROM core override**: `<rom dir>/.game_config/<rom name>.cfg` (section 6).

## 9. Profiles

- **`Saves/CurrentProfile` is whichever profile is active.** Guest Mode
  swaps folders by renaming: going to guest, `CurrentProfile` becomes
  `MainProfile` and `GuestProfile` becomes `CurrentProfile`; going back, the
  reverse. It also moves `Roms/*.json` (favourites, recents) into the outgoing
  profile's `lists/` and restores the incoming one's
  ([Guest Mode `launch.sh` lines 16–37](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/App/Guest%20Mode/App/Guest_Mode/launch.sh#L16-L37)).
  Which one is current is read from `GuestProfile`'s existence
  (`GUEST_DIR "/mnt/SDCARD/Saves/GuestProfile"`,
  [`apply_icons.h` lines 23, 52–58](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/utils/apply_icons.h#L23-L58)).
- **While the guest profile is active, the main profile's saves are in
  `/mnt/SDCARD/Saves/MainProfile/saves/` and `.../states/`**; RetroArch writes
  only under `CurrentProfile` (section 7). The build ships `GuestProfile` as a
  copy of `CurrentProfile`
  ([`Makefile` line 202](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L202)).
- The app's own files under `App/ZeldaRando/` and the ROMs under `Roms/` are not
  per profile.

## 10. Checked against the plan

Each path or layout of `docs/plan-1.md` §2, against sections 1–9. The two SD
directories are spelled once in code, as `RANDO_SD_ROM_DIR` and
`RANDO_SD_HISTORY_DIR` (`app/sd/rando_seed_save.h`, relative to the SD
root; `git grep` finds no other spelling outside `docs/`). The ROM name is
`<Mode>-<Goal>-<code>[-<n>].sfc` (`alttp/src/name.c`): ASCII letters, digits
and `-`, lower-case `.sfc`, at most 36 characters
(`Inverted-Completionist-XXXXXX-99.sfc`) against `RANDO_NAME_ROM_MAX` 48.
Every fact used below is the same at `07505ea5` and `2bab767e` (section
*Pinned sources*), so no row differs between the branches.

| §2 path or layout | §2 text | Code | Facts | Verdict |
|---|---|---|---|---|
| `App/ZeldaRando/` as an Onion app (`config.json`, `launch.sh`, icon) | "An Onion app (`App/ZeldaRando/`)" | none yet (Phase 4 step 2's zip) | A folder under `App/` with a `config.json` is an app, named by `label`, started by `launch`, run from its own folder; the app sorter moves it whole; icon packs rewrite only a matching icon; an icon inside the app folder is not shown in a shipped app, an absolute path is a shipped form ([§2](#2-app-folder)) | MATCH, with a consequence (C1, C2) |
| `App/ZeldaRando/alttp/base.sfc` | "placed once at `App/ZeldaRando/alttp/base.sfc`" | none yet (Phase 3 step 5 reads it) | App data inside the folder moves with it and is not per profile; nothing outside `Roms/` is listed as a game ([§2](#2-app-folder), [§9](#9-profiles)) | MATCH |
| `App/ZeldaRando/alttp/history/` | "History records go to `App/ZeldaRando/alttp/history/<code>.json`" | `RANDO_SD_HISTORY_DIR` `"App/ZeldaRando/alttp/history"` | Same as `base.sfc`; not per profile ([§2](#2-app-folder), [§9](#9-profiles)) | MATCH, with a consequence (C3) |
| `Roms/SFC/ZeldaRando/` as a folder of the SNES list, played by the default core | "Generated ROMs go to `Roms/SFC/ZeldaRando/…`, a subfolder of Onion's SNES list"; played "from Onion's SNES list with Onion's own SNES core" | `RANDO_SD_ROM_DIR` `"Roms/SFC/ZeldaRando"` | `Emu/SFC` has `rompath` `../../Roms/SFC` and starts `mednafen_supafaust_libretro.so`; subfolders one level deep, no apostrophe, flattened with `miyoogamelist.xml`; the list is cached in `Roms/SFC/SFC_cache6.db` and a new ROM shows only after a refresh, which Onion's scripts force by deleting that file ([§6](#6-snes-list)) | MATCH, with a consequence (C3, C4) |
| The ROM name as a content name | "`<Mode>-<Goal>-<code>.sfc` (e.g. `Open-FastGanon-4F2A9C.sfc`)" | `rando_name_make`, `RANDO_NAME_ROM_MAX` | `extlist` holds lower-case `sfc` and is case-sensitive; the card is FAT32; RetroArch's base name is the file name without its last extension, so `Open-Ganon-1CA10A.sfc` gives `Open-Ganon-1CA10A` unchanged ([§6](#6-snes-list), [§7](#7-saves-and-states)) | MATCH |
| The `.srm` location | Delete "removes the ROM, its RetroArch `.srm` save" | none yet (Phase 3 step 6) | `Saves/CurrentProfile/saves/Supafaust/<base>.srm`: `savefile_directory`, sorted by the core's `library_name` `Supafaust`, not by content folder, not next to the content ([§7](#7-saves-and-states)); `CurrentProfile` is the active profile, the other one is `MainProfile` or `GuestProfile` ([§9](#9-profiles)) | MATCH, with a consequence (C5, C6, C7) |
| The save-state location | Delete removes "its RetroArch save states" | none yet (Phase 3 step 6) | `Saves/CurrentProfile/states/Supafaust/`: `<base>.state`, `<base>.stateN` (0…10 by auto-index, any N by hand), `<base>.state.auto`, each with an optional `.png`; all start with `<base>.state`, which no `-<n>` sibling does; compression adds no extension ([§7](#7-saves-and-states)) | MATCH, with a consequence (C5, C6, C8) |

No row is a `MISMATCH`.

### Consequences

- **C1 (Phase 4 step 2).** The zip's `App/ZeldaRando/config.json` needs `label`
  and `launch` (`"launch.sh"`); give `icon` as the absolute
  `/mnt/SDCARD/App/ZeldaRando/<file>.png`, the shipped absolute form, until
  device check 6 confirms it. `launch.sh` does `cd $(dirname "$0")` like
  Onion's apps. No `full_resolution` file, so the app gets 640×480.
- **C2 (Phase 4 step 2, Phase 3 step 5).** The binary finds the SD root
  from its folder (`../..` of `App/ZeldaRando/`, or `/mnt/SDCARD`) to join
  `RANDO_SD_ROM_DIR` and `RANDO_SD_HISTORY_DIR`; `launch.sh` starts it in
  `App/ZeldaRando/`.
- **C3 (Phase 3 step 5 or Phase 4 step 2).** `rando_seed_save` creates no
  directory: `App/ZeldaRando/alttp/history/` ships in the zip or is created
  at start, and `Roms/SFC/ZeldaRando/` is created before the first save (it is
  not in the zip, which holds only `App/ZeldaRando/`, with `alttp/base.bps`).
- **C4 (Phase 3 steps 5 and 6).** After writing a ROM (step 5) and after
  deleting one (step 6), the app deletes `Roms/SFC/SFC_cache6.db` (and
  `SFC_cache2.db`), as `reset_list.sh` does, so the list is rebuilt; that a
  subfolder's ROMs live in the parent's cache is device check 5.
- **C5 (Phase 3 step 6).** Delete builds the paths from the ROM's base name
  only: `Saves/CurrentProfile/saves/Supafaust/<base>.srm`, and every file of
  `Saves/CurrentProfile/states/Supafaust/` whose name starts with
  `<base>.state` (numbered slots of any number, `.auto`, `.png`). The folder
  name `Supafaust` is device check 3; the card's own `retroarch.cfg` keeping
  Onion's values is device check 4.
- **C6 (Phase 3 step 6).** Saves are per profile and History is not: while
  Guest Mode is on, the main profile's files are under `Saves/MainProfile/`,
  and the other way round under `Saves/GuestProfile/`. Step 6 decides whether
  delete also removes them there; if not, a later seed reusing the freed name
  (the name check looks only at the ROM and the record) loads the other
  profile's stale save or state when that profile comes back.
- **C7 (Phase 3 step 6).** The content folder is ignored, so a ROM of the
  same file name in another `Roms/SFC/` folder shares the `.srm` and
  states; generated names make this unlikely, and step 6 records it.
- **C8 (Phase 3 step 6).** Section 8's other per-ROM files (RetroArch
  history entry, Recents, game-switcher screenshot, play activity,
  `.game_config/<rom>.cfg`) are not part of §2's delete; step 6 decides.

Not §2 paths, recorded for their steps: Phase 3 step 2 sizes its grid from
640×480 ([§4](#4-screen)); Phase 3 step 4 follows the Tweaks and Package
Manager keys and leaves keymon's combinations alone ([§5](#5-buttons));
Phase 4 step 1 chooses between the device's SDL 1.2 and a bundled SDL2
([§3](#3-sdl-and-video)); the app targets a Flip-aware Onion, `v4.5-dev`
or later, which `main` is not ([§1](#1-device-and-onion-version), device
check 1).

### Device checks for the step note

The six checks below. Checks 3, 4 and 5 bound rows of this table (the
`.srm` and state folders, the SNES list's refresh) without changing a
verdict: the sources settle each fact, the check confirms the card follows
them.

## 11. GBC list, saves and states (plan 3)

What plan 3's LADX seeds need from Onion, cited like sections 6 and 7 and
reusing their RetroArch facts by reference. Every Onion file cited here is
outside the eight files that differ on `v4.5-dev` (*Pinned sources*), except
`Makefile` and `install.sh`, which get a second link; the Gambatte binary is
not in the `v4.5-dev` diff either. To re-check: clone each repository
blobless into `tmp/` (`git clone --filter=blob:none --no-checkout <url>`),
read files with `git show <commit>:<path>`, and identify a core with
`strings` on `git show <commit>:static/build/RetroArch/.retroarch/cores/<core>.so`;
`04e73b8` resolves after `git fetch` of `schmurtzm/gambatte-libretro` (not
of `libretro/gambatte-libretro`).

### The GBC entry

- **The GBC entry is `Emu/GBC`, played with Gambatte.** Its `config.json`:
  `"label":"GBC"`, `"launch":"launch.sh"`, `"rompath":"../../Roms/GBC"`,
  `"gamelist":"../../Roms/GBC/miyoogamelist.xml"`,
  `"extlist":"bin|dmg|gb|gbc|zip|7z"`
  ([`config.json` lines 1–13](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/Emu/Nintendo%20-%20Game%20Boy%20Color%20%28Gambatte%29/Emu/GBC/config.json#L1-L13));
  `launch.sh` runs
  `HOME=/mnt/SDCARD/RetroArch/ $progdir/../../RetroArch/retroarch -v -L $progdir/../../RetroArch/.retroarch/cores/gambatte_libretro.so "$1"`
  ([`launch.sh` lines 6–7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/Emu/Nintendo%20-%20Game%20Boy%20Color%20%28Gambatte%29/Emu/GBC/launch.sh#L6-L7)).
  Onion's GBC page lists `**lr-gambatte**, lr-gearboy, lr-tgbdual`, rom
  folder `GBC`
  ([`06-nintendo-gbc.md` lines 9–11](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/website/docs/04-emulators/03-handhelds/06-nintendo-gbc.md#L9-L11)).
- **It is the only `Emu` package for `Roms/GBC`; two `RApp` packages share
  the folder with other cores.** `RApp/gearboy_gbc` has
  `"rompath":"../../Roms/GBC"` and `"extlist":""`
  ([`config.json` lines 6–11](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/RApp/Nintendo%20-%20GBC%20%28Gearboy%29/RApp/gearboy_gbc/config.json#L6-L11))
  and starts `gearboy_libretro.so`
  ([`launch.sh` line 7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/RApp/Nintendo%20-%20GBC%20%28Gearboy%29/RApp/gearboy_gbc/launch.sh#L7));
  `RApp/TGB_Dual_gbc` has the same `rompath` and
  `"extlist":"bin|dmg|gb|gbc|zip|7z|cgb"`
  ([`config.json` lines 6–11](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/RApp/Nintendo%20-%20GBC%20%28TGB%20Dual%29/RApp/TGB_Dual_gbc/config.json#L6-L11))
  and starts `tgbdual_libretro.so`
  ([`launch.sh` line 7](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/RApp/Nintendo%20-%20GBC%20%28TGB%20Dual%29/RApp/TGB_Dual_gbc/launch.sh#L7)).
  `Emu/GB` also accepts `gbc` but lists `"rompath":"../../Roms/GB"`
  ([`config.json` lines 6–12](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/packages/Emu/Nintendo%20-%20Game%20Boy%20%28Gambatte%29/Emu/GB/config.json#L6-L12)),
  not the app's folder; a player copying a seed there gets the same core.
- **Packages are chosen in the Package Manager, not installed by default.**
  The build copies `static/packages/Emu/`, `App/` and `RApp/` into
  `App/PackageManager/data`
  ([`Makefile` lines 98–104](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L98-L104);
  [same lines at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/Makefile#L98-L104)),
  and the installer ends by running `packageManager --confirm` (or
  `--confirm --auto_update` without a reset)
  ([`install.sh` lines 329–335](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/dist/miyoo/app/.tmp_update/install.sh#L329-L335);
  [lines 337–343 at `2bab767e`](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/static/dist/miyoo/app/.tmp_update/install.sh#L337-L343)).
  With `--confirm`, if no package of the first tab (`Emu`, "VERIFIED") is
  installed, the ones whose rom folder already holds a file of their
  `extlist` (one subfolder level included) are pre-selected; the user
  confirms or changes the choice
  ([`packageManager.c` lines 41–45](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/packageManager.c#L41-L45);
  [`listActions.h` lines 7–45](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/listActions.h#L7-L45);
  [`changes.h` lines 38–43](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/changes.h#L38-L43);
  [`fileActions.h` lines 120–190](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/fileActions.h#L120-L190);
  [`globals.h` lines 35–38](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/packageManager/globals.h#L35-L38)).
  So whether `Emu/GBC` is on a given card is not stated in the sources
  (device check 9). The same holds for `Emu/SFC`, which section 6 assumed
  present.

### Listing

- **`.gbc` is in the `extlist`, lower case**, and the list is
  case-sensitive (FAQ, [§6](#6-snes-list)), so a LADX ROM must end in
  lower-case `.gbc`.
- **`Roms/GBC/ZeldaRando/` is listed**: the subfolder rules of
  [§6](#6-snes-list) apply to every rom folder (one level deep, no
  apostrophe; the docs' own example is `Roms/GB/Hacks`), and `Emu/GBC`
  has a `gamelist` key, so a `Roms/GBC/miyoogamelist.xml` on the card would
  flatten the subfolder into one list.
- **The cache is `Roms/GBC/GBC_cache6.db`, table `GBC_roms`.** The name is
  `<rompath>/<basename(rompath)>_cache6.db` and the table
  `<basename(rompath)>_roms`
  ([`randomGamePicker.c` lines 123–132](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/randomGamePicker/randomGamePicker.c#L123-L132)),
  and `reset_list.sh` deletes
  `rm -f "$romdir/$(basename "$romdir")_cache2.db"` and `_cache6.db` for any
  rom folder it is given
  ([`reset_list.sh` lines 15–16](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/.tmp_update/script/reset_list.sh#L15-L16)),
  here `Roms/GBC/GBC_cache2.db` and `Roms/GBC/GBC_cache6.db`. That a
  subfolder's ROMs live in the parent's cache stays an inference, as in
  [§6](#6-snes-list).
- **Per-ROM core override**: `Roms/GBC/ZeldaRando/.game_config/<rom name>.cfg`,
  the `runtime.sh` mechanism of [§6](#6-snes-list); it exists only if the
  user changes the core for that ROM.

### Saves and states

- **The save folder is `Gambatte`, the core's `library_name`.** RetroArch
  sorts saves and states by `library_name` ([§7](#7-saves-and-states)), and
  the core sets `info->library_name = "Gambatte";`
  ([`libretro.cpp` lines 1597–1611](https://github.com/schmurtzm/gambatte-libretro/blob/04e73b8215601b61fd414c66ab20db5a99d65001/libgambatte/libretro/libretro.cpp#L1597-L1611)),
  the string in the shipped binary. The `.info` file's `corename = "Gambatte"`
  ([`gambatte_libretro.info` line 5](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/build/RetroArch/.retroarch/cores/gambatte_libretro.info#L5))
  and Onion's README lines `GBC             Gambatte`
  ([`saves/README.txt` line 14](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/saves/README.txt#L14),
  [`states/README.txt` line 14](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/states/README.txt#L14))
  agree here, unlike SNES's.
- **Onion's Gambatte config overrides no save or state key.**
  `Saves/CurrentProfile/config/Gambatte/Gambatte.cfg` holds only
  `video_dingux_ipu_keep_aspect = "true"`
  ([`Gambatte.cfg` line 1](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/config/Gambatte/Gambatte.cfg#L1)),
  and `Gambatte.opt` only `gambatte_*` core options
  ([`Gambatte.opt` lines 1–31](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/Saves/CurrentProfile/config/Gambatte/Gambatte.opt#L1-L31)).
  So section 7's `retroarch.cfg` values apply unchanged, with the same
  caveat that a card's own files may differ (device check 4).
- **RetroArch writes `<base>.srm` and `<base>.rtc`, each only when the core
  gives it a non-empty memory.** For a single content, RetroArch registers
  the save path for `RETRO_MEMORY_SAVE_RAM` and infers an `.rtc` path for
  `RETRO_MEMORY_RTC` with `fill_pathname(savefile_name_rtc, savefile_path, ".rtc", …)`
  ([`save.c` lines 529–543](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/save.c#L529-L543),
  called from
  [`runloop.c` lines 4228–4234](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/runloop.c#L4228-L4234)),
  which replaces the last extension
  ([`file_path.c` lines 329–340](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/libretro-common/file/file_path.c#L329-L340)),
  so both sit in `saves/Gambatte/` with the same base name
  (`FILE_PATH_RTC_EXTENSION ".rtc"`, `FILE_PATH_SRM_EXTENSION ".srm"`,
  [`file_path_special.h` lines 45–47](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/file_path_special.h#L45-L47)).
  A memory whose data is null or whose size is 0 is skipped
  (`if (!mem_info->data || mem_info->size == 0) return false;`,
  [`save.c` lines 322–334](https://github.com/libretro/RetroArch/blob/69a4f0ea1e8aaf442ae4858f2e7f2b31a1776576/save.c#L322-L334)).
  No Onion patch touches `save.c` (*Pinned sources*). Onion ships
  `save_file_compression = "false"` and `autosave_interval = "0"`
  ([`retroarch.cfg` line 2958](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L2958),
  [line 41](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/static/configs/RetroArch/.retroarch/retroarch.cfg#L41)).
- **Gambatte gives save RAM only to a battery cartridge, and an RTC only to
  MBC3 with a timer or HuC3.** `retro_get_memory_data` and
  `retro_get_memory_size` return `gb.savedata_*` for
  `RETRO_MEMORY_SAVE_RAM` and `gb.rtcdata_*` for `RETRO_MEMORY_RTC`
  ([`libretro.cpp` lines 2766–2792](https://github.com/schmurtzm/gambatte-libretro/blob/04e73b8215601b61fd414c66ab20db5a99d65001/libgambatte/libretro/libretro.cpp#L2766-L2792));
  `savedata_size` is non-zero only when header byte `0x147` is one of
  `0x03 0x06 0x09 0x0F 0x10 0x13 0x1B 0x1E 0xFE 0xFF`, and `rtcdata_size`
  only for `0xFE` (HuC3), `0x0F` and `0x10`
  ([`cartridge_libretro.cpp` lines 10–80](https://github.com/schmurtzm/gambatte-libretro/blob/04e73b8215601b61fd414c66ab20db5a99d65001/libgambatte/src/mem/cartridge_libretro.cpp#L10-L80)).
  LADX DX's header byte is not in any pinned source (no ROM in the
  repository); it is known as `0x1B` (MBC5 + RAM + battery), which gives an
  `.srm` and no `.rtc`. That is an inference, settled by reading byte `0x147`
  of the base ROM or of a generated seed when the oracle runs with the ROM
  (Phase 2); delete covers `.rtc` anyway (G4).
- **State names and the base-name rule are those of [§7](#7-saves-and-states)**:
  `<base>.state`, `<base>.stateN`, `<base>.state.auto`, each with an optional
  `.png`, in `Saves/CurrentProfile/states/Gambatte/`; `ZeldaRando/` is
  ignored.
- **The other cores use other folders.** Started from `RApp/gearboy_gbc`,
  `RApp/TGB_Dual_gbc` or a `.game_config` override, a seed's saves go to that
  core's `library_name` folder. The shipped binaries hold the strings
  `Gearboy` and `TGB Dual` (with `v0.8.3 a6f3018` for TGB Dual), their
  `.info` files say `corename = "Gearboy"` and `"TGB Dual"`, and Onion ships
  `Saves/CurrentProfile/config/TGB Dual/`; these cores are not pinned to a
  source commit, so `Gearboy` and `TGB Dual` as folder names are inferences.

### Worked example

`Roms/GBC/ZeldaRando/Normal-All-1CA10A.gbc` (an illustrative name shaped
like ALttP's; Phase 4 fixes the real format), base name
`Normal-All-1CA10A`, with Onion's shipped config and the `Emu/GBC` core:

- save: `/mnt/SDCARD/Saves/CurrentProfile/saves/Gambatte/Normal-All-1CA10A.srm`
  (and `Normal-All-1CA10A.rtc` beside it only for a cartridge with a clock);
- states, in `/mnt/SDCARD/Saves/CurrentProfile/states/Gambatte/`:
  `Normal-All-1CA10A.state`, `Normal-All-1CA10A.state1` …
  `Normal-All-1CA10A.state10` (and any higher slot picked by hand),
  `Normal-All-1CA10A.state.auto`, and each one's `.png` if thumbnails are
  turned on; all of them start with `Normal-All-1CA10A.state`.

### Checked against plan 3 §2

Each GBC path of `docs/plan-3.md` §2 ("SD layout per game", and the
phase steps that use them), against the facts above. The Code column names the constants of `app/sd/rando_seed_save.h` (Phase 4 step 1).

| §2 path or layout | §2 text | Code | Facts | Verdict |
|---|---|---|---|---|
| `Roms/GBC/ZeldaRando/` as a folder of the GBC list, played by the default core | "Generated ROMs go to `Roms/SFC/ZeldaRando/` and `Roms/GBC/ZeldaRando/`"; "a patched `.gbc` that Onion's GBC core plays" | `RANDO_SD_LADX_ROM_DIR`, made before a save (`ladx_gen.c`) | `Emu/GBC` has `rompath` `../../Roms/GBC` and starts `gambatte_libretro.so`; one subfolder level is listed, no apostrophe, flattened with `miyoogamelist.xml`; the package is chosen in the Package Manager | MATCH, with a consequence (G1, G6) |
| The `.gbc` ROM name as content | "a readable ROM name built from the main settings and a hex code, with the same collision rule" | `rando_ladx_name_make` (`ladx_name.c`): `<Logic>-<Goal>-<CODE>.gbc` | `extlist` holds lower-case `gbc` and is case-sensitive; the base name is the file name without its last extension ([§7](#7-saves-and-states)) | MATCH, with a consequence (G2) |
| The `.srm` location | Phase 4 step 1: "deletion of the ROM, `.srm` and save states, using Phase 1 step 5's paths" | `RANDO_SD_LADX_CORE_DIR`, `.srm` and `.rtc` (`ladx_history.c`, `seed_files.c`) | `Saves/CurrentProfile/saves/Gambatte/<base>.srm`, plus `<base>.rtc` for a cartridge with a clock; Onion's Gambatte config overrides nothing | MATCH, with a consequence (G4, G5) |
| The save-state location | Same Phase 4 step 1 text | `RANDO_SD_LADX_CORE_DIR`, every `<base>.state*` (`seed_files.c`) | `Saves/CurrentProfile/states/Gambatte/`, every file starting with `<base>.state` ([§7](#7-saves-and-states)) | MATCH, with a consequence (G4, G5) |
| The cache to delete | "The GBC folder, core and cache are subject to Phase 1's Onion research"; step 5: "the cache files to delete" | `RANDO_SD_LADX_LIST_CACHE6`, `RANDO_SD_LADX_LIST_CACHE2` | `Roms/GBC/GBC_cache6.db` (table `GBC_roms`) and `GBC_cache2.db`, deleted the way `reset_list.sh` does | MATCH, with a consequence (G3) |
| `App/ZeldaRando/ladx/` (`base.gbc`, the patch tables, `history/`, `settings.json`) | "`App/ZeldaRando/ladx/` holds `base.gbc`, the patch tables, `history/` and `settings.json`" | `RANDO_SD_LADX_DIR` and its files (`base.gbc`, `tables/`, `history/`, `settings.json`) | App data inside the app folder moves with it, is not listed as a game and is not per profile ([§2](#2-app-folder), [§9](#9-profiles)) | MATCH |

No row is a `MISMATCH`: §2's GBC paths stand.

### Consequences for plan 3

- **G1 (Phase 4 step 1).** `Roms/GBC/ZeldaRando/` is created before the
  first LADX save, like `Roms/SFC/ZeldaRando/` (C3); it is not in the zip.
- **G2 (Phase 4 step 1).** The LADX ROM name ends in lower-case `.gbc` and
  keeps ALttP's character set (letters, digits, `-`), so the base name is
  the name without `.gbc`.
- **G3 (Phase 4 step 1).** After writing and after deleting a LADX ROM, the
  app deletes `Roms/GBC/GBC_cache6.db` and `Roms/GBC/GBC_cache2.db`, the
  GBC counterparts of `RANDO_SD_LIST_CACHE6` and `RANDO_SD_LIST_CACHE2`
  (C4; device check 8).
- **G4 (Phase 4 step 1).** Delete removes
  `saves/Gambatte/<base>.srm` and `saves/Gambatte/<base>.rtc` (absent for a
  cartridge without a clock, harmless to try), and every file of
  `states/Gambatte/` whose name starts with `<base>.state`. The folder name
  is the GBC counterpart of `RANDO_SD_CORE_DIR` (device check 7).
- **G5 (Phase 4 step 1).** The profiles are handled as for ALttP (C6): the
  same paths under `Saves/MainProfile/` and `Saves/GuestProfile/`. Other
  cores and the per-ROM override (`Gearboy`, `TGB Dual`,
  `Roms/GBC/ZeldaRando/.game_config/<rom>.cfg`) are handled as C7 and C8
  decided for ALttP; a seed played with another core keeps that core's save.
- **G6 (Phase 4 step 1).** A LADX and an ALttP seed never share a save or
  state file: their folders are `Gambatte` and `Supafaust`, two different
  `library_name`s. A same-named ROM under `Roms/GB/` played with the
  `Emu/GB` Gambatte core would share the LADX seed's files (as C7 for
  SNES); generated names make this unlikely.
- **G7 (Phase 4 step 5).** The README can say a LADX seed needs the GBC
  list (`Emu/GBC`) installed through the Package Manager, as ALttP needs
  `Emu/SFC` (device check 9).

## Device checks for the owner

What the sources do not settle, with what the app assumes meanwhile:

1. The Onion build on the Flip: `cat /mnt/SDCARD/.tmp_update/onionVersion/version.txt`
   and `cat /tmp/deviceModel` (285 means a Flip-aware build). Assumed:
   `v4.5-dev` or later.
2. The resolution an app gets on the Flip: `cat /tmp/screen_resolution` and
   whether `/tmp/new_res_available` exists. Assumed: 640×480.
3. An existing SNES ROM's save folder: the folder name under
   `Saves/CurrentProfile/saves/` and `states/` after saving in a game started
   from the SNES list. Assumed: `Supafaust`.
4. The card's `RetroArch/.retroarch/retroarch.cfg` still has the
   `savefile_directory`, `savestate_directory`, `sort_*` and
   `*_in_content_dir` values of section 7, and
   `Saves/CurrentProfile/config/Supafaust/` holds no override of them.
5. A ROM copied into `Roms/SFC/ZeldaRando/` shows in the SNES list only after
   Refresh, and shows without it once `Roms/SFC/SFC_cache6.db` is deleted.
6. An app icon given as `/mnt/SDCARD/App/ZeldaRando/<file>.png` (or a path
   relative to the app folder) is shown in the Apps list.
7. An existing GBC ROM's save folder: the folder name under
   `Saves/CurrentProfile/saves/` and `states/` after saving in a game started
   from the GBC list. Assumed: `Gambatte`.
8. A ROM copied into `Roms/GBC/ZeldaRando/` shows in the GBC list only after
   Refresh, and shows without it once `Roms/GBC/GBC_cache6.db` is deleted.
9. The GBC list (`Emu/GBC`) and the SNES list (`Emu/SFC`) are installed on
   the card (Package Manager, "VERIFIED" tab). Assumed: both are.
