/*
 * e2e-synthetic's tool (tests/ladx/README.md, "Synthetic reference"): C's
 * outputs on a synthetic base, as digests, written or checked against
 * tests/ladx/synthetic_refs.txt. Host only; never reads the ROM.
 */
#include "ladx_assemble.h"
#include "ladx_inputs.h"
#include "synth_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROG "synthetic_refs"
#define VERSION_LINE "ladx-synthetic-refs 1"
#define SYNTH_SEED 0x5EED7001u
#define LINE_MAX 512
#define CASE_MAX 256
#define NAME_MAX_LEN 160

static LadxAssembler assembler;
static LadxTablesDir tables;
static SynthRom synth;
static uint8_t image[LADX_ROM_SIZE];
static char base_hex[2 * LADX_SHA1_SIZE + 1];
static uint8_t base_sha1[LADX_SHA1_SIZE];

static void hex(const uint8_t *bytes, size_t n, char *out)
{
	static const char digit[] = "0123456789abcdef";
	size_t i;

	for (i = 0; i < n; i++) {
		out[2 * i] = digit[bytes[i] >> 4];
		out[2 * i + 1] = digit[bytes[i] & 15];
	}
	out[2 * n] = '\0';
}

/* The synthetic base ------------------------------------------------------------------------------ */

typedef struct Fixer {
	uint8_t *rom;
	uint8_t set[LADX_ROM_SIZE / 8]; /* bytes already given a value: the first assignment wins */
	uint32_t state;
	int conflicts;
} Fixer;

static Fixer fixer;

static uint8_t draw(Fixer *f)
{
	f->state ^= f->state << 13;
	f->state ^= f->state >> 17;
	f->state ^= f->state << 5;
	return (uint8_t)(f->state >> 24);
}

enum { ANY, NOT_FF, IS_FF, NOT_TERM, TERM, IS_FE, OBJECT_START };

static int fits(int want, uint8_t b)
{
	switch (want) {
	case NOT_FF:
		return b != 0xFF;
	case IS_FF:
		return b == 0xFF;
	case NOT_TERM:
		return b != 0xFE && b != 0xFF;
	case TERM:
		return b == 0xFE || b == 0xFF;
	case IS_FE:
		return b == 0xFE;
	case OBJECT_START:
		return b != 0xFE;
	default:
		return 1;
	}
}

/* A fresh value for want; an object start prefers a two-byte object. */
static uint8_t pick(Fixer *f, int want)
{
	uint8_t b;

	if (want == IS_FF)
		return 0xFF;
	if (want == IS_FE)
		return 0xFE;
	do
		b = draw(f);
	while (!fits(want, b) || (want == OBJECT_START && (b >> 4 == 0x8 || b >> 4 == 0xC || b >> 4 >= 0xE)));
	return b;
}

/* The value of an atom constrained to want: a literal's, a fixed byte's, or a new one; -1 on a conflict. */
static int need(Fixer *f, LadxAtom a, int want)
{
	uint32_t q;

	if (a < 0)
		return fits(want, (uint8_t)(-1 - a)) ? -1 - a : -1;
	q = (uint32_t)a;
	if (q >= LADX_ROM_SIZE)
		return -1;
	if (f->set[q / 8] & (1u << (q % 8))) {
		if (fits(want, f->rom[q]))
			return f->rom[q];
		f->conflicts++;
		return -1;
	}
	f->set[q / 8] |= (uint8_t)(1u << (q % 8));
	f->rom[q] = pick(f, want);
	return f->rom[q];
}

