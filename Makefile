.PHONY: help install test test-rando ui-cli test-template lint logic selftest e2e e2e-combos e2e-arm unit-arm sdl sdl-pc sdl-goldens mockups theme-compare sdl-arm package patch-header measure-compare check

help:
	@echo "make install        nothing to install: host gcc and python3 are enough"
	@echo "make test           the fast loop: unit tests, on the host, in seconds"
	@echo "make test-rando     the randomizer's unit tests (tests/rando)"
	@echo "make test-template  the template's own tooling tests (.claude/), run by make test"
	@echo "make ui-cli         builds tests/rando/build/ui_cli, the agent's text view of the app"
	@echo "make lint           rando/ and app/ built as strict ISO C99 (-pedantic-errors)"
	@echo "make logic          unit tests replaying the oracle fixtures (tests/rando/oracle)"
	@echo "make selftest       the e2e driver's own self-test"
	@echo "make e2e            every case of tests/rando/cases diffed against its golden"
	@echo "make e2e-combos     every case of tests/rando/combos diffed against its golden, kept out of e2e"
	@echo "make e2e-arm        the e2e cases on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make unit-arm       every unit test on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make sdl            sdl-pc then sdl-arm, kept out of check"
	@echo "make sdl-pc         the SDL PC build, its SDL tests, headless smoke test and golden comparison (needs libsdl1.2-dev, libsdl-ttf2.0-dev, libsdl-image1.2-dev)"
	@echo "make sdl-goldens    writes the SDL goldens once into tests/rando/sdl/goldens/, refused if any exist, kept out of check"
	@echo "make mockups        the mockups of design/mockups/ checked: rendered offline by Chromium with the local font, kept out of check (needs sdl-pc's packages, node, Playwright's Chromium)"
	@echo "make theme-compare  the mockups beside the app's captures with a diff and a score per mockup, into tests/rando/build/theme-compare/ (THEME=<dir>, default themes/alttp; needs Playwright's Chromium and sdl-pc's packages; no threshold)"
	@echo "make sdl-arm        the SDL app compiled and linked by Onion's toolchain, run nowhere (needs docker)"
	@echo "make package        the release zip tests/rando/build/package/ALttPR.zip: sdl-arm stripped, ABI-checked, with app/package/, data/base.bps and themes/classic as themes/default (needs docker)"
	@echo "make patch-header   the base patch checked like make test checks data/base.bps (BPS=<file>, default data/base.bps), kept out of check"
	@echo "make measure-compare the generator timed against a worktree of 40314c3, interleaved, +-10% verdicts, kept out of check"
	@echo "make check          the net: everything that must be green before a push"

install:
	@echo "install: no dependency to install (host gcc, make, python3)"

# The fast loop. Keep it a few seconds, with no network and no external service.
test: test-rando test-template

test-rando:
	$(MAKE) -C tests/rando test

# The template's tooling under .claude/: the pre-push hook and plan-pilot's
# progress.md hook. Kept apart from the project's own tests.
test-template:
	python3 -m unittest discover -s .claude/hooks/tests
	python3 -m unittest discover -s .claude/skills/plan-pilot/tests

ui-cli:
	$(MAKE) -C tests/rando ui-cli

lint:
	$(MAKE) -C tests/rando lint

logic:
	$(MAKE) -C tests/rando logic

selftest:
	$(MAKE) -C tests/rando selftest

e2e:
	$(MAKE) -C tests/rando e2e

e2e-combos:
	$(MAKE) -C tests/rando e2e-combos

# Outside the net: needs docker and qemu-user.
e2e-arm:
	$(MAKE) -C tests/rando e2e-arm

# Outside the net: needs docker and qemu-user.
unit-arm:
	$(MAKE) -C tests/rando unit-arm

# Outside the net: sdl-pc needs the SDL 1.2, SDL_ttf 2.0 and SDL_image 1.2 packages, sdl-arm docker.
sdl:
	$(MAKE) -C tests/rando sdl

sdl-pc:
	$(MAKE) -C tests/rando sdl-pc

sdl-arm:
	$(MAKE) -C tests/rando sdl-arm

# Outside the net, run once: refused when the goldens folder already holds goldens.
sdl-goldens:
	$(MAKE) -C tests/rando sdl-goldens

# Outside the net: needs sdl-pc's packages, node and Playwright's Chromium.
mockups:
	$(MAKE) -C tests/rando mockups

# Outside the net: needs sdl-pc's packages, node and Playwright's Chromium. THEME is relative to this directory.
THEME ?= themes/alttp
theme-compare:
	$(MAKE) -C tests/rando theme-compare COMPARE_THEME=$(abspath $(THEME))

# Outside the net: needs docker.
package:
	$(MAKE) -C tests/rando package

# Outside the net; make test checks data/base.bps. BPS is relative to this directory.
patch-header:
	$(MAKE) -C tests/rando patch-header $(if $(BPS),BPS=$(abspath $(BPS)))

# Outside the net: creates or reuses tmp/measure-ref-40314c3.
measure-compare:
	$(MAKE) -C tests/rando measure-compare

# The net. A change that keeps it green is correct; one that breaks it is a bug.
check: test lint logic selftest e2e e2e-combos
