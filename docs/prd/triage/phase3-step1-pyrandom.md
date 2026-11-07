---
title: "LADX library: Python's RNG in C (plan 3, Phase 3 step 1)"
description: "The LADX C port needs CPython's random.Random reproduced bit for bit, and a LADX library and test tree the fast loop, the lint and the ARM run all cover."
status: needs-triage
---

## Plan step (verbatim, `docs/plan-3.md`, §3 Phase 3 item 1)

1. **Python RNG in C.** `ladx/src/pyrandom.c`: MT19937, seeding from bytes (SHA-512, so a SHA-512 module tested against FIPS 180-4 vectors), and every method of the inventory. This step also extends `make unit-arm` to `tests/ladx`'s unit tests, adds LADX's unit tests to the root `make test`, moves `test-ladx-oracle` and `ladx-cases` from `make test` to separate targets called from `make check` (`CLAUDE.md`, "never make it slower"; the checks are unchanged, only which target runs them), records `make test`'s time before and after in the step note, and extends `make lint` (`-std=c99 -pedantic-errors`) to `ladx/`, verified by `make lint` building the `ladx/` sources. Verified by `tests/ladx/unit/test_pyrandom.c` against Phase 1 step 8's fixtures, under `make test` and `make unit-arm`.

## Problem Statement

