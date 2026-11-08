#define _POSIX_C_SOURCE 200809L

#include "rando_seed_save.h"
#include "rando_seed_files.h"
#include "rando_out.h"
#include "rando_playthrough.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static RandoPlaythrough playthrough;

typedef struct Dirs {
	const char *rom_dir, *history_dir;
} Dirs;

static void rom_path(const char *rom_dir, const RandoName *name, char out[RANDO_SEED_PATH_MAX])
{
	snprintf(out, RANDO_SEED_PATH_MAX, "%s/%s", rom_dir, name->rom);
}

static void record_path(const char *history_dir, const RandoName *name, char out[RANDO_SEED_PATH_MAX])
{
	snprintf(out, RANDO_SEED_PATH_MAX, "%s/%s.json", history_dir, name->code);
}

static bool exists(const char *path)
{
	FILE *f = fopen(path, "rb");

	if (f == NULL)
		return false;
	fclose(f);
	return true;
}

static bool taken(void *ctx, const RandoName *candidate)
{
	const Dirs *dirs = ctx;
	char path[RANDO_SEED_PATH_MAX];

	rom_path(dirs->rom_dir, candidate, path);
	if (exists(path))
		return true;
	record_path(dirs->history_dir, candidate, path);
	return exists(path);
}

static bool write_record(const char *path, const RandoRecord *record, const RandoWorld *world)
{
	FILE *f = fopen(path, "wb");
	bool ok;

	if (f == NULL)
		return false;
	ok = rando_record_write(f, record, world, record->has_spoiler ? &playthrough : NULL);
	if (fclose(f) != 0)
		ok = false;
	if (!ok)
		remove(path);
	return ok;
}

RandoSaveStatus rando_seed_save(const char *rom_dir, const char *history_dir, const RandoAssembleRequest *request,
				RandoAssembleWork *work, const RandoRecordDate *date, bool spoiler,
				RandoSaveResult *result)
{
	Dirs dirs;
	RandoAssembleRequest assemble = *request;
	RandoRecord record;

	assert(strlen(rom_dir) < RANDO_SEED_DIR_MAX && strlen(history_dir) < RANDO_SEED_DIR_MAX);
	memset(result, 0, sizeof *result);
	dirs.rom_dir = rom_dir;
	dirs.history_dir = history_dir;
	if (!rando_name_choose(&request->options, request->heart_beep, request->seed, taken, &dirs, &result->name)) {
		rando_name_make(&request->options,
				rando_name_code(&request->options, request->heart_beep, request->seed), 1,
				&result->name);
		return RANDO_SAVE_NO_NAME;
	}
	rom_path(rom_dir, &result->name, result->rom_path);
	record_path(history_dir, &result->name, result->record_path);

	assemble.output = rando_out_write_file;
	assemble.output_ctx = result->rom_path;
	result->assemble = rando_assemble(&assemble, work, &result->bps);
	if (result->assemble != RANDO_ASSEMBLE_OK)
		return RANDO_SAVE_ASSEMBLE;

	memset(&record, 0, sizeof record);
	record.name = result->name;
	record.date = *date;
	record.seed = request->seed;
	record.options = request->options;
	record.heart_beep = request->heart_beep;
	record.has_spoiler = spoiler;
	if (spoiler)
		rando_playthrough(&work->gen.world, &playthrough);
	if (!write_record(result->record_path, &record, &work->gen.world)) {
		remove(result->rom_path);
		return RANDO_SAVE_RECORD;
	}
	return RANDO_SAVE_OK;
}

bool rando_sd_remove_list_caches(const char *sd_root)
{
	return rando_sd_remove_caches(sd_root, RANDO_SD_LIST_CACHE6, RANDO_SD_LIST_CACHE2);
}
