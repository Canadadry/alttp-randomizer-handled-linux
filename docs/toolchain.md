# Onion toolchain facts

What this repository takes from Onion, each fact cited from Onion's own
sources at `OnionUI/Onion` commit
[`07505ea58c7bba698d6b9220ff43946a43cac76b`](https://github.com/OnionUI/Onion/tree/07505ea58c7bba698d6b9220ff43946a43cac76b)
(2026-01-21).

The other Onion facts (app folder, SDL, screen, buttons, SNES list, saves)
are in [`docs/onion.md`](onion.md).

Re-checked at `v4.5-dev`
[`2bab767e4a17b1b489f02c9fb6af8dde310d4c3e`](https://github.com/OnionUI/Onion/tree/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e)
(2026-02-07): same `TOOLCHAIN` and `with-toolchain` lines
([`Makefile` line 51](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/Makefile#L51),
[lines 263–264](https://github.com/OnionUI/Onion/blob/2bab767e4a17b1b489f02c9fb6af8dde310d4c3e/Makefile#L263-L264)),
same miyoomini flags (`src/common/config.mk` and `.github/workflows/` identical).

## Toolchain

- **Onion builds in the docker image `aemiii91/miyoomini-toolchain:latest`.**
  The root `Makefile` sets `TOOLCHAIN := aemiii91/miyoomini-toolchain:latest`
  and its `with-toolchain` target runs
  `docker run --rm -v "$(ROOT_DIR)":/root/workspace $(TOOLCHAIN) /bin/bash -c "source /root/.bashrc; make $(CMD)"`
  ([`Makefile` line 51](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L51),
  [lines 263–264](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/Makefile#L263-L264)).
- **Onion's CI builds in the same image, sourcing `/root/.bashrc`.**
  [`.github/workflows/build.yml`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/.github/workflows/build.yml#L14-L24)
  runs in `container: image: aemiii91/miyoomini-toolchain:latest` and starts
  its build step with `source /root/.bashrc`; so do
  [`pre-release.yml`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/.github/workflows/pre-release.yml#L18-L39)
  and
  [`tagged-release.yml`](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/.github/workflows/tagged-release.yml#L13-L27).
- **The Miyoo Mini flags.** For `PLATFORM=miyoomini`,
  [`src/common/config.mk` line 54](https://github.com/OnionUI/Onion/blob/07505ea58c7bba698d6b9220ff43946a43cac76b/src/common/config.mk#L53-L54)
  adds `-marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve`
  to `CFLAGS`.
- **Pinned by digest, never by tag:**
  `aemiii91/miyoomini-toolchain@sha256:e5123590ad75d27f0f4c91196e3119a255cad45f3ae15243e29a8e0a2ec50132`,
  what `:latest` resolved to on 2026-11-01 (image created 2026-01-20). The
  digest is named only in `onion-toolchain.sh`, the runner every cross build
  goes through.
- **The compiler.** In that image, `/root/.bashrc` exports
  `CROSS_COMPILE=/opt/miyoomini-toolchain/usr/bin/arm-linux-gnueabihf-`:
  `arm-linux-gnueabihf-gcc (GNU Toolchain for the A-profile Architecture 8.3-2019.03 (arm-rel-8.36)) 8.3.0`,
  with glibc 2.28.

## How this repository uses it

- `onion-toolchain.sh <command>` runs the command in the pinned image, as
  root, with the repository at `/root/workspace` and `CROSS_COMPILE` set.
- `make e2e-arm` builds the generator with the host `e2e` flags, Onion's
  flags above and `-static` (so `qemu-arm` needs no sysroot), checks it is a
  32-bit ARM EABI executable, and runs the host e2e driver on it under
  `qemu-arm`. Its comparison is the host's, claims included, so a write-group
  claim (`<case> all <group>`) compares the ARM build's `rom_writes.txt` too.
- `make unit-arm` builds every unit test of `tests/alttp/Makefile`'s
  `UNIT_TESTS` with the same flags plus `-D_FILE_OFFSET_BITS=64`, and runs
  each under `qemu-arm` from `tests/alttp`, then each of `LOGIC_TESTS` with
  `--fixtures`. The extra define is for qemu-user only: on a 64-bit host
  kernel, ext4 hands the 32-bit guest 64-bit directory cookies, and a 32-bit
  `readdir` fails with `EOVERFLOW` (the tests that list `cases/` see an empty
  directory). A 32-bit kernel, as on the device, returns 32-bit cookies.
- `make sdl-arm` compiles the SDL layer (`app/sdl/`) and the SDL main with
  the host flags, `-pedantic-errors`, `-O2` and Onion's flags, compiles the
  library, `alttp/host` and the app archive's sources with the same flags,
  and links them dynamically with `-lSDL -lSDL_ttf` against the image's
  sysroot (SDL 1.2.15, SDL_ttf 2.0.11, the copies Onion ships in
  `/mnt/SDCARD/miyoo/lib/`). It never calls `sdl-config` or `pkg-config`:
  the image's are the x86 host's (`-L/usr/lib/x86_64-linux-gnu`), and the
  sysroot resolves `<SDL/SDL.h>` with no `-I`. Linking proves every SDL
  symbol the code uses exists in the device's libraries; `file` checks a
  dynamically linked 32-bit ARM EABI executable, and nothing is run.
- `make package` runs `make sdl-arm`, strips its binary with the image's
  `${CROSS_COMPILE}strip`, and checks the stripped copy on the host: `file`
  reports a stripped, dynamically linked 32-bit ARM EABI5 executable with the
  armhf loader `/lib/ld-linux-armhf.so.3` (`file` prints no hard-float word),
  `readelf -h` reports `hard-float ABI` in the ELF flags, and `readelf -d`
  lists `libSDL-1.2.so.0` and `libSDL_ttf-2.0.so.0` as NEEDED; a miss fails
  with the offending line. It re-checks `data/base.bps` with
  `test_patch_header --file`, then `app/package/build_zip.py` writes
  `tests/alttp/build/package/ZeldaRando.zip` (`App/ZeldaRando/`: the binary,
  `launch.sh`, `config.json`, `icon.png`, `base.bps`, the font and its
  licence). The shipped binary has `sdl-arm`'s flags, the same as the code
  `e2e-arm` and `unit-arm` run under qemu; it is not run itself.
