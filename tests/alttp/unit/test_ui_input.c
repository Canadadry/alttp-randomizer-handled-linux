#include "support.h"
#include "ui_input.h"

#include <stdio.h>

#define TEST "test_ui_input"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define NONE UI_BUTTON_COUNT

static UiInput input;

/* The button the event emits, NONE when it emits nothing. */
static UiButton event(UiButton button, bool down)
{
	UiButton out = NONE;

	if (!ui_input_event(&input, button, down, &out))
		return NONE;
	CHECK(out < UI_BUTTON_COUNT);
	return out;
}

static UiButton press(UiButton b)
{
	return event(b, true);
}

static UiButton release(UiButton b)
{
	return event(b, false);
}

static bool is_starter(UiButton b)
{
	return b == UI_BUTTON_START || b == UI_BUTTON_SELECT || b == UI_BUTTON_MENU;
}

static void test_lone_buttons(void)
{
	int b;

	for (b = 0; b < UI_BUTTON_COUNT; b++) {
		ui_input_init(&input);
		if (is_starter((UiButton)b)) {
			if (press((UiButton)b) != NONE)
				test_fail(TEST, "starter %d emitted on press", b);
			if (release((UiButton)b) != (UiButton)b)
				test_fail(TEST, "starter %d alone did not emit on release", b);
		} else {
			if (press((UiButton)b) != (UiButton)b)
				test_fail(TEST, "button %d did not emit on press", b);
			if (press((UiButton)b) != (UiButton)b)
				test_fail(TEST, "button %d did not emit on a repeated press", b);
			if (release((UiButton)b) != NONE)
				test_fail(TEST, "button %d emitted on release", b);
		}
	}
}

typedef struct Chord {
	const char *name;
	UiButton keys[3];
	int count;
} Chord;

static const Chord chords[] = {
	{"Start + L2", {UI_BUTTON_START, UI_BUTTON_L2}, 2},
	{"Start + R2", {UI_BUTTON_START, UI_BUTTON_R2}, 2},
	{"Select + L2", {UI_BUTTON_SELECT, UI_BUTTON_L2}, 2},
	{"Select + R2", {UI_BUTTON_SELECT, UI_BUTTON_R2}, 2},
	{"Select + Start + L1", {UI_BUTTON_SELECT, UI_BUTTON_START, UI_BUTTON_L1}, 3},
	{"Select + Start + R1", {UI_BUTTON_SELECT, UI_BUTTON_START, UI_BUTTON_R1}, 3},
	{"Menu + A", {UI_BUTTON_MENU, UI_BUTTON_A}, 2},
	{"Menu + B", {UI_BUTTON_MENU, UI_BUTTON_B}, 2},
	{"Select + Start", {UI_BUTTON_SELECT, UI_BUTTON_START}, 2},
};

/* Presses keys in order, then releases them in that order (forward) or the reverse; every event must emit
 * nothing, and a lone A afterwards must work again. */
static void play_chord(const Chord *c, bool reverse_release)
{
	int i;

	ui_input_init(&input);
	for (i = 0; i < c->count; i++)
		if (press(c->keys[i]) != NONE)
			test_fail(TEST, "%s: press %d emitted", c->name, i);
	if (c->keys[c->count - 1] == UI_BUTTON_L2 || c->keys[c->count - 1] == UI_BUTTON_R2)
		if (press(c->keys[c->count - 1]) != NONE)
			test_fail(TEST, "%s: a repeat emitted", c->name);
	for (i = 0; i < c->count; i++) {
		UiButton b = c->keys[reverse_release ? c->count - 1 - i : i];

		if (release(b) != NONE)
			test_fail(TEST, "%s: release of %d emitted (%s order)", c->name, (int)b,
				  reverse_release ? "reverse" : "forward");
	}
	if (press(UI_BUTTON_A) != UI_BUTTON_A || release(UI_BUTTON_A) != NONE)
		test_fail(TEST, "%s: a lone A after the chord did not emit", c->name);
	if (press(UI_BUTTON_START) != NONE || release(UI_BUTTON_START) != UI_BUTTON_START)
		test_fail(TEST, "%s: a lone Start after the chord did not emit", c->name);
}

static void test_keymon_chords(void)
{
	size_t i;

	for (i = 0; i < sizeof chords / sizeof chords[0]; i++) {
		play_chord(&chords[i], false);
		play_chord(&chords[i], true);
	}
}

static void test_held_menu(void)
{
	ui_input_init(&input);
	CHECK(press(UI_BUTTON_MENU) == NONE);
	CHECK(press(UI_BUTTON_A) == NONE);
	CHECK(press(UI_BUTTON_A) == NONE);
	CHECK(release(UI_BUTTON_A) == NONE);
	CHECK(press(UI_BUTTON_DOWN) == NONE);
	CHECK(release(UI_BUTTON_DOWN) == NONE);
	CHECK(release(UI_BUTTON_MENU) == NONE);
	CHECK(press(UI_BUTTON_DOWN) == UI_BUTTON_DOWN);
}

static void test_starter_after_other_button(void)
{
	ui_input_init(&input);
	CHECK(press(UI_BUTTON_DOWN) == UI_BUTTON_DOWN);
	CHECK(press(UI_BUTTON_START) == NONE);
	CHECK(release(UI_BUTTON_DOWN) == NONE);
	CHECK(release(UI_BUTTON_START) == UI_BUTTON_START);
	CHECK(release(UI_BUTTON_START) == NONE);
	CHECK(release(UI_BUTTON_SELECT) == NONE);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	test_lone_buttons();
	test_keymon_chords();
	test_held_menu();
	test_starter_after_other_button();
	printf("%s: passed\n", TEST);
	return 0;
}