static void fix_record(Fixer *f, const char *table, const LadxAtom *atoms, size_t n)
{
	size_t i;

	if (strcmp(table, "entities") == 0) {
		for (i = 0; i < n; i++)
			need(f, atoms[i], i + 1 == n ? IS_FF : NOT_FF);
	} else if (strcmp(table, "texts") == 0) {
		for (i = 0; i < n; i++)
			need(f, atoms[i], i + 1 == n ? TERM : NOT_TERM);
	} else if (strncmp(table, "rooms_", 6) == 0) {
		for (i = 0; i < n && i < 2; i++)
			need(f, atoms[i], ANY);
		while (i < n) {
			int v, width;

			if (i + 1 == n) {
				need(f, atoms[i], IS_FE);
				return;
			}
			if ((v = need(f, atoms[i], OBJECT_START)) < 0)
				return;
			width = v >> 4 == 0x8 || v >> 4 == 0xC ? 3 : v >> 4 == 0xE ? 5 : 2;
			for (i++; width > 1 && i < n; width--, i++)
				need(f, atoms[i], ANY);
		}
	} else {
		for (i = 0; i < n; i++)
			need(f, atoms[i], ANY);
	}
}

/* `@<q>+<n>` and literal hex tokens of a T record's value into atoms; 0 for a pointer or a bad token. */
static int record_atoms(const char *tokens, LadxAtom *atoms, size_t cap, size_t *n)
{
	const char *p = tokens;

	*n = 0;
	while (*p && *p != '\n') {
		char *end;

		if (*p == ' ') {
			p++;
		} else if (*p == '@') {
			unsigned long q = strtoul(p + 1, &end, 16), k, len;

			if (*end != '+')
				return 0;
			len = strtoul(end + 1, &end, 16);
			for (k = 0; k < len; k++) {
				if (*n == cap)
					return 0;
				atoms[(*n)++] = (LadxAtom)(q + k);
			}
			p = end;
		} else if (*p == 'p') {
			return 0;
		} else {
			while (*p && *p != ' ' && *p != '\n') {
				unsigned v;

				if (!p[1] || sscanf(p, "%2x", &v) != 1 || *n == cap)
					return 0;
				atoms[(*n)++] = (LadxAtom)(-1 - (int)v);
				p += 2;
			}
		}
	}
	return 1;
}

/*
 * The builder's ROM made well formed for every T record of the tables:
 * each record's value parses as its table's kind would. Then the old bytes
 * of items' two single-byte rom.patch checks (LADXR's public patch values).
 */
static int build_base(void)
{
	static LadxAtom atoms[LADX_WLOG_ATOM_MAX];
	static const struct {
		uint32_t at;
		uint8_t old;
	} patch_old[] = {{0x05u * LADX_ROM_BANK + 0x08D5u, 0x09}, {0x19u * LADX_ROM_BANK + 0x072Bu, 0x00}};
	static LadxRomTables parsed;
	const char *p = tables.text, *end = tables.text + tables.len;
	size_t i, n;

	synth_rom_build(&synth, SYNTH_SEED);
	memset(&fixer, 0, sizeof fixer);
	fixer.rom = synth.rom;
	fixer.state = SYNTH_SEED;
	while (p < end) {
		const char *eol = memchr(p, '\n', (size_t)(end - p));
		char table[LADX_WLOG_NAME_MAX + 1];
		int skip = 0;

		if (!eol)
			eol = end;
		if (p[0] == 'T' && p[1] == ' ' && sscanf(p + 2, "%31s %*s%n", table, &skip) == 1 && skip > 0 &&
		    record_atoms(p + 2 + skip, atoms, LADX_WLOG_ATOM_MAX, &n))
			fix_record(&fixer, table, atoms, n);
		p = eol + 1;
	}
	for (i = 0; i < sizeof patch_old / sizeof patch_old[0]; i++)
		synth.rom[patch_old[i].at] = patch_old[i].old;
	if (ladx_rom_tables_parse(synth.rom, LADX_ROM_SIZE, &parsed) != LADX_ROM_PARSE_OK) {
		fprintf(stderr, PROG ": the synthetic base does not parse\n");
		return 0;
	}
	ladx_sha1(synth.rom, LADX_ROM_SIZE, base_sha1);
	hex(base_sha1, LADX_SHA1_SIZE, base_hex);
	return 1;
}

