CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -DRANDO_FAULTS
RANDO := ../../rando
BUILD := build

UNIT_TESTS := test_prng test_items test_bosses test_world test_regions test_options test_case_reader test_pool test_prepare test_generate test_spoiler_json test_filler test_playthrough test_postfill test_rom test_rom_items test_rom_settings test_rom_credits test_rom_text test_rom_sram test_rom_checksum test_rom_cosmetics test_crc32 test_bps test_md5 test_patch_header test_assemble test_patch_cli test_name test_record test_seed_save
# No fixture mode: test_prng already replays its vectors in `test`, the others have no fixture.
LOGIC_TESTS := $(filter-out test_prng test_options test_case_reader test_pool test_spoiler_json test_filler test_playthrough test_rom test_rom_items test_rom_settings test_rom_checksum test_rom_cosmetics test_crc32 test_bps test_md5 test_patch_header test_patch_cli test_name test_seed_save,$(UNIT_TESTS))

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
HOST_SRC := $(wildcard $(RANDO)/host/*.c)
HOST_HDR := $(wildcard $(RANDO)/host/*.h)
HOST_OBJ := $(patsubst $(RANDO)/host/%.c,$(BUILD)/host/%.o,$(HOST_SRC))
HOST_LIB := $(BUILD)/librando_host.a
INCLUDES := -I$(RANDO)/include -I$(RANDO)/host
SUPPORT_OBJ := $(BUILD)/support.o $(BUILD)/synthetic_base.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
GEN := $(BUILD)/rando_gen
PATCH_CLI := $(BUILD)/rando_patch
GEN_DIR := $(BUILD)/o2
GEN_OBJ := $(patsubst $(RANDO)/src/%.c,$(GEN_DIR)/obj/%.o,$(RANDO_SRC)) $(patsubst $(RANDO)/host/%.c,$(GEN_DIR)/host/%.o,$(HOST_SRC))
LOGIC_BINS := $(addprefix $(BUILD)/,$(LOGIC_TESTS))

MEASURE := $(BUILD)/measure
MEASURE_CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -fstack-usage -fno-stack-protector -U_FORTIFY_SOURCE
MEASURE_OBJ := $(patsubst $(RANDO)/src/%.c,$(MEASURE)/host-O2/obj/%.o,$(RANDO_SRC)) $(MEASURE)/host-O2/host/case_reader.o
MEASURE_OS_OBJ := $(patsubst $(RANDO)/src/%.c,$(MEASURE)/host-Os/obj/%.o,$(RANDO_SRC))
MEASURE_PROBE := $(MEASURE)/host-O2/size_probe.o
MEASURE_BIN := $(MEASURE)/rando_measure
MEASURE_SWEEP ?= 1024
MEASURE_REPS ?= 5

# patch-header: the base patch whose header test_patch_header --file checks.
BPS ?= ../../data/base.bps

# lint: the library alone, strict ISO C99, so no compiler extension slips in.
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(RANDO)/src/%.c,$(LINT_DIR)/%.o,$(RANDO_SRC))

# e2e-arm: the generator built by Onion's toolchain with Onion's miyoomini
# flags (docs/toolchain.md), run under qemu-arm by the same e2e driver.
TOOLCHAIN := ../../onion-toolchain.sh
ONION_CFLAGS := -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
ARM_DIR := $(BUILD)/arm
ARM_GEN := $(ARM_DIR)/rando_gen
ARM_WRAPPER := $(ARM_DIR)/rando_gen_qemu
ARM_UNIT_DIR := $(ARM_DIR)/unit
ARM_UNIT_SRC := $(RANDO_SRC) $(HOST_SRC) unit/support.c unit/synthetic_base.c

.PHONY: test lint logic selftest e2e e2e-combos e2e-arm unit-arm measure patch-header clean

test: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@./selftest.sh discovery
	@echo "test: $(words $(UNIT_TESTS)) unit tests and the discovery self-test ran"

lint: $(LINT_OBJ)
	@echo "lint: $(words $(RANDO_SRC)) sources build with -pedantic-errors"

logic: $(LOGIC_BINS)
	@set -e; for t in $(LOGIC_BINS); do ./$$t --fixtures; done
	@echo "logic: $(words $(LOGIC_TESTS)) unit tests replayed the oracle fixtures"

selftest:
	@./selftest.sh

patch-header: $(BUILD)/test_patch_header
	@./$(BUILD)/test_patch_header --file $(BPS)

e2e: $(GEN) $(PATCH_CLI)
	@./run_e2e.sh

e2e-combos: $(GEN)
	@echo "e2e-combos: the combination cases"
	@RANDO_CASES="$(CURDIR)/combos" RANDO_CLAIMS="$(CURDIR)/combo_claims.txt" ./run_e2e.sh

# -static: qemu-arm then needs no copy of the image's sysroot (-L).
# The wrapper exists because run_e2e.sh runs RANDO_GEN as one word.
e2e-arm:
	@command -v qemu-arm >/dev/null 2>&1 || { echo "setup: qemu-arm not found on PATH (install qemu-user)" >&2; exit 1; }
	@mkdir -p $(ARM_DIR)
	@echo "e2e-arm: building $(ARM_GEN) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c '"$${CROSS_COMPILE}gcc" $(CFLAGS) -O2 $(ONION_CFLAGS) -static $(INCLUDES) -o $(ARM_GEN) $(RANDO)/host/main/rando_gen.c $(RANDO_SRC) $(HOST_SRC)'
	@file $(ARM_GEN) | grep -q 'ELF 32-bit LSB executable, ARM, EABI5' || { file $(ARM_GEN); echo "e2e-arm: $(ARM_GEN) is not a 32-bit ARM EABI executable" >&2; exit 1; }
	@file $(ARM_GEN)
	@qemu-arm --version | head -n 1
	@printf '#!/bin/sh\nexec qemu-arm "%s" "$$@"\n' "$(abspath $(ARM_GEN))" > $(ARM_WRAPPER)
	@chmod +x $(ARM_WRAPPER)
	@RANDO_GEN="$(abspath $(ARM_WRAPPER))" ./run_e2e.sh

# unit-arm: every unit test built like e2e-arm's generator, run under qemu-arm
# from here (fixture paths are relative), LOGIC_TESTS with --fixtures too.
# _FILE_OFFSET_BITS=64: on a 64-bit host kernel ext4 hands qemu-arm 64-bit
# d_off cookies, so a 32-bit readdir fails with EOVERFLOW (not on the device).
unit-arm:
	@command -v qemu-arm >/dev/null 2>&1 || { echo "setup: qemu-arm not found on PATH (install qemu-user)" >&2; exit 1; }
	@rm -rf $(ARM_UNIT_DIR) && mkdir -p $(ARM_UNIT_DIR)/obj
	@echo "unit-arm: building $(words $(UNIT_TESTS)) unit tests in $(ARM_UNIT_DIR) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c 'set -e; cc="$${CROSS_COMPILE}gcc"; f="$(CFLAGS) -O2 $(ONION_CFLAGS) -static -D_FILE_OFFSET_BITS=64 $(INCLUDES)"; \
		for s in $(ARM_UNIT_SRC); do echo "$$s"; done | xargs -P 4 -I{} sh -c "\"$$cc\" $$f -c -o $(ARM_UNIT_DIR)/obj/\$$(basename {} .c).o {}"; \
		for t in $(UNIT_TESTS); do "$$cc" $$f -o $(ARM_UNIT_DIR)/$$t unit/$$t.c $(ARM_UNIT_DIR)/obj/*.o; done'
	@file $(ARM_UNIT_DIR)/test_prng | grep -q 'ELF 32-bit LSB executable, ARM, EABI5' || { file $(ARM_UNIT_DIR)/test_prng; echo "unit-arm: $(ARM_UNIT_DIR)/test_prng is not a 32-bit ARM EABI executable" >&2; exit 1; }
	@qemu-arm --version | head -n 1
	@set -e; for t in $(UNIT_TESTS); do qemu-arm $(ARM_UNIT_DIR)/$$t; done
	@set -e; for t in $(LOGIC_TESTS); do qemu-arm $(ARM_UNIT_DIR)/$$t --fixtures; done
	@echo "unit-arm: $(words $(UNIT_TESTS)) unit tests and $(words $(LOGIC_TESTS)) fixture replays passed under qemu-arm"

$(BUILD)/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

# rm first: `ar r` would keep the object of a deleted source.
$(RANDO_LIB): $(RANDO_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(BUILD)/host/%.o: $(RANDO)/host/%.c $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

$(HOST_LIB): $(HOST_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(BUILD)/support.o: unit/support.c unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/synthetic_base.o: unit/synthetic_base.c unit/synthetic_base.h unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h unit/synthetic_base.h $(SUPPORT_OBJ) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(SUPPORT_OBJ) $(HOST_LIB) $(RANDO_LIB)

$(LINT_DIR)/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(RANDO)/include -c -o $@ $<

$(GEN_DIR)/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O2 -I$(RANDO)/include -c -o $@ $<

$(GEN_DIR)/host/%.o: $(RANDO)/host/%.c $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -O2 $(INCLUDES) -c -o $@ $<

$(GEN): $(RANDO)/host/main/rando_gen.c $(GEN_OBJ) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -O2 $(INCLUDES) -o $@ $< $(GEN_OBJ)

$(PATCH_CLI): $(RANDO)/host/main/rando_patch.c $(GEN_OBJ) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -O2 $(INCLUDES) -o $@ $< $(GEN_OBJ)

measure: $(MEASURE_OBJ) $(MEASURE_OS_OBJ) $(MEASURE_PROBE) $(MEASURE_BIN)
	@CC='$(CC)' MEASURE_DIR='$(MEASURE)' MEASURE_SWEEP='$(MEASURE_SWEEP)' MEASURE_REPS='$(MEASURE_REPS)' ./measure.sh

$(MEASURE)/host-O2/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(MEASURE)/host-Os/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -Os -I$(RANDO)/include -c -o $@ $<

$(MEASURE)/host-O2/host/%.o: $(RANDO)/host/%.c $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) $(INCLUDES) -c -o $@ $<

$(MEASURE_PROBE): measure/size_probe.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -fno-common -I$(RANDO)/include -c -o $@ $<

$(MEASURE_BIN): measure/rando_measure.c $(MEASURE_OBJ) $(RANDO_HDR) $(HOST_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -D_POSIX_C_SOURCE=200809L $(INCLUDES) -pthread -o $@ $< $(MEASURE_OBJ)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
