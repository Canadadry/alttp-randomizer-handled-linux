#include "ladx_pyrandom.h"
#include "support.h"

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST "test_pyrandom"
#define VECTORS "oracle/pyrandom_vectors.txt"
#define MAX_LINE 8192
#define MAX_SEED 1024
#define MAX_ITEMS 256

static const char *line_now;

static void fail(const char *what)
{
	test_fail(TEST, "%s: %s", what, line_now);
}

typedef struct Fields {
	char *at[MAX_ITEMS];
	int count;
} Fields;

static void split(char *text, char sep, Fields *f)
{
	char *p = text;

	f->count = 0;
	for (;;) {
		char *next = strchr(p, sep);

		if (f->count == MAX_ITEMS)
			fail("too many fields");
		f->at[f->count++] = p;
		if (!next)
			break;
		*next = '\0';
		p = next + 1;
	}
}

static int64_t parse_i64(const char *s)
{
	char *end;
	long long v;

	errno = 0;
	v = strtoll(s, &end, 10);
	if (end == s || *end || errno)
		fail("malformed integer");
	return (int64_t)v;
}

static uint64_t parse_u64(const char *s)
{
	char *end;
	unsigned long long v;

	errno = 0;
	if (*s == '-')
		fail("malformed unsigned integer");
	v = strtoull(s, &end, 10);
	if (end == s || *end || errno)
		fail("malformed unsigned integer");
	return (uint64_t)v;
}

static double parse_double(const char *s)
{
	char *end;
	double v = strtod(s, &end);

	if (end == s || *end)
		fail("malformed float");
	return v;
}

static int same_bits(double a, double b)
{
	return memcmp(&a, &b, sizeof a) == 0;
}

static int hex_digit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	fail("malformed seed");
	return 0;
}

static void seed_from_hex(LadxPyRandom *rng, const char *hex)
{
	uint8_t seed[MAX_SEED];
	size_t n = 0, len;

	if (strcmp(hex, "-") == 0) {
		ladx_pyrandom_seed(rng, NULL, 0);
		return;
	}
	len = strlen(hex);
	if (len == 0 || len % 2 || len / 2 > MAX_SEED)
		fail("malformed seed");
	for (n = 0; n < len / 2; n++)
		seed[n] = (uint8_t)(hex_digit(hex[2 * n]) << 4 | hex_digit(hex[2 * n + 1]));
	ladx_pyrandom_seed(rng, seed, n);
}

static void expect_ok(LadxPyRandomStatus status)
{
	if (status != LADX_PYRANDOM_OK)
		fail("refused call");
}

static void need_fields(const Fields *f, int n)
{
	if (f->count != n)
		fail("wrong field count");
}

static void check_words(const Fields *f)
{
	LadxPyRandom rng;
	int i;

	need_fields(f, 18);
	seed_from_hex(&rng, f->at[1]);
	for (i = 2; i < 18; i++) {
		uint64_t got;

		expect_ok(ladx_pyrandom_getrandbits(&rng, 32, &got));
		if (got != parse_u64(f->at[i]))
			fail("word");
	}
}

static void check_random(const Fields *f)
{
	LadxPyRandom rng;
	int i;

	need_fields(f, 10);
	seed_from_hex(&rng, f->at[1]);
	for (i = 2; i < 10; i++)
		if (!same_bits(ladx_pyrandom_random(&rng), parse_double(f->at[i])))
			fail("random");
}

static void check_getrandbits(const Fields *f)
{
	LadxPyRandom rng;
	int64_t k;
	int i;

	need_fields(f, 7);
	seed_from_hex(&rng, f->at[1]);
	k = parse_i64(f->at[2]);
	if (k < 0 || k > 64)
		fail("k out of range");
	for (i = 3; i < 7; i++) {
		uint64_t got;

		expect_ok(ladx_pyrandom_getrandbits(&rng, (int)k, &got));
		if (got != parse_u64(f->at[i]))
			fail("getrandbits");
	}
}

typedef LadxPyRandomStatus (*RangeFn)(LadxPyRandom *rng, int64_t a, int64_t b, int64_t *out);