/* One case ---------------------------------------------------------------------------------------- */

static void sha1_sink(void *ctx, const char *text, size_t len)
{
	ladx_sha1_update(ctx, (const uint8_t *)text, len);
}

/* Each skipped group's name, comma-separated in the library's order, or "-". */
static int in_list(const char *list, const char *name)
{
	size_t n = strlen(name);
	const char *p = list;

	while ((p = strstr(p, name)) != NULL) {
		if ((p == list || p[-1] == ',') && (p[n] == ',' || p[n] == '\0'))
			return 1;
		p += n;
	}
	return 0;
}

/*
 * The case's line after its name into out, groups of skip left out: 1, or
 * 0 with *failed the failing group (a group failing or raising), or -1 on
 * another failure (printed).
 */
static int run_case(const char *dir, const char *skip, char *out, size_t cap, const char **failed)
{
	static LadxPipelineGroup kept[LADX_PIPELINE_GROUP_MAX];
	uint8_t seed[LADX_INPUTS_SEED_BYTES], digest[LADX_SHA1_SIZE];
	char log_hex[41], rom_hex[41], spoiler_hex[41];
	const LadxPipelineGroup *all;
	LadxAssembleInput in;
	LadxAssembleStatus st;
	LadxSettings settings;
	LadxSha1 log_sha, spoiler_sha;
	int count, i, k = 0;

	if (!ladx_inputs_case(PROG, dir, &settings, seed))
		return -1;
	all = ladx_pipeline_groups(&count);
	for (i = 0; i < count; i++)
		if (!in_list(skip, all[i].name))
			kept[k++] = all[i];
	ladx_sha1_init(&log_sha);
	memset(&in, 0, sizeof in);
	in.base = synth.rom;
	in.base_len = LADX_ROM_SIZE;
	in.base_sha1 = base_sha1;
	in.tables = &tables.tables;
	in.settings = &settings;
	in.seed = seed;
	in.seed_len = sizeof seed;
	in.log = sha1_sink;
	in.log_ctx = &log_sha;
	in.groups = kept;
	in.group_count = k;
	st = ladx_assemble(&assembler, &in, image);
	if (st == LADX_ASSEMBLE_PIPELINE && (assembler.pipeline_error.status == LADX_PIPELINE_GROUP ||
					     assembler.pipeline_error.status == LADX_PIPELINE_RAISED)) {
		*failed = assembler.pipeline_error.group;
		return 0;
	}
	if (st != LADX_ASSEMBLE_OK) {
		fprintf(stderr, PROG ": %s: %s\n", dir, ladx_assemble_status_name(st));
		return -1;
	}
	ladx_sha1_final(&log_sha, digest);
	hex(digest, sizeof digest, log_hex);
	ladx_sha1(image, sizeof image, digest);
	hex(digest, sizeof digest, rom_hex);
	strcpy(spoiler_hex, "none");
	if (assembler.spoiler.built) {
		ladx_sha1_init(&spoiler_sha);
		if (!ladx_spoiler_render(&assembler.spoiler, sha1_sink, &spoiler_sha)) {
			fprintf(stderr, PROG ": %s: the spoiler does not render\n", dir);
			return -1;
		}
		ladx_sha1_final(&spoiler_sha, digest);
		hex(digest, sizeof digest, spoiler_hex);
	}
	snprintf(out, cap, "log %s rom %s spoiler %s skipped %s", log_hex, rom_hex, spoiler_hex, skip[0] ? skip : "-");
	return 1;
}

/* dir's last component, trailing slashes dropped (static, overwritten by the next call). */
static const char *base_name(const char *dir)
{
	static char name[NAME_MAX_LEN];
	size_t n = strlen(dir), start;

	while (n > 1 && dir[n - 1] == '/')
		n--;
	for (start = n; start > 0 && dir[start - 1] != '/'; start--)
		;
	snprintf(name, sizeof name, "%.*s", (int)(n - start), dir + start);
	return name;
}

