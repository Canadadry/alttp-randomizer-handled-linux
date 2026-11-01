CC ?= gcc
CFLAGS := -std=c99 -Wall -Wextra -Werror
RANDO := ../../rando
BUILD := build

UNIT_TESTS := test_version

RANDO_SRC := $(wildcard $(RANDO)/src/*.c $(RANDO)/src/*/*.c)
RANDO_HDR := $(wildcard $(RANDO)/include/*.h $(RANDO)/src/*.h)
RANDO_OBJ := $(patsubst $(RANDO)/src/%.c,$(BUILD)/obj/%.o,$(RANDO_SRC))
RANDO_LIB := $(BUILD)/librando.a
INCLUDES := -I$(RANDO)/include
SUPPORT_OBJ := $(BUILD)/support.o
UNIT_BINS := $(addprefix $(BUILD)/,$(UNIT_TESTS))

# lint: the library alone, strict ISO C99, so no compiler extension slips in.
LINT_DIR := $(BUILD)/lint
LINT_OBJ := $(patsubst $(RANDO)/src/%.c,$(LINT_DIR)/%.o,$(RANDO_SRC))

.PHONY: test lint clean

test: $(UNIT_BINS)
	@set -e; for t in $(UNIT_BINS); do ./$$t; done
	@echo "test: $(words $(UNIT_TESTS)) unit tests ran"

lint: $(LINT_OBJ)
	@echo "lint: $(words $(RANDO_SRC)) sources build with -pedantic"

$(BUILD):
	@mkdir -p $@

$(BUILD)/obj/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $@ $<

# rm first: `ar r` would keep the object of a deleted source.
$(RANDO_LIB): $(RANDO_OBJ)
	@rm -f $@
	ar rcs $@ $^

$(SUPPORT_OBJ): unit/support.c unit/support.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/test_%: unit/test_%.c unit/support.h $(SUPPORT_OBJ) $(RANDO_LIB) $(RANDO_HDR) | $(BUILD)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(SUPPORT_OBJ) $(RANDO_LIB)

$(LINT_DIR)/%.o: $(RANDO)/src/%.c $(RANDO_HDR)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -pedantic-errors $(INCLUDES) -c -o $@ $<

clean:
	rm -rf $(BUILD)