static void check_range(const Fields *f, RangeFn fn)
{
	LadxPyRandom rng;
	int64_t a, b;
	int i;

	need_fields(f, 12);
	seed_from_hex(&rng, f->at[1]);
	a = parse_i64(f->at[2]);
	b = parse_i64(f->at[3]);
	for (i = 4; i < 12; i++) {
		int64_t got;

		expect_ok(fn(&rng, a, b, &got));
		if (got != parse_i64(f->at[i]))
			fail("range draw");
	}
}

static void check_randrange(const Fields *f)
{
	check_range(f, ladx_pyrandom_randrange);
}

static void check_randint(const Fields *f)
{
	check_range(f, ladx_pyrandom_randint);
}

static void check_choice(const Fields *f)
{
	LadxPyRandom rng;
	uint64_t n;
	int i;

	need_fields(f, 11);
	seed_from_hex(&rng, f->at[1]);
	n = parse_u64(f->at[2]);
	for (i = 3; i < 11; i++) {
		size_t got;

		expect_ok(ladx_pyrandom_choice(&rng, (size_t)n, &got));
		if (got != parse_u64(f->at[i]))
			fail("choice");
	}
}

static size_t parse_index_list(char *text, uint64_t *out, size_t max)
{
	Fields items;
	int i;

	if (strcmp(text, "-") == 0)
		return 0;
	split(text, ',', &items);
	if ((size_t)items.count > max)
		fail("list too long");
	for (i = 0; i < items.count; i++)
		out[i] = parse_u64(items.at[i]);
	return (size_t)items.count;
}

static void expect_shuffle(LadxPyRandom *rng, uint64_t n, char *expected)
{
	uint32_t list[MAX_ITEMS];
	uint64_t want[MAX_ITEMS];
	size_t i;

	if (n > MAX_ITEMS)
		fail("shuffle too long");
	for (i = 0; i < n; i++)
		list[i] = (uint32_t)i;
	ladx_pyrandom_shuffle(rng, list, (size_t)n, sizeof list[0]);
	if (parse_index_list(expected, want, MAX_ITEMS) != n)
		fail("shuffle length");
	for (i = 0; i < n; i++)
		if (list[i] != want[i])
			fail("shuffle");
}

static void check_shuffle(const Fields *f)
{
	LadxPyRandom rng;

	need_fields(f, 4);
	seed_from_hex(&rng, f->at[1]);
	expect_shuffle(&rng, parse_u64(f->at[2]), f->at[3]);
}

static size_t parse_weights(char *text, double *out)
{
	Fields items;
	int i;

	split(text, ',', &items);
	for (i = 0; i < items.count; i++)
		out[i] = parse_double(items.at[i]);
	return (size_t)items.count;
}

static void expect_choices(LadxPyRandom *rng, char *weights_text, uint64_t k, char *expected)
{
	double weights[MAX_ITEMS];
	size_t got[MAX_ITEMS];
	uint64_t want[MAX_ITEMS];
	size_t n = parse_weights(weights_text, weights);
	size_t i;

	if (k > MAX_ITEMS)
		fail("k too large");
	expect_ok(ladx_pyrandom_choices(rng, weights, n, (size_t)k, got));
	if (parse_index_list(expected, want, MAX_ITEMS) != k)
		fail("choices length");
	for (i = 0; i < k; i++)
		if (got[i] != want[i])
			fail("choices");
}

static void check_choices(const Fields *f)
{
	LadxPyRandom rng;

	need_fields(f, 4);
	seed_from_hex(&rng, f->at[1]);
	expect_choices(&rng, f->at[2], 8, f->at[3]);
}

static void check_uniform(const Fields *f)
{
	LadxPyRandom rng;
	double a, b;
	int i;

	need_fields(f, 12);
	seed_from_hex(&rng, f->at[1]);
	a = parse_double(f->at[2]);
	b = parse_double(f->at[3]);
	for (i = 4; i < 12; i++)
		if (!same_bits(ladx_pyrandom_uniform(&rng, a, b), parse_double(f->at[i])))
			fail("uniform");
}

