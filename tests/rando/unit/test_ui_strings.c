#include "support.h"
#include "ui_strings.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_ui_strings"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static void test_seed_texts_and_arities(void)
{
	const UiStrTable *en = ui_str_english();

	CHECK(strcmp(ui_str(en, UI_STR_TAB_GENERATE), "Generate") == 0);
	CHECK(strcmp(ui_str(en, UI_STR_TAB_HISTORY), "History") == 0);
	CHECK(strcmp(ui_str(en, UI_STR_HISTORY_DELETE_CONFIRM), "Delete {}?") == 0);
	CHECK(ui_str_arity(UI_STR_TAB_GENERATE) == 0);
	CHECK(ui_str_arity(UI_STR_TAB_HISTORY) == 0);
	CHECK(ui_str_arity(UI_STR_HISTORY_DELETE_CONFIRM) == 1);
}

static void test_lookup_never_returns_null(void)
{
	const UiStrTable *en = ui_str_english();
	int id;

	for (id = 1; id < UI_STR_COUNT; id++) {
		const char *text = ui_str(en, (UiStrId)id);

		if (!text || !text[0])
			test_fail(TEST, "id %d has no English text", id);
	}
	CHECK(ui_str(en, UI_STR_NONE) && strcmp(ui_str(en, UI_STR_NONE), "") == 0);
	CHECK(ui_str(en, UI_STR_COUNT) && strcmp(ui_str(en, UI_STR_COUNT), "") == 0);
	CHECK(ui_str(en, (UiStrId)-1) && strcmp(ui_str(en, (UiStrId)-1), "") == 0);
	CHECK(ui_str_arity(UI_STR_NONE) == 0);
	CHECK(ui_str_arity(UI_STR_COUNT) == 0);
	CHECK(ui_str_arity((UiStrId)-1) == 0);
}

static void test_missing_entry_falls_back_to_english(void)
{
	UiStrTable partial = { { 0 } };

	partial.text[UI_STR_TAB_HISTORY] = "Historique";
	CHECK(strcmp(ui_str(&partial, UI_STR_TAB_HISTORY), "Historique") == 0);
	CHECK(strcmp(ui_str(&partial, UI_STR_TAB_GENERATE), "Generate") == 0);
	CHECK(strcmp(ui_str(&partial, UI_STR_HISTORY_DELETE_CONFIRM), "Delete {}?") == 0);
	CHECK(strcmp(ui_str(&partial, UI_STR_NONE), "") == 0);
	partial.text[UI_STR_NONE] = "Rien";
	CHECK(strcmp(ui_str(&partial, UI_STR_NONE), "") == 0);
	CHECK(strcmp(ui_str(NULL, UI_STR_TAB_HISTORY), "History") == 0);
	CHECK(strcmp(ui_str(NULL, UI_STR_COUNT), "") == 0);
}

static void check_fill(const char *pattern, const char *arg, size_t cap, bool fit, const char *expected, int line)
{
	char buf[64];
	bool got;

	memset(buf, '@', sizeof buf);
	got = ui_str_fill(pattern, arg, buf, cap);
	if (got != fit || strcmp(buf, expected) != 0)
		test_fail(TEST, "line %d: fill(\"%s\", \"%s\", %zu) = %d \"%s\", want %d \"%s\"", line, pattern,
			  arg ? arg : "(null)", cap, got, buf, fit, expected);
	if (buf[strlen(expected) + 1] != '@')
		test_fail(TEST, "line %d: fill wrote past its NUL", line);
}

static void test_fill(void)
{
	char buf[4];

	check_fill("{} saved", "x.sfc", 64, true, "x.sfc saved", __LINE__);
	check_fill("Delete {}?", "x.sfc", 64, true, "Delete x.sfc?", __LINE__);
	check_fill("Saved: {}", "x.sfc", 64, true, "Saved: x.sfc", __LINE__);
	check_fill("History", "x.sfc", 64, true, "History", __LINE__);
	check_fill("Delete {}?", NULL, 64, true, "Delete ?", __LINE__);
	check_fill("Delete {}?", "ab", 11, true, "Delete ab?", __LINE__);
	check_fill("Delete {}?", "ab", 10, false, "Delete ab", __LINE__);
	check_fill("Delete {}?", "ab", 8, false, "Delete ", __LINE__);
	check_fill("History", NULL, 8, true, "History", __LINE__);
	check_fill("History", NULL, 7, false, "Histor", __LINE__);
	check_fill("History", NULL, 1, false, "", __LINE__);

	memset(buf, '@', sizeof buf);
	CHECK(!ui_str_fill("History", NULL, buf, 0));
	CHECK(buf[0] == '@');
}

/* A copy of English with one entry replaced. */
static UiStrTable english_with(UiStrId id, const char *text)
{
	UiStrTable t = *ui_str_english();

	t.text[id] = text;
	return t;
}

static void check_fault(const UiStrTable *table, bool complete, UiStrId want_id, UiStrFault want_fault, int line)
{
	UiStrId bad_id = UI_STR_NONE;
	UiStrFault fault = UI_STR_FAULT_NONE;

	if (ui_strings_check(table, complete, &bad_id, &fault))
		test_fail(TEST, "line %d: table passed, want id %d fault %d", line, want_id, want_fault);
	if (bad_id != want_id || fault != want_fault)
		test_fail(TEST, "line %d: id %d fault %d, want id %d fault %d", line, bad_id, fault, want_id,
			  want_fault);
}

