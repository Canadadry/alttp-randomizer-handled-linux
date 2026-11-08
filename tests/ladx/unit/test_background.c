#include "ladx_background.h"
#include "support.h"

#include <stdio.h>
#include <string.h>

#define TEST "test_background"
#define OUT_MAX 4096

static LadxBackground bg, other;
static int checks;

static void check(int ok, const char *what)
{
	checks++;
	if (!ok)
		test_fail(TEST, "%s", what);
}

static int tile_is(uint32_t addr, int value)
{
	if (value < 0)
		return !(bg.present[addr / 8] >> (addr % 8) & 1);
	return (bg.present[addr / 8] >> (addr % 8) & 1) && bg.tile[addr] == value;
}

static void check_encoded(const char *what, const uint8_t *want, size_t want_n)
{
	uint8_t out[OUT_MAX];
	size_t n = ladx_background_encode(&bg, out, sizeof out);

	checks++;
	if (n != want_n || memcmp(out, want, n) != 0) {
		size_t i;

		fprintf(stderr, "%s: got", what);
		for (i = 0; i < n; i++)
			fprintf(stderr, " %02x", out[i]);
		fprintf(stderr, "\n");
		test_fail(TEST, "%s: encoded bytes differ", what);
	}
}

static void test_decode(void)
{
	/* A literal run of 3, a repeated run of 4, a vertical literal run of 2, a vertical repeat of 3. */
	static const uint8_t data[] = {0x98, 0x00, 0x02, 0x11, 0x12, 0x13, 0x98, 0x10, 0x43, 0x7f,
				       0x99, 0x00, 0x81, 0x21, 0x22, 0x9a, 0x05, 0xc2, 0x44, 0x00};

	check(ladx_background_decode(&bg, data, sizeof data), "a well-formed background decodes");
	check(bg.count == 12, "12 tiles");
	check(tile_is(0x9800, 0x11) && tile_is(0x9801, 0x12) && tile_is(0x9802, 0x13) && tile_is(0x9803, -1),
	      "the literal run");
	check(tile_is(0x9810, 0x7f) && tile_is(0x9813, 0x7f) && tile_is(0x9814, -1), "the repeated run of 4");
	check(tile_is(0x9900, 0x21) && tile_is(0x9920, 0x22) && tile_is(0x9901, -1), "the vertical literal run");
	check(tile_is(0x9a05, 0x44) && tile_is(0x9a25, 0x44) && tile_is(0x9a45, 0x44) && tile_is(0x9a65, -1),
	      "the vertical repeat");
	check(bg.low == 0x9800 && bg.high == 0x9a45, "the address bounds");

	check(!ladx_background_decode(&bg, data, sizeof data - 1), "no terminator: refused");
	check(!ladx_background_decode(&bg, data, 4), "a run cut short: refused");
	check(ladx_background_decode(&bg, data + sizeof data - 1, 1) && bg.count == 0, "the terminator alone: empty");

	{
		static const uint8_t over[] = {0xff, 0xff, 0x83, 1, 2, 3, 4, 0x00};

		check(ladx_background_decode(&bg, over, sizeof over) && tile_is(0x1005f, 4),
		      "a vertical run past 0xFFFF keeps its addresses");
	}
}

static void fill(uint32_t addr, const uint8_t *values, size_t n)
{
	size_t i;

	for (i = 0; i < n; i++)
		ladx_background_set(&bg, addr + (uint32_t)i, values[i]);
}