/* The comparison ---------------------------------------------------------------------------------- */

typedef struct Refs {
	char name[CASE_MAX][NAME_MAX_LEN];
	char line[CASE_MAX][LINE_MAX];
	int count;
	char base[LINE_MAX];
} Refs;

/* The value of field (the token after it) in line into out; "" when absent. */
static void field_value(const char *line, const char *field, char *out)
{
	char copy[LINE_MAX], *t;

	out[0] = '\0';
	strcpy(copy, line);
	for (t = strtok(copy, " "); t; t = strtok(NULL, " "))
		if (strcmp(t, field) == 0) {
			t = strtok(NULL, " ");
			if (t)
				strcpy(out, t);
			return;
		}
}

/* The first field whose value differs ("log", "rom", "spoiler", "skipped", else "line"), or NULL when equal. */
static const char *first_difference(const char *want, const char *got)
{
	static const char *const fields[] = {"log", "rom", "spoiler", "skipped"};
	char a[LINE_MAX], b[LINE_MAX];
	size_t i;

	if (strcmp(want, got) == 0)
		return NULL;
	for (i = 0; i < sizeof fields / sizeof fields[0]; i++) {
		field_value(want, fields[i], a);
		field_value(got, fields[i], b);
		if (strcmp(a, b) != 0)
			return fields[i];
	}
	return "line";
}

static int find(const Refs *r, const char *name)
{
	int i;

	for (i = 0; i < r->count; i++)
		if (strcmp(r->name[i], name) == 0)
			return i;
	return -1;
}

/* got's lines against want's, one verdict line each, to out; the number of problems. */
static int compare(const Refs *want, const Refs *got, FILE *out)
{
	int i, bad = 0;

	if (strcmp(want->base, got->base) != 0) {
		fprintf(out, "DIFFERENT base: the synthetic base is not the reference's\n");
		bad++;
	}
	for (i = 0; i < got->count; i++) {
		int j = find(want, got->name[i]);
		const char *field;

		if (j < 0) {
			fprintf(out, "EXTRA %s\n", got->name[i]);
			bad++;
		} else if (strncmp(got->line[i], "fails in ", 9) == 0) {
			fprintf(out, "DIFFERENT %s: %s\n", got->name[i], got->line[i]);
			bad++;
		} else if ((field = first_difference(want->line[j], got->line[i])) != NULL) {
			fprintf(out, "DIFFERENT %s: %s\n", got->name[i], field);
			bad++;
		} else {
			fprintf(out, "ok %s\n", got->name[i]);
		}
	}
	for (i = 0; i < want->count; i++)
		if (find(got, want->name[i]) < 0) {
			fprintf(out, "MISSING %s\n", want->name[i]);
			bad++;
		}
	return bad;
}

static int read_refs(const char *path, Refs *r)
{
	char line[LINE_MAX + NAME_MAX_LEN];
	int n = 0;
	FILE *f = fopen(path, "r");

	if (!f) {
		fprintf(stderr, PROG ": cannot open %s\n", path);
		return 0;
	}
	r->count = 0;
	while (fgets(line, sizeof line, f)) {
		char *sp;
		size_t len = strlen(line);

		n++;
		if (len && line[len - 1] == '\n')
			line[--len] = '\0';
		if (n == 1) {
			if (strcmp(line, VERSION_LINE) != 0)
				break;
			continue;
		}
		if (n == 2) {
			if (strncmp(line, "base ", 5) != 0)
				break;
			strcpy(r->base, line + 5);
			continue;
		}
		sp = strchr(line, ' ');
		if (!sp || sp - line >= NAME_MAX_LEN || strlen(sp + 1) >= LINE_MAX || r->count == CASE_MAX)
			break;
		*sp = '\0';
		strcpy(r->name[r->count], line);
		strcpy(r->line[r->count], sp + 1);
		r->count++;
		n = n > 2 ? n : 3;
		continue;
	}
	if (!feof(f) || n < 2) {
		fprintf(stderr, PROG ": %s:%d: malformed\n", path, n);
		fclose(f);
		return 0;
	}
	fclose(f);
	return 1;
}

