#include "rando_fixed_run.h"
#include "support.h"
#include "ui_input.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST "test_fixed_run"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static RandoFixedKeys keys;

static void test_key_names(void)
{
	static const char *const names[UI_BUTTON_COUNT] = {"up", "down", "left", "right", "a",	   "b",
							   "x",	 "y",	 "l1",	 "r1",	  "l2",	   "r2",
							   "select", "start", "menu"};
	int b;

	memset(&keys, 0, sizeof keys);
	for (b = 0; b < UI_BUTTON_COUNT; b++) {
		CHECK(strcmp(rando_fixed_key_name((UiButton)b), names[b]) == 0);
		CHECK(rando_fixed_keys_add(&keys, names[b]) == RANDO_FIXED_KEY_OK);
		CHECK(keys.count == b + 1);
		CHECK(keys.key[b].button == (UiButton)b);
		CHECK(keys.key[b].count == 1);
	}
	CHECK(keys.presses == UI_BUTTON_COUNT);
}

static void test_repeats(void)
{
	memset(&keys, 0, sizeof keys);
	CHECK(rando_fixed_keys_add(&keys, "down*1") == RANDO_FIXED_KEY_OK);
	CHECK(rando_fixed_keys_add(&keys, "start*999") == RANDO_FIXED_KEY_OK);
	CHECK(keys.count == 2);
	CHECK(keys.key[0].button == UI_BUTTON_DOWN && keys.key[0].count == 1);
	CHECK(keys.key[1].button == UI_BUTTON_START && keys.key[1].count == 999);
	CHECK(keys.presses == 1000);
	CHECK(rando_fixed_keys_add(&keys, "down*0") == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(rando_fixed_keys_add(&keys, "down*1000") == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(rando_fixed_keys_add(&keys, "down*") == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(rando_fixed_keys_add(&keys, "down*2x") == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(rando_fixed_keys_add(&keys, "down*-1") == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(keys.count == 2 && keys.presses == 1000);
}

static void test_unknown_keys(void)
{
	memset(&keys, 0, sizeof keys);
	CHECK(rando_fixed_keys_add(&keys, "") == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(rando_fixed_keys_add(&keys, "Up") == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(rando_fixed_keys_add(&keys, "home") == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(rando_fixed_keys_add(&keys, "home*2") == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(rando_fixed_keys_add(&keys, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa") == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(keys.count == 0 && keys.presses == 0);
}

static void test_key_cap(void)
{
	int i;

	memset(&keys, 0, sizeof keys);
	for (i = 0; i < RANDO_FIXED_KEYS_MAX; i++)
		CHECK(rando_fixed_keys_add(&keys, "a*2") == RANDO_FIXED_KEY_OK);
	CHECK(keys.count == RANDO_FIXED_KEYS_MAX);
	CHECK(rando_fixed_keys_add(&keys, "a") == RANDO_FIXED_KEY_TOO_MANY);
	CHECK(keys.count == RANDO_FIXED_KEYS_MAX && keys.presses == 2L * RANDO_FIXED_KEYS_MAX);
}

static FILE *stream_of(const char *text)
{
	FILE *f = tmpfile();

	if (f == NULL || fputs(text, f) == EOF || fseek(f, 0, SEEK_SET) != 0)
		test_fail(TEST, "cannot build a stream");
	return f;
}

static RandoFixedKeyStatus read_text(const char *text, char bad[RANDO_FIXED_TOKEN_MAX + 1])
{
	FILE *f = stream_of(text);
	RandoFixedKeyStatus status;

	memset(&keys, 0, sizeof keys);
	bad[0] = '\0';
	status = rando_fixed_keys_read(&keys, f, bad);
	fclose(f);
	return status;
}

static void test_read_stream(void)
{
	char bad[RANDO_FIXED_TOKEN_MAX + 1];

	CHECK(read_text(" down\tright\r\nup*2\n\nmenu", bad) == RANDO_FIXED_KEY_OK);
	CHECK(keys.count == 4 && keys.presses == 5);
	CHECK(keys.key[0].button == UI_BUTTON_DOWN && keys.key[1].button == UI_BUTTON_RIGHT);
	CHECK(keys.key[2].button == UI_BUTTON_UP && keys.key[2].count == 2);
	CHECK(keys.key[3].button == UI_BUTTON_MENU);
	CHECK(read_text("", bad) == RANDO_FIXED_KEY_OK && keys.count == 0);

	CHECK(read_text("down nope up", bad) == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(strcmp(bad, "nope") == 0 && keys.count == 1);
	CHECK(read_text("up*0", bad) == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(strcmp(bad, "up*0") == 0);
	CHECK(read_text("up bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", bad) == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(strlen(bad) == RANDO_FIXED_TOKEN_MAX && strspn(bad, "b") == RANDO_FIXED_TOKEN_MAX);
	CHECK(keys.count == 1);
}

static void test_takes(void)
{
	CHECK(rando_fixed_run_takes("--seed"));
	CHECK(rando_fixed_run_takes("--date"));
	CHECK(rando_fixed_run_takes("--source-md5"));
	CHECK(rando_fixed_run_takes("--base-md5"));
	CHECK(!rando_fixed_run_takes("--sd"));
	CHECK(!rando_fixed_run_takes("--seeds"));
	CHECK(!rando_fixed_run_takes("seed"));
}

static void test_seed(void)
{
	RandoFixedRun run;

	memset(&run, 0, sizeof run);
	CHECK(rando_fixed_run_option(&run, "--seed", "0") && run.has_seed && run.seed == 0);
	CHECK(rando_fixed_run_option(&run, "--seed", "4294967295") && run.seed == 4294967295u);
	CHECK(!rando_fixed_run_option(&run, "--seed", "4294967296"));
	CHECK(!rando_fixed_run_option(&run, "--seed", ""));
	CHECK(!rando_fixed_run_option(&run, "--seed", "12x"));
	CHECK(!rando_fixed_run_option(&run, "--seed", "-1"));
	CHECK(!rando_fixed_run_option(&run, "--seed", "12\n"));
	CHECK(run.seed == 4294967295u);
	CHECK(!run.has_date && !run.has_source_md5 && !run.has_base_md5);
}

static bool date_is(const RandoRecordDate *d, int year, int month, int day, int hour, int minute, int second)
{
	return d->year == year && d->month == month && d->day == day && d->hour == hour && d->minute == minute &&
	       d->second == second;
}

static void test_date(void)
{
	static const char *const bad[] = {
		"2026-13-01T00:00:00", "2026-00-01T00:00:00", "1969-12-31T23:59:59", "2026-11-32T00:00:00",
		"2026-11-01T24:00:00", "2026-11-01T00:60:00", "2026-11-01T00:00:60", "2026/11/01T00:00:00",
		"2026-11-01 00:00:00", "2026-11-01T00-00-00", "2026-11-01T00:00",    "2026-11-01T00:00:000",
		"2026-1a-01T00:00:00", "",
	};
	RandoFixedRun run;
	size_t i;

	memset(&run, 0, sizeof run);
	CHECK(rando_fixed_run_option(&run, "--date", "2026-11-01T14:03:27") && run.has_date);
	CHECK(date_is(&run.date, 2026, 11, 1, 14, 3, 27));
	CHECK(rando_fixed_run_option(&run, "--date", "2024-02-29T23:59:59"));
	CHECK(date_is(&run.date, 2024, 2, 29, 23, 59, 59));
	for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		if (rando_fixed_run_option(&run, "--date", bad[i]))
			test_fail(TEST, "date accepted: %s", bad[i]);
		CHECK(date_is(&run.date, 2024, 2, 29, 23, 59, 59));
	}
	CHECK(!run.has_seed);
}

static void test_md5(void)
{
	static const uint8_t want[RANDO_MD5_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
						     0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
	RandoFixedRun run;

	memset(&run, 0, sizeof run);
	CHECK(rando_fixed_run_option(&run, "--source-md5", "0123456789abcdef0123456789ABCDEF"));
	CHECK(run.has_source_md5 && !run.has_base_md5);
	CHECK(memcmp(run.source_md5, want, RANDO_MD5_SIZE) == 0);
	CHECK(rando_fixed_run_option(&run, "--base-md5", "0123456789ABCDEF0123456789abcdef"));
	CHECK(run.has_base_md5 && memcmp(run.base_md5, want, RANDO_MD5_SIZE) == 0);
	CHECK(!rando_fixed_run_option(&run, "--base-md5", "0123456789abcdef0123456789abcdeg"));
	CHECK(!rando_fixed_run_option(&run, "--base-md5", "0123456789abcdef0123456789abcde"));
	CHECK(!rando_fixed_run_option(&run, "--base-md5", "0123456789abcdef0123456789abcdef0"));
	CHECK(!rando_fixed_run_option(&run, "--source-md5", "zz"));
	CHECK(memcmp(run.source_md5, want, RANDO_MD5_SIZE) == 0);
	CHECK(memcmp(run.base_md5, want, RANDO_MD5_SIZE) == 0);
}

static RandoAppShell shell, before;

static void start_shell(void)
{
	CHECK(rando_app_shell_init(&shell, "/nonexistent/test_fixed_run", NULL, 10, 40));
	memcpy(&before, &shell, sizeof shell);
}

static void test_apply_nothing(void)
{
	RandoFixedRun run;

	memset(&run, 0, sizeof run);
	start_shell();
	rando_fixed_run_apply(&run, &shell);
	CHECK(memcmp(&shell, &before, sizeof shell) == 0);
}

static void test_apply_all(void)
{
	RandoFixedRun run;
	RandoRecordDate date;

	memset(&run, 0, sizeof run);
	CHECK(rando_fixed_run_option(&run, "--seed", "1234567"));
	CHECK(rando_fixed_run_option(&run, "--date", "2026-11-01T14:03:27"));
	CHECK(rando_fixed_run_option(&run, "--source-md5", "00112233445566778899aabbccddeeff"));
	CHECK(rando_fixed_run_option(&run, "--base-md5", "ffeeddccbbaa99887766554433221100"));
	start_shell();
	rando_fixed_run_apply(&run, &shell);
	CHECK(shell.gen.seed() == 1234567);
	CHECK(shell.gen.seed() == 1234567);
	memset(&date, 0, sizeof date);
	shell.gen.date(&date);
	CHECK(date_is(&date, 2026, 11, 1, 14, 3, 27));
	CHECK(memcmp(shell.gen.source_md5, run.source_md5, RANDO_MD5_SIZE) == 0);
	CHECK(memcmp(shell.gen.base_md5, run.base_md5, RANDO_MD5_SIZE) == 0);

	shell.gen.seed = before.gen.seed;
	shell.gen.date = before.gen.date;
	memcpy(shell.gen.source_md5, before.gen.source_md5, RANDO_MD5_SIZE);
	memcpy(shell.gen.base_md5, before.gen.base_md5, RANDO_MD5_SIZE);
	CHECK(memcmp(&shell, &before, sizeof shell) == 0);
}

static void test_apply_some(void)
{
	RandoFixedRun run;

	memset(&run, 0, sizeof run);
	CHECK(rando_fixed_run_option(&run, "--date", "2030-01-02T03:04:05"));
	CHECK(rando_fixed_run_option(&run, "--base-md5", "ffeeddccbbaa99887766554433221100"));
	start_shell();
	rando_fixed_run_apply(&run, &shell);
	CHECK(shell.gen.seed == before.gen.seed && shell.gen.date != before.gen.date);
	CHECK(memcmp(shell.gen.source_md5, before.gen.source_md5, RANDO_MD5_SIZE) == 0);
	CHECK(memcmp(shell.gen.base_md5, run.base_md5, RANDO_MD5_SIZE) == 0);
}

static void test_key_argument(void)
{
	static const char long_name[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
	char buf[RANDO_FIXED_TOKEN_MAX + 1];
	const char *bad = NULL;
	FILE *in = stream_of("left*3 nope");

	memset(&keys, 0, sizeof keys);
	CHECK(rando_fixed_keys_arg(&keys, "up*2", in, buf, &bad) == RANDO_FIXED_KEY_OK);
	CHECK(rando_fixed_keys_arg(&keys, "-", in, buf, &bad) == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(bad == buf && strcmp(bad, "nope") == 0);
	CHECK(keys.count == 2 && keys.key[1].button == UI_BUTTON_LEFT && keys.presses == 5);
	CHECK(rando_fixed_keys_arg(&keys, "up*0", in, buf, &bad) == RANDO_FIXED_KEY_BAD_REPEAT);
	CHECK(strcmp(bad, "up*0") == 0);
	CHECK(rando_fixed_keys_arg(&keys, long_name, in, buf, &bad) == RANDO_FIXED_KEY_UNKNOWN);
	CHECK(bad == long_name);
	CHECK(keys.count == 2);
	fclose(in);
}

int main(int argc, char **argv)
{
	(void)test_fixtures_mode(TEST, argc, argv);
	test_key_names();
	test_repeats();
	test_unknown_keys();
	test_key_cap();
	test_read_stream();
	test_key_argument();
	test_takes();
	test_seed();
	test_date();
	test_md5();
	test_apply_nothing();
	test_apply_all();
	test_apply_some();
	return 0;
}
