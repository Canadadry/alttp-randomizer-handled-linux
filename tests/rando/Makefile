CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror -DRANDO_FAULTS
RANDO := ../../rando
BUILD := build

UNIT_TESTS := test_prng test_items test_bosses test_world test_regions test_options test_case_reader test_pool test_prepare test_generate test_spoiler_json test_filler test_playthrough
# No fixture mode: test_prng already replays its vectors in `test`, the others have no fixture.
LOGIC_TESTS := $(filter-out test_prng test_options test_case_reader test_pool test_spoiler_json test_filler test_playthrough,$(UNIT_TESTS))

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
HOST_SRC := $(wildcard $(RANDO)/host/*.c)
HOST_HDR := $(wildcard $(RANDO)/host/*.h)
HOST_OBJ := $(patsubst $(RANDO)/host/%.c,$(BUILD)/host/%.o,$(HOST_SRC))
HOST_LIB := $(BUILD)/librando_host.a
INCLUDES := -I$(RANDO)/include -I$(RANDO)/host
SUPPORT_OBJ := $(BUILD)/support.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))
GEN := $(BUILD)/rando_gen
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

# lint: the library alone, strict ISO C99, so no compiler extension slips in.
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(RANDO)/src/%.c,$(LINT_DIR)/%.o,$(RANDO_SRC))

.PHONY: test lint logic selftest e2e measure clean

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

e2e: $(GEN)
	@./run_e2e.sh

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

$(SUPPORT_OBJ): unit/support.c unit/support.h $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) -I$(RANDO)/include -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(HOST_LIB) $(RANDO_LIB) $(RANDO_HDR) $(HOST_HDR) | $(BUILD)
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
