#define _POSIX_C_SOURCE 200809L

#include "rando_ladx_gen.h"
#include "rando_ladx_record.h"
#include "rando_out.h"
#include "rando_seed_files.h"

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const RandoAppText gen_texts[RANDO_LADX_GEN_STATUS_COUNT] = {
	[RANDO_LADX_GEN_OK] = {UI_STR_GEN_OK, UI_STR_ADVICE_NONE},
	[RANDO_LADX_GEN_BASE_MISSING] = {UI_STR_GEN_BASE_MISSING, UI_STR_LADX_ADVICE_BASE},
	[RANDO_LADX_GEN_BASE_READ] = {UI_STR_GEN_BASE_READ, UI_STR_LADX_ADVICE_BASE},
	[RANDO_LADX_GEN_BASE_SIZE] = {UI_STR_ASSEMBLE_SOURCE_SIZE, UI_STR_LADX_ADVICE_BASE},
	[RANDO_LADX_GEN_BASE_SHA1] = {UI_STR_LADX_BASE_SHA1, UI_STR_LADX_ADVICE_BASE},
	[RANDO_LADX_GEN_MEMORY] = {UI_STR_LADX_MEMORY, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_LADX_GEN_TABLES] = {UI_STR_LADX_TABLES, UI_STR_ADVICE_REINSTALL},
	[RANDO_LADX_GEN_UNSUPPORTED] = {UI_STR_ASSEMBLE_UNSUPPORTED, UI_STR_ADVICE_CHANGE_SETTINGS},
	[RANDO_LADX_GEN_FOLDERS] = {UI_STR_GEN_FOLDERS, UI_STR_ADVICE_SD_CARD},
	[RANDO_LADX_GEN_SEED] = {UI_STR_LADX_SEED, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_LADX_GEN_NO_NAME] = {UI_STR_SAVE_NO_NAME, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_LADX_GEN_GAVE_UP] = {UI_STR_ASSEMBLE_GAVE_UP, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_LADX_GEN_BROKEN] = {UI_STR_LADX_BROKEN, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_LADX_GEN_ROM_WRITE] = {UI_STR_ASSEMBLE_OUTPUT, UI_STR_ADVICE_SD_CARD},
	[RANDO_LADX_GEN_RECORD] = {UI_STR_SAVE_RECORD, UI_STR_ADVICE_SD_CARD},
};

RandoAppText rando_ladx_gen_text(RandoLadxGenStatus status)
{
	assert((size_t)status < RANDO_LADX_GEN_STATUS_COUNT);
	return gen_texts[status];
}

RandoLadxBaseStatus rando_ladx_base_check(const char *path, const uint8_t sha1[LADX_SHA1_SIZE], uint8_t *buf)
{
	uint8_t digest[LADX_SHA1_SIZE];
	size_t size = 0;
	long total;

	switch (rando_out_read_capped(path, buf, LADX_ROM_SIZE, &size, &total)) {
	case RANDO_OUT_READ_OK:
		break;
	case RANDO_OUT_READ_TOO_LARGE:
		return RANDO_LADX_BASE_SIZE;
	case RANDO_OUT_READ_MISSING:
		return RANDO_LADX_BASE_MISSING;
	case RANDO_OUT_READ_OPEN:
	case RANDO_OUT_READ_READ:
		return RANDO_LADX_BASE_READ;
	}
	if (size != LADX_ROM_SIZE)
		return RANDO_LADX_BASE_SIZE;
	ladx_sha1(buf, LADX_ROM_SIZE, digest);
	return memcmp(digest, sha1, LADX_SHA1_SIZE) == 0 ? RANDO_LADX_BASE_OK : RANDO_LADX_BASE_SHA1;
}

static bool join(char *out, size_t cap, const char *root, const char *rel)
{
	int n = snprintf(out, cap, "%s/%s", root, rel);

	return n >= 0 && (size_t)n < cap;
}

static bool urandom_seed(uint8_t seed[RANDO_LADX_SEED_BYTES])
{
	FILE *f = fopen("/dev/urandom", "rb");
	size_t n;

	if (f == NULL)
		return false;
	n = fread(seed, 1, RANDO_LADX_SEED_BYTES, f);
	fclose(f);
	return n == RANDO_LADX_SEED_BYTES;
}

static void local_date(RandoRecordDate *out)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	memset(out, 0, sizeof *out);
	if (tm == NULL)
		return;
	out->year = tm->tm_year + 1900;
	out->month = tm->tm_mon + 1;
	out->day = tm->tm_mday;
	out->hour = tm->tm_hour;
	out->minute = tm->tm_min;
	out->second = tm->tm_sec;
}

static void file_sink(void *ctx, const char *text, size_t len)
{
	(void)fwrite(text, 1, len, (FILE *)ctx);
}

static bool render_spoiler(const LadxAssembler *a, FILE *out)
{
	return ladx_spoiler_render(&a->spoiler, file_sink, out) != 0 && !ferror(out);
}

