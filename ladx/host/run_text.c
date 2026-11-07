#include "ladx_run_text.h"

#include <float.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

/* CPython's float_hex (Objects/floatobject.c), for IEEE doubles. */
void ladx_float_hex(double x, char out[LADX_FLOAT_HEX_SIZE])
{
	static const char digits[] = "0123456789abcdef";
	char mantissa[16];
	double m;
	int e, shift, i, at = 0;

	if (isnan(x)) {
		strcpy(out, "nan");
		return;
	}
	if (isinf(x)) {
		strcpy(out, x < 0 ? "-inf" : "inf");
		return;
	}
	if (x == 0.0) {
		strcpy(out, signbit(x) ? "-0x0.0p+0" : "0x0.0p+0");
		return;
	}
	m = frexp(fabs(x), &e);
	shift = 1 - (DBL_MIN_EXP - e > 0 ? DBL_MIN_EXP - e : 0);
	m = ldexp(m, shift);
	e -= shift;
	mantissa[at++] = digits[(int)m];
	m -= (int)m;
	mantissa[at++] = '.';
	for (i = 0; i < 13; i++) {
		m *= 16.0;
		mantissa[at++] = digits[(int)m];
		m -= (int)m;
	}
	mantissa[at] = '\0';
	sprintf(out, "%s0x%sp%c%d", x < 0 ? "-" : "", mantissa, e < 0 ? '-' : '+', e < 0 ? -e : e);
}

typedef struct Text {
	char *buf;
	size_t cap, len;
	int over;
} Text;

static void put(Text *t, const char *fmt, ...)
{
	va_list ap;
	int n;

	if (t->over)
		return;
	va_start(ap, fmt);
	n = vsnprintf(t->buf + t->len, t->cap - t->len, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= t->cap - t->len)
		t->over = 1;
	else
		t->len += (size_t)n;
}

static size_t finish(Text *t)
{
	if (!t->over)
		return t->len;
	if (t->cap > 0)
		t->buf[0] = '\0';
	return 0;
}

static void open_text(Text *t, char *buf, size_t cap)
{
	t->buf = buf;
	t->cap = cap;
	t->len = 0;
	t->over = cap == 0;
	if (cap > 0)
		buf[0] = '\0';
}

static void put_list(Text *t, const size_t *list, size_t n)
{
	size_t i;

	if (n == 0)
		put(t, "-");
	for (i = 0; i < n; i++)
		put(t, i ? ",%lu" : "%lu", (unsigned long)list[i]);
}

static void put_hex(Text *t, const uint8_t *bytes, size_t size)
{
	size_t i;

	if (size == 0)
		put(t, "-");
	for (i = 0; i < size; i++)
		put(t, "%02x", bytes[i]);
}

static const char *field(const char *text)
{
	return *text ? text : "\"\"";
}

size_t ladx_run_text_trace_line(char *buf, size_t cap, unsigned long n, const LadxRngRecord *r)
{
	const char *gen = ladx_rng_gen_name(r->gen);
	const char *stage = ladx_rng_stage_name(r->stage);
	const char *method = ladx_rng_method_name(r->method);
	char real[LADX_FLOAT_HEX_SIZE];
	Text t;
	size_t i;

	open_text(&t, buf, cap);
	if (!gen || !stage || !method)
		t.over = 1;
	put(&t, "trace %lu %s %s %s ", n, gen, stage, method);
	switch (r->method) {
	case LADX_RNG_SEED:
		put_hex(&t, r->seed, r->seed_len);
		put(&t, " -> -");
		break;
	case LADX_RNG_RANDOM:
		ladx_float_hex(r->real, real);
		put(&t, "- -> %s", real);
		break;
	case LADX_RNG_CHOICE:
		put(&t, "%lu -> %lu", (unsigned long)r->n, (unsigned long)r->index);
		break;
	case LADX_RNG_CHOICES:
		put(&t, "%lu ", (unsigned long)r->n);
		for (i = 0; i < r->n; i++)
			put(&t, i ? ",%d" : "%d", r->weights[i]);
		put(&t, " %lu -> ", (unsigned long)r->k);
		put_list(&t, r->list, r->k);
		break;
	case LADX_RNG_SHUFFLE:
		put(&t, "%lu -> ", (unsigned long)r->n);
		put_list(&t, r->list, r->n);
		break;
	case LADX_RNG_RANDINT:
	case LADX_RNG_RANDRANGE:
		put(&t, "%lld %lld -> %lld", (long long)r->a, (long long)r->b, (long long)r->value);
		break;
	case LADX_RNG_GETSTATE:
	case LADX_RNG_SETSTATE:
		put(&t, "- -> -");
		break;
	default:
		t.over = 1;
	}
	put(&t, " [%llu]", (unsigned long long)r->words);
	return finish(&t);
}