static const char *const ops[] = {"random", "getrandbits", "randrange", "randint", "choice",
				  "shuffle", "choices", "uniform", "getstate", "setstate"};
#define OPS (sizeof ops / sizeof ops[0])

static int op_counts[OPS];

static void expect_none(const char *result)
{
	if (strcmp(result, "-") != 0)
		fail("script result of getstate or setstate");
}

static void run_op(LadxPyRandom *rng, LadxPyRandom *saved, int *have_saved, char *op, char *result)
{
	Fields a;
	size_t o;

	split(op, ':', &a);
	for (o = 0; o < OPS; o++)
		if (strcmp(a.at[0], ops[o]) == 0)
			break;
	if (o == OPS)
		fail("unknown script op");
	op_counts[o]++;
	if (strcmp(a.at[0], "random") == 0 && a.count == 1) {
		if (!same_bits(ladx_pyrandom_random(rng), parse_double(result)))
			fail("script random");
	} else if (strcmp(a.at[0], "getrandbits") == 0 && a.count == 2) {
		int64_t k = parse_i64(a.at[1]);
		uint64_t got;

		if (k < 0 || k > 64)
			fail("k out of range");
		expect_ok(ladx_pyrandom_getrandbits(rng, (int)k, &got));
		if (got != parse_u64(result))
			fail("script getrandbits");
	} else if ((strcmp(a.at[0], "randrange") == 0 || strcmp(a.at[0], "randint") == 0) && a.count == 3) {
		RangeFn fn = strcmp(a.at[0], "randrange") == 0 ? ladx_pyrandom_randrange : ladx_pyrandom_randint;
		int64_t got;

		expect_ok(fn(rng, parse_i64(a.at[1]), parse_i64(a.at[2]), &got));
		if (got != parse_i64(result))
			fail("script range draw");
	} else if (strcmp(a.at[0], "choice") == 0 && a.count == 2) {
		size_t got;

		expect_ok(ladx_pyrandom_choice(rng, (size_t)parse_u64(a.at[1]), &got));
		if (got != parse_u64(result))
			fail("script choice");
	} else if (strcmp(a.at[0], "shuffle") == 0 && a.count == 2) {
		expect_shuffle(rng, parse_u64(a.at[1]), result);
	} else if (strcmp(a.at[0], "choices") == 0 && a.count == 3) {
		expect_choices(rng, a.at[1], parse_u64(a.at[2]), result);
	} else if (strcmp(a.at[0], "uniform") == 0 && a.count == 3) {
		double got = ladx_pyrandom_uniform(rng, parse_double(a.at[1]), parse_double(a.at[2]));

		if (!same_bits(got, parse_double(result)))
			fail("script uniform");
	} else if (strcmp(a.at[0], "getstate") == 0 && a.count == 1) {
		expect_none(result);
		*saved = *rng;
		*have_saved = 1;
	} else if (strcmp(a.at[0], "setstate") == 0 && a.count == 1) {
		expect_none(result);
		if (!*have_saved)
			fail("setstate before getstate");
		*rng = *saved;
	} else {
		fail("malformed script op");
	}
}

static void check_script(const Fields *f)
{
	static Fields op_list, results;
	LadxPyRandom rng, saved;
	int have_saved = 0;
	int i;

	need_fields(f, 6);
	if (strcmp(f->at[3], "->") != 0)
		fail("script without ->");
	seed_from_hex(&rng, f->at[1]);
	split(f->at[2], ';', &op_list);
	split(f->at[4], ';', &results);
	if (op_list.count != results.count)
		fail("script ops and results differ in count");
	for (i = 0; i < op_list.count; i++)
		run_op(&rng, &saved, &have_saved, op_list.at[i], results.at[i]);
	if (rng.words != parse_u64(f->at[5]))
		fail("script final word position");
}

static const char *const kinds[] = {"words", "random", "getrandbits", "randrange", "randint",
				    "choice", "shuffle", "choices", "uniform", "script"};
#define KINDS (sizeof kinds / sizeof kinds[0])

typedef void (*Checker)(const Fields *f);

static const Checker checkers[KINDS] = {check_words, check_random, check_getrandbits, check_randrange,
					 check_randint, check_choice, check_shuffle,
					 check_choices, check_uniform, check_script};

