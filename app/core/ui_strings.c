#include "ui_strings.h"

#include <assert.h>

#define UI_STR_ARITY_ENTRY(id, arity) arity,
static const unsigned char arities[UI_STR_COUNT] = { UI_STR_LIST(UI_STR_ARITY_ENTRY) };
#undef UI_STR_ARITY_ENTRY

static bool in_range(UiStrId id)
{
	return (int)id >= 0 && (int)id < UI_STR_COUNT;
}

const char *ui_str(const UiStrTable *table, UiStrId id)
{
	const char *text;

	if (!in_range(id) || id == UI_STR_NONE)
		return "";
	text = table ? table->text[id] : NULL;
	if (!text)
		text = ui_str_english()->text[id];
	return text ? text : "";
}

int ui_str_arity(UiStrId id)
{
	return in_range(id) ? arities[id] : 0;
}

static bool put(char *buf, size_t cap, size_t *len, const char *src, size_t n)
{
	size_t i;

	for (i = 0; src && i < n && src[i]; i++) {
		if (*len + 1 >= cap)
			return false;
		buf[(*len)++] = src[i];
	}
	return true;
}

bool ui_str_fill(const char *pattern, const char *arg, char *buf, size_t cap)
{
	const char *slot;
	size_t len = 0;
	bool fit;

	assert(pattern && buf);
	if (cap == 0)
		return false;
	for (slot = pattern; *slot && !(slot[0] == '{' && slot[1] == '}'); slot++)
		;
	fit = put(buf, cap, &len, pattern, (size_t)(slot - pattern));
	if (fit && *slot)
		fit = put(buf, cap, &len, arg, (size_t)-1) && put(buf, cap, &len, slot + 2, (size_t)-1);
	buf[len] = '\0';
	return fit;
}

static UiStrFault text_fault(UiStrId id, const char *text, bool complete)
{
	const char *c;
	int slots = 0, len = 0;

	if (!text)
		return complete ? UI_STR_FAULT_MISSING : UI_STR_FAULT_NONE;
	if (!text[0])
		return UI_STR_FAULT_EMPTY;
	for (c = text; *c; c++)
		if ((unsigned char)*c < 0x20 || (unsigned char)*c > 0x7E)
			return UI_STR_FAULT_BYTE;
	for (c = text; *c; c++) {
		if (c[0] == '{' && c[1] == '}') {
			slots++;
			c++;
		} else if (*c == '{' || *c == '}') {
			return UI_STR_FAULT_BRACE;
		} else {
			len++;
		}
	}
	if (slots != ui_str_arity(id))
		return UI_STR_FAULT_ARITY;
	return len > UI_STR_LEN_MAX ? UI_STR_FAULT_TOO_LONG : UI_STR_FAULT_NONE;
}

bool ui_strings_check(const UiStrTable *table, bool complete, UiStrId *bad_id, UiStrFault *fault)
{
	int id;

	assert(table && bad_id && fault);
	for (id = 1; id < UI_STR_COUNT; id++) {
		UiStrFault f = text_fault((UiStrId)id, table->text[id], complete);

		if (f != UI_STR_FAULT_NONE) {
			*bad_id = (UiStrId)id;
			*fault = f;
			return false;
		}
	}
	*bad_id = UI_STR_NONE;
	*fault = UI_STR_FAULT_NONE;
	return true;
}
