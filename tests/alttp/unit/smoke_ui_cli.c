#define _POSIX_C_SOURCE 200809L

#include "rando_seed_save.h"
#include "support.h"
#include "synthetic_base.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST "smoke_ui_cli"
#define SNAPSHOTS "unit/snapshots/"
#define OUT_MAX (256 * 1024)

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static const char *cli;
/* The SD tree, and a scratch folder outside it for standard error. */
static char root[64], scratch[64];
static char fixed[512];
static char out[OUT_MAX], err[OUT_MAX];
static int status;

static const char *at(const char *dir, const char *rel)
{
	static char path[512];

	snprintf(path, sizeof path, "%s/%s", dir, rel);
	return path;
}

static bool exists(const char *rel)
{
	struct stat st;

	return stat(at(root, rel), &st) == 0;
}

static void make_dir(const char *rel)
{
	if (mkdir(at(root, rel), 0700) != 0)
		test_fail(TEST, "cannot create %s", at(root, rel));
}

static void write_bytes(const char *rel, const uint8_t *bytes, size_t size)
{
	FILE *f = fopen(at(root, rel), "wb");

	if (f == NULL || fwrite(bytes, 1, size, f) != size || fclose(f) != 0)
		test_fail(TEST, "cannot write %s", at(root, rel));
}

static void remove_all(const char *path)
{
	struct stat st;
	DIR *d;
	struct dirent *e;
	char child[512];

	if (lstat(path, &st) != 0)
		return;
	if (!S_ISDIR(st.st_mode)) {
		remove(path);
		return;
	}
	d = opendir(path);
	while (d != NULL && (e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		snprintf(child, sizeof child, "%s/%s", path, e->d_name);
		remove_all(child);
	}
	if (d != NULL)
		closedir(d);
	rmdir(path);
}

/* Registered with atexit, so a failing check still removes both trees. */
static void drop_trees(void)
{
	if (root[0] != '\0')
		remove_all(root);
	if (scratch[0] != '\0')
		remove_all(scratch);
	root[0] = scratch[0] = '\0';
}

static void make_tree(char *dir, const char *pattern)
{
	strcpy(dir, pattern);
	if (mkdtemp(dir) == NULL) {
		dir[0] = '\0';
		test_fail(TEST, "mkdtemp failed");
	}
}

/* Every path under dir, one per line, appended to buf. */
static void list_tree(const char *dir, char *buf, size_t cap)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char child[512];
	struct stat st;

	while (d != NULL && (e = readdir(d)) != NULL) {
		size_t used = strlen(buf);

		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		snprintf(child, sizeof child, "%s/%s", dir, e->d_name);
		snprintf(buf + used, cap - used, "%s\n", child);
		if (lstat(child, &st) == 0 && S_ISDIR(st.st_mode))
			list_tree(child, buf, cap);
	}
	if (d != NULL)
		closedir(d);
}

static void read_all(FILE *f, char *buf)
{
	size_t n = fread(buf, 1, OUT_MAX - 1, f);

	buf[n] = '\0';
}

/* Runs `<stdin_text | >ui_cli <args>`; fills out, err and status (the exit status, -1 when not exited). */
static void run(const char *stdin_text, const char *args)
{
	char command[2048];
	FILE *p, *e;
	int raw;

	if (stdin_text != NULL)
		snprintf(command, sizeof command, "printf '%%s' '%s' | '%s' %s 2>'%s'", stdin_text, cli, args,
			 at(scratch, "err"));
	else
		snprintf(command, sizeof command, "'%s' %s </dev/null 2>'%s'", cli, args, at(scratch, "err"));
	p = popen(command, "r");
	CHECK(p != NULL);
	read_all(p, out);
	raw = pclose(p);
	status = WIFEXITED(raw) ? WEXITSTATUS(raw) : -1;
	e = fopen(at(scratch, "err"), "r");
	CHECK(e != NULL);
	read_all(e, err);
	fclose(e);
}

static void hex(char *dst, const uint8_t *digest)
{
	int i;

	for (i = 0; i < RANDO_MD5_SIZE; i++)
		sprintf(dst + 2 * i, "%02x", digest[i]);
}

static void run_fixed(const char *keys)
{
	char args[1024];

	snprintf(args, sizeof args, "%s %s", fixed, keys);
	run(NULL, args);
}

