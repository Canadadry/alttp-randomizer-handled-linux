#include "rando_patch.h"

#include "rando_case.h"
#include "rando_out.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#define PATCH_FILE_MAX (4u << 20)
#define KEY_MAX 64
/* The RandoOptionKey keys, then hints and heart_beep. */
#define SETTING_HINTS RANDO_OPTION_KEY_COUNT
#define SETTING_HEART_BEEP (RANDO_OPTION_KEY_COUNT + 1)
#define SETTING_COUNT (RANDO_OPTION_KEY_COUNT + 2)

enum {
	EXIT_OK = 0,
	EXIT_INPUT = 1,
	EXIT_UNSUPPORTED = 4,
	EXIT_GAVE_UP = 5,
};

static RandoAssembleWork work;
static uint8_t source[RANDO_ROM_SOURCE_SIZE + 1];
static uint8_t patch[PATCH_FILE_MAX + 1];

static bool refuse(char error[RANDO_PATCH_ERROR_MAX], const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(error, RANDO_PATCH_ERROR_MAX, fmt, ap);
	va_end(ap);
	return false;
}

/* -1 when the key is unknown. */
static int setting_find(const char *key)
{
	if (strcmp(key, "hints") == 0)
		return SETTING_HINTS;
	if (strcmp(key, "heart_beep") == 0)
		return SETTING_HEART_BEEP;
	if (rando_option_key_find(key) == RANDO_OPTION_KEY_COUNT)
		return -1;
	return (int)rando_option_key_find(key);
}

static bool set_setting(RandoPatchArgs *args, const char *arg, bool seen[SETTING_COUNT],
			char error[RANDO_PATCH_ERROR_MAX])
{
	const char *eq = strchr(arg, '=');
	const char *value = eq + 1;
	char key[KEY_MAX];
	size_t len = (size_t)(eq - arg);
	int setting;
	bool ok;

	if (len >= KEY_MAX)
		return refuse(error, "unknown key %.*s", (int)len, arg);
	memcpy(key, arg, len);
	key[len] = '\0';
	setting = setting_find(key);
	if (setting < 0)
		return refuse(error, "unknown key %s", key);
	if (seen[setting])
		return refuse(error, "%s given twice", key);
	seen[setting] = true;
	if (setting == SETTING_HINTS)
		ok = rando_hints_find(value, &args->request.options.hints);
	else if (setting == SETTING_HEART_BEEP)
		ok = rando_heart_beep_find(value, &args->request.heart_beep);
	else
		ok = rando_options_set(&args->request.options, key, value);
	if (!ok)
		return refuse(error, "unknown value %s for %s", value, key);
	return true;
}

bool rando_patch_parse_args(int argc, char **argv, RandoPatchArgs *args, char error[RANDO_PATCH_ERROR_MAX])
{
	bool seen[SETTING_COUNT] = { false };
	bool bps_given = false;
	int i;

	args->base_path = NULL;
	args->out_path = NULL;
	args->bps_path = RANDO_PATCH_DEFAULT_BPS;
	args->has_seed = false;
	args->seed = 0;
	rando_assemble_request_init(&args->request);
	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];
		const char **path = NULL;
		bool is_seed = strcmp(arg, "--seed") == 0;

		if (strcmp(arg, "--base") == 0)
			path = &args->base_path;
		else if (strcmp(arg, "--out") == 0)
			path = &args->out_path;
		else if (strcmp(arg, "--bps") == 0)
			path = &args->bps_path;
		if (path != NULL || is_seed) {
			bool twice = is_seed ? args->has_seed : path == &args->bps_path ? bps_given : *path != NULL;
			if (twice)
				return refuse(error, "%s given twice", arg);
			if (i + 1 >= argc)
				return refuse(error, "%s needs a value", arg);
			i++;
			if (is_seed) {
				char seed_error[RANDO_CASE_ERROR_MAX];
				if (!rando_case_parse_uint32(argv[i], strlen(argv[i]), "--seed", &args->seed,
							     seed_error))
					return refuse(error, "%s", seed_error);
				args->has_seed = true;
			} else {
				*path = argv[i];
				if (path == &args->bps_path)
					bps_given = true;
			}
		} else if (strchr(arg, '=') != NULL) {
			if (!set_setting(args, arg, seen, error))
				return false;
		} else {
			return refuse(error, "unexpected argument %s", arg);
		}
	}
	if (args->base_path == NULL)
		return refuse(error, "--base is required");
	if (args->out_path == NULL)
		return refuse(error, "--out is required");
	return true;
}

static const char *bps_name(RandoBpsResult result)
{
	static const char *const names[] = {
		"OK", "NOT_BPS", "MALFORMED", "PATCH_CRC", "SOURCE_SIZE", "SOURCE_CRC", "TARGET_TOO_SMALL", "TARGET_CRC",
	};
	return (size_t)result < sizeof names / sizeof names[0] ? names[result] : "unknown";
}

/* Reads at most max bytes into buf; *size is max + 1 when the file is larger, the true size then in
 * *total when it can be told (else -1). */
