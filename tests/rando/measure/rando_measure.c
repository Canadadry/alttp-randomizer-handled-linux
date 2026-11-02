#include "rando.h"
#include "rando_assemble.h"
#include "rando_case.h"
#include "synthetic_base.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STACK_SIZE (1u << 20)
#define PAINT 0xA5u
#define SELFCHECK_BYTES 16384u
#define SELFCHECK_SLACK 1024u
#define WATCHDOG 1000u
#define MAX_REPS 101

typedef struct Counts {
	uint32_t attempt;
	uint32_t total;
	uint32_t attempts_seen;
	uint32_t attempt_draws[WATCHDOG];
	uint32_t stage[RANDO_STAGE_COUNT];
} Counts;

typedef struct Job {
	void (*fn)(struct Job *job);
	const RandoCase *c;
	uint32_t cap;
	RandoStatus status;
	size_t depth;
} Job;

static RandoGen gen;
static RandoAssembleWork work;
static Counts counts;
static RandoStage until = RANDO_STAGE_NONE;
static const char *stage_names[RANDO_STAGE_COUNT];
static unsigned char stack[STACK_SIZE] __attribute__((aligned(4096)));
static size_t overhead;

static void count_draw(void *ctx, uint32_t attempt, uint32_t draw, const char *stage, int64_t min, int64_t max,
		       int64_t r)
{
	Counts *c = ctx;
	int s;

	(void)draw;
	(void)min;
	(void)max;
	(void)r;
	if (attempt != c->attempt) {
		c->attempt = attempt;
		for (s = 0; s < RANDO_STAGE_COUNT; s++)
			c->stage[s] = 0;
	}
	if (attempt < WATCHDOG) {
		c->attempt_draws[attempt]++;
		if (attempt + 1 > c->attempts_seen)
			c->attempts_seen = attempt + 1;
	}
	c->total++;
	for (s = 0; s < RANDO_STAGE_COUNT; s++)
		if (stage == stage_names[s]) {
			c->stage[s]++;
			break;
		}
}

static RandoStatus generate(const RandoCase *c, uint32_t seed, uint32_t cap)
{
	memset(&counts, 0, sizeof counts);
	if (until == RANDO_STAGE_NONE)
		return rando_generate(&gen, &c->options, seed, cap, count_draw, &counts);
	return rando_generate_until(&gen, &c->options, seed, cap, count_draw, &counts, until);
}

static void *thread_main(void *arg)
{
	Job *job = arg;

	job->fn(job);
	return NULL;
}

static size_t painted_depth(void)
{
	size_t i;

	for (i = 0; i < sizeof stack && stack[i] == PAINT; i++)
		;
	return sizeof stack - i;
}

/* The painted stack's peak in bytes, from the buffer's top to its deepest write; exits on a pthread
 * failure or when the peak reaches the buffer's low end. */
static size_t painted_run(Job *job)
{
	pthread_attr_t attr;
	pthread_t thread;
	size_t peak;

	memset(stack, PAINT, sizeof stack);
	if (pthread_attr_init(&attr) != 0 || pthread_attr_setstack(&attr, stack, sizeof stack) != 0
	    || pthread_create(&thread, &attr, thread_main, job) != 0 || pthread_join(thread, NULL) != 0) {
		fprintf(stderr, "rando_measure: cannot run the painted thread\n");
		exit(1);
	}
	pthread_attr_destroy(&attr);
	peak = painted_depth();
	if (peak == sizeof stack) {
		fprintf(stderr, "rando_measure: the painted stack overflowed %u bytes\n", STACK_SIZE);
		exit(1);
	}
	return peak;
}

/* glibc's thread start-up writes about 2 KiB below a job's entry frame before the job runs, so a
 * painted empty run would overstate the overhead: it is read as the depth of the caller's frame. */
static __attribute__((noinline)) void empty_job(Job *job)
{
	job->depth = (size_t)(stack + sizeof stack - ((unsigned char *)__builtin_frame_address(0) + 2 * sizeof(void *)));
}

static __attribute__((noinline)) void selfcheck_job(Job *job)
{
	volatile unsigned char buf[SELFCHECK_BYTES];
	size_t i;

	(void)job;
	for (i = 0; i < sizeof buf; i++)
		buf[i] = (unsigned char)i;
}