static int hex_value(char c)
{
	return c <= '9' ? c - '0' : c - 'a' + 10;
}

bool rando_ladx_gen_init(RandoLadxGen *gen, const char *sd_root)
{
	static const char *const hex = LADX_ROM_SHA1_HEX;
	size_t len = strlen(sd_root);
	int i;

	memset(gen, 0, sizeof *gen);
	if (len >= sizeof gen->root)
		return false;
	memcpy(gen->root, sd_root, len + 1);
	if (!join(gen->rom_dir, sizeof gen->rom_dir, sd_root, RANDO_SD_LADX_ROM_DIR) ||
	    !join(gen->history_dir, sizeof gen->history_dir, sd_root, RANDO_SD_LADX_HISTORY_DIR) ||
	    !join(gen->base_rom, sizeof gen->base_rom, sd_root, RANDO_SD_LADX_BASE_FILE) ||
	    !join(gen->tables_dir, sizeof gen->tables_dir, sd_root, RANDO_SD_LADX_TABLES_DIR))
		return false;
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		gen->base_sha1[i] = (uint8_t)(hex_value(hex[2 * i]) << 4 | hex_value(hex[2 * i + 1]));
	gen->seed = urandom_seed;
	gen->date = local_date;
	gen->alloc = malloc;
	gen->release = free;
	gen->assemble = ladx_assemble;
	gen->spoiler = render_spoiler;
	return true;
}

static bool make_dir(const char *root, const char *rel)
{
	char path[RANDO_SEED_PATH_MAX];
	struct stat st;

	if (!join(path, sizeof path, root, rel))
		return false;
	if (mkdir(path, 0777) == 0)
		return true;
	return errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool make_folders(const char *root)
{
	static const char *const dirs[] = {"Roms",	     "Roms/GBC",	RANDO_SD_LADX_ROM_DIR,	   "App",
					     RANDO_SD_APP_DIR, RANDO_SD_LADX_DIR, RANDO_SD_LADX_HISTORY_DIR};
	size_t i;

	for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++)
		if (!make_dir(root, dirs[i]))
			return false;
	return true;
}

static bool exists(const char *path)
{
	struct stat st;

	return lstat(path, &st) == 0;
}

static bool set_paths(RandoLadxGen *gen, const RandoName *name)
{
	int a = snprintf(gen->rom_path, sizeof gen->rom_path, "%s/%s", gen->rom_dir, name->rom);
	int b = snprintf(gen->record_path, sizeof gen->record_path, "%s/%s.json", gen->history_dir, name->code);
	int c = snprintf(gen->spoiler_path, sizeof gen->spoiler_path, "%s/%s.spoiler.json", gen->history_dir,
			 name->code);

	return a > 0 && (size_t)a < sizeof gen->rom_path && b > 0 && (size_t)b < sizeof gen->record_path && c > 0 &&
	       (size_t)c < sizeof gen->spoiler_path;
}

static bool taken(void *ctx, const RandoName *candidate)
{
	RandoLadxGen *gen = ctx;

	return !set_paths(gen, candidate) || exists(gen->rom_path) || exists(gen->record_path);
}

static RandoLadxGenStatus assemble_status(const LadxAssembler *a, LadxAssembleStatus status)
{
	if (status == LADX_ASSEMBLE_OUTCOME ||
	    (status == LADX_ASSEMBLE_PIPELINE && a->pipeline_error.status == LADX_PIPELINE_RAISED))
		return RANDO_LADX_GEN_GAVE_UP;
	return RANDO_LADX_GEN_BROKEN;
}

static bool write_rom(const char *path, const uint8_t *image)
{
	char part[RANDO_SEED_PATH_MAX + 8];
	FILE *f;
	bool ok;

	snprintf(part, sizeof part, "%s.part", path);
	f = fopen(part, "wb");
	if (f == NULL)
		return false;
	ok = fwrite(image, 1, LADX_ROM_SIZE, f) == LADX_ROM_SIZE;
	if (fclose(f) != 0)
		ok = false;
	if (ok && rename(part, path) != 0)
		ok = false;
	if (!ok)
		remove(part);
	return ok;
}

static bool write_spoiler(RandoLadxGen *gen, const LadxAssembler *a)
{
	FILE *f = fopen(gen->spoiler_path, "wb");
	bool ok;

	if (f == NULL)
		return false;
	ok = gen->spoiler(a, f);
	if (fclose(f) != 0)
		ok = false;
	return ok;
}

static bool write_record(const char *path, const RandoLadxRecord *record)
{
	FILE *f = fopen(path, "wb");
	bool ok;

	if (f == NULL)
		return false;
	ok = rando_ladx_record_write(f, record);
	if (fclose(f) != 0)
		ok = false;
	return ok;
}