size_t ladx_run_text_seed_line(char *buf, size_t cap, const uint8_t *seed, size_t size)
{
	Text t;

	open_text(&t, buf, cap);
	put(&t, "seed ");
	put_hex(&t, seed, size);
	return finish(&t);
}

size_t ladx_run_text_settings_line(char *buf, size_t cap, const char *short_string)
{
	Text t;

	open_text(&t, buf, cap);
	put(&t, "settings %s", field(short_string));
	return finish(&t);
}

size_t ladx_run_text_outcome_line(char *buf, size_t cap, const LadxOutcome *outcome)
{
	Text t;

	open_text(&t, buf, cap);
	switch (outcome->kind) {
	case LADX_OUTCOME_OK:
		put(&t, "outcome ok");
		break;
	case LADX_OUTCOME_ERROR:
		put(&t, "outcome error %s", field(outcome->message));
		break;
	case LADX_OUTCOME_CRASH:
		put(&t, "outcome crash %s %s", outcome->type, field(outcome->message));
		break;
	default:
		t.over = 1;
	}
	return finish(&t);
}

size_t ladx_run_text_words_line(char *buf, size_t cap, const char *tag, uint64_t words)
{
	Text t;

	open_text(&t, buf, cap);
	put(&t, "%s %llu", tag, (unsigned long long)words);
	return finish(&t);
}

size_t ladx_run_text_pool_line(char *buf, size_t cap, const char *tag, const LadxItemPool *pool)
{
	int i, item, count;
	Text t;

	open_text(&t, buf, cap);
	put(&t, "%s", tag);
	for (i = 0; ladx_itempool_entry(pool, i, &item, &count); i++) {
		if (!ladx_item_name(item))
			t.over = 1;
		put(&t, " %s=%d", ladx_item_name(item), count);
	}
	return finish(&t);
}

size_t ladx_run_text_ws_line(char *buf, size_t cap, const LadxWorldSetup *ws, int i)
{
	Text t;
	int n;

	open_text(&t, buf, cap);
	switch (i) {
	case 0:
		put(&t, "ws entrance_mapping identity");
		break;
	case 1:
		put(&t, "ws boss_mapping ");
		for (n = 0; n < 9; n++)
			put(&t, n ? ",%d" : "%d", ws->boss_mapping[n]);
		break;
	case 2:
		put(&t, "ws miniboss_mapping ");
		for (n = 0; n < LADX_MINIBOSS_KEY_COUNT; n++) {
			const char *name = ladx_miniboss_name(ws->miniboss_mapping[n]);

			if (!name)
				t.over = 1;
			put(&t, n ? ",%s=%s" : "%s=%s", ladx_miniboss_key(n), name);
		}
		break;
	case 3:
		if (ws->goal == LADX_GOAL_SPECIFIC)
			put(&t, "ws goal =%s", ws->goal_instruments);
		else if (ladx_goal_name(ws->goal))
			put(&t, "ws goal %s", ladx_goal_name(ws->goal));
		else
			t.over = 1;
		break;
	case 4:
		put(&t, "ws goal_count %d", ws->goal_count);
		break;
	case 5:
		put(&t, "ws bingo_goals -");
		break;
	case 6:
		put(&t, "ws sign_maze ");
		if (ws->sign_count == 0)
			put(&t, "-");
		for (n = 0; n < ws->sign_count; n++)
			put(&t, n ? ",%d:%d" : "%d:%d", ws->signs[n].x, ws->signs[n].y);
		for (n = 0; n + 1 < ws->sign_count; n++)
			put(&t, n ? ",%d" : ";%d", ws->directions[n]);
		break;
	case 7:
		if (ladx_item_name(ws->multichest))
			put(&t, "ws multichest %s", ladx_item_name(ws->multichest));
		else
			t.over = 1;
		break;
	case 8:
		put(&t, "ws map -");
		break;
	case 9:
		put(&t, "ws dungeon_chain -");
		break;
	case 10:
		put(&t, "ws inside_to_outside %d", ws->inside_to_outside ? 1 : 0);
		break;
	case 11:
		put(&t, "ws keep_two_way %d", ws->keep_two_way ? 1 : 0);
		break;
	case 12:
		put(&t, "ws one_on_one %d", ws->one_on_one ? 1 : 0);
		break;
	default:
		t.over = 1;
	}
	return finish(&t);
}