LADXR draws every seed-dependent decision (world setup, item pool, fill, Marin's line, hints, music, the end screen, bosses, the enemizer) from CPython's `random.Random`, seeded from the 16-byte seed (`randomizer.py:29`), with the state copied into the global `random` (`randomizer.py:42`) and a private `random.Random(seed)` in the enemizer. The C port can match LADXR's goldens byte for byte only if it reproduces that generator exactly: MT19937, CPython's version-2 seeding from bytes (which hashes the seed with SHA-512), and the exact algorithms of every method LADXR calls, down to the number of 32-bit words each consumes and the bits of every float.

Nothing of the LADX C library exists yet: no `ladx/` tree, no LADX unit-test Makefile targets, no place in `make test`, `make lint` or `make unit-arm`. Meanwhile `make test` has grown slow: the LADX oracle's Python self-tests (`test-ladx-oracle`, ~11 s) and the case-tree check (`ladx-cases`, ~7.6 s) run in it, against `CLAUDE.md`'s "never make it slower" rule.

## Solution

A pure C99 LADX library is started under `ladx/` with two modules: a SHA-512 module and a CPython `random.Random` port (`pyrandom`). Its unit tests live in `tests/ladx/unit/`, built by `tests/ladx/Makefile`'s new `test`, `lint` and `unit-arm` targets. The RNG is proven against the committed CPython fixture `tests/ladx/oracle/pyrandom_vectors.txt` (451 lines, every method LADXR calls, written by CPython 3.13 in Phase 1 step 8), on the host and under `qemu-arm` with Onion's toolchain; SHA-512 against FIPS 180-4's vectors.

The root `make test` runs LADX's unit tests and no longer runs `test-ladx-oracle` and `ladx-cases`; `make check` runs those two as separate targets, so the net checks exactly what it checked before. `make lint` and `make unit-arm` cover `ladx/` and `tests/ladx`. `make test`'s time before and after is recorded in the step note.

## User Stories

1. As the plan-pilot running later Phase 3 steps, I want a `LadxPyRandom` generator seeded from bytes exactly like `random.Random(bytes)`, so that the world setup, pool and fill draw the same numbers as LADXR.
2. As the port of `randomizer.py`, I want to copy one generator's state into another, so that `random.setstate(self.rnd.getstate())` becomes a struct assignment between the `rnd` and the global generator.
3. As the port of the enemizer, I want to seed a second, independent generator from the same seed bytes, so that `random.Random(seed)` in `patches/enemies.py` is reproduced.
4. As the port of the race setting, I want `random()` to return CPython's 53-bit double, bit for bit, so that the one draw race seeds pull first shifts the stream identically.
5. As the port of the fill, I want `randrange(start, stop)` and `randint(a, b)` with CPython's rejection sampling on `getrandbits(bit_length(n))`, so that every draw and its word count match.
6. As the port of the fill and the item pool, I want `choice` over a length returning an index, so that I can pick from any C array.
7. As the port of the fill, I want `shuffle` of any C array (element size given), with CPython's reversed Fisher–Yates, so that shuffled lists come out in LADXR's order.
8. As the port of `itempool.py`, `worldSetup.py` and `randomizer.py`'s spot choice, I want `choices` with weights and `k`, with CPython's cumulative weights, `random() * total` and `bisect_right` with `hi = n - 1`, so that weighted picks match for integer and float weights.
9. As the port of whatever calls `rnd.uniform`, I want `uniform(a, b)` computed exactly as `a + (b - a) * random()`, never fused into an FMA, so that its float matches on x86-64 and on the Cortex-A7.
10. As the port of any `getrandbits(k)` caller, I want `getrandbits` for `0 <= k <= 64`, words filled least-significant first and the last word shifted, so that wide draws match.
11. As the RNG trace of Phase 3 step 4, I want the generator to count the MT words it has drawn since seeding, and that count to travel with the state when it is copied, so that each traced call's word count equals the oracle's `TracingRandom` count.
12. As the port of `patches/maze.py`, I want `choice` on an empty sequence to report an `IndexError`-kind failure without drawing, so that the C port refuses the same `goal=maze` seeds LADXR crashes on.
13. As a caller, I want every refused call (empty range, empty choice, bad weights, an unsupported argument) to leave the generator untouched, so that an error never shifts the stream silently.
14. As a caller, I want the failure kinds `IndexError`, `ValueError` and "unsupported" told apart, so that a later outcome line can name what LADXR raised.
15. As the port's author, I want a SHA-512 module with a streaming and a one-shot interface, so that the seeding hashes seeds of any length with no allocation.
16. As a reviewer, I want SHA-512 proven on FIPS 180-4's vectors (empty, `abc`, the 896-bit two-block message, one million `a`) and on padding boundaries computed by Python's `hashlib`, so that a padding or length-encoding bug cannot hide.
17. As a reviewer, I want `test_pyrandom` to replay every line of `pyrandom_vectors.txt` (words, random, getrandbits, randrange, randint, choice, shuffle, choices, uniform and the scripts with getstate/setstate and the final word position), so that every method is proven against values CPython printed.
18. As a reviewer, I want `test_pyrandom` to fail on an unknown line kind, an unknown script op, a malformed line, or a fixture missing any of its ten kinds or any script op, so that a truncated or changed fixture cannot pass quietly.
19. As a reviewer, I want floats compared by bits after parsing `float.hex` with C99 `strtod`, so that a last-bit difference fails.
20. As a reviewer, I want the seeds with leading zero bytes and the empty seed (both in the fixture) to pass, so that CPython's stripping of high zero words in `init_by_array`'s key is proven.
21. As the owner, I want the same tests to pass under `qemu-arm` built by Onion's toolchain with Onion's flags, so that the device's floating point (VFPv4, where FMA exists) gives the same numbers.
22. As an agent on the fast loop, I want `make test` to run LADX's unit tests and be faster than before, so that `test-ladx-oracle` and `ladx-cases` no longer cost ~18 s each run.
23. As an agent before a push, I want `make check` to still run `test-ladx-oracle` and `ladx-cases`, so that the net checks exactly what it checked before.
24. As an agent, I want `make lint` to build every `ladx/` source with `-std=c99 -pedantic-errors`, so that no compiler extension slips into the LADX library.
25. As an agent, I want `make unit-arm` to also build and run `tests/ladx`'s unit tests under `qemu-arm`, so that the cross build covers LADX from its first module.
26. As a future agent, I want `tests/ladx/README.md` to state the new targets' contract in one section and `CLAUDE.md` to give one line per target with a pointer, so that the contract has one home.
27. As the owner, I want `make test`'s time before and after recorded in the step note, so that the "never make it slower" rule is visibly kept.
28. As a maintainer, I want `ladx/` built with only `ladx/include` on its include path and never including `alttp/` or `app/`, so that the two libraries stay independent as §2 requires.
29. As a maintainer, I want no `malloc` and no platform library in `ladx/`, so that the same code runs on the host, the Miyoo and qemu.

## Implementation Decisions

### Library layout and naming

- New tree `ladx/include/` (public headers) and `ladx/src/` (sources); `ladx/host/` is not created in this step (no host code yet). Prefix `ladx_` for functions and `LADX_` for macros, `Ladx` for types, mirroring `alttp/`'s `rando_`.
- Two modules: `sha512` (header `ladx_sha512.h`) and `pyrandom` (header `ladx_pyrandom.h`, source `ladx/src/pyrandom.c`, the name the plan gives).
- SHA-512 lives in `ladx/`, not in a `common/` library: `alttp/` has no SHA-512, so it has one user (§2: `common/` only with two real users).
- No allocation, no `<stdio.h>` in the library, only `<stdint.h>`, `<stddef.h>`, `<string.h>` and `<math.h>`-free arithmetic (`isfinite` is replaced by a comparison-based finite check so `-lm` is never needed; decided to keep the library link-free).

### SHA-512 interface

- A context type with `init`, `update(bytes, size)` and `final(digest[64])`, plus a one-shot `ladx_sha512(bytes, size, digest)`. `update` accepts any number of calls of any size (`NULL` with size 0 allowed, as `rando_md5`). The message length is counted in 128 bits as FIPS 180-4 states, with sizes up to `SIZE_MAX` handled.

### `LadxPyRandom` interface (the deep module)

- `LadxPyRandom` is a plain value struct: the 624-word MT state, the index, and `words`, a `uint64_t` count of MT words drawn since seeding. **Copying the struct is `getstate`/`setstate`**: no functions for them; the header states this contract, and that `words` travels with the copy (the fixture's script positions are computed by state equality, so a restored state has its saved position).
- `ladx_pyrandom_seed(rng, bytes, size)`: CPython version-2 seeding of a `bytes` seed. The integer is `bytes || SHA-512(bytes)` read big-endian; `init_by_array`'s key is its 32-bit words least significant first, with high zero words dropped (the key is at least one word). The key words are read on the fly from the seed bytes and the 64-byte digest, so seeds of any length need no buffer. Integer seeds, `None` seeds and version 1 are not ported (LADXR seeds only from bytes: `randomizer.py:29`, `patches/enemies.py:577`).
- Methods, the oracle's `ladx_oracle.COVERED` list (the RNG methods LADXR calls, which `TracingRandom` refuses to go beyond): `random`, `getrandbits`, `randrange`, `randint`, `choice`, `choices`, `shuffle`, `uniform`, plus seeding and the state copy. `sample`, named in §2's list with "…", is not called by LADXR at the pin (grep of the checkout: no `sample(`) and has no fixture, so it is not ported; recorded here as the decision.
- Signatures (decided; the exact spelling is the implementer's):
  - `double ladx_pyrandom_random(rng)` — `(a * 67108864.0 + b) * (1.0 / 9007199254740992.0)` with `a = w >> 5`, `b = w >> 6`; 2 words.
  - `int ladx_pyrandom_getrandbits(rng, int k, uint64_t *out)` — `0 <= k <= 64`; `k == 0` gives 0 and draws nothing; `k <= 32` one word shifted right by `32 - k`; wider: words least significant first, the last shifted. `k < 0` is a `ValueError`; `k > 64` unsupported.
  - `int ladx_pyrandom_randrange(rng, int64_t start, int64_t stop, int64_t *out)` — step 1 only (LADXR never passes a step); width `stop - start` computed in `uint64_t`; `start + randbelow(width)`, `randbelow(n)` = `getrandbits(bit_length(n))` retried while `>= n`. `stop <= start` is a `ValueError`.
  - `int ladx_pyrandom_randint(rng, int64_t a, int64_t b, int64_t *out)` — `randrange(a, b + 1)`; `b == INT64_MAX` unsupported.
  - `int ladx_pyrandom_choice(rng, size_t n, size_t *index)` — `randbelow(n)`; `n == 0` an `IndexError`, no draw.
  - `void ladx_pyrandom_shuffle(rng, void *base, size_t count, size_t size)` — `for i = count-1 down to 1: j = randbelow(i + 1); swap(i, j)`, swapping bytes in place (no temporary buffer).
  - `int ladx_pyrandom_choices(rng, const double *weights, size_t n, size_t k, size_t *out)` — CPython 3.13's weighted path: cumulative sums left to right in `double` (exact for integer weights below 2^53, as Python's int sums), `total = cum[n-1]`; for each of the `k` picks, `x = random() * total` and the first index `i < n - 1` with `x < cum[i]`, else `n - 1` (`bisect_right(cum, x, 0, n - 1)`), found by a linear running sum so no cumulative buffer is needed. Errors, all before any draw: `n == 0` an `IndexError`; `total <= 0` or not finite a `ValueError`; a negative or NaN weight unsupported (CPython accepts negatives, but its bisection on a non-monotone list is not a linear scan; LADXR's weights are all positive). `weights == NULL` (the unweighted path) is unsupported: LADXR at the pin always passes weights (`itempool.py:432,434`, `worldSetup.py:270`, `randomizer.py:368`).
  - `double ladx_pyrandom_uniform(rng, double a, double b)` — `a + (b - a) * random()`.
- Status: an enum `LADX_PYRANDOM_OK` (0), `LADX_PYRANDOM_INDEX_ERROR`, `LADX_PYRANDOM_VALUE_ERROR`, `LADX_PYRANDOM_UNSUPPORTED`. Every non-OK return leaves the struct byte-identical (no word drawn, `words` unchanged) and the out-parameter untouched. Mirrors what CPython 3.13 raises before drawing.
- **Floating-point contraction trap**: `uniform` (and `choices`' `random() * total`, harmless alone) must never be contracted into an FMA. Onion's flags include `-mfpu=neon-vfpv4`, which has FMA; GCC contracts by default in GNU modes. `tests/ladx/Makefile` passes `-ffp-contract=off` on every LADX build (host, lint, ARM) in addition to `-std=c99`, and `pyrandom.c` carries a one-line trap comment at `uniform`. The fixture's `uniform` lines under `make unit-arm` are the proof. Phase 4, which builds `ladx/` into the app, must keep the flag (noted in the README section).
- Determinism: no global state; any number of independent generators.

### Build and targets

- `tests/ladx/Makefile` (help stays the default target and lists the new targets):
  - `CFLAGS := -std=c99 -Wall -Wextra -Werror -ffp-contract=off`; library objects built with only `-I ladx/include`; archive `build/libladx.a` (rm-first `ar rcs`, as `alttp`'s).
  - `UNIT_TESTS := test_sha512 test_pyrandom`; `test` builds and runs them from `tests/ladx` (fixture paths relative: `oracle/pyrandom_vectors.txt`) and prints `test: <n> LADX unit tests ran`.
  - `lint`: every `ladx/src` source with `-pedantic-errors`, printing `lint: <n> ladx/ sources build with -pedantic-errors`.
  - `unit-arm`: the same as `tests/alttp`'s (`onion-toolchain.sh`, the same `ONION_CFLAGS`, `-O2 -static -D_FILE_OFFSET_BITS=64`, the `file` check for 32-bit ARM EABI5, `qemu-arm` on PATH or a `setup:` line), run from `tests/ladx`, printing `unit-arm: <n> LADX unit tests passed under qemu-arm`. No `--fixtures` mode: like ALttP's `test_prng`, `test_pyrandom` always replays its fixture.
  - Build output under `tests/ladx/build/` (already git-ignored by `build/`).
- LADX test support: `tests/ladx/unit/support.{c,h}` with `test_fail` only (same output format as ALttP's: `<test>: FAIL <message>`, exit 1). ALttP's `support.h` is not reused: it includes ALttP headers, and `tests/ladx` must not depend on `alttp/`.
- Root `Makefile`:
  - New `test-ladx` = `$(MAKE) -C tests/ladx test`.
  - `test: test-rando test-ladx test-template rom-guard ladx-tables ladx-extract` (`ladx-tables` and `ladx-extract` stay: ~0.3 s each).
  - `check: test test-ladx-oracle ladx-cases lint logic selftest e2e e2e-combos` — the two moved targets keep their exact commands.
  - `lint` runs `tests/alttp`'s lint then `tests/ladx`'s; `unit-arm` runs both games' `unit-arm`.
  - Help lines: `test-ladx` added; `test-ladx-oracle` and `ladx-cases` now say "run by make check"; `lint` says `alttp/`, `ladx/` and `app/`; `unit-arm` says both games.
- CI is unchanged: it runs `make -k check`, which still covers everything.

### Documentation (one home per contract)

- `tests/ladx/README.md` gets a section "Unit tests and lint" holding the full contract of `tests/ladx`'s `test`, `lint` and `unit-arm` (what they build, flags including `-ffp-contract=off` and why, output lines, `setup:` lines, where fixtures are read from).
- `CLAUDE.md`: the Layout gets `ladx/` (library, `include/`, `src/`, pure C99, nothing of `alttp/` or `app/`) and `tests/ladx/unit/`; Checks: `make test`'s definition lists `test-ladx` and drops `test-ladx-oracle` and `ladx-cases`, which move to `make check`'s definition; the `make ladx-cases` paragraph says "run by `make check`"; `lint` and `unit-arm` mention `ladx/`; one line per new target with a pointer to the README section. The contract is not restated in `CLAUDE.md`.

### Step note

- `make test`'s wall time is measured before any change and after the last chunk, on the same host, three runs each, median reported (`time make test`, a warm build: one untimed run first), with the measured times of `test-ladx-oracle` and `ladx-cases` alone (at writing: ~11.0 s and ~7.6 s). These numbers go in the step note `### Step 3.1`.

## Implementation Chunks

1. **LADX library skeleton and SHA-512.** `ladx/include`, `ladx/src` with the SHA-512 module; `tests/ladx/unit/support.{c,h}` and `test_sha512`; `tests/ladx/Makefile`'s `test` and `lint`; root `test-ladx` added to `make test`, root `lint` extended. Measure `make test`'s "before" time first, before touching anything.
2. **`pyrandom` and its fixture replay.** The `LadxPyRandom` module and `test_pyrandom` (fixture replay plus the error and state-copy tests).
3. **Targets moved, ARM, docs.** `test-ladx-oracle` and `ladx-cases` moved from `make test` to `make check`; `tests/ladx`'s `unit-arm` and the root `unit-arm`; help lines; `tests/ladx/README.md` section; `CLAUDE.md`; the "after" timing for the step note. `make unit-arm` is run if docker and `qemu-user` are available; else its `setup:` line is recorded in the step note as a setup problem.

## Testing Decisions

- Good tests here drive only the public interface (`ladx_sha512*`, `ladx_pyrandom_*`) and compare with values CPython or FIPS 180-4 printed; none reads the MT array or the SHA-512 internals. The one exception by contract is `words`, which the header documents as public.
- `test_sha512`: FIPS 180-4 / NIST example vectors (empty string, `abc`, the 896-bit `abcdefghbcdefghi…` message, one million `a` through repeated `update` calls); padding boundaries 111, 112, 127, 128, 239 and 240 bytes of a fixed byte pattern, expected digests computed once with Python's `hashlib.sha512` and written in the test with the generating one-liner as a comment (as `test_md5`'s padding cases); streaming split at every offset of a 300-byte message equals the one-shot digest; `NULL, 0` equals the empty digest.
- `test_pyrandom`, fixture replay of `oracle/pyrandom_vectors.txt` (relative to `tests/ladx`): one fresh generator per line seeded from the hex seed (`-` = empty); each kind checked as its header describes (`words`: 16 `getrandbits(32)`; `random`/`uniform`: bits of `strtod` of `float.hex`; `getrandbits` up to 64 bits via `strtoull`; `randrange`/`randint` with negative bounds; `choice` indexes; `shuffle` of `0..n-1`; `choices` with weights parsed by `strtod` — decimal or `float.hex` — and `k = 8`; `script`: every op in order, `getstate`/`setstate` as struct copies, results compared field by field, then the generator's `words` equal to the line's final position). The test fails on an unknown kind or op, a malformed line, a line count of zero, or a fixture lacking any of the ten kinds or any of the ten script ops; it prints `test_pyrandom: <n> fixture lines ok (<per kind counts>)`.
- `test_pyrandom` also checks, outside the fixture: every refused call (`randrange` with `stop <= start`, `choice(0)`, `choices` with `n == 0`, zero total, a negative weight, NULL weights, `getrandbits(-1)` and `(65)`, `randint` with `b == INT64_MAX`) returns its documented kind and leaves the struct `memcmp`-equal to a copy taken before; `words` counts 2 per `random`, `ceil(k/32)` per `getrandbits`, and grows across MT regeneration (more than 624 words); a copied state replays the same draws as the original; two generators seeded alike stay independent.
- Prior art: `tests/alttp/unit/test_prng.c` (self-contained fixture replay of an RNG run under `make test`, no `--fixtures` mode) and `tests/alttp/unit/test_md5.c` (hash vectors and padding boundaries).
- Both tests run under `make test` (host gcc) and `make unit-arm` (Onion's toolchain, `qemu-arm`); the `-pedantic-errors` build of `ladx/src` runs under `make lint`.
- The moved targets are verified by `make check` still printing their summary lines (`ladx-cases: ...`, the unittest summary) and `make test` no longer printing them.

## Out of Scope

- Integer and `None` seeds, seed version 1, `sample`, `randrange` with a step, unweighted `choices`, `cum_weights`, `gauss` and every other `random` method LADXR does not call at the pin.
- The RNG trace format and the tracing hooks (Phase 3 step 4 builds them on `words`).
- Any LADXR logic, settings table, item pool or ROM write (Phase 3 steps 2 onward); `ladx/host/` and any CLI.
- MD5 for the title screen (step 6.4) and a `common/` library.
- Changing what `test-ladx-oracle` or `ladx-cases` check, or CI's workflow.
- `make logic`, `selftest`, `e2e`, `measure` for LADX (later steps).

## Further Notes

- The fixture was written by CPython 3.13.18 (step 1.8's note); the ported algorithms (`_randbelow_with_getrandbits`, reversed Fisher–Yates `shuffle`, the 3.9+ `choices`, `random_random`, `init_by_array`) are stable across 3.9–3.13, so the fixture pins them.
- A seed whose `bytes || SHA-512` starts with zero bytes (e.g. `00`) loses those bytes as an integer; the high zero words are dropped from the key. The fixture's `00` seed covers it; getting it wrong changes every draw of that seed.
- `getrandbits` is overridden together with `random` in the oracle's `TracingRandom` for a reason that matters here too: CPython's `randrange`, `choice` and `shuffle` go through `getrandbits` (never `random()`), so the C port must use rejection sampling on `getrandbits`, not float scaling.