static void test_generate_and_find(void)
{
	run_fixed("--every a start a r1");
	CHECK(status == 0);
	CHECK(err[0] == '\0');
	test_snapshot(TEST, SNAPSHOTS "ui_cli_generate_and_find.txt", out);
	CHECK(exists(RANDO_SD_LAST_GAME_FILE));
	CHECK(exists(RANDO_SD_ROM_DIR "/Open-Ganon-1CA10A.sfc"));
	CHECK(exists(RANDO_SD_HISTORY_DIR "/1CA10A.json"));
	CHECK(exists(RANDO_SD_SETTINGS_FILE));
}

static void test_next_start(void)
{
	char args[1024];

	snprintf(args, sizeof args, "%s -", fixed);
	run(" a r1\n", args);
	CHECK(status == 0);
	CHECK(err[0] == '\0');
	test_snapshot(TEST, SNAPSHOTS "app_shell_generated.txt", out);
}

static void test_delete(void)
{
	run_fixed("a r1 x a");
	CHECK(status == 0);
	test_snapshot(TEST, SNAPSHOTS "ui_cli_deleted.txt", out);
	CHECK(!exists(RANDO_SD_ROM_DIR "/Open-Ganon-1CA10A.sfc"));
	CHECK(!exists(RANDO_SD_HISTORY_DIR "/1CA10A.json"));
	CHECK(exists(RANDO_SD_SETTINGS_FILE));
}

/* One framed screen: exactly two frame lines, the first and the last. */
static bool one_screen(const char *text)
{
	const char *line = text, *last = NULL;
	int frames = 0;

	while (*line != '\0') {
		if (strncmp(line, " +", 2) == 0) {
			frames++;
			last = line;
		}
		line = strchr(line, '\n');
		if (line == NULL)
			return false;
		line++;
	}
	return frames == 2 && strncmp(text, " +", 2) == 0 && last != NULL && strchr(last, '\n')[1] == '\0';
}

static void test_every_key_name(void)
{
	run_fixed("a up down left right x y l1 l2 r2*2 select r1 start b a menu");
	CHECK(status == 0);
	CHECK(err[0] == '\0');
	CHECK(one_screen(out));
	CHECK(exists(RANDO_SD_ROM_DIR "/Open-Ganon-1CA10A.sfc"));
}

static void test_early_quit(void)
{
	run_fixed("a menu down*3");
	CHECK(status == 0);
	CHECK(strcmp(err, "ui_cli: quit at key 2 of 5, 3 ignored\n") == 0);
	test_snapshot(TEST, SNAPSHOTS "generate_initial.txt", out);
}

static void test_games_quit(void)
{
	run_fixed("--every a b b");
	CHECK(status == 0);
	CHECK(err[0] == '\0');
	test_snapshot(TEST, SNAPSHOTS "ui_cli_games_quit.txt", out);
}

static void test_ladx_remembered(void)
{
	static const char ladx[] = "{\"last_game\":\"ladx\"}\n";

	write_bytes(RANDO_SD_LAST_GAME_FILE, (const uint8_t *)ladx, sizeof ladx - 1);
	run_fixed("--every a b menu");
	CHECK(status == 0);
	CHECK(err[0] == '\0');
	test_snapshot(TEST, SNAPSHOTS "ui_cli_ladx_remembered.txt", out);
}

static void run_ladx(const char *keys)
{
	char args[1024];

	snprintf(args, sizeof args, "%s --ladx-seed 00112233445566778899aabbccddeeff %s", fixed, keys);
	run(NULL, args);
	CHECK(status == 0);
	CHECK(err[0] == '\0');
}

/* After test_ladx_remembered: A opens LADX. */
static void test_ladx_grid(void)
{
	run_ladx("--every a r2*3 right l2 l2");
	test_snapshot(TEST, SNAPSHOTS "ui_cli_ladx_grid.txt", out);
}

