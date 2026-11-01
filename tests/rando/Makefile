CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -DRANDO_FAULTS
RANDO := ../../rando
APP := ../../app
BUILD := build

UNIT_TESTS := test_prng test_items test_bosses test_world test_regions test_options test_case_reader test_pool test_prepare test_generate test_spoiler_json test_filler test_playthrough test_postfill test_rom test_rom_items test_rom_settings test_rom_credits test_rom_text test_rom_sram test_rom_checksum test_rom_cosmetics test_crc32 test_bps test_md5 test_patch_header test_assemble test_patch_cli test_out test_name test_record test_seed_save test_ui_screen test_ui_grid test_ui_strings test_settings test_last_settings test_ui_input test_ui_generate test_ui_genflow test_ui_history test_history test_app_gen test_app_flow test_ui_app test_app_shell
# No fixture mode: test_prng already replays its vectors in `test`, the others have no fixture.
LOGIC_TESTS := $(filter-out test_prng test_options test_case_reader test_pool test_spoiler_json test_filler test_playthrough test_rom test_rom_items test_rom_settings test_rom_checksum test_rom_cosmetics test_crc32 test_bps test_md5 test_patch_header test_patch_cli test_out test_name test_seed_save test_ui_screen test_ui_grid test_ui_strings test_settings test_last_settings test_ui_input test_ui_generate test_ui_genflow test_ui_history test_history test_app_gen test_app_flow test_ui_app test_app_shell,$(UNIT_TESTS))

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
HOST_SRC := $(wildcard $(RANDO)/host/*.c)
HOST_HDR := $(wildcard $(RANDO)/host/*.h)
HOST_OBJ := $(patsubst $(RANDO)/host/%.c,$(BUILD)/host/%.o,$(HOST_SRC))
HOST_LIB := $(BUILD)/librando_host.a
INCLUDES := -I$(RANDO)/include -I$(RANDO)/host
APP_CORE_SRC := $(wildcard $(APP)/core/*.c)
APP_SD_SRC := $(wildcard $(APP)/sd/*.c)
APP_SRC := $(APP_CORE_SRC) $(APP_SD_SRC)
APP_HDR := $(wildcard $(APP)/core/*.h $(APP)/sd/*.h)
APP_OBJ := $(patsubst $(APP)/%.c,$(BUILD)/app/%.o,$(APP_SRC))
APP_LIB := $(BUILD)/librando_app.a
# app/core sees only the library and itself, so an include of app/sd or rando/host fails to build.
CORE_INCLUDES := -I$(RANDO)/include -I$(APP)/core
TEST_INCLUDES := $(INCLUDES) -I$(APP)/core -I$(APP)/sd
SUPPORT_OBJ := $(BUILD)/support.o $(BUILD)/synthetic_base.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
GEN := $(BUILD)/rando_gen
PATCH_CLI := $(BUILD)/rando_patch
GEN_DIR := $(BUILD)/o2
GEN_OBJ := $(patsubst $(RANDO)/src/%.c,$(GEN_DIR)/obj/%.o,$(RANDO_SRC)) $(patsubst $(RANDO)/host/%.c,$(GEN_DIR)/host/%.o,$(HOST_SRC))
LOGIC_BINS := $(addprefix $(BUILD)/,$(LOGIC_TESTS))
# The SDL main: built by sdl-pc, never by lint or test (they need no SDL).
SDL_MAIN := $(APP)/main/sdl_main.c
APP_MAIN_SRC := $(filter-out $(SDL_MAIN),$(wildcard $(APP)/main/*.c))
UI_CLI := $(BUILD)/ui_cli
SMOKE := $(BUILD)/smoke_ui_cli

MEASURE := $(BUILD)/measure
MEASURE_CFLAGS := -std=c99 -Wall -Wextra -Werror -O2 -fstack-usage -fno-stack-protector -U_FORTIFY_SOURCE
MEASURE_OBJ := $(patsubst $(RANDO)/src/%.c,$(MEASURE)/host-O2/obj/%.o,$(RANDO_SRC)) $(MEASURE)/host-O2/host/case_reader.o
MEASURE_OS_OBJ := $(patsubst $(RANDO)/src/%.c,$(MEASURE)/host-Os/obj/%.o,$(RANDO_SRC))
MEASURE_PROBE := $(MEASURE)/host-O2/size_probe.o
MEASURE_SYNTH_OBJ := $(MEASURE)/host-O2/unit/support.o $(MEASURE)/host-O2/unit/synthetic_base.o
MEASURE_BIN := $(MEASURE)/rando_measure
MEASURE_SWEEP ?= 1024
MEASURE_REPS ?= 5
REF_COMMIT ?= 40314c3
REF_DIR ?= $(abspath ../../tmp/measure-ref-$(REF_COMMIT))

# patch-header: the base patch whose header test_patch_header --file checks.
BPS ?= ../../data/base.bps

# lint: the library and app/, strict ISO C99, so no compiler extension slips in.
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(RANDO)/src/%.c,$(LINT_DIR)/%.o,$(RANDO_SRC))
LINT_APP_DIR := $(BUILD)/lint-app
LINT_APP_OBJ := $(patsubst $(APP)/%.c,$(LINT_APP_DIR)/%.o,$(APP_SRC) $(APP_MAIN_SRC))

# sdl-pc: the SDL layer, built and tested on the host's SDL 1.2 and SDL_ttf 2.0.
# The SDL flags are expanded by sdl-pc's rules only, so test never calls pkg-config.
SDL_TESTS := test_sdl_keys test_sdl_view test_sdl_loop
SDL_SRC := $(wildcard $(APP)/sdl/*.c)
SDL_HDR := $(wildcard $(APP)/sdl/*.h)
SDL_DIR := $(BUILD)/sdl
SDL_OBJ := $(patsubst $(APP)/sdl/%.c,$(SDL_DIR)/obj/%.o,$(SDL_SRC))
SDL_BINS := $(addprefix $(SDL_DIR)/,$(SDL_TESTS))
SDL_APP := $(SDL_DIR)/alttpr
SDL_CFLAGS = $(shell pkg-config --cflags sdl SDL_ttf)
SDL_LIBS = $(shell pkg-config --libs sdl SDL_ttf)

# e2e-arm: the generator built by Onion's toolchain with Onion's miyoomini
# flags (docs/toolchain.md), run under qemu-arm by the same e2e driver.
TOOLCHAIN := ../../onion-toolchain.sh
ONION_CFLAGS := -marm -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -march=armv7ve
ARM_DIR := $(BUILD)/arm
ARM_GEN := $(ARM_DIR)/rando_gen
ARM_WRAPPER := $(ARM_DIR)/rando_gen_qemu
ARM_UNIT_DIR := $(ARM_DIR)/unit
ARM_SDL_DIR := $(ARM_DIR)/sdl
ARM_SDL_APP := $(ARM_SDL_DIR)/alttpr
ARM_UNIT_SRC := $(RANDO_SRC) $(HOST_SRC) $(APP_SRC) unit/support.c unit/synthetic_base.c

.PHONY: test ui-cli sdl sdl-pc sdl-pc-build sdl-arm lint logic selftest e2e e2e-combos e2e-arm unit-arm measure measure-compare patch-header clean

test: $(UNIT_BINS) $(UI_CLI) $(SMOKE)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@./$(SMOKE) ./$(UI_CLI)
	@./selftest.sh discovery
	@./no_sdl.sh --exempt $(SDL_MAIN) $(APP)/core $(APP)/sd $(APP)/main
	@python3 -I ./no_literals.py $(APP)/core
	@python3 -I ./no_literals.py --no-table $(APP)/sdl
	@echo "test: $(words $(UNIT_TESTS)) unit tests, the ui_cli smoke test, the discovery self-test, the app SDL check and the UI-core and SDL-layer literal checks ran"

lint: $(LINT_OBJ) $(LINT_APP_OBJ)
	@echo "lint: $(words $(RANDO_SRC)) rando/ and $(words $(APP_SRC) $(APP_MAIN_SRC)) app/ sources build with -pedantic-errors"

ui-cli: $(UI_CLI)

sdl-pc:
	@command -v pkg-config >/dev/null 2>&1 && pkg-config --exists sdl SDL_ttf || { echo "setup: SDL 1.2 and SDL_ttf 2.0 development files not found (apt install libsdl1.2-dev libsdl-ttf2.0-dev)" >&2; exit 1; }
	@$(MAKE) --no-print-directory sdl-pc-build

sdl-pc-build: $(SDL_OBJ) $(SDL_BINS) $(SDL_APP)
	@set -e; for t in $(SDL_BINS); do SDL_VIDEODRIVER=dummy ./$$t; done
	@./sdl/smoke_sdl.sh ./$(SDL_APP) ../../data/DejaVuSansMono.ttf
	@echo "sdl-pc: $(words $(SDL_SRC)) SDL-layer sources and the SDL main built with -pedantic-errors, $(words $(SDL_TESTS)) SDL tests and the smoke test ran under the dummy video driver"

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

sdl: sdl-pc sdl-arm

# sdl-arm: the SDL layer and main compiled and linked, dynamically, against the
# image's sysroot SDL 1.2 / SDL_ttf 2.0; run nowhere. The image's sdl-config and
# pkg-config are the x86 host's, so no SDL flag is asked of them.
sdl-arm:
	@rm -rf $(ARM_SDL_DIR) && mkdir -p $(ARM_SDL_DIR)/obj
	@echo "sdl-arm: building $(ARM_SDL_APP) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c 'set -e; cc="$${CROSS_COMPILE}gcc"; f="$(CFLAGS) -O2 $(ONION_CFLAGS)"; o=$(ARM_SDL_DIR)/obj; \
		for s in $(SDL_SRC); do "$$cc" $$f -pedantic-errors $(CORE_INCLUDES) -c -o $$o/sdl_$$(basename $$s .c).o $$s; done; \
		"$$cc" $$f -pedantic-errors $(TEST_INCLUDES) -I$(APP)/sdl -c -o $$o/main.o $(SDL_MAIN); \
		for s in $(APP_SRC) $(HOST_SRC) $(RANDO_SRC); do echo "$$s"; done | xargs -P 4 -I{} sh -c "\"$$cc\" $$f $(TEST_INCLUDES) -c -o $$o/\$$(basename {} .c).o {}"; \
		"$$cc" $$f -o $(ARM_SDL_APP) $$o/*.o -lSDL -lSDL_ttf'
	@file $(ARM_SDL_APP) | grep 'ELF 32-bit LSB executable, ARM, EABI5' | grep -q 'dynamically linked' || { file $(ARM_SDL_APP); echo "sdl-arm: $(ARM_SDL_APP) is not a dynamically linked 32-bit ARM EABI executable" >&2; exit 1; }
	@file $(ARM_SDL_APP)
	@echo "sdl-arm: the SDL layer and main compiled with -pedantic-errors and linked against the toolchain's SDL 1.2 and SDL_ttf"

# unit-arm: every unit test built like e2e-arm's generator, run under qemu-arm
# from here (fixture paths are relative), LOGIC_TESTS with --fixtures too.
# _FILE_OFFSET_BITS=64: on a 64-bit host kernel ext4 hands qemu-arm 64-bit
# d_off cookies, so a 32-bit readdir fails with EOVERFLOW (not on the device).
unit-arm:
	@command -v qemu-arm >/dev/null 2>&1 || { echo "setup: qemu-arm not found on PATH (install qemu-user)" >&2; exit 1; }
	@rm -rf $(ARM_UNIT_DIR) && mkdir -p $(ARM_UNIT_DIR)/obj
	@echo "unit-arm: building $(words $(UNIT_TESTS)) unit tests in $(ARM_UNIT_DIR) with Onion's toolchain"
	@$(TOOLCHAIN) sh -c 'set -e; cc="$${CROSS_COMPILE}gcc"; f="$(CFLAGS) -O2 $(ONION_CFLAGS) -static -D_FILE_OFFSET_BITS=64 $(TEST_INCLUDES)"; \
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

$(BUILD)/app/core/%.o: $(APP)/core/%.c $(APP_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CORE_INCLUDES) -c -o $@ $<

$(BUILD)/app/sd/%.o: $(APP)/sd/%.c $(APP_HDR) $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -c -o $@ $<

$(APP_LIB): $(APP_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(BUILD)/support.o: unit/support.c unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/synthetic_base.o: unit/synthetic_base.c unit/synthetic_base.h unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h unit/synthetic_base.h $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) $(APP_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -o $@ $< $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB)

$(UI_CLI): $(APP)/main/ui_cli.c $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) $(APP_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -o $@ $< $(APP_LIB) $(HOST_LIB) $(RANDO_LIB)

$(SMOKE): unit/smoke_ui_cli.c unit/support.h unit/synthetic_base.h $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) $(APP_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -o $@ $< $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB)

$(SDL_DIR)/obj/%.o: $(APP)/sdl/%.c $(SDL_HDR) $(APP_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(CORE_INCLUDES) $(SDL_CFLAGS) -c -o $@ $<

$(SDL_DIR)/test_%: sdl/test_%.c unit/support.h $(SDL_OBJ) $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(SDL_HDR) $(APP_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(TEST_INCLUDES) -I$(APP)/sdl -Iunit $(SDL_CFLAGS) -o $@ $< $(SDL_OBJ) $(SUPPORT_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(SDL_LIBS)

$(SDL_APP): $(SDL_MAIN) $(SDL_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(SDL_HDR) $(APP_HDR) $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(TEST_INCLUDES) -I$(APP)/sdl $(SDL_CFLAGS) -o $@ $< $(SDL_OBJ) $(APP_LIB) $(HOST_LIB) $(RANDO_LIB) $(SDL_LIBS)

$(LINT_DIR)/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors -I$(RANDO)/include -c -o $@ $<

$(LINT_APP_DIR)/core/%.o: $(APP)/core/%.c $(APP_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(CORE_INCLUDES) -c -o $@ $<

$(LINT_APP_DIR)/sd/%.o: $(APP)/sd/%.c $(APP_HDR) $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(TEST_INCLUDES) -c -o $@ $<

$(LINT_APP_DIR)/main/%.o: $(APP)/main/%.c $(APP_HDR) $(HOST_HDR) $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(TEST_INCLUDES) -c -o $@ $<

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

# The reference's own measure.sh fails on a clang host: only its driver is built.
measure-compare: $(MEASURE_BIN)
	@mkdir -p $(MEASURE)
	@if [ ! -e '$(REF_DIR)' ]; then \
		git -C ../.. worktree add --detach '$(REF_DIR)' '$(REF_COMMIT)' >$(MEASURE)/ref-setup.log 2>&1 || \
		{ echo "setup: cannot build the reference driver at $(REF_COMMIT) ($$(tail -n 1 $(MEASURE)/ref-setup.log))"; exit 1; }; \
	fi
	@REF_DIR='$(REF_DIR)' REF_COMMIT='$(REF_COMMIT)' ./measure_compare.sh --guard
	@$(MAKE) --no-print-directory -C '$(REF_DIR)/tests/rando' CC='$(CC)' build/measure/rando_measure >$(MEASURE)/ref-build.log 2>&1 || \
		{ echo "setup: cannot build the reference driver at $(REF_COMMIT) ($$(tail -n 1 $(MEASURE)/ref-build.log))"; exit 1; }
	@CC='$(CC)' MEASURE_DIR='$(MEASURE)' REF_DIR='$(REF_DIR)' REF_COMMIT='$(REF_COMMIT)' ./measure_compare.sh

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

$(MEASURE)/host-O2/unit/%.o: unit/%.c unit/support.h unit/synthetic_base.h $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(MEASURE_BIN): measure/rando_measure.c $(MEASURE_OBJ) $(MEASURE_SYNTH_OBJ) $(RANDO_HDR) $(HOST_HDR) unit/synthetic_base.h
	@mkdir -p $(@D)
	$(CC) $(MEASURE_CFLAGS) -D_POSIX_C_SOURCE=200809L $(INCLUDES) -Iunit -pthread -o $@ $< $(MEASURE_OBJ) $(MEASURE_SYNTH_OBJ)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
