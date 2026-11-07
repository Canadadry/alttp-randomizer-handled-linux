#define _POSIX_C_SOURCE 200809L

#include "support.h"
#include "theme.h"
#include "ui_grid.h"
#include "ui_history.h"
#include "ui_strings.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define TEST "test_theme"
#define FILE_NAME "t.ini"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

#define SCREEN \
	"[screen]\n" \
	"colour = #14161a\n" \
	"image = none\n" \
	"cut = text:~\n"

#define FONT_MONO \
	"[font.mono]\n" \
	"file = DejaVuSansMono.ttf\n" \
	"size = fit\n" \
	"cell = 16 32\n" \
	"anchor = top\n"

#define CLASSIC_DIR "../../themes/classic"
#define INVENTORY_DIR "unit/themes/inventory"
#define ALTTP_DIR "../../themes/alttp"
#define DATA_DIR "../../data"

static Theme theme;
static char classic[THEME_FILE_BYTES_MAX + 1];
static const char *rest;
static char buf[THEME_FILE_BYTES_MAX * 2];

static void read_classic(void)
{
	FILE *f = fopen(CLASSIC_DIR "/theme.ini", "rb");
	size_t n;

	if (!f)
		test_fail(TEST, "cannot open %s/theme.ini", CLASSIC_DIR);
	n = fread(classic, 1, sizeof classic - 1, f);
	fclose(f);
	classic[n] = '\0';
	rest = strstr(classic, "\n[tabs]\n");
	if (!rest)
		test_fail(TEST, "classic has no [tabs] section");
}

/* The sections after [screen] and [font.*] are classic's, so a prefix is a whole theme. */
static const char *with_rest(const char *prefix)
{
	if (strlen(prefix) + strlen(rest) >= sizeof buf)
		test_fail(TEST, "theme text too long");
	strcpy(buf, prefix);
	strcat(buf, rest);
	return buf;
}

static void parse_ok(int line, const char *text)
{
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (!theme_parse(&theme, text, FILE_NAME, &err))
		test_fail(TEST, "%s:%d: refused: %s", __FILE__, line, err.message);
}

static void parse_fails(int line, const char *text, const char *expected)
{
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (theme_parse(&theme, text, FILE_NAME, &err))
		test_fail(TEST, "%s:%d: accepted, expected \"%s\"", __FILE__, line, expected);
	if (strcmp(err.message, expected) != 0)
		test_fail(TEST, "%s:%d: got \"%s\", expected \"%s\"", __FILE__, line, err.message, expected);
}

#define OK(text) parse_ok(__LINE__, with_rest(text))
#define FAILS(text, expected) parse_fails(__LINE__, with_rest(text), expected)
#define FAILS_RAW(text, expected) parse_fails(__LINE__, text, expected)


static char work[THEME_FILE_BYTES_MAX * 2];
static char expected[THEME_ERROR_MAX];

static void start(void)
{
	strcpy(work, classic);
}

static char *section_at(const char *section)
{
	char header[64];
	char *at;

	snprintf(header, sizeof header, "\n[%s]\n", section);
	if (strncmp(work, header + 1, strlen(header + 1)) == 0)
		return work;
	at = strstr(work, header);
	if (!at)
		test_fail(TEST, "classic has no [%s]", section);
	return at + 1;
}

static int line_number(const char *at)
{
	const char *c;
	int line = 1;

	for (c = work; c < at; c++)
		line += *c == '\n';
	return line;
}

/* Replaces the first line starting with prefix from [section]'s header on ("" removes it);
   returns that line's number. */
static int replace(const char *section, const char *prefix, const char *with)
{
	char *at = section_at(section);
	char *end;
	char tail[sizeof work];
	int line;

	while (strncmp(at, prefix, strlen(prefix)) != 0) {
		at = strchr(at, '\n');
		if (!at)
			test_fail(TEST, "no line \"%s\" in [%s]", prefix, section);
		at++;
	}
	line = line_number(at);
	end = strchr(at, '\n') + 1;
	strcpy(tail, end);
	if (*with) {
		strcpy(at, with);
		strcat(at, "\n");
	} else {
		*at = '\0';
	}
	strcat(work, tail);
	return line;
}

static int header_line(const char *section)
{
	return line_number(section_at(section));
}

/* Removes [section] up to the next section's header. */
static void drop(const char *section)
{
	char *at = section_at(section);
	char *next = strstr(at, "\n[");
	char tail[sizeof work];

	strcpy(tail, next ? next + 1 : "");
	strcpy(at, tail);
}

static const char *expect(const char *fmt, int line)
{
	snprintf(expected, sizeof expected, fmt, line);
	return expected;
}

#define WORK_OK() parse_ok(__LINE__, work)
#define WORK_FAILS(message) parse_fails(__LINE__, work, message)

