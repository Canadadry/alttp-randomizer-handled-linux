.PHONY: help install test test-rando test-template lint logic selftest e2e e2e-combos e2e-arm unit-arm patch-header check

help:
	@echo "make install        nothing to install: host gcc and python3 are enough"
	@echo "make test           the fast loop: unit tests, on the host, in seconds"
	@echo "make test-rando     the randomizer's unit tests (tests/rando)"
	@echo "make test-template  the template's own tooling tests (.claude/), run by make test"
	@echo "make lint           rando/ and app/ built as strict ISO C99 (-pedantic-errors)"
	@echo "make logic          unit tests replaying the oracle fixtures (tests/rando/oracle)"
	@echo "make selftest       the e2e driver's own self-test"
	@echo "make e2e            every case of tests/rando/cases diffed against its golden"
	@echo "make e2e-combos     every case of tests/rando/combos diffed against its golden, kept out of e2e"
	@echo "make e2e-arm        the e2e cases on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make unit-arm       every unit test on the Onion cross build under qemu-arm (needs docker, qemu-user)"
	@echo "make patch-header   the base patch's declared sizes checked (BPS=<file>, default data/base.bps), kept out of check"
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

# Outside the net until data/base.bps exists. BPS is relative to this directory.
patch-header:
	$(MAKE) -C tests/rando patch-header $(if $(BPS),BPS=$(abspath $(BPS)))

# The net. A change that keeps it green is correct; one that breaks it is a bug.
check: test lint logic selftest e2e e2e-combos