static void test_encode(void)
{
	static const uint8_t empty[] = {0x00};
	uint8_t values[200], out[OUT_MAX];
	size_t i, n;

	ladx_background_decode(&bg, empty, 1);
	check(ladx_background_encode(&bg, out, sizeof out) == 0, "an empty background does not encode");

	{
		static const uint8_t v[] = {1, 2, 3};
		static const uint8_t want[] = {0x98, 0x00, 0x02, 1, 2, 3, 0x00};

		fill(0x9800, v, 3);
		check_encoded("a literal run of 3", want, sizeof want);
	}
	{
		/* same 3 > different 3 - 4 and > 2: a repeat. */
		static const uint8_t v[] = {7, 7, 7};
		static const uint8_t want[] = {0x98, 0x00, 0x42, 7, 0x00};

		ladx_background_decode(&bg, empty, 1);
		fill(0x9800, v, 3);
		check_encoded("three equal tiles repeat", want, sizeof want);
	}
	{
		/* same 2 is not > 2: literal. */
		static const uint8_t v[] = {7, 7};
		static const uint8_t want[] = {0x98, 0x00, 0x01, 7, 7, 0x00};

		ladx_background_decode(&bg, empty, 1);
		fill(0x9800, v, 2);
		check_encoded("two equal tiles stay literal", want, sizeof want);
	}
	{
		/* 3 equal then 5 others: same 3 > 8 - 4 fails, one literal run of 8. */
		static const uint8_t v[] = {5, 5, 5, 1, 2, 3, 4, 6};
		static const uint8_t want[] = {0x98, 0x00, 0x07, 5, 5, 5, 1, 2, 3, 4, 6, 0x00};

		ladx_background_decode(&bg, empty, 1);
		fill(0x9800, v, 8);
		check_encoded("a short repeat inside a longer run stays literal", want, sizeof want);
	}
	{
		/* 4 equal then 3 others: same 4 > 7 - 4, a repeat then a literal run of 3. */
		static const uint8_t v[] = {5, 5, 5, 5, 1, 2, 3};
		static const uint8_t want[] = {0x98, 0x00, 0x43, 5, 0x98, 0x04, 0x02, 1, 2, 3, 0x00};

		ladx_background_decode(&bg, empty, 1);
		fill(0x9800, v, 7);
		check_encoded("a repeat then the rest", want, sizeof want);
	}
	{
		/* A gap splits runs; a run starting at 0x98ff. */
		static const uint8_t want[] = {0x98, 0xfe, 0x01, 9, 8, 0x99, 0x10, 0x00, 6, 0x00};

		ladx_background_decode(&bg, empty, 1);
		ladx_background_set(&bg, 0x98fe, 9);
		ladx_background_set(&bg, 0x98ff, 8);
		ladx_background_set(&bg, 0x9910, 6);
		check_encoded("a gap splits runs", want, sizeof want);
	}

	/* 0x40 caps both counts: 70 distinct tiles give runs of 64 and 6; 70 equal give repeats of 64 and 6. */
	ladx_background_decode(&bg, empty, 1);
	for (i = 0; i < 70; i++)
		values[i] = (uint8_t)i;
	fill(0x9800, values, 70);
	n = ladx_background_encode(&bg, out, sizeof out);
	check(n == 3 + 64 + 3 + 6 + 1 && out[2] == 0x3f && out[67] == 0x98 && out[68] == 0x40 && out[69] == 0x05,
	      "a literal run is capped at 0x40");
	ladx_background_decode(&bg, empty, 1);
	memset(values, 3, 70);
	fill(0x9800, values, 70);
	{
		static const uint8_t want[] = {0x98, 0x00, 0x7f, 3, 0x98, 0x40, 0x45, 3, 0x00};

		check_encoded("a repeat is capped at 0x40", want, sizeof want);
	}
	check(ladx_background_encode(&bg, out, 8) == 0 && ladx_background_encode(&bg, out, 9) == 9,
	      "the output cap counts the terminator");

	ladx_background_decode(&bg, empty, 1);
	check(ladx_background_set(&bg, 0x10000, 1) && ladx_background_encode(&bg, out, sizeof out) == 0,
	      "a run at 0x10000 does not encode");
	check(!ladx_background_set(&bg, LADX_BACKGROUND_ADDR_SPAN, 1), "an address past the span is refused");
}

static void test_round_trip(void)
{
	static const uint8_t data[] = {0x98, 0x00, 0x45, 0x7f, 0x98, 0x20, 0x03, 1, 2, 3, 4, 0x98, 0x41, 0x82, 9, 8, 7, 0x00};
	uint8_t out[OUT_MAX];
	size_t n;

	check(ladx_background_decode(&bg, data, sizeof data), "the round trip's background decodes");
	ladx_background_set(&bg, 0x9802, 0x55);
	n = ladx_background_encode(&bg, out, sizeof out);
	check(n > 0 && ladx_background_decode(&other, out, n), "its encoding decodes");
	check(other.count == bg.count && other.low == bg.low && other.high == bg.high &&
		      memcmp(other.present, bg.present, sizeof bg.present) == 0 && tile_is(0x9802, 0x55),
	      "the same addresses, the set tile overwritten");
	{
		uint32_t a;
		int same = 1;

		for (a = bg.low; a <= bg.high; a++)
			if ((bg.present[a / 8] >> (a % 8) & 1) && bg.tile[a] != other.tile[a])
				same = 0;
		check(same, "the same tiles");
	}
}

int main(void)
{
	test_decode();
	test_encode();
	test_round_trip();
	printf("%s: decode, the store encoder's choices and caps, round trip on synthetic backgrounds (%d checks)\n",
	       TEST, checks);
	return 0;
}
