#include "synthetic_base.h"

#include "rando_crc32.h"
#include "rando_md5.h"
#include "support.h"

#include <string.h>

#define TEST "synthetic_base"
#define SOURCE_SIZE SYNTH_SOURCE_SIZE
#define ROM_SIZE SYNTH_ROM_SIZE
#define PATCH_MAX SYNTH_PATCH_MAX
#define OPS_MAX 32
#define VERSION_ADDRESS 0x7FE2u

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

typedef enum OpKind { SOURCE_READ = 0, TARGET_READ = 1, TARGET_COPY = 3 } OpKind;

typedef struct Op {
	OpKind kind;
	size_t length;
	const uint8_t *bytes; /* TARGET_READ */
	size_t from;          /* TARGET_COPY: absolute target offset */
} Op;

/* One synthetic base build: the ops, in target order, from the 1 MiB source to the 2 MiB image. */
typedef struct Recipe {
	Op ops[OPS_MAX];
	int nops;
	size_t at;
} Recipe;

uint8_t synth_source[SOURCE_SIZE];
uint8_t synth_base[ROM_SIZE];
uint8_t synth_sprite_base[ROM_SIZE];
SynthPatch synth_good_patch, synth_sprite_patch;
uint8_t synth_source_md5[RANDO_MD5_SIZE], synth_base_md5[RANDO_MD5_SIZE], synth_sprite_base_md5[RANDO_MD5_SIZE];
static uint8_t gfx_bytes[0x7000];
static uint8_t palette_bytes[120];
static uint8_t gloves_bytes[4];

static const uint8_t version_byte[1] = { 0x05 };
static const uint8_t guard[2] = { 0x02, 0x37 };
static const uint8_t filler[16] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
				    0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xF0, 0x0F };

static void add(Recipe *r, OpKind kind, size_t length, const uint8_t *bytes, size_t from)
{
	CHECK(r->nops < OPS_MAX && length > 0);
	r->ops[r->nops].kind = kind;
	r->ops[r->nops].length = length;
	r->ops[r->nops].bytes = bytes;
	r->ops[r->nops].from = from;
	r->nops++;
	r->at += length;
}

static void source_read_to(Recipe *r, size_t end)
{
	CHECK(end > r->at);
	add(r, SOURCE_READ, end - r->at, NULL, 0);
}

static void copy_filler_to(Recipe *r, size_t end)
{
	CHECK(end > r->at);
	add(r, TARGET_COPY, end - r->at, NULL, SOURCE_SIZE);
}

/* The source with the version byte (and, for the sprite variant, distinct sprite bytes), then a
 * repeated filler with the two `02 37` author guards. */
static Recipe recipe(int sprite)
{
	Recipe r;
	r.nops = 0;
	r.at = 0;
	source_read_to(&r, VERSION_ADDRESS);
	add(&r, TARGET_READ, 1, version_byte, 0);
	if (sprite) {
		source_read_to(&r, 0x80000u);
		add(&r, TARGET_READ, sizeof gfx_bytes, gfx_bytes, 0);
		source_read_to(&r, 0xDD308u);
		add(&r, TARGET_READ, sizeof palette_bytes, palette_bytes, 0);
		source_read_to(&r, 0xDEDF5u);
		add(&r, TARGET_READ, sizeof gloves_bytes, gloves_bytes, 0);
	}
	source_read_to(&r, SOURCE_SIZE);
	add(&r, TARGET_READ, sizeof filler, filler, 0);
	copy_filler_to(&r, 0x118000u);
	add(&r, TARGET_READ, 2, guard, 0);
	copy_filler_to(&r, 0x11801Eu);
	add(&r, TARGET_READ, 2, guard, 0);
	copy_filler_to(&r, ROM_SIZE);
	return r;
}

static void build_image(const Recipe *r, const uint8_t *src, uint8_t *out)
{
	size_t at = 0, i;
	int k;
	for (k = 0; k < r->nops; k++) {
		const Op *op = &r->ops[k];
		for (i = 0; i < op->length; i++) {
			if (op->kind == SOURCE_READ)
				out[at + i] = src[at + i];
			else if (op->kind == TARGET_READ)
				out[at + i] = op->bytes[i];
			else
				out[at + i] = out[op->from + i];
		}
		at += op->length;
	}
	CHECK(at == ROM_SIZE);
}

