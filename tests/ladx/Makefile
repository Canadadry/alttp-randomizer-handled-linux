.PHONY: help test lint unit-arm rom-check rom-check-selftest

CC ?= gcc
# -ffp-contract=off: GCC fuses a*b+c into an FMA by default, and Onion's VFPv4 has one; CPython's floats are never fused.
CFLAGS := -std=c99 -Wall -Wextra -Werror -ffp-contract=off
LADX := ../../ladx
BUILD := build

UNIT_TESTS := test_sha512 test_pyrandom test_settings

LADX_SRC := $(wildcard $(LADX)/src/*.c)
LADX_HDR := $(wildcard $(LADX)/include/*.h $(LADX)/src/*.h)
LADX_OBJ := $(patsubst $(LADX)/src/%.c,$(BUILD)/obj/%.o,$(LADX_SRC))
LADX_LIB := $(BUILD)/libladx.a
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
SUPPORT_OBJ := $(BUILD)/support.o
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(LADX)/src/%.c,$(LINT_DIR)/%.o,$(LADX_SRC))

TOOLCHAIN := ../../onion-toolchain.sh
ONION_CFLAGS := -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
ARM_UNIT_DIR := $(BUILD)/arm/unit

TOP := $(abspath ../..)
LADX_ROM ?= tmp/ladx/base.gbc
LADX_GEN ?= tests/ladx/build/ladx_patch
CASES ?=

help:
	@echo "make test              LADX's unit tests (tests/ladx/unit), built with the ladx/ library, run from tests/ladx"
	@echo "make lint              every ladx/ source built as strict ISO C99 (-pedantic-errors)"
	@echo "make unit-arm          LADX's unit tests built by Onion's toolchain (-ffp-contract=off kept) and run under qemu-arm from tests/ladx (needs docker, qemu-user)"
	@echo "make rom-checkthe generator LADX_GEN (default tests/ladx/build/ladx_patch) run on every case and combo (or CASES=\"a b\") with LADX_ROM (default tmp/ladx/base.gbc), each output's SHA-1 compared with its golden rom_sha1, after the self-test; outside the net"
	@echo "make rom-check-selftest rom_check.py's self-test alone: a synthetic ROM, a planted case tree and fake generators, no ROM"

# Paths are repository-relative: the driver runs from the repository's root.
rom-check:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --rom $(LADX_ROM) --gen $(LADX_GEN) $(CASES)

rom-check-selftest:
	cd $(TOP) && python3 -I -B tests/ladx/oracle/rom_check.py --self-test

test: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@echo "test: $(words $(UNIT_BINS)) LADX unit tests ran"

lint: $(LINT_OBJ)
	@echo "lint: $(words $(LADX_SRC)) ladx/ sources build with -pedantic-errors"

# _FILE_OFFSET_BITS=64 as tests/alttp's: under qemu-arm a 32-bit readdir fails with EOVERFLOW on ext4.
unit-arm:
	@command -v qemu-arm >/dev/null 2>&1 || { echo "setup: qemu-arm not found on PATH (install qemu-user)" >&2; exit 1; }
	@rm -rf $(ARM_UNIT_DIR) && mkdir -p $(ARM_UNIT_DIR)/obj
	@echo "unit-arm: building $(words $(UNIT_TESTS)) LADX unit tests in $(ARM_UNIT_DIR) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c 'set -e; cc="$${CROSS_COMPILE}gcc"; f="$(CFLAGS) -O2 $(ONION_CFLAGS) -static -D_FILE_OFFSET_BITS=64 -I$(LADX)/include"; \
		for s in $(LADX_SRC) unit/support.c; do "$$cc" $$f -c -o $(ARM_UNIT_DIR)/obj/$$(basename $$s .c).o $$s; done; \
		for t in $(UNIT_TESTS); do "$$cc" $$f -Iunit -o $(ARM_UNIT_DIR)/$$t unit/$$t.c $(ARM_UNIT_DIR)/obj/*.o; done'
	@file $(ARM_UNIT_DIR)/test_pyrandom | grep -q 'ELF 32-bit LSB executable, ARM, EABI5' || { file $(ARM_UNIT_DIR)/test_pyrandom; echo "unit-arm: $(ARM_UNIT_DIR)/test_pyrandom is not a 32-bit ARM EABI executable" >&2; exit 1; }
	@qemu-arm --version | head -n 1
	@set -e; for t in $(UNIT_TESTS); do qemu-arm $(ARM_UNIT_DIR)/$$t; done
	@echo "unit-arm: $(words $(UNIT_TESTS)) LADX unit tests passed under qemu-arm"

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

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(LADX_LIB) $(LADX_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(LADX)/include -Iunit -o $@ $< $(SUPPORT_OBJ) $(LADX_LIB)

$(LINT_DIR)/%.o: $(LADX)/src/%.c $(LADX_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(LADX)/include -c -o $@ $<