static void generate_job(Job *job)
{
	job->status = generate(job->c, job->c->seed, job->cap);
}

static int64_t now_ns(void)
{
	struct timespec t;

	clock_gettime(CLOCK_MONOTONIC, &t);
	return (int64_t)t.tv_sec * 1000000000 + t.tv_nsec;
}

static int cmp_i64(const void *a, const void *b)
{
	int64_t x = *(const int64_t *)a, y = *(const int64_t *)b;

	return (x > y) - (x < y);
}

static const char *status_name(RandoStatus status)
{
	switch (status) {
	case RANDO_COMPLETE:
		return "complete";
	case RANDO_INCOMPLETE:
		return "incomplete";
	case RANDO_UNSUPPORTED:
		return "unsupported";
	case RANDO_GAVE_UP:
		return "gave-up";
	}
	return "unknown";
}

static void read_case(const char *dir, RandoCase *c)
{
	char error[RANDO_CASE_ERROR_MAX];

	if (!rando_case_read(dir, c, error)) {
		fprintf(stderr, "rando_measure: %s\n", error);
		exit(1);
	}
}

static const char *case_name(const char *dir)
{
	return strrchr(dir, '/') ? strrchr(dir, '/') + 1 : dir;
}

static uint32_t case_cap(const RandoCase *c)
{
	return !c->has_attempts ? WATCHDOG : c->attempts == UINT32_MAX ? 0 : c->attempts + 1;
}

static void selfcheck(void)
{
	Job job = { empty_job, NULL, 0, RANDO_COMPLETE, 0 };
	size_t raw, net;

	printf("empty %zu\n", painted_run(&job));
	overhead = job.depth;
	job.fn = selfcheck_job;
	raw = painted_run(&job);
	net = raw - overhead;
	printf("overhead %zu\n", overhead);
	printf("selfcheck %zu %zu %u\n", raw, net, SELFCHECK_BYTES);
	if (raw < overhead || net < SELFCHECK_BYTES || net > SELFCHECK_BYTES + SELFCHECK_SLACK) {
		fprintf(stderr, "rando_measure: a known %u-byte use read back as %zu bytes\n", SELFCHECK_BYTES, net);
		exit(1);
	}
}

/* One line per case: case <name> <status> <attempt> <draws> <per-attempt a/b/..> <per-stage s/s/..>
 * <peak raw> <peak net> <median ns>. */
static void measure_case(const char *dir, int reps)
{
	RandoCase c;
	Job job;
	int64_t t[MAX_REPS], t0;
	const char *name = case_name(dir);
	size_t peak;
	uint32_t a;
	int i, s;

	read_case(dir, &c);
	job.fn = generate_job;
	job.c = &c;
	job.cap = case_cap(&c);
	peak = painted_run(&job);
	printf("case %s %s %u %u ", name, status_name(job.status), (unsigned)gen.attempt, (unsigned)counts.total);
	for (a = 0; a < counts.attempts_seen; a++)
		printf("%s%u", a ? "/" : "", (unsigned)counts.attempt_draws[a]);
	if (!counts.attempts_seen)
		printf("0");
	printf(" ");
	for (s = 0; s < RANDO_STAGE_COUNT; s++)
		printf("%s%u", s ? "/" : "", (unsigned)counts.stage[s]);
	printf(" %zu %zu ", peak, peak - overhead);

	generate(&c, c.seed, job.cap);
	for (i = 0; i < reps; i++) {
		t0 = now_ns();
		generate(&c, c.seed, job.cap);
		t[i] = now_ns() - t0;
	}
	qsort(t, (size_t)reps, sizeof t[0], cmp_i64);
	printf("%lld\n", (long long)t[reps / 2]);
}

/* One line per seed: seed <seed> <status> <attempt> <draws over all attempts> <ns>. */
static void sweep(const char *dir, uint32_t n)
{
	RandoCase c;
	RandoStatus status;
	int64_t t0, t;
	uint32_t seed;

	read_case(dir, &c);
	generate(&c, 1, WATCHDOG);
	for (seed = 1; seed <= n; seed++) {
		t0 = now_ns();
		status = generate(&c, seed, WATCHDOG);
		t = now_ns() - t0;
		printf("seed %u %s %u %u %lld\n", (unsigned)seed, status_name(status), (unsigned)gen.attempt,
		       (unsigned)counts.total, (long long)t);
	}
}

