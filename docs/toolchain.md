# Onion platform facts

What this repository takes from Onion, each fact cited from Onion's own
sources at `OnionUI/Onion` commit
[`07505ea58c7bba698d6b9220ff43946a43cac76b`](https://github.com/OnionUI/Onion/tree/07505ea58c7bba698d6b9220ff43946a43cac76b)
(2026-01-21).

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
  `qemu-arm`.
