#include "rando_app_shell.h"
#include "rando_settings.h"
#include "ui_app.h"
#include "ui_grid.h"
#include "ui_history.h"
#include "ui_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEYS_MAX 4096
#define REPEAT_MAX 999
#define TOKEN_MAX 32

static const char usage[] = "usage: ui_cli --sd <root> [--every] [--cols <n>] [--lines <n>] [--seed <n>] "
			    "[--date <YYYY-MM-DDTHH:MM:SS>] [--source-md5 <32 hex>] [--base-md5 <32 hex>] [--] <key>...\n"
			    "keys: up down left right a b x y l1 r1 l2 r2 select start menu, <key>*<n> repeats, "
			    "- reads keys from standard input\n";

/* In UiButton's order. */
static const char *const key_names[] = {"up", "down", "left", "right", "a",	  "b",	   "x",	   "y",
					"l1", "r1",   "l2",   "r2",    "select", "start", "menu"};

typedef char key_names_match_buttons[sizeof key_names / sizeof key_names[0] == UI_BUTTON_COUNT ? 1 : -1];

typedef struct Key {
	UiButton button;
	int count;
} Key;

typedef struct Options {
	const char *sd;
	bool every;
	int cols, lines;
	bool has_seed, has_date, has_source_md5, has_base_md5;
	uint32_t seed;
	RandoRecordDate date;
	uint8_t source_md5[RANDO_MD5_SIZE], base_md5[RANDO_MD5_SIZE];
	Key keys[KEYS_MAX];
	int key_count;
	long presses;
} Options;

static RandoAppShell shell;
static Options opt;
static UiGrid grid;
static char text[UI_GRID_TEXT_MAX];
static uint32_t fixed_seed_value;
static RandoRecordDate fixed_date_value;

static uint32_t fixed_seed(void)
{
	return fixed_seed_value;
}

static void fixed_date(RandoRecordDate *out)
{
	*out = fixed_date_value;
}

static void fail_usage(const char *what, const char *arg)
{
	fprintf(stderr, "ui_cli: %s%s%s\n%s", what, arg != NULL ? ": " : "", arg != NULL ? arg : "", usage);
	exit(2);
}

