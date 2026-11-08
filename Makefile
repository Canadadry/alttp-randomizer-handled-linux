.PHONY: help install test test-rando test-ladx ui-cli test-template rom-guard test-ladx-oracle ladx-cases ladx-tables lint logic selftest e2e e2e-combos e2e-synthetic e2e-arm unit-arm sdl sdl-pc sdl-goldens sdl-goldens-add screenshots mockups theme-compare theme-art theme-art-write sdl-arm package patch-header measure-compare mac mac-rom mac-run mac-clean check

help:
	@echo "make install        nothing to install: host gcc and python3 are enough"
	@echo "make test           the fast loop: unit tests, on the host, in seconds"
	@echo "make test-rando     the randomizer's unit tests (tests/alttp)"
	@echo "make test-ladx      LADX's unit tests (tests/ladx/unit), run by make test"
	@echo "make test-template  the template's own tooling tests (.claude/), run by make test"
	@echo "make test-ladx-oracle the LADX oracle's helper unit tests (tests/ladx/oracle), no LADXR, no network, run by make check"
	@echo "make ladx-cases     tests/ladx/cases/ and combos/ checked against tier1.txt and combos.txt, their goldens and size budget, no ROM, no LADXR, run by make check"
	@echo "make ladx-tables    data/ladx/ (LADX patch tables) parsed and checked against the inventory, no ROM, no LADXR, run by make test"
	@echo "make rom-guard      fails if git's index holds a .gb/.gbc/.sfc/.smc file or a LADX/ALttP ROM by digest, run by make test"
	@echo "make ui-cli         builds tests/alttp/build/ui_cli, the agent's text view of the app"
	@echo "make lint           alttp/, ladx/ and app/ built as strict ISO C99 (-pedantic-errors)"
	@echo "make logic          unit tests replaying the oracle fixtures of both games (tests/alttp/oracle, tests/ladx/oracle)"
	@echo "make selftest       the e2e drivers' own self-tests (tests/alttp, then tests/ladx)"
	@echo "make e2e            every case of tests/alttp/cases, then tests/ladx/cases, diffed against its golden"
	@echo "make e2e-combos     every case of tests/alttp/combos, then tests/ladx/combos, diffed against its golden, kept out of e2e"
	@echo "make e2e-synthetic  LADX's ROM-free regression reference: C's outputs on a synthetic base against tests/ladx/synthetic_refs.txt, run by make check"
	@echo "make e2e-arm        the e2e cases on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make unit-arm       every unit test of both games (tests/alttp, tests/ladx) on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make sdl            sdl-pc then sdl-arm, kept out of check"
	@echo "make sdl-pc         the SDL PC build, its SDL tests, headless smoke test and golden comparison (needs libsdl1.2-dev, libsdl-ttf2.0-dev, libsdl-image1.2-dev)"
	@echo "make sdl-goldens    writes the SDL goldens once into tests/alttp/sdl/goldens/, refused if any exist, kept out of check"
	@echo "make sdl-goldens-add STATES=\"a b\" writes only those states' missing goldens, rendered twice; refused if one exists or the host's fingerprint differs, kept out of check"
	@echo "make screenshots    the README's pictures: 4 states drawn with themes/alttp twice, byte-identical, written to docs/screenshots/ (needs sdl-pc's packages)"
	@echo "make mockups        the mockups of design/mockups/ checked: rendered offline by Chromium with the local font, kept out of check (needs sdl-pc's packages, node, Playwright's Chromium)"
	@echo "make theme-compare  the mockups beside the app's captures with a diff, a score and the measures (offsets.txt, outside px, text offsets) per mockup, into tests/alttp/build/theme-compare/ (THEME=<dir>, default themes/alttp; needs Playwright's Chromium and sdl-pc's packages; no threshold)"
	@echo "make theme-art      the art of themes/alttp re-rendered from design/mockups/ by Chromium and compared pixel by pixel (THEME=<dir>; needs node and Playwright's Chromium)"
	@echo "make theme-art-write the art rendered twice, refused unless byte-identical, written into themes/alttp (THEME=<dir>; needs node and Playwright's Chromium)"
	@echo "make sdl-arm        the SDL app compiled and linked by Onion's toolchain, run nowhere (needs docker)"
	@echo "make package        the release zip tests/alttp/build/package/ZeldaRando.zip: sdl-arm stripped, ABI-checked, with app/package/, data/base.bps and themes/alttp as themes/default (needs docker)"
	@echo "make patch-header   the base patch checked like make test checks data/base.bps (BPS=<file>, default data/base.bps), kept out of check"
	@echo "make measure-compare the generator timed against a worktree of 40314c3, interleaved, +-10% verdicts, kept out of check"
	@echo "make mac            macOS: the SDL app and the fake SD card tmp/sd (targets/mac), kept out of check"
	@echo "make mac-rom        macOS: ROM=<file.sfc> copied to tmp/sd, copier header stripped, JP 1.0 checked"
	@echo "make mac-run        macOS: builds then runs the SDL app on tmp/sd"
	@echo "make mac-clean      macOS: removes the local SDL_ttf and SDL_image builds and the app binary"
	@echo "make check          the net: everything that must be green before a push"