static void test_classic_line_and_section_problems(void)
{
	int line;

	start();
	line = replace("list", "top =", "tops = 64");
	WORK_FAILS(expect(FILE_NAME ":%d: list.tops: unknown key", line));
	start();
	line = replace("footer", "button.start", "button.strat = hint_text");
	WORK_FAILS(expect(FILE_NAME ":%d: footer.button.strat: unknown key", line));
	start();
	line = replace("title", "x =", "x = 16\nx = 17");
	WORK_FAILS(expect(FILE_NAME ":%d: title.x: duplicate key", line + 1));
	start();
	line = replace("notice", "[notice]", "[notices]");
	WORK_FAILS(expect(FILE_NAME ":%d: [notices]: unknown section", line));
	start();
	line = replace("tab.2", "[tab.2]", "[tab.1]");
	WORK_FAILS(expect(FILE_NAME ":%d: [tab.1]: duplicate section", line));
	start();
	line = replace("tab.2", "[tab.2]", "[tab.02]");
	WORK_FAILS(expect(FILE_NAME ":%d: [tab.02]: unknown section", line));
	start();
	line = replace("tab.2", "[tab.2]", "[tab.x]");
	WORK_FAILS(expect(FILE_NAME ":%d: [tab.x]: unknown section", line));
	start();
	line = replace("list", "[list]", "[list.band]");
	WORK_FAILS(expect(FILE_NAME ":%d: [list.band]: unknown section", line));
	start();
	replace("screen", "[screen]", "colour = #000000\n[screen]");
	WORK_FAILS(FILE_NAME ":1: key outside a section");
	start();
	line = replace("list", "top =", "top 64");
	WORK_FAILS(expect(FILE_NAME ":%d: not \"key = value\" or \"[section]\"", line));
	start();
	line = replace("list", "top =",
		       "; xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
		       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
		       "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
	WORK_FAILS(expect(FILE_NAME ":%d: line longer than 255 bytes", line));
	start();
	line = replace("tab.1", "x =", "x =\t16");
	WORK_FAILS(expect(FILE_NAME ":%d: not printable ASCII", line));
	start();
	line = replace("tab.1", "x =", "x = 16\r");
	WORK_FAILS(expect(FILE_NAME ":%d: not printable ASCII", line));
}

static void test_classic_missing_keys_and_sections(void)
{
	FAILS_RAW("", FILE_NAME ": [screen]: missing");
	start();
	replace("list", "text_y", "");
	WORK_FAILS(expect(FILE_NAME ":%d: list.text_y: missing", header_line("list")));
	start();
	replace("footer", "x =", "");
	replace("list", "top =", "");
	WORK_FAILS(expect(FILE_NAME ":%d: list.top: missing", header_line("list")));
	start();
	replace("footer.band", "line_colour", "");
	replace("tabs.band", "w =", "");
	replace("screen", "image", "");
	WORK_FAILS(expect(FILE_NAME ":%d: screen.image: missing", header_line("screen")));
	start();
	replace("footer", "x =", "");
	drop("message");
	WORK_FAILS(expect(FILE_NAME ":%d: footer.x: missing", header_line("footer")));
	start();
	drop("message");
	WORK_FAILS(FILE_NAME ": [message]: missing");
	start();
	drop("notice");
	drop("message");
	WORK_FAILS(FILE_NAME ": [notice]: missing");
	start();
	drop("tab.1");
	drop("tab.2");
	WORK_FAILS(FILE_NAME ": [tab.<n>]: missing");
	start();
	drop("list.selected_disabled");
	WORK_FAILS(FILE_NAME ": [list.selected_disabled]: missing");
	start();
	drop("tabs");
	WORK_FAILS(FILE_NAME ": [tabs]: missing");
}

static void test_optional_sections(void)
{
	start();
	drop("tabs.band");
	WORK_OK();
	CHECK(!theme.tabs.band.present && theme.footer.band.present);
	start();
	drop("footer.band");
	WORK_OK();
	CHECK(!theme.footer.band.present && theme.tabs.band.present);
	start();
	replace("list", "[list]",
		"[title.rule]\ncolour = #a0a0a0\ny = 77\nthickness = 4\ndash = 8\ngap = 4\noffset = 12\nend = 610\n\n[list]");
	WORK_OK();
	CHECK(theme.title.rule.present && theme.title.rule.y == 77 && theme.title.rule.thickness == 4);
	CHECK(theme.title.rule.dash == 8 && theme.title.rule.gap == 4 && theme.title.rule.offset == 12);
	CHECK(theme.title.rule.end == 610 && theme.title.rule.colour.r == 0xa0);
	start();
	replace("list", "[list]", "[title.rule]\ncolour = #a0a0a0\ny = 77\n\n[list]");
	WORK_FAILS(expect(FILE_NAME ":%d: title.rule.thickness: missing", header_line("title.rule")));
}

#define MARKER_PROBLEM "not a marker (none, text:<chars>, sprite:<file.png>)"

static void fails_with(int test_line, const char *section, const char *prefix, const char *with, const char *fmt)
{
	int line;

	start();
	line = replace(section, prefix, with);
	parse_fails(test_line, work, expect(fmt, line));
}

#define EDIT_FAILS(section, prefix, with, fmt) fails_with(__LINE__, section, prefix, with, fmt)

static void test_classic_values(void)
{
	EDIT_FAILS("list", "top =", "top = 6x4", FILE_NAME ":%d: list.top: not an integer");
	EDIT_FAILS("list", "text_y", "text_y = -129", FILE_NAME ":%d: list.text_y: out of range -128..128");
	EDIT_FAILS("list", "text_y", "text_y = 129", FILE_NAME ":%d: list.text_y: out of range -128..128");
	EDIT_FAILS("tabs", "y =", "y = 481", FILE_NAME ":%d: tabs.y: out of range 0..480");
	EDIT_FAILS("tabs", "y =", "y = -1", FILE_NAME ":%d: tabs.y: out of range 0..480");
	EDIT_FAILS("tab.2", "x =", "x = 641", FILE_NAME ":%d: tab.2.x: out of range 0..640");
	EDIT_FAILS("list", "row_height", "row_height = 7", FILE_NAME ":%d: list.row_height: out of range 8..240");
	EDIT_FAILS("notice", "row_height", "row_height = 241", FILE_NAME ":%d: notice.row_height: out of range 8..240");
	EDIT_FAILS("list.selected", "y =", "y = 241", FILE_NAME ":%d: list.selected.y: out of range 0..240");
	EDIT_FAILS("footer.band", "y =", "y = 481", FILE_NAME ":%d: footer.band.y: out of range 0..480");
	EDIT_FAILS("footer.band", "w =", "w = 0", FILE_NAME ":%d: footer.band.w: out of range 1..640");
	EDIT_FAILS("tabs.band", "h =", "h = 481", FILE_NAME ":%d: tabs.band.h: out of range 1..480");
	EDIT_FAILS("tabs.band", "top_line", "top_line = 17", FILE_NAME ":%d: tabs.band.top_line: out of range 0..16");
	EDIT_FAILS("footer", "hint_gap", "hint_gap = -1", FILE_NAME ":%d: footer.hint_gap: out of range 0..640");
	EDIT_FAILS("message", "info_colour", "info_colour = #9ad0f", FILE_NAME ":%d: message.info_colour: not a colour (#rrggbb)");
	EDIT_FAILS("message", "info_colour", "info_colour = #9ad0fg", FILE_NAME ":%d: message.info_colour: not a colour (#rrggbb)");
	EDIT_FAILS("message", "info_colour", "info_colour = 9ad0ff", FILE_NAME ":%d: message.info_colour: not a colour (#rrggbb)");
	EDIT_FAILS("list.selected", "line_colour", "line_colour = red",
		   FILE_NAME ":%d: list.selected.line_colour: not a colour (#rrggbb)");
	EDIT_FAILS("font.mono", "file", "file = fonts/a.ttf", FILE_NAME ":%d: font.mono.file: not a file name (.ttf)");
	EDIT_FAILS("screen", "image", "image = ../frame.png", FILE_NAME ":%d: screen.image: not a file name (.png)");
}

static void test_classic_markers_and_buttons(void)
{
	start();
	replace("list", "cursor =", "cursor = sprite:triforce-16_b.png");
	replace("tabs", "active_after", "active_after = none");
	replace("footer", "button.start", "button.start = sprite:start.png");
	WORK_OK();
	CHECK(theme.list.cursor.kind == THEME_MARKER_SPRITE && strcmp(theme.list.cursor.file, "triforce-16_b.png") == 0);
	CHECK(theme.list.cursor.text[0] == '\0');
	CHECK(theme.tabs.active_after.kind == THEME_MARKER_NONE);
	CHECK(theme.list.arrow_left.kind == THEME_MARKER_TEXT && strcmp(theme.list.arrow_left.text, "<") == 0);
	CHECK(theme.footer.buttons[THEME_BUTTON_START].kind == THEME_BUTTON_SPRITE);
	CHECK(strcmp(theme.footer.buttons[THEME_BUTTON_START].file, "start.png") == 0);
	CHECK(theme.footer.buttons[THEME_BUTTON_A].kind == THEME_BUTTON_HINT_TEXT);
	EDIT_FAILS("list", "arrow_left", "arrow_left = <", FILE_NAME ":%d: list.arrow_left: " MARKER_PROBLEM);
	EDIT_FAILS("list", "arrow_left", "arrow_left = text:12345678", FILE_NAME ":%d: list.arrow_left: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =", "cursor = sprite:", FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =", "cursor = sprite:a/b.png", FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =", "cursor = sprite:..png", FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =", "cursor = sprite:.a.png", FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =", "cursor = sprite:a.ttf", FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("list", "cursor =",
		   "cursor = sprite:abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefgh.png",
		   FILE_NAME ":%d: list.cursor: " MARKER_PROBLEM);
	EDIT_FAILS("tabs", "active_before", "active_before = Sprite:a.png", FILE_NAME ":%d: tabs.active_before: " MARKER_PROBLEM);
	EDIT_FAILS("screen", "cut", "cut = sprite:cut.png", FILE_NAME ":%d: screen.cut: not a marker (none, text:<chars>)");
	EDIT_FAILS("footer", "button.a", "button.a = A", FILE_NAME ":%d: footer.button.a: not hint_text or sprite:<file.png>");
	EDIT_FAILS("footer", "button.a", "button.a = text:A", FILE_NAME ":%d: footer.button.a: not hint_text or sprite:<file.png>");
	EDIT_FAILS("footer", "button.a", "button.a = none", FILE_NAME ":%d: footer.button.a: not hint_text or sprite:<file.png>");
	EDIT_FAILS("footer", "button.y", "button.y = sprite:../y.png",
		   FILE_NAME ":%d: footer.button.y: not hint_text or sprite:<file.png>");
}

static void test_classic_cross_checks(void)
{
	EDIT_FAILS("list", "font", "font = mona", FILE_NAME ":%d: list.font: unknown font mona");
	EDIT_FAILS("footer", "font", "font = Mono", FILE_NAME ":%d: footer.font: unknown font Mono");
	start();
	replace("font.mono", "cell", "cell = none");
	WORK_FAILS(expect(FILE_NAME ":%d: font.mono.size: fit needs a cell", header_line("font.mono") + 2));
	start();
	replace("font.mono", "cell", "cell = none");
	replace("title", "font", "font = big");
	WORK_FAILS(expect(FILE_NAME ":%d: title.font: unknown font big", header_line("title") + 1));
	start();
	replace("font.mono", "[font.mono]", "[font.big]\nfile = Big.ttf\nsize = 20\ncell = none\nanchor = baseline\n\n[font.mono]");
	replace("title", "font", "font = big");
	WORK_OK();
	CHECK(theme.font_count == 2 && theme.title.font == 0 && theme.list.font == 1 && theme.tabs.font == 1);
	start();
	replace("tabs.band", "x =", "x = 1");
	WORK_FAILS(expect(FILE_NAME ":%d: tabs.band.w: outside the screen", header_line("tabs.band") + 3));
	start();
	replace("tabs.band", "x =", "x = 640");
	WORK_FAILS(expect(FILE_NAME ":%d: tabs.band.w: outside the screen", header_line("tabs.band") + 3));
	start();
	replace("footer.band", "y =", "y = 449");
	WORK_FAILS(expect(FILE_NAME ":%d: footer.band.h: outside the screen", header_line("footer.band") + 4));
	start();
	replace("footer.band", "y =", "y = 0");
	replace("footer.band", "h =", "h = 480");
	WORK_OK();
	start();
	replace("list.selected", "w =", "w = 641");
	WORK_FAILS(expect(FILE_NAME ":%d: list.selected.w: out of range 1..640", header_line("list.selected") + 3));
	start();
	replace("list.selected", "x =", "x = 1");
	WORK_FAILS(expect(FILE_NAME ":%d: list.selected.w: outside the screen", header_line("list.selected") + 3));
	EDIT_FAILS("list.selected", "h =", "h = 33", FILE_NAME ":%d: list.selected.h: out of range 1..32");
	EDIT_FAILS("list.selected_disabled", "y =", "y = 1", FILE_NAME ":%d: list.selected_disabled.y: out of range 0..0");
	start();
	replace("list.selected", "y =", "y = 4");
	replace("list.selected", "h =", "h = 28");
	WORK_OK();
	CHECK(theme.list.selected.y == 4 && theme.list.selected.h == 28);
	start();
	replace("list", "top =", "top = 416");
	WORK_FAILS(expect(FILE_NAME ":%d: list.bottom: out of range 448..480", header_line("list") + 3));
	start();
	replace("list", "top =", "top = 400");
	WORK_FAILS(expect(FILE_NAME ":%d: list.bottom: out of range 432..480", header_line("list") + 3));
	start();
	replace("list", "top =", "top = 384");
	WORK_OK();
	EDIT_FAILS("list", "value_right", "value_right = 625", FILE_NAME ":%d: list.value_right: out of range 0..624");
	EDIT_FAILS("list", "label_x =", "label_x = 624", FILE_NAME ":%d: list.label_x: out of range 0..623");
	EDIT_FAILS("list", "label_x_cursor", "label_x_cursor = 630", FILE_NAME ":%d: list.label_x_cursor: out of range 0..623");
	start();
	replace("list", "value_right", "value_right = 624");
	WORK_OK();
}

#define TAB_NUMBERING FILE_NAME ": [tab.<n>]: tab slots must be numbered 1 to N, N at most 4"
#define TAB(n) "[tab." n "]\nx = 0\nactive_x = 0\nmarker_x = 0\n\n"

static void test_classic_tab_slots(void)
{
	start();
	replace("tab.2", "[tab.2]", "[tab.3]");
	WORK_FAILS(TAB_NUMBERING);
	start();
	replace("tab.1", "[tab.1]", "[tab.0]");
	WORK_FAILS(TAB_NUMBERING);
	start();
	drop("tab.1");
	WORK_FAILS(TAB_NUMBERING);
	start();
	replace("title", "[title]", TAB("3") TAB("4") "[title]");
	WORK_OK();
	CHECK(theme.tabs.slot_count == 4 && theme.tabs.slots[1].x == 192);
	start();
	replace("title", "[title]", TAB("4") TAB("3") TAB("5") "[title]");
	WORK_FAILS(TAB_NUMBERING);
	start();
	replace("title", "[title]", TAB("999999") "[title]");
	WORK_FAILS(TAB_NUMBERING);
	start();
	replace("tab.2", "x =", "");
	replace("title", "[title]", TAB("4") "[title]");
	WORK_FAILS(expect(FILE_NAME ":%d: tab.2.x: missing", header_line("tab.2")));
	start();
	replace("tab.2", "[tab.2]", "[tab.3]");
	replace("tab.1", "[tab.1]", "[tab.2]\nx = 0\nactive_x = 0\nmarker_x = 0\n[tab.1]");
	WORK_OK();
	CHECK(theme.tabs.slot_count == 3 && theme.tabs.slots[0].x == 16 && theme.tabs.slots[2].x == 192);
}

#define LOAD_DIR "build/test_theme_load"

static void write_load_file(const char *bytes, size_t n)
{
	FILE *f;

	if (mkdir(LOAD_DIR, 0700) != 0 && errno != EEXIST)
		test_fail(TEST, "mkdir %s failed", LOAD_DIR);
	f = fopen(LOAD_DIR "/theme.ini", "wb");
	if (!f || fwrite(bytes, 1, n, f) != n || fclose(f) != 0)
		test_fail(TEST, "cannot write %s/theme.ini", LOAD_DIR);
}

static void load_fails(int line, const char *dir, const char *expected)
{
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (theme_load(&theme, dir, &err))
		test_fail(TEST, "%s:%d: loaded, expected \"%s\"", __FILE__, line, expected);
	if (strcmp(err.message, expected) != 0)
		test_fail(TEST, "%s:%d: got \"%s\", expected \"%s\"", __FILE__, line, err.message, expected);
}

static void test_load(void)
{
	ThemeError err;
	size_t n;

	load_fails(__LINE__, "build/no_such_theme", "build/no_such_theme/theme.ini: cannot open: No such file or directory");
	start();
	n = strlen(work);
	memset(work + n, '\n', THEME_FILE_BYTES_MAX - n);
	write_load_file(work, THEME_FILE_BYTES_MAX);
	if (!theme_load(&theme, LOAD_DIR, &err))
		test_fail(TEST, "%d bytes refused: %s", THEME_FILE_BYTES_MAX, err.message);
	CHECK(theme.list.top == 64);
	work[THEME_FILE_BYTES_MAX] = '\n';
	write_load_file(work, THEME_FILE_BYTES_MAX + 1);
	load_fails(__LINE__, LOAD_DIR, LOAD_DIR "/theme.ini: larger than 16384 bytes");
	start();
	replace("list", "top =", "top = 6x4");
	n = strlen(work);
	*strstr(work, "6x4") = '\0';
	write_load_file(work, n);
	load_fails(__LINE__, LOAD_DIR, expect(LOAD_DIR "/theme.ini:%d: not printable ASCII", header_line("list") + 2));
	start();
	replace("list", "top =", "top = 6x4");
	write_load_file(work, strlen(work));
	load_fails(__LINE__, LOAD_DIR, expect(LOAD_DIR "/theme.ini:%d: list.top: not an integer", header_line("list") + 2));
}

static void test_classic_loads(void)
{
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (!theme_load(&theme, CLASSIC_DIR, &err))
		test_fail(TEST, "classic refused: %s", err.message);
	CHECK(theme.screen.image.present && strcmp(theme.screen.image.file, "background.png") == 0);
	CHECK(theme.font_count == 1);
	CHECK(theme.fonts[0].size.fit);
	CHECK(theme.fonts[0].cell.present && theme.fonts[0].cell.w == 16 && theme.fonts[0].cell.h == 32);
	CHECK(theme.list.font == 0 && theme.footer.font == 0);
	CHECK(theme.list.top == 64 && theme.list.bottom == 416 && theme.list.row_height == 32);
	CHECK(theme.list.label_x == 16 && theme.list.right == 624);
	CHECK(theme.tabs.slot_count == 2);
	CHECK(theme.tabs.slots[1].x == 192 && theme.tabs.slots[1].marker_x == 176);
	CHECK(theme.tabs.active_before.kind == THEME_MARKER_TEXT && strcmp(theme.tabs.active_before.text, "[") == 0);
	CHECK(theme.tabs.band.present && theme.footer.band.present && !theme.title.rule.present);
	CHECK(theme.list.selected.present && theme.list.selected.colour.r == 0x2f);
	CHECK(theme.footer.buttons[THEME_BUTTON_START].kind == THEME_BUTTON_HINT_TEXT);
	CHECK(theme.message.error_colour.r == 0xff && theme.message.error_colour.g == 0x6b);
}

static void test_inventory_loads(void)
{
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (!theme_load(&theme, INVENTORY_DIR, &err))
		test_fail(TEST, "inventory refused: %s", err.message);
	CHECK(theme.font_count == 3);
	CHECK(theme.fonts[theme.list.font].anchor == THEME_ANCHOR_BASELINE);
	CHECK(!theme.fonts[theme.list.font].cell.present && theme.fonts[theme.list.font].size.px == 12);
	CHECK(theme.fonts[theme.tabs.font].size.px == 16 && theme.fonts[theme.notice.font].size.px == 14);
	CHECK(theme.screen.image.present && strcmp(theme.screen.image.file, "frame.png") == 0);
	CHECK(theme.tabs.slot_count == 2 && theme.tabs.slots[0].x == 30 && theme.tabs.slots[0].active_x == 58);
	CHECK(theme.tabs.active_before.kind == THEME_MARKER_SPRITE && theme.tabs.active_after.kind == THEME_MARKER_NONE);
	CHECK(theme.title.rule.present && theme.title.rule.thickness == 4 && theme.title.rule.end == 610);
	CHECK(!theme.tabs.band.present && theme.footer.band.present && theme.footer.band.top_line == 4);
	CHECK(theme.list.selected.top_line == 2 && theme.list.selected.bottom_line == 2);
	CHECK(theme.list.cursor.kind == THEME_MARKER_SPRITE && theme.list.scroll_down.kind == THEME_MARKER_SPRITE);
	CHECK(theme.footer.buttons[THEME_BUTTON_START].kind == THEME_BUTTON_SPRITE);
	CHECK(theme.footer.key_gap == 10 && theme.footer.hint_gap == 36);
	CHECK(theme.notice.emphasis_x == 50 && theme.notice.row_height == 32);
}

static void test_minimal_theme_values(void)
{
	OK(SCREEN FONT_MONO);
	CHECK(theme.screen.colour.r == 0x14 && theme.screen.colour.g == 0x16 && theme.screen.colour.b == 0x1a);
	CHECK(!theme.screen.image.present);
	CHECK(theme.screen.cut.kind == THEME_MARKER_TEXT && strcmp(theme.screen.cut.text, "~") == 0);
	CHECK(theme.font_count == 1);
	CHECK(strcmp(theme.fonts[0].name, "mono") == 0);
	CHECK(strcmp(theme.fonts[0].file, "DejaVuSansMono.ttf") == 0);
	CHECK(theme.fonts[0].size.fit);
	CHECK(theme.fonts[0].cell.present && theme.fonts[0].cell.w == 16 && theme.fonts[0].cell.h == 32);
	CHECK(theme.fonts[0].anchor == THEME_ANCHOR_TOP);
}

static void test_missing_sections(void)
{
	FAILS("", FILE_NAME ": [screen]: missing");
	FAILS(FONT_MONO, FILE_NAME ": [screen]: missing");
	FAILS(SCREEN, FILE_NAME ": [font.<name>]: missing");
}

static void test_blank_and_comment_lines_are_ignored(void)
{
	OK("\n; a comment\n   # another\n" SCREEN "\n   \n" FONT_MONO "; end");
	OK("  [ screen ]  \n  colour   =   #14161A  \nimage=none\ncut = none\n" FONT_MONO);
	CHECK(theme.screen.colour.b == 0x1a);
	CHECK(theme.screen.cut.kind == THEME_MARKER_NONE);
	OK(SCREEN "[font.mono]\nfile = a.ttf\nsize = 12\ncell = none\nanchor = baseline");
	CHECK(!theme.fonts[0].size.fit && theme.fonts[0].size.px == 12);
	CHECK(!theme.fonts[0].cell.present);
	CHECK(theme.fonts[0].anchor == THEME_ANCHOR_BASELINE);
}

static void test_line_problems(void)
{
	char long_line[512];

	memset(long_line, 0, sizeof long_line);
	strcpy(long_line, SCREEN "; ");
	memset(long_line + strlen(long_line), 'x', 255 - 2);
	OK(strcat(long_line, "\n" FONT_MONO));
	memset(long_line, 0, sizeof long_line);
	strcpy(long_line, SCREEN "; ");
	memset(long_line + strlen(long_line), 'x', 256 - 2);
	FAILS(long_line, FILE_NAME ":5: line longer than 255 bytes");
	FAILS(SCREEN "image\t= none\n", FILE_NAME ":5: not printable ASCII");
	FAILS("[screen]\r\n", FILE_NAME ":1: not printable ASCII");
	FAILS(SCREEN "; caf\xc3\xa9\n", FILE_NAME ":5: not printable ASCII");
	FAILS("[screen]\ncolour #14161a\n", FILE_NAME ":2: not \"key = value\" or \"[section]\"");
	FAILS("[screen\n", FILE_NAME ":1: not \"key = value\" or \"[section]\"");
	FAILS("[]\n", FILE_NAME ":1: not \"key = value\" or \"[section]\"");
	FAILS("[ ]\n", FILE_NAME ":1: not \"key = value\" or \"[section]\"");
	FAILS("[screen] x\n", FILE_NAME ":1: not \"key = value\" or \"[section]\"");
	FAILS("[screen]\n = #14161a\n", FILE_NAME ":2: not \"key = value\" or \"[section]\"");
	FAILS("[screen]\ncolour =\n", FILE_NAME ":2: not \"key = value\" or \"[section]\"");
	FAILS("\n; c\ncolour = #14161a\n" SCREEN, FILE_NAME ":3: key outside a section");
}

static void test_section_problems(void)
{
	FAILS(SCREEN "[tabz]\n", FILE_NAME ":5: [tabz]: unknown section");
	FAILS(SCREEN "[font]\n", FILE_NAME ":5: [font]: unknown section");
	FAILS(SCREEN "[font.]\n", FILE_NAME ":5: [font.]: unknown section");
	FAILS(SCREEN "[font.Mono]\n", FILE_NAME ":5: [font.Mono]: unknown section");
	FAILS(SCREEN "[font.a-b]\n", FILE_NAME ":5: [font.a-b]: unknown section");
	FAILS(SCREEN "[font.abcdefghijklmnop]\n", FILE_NAME ":5: [font.abcdefghijklmnop]: unknown section");
	OK(SCREEN "[font.abcdefghijklmno]\nfile = a.ttf\nsize = 8\ncell = none\nanchor = top\n" FONT_MONO);
	CHECK(strcmp(theme.fonts[0].name, "abcdefghijklmno") == 0);
	FAILS(SCREEN "[fonts.mono]\n", FILE_NAME ":5: [fonts.mono]: unknown section");
	FAILS(SCREEN "[screen.x]\n", FILE_NAME ":5: [screen.x]: unknown section");
	FAILS(SCREEN FONT_MONO "[ screen ]\n", FILE_NAME ":10: [screen]: duplicate section");
	FAILS(SCREEN FONT_MONO FONT_MONO, FILE_NAME ":10: [font.mono]: duplicate section");
}

static void test_fonts(void)
{
	OK(SCREEN FONT_MONO "[font.big]\nfile = Big-Font_2.ttf\nsize = 64\ncell = 64 128\nanchor = top\n");
	CHECK(theme.font_count == 2);
	CHECK(strcmp(theme.fonts[1].name, "big") == 0 && strcmp(theme.fonts[1].file, "Big-Font_2.ttf") == 0);
	CHECK(theme.fonts[1].size.px == 64 && theme.fonts[1].cell.w == 64 && theme.fonts[1].cell.h == 128);
	OK(SCREEN "[font.mono]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f2]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f3]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f4]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f5]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f6]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f7]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f8]\n" "file = b.ttf\nsize = 9\ncell = none\nanchor = top\n");
	CHECK(theme.font_count == THEME_FONTS_MAX);
	CHECK(strcmp(theme.fonts[7].name, "f8") == 0 && theme.fonts[7].size.px == 9);
	FAILS(SCREEN "[font.mono]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f2]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f3]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f4]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f5]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f6]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f7]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f8]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f9]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	      "[font.f10]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n",
	      FILE_NAME ": [font.f9]: more than 8 fonts");
}

static void test_key_problems(void)
{
	FAILS("[screen]\ncolor = #14161a\n", FILE_NAME ":2: screen.color: unknown key");
	FAILS("[screen]\nColour = #14161a\n", FILE_NAME ":2: screen.Colour: unknown key");
	FAILS(SCREEN "[font.mono]\nfile = a.ttf\nsize = fit\nsizes = 9\n", FILE_NAME ":8: font.mono.sizes: unknown key");
	FAILS(SCREEN "colour = #000000\n", FILE_NAME ":5: screen.colour: duplicate key");
	FAILS(SCREEN FONT_MONO "anchor = top\n", FILE_NAME ":10: font.mono.anchor: duplicate key");
	FAILS("[screen]\ncolour = #14161a\ncut = none\n" FONT_MONO, FILE_NAME ":1: screen.image: missing");
	FAILS("\n" SCREEN "\n[font.mono]\nfile = a.ttf\nsize = fit\nanchor = top\n",
	      FILE_NAME ":7: font.mono.cell: missing");
	FAILS(SCREEN "[font.a]\nfile = a.ttf\n[font.b]\nsize = 8\n", FILE_NAME ":5: font.a.size: missing");
	FAILS("[font.a]\nfile = a.ttf\n[screen]\ncolour = #14161a\n", FILE_NAME ":3: screen.image: missing");
	FAILS(FONT_MONO "[screen]\n", FILE_NAME ":6: screen.colour: missing");
}

#define SCREEN_WITH(key_line) "[screen]\n" key_line "\n" "colour = #14161a\nimage = none\ncut = none\n" FONT_MONO
#define FONT_WITH(key_line) SCREEN "[font.mono]\n" key_line "\n" "file = a.ttf\nsize = fit\ncell = 16 32\nanchor = top\n"

static void test_colour_values(void)
{
	OK(SCREEN_WITH("; c"));
	FAILS(SCREEN_WITH("colour = #14161"), FILE_NAME ":2: screen.colour: not a colour (#rrggbb)");
	FAILS(SCREEN_WITH("colour = #14161a0"), FILE_NAME ":2: screen.colour: not a colour (#rrggbb)");
	FAILS(SCREEN_WITH("colour = #14161g"), FILE_NAME ":2: screen.colour: not a colour (#rrggbb)");
	FAILS(SCREEN_WITH("colour = 14161a"), FILE_NAME ":2: screen.colour: not a colour (#rrggbb)");
	FAILS(SCREEN_WITH("colour = # 14161"), FILE_NAME ":2: screen.colour: not a colour (#rrggbb)");
	OK("[screen]\ncolour = #FfA09b\nimage = none\ncut = none\n" FONT_MONO);
	CHECK(theme.screen.colour.r == 0xff && theme.screen.colour.g == 0xa0 && theme.screen.colour.b == 0x9b);
}

static void test_file_values(void)
{
	OK("[screen]\ncolour = #000000\nimage = frame-2_b.png\ncut = none\n" FONT_MONO);
	CHECK(theme.screen.image.present && strcmp(theme.screen.image.file, "frame-2_b.png") == 0);
	FAILS(SCREEN_WITH("image = img/frame.png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = a\\b.png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = ..png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = a..b.png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = .frame.png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = frame.PNG"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = frame.ttf"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = .png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = my frame.png"), FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(SCREEN_WITH("image = Frame"), FILE_NAME ":2: screen.image: not a file name (.png)");
	OK("[screen]\ncolour = #000000\nimage = "
	   "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefg.png\ncut = none\n" FONT_MONO);
	FAILS(SCREEN_WITH("image = abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefgh.png"),
	      FILE_NAME ":2: screen.image: not a file name (.png)");
	FAILS(FONT_WITH("file = mono.png"), FILE_NAME ":6: font.mono.file: not a file name (.ttf)");
	FAILS(FONT_WITH("file = ../mono.ttf"), FILE_NAME ":6: font.mono.file: not a file name (.ttf)");
	FAILS(FONT_WITH("file = none"), FILE_NAME ":6: font.mono.file: not a file name (.ttf)");
}

static void test_marker_values(void)
{
	OK("[screen]\ncolour = #000000\nimage = none\ncut = text:1234567\n" FONT_MONO);
	CHECK(theme.screen.cut.kind == THEME_MARKER_TEXT && strcmp(theme.screen.cut.text, "1234567") == 0);
	OK("[screen]\ncolour = #000000\nimage = none\ncut = text:[#]\n" FONT_MONO);
	CHECK(strcmp(theme.screen.cut.text, "[#]") == 0);
	FAILS(SCREEN_WITH("cut = text:12345678"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
	FAILS(SCREEN_WITH("cut = text:"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
	FAILS(SCREEN_WITH("cut = text:a b"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
	FAILS(SCREEN_WITH("cut = ~"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
	FAILS(SCREEN_WITH("cut = None"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
	FAILS(SCREEN_WITH("cut = sprite:cut.png"), FILE_NAME ":2: screen.cut: not a marker (none, text:<chars>)");
}

static void test_size_values(void)
{
	OK(FONT_WITH("; c"));
	FAILS(FONT_WITH("size = 7"), FILE_NAME ":6: font.mono.size: out of range 8..64");
	FAILS(FONT_WITH("size = 65"), FILE_NAME ":6: font.mono.size: out of range 8..64");
	FAILS(FONT_WITH("size = -12"), FILE_NAME ":6: font.mono.size: out of range 8..64");
	FAILS(FONT_WITH("size = 99999999999999999999"), FILE_NAME ":6: font.mono.size: out of range 8..64");
	FAILS(FONT_WITH("size = 12px"), FILE_NAME ":6: font.mono.size: not an integer");
	FAILS(FONT_WITH("size = -"), FILE_NAME ":6: font.mono.size: not an integer");
	FAILS(FONT_WITH("size = +12"), FILE_NAME ":6: font.mono.size: not an integer");
	FAILS(FONT_WITH("size = 1 2"), FILE_NAME ":6: font.mono.size: not an integer");
	FAILS(FONT_WITH("size = Fit"), FILE_NAME ":6: font.mono.size: not an integer");
	OK(SCREEN "[font.mono]\nfile = a.ttf\nsize = 8\ncell = none\nanchor = top\n");
	CHECK(theme.fonts[0].size.px == 8 && !theme.fonts[0].size.fit);
}

static void test_cell_values(void)
{
	OK(SCREEN "[font.mono]\nfile = a.ttf\nsize = fit\ncell = 4   4\nanchor = top\n");
	CHECK(theme.fonts[0].cell.present && theme.fonts[0].cell.w == 4 && theme.fonts[0].cell.h == 4);
	FAILS(FONT_WITH("cell = 3 32"), FILE_NAME ":6: font.mono.cell: out of range 4..64");
	FAILS(FONT_WITH("cell = 65 32"), FILE_NAME ":6: font.mono.cell: out of range 4..64");
	FAILS(FONT_WITH("cell = 16 3"), FILE_NAME ":6: font.mono.cell: out of range 4..128");
	FAILS(FONT_WITH("cell = 16 129"), FILE_NAME ":6: font.mono.cell: out of range 4..128");
	FAILS(FONT_WITH("cell = 16"), FILE_NAME ":6: font.mono.cell: not an integer");
	FAILS(FONT_WITH("cell = 16 32 1"), FILE_NAME ":6: font.mono.cell: not an integer");
	FAILS(FONT_WITH("cell = 16x32"), FILE_NAME ":6: font.mono.cell: not an integer");
	FAILS(FONT_WITH("cell = a 32"), FILE_NAME ":6: font.mono.cell: not an integer");
	FAILS(FONT_WITH("cell = fit"), FILE_NAME ":6: font.mono.cell: not an integer");
}

static void test_anchor_values(void)
{
	FAILS(FONT_WITH("anchor = Top"), FILE_NAME ":6: font.mono.anchor: not top or baseline");
	FAILS(FONT_WITH("anchor = bottom"), FILE_NAME ":6: font.mono.anchor: not top or baseline");
}

static void test_message_is_truncated_not_overflowed(void)
{
	char text[400];
	ThemeError err;
	size_t n;

	strcpy(text, "[screen]\n");
	n = strlen(text);
	memset(text + n, 'k', 250);
	strcpy(text + n + 250, " = 1\n");
	memset(&err, 0x55, sizeof err);
	CHECK(!theme_parse(&theme, text, text, &err));
	CHECK(strlen(err.message) == THEME_ERROR_MAX - 1);
	CHECK(strncmp(err.message, "[screen]\n", 9) == 0);
}

static void test_classic_page_and_columns(void)
{
	static UiGrid grid;
	ThemeError err;

	memset(&err, 0, sizeof err);
	if (!theme_load(&theme, CLASSIC_DIR, &err))
		test_fail(TEST, "classic refused: %s", err.message);
	CHECK(ui_grid_init(&grid, UI_GRID_DEFAULT_COLS, UI_GRID_DEFAULT_LINES));
	CHECK(theme_page(&theme) == 11);
	CHECK(theme_page(&theme) == ui_grid_page(&grid));
	CHECK(theme_columns(&theme, theme.fonts[theme.list.font].cell.w) == 38);
	CHECK(theme_columns(&theme, theme.fonts[theme.list.font].cell.w) == UI_GRID_DEFAULT_COLS - 2);
}

static void list_row_height(int px)
{
	char line[32];

	snprintf(line, sizeof line, "row_height = %d", px);
	replace("list", "row_height", line);
	snprintf(line, sizeof line, "h = %d", px);
	replace("list.selected", "h =", line);
	replace("list.selected_disabled", "h =", line);
}

static void test_page_floors_partial_rows(void)
{
	start();
	list_row_height(24);
	WORK_OK();
	CHECK(theme_page(&theme) == 14);
	start();
	list_row_height(30);
	WORK_OK();
	CHECK(theme_page(&theme) == 11);
	start();
	replace("notice", "row_height", "row_height = 24");
	WORK_OK();
	CHECK(theme_page(&theme) == 11);
}

static void test_columns_floor_partial_characters(void)
{
	start();
	replace("list", "label_x =", "label_x = 20");
	WORK_OK();
	CHECK(theme_columns(&theme, 16) == 37);
	start();
	WORK_OK();
	CHECK(theme_columns(&theme, 12) == 50);
	start();
	replace("list", "label_x_cursor", "label_x_cursor = 100");
	WORK_OK();
	CHECK(theme_columns(&theme, 16) == 38);
}

#define HELP_KEYS "font = mono\nx = 30\ny = 412\nright = 610\ncolour = #78d8f8\n"

/* Inserts [help] with the given key lines right before [footer]; returns its header's line. */
static int add_help(const char *keys)
{
	char with[512];

	snprintf(with, sizeof with, "[help]\n%s\n[footer]", keys);
	start();
	return replace("footer", "[footer]", with);
}

static void help_key_fails(int test_line, const char *prefix, const char *with, const char *fmt)
{
	int line;

	add_help(HELP_KEYS);
	line = replace("help", prefix, with);
	parse_fails(test_line, work, expect(fmt, line));
}

#define HELP_FAILS(prefix, with, fmt) help_key_fails(__LINE__, prefix, with, fmt)

static void test_help_absent(void)
{
	ThemeError err;

	start();
	WORK_OK();
	CHECK(!theme.help.present && theme.help.font == 0 && theme.help.x == 0 && theme.help.right == 0);
	CHECK(theme_help_columns(&theme, 10) == 0);
	memset(&err, 0, sizeof err);
	if (!theme_load(&theme, INVENTORY_DIR, &err))
		test_fail(TEST, "inventory refused: %s", err.message);
	CHECK(!theme.help.present && theme_help_columns(&theme, 10) == 0);
}

static void test_help_present(void)
{
	static const char *const keys[] = { "font", "x", "y", "right", "colour" };
	char without[sizeof HELP_KEYS];
	char message[THEME_ERROR_MAX];
	size_t i;
	int line;

	add_help(HELP_KEYS);
	WORK_OK();
	CHECK(theme.help.present && theme.help.font == 0);
	CHECK(theme.help.x == 30 && theme.help.y == 412 && theme.help.right == 610);
	CHECK(theme.help.colour.r == 0x78 && theme.help.colour.g == 0xd8 && theme.help.colour.b == 0xf8);
	add_help(HELP_KEYS);
	replace("font.mono", "[font.mono]", "[font.small]\nfile = Small.ttf\nsize = 10\ncell = none\nanchor = baseline\n\n[font.mono]");
	replace("help", "font", "font = small");
	WORK_OK();
	CHECK(theme.help.font == 0 && theme.list.font == 1);
	for (i = 0; i < sizeof keys / sizeof keys[0]; i++) {
		const char *at = HELP_KEYS;
		char *out = without;

		while (*at) {
			const char *end = strchr(at, '\n') + 1;

			if (!(strncmp(at, keys[i], strlen(keys[i])) == 0 && at[strlen(keys[i])] == ' ')) {
				memcpy(out, at, (size_t)(end - at));
				out += end - at;
			}
			at = end;
		}
		*out = '\0';
		line = add_help(without);
		snprintf(message, sizeof message, FILE_NAME ":%d: help.%s: missing", line, keys[i]);
		WORK_FAILS(message);
	}
	add_help(HELP_KEYS);
	line = replace("help", "x =", "x = 30\nleft = 4");
	WORK_FAILS(expect(FILE_NAME ":%d: help.left: unknown key", line + 1));
	add_help(HELP_KEYS);
	line = replace("help", "x =", "x = 30\nx = 31");
	WORK_FAILS(expect(FILE_NAME ":%d: help.x: duplicate key", line + 1));
}

static void test_help_values(void)
{
	int line;

	add_help(HELP_KEYS);
	line = replace("help", "font", "font = tiny");
	WORK_FAILS(expect(FILE_NAME ":%d: help.font: unknown font tiny", line));
	HELP_FAILS("x =", "x = 641", FILE_NAME ":%d: help.x: out of range 0..640");
	HELP_FAILS("x =", "x = -1", FILE_NAME ":%d: help.x: out of range 0..640");
	HELP_FAILS("y =", "y = 481", FILE_NAME ":%d: help.y: out of range 0..480");
	HELP_FAILS("right", "right = 641", FILE_NAME ":%d: help.right: out of range 0..640");
	HELP_FAILS("colour", "colour = blue", FILE_NAME ":%d: help.colour: not a colour (#rrggbb)");
	HELP_FAILS("x =", "x = 610", FILE_NAME ":%d: help.x: out of range 0..609");
	HELP_FAILS("x =", "x = 620", FILE_NAME ":%d: help.x: out of range 0..609");
	add_help(HELP_KEYS);
	replace("help", "x =", "x = 609");
	WORK_OK();
	CHECK(theme_help_columns(&theme, 1) == 1);
	add_help(HELP_KEYS);
	line = replace("footer", "[footer]", "[help]\n" HELP_KEYS "\n[footer]");
	WORK_FAILS(expect(FILE_NAME ":%d: [help]: duplicate section", line));
}

static void test_help_columns_floor_partial_characters(void)
{
	add_help(HELP_KEYS);
	WORK_OK();
	CHECK(theme_help_columns(&theme, 10) == 58);
	CHECK(theme_help_columns(&theme, 12) == 48);
	CHECK(theme_help_columns(&theme, 16) == 36);
	CHECK(theme_help_columns(&theme, 1) == 580);
}

static void test_alttp_loads_within_the_app_ranges(void)
{
	ThemeError err;
	int page, columns, help, id;

	memset(&err, 0, sizeof err);
	if (!theme_load(&theme, ALTTP_DIR, &err))
		test_fail(TEST, "alttp refused: %s", err.message);
	CHECK(theme.help.present);
	CHECK(!theme.fonts[theme.list.font].size.fit && !theme.fonts[theme.help.font].size.fit);
	page = theme_page(&theme);
	columns = theme_columns(&theme, theme.fonts[theme.list.font].size.px);
	help = theme_help_columns(&theme, theme.fonts[theme.help.font].size.px);
	if (page < 1 || page > UI_GRID_LINES_MAX - 4)
		test_fail(TEST, "alttp page %d outside 1..%d", page, UI_GRID_LINES_MAX - 4);
	if (columns < UI_HISTORY_WIDTH_MIN || columns > UI_HISTORY_WIDTH_MAX)
		test_fail(TEST, "alttp width %d outside %d..%d", columns, UI_HISTORY_WIDTH_MIN, UI_HISTORY_WIDTH_MAX);
	if (help != UI_STR_HELP_LEN_MAX)
		test_fail(TEST, "alttp help line of %d characters, the core's help bound is %d", help,
			  UI_STR_HELP_LEN_MAX);
	for (id = UI_STR_HELP_FIRST; id <= UI_STR_HELP_LAST; id++)
		if ((int)strlen(ui_str(ui_str_english(), (UiStrId)id)) > help)
			test_fail(TEST, "help %d \"%s\" is longer than alttp's %d-character help line", id,
				  ui_str(ui_str_english(), (UiStrId)id), help);
}

static bool ends_with(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);

	return n >= m && strcmp(s + n - m, suffix) == 0;
}

/* false when a or b cannot be opened; otherwise *same says whether their bytes are equal. */
static bool same_bytes(const char *a, const char *b, bool *same)
{
	FILE *fa = fopen(a, "rb");
	FILE *fb = fa ? fopen(b, "rb") : NULL;
	int ca, cb;

	if (!fa || !fb) {
		if (fa)
			fclose(fa);
		return false;
	}
	do {
		ca = getc(fa);
		cb = getc(fb);
	} while (ca == cb && ca != EOF);
	*same = ca == cb && !ferror(fa) && !ferror(fb);
	fclose(fa);
	fclose(fb);
	return true;
}

static void test_alttp_fonts_are_data_copies(void)
{
	DIR *dir = opendir(ALTTP_DIR);
	struct dirent *e;
	int compared = 0;

	if (!dir)
		test_fail(TEST, "cannot open %s: %s", ALTTP_DIR, strerror(errno));
	while ((e = readdir(dir)) != NULL) {
		char ours[512], theirs[512];
		bool same = false;
		FILE *f;

		if (!ends_with(e->d_name, ".ttf") && !ends_with(e->d_name, "-LICENSE.txt"))
			continue;
		snprintf(theirs, sizeof theirs, "%s/%s", DATA_DIR, e->d_name);
		f = fopen(theirs, "rb");
		if (!f)
			continue;
		fclose(f);
		snprintf(ours, sizeof ours, "%s/%s", ALTTP_DIR, e->d_name);
		if (!same_bytes(ours, theirs, &same))
			test_fail(TEST, "cannot read %s", ours);
		if (!same)
			test_fail(TEST, "%s differs from %s", ours, theirs);
		compared++;
	}
	closedir(dir);
	if (compared < 2)
		test_fail(TEST, "%s: %d font or licence files shared with %s, expected at least 2", ALTTP_DIR, compared,
			  DATA_DIR);
}

static void test_alttp_holds_only_its_named_files(void)
{
	ThemeAsset assets[THEME_ASSETS_MAX];
	ThemeError err;
	DIR *dir;
	struct dirent *e;
	int n, i;

	if (!theme_load(&theme, ALTTP_DIR, &err))
		test_fail(TEST, "alttp refused: %s", err.message);
	n = theme_assets(&theme, assets);
	for (i = 0; i < n; i++) {
		char path[512];
		struct stat st;

		snprintf(path, sizeof path, "%s/%s", ALTTP_DIR, assets[i].file);
		if (stat(path, &st) != 0 || !S_ISREG(st.st_mode))
			test_fail(TEST, "%s: %s names %s, not a file of the folder", ALTTP_DIR, assets[i].key, assets[i].file);
	}
	dir = opendir(ALTTP_DIR);
	if (!dir)
		test_fail(TEST, "cannot open %s: %s", ALTTP_DIR, strerror(errno));
	while ((e = readdir(dir)) != NULL) {
		bool named = strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 ||
			     strcmp(e->d_name, "theme.ini") == 0 || ends_with(e->d_name, "-LICENSE.txt");

		for (i = 0; i < n && !named; i++)
			named = strcmp(e->d_name, assets[i].file) == 0;
		if (!named)
			test_fail(TEST, "%s/%s: named by no key of theme.ini and not a licence; it would ship in the zip",
				  ALTTP_DIR, e->d_name);
	}
	closedir(dir);
}

static void check_asset(int line,const ThemeAsset *a, ThemeAssetKind kind, const char *key, const char *file)
{
	if (a->kind != kind || strcmp(a->key, key) != 0 || strcmp(a->file, file) != 0)
		test_fail(TEST, "%s:%d: asset %d %s %s, expected %d %s %s", __FILE__, line, (int)a->kind, a->key, a->file,
			  (int)kind, key, file);
}

#define ASSET(a, kind, key, file) check_asset(__LINE__, a, kind, key, file)

static void test_classic_assets(void)
{
	ThemeAsset assets[THEME_ASSETS_MAX];
	ThemeError err;

	if (!theme_load(&theme, CLASSIC_DIR, &err))
		test_fail(TEST, "classic refused: %s", err.message);
	CHECK(theme_assets(&theme, assets) == 2);
	ASSET(&assets[0], THEME_ASSET_SCREEN, "screen.image", "background.png");
	ASSET(&assets[1], THEME_ASSET_FONT, "font.mono.file", "DejaVuSansMono.ttf");
	CHECK(assets[1].font == 0);
}

static void test_inventory_assets(void)
{
	static const struct {
		ThemeAssetKind kind;
		const char *key, *file;
	} want[] = {
		{ THEME_ASSET_SCREEN, "screen.image", "frame.png" },
		{ THEME_ASSET_FONT, "font.ps2p_16.file", "PressStart2P.ttf" },
		{ THEME_ASSET_FONT, "font.ps2p_14.file", "PressStart2P.ttf" },
		{ THEME_ASSET_FONT, "font.ps2p_12.file", "PressStart2P.ttf" },
		{ THEME_ASSET_SPRITE, "tabs.active_before", "triforce_tab.png" },
		{ THEME_ASSET_SPRITE, "list.cursor", "triforce_row.png" },
		{ THEME_ASSET_SPRITE, "list.cursor_disabled", "triforce_row_disabled.png" },
		{ THEME_ASSET_SPRITE, "list.arrow_left", "arrow_left.png" },
		{ THEME_ASSET_SPRITE, "list.arrow_right", "arrow_right.png" },
		{ THEME_ASSET_SPRITE, "list.scroll_up", "scroll_up.png" },
		{ THEME_ASSET_SPRITE, "list.scroll_down", "scroll_down.png" },
		{ THEME_ASSET_SPRITE, "footer.button.left_right", "button_dpad.png" },
		{ THEME_ASSET_SPRITE, "footer.button.a", "button_a.png" },
		{ THEME_ASSET_SPRITE, "footer.button.b", "button_b.png" },
		{ THEME_ASSET_SPRITE, "footer.button.x", "button_x.png" },
		{ THEME_ASSET_SPRITE, "footer.button.y", "button_y.png" },
		{ THEME_ASSET_SPRITE, "footer.button.start", "button_start.png" },
	};
	ThemeAsset assets[THEME_ASSETS_MAX];
	ThemeError err;
	size_t i;

	if (!theme_load(&theme, INVENTORY_DIR, &err))
		test_fail(TEST, "inventory refused: %s", err.message);
	CHECK(theme_assets(&theme, assets) == (int)(sizeof want / sizeof want[0]));
	for (i = 0; i < sizeof want / sizeof want[0]; i++)
		ASSET(&assets[i], want[i].kind, want[i].key, want[i].file);
	CHECK(assets[1].font == 0 && assets[2].font == 1 && assets[3].font == 2);
	CHECK(assets[0].font == -1 && assets[4].font == -1);
}

static void test_assets_skip_text_markers_and_a_missing_image(void)
{
	ThemeAsset assets[THEME_ASSETS_MAX];

	start();
	replace("screen", "image", "image = none");
	replace("tabs", "active_before", "active_before = sprite:same.png");
	replace("tabs", "active_after", "active_after = sprite:same.png");
	replace("footer", "button.b", "button.b = sprite:b.png");
	WORK_OK();
	CHECK(theme_assets(&theme, assets) == 4);
	ASSET(&assets[0], THEME_ASSET_FONT, "font.mono.file", "DejaVuSansMono.ttf");
	ASSET(&assets[1], THEME_ASSET_SPRITE, "tabs.active_before", "same.png");
	ASSET(&assets[2], THEME_ASSET_SPRITE, "tabs.active_after", "same.png");
	ASSET(&assets[3], THEME_ASSET_SPRITE, "footer.button.b", "b.png");
}

static void test_path_join(void)
{
	char out[16];

	CHECK(theme_path(out, sizeof out, "dir", "a.png"));
	CHECK(strcmp(out, "dir/a.png") == 0);
	CHECK(theme_path(out, sizeof out, "012345678", "a.png"));
	CHECK(strcmp(out, "012345678/a.png") == 0);
	CHECK(!theme_path(out, sizeof out, "0123456789", "a.png"));
	CHECK(!theme_path(out, sizeof out, "dir", "0123456789abc.png"));
}

int main(void)
{
	read_classic();
	test_classic_loads();
	test_classic_page_and_columns();
	test_page_floors_partial_rows();
	test_columns_floor_partial_characters();
	test_inventory_loads();
	test_classic_assets();
	test_inventory_assets();
	test_assets_skip_text_markers_and_a_missing_image();
	test_path_join();
	test_help_absent();
	test_help_present();
	test_help_values();
	test_help_columns_floor_partial_characters();
	test_alttp_loads_within_the_app_ranges();
	test_alttp_fonts_are_data_copies();
	test_alttp_holds_only_its_named_files();
	test_classic_line_and_section_problems();
	test_classic_missing_keys_and_sections();
	test_optional_sections();
	test_classic_values();
	test_classic_markers_and_buttons();
	test_classic_cross_checks();
	test_classic_tab_slots();
	test_load();
	test_minimal_theme_values();
	test_missing_sections();
	test_blank_and_comment_lines_are_ignored();
	test_line_problems();
	test_section_problems();
	test_fonts();
	test_key_problems();
	test_colour_values();
	test_file_values();
	test_marker_values();
	test_size_values();
	test_cell_values();
	test_anchor_values();
	test_message_is_truncated_not_overflowed();
	printf("%s: passed\n", TEST);
	return 0;
}