static bool parse_number(const char *s, uint32_t *out)
{
	return rando_parse_uint32(s, strlen(s), out) && strchr(s, '\n') == NULL;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool parse_md5(const char *s, uint8_t out[RANDO_MD5_SIZE])
{
	int i;

	if (strlen(s) != 2 * RANDO_MD5_SIZE)
		return false;
	for (i = 0; i < RANDO_MD5_SIZE; i++) {
		int hi = hex_digit(s[2 * i]), lo = hex_digit(s[2 * i + 1]);

		if (hi < 0 || lo < 0)
			return false;
		out[i] = (uint8_t)(hi * 16 + lo);
	}
	return true;
}

static bool field(const char *s, int from, int n, int min, int max, int *out)
{
	int i, v = 0;

	for (i = from; i < from + n; i++) {
		if (s[i] < '0' || s[i] > '9')
			return false;
		v = v * 10 + (s[i] - '0');
	}
	*out = v;
	return v >= min && v <= max;
}

static bool parse_date(const char *s, RandoRecordDate *d)
{
	return strlen(s) == 19 && s[4] == '-' && s[7] == '-' && s[10] == 'T' && s[13] == ':' && s[16] == ':' &&
	       field(s, 0, 4, 0, 9999, &d->year) && field(s, 5, 2, 1, 12, &d->month) &&
	       field(s, 8, 2, 1, 31, &d->day) && field(s, 11, 2, 0, 23, &d->hour) &&
	       field(s, 14, 2, 0, 59, &d->minute) && field(s, 17, 2, 0, 59, &d->second);
}

static void add_key(const char *token)
{
	char name[TOKEN_MAX];
	const char *star = strchr(token, '*');
	size_t len = star != NULL ? (size_t)(star - token) : strlen(token);
	uint32_t count = 1;
	int b;

	if (len >= sizeof name)
		fail_usage("unknown key", token);
	memcpy(name, token, len);
	name[len] = '\0';
	if (star != NULL && (!parse_number(star + 1, &count) || count < 1 || count > REPEAT_MAX))
		fail_usage("bad repeat count", token);
	for (b = 0; b < UI_BUTTON_COUNT; b++)
		if (strcmp(name, key_names[b]) == 0)
			break;
	if (b == UI_BUTTON_COUNT)
		fail_usage("unknown key", token);
	if (opt.key_count == KEYS_MAX)
		fail_usage("too many keys", NULL);
	opt.keys[opt.key_count].button = (UiButton)b;
	opt.keys[opt.key_count].count = (int)count;
	opt.key_count++;
	opt.presses += (long)count;
}

static void read_stdin_keys(void)
{
	char token[TOKEN_MAX + 1];
	size_t len = 0;
	int c;

	do {
		c = getchar();
		if (c == EOF || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			if (len > 0) {
				token[len] = '\0';
				add_key(token);
				len = 0;
			}
		} else {
			if (len == TOKEN_MAX) {
				token[len] = '\0';
				fail_usage("unknown key", token);
			}
			token[len++] = (char)c;
		}
	} while (c != EOF);
}

static const char *value_of(int argc, char **argv, int *i)
{
	if (*i + 1 >= argc)
		fail_usage("missing value for", argv[*i]);
	*i += 1;
	return argv[*i];
}

static void parse_size(const char *s, const char *what, int min, int max, int *out)
{
	uint32_t v;

	if (!parse_number(s, &v) || v < (uint32_t)min || v > (uint32_t)max)
		fail_usage(what, s);
	*out = (int)v;
}

static void parse_args(int argc, char **argv)
{
	bool keys_only = false;
	int i;

	opt.cols = UI_GRID_DEFAULT_COLS;
	opt.lines = UI_GRID_DEFAULT_LINES;
	for (i = 1; i < argc; i++) {
		const char *a = argv[i];

		if (keys_only || strncmp(a, "--", 2) != 0) {
			keys_only = true;
			if (strcmp(a, "-") == 0)
				read_stdin_keys();
			else
				add_key(a);
		} else if (strcmp(a, "--") == 0) {
			keys_only = true;
		} else if (strcmp(a, "--sd") == 0) {
			opt.sd = value_of(argc, argv, &i);
		} else if (strcmp(a, "--every") == 0) {
			opt.every = true;
		} else if (strcmp(a, "--cols") == 0) {
			parse_size(value_of(argc, argv, &i), "bad --cols", UI_HISTORY_WIDTH_MIN + 2, UI_GRID_COLS_MAX,
				   &opt.cols);
		} else if (strcmp(a, "--lines") == 0) {
			parse_size(value_of(argc, argv, &i), "bad --lines", UI_GRID_LINES_MIN, UI_GRID_LINES_MAX,
				   &opt.lines);
		} else if (strcmp(a, "--seed") == 0) {
			const char *v = value_of(argc, argv, &i);

			if (!parse_number(v, &opt.seed))
				fail_usage("bad --seed", v);
			opt.has_seed = true;
		} else if (strcmp(a, "--date") == 0) {
			const char *v = value_of(argc, argv, &i);

			if (!parse_date(v, &opt.date))
				fail_usage("bad --date", v);
			opt.has_date = true;
		} else if (strcmp(a, "--source-md5") == 0) {
			const char *v = value_of(argc, argv, &i);

			if (!parse_md5(v, opt.source_md5))
				fail_usage("bad --source-md5", v);
			opt.has_source_md5 = true;
		} else if (strcmp(a, "--base-md5") == 0) {
			const char *v = value_of(argc, argv, &i);

			if (!parse_md5(v, opt.base_md5))
				fail_usage("bad --base-md5", v);
			opt.has_base_md5 = true;
		} else {
			fail_usage("unknown option", a);
		}
	}
	if (opt.sd == NULL)
		fail_usage("missing --sd", NULL);
}

static void print_screen(const char *header)
{
	static UiScreen screen;

	if (header != NULL)
		printf("@ %s\n", header);
	ui_app_screen(&shell.app, &screen);
	if (!ui_grid_render(&grid, &screen) || ui_grid_text(&grid, text, sizeof text) == 0) {
		fprintf(stderr, "ui_cli: the screen does not fit the grid\n");
		exit(1);
	}
	fputs(text, stdout);
}

static void set_up(void)
{
	if (!ui_grid_init(&grid, opt.cols, opt.lines))
		fail_usage("bad grid size", NULL);
	if (!rando_app_shell_init(&shell, opt.sd, NULL, ui_grid_page(&grid), opt.cols - 2)) {
		fprintf(stderr, "ui_cli: cannot use SD root %s\n", opt.sd);
		exit(1);
	}
	if (opt.has_seed) {
		fixed_seed_value = opt.seed;
		shell.gen.seed = fixed_seed;
	}
	if (opt.has_date) {
		fixed_date_value = opt.date;
		shell.gen.date = fixed_date;
	}
	if (opt.has_source_md5)
		memcpy(shell.gen.source_md5, opt.source_md5, RANDO_MD5_SIZE);
	if (opt.has_base_md5)
		memcpy(shell.gen.base_md5, opt.base_md5, RANDO_MD5_SIZE);
}

static UiAppEvent play(UiButton button)
{
	if (ui_app_input(&shell.app, button, true) == UI_APP_QUIT)
		return UI_APP_QUIT;
	return ui_app_input(&shell.app, button, false);
}

int main(int argc, char **argv)
{
	long played = 0;
	int k, r;

	parse_args(argc, argv);
	set_up();
	if (opt.every)
		print_screen("init");
	for (k = 0; k < opt.key_count; k++) {
		for (r = 0; r < opt.keys[k].count; r++) {
			const char *name = key_names[opt.keys[k].button];
			UiAppEvent event = play(opt.keys[k].button);

			played++;
			if (ui_app_pending(&shell.app)) {
				if (opt.every)
					print_screen("generating");
				ui_app_run(&shell.app);
			}
			if (opt.every)
				print_screen(name);
			if (event == UI_APP_QUIT) {
				if (!opt.every)
					print_screen(NULL);
				if (played < opt.presses)
					fprintf(stderr, "ui_cli: quit at key %ld of %ld, %ld ignored\n", played, opt.presses,
						opt.presses - played);
				return EXIT_SUCCESS;
			}
		}
	}
	if (!opt.every)
		print_screen(NULL);
	return EXIT_SUCCESS;
}
