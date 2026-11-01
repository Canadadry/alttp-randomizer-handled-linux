#define _POSIX_C_SOURCE 200809L

#include "rando_app_gen.h"
#include "rando_last_settings.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static const RandoAppText gen_texts[RANDO_APP_GEN_STATUS_COUNT] = {
	[RANDO_APP_GEN_OK] = {UI_STR_GEN_OK, UI_STR_ADVICE_NONE},
	[RANDO_APP_GEN_BASE_MISSING] = {UI_STR_GEN_BASE_MISSING, UI_STR_ADVICE_BASE},
	[RANDO_APP_GEN_BASE_READ] = {UI_STR_GEN_BASE_READ, UI_STR_ADVICE_BASE},
	[RANDO_APP_GEN_PATCH_MISSING] = {UI_STR_GEN_PATCH_MISSING, UI_STR_ADVICE_REINSTALL},
	[RANDO_APP_GEN_PATCH_READ] = {UI_STR_GEN_PATCH_READ, UI_STR_ADVICE_REINSTALL},
	[RANDO_APP_GEN_PATCH_TOO_LARGE] = {UI_STR_GEN_PATCH_TOO_LARGE, UI_STR_ADVICE_REINSTALL},
	[RANDO_APP_GEN_FOLDERS] = {UI_STR_GEN_FOLDERS, UI_STR_ADVICE_SD_CARD},
	[RANDO_APP_GEN_SAVE] = {UI_STR_GEN_SAVE, UI_STR_ADVICE_SD_CARD},
};

static const RandoAppText save_texts[RANDO_SAVE_STATUS_COUNT] = {
	[RANDO_SAVE_OK] = {UI_STR_SAVE_OK, UI_STR_ADVICE_NONE},
	[RANDO_SAVE_NO_NAME] = {UI_STR_SAVE_NO_NAME, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_SAVE_ASSEMBLE] = {UI_STR_SAVE_ASSEMBLE, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_SAVE_RECORD] = {UI_STR_SAVE_RECORD, UI_STR_ADVICE_SD_CARD},
};

static const RandoAppText assemble_texts[RANDO_ASSEMBLE_STATUS_COUNT] = {
	[RANDO_ASSEMBLE_OK] = {UI_STR_ASSEMBLE_OK, UI_STR_ADVICE_NONE},
	[RANDO_ASSEMBLE_SOURCE_SIZE] = {UI_STR_ASSEMBLE_SOURCE_SIZE, UI_STR_ADVICE_BASE},
	[RANDO_ASSEMBLE_SOURCE_MD5] = {UI_STR_ASSEMBLE_SOURCE_MD5, UI_STR_ADVICE_BASE},
	[RANDO_ASSEMBLE_PATCH] = {UI_STR_ASSEMBLE_PATCH, UI_STR_ADVICE_REINSTALL},
	[RANDO_ASSEMBLE_PATCH_SIZES] = {UI_STR_ASSEMBLE_PATCH_SIZES, UI_STR_ADVICE_REINSTALL},
	[RANDO_ASSEMBLE_BASE_MD5] = {UI_STR_ASSEMBLE_BASE_MD5, UI_STR_ADVICE_REINSTALL},
	[RANDO_ASSEMBLE_UNSUPPORTED] = {UI_STR_ASSEMBLE_UNSUPPORTED, UI_STR_ADVICE_CHANGE_SETTINGS},
	[RANDO_ASSEMBLE_GAVE_UP] = {UI_STR_ASSEMBLE_GAVE_UP, UI_STR_ADVICE_GENERATE_AGAIN},
	[RANDO_ASSEMBLE_OUTPUT] = {UI_STR_ASSEMBLE_OUTPUT, UI_STR_ADVICE_SD_CARD},
};