/* Planted references against a planted run: every kind of problem detected. */
static int self_test(void)
{
	static Refs want, got;
	static const char *const line =
		"log 1111111111111111111111111111111111111111 rom 2222222222222222222222222222222222222222 "
		"spoiler none skipped -";
	static const struct {
		const char *what, *name, *got_line, *expected;
	} plants[] = {
		{"equal", "a", NULL, "ok a\nok b\n"},
		{"a flipped log digest", "a",
		 "log 1111111111111111111111111111111111111110 rom 2222222222222222222222222222222222222222 "
		 "spoiler none skipped -",
		 "DIFFERENT a: log\nok b\n"},
		{"a flipped output digest", "a",
		 "log 1111111111111111111111111111111111111111 rom 2222222222222222222222222222222222222223 "
		 "spoiler none skipped -",
		 "DIFFERENT a: rom\nok b\n"},
		{"a spoiler where none was", "a",
		 "log 1111111111111111111111111111111111111111 rom 2222222222222222222222222222222222222222 "
		 "spoiler 3333333333333333333333333333333333333333 skipped -",
		 "DIFFERENT a: spoiler\nok b\n"},
		{"a group skipped", "a",
		 "log 1111111111111111111111111111111111111111 rom 2222222222222222222222222222222222222222 "
		 "spoiler none skipped items",
		 "DIFFERENT a: skipped\nok b\n"},
		{"a group failing that the reference ran", "a", "fails in items", "DIFFERENT a: fails in items\nok b\n"},
		{"a missing case", NULL, NULL, "ok b\nMISSING a\n"},
		{"an extra case", "c", NULL, "ok a\nok b\nEXTRA c\n"},
	};
	char text[512];
	size_t i;
	int failed = 0;

	for (i = 0; i < sizeof plants / sizeof plants[0]; i++) {
		FILE *f = tmpfile();
		size_t n;

		if (!f) {
			fprintf(stderr, PROG ": cannot open a temporary file\n");
			return 2;
		}
		memset(&want, 0, sizeof want);
		memset(&got, 0, sizeof got);
		strcpy(want.base, "b");
		strcpy(got.base, "b");
		strcpy(want.name[0], "a");
		strcpy(want.line[0], line);
		strcpy(want.name[1], "b");
		strcpy(want.line[1], line);
		want.count = 2;
		if (plants[i].name && strcmp(plants[i].name, "c") == 0) {
			got = want;
			strcpy(got.name[2], "c");
			strcpy(got.line[2], line);
			got.count = 3;
		} else if (plants[i].name) {
			got = want;
			if (plants[i].got_line)
				strcpy(got.line[0], plants[i].got_line);
		} else {
			strcpy(got.name[0], "b");
			strcpy(got.line[0], line);
			got.count = 1;
		}
		compare(&want, &got, f);
		rewind(f);
		n = fread(text, 1, sizeof text - 1, f);
		text[n] = '\0';
		fclose(f);
		if (strcmp(text, plants[i].expected) != 0) {
			printf("e2e-synthetic: self-test failed: %s\n", plants[i].what);
			failed = 1;
		}
	}
	{
		FILE *f = tmpfile();

		strcpy(got.base, "other");
		if (!f || compare(&want, &got, f) == 0) {
			printf("e2e-synthetic: self-test failed: another base\n");
			failed = 1;
		}
		if (f)
			fclose(f);
	}
	if (failed)
		return 1;
	printf("e2e-synthetic: self-test, %d planted references as expected\n", (int)(sizeof plants / sizeof plants[0]) + 1);
	return 0;
}

/* Modes ------------------------------------------------------------------------------------------- */

static Refs refs_want, refs_got;

