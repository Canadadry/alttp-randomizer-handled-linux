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
	put(&t, "settings %s", *short_string ? short_string : "\"\"");
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