typedef struct Output {
	uint32_t calls;
	size_t bytes;
} Output;

static bool count_output(void *ctx, const uint8_t *rom, size_t size)
{
	Output *out = ctx;

	(void)rom;
	out->calls++;
	out->bytes += size;
	return true;
}

static void assemble_once(RandoAssembleRequest *request, Output *out, const char *name)
{
	RandoAssembleStatus status;

	memset(out, 0, sizeof *out);
	request->output_ctx = out;
	status = rando_assemble(request, &work, NULL);
	if (status != RANDO_ASSEMBLE_OK || out->calls != 1 || out->bytes != RANDO_ROM_SIZE) {
		fprintf(stderr, "rando_measure: assemble %s: status %d, %u outputs, %zu bytes, expected ok and one %u-byte image\n",
			name, (int)status, (unsigned)out->calls, out->bytes, (unsigned)RANDO_ROM_SIZE);
		exit(1);
	}
}

/* One line per case: assemble <name> <status> <bytes out> <median ns>, on the synthetic base. */
static void assemble_case(const char *dir, int reps)
{
	RandoCase c;
	RandoAssembleRequest request;
	Output out;
	int64_t t[MAX_REPS], t0;
	const char *name = case_name(dir);
	int i;

	read_case(dir, &c);
	rando_assemble_request_init(&request);
	request.source = synth_source;
	request.source_size = SYNTH_SOURCE_SIZE;
	request.patch = synth_good_patch.bytes;
	request.patch_size = synth_good_patch.size;
	memcpy(request.source_md5, synth_source_md5, RANDO_MD5_SIZE);
	memcpy(request.base_md5, synth_base_md5, RANDO_MD5_SIZE);
	request.options = c.options;
	request.heart_beep = c.heart_beep;
	request.seed = c.seed;
	request.max_attempts = case_cap(&c);
	request.output = count_output;

	assemble_once(&request, &out, name);
	for (i = 0; i < reps; i++) {
		t0 = now_ns();
		assemble_once(&request, &out, name);
		t[i] = now_ns() - t0;
	}
	qsort(t, (size_t)reps, sizeof t[0], cmp_i64);
	printf("assemble %s ok %zu %lld\n", name, out.bytes, (long long)t[reps / 2]);
}

static int usage(void)
{
	fprintf(stderr, "usage: rando_measure cases <reps> <case_dir>...\n"
			"       rando_measure cases-until <stage> <reps> <case_dir>...\n"
			"       rando_measure assemble <reps> <case_dir>...\n"
			"       rando_measure sweep <case_dir> <seeds>\n");
	return 2;
}

static int parse_reps(const char *arg)
{
	int reps = atoi(arg);

	return reps < 1 || reps > MAX_REPS ? 0 : reps;
}

int main(int argc, char **argv)
{
	int s, i, reps;
	long n;

	for (s = 0; s < RANDO_STAGE_COUNT; s++)
		stage_names[s] = rando_stage_name((RandoStage)s);
	if (argc >= 4 && strcmp(argv[1], "cases") == 0) {
		reps = parse_reps(argv[2]);
		if (!reps)
			return usage();
		selfcheck();
		for (i = 3; i < argc; i++)
			measure_case(argv[i], reps);
	} else if (argc >= 5 && strcmp(argv[1], "cases-until") == 0) {
		for (s = 0; s < RANDO_STAGE_COUNT && strcmp(argv[2], stage_names[s]) != 0; s++)
			;
		reps = parse_reps(argv[3]);
		if (s == RANDO_STAGE_COUNT || !reps)
			return usage();
		until = (RandoStage)s;
		selfcheck();
		for (i = 4; i < argc; i++)
			measure_case(argv[i], reps);
	} else if (argc >= 4 && strcmp(argv[1], "assemble") == 0) {
		reps = parse_reps(argv[2]);
		if (!reps)
			return usage();
		synth_build();
		for (i = 3; i < argc; i++)
			assemble_case(argv[i], reps);
	} else if (argc == 4 && strcmp(argv[1], "sweep") == 0) {
		n = atol(argv[3]);
		if (n < 1)
			return usage();
		sweep(argv[2], (uint32_t)n);
	} else {
		return usage();
	}
	return fflush(stdout) == 0 ? 0 : 1;
}
