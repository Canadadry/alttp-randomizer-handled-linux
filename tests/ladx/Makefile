.PHONY: help test lint logic unit-arm selftest e2e e2e-combos measure rom-check rom-check-selftest

CC ?= gcc
# -ffp-contract=off: GCC fuses a*b+c into an FMA by default, and Onion's VFPv4 has one; CPython's floats are never fused.
CFLAGS := -std=c99 -Wall -Wextra -Werror -ffp-contract=off
LADX := ../../ladx
BUILD := build

UNIT_TESTS := test_sha512 test_sha1 test_rom test_ks test_pyrandom test_rng test_run_text test_settings test_requirements test_logic test_world_setup \
	test_itempool test_fill test_wlog test_tables test_encoder
# Run again with --fixtures by make logic and unit-arm.
LOGIC_TESTS := test_logic test_itempool test_fill test_encoder
# Linked with the host layer ladx/host/ too.
HOST_TESTS := test_run_text test_itempool test_fill

LADX_SRC := $(wildcard $(LADX)/src/*.c)
LADX_HDR := $(wildcard $(LADX)/include/*.h $(LADX)/src/*.h)
HOST_SRC := $(wildcard $(LADX)/host/*.c)
HOST_HDR := $(wildcard $(LADX)/host/*.h)
HOST_OBJ := $(patsubst $(LADX)/host/%.c,$(BUILD)/host/%.o,$(HOST_SRC))
LADX_OBJ := $(patsubst $(LADX)/src/%.c,$(BUILD)/obj/%.o,$(LADX_SRC))
LADX_LIB := $(BUILD)/libladx.a
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
SUPPORT_OBJ := $(BUILD)/support.o
LOGIC_FIXTURE_OBJ := $(BUILD)/logic_fixture.o
SYNTH_ROM_OBJ := $(BUILD)/synth_rom.o
# Linked with the synthetic ROM builder unit/synth_rom.c.
SYNTH_TESTS := test_rom test_ks
LOGIC_BINS := $(addprefix $(BUILD)/,$(LOGIC_TESTS))
MAIN_SRC := $(wildcard $(LADX)/host/main/*.c)
GEN := $(BUILD)/ladx_gen
GEN_DIR := $(BUILD)/gen
GEN_OBJ := $(patsubst $(LADX)/src/%.c,$(GEN_DIR)/%.o,$(LADX_SRC)) \
	$(patsubst $(LADX)/host/%.c,$(GEN_DIR)/host/%.o,$(HOST_SRC))
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(LADX)/src/%.c,$(LINT_DIR)/%.o,$(LADX_SRC)) \
	$(patsubst $(LADX)/host/%.c,$(LINT_DIR)/host/%.o,$(HOST_SRC)) \
	$(patsubst $(LADX)/host/main/%.c,$(LINT_DIR)/host/main/%.o,$(MAIN_SRC))

TOOLCHAIN := ../../onion-toolchain.sh
ONION_CFLAGS := -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
ARM_UNIT_DIR := $(BUILD)/arm/unit

TOP := $(abspath ../..)
TABLES := $(TOP)/data/ladx
LADX_ROM ?= tmp/ladx/base.gbc
LADX_GEN ?= tests/ladx/build/ladx_patch
CASES ?=

help:
	@echo "make test              LADX's unit tests (tests/ladx/unit), built with the ladx/ library, run from tests/ladx"
	@echo "make logic             LADX's LOGIC_TESTS run with --fixtures from tests/ladx: the oracle's logic fixtures replayed"
	@echo "make lint              every ladx/ source, ladx/host/ included, built as strict ISO C99 (-pedantic-errors)"
	@echo "make unit-arm          LADX's unit tests built by Onion's toolchain (-ffp-contract=off kept) and run under qemu-arm from tests/ladx (needs docker, qemu-user)"
	@echo "make selftest          the e2e driver run_e2e.py's self-test on planted runs and fake generators"
	@echo "make e2e               build/ladx_gen --project's projection of every golden rom_writes.txt of cases/ compared with write_log.py's, then build/ladx_gen on every case of cases/ (or CASES=\"a b\"), its run.txt's stage projection and claimed groups' rom_writes.txt projections (tables TABLES, default data/ladx) diffed against the golden's per claims.txt"
	@echo "make e2e-combos        the same on every case of combos/"
	@echo "make measure           build/ladx_gen --time (the run stopped after the fill) 3 times on every case and combo, the minimum kept, projected onto the Cortex-A7; report in build/measure/report.txt; outside the net"
	@echo "make rom-check          the generator LADX_GEN (default tests/ladx/build/ladx_patch) run on every case and combo (or CASES=\"a b\") with LADX_ROM (default tmp/ladx/base.gbc), each output's SHA-1 compared with its golden rom_sha1, after the self-test; outside the net"
	@echo "make rom-check-selftest rom_check.py's self-test alone: a synthetic ROM, a planted case tree and fake generators, no ROM"

selftest:
	@python3 -I -B run_e2e.py --self-test

e2e: $(GEN)
	@python3 -I -B run_e2e.py --projections --cases cases --gen $(GEN) $(CASES)
	@python3 -I -B run_e2e.py --cases cases --gen $(GEN) --tables $(TABLES) $(CASES)

e2e-combos: $(GEN)
	@echo "e2e-combos: the combination cases"
	@python3 -I -B run_e2e.py --projections --cases combos --gen $(GEN) $(CASES)
	@python3 -I -B run_e2e.py --cases combos --gen $(GEN) --tables $(TABLES) $(CASES)

measure: $(GEN)
	@CC='$(CC)' ./measure.sh $(GEN) $(BUILD)/measure

# Paths are repository-relative: the driver runs from the repository's root.
rom-check:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --rom $(LADX_ROM) --gen $(LADX_GEN) $(CASES)

rom-check-selftest:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --self-test

# test_logic writes a differing graph's text into build/logic.
test: $(UNIT_BINS)
	@mkdir -p $(BUILD)/logic
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@echo "test: $(words $(UNIT_BINS)) LADX unit tests ran"

logic: $(LOGIC_BINS)
	@mkdir -p $(BUILD)/logic
	@set -e; for t in $(LOGIC_BINS); do ./$$t --fixtures; done
	@echo "logic: $(words $(LOGIC_TESTS)) LADX unit tests replayed the oracle fixtures"

lint: $(LINT_OBJ)
	@echo "lint: $(words $(LADX_SRC)) ladx/ sources and $(words $(HOST_SRC) $(MAIN_SRC)) ladx/host/ sources build with -pedantic-errors"

# _FILE_OFFSET_BITS=64 as tests/alttp's: under qemu-arm a 32-bit readdir fails with EOVERFLOW on ext4.
unit-arm:
	@command -v qemu-arm >/dev/null 2>&1 || { echo "setup: qemu-arm not found on PATH (install qemu-user)" >&2; exit 1; }
	@rm -rf $(ARM_UNIT_DIR) && mkdir -p $(ARM_UNIT_DIR)/obj
	@echo "unit-arm: building $(words $(UNIT_TESTS)) LADX unit tests in $(ARM_UNIT_DIR) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c 'set -e; cc="$${CROSS_COMPILE}gcc"; f="$(CFLAGS) -O2 $(ONION_CFLAGS) -static -D_FILE_OFFSET_BITS=64 -I$(LADX)/include -I$(LADX)/host"; \
		for s in $(LADX_SRC) $(HOST_SRC) unit/support.c unit/logic_fixture.c unit/synth_rom.c; do "$$cc" $$f -c -o $(ARM_UNIT_DIR)/obj/$$(basename $$s .c).o $$s; done; \
		for t in $(UNIT_TESTS); do "$$cc" $$f -Iunit -o $(ARM_UNIT_DIR)/$$t unit/$$t.c $(ARM_UNIT_DIR)/obj/*.o -lm; done'
	@file $(ARM_UNIT_DIR)/test_pyrandom | grep -q 'ELF 32-bit LSB executable, ARM, EABI5' || { file $(ARM_UNIT_DIR)/test_pyrandom; echo "unit-arm: $(ARM_UNIT_DIR)/test_pyrandom is not a 32-bit ARM EABI executable" >&2; exit 1; }
	@qemu-arm --version | head -n 1
	@mkdir -p $(BUILD)/logic
	@set -e; for t in $(UNIT_TESTS); do qemu-arm $(ARM_UNIT_DIR)/$$t; done
	@set -e; for t in $(LOGIC_TESTS); do qemu-arm $(ARM_UNIT_DIR)/$$t --fixtures; done
	@echo "unit-arm: $(words $(UNIT_TESTS)) LADX unit tests and $(words $(LOGIC_TESTS)) fixture replays passed under qemu-arm"

$(BUILD):
	@mkdir -p $@

$(BUILD)/obj/%.o: $(LADX)/src/%.c $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(LADX)/include -c -o $@ $<

# rm first: `ar r` would keep the object of a deleted source.
$(LADX_LIB): $(LADX_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(SUPPORT_OBJ): unit/support.c unit/support.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(SYNTH_ROM_OBJ): unit/synth_rom.c unit/synth_rom.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(addprefix $(BUILD)/,$(SYNTH_TESTS)): $(BUILD)/test_%: unit/test_%.c unit/support.h unit/synth_rom.h $(SUPPORT_OBJ) $(SYNTH_ROM_OBJ) $(LADX_LIB) $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -Iunit -o $@ $< $(SUPPORT_OBJ) $(SYNTH_ROM_OBJ) $(LADX_LIB)

$(LOGIC_FIXTURE_OBJ): unit/logic_fixture.c unit/logic_fixture.h unit/support.h $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -Iunit -c -o $@ $<

$(BUILD)/test_logic: unit/test_logic.c unit/support.h unit/logic_fixture.h $(SUPPORT_OBJ) $(LOGIC_FIXTURE_OBJ) $(LADX_LIB) $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -Iunit -o $@ $< $(SUPPORT_OBJ) $(LOGIC_FIXTURE_OBJ) $(LADX_LIB)

# The host layer sees ladx/include and itself only.
$(BUILD)/host/%.o: $(LADX)/host/%.c $(HOST_HDR) $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(LADX)/include -I$(LADX)/host -c -o $@ $<

$(addprefix $(BUILD)/,$(HOST_TESTS)): $(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(HOST_OBJ) $(LADX_LIB) $(LADX_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -I$(LADX)/host -Iunit -o $@ $< $(SUPPORT_OBJ) $(HOST_OBJ) $(LADX_LIB) -lm

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(LADX_LIB) $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -Iunit -o $@ $< $(SUPPORT_OBJ) $(LADX_LIB)

$(GEN_DIR)/%.o: $(LADX)/src/%.c $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O2 -I$(LADX)/include -c -o $@ $<

$(GEN_DIR)/host/%.o: $(LADX)/host/%.c $(HOST_HDR) $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O2 -I$(LADX)/include -I$(LADX)/host -c -o $@ $<

$(GEN): $(MAIN_SRC) $(GEN_OBJ) $(HOST_HDR) $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -O2 -I$(LADX)/include -I$(LADX)/host -o $@ $(MAIN_SRC) $(GEN_OBJ) -lm

$(LINT_DIR)/%.o: $(LADX)/src/%.c $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(LADX)/include -c -o $@ $<

$(LINT_DIR)/host/%.o: $(LADX)/host/%.c $(HOST_HDR) $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(LADX)/include -I$(LADX)/host -c -o $@ $<

$(LINT_DIR)/host/main/%.o: $(LADX)/host/main/%.c $(HOST_HDR) $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(LADX)/include -I$(LADX)/host -c -o $@ $<