install:
	@echo "install: no dependency to install (host gcc, make, python3)"

# The fast loop. Keep it a few seconds, with no network and no external service.
test: test-rando test-ladx test-template rom-guard ladx-tables

test-rando:
	$(MAKE) -C tests/alttp test

test-ladx:
	$(MAKE) -C tests/ladx test

# The template's tooling under .claude/: the pre-push hook and plan-pilot's
# progress.md hook. Kept apart from the project's own tests.
test-template:
	python3 -m unittest discover -s .claude/hooks/tests
	python3 -m unittest discover -s .claude/skills/plan-pilot/tests

rom-guard:
	python3 -I tests/rom_guard.py

test-ladx-oracle:
	python3 -I -B -m unittest discover -s tests/ladx/oracle -p 'test_*.py' -q

ladx-cases:
	python3 -I -B tests/ladx/oracle/ladx_cases.py check

ladx-tables:
	python3 -I -B tests/ladx/oracle/tables.py check

ui-cli:
	$(MAKE) -C tests/alttp ui-cli

lint:
	$(MAKE) -C tests/alttp lint
	$(MAKE) -C tests/ladx lint

logic:
	$(MAKE) -C tests/alttp logic
	$(MAKE) -C tests/ladx logic

selftest:
	$(MAKE) -C tests/alttp selftest
	$(MAKE) -C tests/ladx selftest

e2e:
	$(MAKE) -C tests/alttp e2e
	$(MAKE) -C tests/ladx e2e

e2e-combos:
	$(MAKE) -C tests/alttp e2e-combos
	$(MAKE) -C tests/ladx e2e-combos

e2e-synthetic:
	$(MAKE) -C tests/ladx e2e-synthetic

# Outside the net: needs docker and qemu-user.
e2e-arm:
	$(MAKE) -C tests/alttp e2e-arm

# Outside the net: needs docker and qemu-user.
unit-arm:
	$(MAKE) -C tests/alttp unit-arm
	$(MAKE) -C tests/ladx unit-arm

# Outside the net: sdl-pc needs the SDL 1.2, SDL_ttf 2.0 and SDL_image 1.2 packages, sdl-arm docker.
sdl:
	$(MAKE) -C tests/alttp sdl

sdl-pc:
	$(MAKE) -C tests/alttp sdl-pc

sdl-arm:
	$(MAKE) -C tests/alttp sdl-arm

# Outside the net, run once: refused when the goldens folder already holds goldens.
sdl-goldens:
	$(MAKE) -C tests/alttp sdl-goldens

# Outside the net: adds the goldens of STATES only; never rewrites an existing one.
sdl-goldens-add:
	$(MAKE) -C tests/alttp sdl-goldens-add STATES="$(STATES)"

# Outside the net: needs sdl-pc's packages. Rewrites docs/screenshots/; commit them after a theme change.
screenshots:
	$(MAKE) -C tests/alttp screenshots

# Outside the net: needs sdl-pc's packages, node and Playwright's Chromium.
mockups:
	$(MAKE) -C tests/alttp mockups

# Outside the net: needs sdl-pc's packages, node and Playwright's Chromium. THEME is relative to this directory.
THEME ?= themes/alttp
theme-compare:
	$(MAKE) -C tests/alttp theme-compare COMPARE_THEME=$(abspath $(THEME))

# Outside the net: needs node and Playwright's Chromium. THEME is relative to this directory.
theme-art:
	$(MAKE) -C tests/alttp theme-art ART_THEME=$(abspath $(THEME))

theme-art-write:
	$(MAKE) -C tests/alttp theme-art-write ART_THEME=$(abspath $(THEME))

# Outside the net: needs docker.
package:
	$(MAKE) -C tests/alttp package

# Outside the net; make test checks data/base.bps. BPS is relative to this directory.
patch-header:
	$(MAKE) -C tests/alttp patch-header $(if $(BPS),BPS=$(abspath $(BPS)))

# Outside the net: macOS only (targets/mac/README.md). ROM is relative to this directory.
mac:
	$(MAKE) -C targets/mac

mac-rom:
	$(MAKE) -C targets/mac rom $(if $(ROM),ROM=$(abspath $(ROM)))

mac-run:
	$(MAKE) -C targets/mac run

mac-clean:
	$(MAKE) -C targets/mac clean

# Outside the net: creates or reuses tmp/measure-ref-40314c3.
measure-compare:
	$(MAKE) -C tests/alttp measure-compare

# The net. A change that keeps it green is correct; one that breaks it is a bug.
check: test test-ladx-oracle ladx-cases lint logic selftest e2e e2e-combos e2e-synthetic