static void test_ladx_base(void)
{
	static uint8_t wrong[1024 * 1024];
	static const char junk[] = "{\"logic\": \"hard\"}\n";

	run_ladx("a right start");
	test_snapshot(TEST, SNAPSHOTS "genflow_ladx_base_missing.txt", out);
	CHECK(!exists(RANDO_SD_LADX_SETTINGS_FILE) && !exists(RANDO_SD_LADX_ROM_DIR));
	make_dir(RANDO_SD_LADX_DIR);
	memset(wrong, 0x5A, sizeof wrong);
	write_bytes(RANDO_SD_LADX_BASE_FILE, wrong, sizeof wrong);
	run_ladx("a right start");
	test_snapshot(TEST, SNAPSHOTS "genflow_ladx_base_wrong.txt", out);
	CHECK(exists(RANDO_SD_LADX_SETTINGS_FILE) && !exists(RANDO_SD_LADX_ROM_DIR));
	run_ladx("a");
	test_snapshot(TEST, SNAPSHOTS "ui_cli_ladx_restored.txt", out);
	write_bytes(RANDO_SD_LADX_SETTINGS_FILE, (const uint8_t *)junk, sizeof junk - 1);
	run_ladx("a");
	test_snapshot(TEST, SNAPSHOTS "ui_cli_ladx_unreadable.txt", out);
	run_ladx("a r1");
	test_snapshot(TEST, SNAPSHOTS "ui_cli_ladx_history_empty.txt", out);
}

static void test_usage(void)
{
	static char before[OUT_MAX], after[OUT_MAX];
	const char *bad[] = {"bogus", "r1*0", "r1*1000", "--cols 21", "--lines 41", "--date 2026-13-01T00:00:00",
			     "--date 1969-12-31T23:59:59 --every start a",
			     "--seed x", "--base-md5 abc", "--ladx-seed 0011", "--every"};
	char args[1024];
	size_t i;

	before[0] = '\0';
	list_tree(root, before, sizeof before);
	for (i = 0; i < sizeof bad / sizeof bad[0]; i++) {
		snprintf(args, sizeof args, "%s %s r1", fixed, bad[i]);
		if (strcmp(bad[i], "--every") == 0)
			snprintf(args, sizeof args, "--every r1");
		run(NULL, args);
		if (status != 2 || out[0] != '\0' || strncmp(err, "ui_cli: ", 8) != 0 || strstr(err, "usage: ") == NULL)
			test_fail(TEST, "%s: status %d, out %s, err %s", bad[i], status, out, err);
	}
	run("r1 bogus", "--sd '/nonexistent' -");
	CHECK(status == 2 && out[0] == '\0');
	after[0] = '\0';
	list_tree(root, after, sizeof after);
	CHECK(strcmp(before, after) == 0);
}

static void test_bad_root(void)
{
	static char args[RANDO_SEED_DIR_MAX + 64];
	char long_root[RANDO_SEED_DIR_MAX + 8];

	memset(long_root, 'a', sizeof long_root - 1);
	long_root[0] = '/';
	long_root[sizeof long_root - 1] = '\0';
	snprintf(args, sizeof args, "--sd '%s' r1", long_root);
	run(NULL, args);
	CHECK(status == 1);
	CHECK(out[0] == '\0');
	CHECK(strncmp(err, "ui_cli: cannot use SD root ", 27) == 0);
}

int main(int argc, char **argv)
{
	char source_md5[2 * RANDO_MD5_SIZE + 1], base_md5[2 * RANDO_MD5_SIZE + 1];

	if (argc != 2) {
		fprintf(stderr, "usage: " TEST " <ui_cli>\n");
		return 2;
	}
	cli = argv[1];
	atexit(drop_trees);
	synth_build();
	make_tree(root, "/tmp/smoke_ui_cli_XXXXXX");
	make_tree(scratch, "/tmp/smoke_ui_cli_err_XXXXXX");
	make_dir("App");
	make_dir(RANDO_SD_APP_DIR);
	make_dir(RANDO_SD_ALTTP_DIR);
	write_bytes(RANDO_SD_BASE_ROM_FILE, synth_source, SYNTH_SOURCE_SIZE);
	write_bytes(RANDO_SD_BASE_PATCH_FILE, synth_good_patch.bytes, synth_good_patch.size);
	hex(source_md5, synth_source_md5);
	hex(base_md5, synth_base_md5);
	snprintf(fixed, sizeof fixed, "--sd '%s' --seed 1 --date 2026-11-01T14:03:27 --source-md5 %s --base-md5 %s",
		 root, source_md5, base_md5);
	test_generate_and_find();
	test_next_start();
	test_delete();
	test_every_key_name();
	test_early_quit();
	test_games_quit();
	test_usage();
	test_bad_root();
	test_ladx_remembered();
	test_ladx_grid();
	test_ladx_base();
	printf(TEST ": passed\n");
	return EXIT_SUCCESS;
}