static void note_skipped(char *all, size_t cap, const char *skip)
{
	char copy[LINE_MAX], *g;

	if (!skip[0])
		return;
	strcpy(copy, skip);
	for (g = strtok(copy, ","); g; g = strtok(NULL, ","))
		if (!in_list(all, g)) {
			size_t n = strlen(all);

			snprintf(all + n, cap - n, "%s%s", n ? "," : "", g);
		}
}

/* --write: each case's skip list found by removing every group that fails on the synthetic base. */
static int write_refs(const char *path, char **dirs, int n)
{
	char skip[LINE_MAX], line[LINE_MAX];
	FILE *f;
	int i;

	f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, PROG ": cannot write %s\n", path);
		return 1;
	}
	fprintf(f, "%s\nbase %s\n", VERSION_LINE, base_hex);
	for (i = 0; i < n; i++) {
		const char *failed = NULL;
		int st;

		skip[0] = '\0';
		while ((st = run_case(dirs[i], skip, line, sizeof line, &failed)) == 0) {
			size_t len = strlen(skip);

			snprintf(skip + len, sizeof skip - len, "%s%s", len ? "," : "", failed);
		}
		if (st < 0) {
			fclose(f);
			remove(path);
			return 2;
		}
		fprintf(f, "%s %s\n", base_name(dirs[i]), line);
	}
	if (fclose(f) != 0) {
		fprintf(stderr, PROG ": cannot write %s\n", path);
		return 1;
	}
	printf("synthetic-refs: %d cases and combos written to %s, base %s\n", n, path, base_hex);
	return 0;
}

static int check_refs(const char *path, char **dirs, int n)
{
	char all[LINE_MAX] = "";
	int i, bad;

	if (!read_refs(path, &refs_want))
		return 2;
	strcpy(refs_got.base, base_hex);
	refs_got.count = 0;
	for (i = 0; i < n && refs_got.count < CASE_MAX; i++) {
		const char *name = base_name(dirs[i]), *failed = NULL;
		int j = find(&refs_want, name), st;
		const char *skip = "";
		char *sp;

		if (j >= 0 && (sp = strstr(refs_want.line[j], " skipped ")) != NULL && strcmp(sp + 9, "-") != 0)
			skip = sp + 9;
		note_skipped(all, sizeof all, skip);
		st = run_case(dirs[i], skip, refs_got.line[refs_got.count], LINE_MAX, &failed);
		if (st < 0)
			return 2;
		if (st == 0)
			snprintf(refs_got.line[refs_got.count], LINE_MAX, "fails in %s", failed);
		snprintf(refs_got.name[refs_got.count], NAME_MAX_LEN, "%s", name);
		refs_got.count++;
	}
	bad = compare(&refs_want, &refs_got, stdout);
	if (bad) {
		printf("e2e-synthetic: %d cases and combos, %d problems against %s\n", n, bad, path);
		return 1;
	}
	printf("e2e-synthetic: %d cases and combos equal to %s (base %s), groups without a reference in some: %s\n", n,
	       path, base_hex, all[0] ? all : "none");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && strcmp(argv[1], "--self-test") == 0)
		return self_test();
	if (argc < 5 || strcmp(argv[1], "--tables") != 0 ||
	    (strcmp(argv[3], "--write") != 0 && strcmp(argv[3], "--check") != 0)) {
		fprintf(stderr, "usage: " PROG " --self-test\n"
				"       " PROG " --tables <dir> --write|--check <refs file> <case_dir>...\n");
		return 2;
	}
	if (!ladx_inputs_tables(PROG, argv[2], &tables) || !build_base())
		return 2;
	if (fixer.conflicts)
		printf("synthetic base: %d table bytes left ill-formed by conflicting records\n", fixer.conflicts);
	if (strcmp(argv[3], "--write") == 0)
		return write_refs(argv[4], argv + 5, argc - 5);
	return check_refs(argv[4], argv + 5, argc - 5);
}