static const RandoAppText bps_texts[RANDO_BPS_RESULT_COUNT] = {
	[RANDO_BPS_OK] = {UI_STR_BPS_OK, UI_STR_NONE},
	[RANDO_BPS_NOT_BPS] = {UI_STR_BPS_NOT_BPS, UI_STR_NONE},
	[RANDO_BPS_MALFORMED] = {UI_STR_BPS_MALFORMED, UI_STR_NONE},
	[RANDO_BPS_PATCH_CRC] = {UI_STR_BPS_PATCH_CRC, UI_STR_NONE},
	[RANDO_BPS_SOURCE_SIZE] = {UI_STR_BPS_SOURCE_SIZE, UI_STR_NONE},
	[RANDO_BPS_SOURCE_CRC] = {UI_STR_BPS_SOURCE_CRC, UI_STR_NONE},
	[RANDO_BPS_TARGET_TOO_SMALL] = {UI_STR_BPS_TARGET_TOO_SMALL, UI_STR_NONE},
	[RANDO_BPS_TARGET_CRC] = {UI_STR_BPS_TARGET_CRC, UI_STR_NONE},
};

RandoAppText rando_app_gen_text(RandoAppGenStatus status)
{
	assert((size_t)status < RANDO_APP_GEN_STATUS_COUNT);
	return gen_texts[status];
}

RandoAppText rando_app_save_text(RandoSaveStatus status)
{
	assert((size_t)status < RANDO_SAVE_STATUS_COUNT);
	return save_texts[status];
}

RandoAppText rando_app_assemble_text(RandoAssembleStatus status)
{
	assert((size_t)status < RANDO_ASSEMBLE_STATUS_COUNT);
	return assemble_texts[status];
}

RandoAppText rando_app_bps_text(RandoBpsResult result)
{
	assert((size_t)result < RANDO_BPS_RESULT_COUNT);
	return bps_texts[result];
}

static bool drop_component(const char *path, size_t *end)
{
	size_t n = *end;

	while (n > 0 && path[n - 1] == '/')
		n--;
	if (n == 0)
		return false;
	while (n > 0 && path[n - 1] != '/')
		n--;
	while (n > 0 && path[n - 1] == '/')
		n--;
	*end = n;
	return true;
}

bool rando_app_sd_root(const char *app_dir, char *out, size_t cap)
{
	const char *root = RANDO_SD_DEFAULT_ROOT;
	size_t len = strlen(root), end;

	if (app_dir != NULL && (end = strlen(app_dir)) > 0 && drop_component(app_dir, &end) &&
	    drop_component(app_dir, &end)) {
		root = end > 0 ? app_dir : app_dir[0] == '/' ? "/" : ".";
		len = end > 0 ? end : 1;
	}
	if (len + 1 > cap)
		return false;
	memcpy(out, root, len);
	out[len] = '\0';
	return true;
}

static bool join(char *out, size_t cap, const char *root, const char *rel)
{
	int n = snprintf(out, cap, "%s/%s", root, rel);

	return n >= 0 && (size_t)n < cap;
}

static void local_date(RandoRecordDate *out)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	if (tm == NULL) {
		memset(out, 0, sizeof *out);
		return;
	}
	out->year = tm->tm_year + 1900;
	out->month = tm->tm_mon + 1;
	out->day = tm->tm_mday;
	out->hour = tm->tm_hour;
	out->minute = tm->tm_min;
	out->second = tm->tm_sec;
}

bool rando_app_gen_init(RandoAppGen *gen, const char *sd_root)
{
	size_t len = strlen(sd_root);

	if (len >= sizeof gen->root)
		return false;
	memcpy(gen->root, sd_root, len + 1);
	if (!join(gen->rom_dir, sizeof gen->rom_dir, sd_root, RANDO_SD_ROM_DIR) ||
	    !join(gen->history_dir, sizeof gen->history_dir, sd_root, RANDO_SD_HISTORY_DIR) ||
	    !join(gen->settings, sizeof gen->settings, sd_root, RANDO_SD_SETTINGS_FILE) ||
	    !join(gen->base_rom, sizeof gen->base_rom, sd_root, RANDO_SD_BASE_ROM_FILE) ||
	    !join(gen->base_patch, sizeof gen->base_patch, sd_root, RANDO_SD_BASE_PATCH_FILE))
		return false;
	memcpy(gen->source_md5, rando_assemble_jp10_md5, RANDO_MD5_SIZE);
	memcpy(gen->base_md5, rando_assemble_base_md5, RANDO_MD5_SIZE);
	gen->seed = rando_out_random_seed;
	gen->date = local_date;
	gen->save = rando_seed_save;
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
	static const char *const dirs[] = {"Roms", "Roms/SFC", RANDO_SD_ROM_DIR, "App", "App/ALttPR", RANDO_SD_HISTORY_DIR};
	size_t i;

	for (i = 0; i < sizeof dirs / sizeof dirs[0]; i++)
		if (!make_dir(root, dirs[i]))
			return false;
	return true;
}

