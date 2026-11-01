#include "rando.h"
#include "rando_case.h"

#include <pthread.h>
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
static Counts counts;
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
	return rando_generate(&gen, &c->options, seed, cap, count_draw, &counts);
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
	const char *name = strrchr(dir, '/') ? strrchr(dir, '/') + 1 : dir;
	size_t peak;
	uint32_t a;
	int i, s;

	read_case(dir, &c);
	job.fn = generate_job;
	job.c = &c;
	job.cap = !c.has_attempts ? WATCHDOG : c.attempts == UINT32_MAX ? 0 : c.attempts + 1;
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

static int usage(void)
{
	fprintf(stderr, "usage: rando_measure cases <reps> <case_dir>...\n"
			"       rando_measure sweep <case_dir> <seeds>\n");
	return 2;
}

int main(int argc, char **argv)
{
	int s, i, reps;
	long n;

	for (s = 0; s < RANDO_STAGE_COUNT; s++)
		stage_names[s] = rando_stage_name((RandoStage)s);
	if (argc >= 4 && strcmp(argv[1], "cases") == 0) {
		reps = atoi(argv[2]);
		if (reps < 1 || reps > MAX_REPS)
			return usage();
		selfcheck();
		for (i = 3; i < argc; i++)
			measure_case(argv[i], reps);
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
