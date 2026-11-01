#include "rando.h"
#include "rando_out.h"
#include "rando_rom.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_rom"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef struct Recorder {
	int calls;
} Recorder;

static bool record(void *ctx, uint32_t address, const uint8_t *bytes, size_t count, const char *method)
{
	Recorder *r = ctx;

	(void)address;
	(void)bytes;
	(void)count;
	(void)method;
	r->calls++;
	return true;
}

static RandoGen gen, before;

/* The skeleton writes nothing: step 6 replaces this expectation with the items group's writes. */
static void check_skeleton_writes_nothing(void)
{
	RandoOptions options;
	Recorder rec = {0};
	RandoRomWriter writer = {record, &rec};

	rando_options_init(&options);
	CHECK(rando_generate(&gen, &options, 1, 0, NULL, NULL) == RANDO_COMPLETE);
	memcpy(&before, &gen, sizeof gen);
	CHECK(rando_rom_write(&gen, RANDO_HEART_BEEP_HALF, &writer));
	CHECK(rec.calls == 0);
	CHECK(memcmp(&before, &gen, sizeof gen) == 0);
}

/* Every byte of buf equals fill, except bytes[0..count) at address. */
static void check_buffer(const uint8_t *buf, size_t size, uint8_t fill, size_t address, const uint8_t *bytes,
			 size_t count)
{
	size_t i;

	for (i = 0; i < size; i++)
		CHECK(buf[i] == (i >= address && i < address + count ? bytes[i - address] : fill));
}

static void check_buffer_writer(void)
{
	static const uint8_t bytes[3] = {0xA1, 0xB2, 0xC3};
	uint8_t buf[8];
	RandoRomBuffer rom = {buf, sizeof buf};

	memset(buf, 0xEE, sizeof buf);
	CHECK(rando_rom_buffer_write(&rom, 2, bytes, 3, "writeItem"));
	check_buffer(buf, sizeof buf, 0xEE, 2, bytes, 3);

	memset(buf, 0xEE, sizeof buf);
	CHECK(rando_rom_buffer_write(&rom, 5, bytes, 3, "writeItem"));
	check_buffer(buf, sizeof buf, 0xEE, 5, bytes, 3);

	memset(buf, 0xEE, sizeof buf);
	CHECK(!rando_rom_buffer_write(&rom, 6, bytes, 3, "writeItem"));
	CHECK(!rando_rom_buffer_write(&rom, 8, bytes, 1, "writeItem"));
	CHECK(!rando_rom_buffer_write(&rom, 0xFFFFFFFFu, bytes, 3, "writeItem"));
	check_buffer(buf, sizeof buf, 0xEE, 0, bytes, 0);
}

#define LONG_WRITE (FIXTURE_LINE_MAX + 1)
#define LOG_MAX (2 * LONG_WRITE + 256)

static void check_log_writer(void)
{
	static const uint8_t one[1] = {0x0A}, three[3] = {0xab, 0x00, 0xFF};
	static uint8_t long_bytes[LONG_WRITE];
	static char expected[LOG_MAX], text[LOG_MAX];
	RandoRomLog log;
	size_t len, n, i;

	for (i = 0; i < LONG_WRITE; i++)
		long_bytes[i] = (uint8_t)(i * 7);
	len = (size_t)sprintf(expected, "00000A 0A setHeartBeepSpeed\n180032 AB00FF writeItem\n1C0000 ");
	for (i = 0; i < LONG_WRITE; i++)
		len += (size_t)sprintf(expected + len, "%02X", long_bytes[i]);
	len += (size_t)sprintf(expected + len, " writeText\n");

	log.file = tmpfile();
	log.failed = false;
	CHECK(log.file != NULL);
	CHECK(rando_rom_log_write(&log, 0x0A, one, 1, "setHeartBeepSpeed"));
	CHECK(rando_rom_log_write(&log, 0x180032, three, 3, "writeItem"));
	CHECK(rando_rom_log_write(&log, 0x1C0000, long_bytes, LONG_WRITE, "writeText"));
	CHECK(!rando_rom_log_write(&log, 0x180000, one, 0, "writeItem"));
	rewind(log.file);
	n = fread(text, 1, LOG_MAX, log.file);
	CHECK(n == len && memcmp(text, expected, len) == 0);
	fclose(log.file);
}

int main(int argc, char **argv)
{
	test_fixtures_mode(TEST, argc, argv);
	check_skeleton_writes_nothing();
	check_buffer_writer();
	check_log_writer();
	printf(TEST ": passed\n");
	return 0;
}