static void replay_fixture(void)
{
	static char line[MAX_LINE];
	static char copy[MAX_LINE];
	int counts[KINDS] = {0};
	int total = 0;
	size_t k;
	FILE *file = fopen(VECTORS, "r");

	if (!file)
		test_fail(TEST, "cannot open %s", VECTORS);
	while (fgets(line, sizeof line, file)) {
		Fields f;
		size_t len = strlen(line);

		if (len == 0 || line[len - 1] != '\n')
			test_fail(TEST, "line too long or unterminated in %s", VECTORS);
		line[len - 1] = '\0';
		if (line[0] == '#')
			continue;
		memcpy(copy, line, len);
		line_now = copy;
		split(line, ' ', &f);
		for (k = 0; k < KINDS; k++)
			if (strcmp(f.at[0], kinds[k]) == 0)
				break;
		if (k == KINDS)
			fail("unknown kind");
		if (!checkers[k])
			fail("kind not replayed");
		checkers[k](&f);
		counts[k]++;
		total++;
	}
	fclose(file);
	if (total == 0)
		test_fail(TEST, "no line in %s", VECTORS);
	for (k = 0; k < KINDS; k++)
		if (counts[k] == 0)
			test_fail(TEST, "no %s line in %s", kinds[k], VECTORS);
	for (k = 0; k < OPS; k++)
		if (op_counts[k] == 0)
			test_fail(TEST, "no script op %s in %s", ops[k], VECTORS);
	printf("%s: %d fixture lines ok (", TEST, total);
	for (k = 0; k < KINDS; k++)
		printf("%s%s %d", k ? ", " : "", kinds[k], counts[k]);
	printf(")\n");
}

static void seeded(LadxPyRandom *rng)
{
	static const uint8_t seed[] = {0x4c, 0x41, 0x44, 0x58};

	ladx_pyrandom_seed(rng, seed, sizeof seed);
}

static void expect_refused(const char *what, LadxPyRandomStatus got, LadxPyRandomStatus want,
			   const LadxPyRandom *rng, const LadxPyRandom *before)
{
	if (got != want)
		test_fail(TEST, "%s: status %d, expected %d", what, (int)got, (int)want);
	if (memcmp(rng, before, sizeof *rng) != 0)
		test_fail(TEST, "%s: refused call changed the generator", what);
}

static void test_refused_calls(void)
{
	static const double one[] = {1.0};
	static const double zeros[] = {0.0, 0.0};
	static const double negative[] = {2.0, -1.0};
	static const double huge[] = {1e308, 1e308};
	LadxPyRandom rng, before;
	uint64_t bits = 7;
	int64_t value = 7;
	size_t index = 7, picks[2] = {7, 7};

	seeded(&rng);
	ladx_pyrandom_random(&rng);
	memcpy(&before, &rng, sizeof rng);
	expect_refused("randrange(5, 5)", ladx_pyrandom_randrange(&rng, 5, 5, &value),
		       LADX_PYRANDOM_VALUE_ERROR, &rng, &before);
	expect_refused("randrange(5, 4)", ladx_pyrandom_randrange(&rng, 5, 4, &value),
		       LADX_PYRANDOM_VALUE_ERROR, &rng, &before);
	expect_refused("randint(1, 0)", ladx_pyrandom_randint(&rng, 1, 0, &value), LADX_PYRANDOM_VALUE_ERROR,
		       &rng, &before);
	expect_refused("randint(0, INT64_MAX)", ladx_pyrandom_randint(&rng, 0, INT64_MAX, &value),
		       LADX_PYRANDOM_UNSUPPORTED, &rng, &before);
	expect_refused("choice(0)", ladx_pyrandom_choice(&rng, 0, &index), LADX_PYRANDOM_INDEX_ERROR, &rng,
		       &before);
	expect_refused("choices n=0", ladx_pyrandom_choices(&rng, one, 0, 2, picks), LADX_PYRANDOM_INDEX_ERROR,
		       &rng, &before);
	expect_refused("choices zero total", ladx_pyrandom_choices(&rng, zeros, 2, 2, picks),
		       LADX_PYRANDOM_VALUE_ERROR, &rng, &before);
	expect_refused("choices infinite total", ladx_pyrandom_choices(&rng, huge, 2, 2, picks),
		       LADX_PYRANDOM_VALUE_ERROR, &rng, &before);
	expect_refused("choices negative weight", ladx_pyrandom_choices(&rng, negative, 2, 2, picks),
		       LADX_PYRANDOM_UNSUPPORTED, &rng, &before);
	expect_refused("choices NULL weights", ladx_pyrandom_choices(&rng, NULL, 2, 2, picks),
		       LADX_PYRANDOM_UNSUPPORTED, &rng, &before);
	expect_refused("getrandbits(-1)", ladx_pyrandom_getrandbits(&rng, -1, &bits), LADX_PYRANDOM_VALUE_ERROR,
		       &rng, &before);
	expect_refused("getrandbits(65)", ladx_pyrandom_getrandbits(&rng, 65, &bits), LADX_PYRANDOM_UNSUPPORTED,
		       &rng, &before);
	if (bits != 7 || value != 7 || index != 7 || picks[0] != 7 || picks[1] != 7)
		test_fail(TEST, "a refused call wrote its out-parameter");
}