static bool read_capped(const char *path, uint8_t *buf, size_t max, size_t *size, long *total, FILE *err)
{
	FILE *f = fopen(path, "rb");
	bool failed;
	if (f == NULL) {
		fprintf(err, "rando_patch: %s: cannot open: %s\n", path, strerror(errno));
		return false;
	}
	*size = fread(buf, 1, max + 1, f);
	failed = ferror(f) != 0;
	*total = (long)*size;
	if (!failed && *size > max)
		*total = fseek(f, 0, SEEK_END) == 0 ? ftell(f) : -1;
	fclose(f);
	if (failed) {
		fprintf(err, "rando_patch: %s: read error\n", path);
		return false;
	}
	return true;
}

static uint32_t draw_seed(void)
{
	uint8_t bytes[4];
	FILE *f = fopen("/dev/urandom", "rb");
	uint32_t seed;
	if (f != NULL) {
		size_t n = fread(bytes, 1, sizeof bytes, f);
		fclose(f);
		if (n == sizeof bytes)
			return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
			       (uint32_t)bytes[3] << 24;
	}
	seed = (uint32_t)time(NULL) * 2654435761u ^ (uint32_t)clock();
	seed ^= seed >> 16;
	seed *= 0x85EBCA6Bu;
	seed ^= seed >> 13;
	return seed;
}

static void source_size_message(FILE *err, const char *path, long size)
{
	if (size < 0)
		fprintf(err, "rando_patch: %s: larger than %u bytes", path, RANDO_ROM_SOURCE_SIZE);
	else
		fprintf(err, "rando_patch: %s: %ld bytes", path, size);
	fprintf(err, "; the base must be the headerless %u-byte JP 1.0 ROM\n", RANDO_ROM_SOURCE_SIZE);
}

int rando_patch_run(const RandoPatchArgs *args, FILE *out, FILE *err)
{
	RandoAssembleRequest request = args->request;
	RandoBpsResult bps = RANDO_BPS_OK;
	size_t source_size, patch_size;
	long total;

	if (!read_capped(args->base_path, source, RANDO_ROM_SOURCE_SIZE, &source_size, &total, err))
		return EXIT_INPUT;
	if (source_size != RANDO_ROM_SOURCE_SIZE) {
		source_size_message(err, args->base_path, total);
		return EXIT_INPUT;
	}
	if (!read_capped(args->bps_path, patch, PATCH_FILE_MAX, &patch_size, &total, err))
		return EXIT_INPUT;
	if (patch_size > PATCH_FILE_MAX) {
		fprintf(err, "rando_patch: %s: larger than %u bytes, not the base patch\n", args->bps_path,
			PATCH_FILE_MAX);
		return EXIT_INPUT;
	}

	request.source = source;
	request.source_size = source_size;
	request.patch = patch;
	request.patch_size = patch_size;
	request.seed = args->has_seed ? args->seed : draw_seed();
	request.output = rando_out_write_file;
	request.output_ctx = (void *)args->out_path;

	switch (rando_assemble(&request, &work, &bps)) {
	case RANDO_ASSEMBLE_OK:
		fprintf(out, "seed %lu\n%s\n", (unsigned long)request.seed, args->out_path);
		return EXIT_OK;
	case RANDO_ASSEMBLE_SOURCE_SIZE:
		source_size_message(err, args->base_path, (long)source_size);
		return EXIT_INPUT;
	case RANDO_ASSEMBLE_SOURCE_MD5:
		fprintf(err, "rando_patch: %s: MD5 mismatch, not the JP 1.0 ROM\n", args->base_path);
		return EXIT_INPUT;
	case RANDO_ASSEMBLE_PATCH:
		fprintf(err, "rando_patch: %s: patch refused (%s)\n", args->bps_path, bps_name(bps));
		return EXIT_INPUT;
	case RANDO_ASSEMBLE_PATCH_SIZES:
		fprintf(err, "rando_patch: %s: does not patch %u bytes into %u\n", args->bps_path,
			RANDO_ROM_SOURCE_SIZE, RANDO_ROM_SIZE);
		return EXIT_INPUT;
	case RANDO_ASSEMBLE_BASE_MD5:
		fprintf(err, "rando_patch: %s: MD5 mismatch, the patched ROM is not VT's base build\n",
			args->bps_path);
		return EXIT_INPUT;
	case RANDO_ASSEMBLE_UNSUPPORTED:
		fprintf(err, "rando_patch: these settings are not supported\n");
		return EXIT_UNSUPPORTED;
	case RANDO_ASSEMBLE_GAVE_UP:
		fprintf(err, "rando_patch: seed %lu: gave up after %lu attempts\n", (unsigned long)request.seed,
			(unsigned long)request.max_attempts);
		return EXIT_GAVE_UP;
	case RANDO_ASSEMBLE_OUTPUT:
		fprintf(err, "rando_patch: %s: write failed\n", args->out_path);
		return EXIT_INPUT;
	}
	return EXIT_INPUT;
}
