.PHONY: help rom-check rom-check-selftest

TOP := $(abspath ../..)
LADX_ROM ?= tmp/ladx/base.gbc
LADX_GEN ?= tests/ladx/build/ladx_patch
CASES ?=

help:
	@echo "make rom-check         the generator LADX_GEN (default tests/ladx/build/ladx_patch) run on every case and combo (or CASES=\"a b\") with LADX_ROM (default tmp/ladx/base.gbc), each output's SHA-1 compared with its golden rom_sha1, after the self-test; outside the net"
	@echo "make rom-check-selftest rom_check.py's self-test alone: a synthetic ROM, a planted case tree and fake generators, no ROM"

# Paths are repository-relative: the driver runs from the repository's root.
rom-check:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --rom $(LADX_ROM) --gen $(LADX_GEN) $(CASES)

rom-check-selftest:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --self-test
