#include "rando_record.h"
#include "rando_spoiler.h"

#include <assert.h>
#include <string.h>

/* Bytewise sorted: the record's settings keys. */
static const char *const settings_keys[] = {
	"accessibility", "crystals_ganon", "crystals_tower", "dungeon_items",  "glitches",
	"goal",		 "heart_beep",	   "hints",	     "item_functionality", "item_placement",
	"item_pool",	 "state",	   "weapons",
};

#define SETTINGS_COUNT (sizeof settings_keys / sizeof settings_keys[0])

static const char *settings_value(const RandoRecord *record, const char *key)
{
	RandoOptionKey option;

	if (strcmp(key, "hints") == 0)
		return rando_hints_name(record->options.hints);
	if (strcmp(key, "heart_beep") == 0)
		return rando_heart_beep_name(record->heart_beep);
	option = rando_option_key_find(key);
	assert(option < RANDO_OPTION_KEY_COUNT);
	return rando_option_value_name(option, rando_options_get(&record->options, option));
}

static void assert_date(const RandoRecordDate *d)
{
	assert(d->year >= 1970 && d->year <= 9999);
	assert(d->month >= 1 && d->month <= 12);
	assert(d->day >= 1 && d->day <= 31);
	assert(d->hour >= 0 && d->hour <= 23);
	assert(d->minute >= 0 && d->minute <= 59);
	assert(d->second >= 0 && d->second <= 59);
	(void)d;
}

bool rando_record_write(FILE *out, const RandoRecord *record, const RandoWorld *world,
			const RandoPlaythrough *playthrough)
{
	const RandoRecordDate *d = &record->date;
	size_t i;
	bool ok = true;

	assert_date(d);
	assert(!record->has_spoiler || (world != NULL && playthrough != NULL));
	if (fprintf(out,
		    "{\n"
		    "    \"code\": \"%s\",\n"
		    "    \"date\": \"%04d-%02d-%02dT%02d:%02d:%02d\",\n"
		    "    \"format\": %d,\n"
		    "    \"rom\": \"%s\",\n"
		    "    \"seed\": %lu,\n"
		    "    \"settings\": {\n",
		    record->name.code, d->year, d->month, d->day, d->hour, d->minute, d->second, RANDO_RECORD_FORMAT,
		    record->name.rom, (unsigned long)record->seed) < 0)
		ok = false;
	for (i = 0; i < SETTINGS_COUNT; i++)
		if (fprintf(out, "        \"%s\": \"%s\"%s\n", settings_keys[i], settings_value(record, settings_keys[i]),
			    i + 1 < SETTINGS_COUNT ? "," : "") < 0)
			ok = false;
	if (record->has_spoiler) {
		if (fputs("    },\n    \"spoiler\": ", out) == EOF)
			ok = false;
		if (!rando_spoiler_write_json(out, world, &record->options, playthrough))
			ok = false;
		if (fputs("}\n", out) == EOF)
			ok = false;
	} else if (fputs("    }\n}\n", out) == EOF)
		ok = false;
	return ok && !ferror(out);
}