static RandoAppGenStatus read_inputs(RandoAppGen *gen, size_t *source_size, size_t *patch_size)
{
	long total;

	switch (rando_out_read_capped(gen->base_rom, gen->source, RANDO_ROM_SOURCE_SIZE, source_size, &total)) {
	case RANDO_OUT_READ_OK:
	case RANDO_OUT_READ_TOO_LARGE:
		break;
	case RANDO_OUT_READ_MISSING:
		return RANDO_APP_GEN_BASE_MISSING;
	case RANDO_OUT_READ_OPEN:
	case RANDO_OUT_READ_READ:
		return RANDO_APP_GEN_BASE_READ;
	}
	switch (rando_out_read_capped(gen->base_patch, gen->patch, RANDO_OUT_PATCH_MAX, patch_size, &total)) {
	case RANDO_OUT_READ_OK:
		break;
	case RANDO_OUT_READ_TOO_LARGE:
		return RANDO_APP_GEN_PATCH_TOO_LARGE;
	case RANDO_OUT_READ_MISSING:
		return RANDO_APP_GEN_PATCH_MISSING;
	case RANDO_OUT_READ_OPEN:
	case RANDO_OUT_READ_READ:
		return RANDO_APP_GEN_PATCH_READ;
	}
	return RANDO_APP_GEN_OK;
}

static void set_failure(UiGenOutcome *outcome, RandoAppText text)
{
	outcome->ok = false;
	outcome->status = text.status;
	outcome->advice = text.advice;
	outcome->status_arg = UI_STR_NONE;
}

static void fail_save(UiGenOutcome *outcome, RandoSaveStatus status, const RandoSaveResult *result)
{
	if (status != RANDO_SAVE_ASSEMBLE) {
		set_failure(outcome, rando_app_save_text(status));
		return;
	}
	set_failure(outcome, rando_app_assemble_text(result->assemble));
	if (result->assemble == RANDO_ASSEMBLE_PATCH)
		outcome->status_arg = rando_app_bps_text(result->bps).status;
}

void rando_app_gen_run(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome)
{
	RandoAppGen *gen = ctx;
	RandoAssembleRequest assemble;
	RandoRecordDate date;
	RandoAppGenStatus status;
	RandoSaveStatus saved;
	size_t source_size = 0, patch_size = 0;

	memset(outcome, 0, sizeof *outcome);
	outcome->status = outcome->status_arg = outcome->advice = UI_STR_NONE;
	(void)rando_last_settings_save(gen->settings, &request->options, request->heart_beep);
	status = read_inputs(gen, &source_size, &patch_size);
	if (status == RANDO_APP_GEN_OK && !make_folders(gen->root))
		status = RANDO_APP_GEN_FOLDERS;
	if (status != RANDO_APP_GEN_OK) {
		set_failure(outcome, rando_app_gen_text(status));
		return;
	}

	rando_assemble_request_init(&assemble);
	memcpy(assemble.source_md5, gen->source_md5, RANDO_MD5_SIZE);
	memcpy(assemble.base_md5, gen->base_md5, RANDO_MD5_SIZE);
	assemble.source = gen->source;
	assemble.source_size = source_size;
	assemble.patch = gen->patch;
	assemble.patch_size = patch_size;
	assemble.options = request->options;
	assemble.heart_beep = request->heart_beep;
	assemble.seed = gen->seed();
	gen->date(&date);
	saved = gen->save(gen->rom_dir, gen->history_dir, &assemble, &gen->work, &date, request->spoiler, &gen->result);
	if (saved != RANDO_SAVE_OK) {
		fail_save(outcome, saved, &gen->result);
		return;
	}
	outcome->ok = true;
	snprintf(outcome->rom, sizeof outcome->rom, "%s", gen->result.name.rom);
	outcome->spoiler = request->spoiler;
	outcome->list_stale = rando_sd_remove_list_caches(gen->root);
}