static void put_byte(SynthPatch *p, uint8_t byte)
{
	CHECK(p->size < PATCH_MAX);
	p->bytes[p->size++] = byte;
}

static void put_u32(SynthPatch *p, uint32_t v)
{
	put_byte(p, (uint8_t)v);
	put_byte(p, (uint8_t)(v >> 8));
	put_byte(p, (uint8_t)(v >> 16));
	put_byte(p, (uint8_t)(v >> 24));
}

static void put_number(SynthPatch *p, uint64_t v)
{
	for (;;) {
		uint8_t x = (uint8_t)(v & 0x7F);
		v >>= 7;
		if (v == 0) {
			put_byte(p, (uint8_t)(0x80 | x));
			return;
		}
		put_byte(p, x);
		v--;
	}
}

static void encode(SynthPatch *p, const Recipe *r, size_t source_size, size_t target_size, const uint8_t *image)
{
	size_t target_offset = 0, i;
	int k;
	p->size = 0;
	put_byte(p, 'B');
	put_byte(p, 'P');
	put_byte(p, 'S');
	put_byte(p, '1');
	put_number(p, source_size);
	put_number(p, target_size);
	put_number(p, 0);
	for (k = 0; k < r->nops; k++) {
		const Op *op = &r->ops[k];
		put_number(p, (uint64_t)(op->length - 1) << 2 | (uint64_t)op->kind);
		if (op->kind == TARGET_READ) {
			for (i = 0; i < op->length; i++)
				put_byte(p, op->bytes[i]);
		} else if (op->kind == TARGET_COPY) {
			if (op->from >= target_offset)
				put_number(p, (uint64_t)(op->from - target_offset) << 1);
			else
				put_number(p, (uint64_t)(target_offset - op->from) << 1 | 1u);
			target_offset = op->from + op->length;
		}
	}
	put_u32(p, rando_crc32(synth_source, SOURCE_SIZE));
	put_u32(p, rando_crc32(image, ROM_SIZE));
	put_u32(p, rando_crc32(p->bytes, p->size));
}

/* A valid BPS header and footer with no action. */
void synth_header_only(SynthPatch *p, size_t source_size, size_t target_size)
{
	p->size = 0;
	put_byte(p, 'B');
	put_byte(p, 'P');
	put_byte(p, 'S');
	put_byte(p, '1');
	put_number(p, source_size);
	put_number(p, target_size);
	put_number(p, 0);
	put_u32(p, 0);
	put_u32(p, 0);
	put_u32(p, rando_crc32(p->bytes, p->size));
}

void synth_build(void)
{
	Recipe r;
	size_t i;
	for (i = 0; i < SOURCE_SIZE; i++)
		synth_source[i] = (uint8_t)((i * 31 + (i >> 9)) & 0xFF);
	for (i = 0; i < sizeof gfx_bytes; i++)
		gfx_bytes[i] = (uint8_t)~synth_source[0x80000u + i];
	for (i = 0; i < sizeof palette_bytes; i++)
		palette_bytes[i] = (uint8_t)~synth_source[0xDD308u + i];
	for (i = 0; i < sizeof gloves_bytes; i++)
		gloves_bytes[i] = (uint8_t)~synth_source[0xDEDF5u + i];
	rando_md5(synth_source, SOURCE_SIZE, synth_source_md5);

	r = recipe(0);
	build_image(&r, synth_source, synth_base);
	encode(&synth_good_patch, &r, SOURCE_SIZE, ROM_SIZE, synth_base);
	rando_md5(synth_base, ROM_SIZE, synth_base_md5);
	r = recipe(1);
	build_image(&r, synth_source, synth_sprite_base);
	encode(&synth_sprite_patch, &r, SOURCE_SIZE, ROM_SIZE, synth_sprite_base);
	rando_md5(synth_sprite_base, ROM_SIZE, synth_sprite_base_md5);
}
