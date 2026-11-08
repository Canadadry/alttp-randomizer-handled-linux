#include "ladx_sha1.h"
#include "ladx_sprite_data.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_sprite_data"
#define ROOMS 0x316

/*
 * LADXR's SPRITE_DATA at the pin rendered over rooms 0..0x315 and entities
 * 0..255 ("%03x %02x none" or "%03x %02x" then " <slot>:<values, sorted, %02x>"
 * per pair, one line each, absent keys skipped), SHA-1 of the text, by
 * python3 over entityData.SPRITE_DATA with a stub room, then again after
 * SPRITE_DATA[0x77] = SPRITE_DATA[0x4D].
 */
static const char *const digest_plain = "5ec78cf102f3c760b8dac53795ecd9533262d481";
static const char *const digest_evilshop = "fd785a7980158cc342a37f947ce2dd0ccadcecb1";

static void render_digest(int evilshop, char hex[2 * LADX_SHA1_SIZE + 1], size_t *bytes)
{
	LadxSha1 ctx;
	LadxSpriteData d;
	uint8_t digest[LADX_SHA1_SIZE];
	char line[96];
	int room, e, i, v, n;
	LadxSpriteKind kind;

	*bytes = 0;
	ladx_sha1_init(&ctx);
	for (room = 0; room < ROOMS; room++)
		for (e = 0; e < 256; e++) {
			kind = ladx_sprite_data(e, room, evilshop, &d);
			if (kind == LADX_SPRITE_ABSENT)
				continue;
			n = sprintf(line, "%03x %02x", room, e);
			if (kind == LADX_SPRITE_NONE) {
				n += sprintf(line + n, " none");
			} else {
				for (i = 0; i < d.count; i++) {
					int lo = d.pair[i].values[0], hi = d.pair[i].count > 1 ? d.pair[i].values[1] : -1;

					if (hi >= 0 && hi < lo) {
						v = lo;
						lo = hi;
						hi = v;
					}
					n += sprintf(line + n, " %d:%02x", d.pair[i].slot, lo);
					if (hi >= 0)
						n += sprintf(line + n, ",%02x", hi);
				}
			}
			line[n++] = '\n';
			ladx_sha1_update(&ctx, (const uint8_t *)line, (size_t)n);
			*bytes += (size_t)n;
		}
	ladx_sha1_final(&ctx, digest);
	for (i = 0; i < LADX_SHA1_SIZE; i++)
		sprintf(hex + 2 * i, "%02x", digest[i]);
}

static void test_digests(void)
{
	char hex[2 * LADX_SHA1_SIZE + 1];
	size_t bytes;

	render_digest(0, hex, &bytes);
	if (strcmp(hex, digest_plain) != 0 || bytes != 2230210)
		test_fail(TEST, "SPRITE_DATA renders %s (%lu bytes), LADXR's %s (2230210)", hex, (unsigned long)bytes,
			  digest_plain);
	render_digest(1, hex, &bytes);
	if (strcmp(hex, digest_evilshop) != 0 || bytes != 2234160)
		test_fail(TEST, "SPRITE_DATA with the evil shop renders %s (%lu bytes), LADXR's %s (2234160)", hex,
			  (unsigned long)bytes, digest_evilshop);
}

static void test_spot_checks(void)
{
	LadxSpriteData d;

	if (ladx_sprite_data(0x01, 0x10, 0, &d) != LADX_SPRITE_ABSENT)
		test_fail(TEST, "entity 0x01 is not a SPRITE_DATA key");
	if (ladx_sprite_data(0x1B, 0x10, 0, &d) != LADX_SPRITE_NONE)
		test_fail(TEST, "GEL's data is None");
	if (ladx_sprite_data(0xFA, 0x10, 0, &d) != LADX_SPRITE_NONE)
		test_fail(TEST, "the photographer outside room 0x2F5 is None");
	if (ladx_sprite_data(0x0B, 0x02, 0, &d) != LADX_SPRITE_DATA || d.count != 1 || d.pair[0].slot != 2 ||
	    d.pair[0].values[0] != 0x9C)
		test_fail(TEST, "a moblin on Tal Tal heights takes slot 2 0x9C");
	if (ladx_sprite_data(0x0B, 0x03, 0, &d) != LADX_SPRITE_DATA || d.pair[0].values[0] != 0x7C)
		test_fail(TEST, "a moblin left of x 4 takes slot 2 0x7C");
	if (ladx_sprite_data(0x77, 0x10, 1, &d) != LADX_SPRITE_DATA || d.count != 2 || d.pair[0].values[0] != 0x88)
		test_fail(TEST, "with the evil shop 0x77 is the shop owner's");
	if (ladx_sprite_data(0xA0, 0x10, 0, &d) != LADX_SPRITE_DATA || d.pair[0].count != 2)
		test_fail(TEST, "a peahat accepts two values in slot 0");
}

int main(void)
{
	test_spot_checks();
	test_digests();
	printf("%s: SPRITE_DATA equal to LADXR's over every room and entity, with and without the evil shop\n", TEST);
	return 0;
}