static void hex_of(const uint8_t *bytes, size_t n, char *out)
{
	static const char digit[] = "0123456789abcdef";
	size_t i;

	for (i = 0; i < n; i++) {
		out[2 * i] = digit[bytes[i] >> 4];
		out[2 * i + 1] = digit[bytes[i] & 15];
	}
	out[2 * n] = '\0';
}

/* Base, tables, settings, folders, seed, name, ROM, spoiler, record: each step only after the one before. */
static RandoLadxGenStatus generate(RandoLadxGen *gen, RandoLadxWork *work, const RandoLadxGenRequest *request,
				   bool *spoiler)
{
	static const RandoLadxGenStatus base_statuses[RANDO_LADX_BASE_STATUS_COUNT] = {
		[RANDO_LADX_BASE_OK] = RANDO_LADX_GEN_OK,
		[RANDO_LADX_BASE_MISSING] = RANDO_LADX_GEN_BASE_MISSING,
		[RANDO_LADX_BASE_READ] = RANDO_LADX_GEN_BASE_READ,
		[RANDO_LADX_BASE_SIZE] = RANDO_LADX_GEN_BASE_SIZE,
		[RANDO_LADX_BASE_SHA1] = RANDO_LADX_GEN_BASE_SHA1,
	};
	uint8_t seed[RANDO_LADX_SEED_BYTES];
	RandoLadxRecord record;
	LadxAssembleInput in;
	RandoRecordDate date;
	RandoLadxGenStatus status = base_statuses[rando_ladx_base_check(gen->base_rom, gen->base_sha1, work->base)];

	if (status != RANDO_LADX_GEN_OK)
		return status;
	if (!ladx_inputs_tables("zrando", gen->tables_dir, &work->tables))
		return RANDO_LADX_GEN_TABLES;
	memset(&record, 0, sizeof record);
	record.settings = request->settings;
	ladx_settings_validate(&record.settings);
	if (!ladx_settings_supported(&record.settings, NULL))
		return RANDO_LADX_GEN_UNSUPPORTED;
	if (!make_folders(gen->root))
		return RANDO_LADX_GEN_FOLDERS;
	if (!gen->seed(seed))
		return RANDO_LADX_GEN_SEED;
	hex_of(seed, sizeof seed, record.seed);
	if (!rando_ladx_name_choose(&record.settings, record.seed, taken, gen, &gen->name))
		return RANDO_LADX_GEN_NO_NAME;
	(void)set_paths(gen, &gen->name);
	record.name = gen->name;
	gen->date(&date);
	if (!rando_record_date_valid(&date)) {
		static const RandoRecordDate epoch = {1970, 1, 1, 0, 0, 0};

		date = epoch;
	}
	record.date = date;

	memset(&in, 0, sizeof in);
	in.base = work->base;
	in.base_len = LADX_ROM_SIZE;
	in.base_sha1 = gen->base_sha1;
	in.tables = &work->tables.tables;
	in.settings = &record.settings;
	in.seed = seed;
	in.seed_len = sizeof seed;
	in.groups = gen->groups;
	in.group_count = gen->group_count;
	gen->assembled = gen->assemble(&work->assembler, &in, work->image);
	if (gen->assembled != LADX_ASSEMBLE_OK)
		return assemble_status(&work->assembler, gen->assembled);
	if (!write_rom(gen->rom_path, work->image))
		return RANDO_LADX_GEN_ROM_WRITE;
	record.has_spoiler = request->spoiler && work->assembler.spoiler.built;
	if ((record.has_spoiler && !write_spoiler(gen, &work->assembler)) || !write_record(gen->record_path, &record)) {
		remove(gen->record_path);
		remove(gen->spoiler_path);
		remove(gen->rom_path);
		return RANDO_LADX_GEN_RECORD;
	}
	*spoiler = record.has_spoiler;
	return RANDO_LADX_GEN_OK;
}

void rando_ladx_gen_run(RandoLadxGen *gen, const RandoLadxGenRequest *request, UiGenOutcome *outcome)
{
	RandoLadxWork *work = gen->alloc(sizeof *work);
	RandoAppText text;
	bool spoiler = false;

	memset(outcome, 0, sizeof *outcome);
	memset(&gen->name, 0, sizeof gen->name);
	gen->assembled = LADX_ASSEMBLE_OK;
	if (work == NULL) {
		gen->status = RANDO_LADX_GEN_MEMORY;
	} else {
		gen->status = generate(gen, work, request, &spoiler);
		gen->release(work);
	}
	text = rando_ladx_gen_text(gen->status);
	outcome->ok = gen->status == RANDO_LADX_GEN_OK;
	outcome->status = text.status;
	outcome->advice = text.advice;
	outcome->status_arg = UI_STR_NONE;
	if (!outcome->ok)
		return;
	snprintf(outcome->rom, sizeof outcome->rom, "%s", gen->name.rom);
	outcome->spoiler = spoiler;
	outcome->list_stale = rando_sd_remove_caches(gen->root, RANDO_SD_LADX_LIST_CACHE6, RANDO_SD_LADX_LIST_CACHE2);
}
