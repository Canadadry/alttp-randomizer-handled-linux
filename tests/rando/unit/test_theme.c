#include "support.h"
#include "theme.h"

#include <stdio.h>
#include <string.h>

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

static Theme theme;

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

#define OK(text) parse_ok(__LINE__, text)
#define FAILS(text, expected) parse_fails(__LINE__, text, expected)

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
	OK(SCREEN "[font.abcdefghijklmno]\nfile = a.ttf\nsize = 8\ncell = none\nanchor = top\n");
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
	OK(SCREEN "[font.f1]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f2]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f3]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f4]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f5]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f6]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f7]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
	   "[font.f8]\n" "file = b.ttf\nsize = 9\ncell = none\nanchor = top\n");
	CHECK(theme.font_count == THEME_FONTS_MAX);
	CHECK(strcmp(theme.fonts[7].name, "f8") == 0 && theme.fonts[7].size.px == 9);
	FAILS(SCREEN "[font.f1]\n" "file = a.ttf\nsize = 8\ncell = none\nanchor = top\n"
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

int main(void)
{
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