void ladx_run_text_trace_init(LadxRunTextTrace *t, FILE *out)
{
	t->out = out;
	t->count = 0;
	t->failed = 0;
}

void ladx_run_text_trace_sink(void *ctx, const LadxRngRecord *record)
{
	LadxRunTextTrace *t = ctx;
	char line[LADX_RUN_TEXT_MAX_LINE];

	if (t->failed)
		return;
	if (ladx_run_text_trace_line(line, sizeof line, t->count, record) == 0 || fputs(line, t->out) == EOF ||
	    fputc('\n', t->out) == EOF) {
		t->failed = 1;
		return;
	}
	t->count++;
}

size_t ladx_run_text_place_line(char *buf, size_t cap, const LadxLogic *logic, int i)
{
	const LadxSpot *spot = ladx_logic_spot(logic, i);
	Text t;

	open_text(&t, buf, cap);
	if (!spot || !ladx_spot_kind_name(spot->kind)) {
		t.over = 1;
		return finish(&t);
	}
	put(&t, "place %d %s %s %s", i, ladx_spot_kind_name(spot->kind), field(spot->name_id),
	    spot->item == LADX_ITEM_NONE ? "-" : ladx_item_name(spot->item));
	if (spot->item != LADX_ITEM_NONE && !ladx_item_name(spot->item))
		t.over = 1;
	return finish(&t);
}

static int put_line(FILE *out, const char *line, size_t len)
{
	return len > 0 && fputs(line, out) != EOF && fputc('\n', out) != EOF;
}

int ladx_run_text_write_run(FILE *out, const LadxSettings *settings, const uint8_t *seed, size_t seed_len,
			    const LadxRandomizer *r, FILE *trace, const LadxOutcome *raised)
{
	char line[LADX_RUN_TEXT_MAX_LINE];
	char short_string[LADX_SETTINGS_SHORT_MAX + 1];
	const LadxOutcome *outcome = ladx_randomizer_outcome(r);
	const LadxWorldSetup *ws = ladx_randomizer_world_setup(r);
	const LadxItemPool *pool, *read = ladx_randomizer_read_pool(r);
	const LadxLogic *placement = ladx_randomizer_placement(r);
	uint64_t before, after;
	size_t got;
	int i;

	if (ladx_settings_short_string(settings, short_string, sizeof short_string) < 0)
		return 0;
	if (!put_line(out, line, ladx_run_text_seed_line(line, sizeof line, seed, seed_len)) ||
	    !put_line(out, line, ladx_run_text_settings_line(line, sizeof line, short_string)))
		return 0;
	if (ws)
		for (i = 0; i < LADX_RUN_TEXT_WS_LINES; i++)
			if (!put_line(out, line, ladx_run_text_ws_line(line, sizeof line, ws, i)))
				return 0;
	pool = ladx_randomizer_pool(r, &before, &after);
	if (pool && (!put_line(out, line, ladx_run_text_words_line(line, sizeof line, "words_before", before)) ||
		     !put_line(out, line, ladx_run_text_pool_line(line, sizeof line, "pool", pool)) ||
		     !put_line(out, line, ladx_run_text_words_line(line, sizeof line, "words_after", after))))
		return 0;
	if (read && !put_line(out, line, ladx_run_text_pool_line(line, sizeof line, "placer_pool", read)))
		return 0;
	if (placement)
		for (i = 0; i < ladx_logic_spot_count(placement); i++)
			if (!put_line(out, line, ladx_run_text_place_line(line, sizeof line, placement, i)))
				return 0;
	rewind(trace);
	while ((got = fread(line, 1, sizeof line, trace)) > 0)
		if (fwrite(line, 1, got, out) != got)
			return 0;
	if (ferror(trace))
		return 0;
	if (raised)
		return put_line(out, line, ladx_run_text_outcome_line(line, sizeof line, raised));
	if (!placement && outcome->kind != LADX_OUTCOME_UNFINISHED &&
	    !put_line(out, line, ladx_run_text_outcome_line(line, sizeof line, outcome)))
		return 0;
	return 1;
}