static void expect_words(const char *what, const LadxPyRandom *rng, uint64_t want)
{
	if (rng->words != want)
		test_fail(TEST, "%s: words %" PRIu64 ", expected %" PRIu64, what, rng->words, want);
}

static void test_word_counts(void)
{
	LadxPyRandom rng;
	uint64_t bits, want = 0;
	int k, i;

	seeded(&rng);
	expect_words("seeded", &rng, 0);
	ladx_pyrandom_random(&rng);
	expect_words("random", &rng, want += 2);
	for (k = 0; k <= 64; k++) {
		expect_ok(ladx_pyrandom_getrandbits(&rng, k, &bits));
		want += (uint64_t)(k + 31) / 32;
		expect_words("getrandbits", &rng, want);
	}
	for (i = 0; i < 700; i++)
		ladx_pyrandom_random(&rng);
	expect_words("past regeneration", &rng, want += 1400);
}

static void test_state_copy(void)
{
	LadxPyRandom rng, copy, twin;
	int i;

	seeded(&rng);
	for (i = 0; i < 300; i++)
		ladx_pyrandom_random(&rng);
	copy = rng;
	seeded(&twin);
	for (i = 0; i < 1000; i++) {
		double a = ladx_pyrandom_random(&rng);

		if (!same_bits(a, ladx_pyrandom_random(&copy)))
			test_fail(TEST, "the copied state draws differently at draw %d", i);
	}
	if (copy.words != rng.words)
		test_fail(TEST, "the copied state lost its word count");
	expect_words("an untouched generator seeded alike", &twin, 0);
}

/* The fixture's uniform lines have a = 0, where an FMA rounds alike; 4 of these 8 differ when fused. */
static void test_uniform_unfused(void)
{
	/* python3 -c "import random; r = random.Random(bytes.fromhex('4c414458'));
	   print([r.uniform(-7.3, 12.9).hex() for _ in range(8)])" */
	static const double want[] = {-0x1.c9dfa21f5af24p+2, -0x1.68e47c092d00cp+0, 0x1.88a0790c13ddfp+2,
				      0x1.883657b9e3f6dp+2,  -0x1.d2065738fe869p+1, 0x1.5bc130697e556p+3,
				      0x1.a04c3ff05bc67p+2,  0x1.4f7e378803cb1p+2};
	LadxPyRandom rng;
	int i;

	seeded(&rng);
	for (i = 0; i < 8; i++)
		if (!same_bits(ladx_pyrandom_uniform(&rng, -7.3, 12.9), want[i]))
			test_fail(TEST, "uniform(-7.3, 12.9) draw %d differs from CPython (fused?)", i);
}

int main(void)
{
	line_now = "(no fixture line)";
	test_refused_calls();
	test_word_counts();
	test_state_copy();
	test_uniform_unfused();
	replay_fixture();
	return 0;
}