static void check_sound(const UiStrTable *table, bool complete, int line)
{
	UiStrId bad_id = UI_STR_NONE;
	UiStrFault fault = UI_STR_FAULT_NONE;

	if (!ui_strings_check(table, complete, &bad_id, &fault))
		test_fail(TEST, "line %d: id %d breaks rule %d", line, bad_id, fault);
}

static void test_english_table_is_sound(void)
{
	check_sound(ui_str_english(), true, __LINE__);
}

static void test_check_missing_and_empty(void)
{
	UiStrTable t;

	t = english_with(UI_STR_TAB_HISTORY, NULL);
	check_fault(&t, true, UI_STR_TAB_HISTORY, UI_STR_FAULT_MISSING, __LINE__);
	t = english_with(UI_STR_TAB_GENERATE, "");
	check_fault(&t, true, UI_STR_TAB_GENERATE, UI_STR_FAULT_EMPTY, __LINE__);
}

static void test_check_bytes(void)
{
	UiStrTable t;

	t = english_with(UI_STR_TAB_HISTORY, "Hist\x7Fory");
	check_fault(&t, true, UI_STR_TAB_HISTORY, UI_STR_FAULT_BYTE, __LINE__);
	t = english_with(UI_STR_TAB_HISTORY, "Hist\xC3\xA9");
	check_fault(&t, true, UI_STR_TAB_HISTORY, UI_STR_FAULT_BYTE, __LINE__);
	t = english_with(UI_STR_TAB_HISTORY, "Hist\tory");
	check_fault(&t, true, UI_STR_TAB_HISTORY, UI_STR_FAULT_BYTE, __LINE__);
	t = english_with(UI_STR_TAB_HISTORY, " ~Hist|ory~ ");
	check_sound(&t, true, __LINE__);
}

static void test_check_placeholders(void)
{
	UiStrTable t;

	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, "Delete it?");
	check_fault(&t, true, UI_STR_HISTORY_DELETE_CONFIRM, UI_STR_FAULT_ARITY, __LINE__);
	t = english_with(UI_STR_TAB_GENERATE, "Generate {}");
	check_fault(&t, true, UI_STR_TAB_GENERATE, UI_STR_FAULT_ARITY, __LINE__);
	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, "Delete {} {}?");
	check_fault(&t, true, UI_STR_HISTORY_DELETE_CONFIRM, UI_STR_FAULT_ARITY, __LINE__);
	t = english_with(UI_STR_TAB_GENERATE, "Gen{rate");
	check_fault(&t, true, UI_STR_TAB_GENERATE, UI_STR_FAULT_BRACE, __LINE__);
	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, "Delete {}}?");
	check_fault(&t, true, UI_STR_HISTORY_DELETE_CONFIRM, UI_STR_FAULT_BRACE, __LINE__);
	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, "{}: delete?");
	check_sound(&t, true, __LINE__);
}

/* A text of n characters, 'a' then 'b's, with {} after the first when slot. */
static const char *text_of(char *buf, int n, bool slot)
{
	int i, len = 0;

	for (i = 0; i < n; i++) {
		buf[len++] = i == 0 ? 'a' : 'b';
		if (i == 0 && slot) {
			buf[len++] = '{';
			buf[len++] = '}';
		}
	}
	buf[len] = '\0';
	return buf;
}

static void test_check_length(void)
{
	char buf[UI_GRID_COLS_MAX + 4];
	int field = UI_GRID_DEFAULT_COLS - 2;
	UiStrTable t;

	CHECK(UI_STR_LEN_MAX == field);
	t = english_with(UI_STR_TAB_GENERATE, text_of(buf, field, false));
	check_sound(&t, true, __LINE__);
	t = english_with(UI_STR_TAB_GENERATE, text_of(buf, field + 1, false));
	check_fault(&t, true, UI_STR_TAB_GENERATE, UI_STR_FAULT_TOO_LONG, __LINE__);
	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, text_of(buf, field, true));
	check_sound(&t, true, __LINE__);
	t = english_with(UI_STR_HISTORY_DELETE_CONFIRM, text_of(buf, field + 1, true));
	check_fault(&t, true, UI_STR_HISTORY_DELETE_CONFIRM, UI_STR_FAULT_TOO_LONG, __LINE__);
}

static void test_check_translated_table(void)
{
	UiStrTable t = { { 0 } };

	check_sound(&t, false, __LINE__);
	t.text[UI_STR_HISTORY_DELETE_CONFIRM] = "Supprimer {} ?";
	check_sound(&t, false, __LINE__);
	check_fault(&t, true, UI_STR_TAB_GENERATE, UI_STR_FAULT_MISSING, __LINE__);
	t.text[UI_STR_TAB_HISTORY] = "Historique {}";
	check_fault(&t, false, UI_STR_TAB_HISTORY, UI_STR_FAULT_ARITY, __LINE__);
}

int main(void)
{
	test_seed_texts_and_arities();
	test_lookup_never_returns_null();
	test_missing_entry_falls_back_to_english();
	test_fill();
	test_english_table_is_sound();
	test_check_missing_and_empty();
	test_check_bytes();
	test_check_placeholders();
	test_check_length();
	test_check_translated_table();
	printf("%s: passed\n", TEST);
	return 0;
}
