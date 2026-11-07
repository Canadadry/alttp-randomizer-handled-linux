#ifndef LADX_GROUPS_H
#define LADX_GROUPS_H

#include "ladx_pipeline.h"
#include "ladx_text.h"

/*
 * The ported C groups of generateRom (notes/ladx-inventory.md's Groups),
 * each a LadxPipelineRun registered by ladx_pipeline_groups
 * (tests/ladx/README.md, "Pipeline").
 */

/*
 * patches/bank3e.py addBank3E: data/ladx/bank3e.txt's constants, text
 * 0xBE as a copy of 0x111, seed[:0x20] at bank 0x3E 0x2F00 and, unless
 * race, the short settings string's length and bytes at 0x2F20. Needs
 * p->rom and the randomizer.
 */
int ladx_group_bank3e(LadxEncoder *enc, LadxPipeline *p);

/*
 * patches/aesthetics.py reduceMessageLengths: text 0x01 Marin's starting
 * text, then the shortened texts in LADXR's order, each with no origin,
 * 0xEB and 0xEC as 0xEA's current value with its origins. Draws from
 * p->rnd and p->global; returns 0 with p->raised set when LADXR raises.
 * Needs p->rom.
 */
int ladx_group_marin(LadxEncoder *enc, LadxPipeline *p);

/* marin's draws without the ROM: the starting text drawn and formatted (its raise included), nothing written. */
int ladx_group_marin_draws(LadxEncoder *enc, LadxPipeline *p);

/* LADXR's patches/marin.txt at the pin, its trailing blank lines dropped. */
#define LADX_MARIN_LINE_COUNT 480

/* Line i's bytes with its newline, *len set; NULL for i out of range. */
const char *ladx_marin_line(size_t i, size_t *len);

/*
 * getMarinStartingText's text of one line of len bytes: stripped,
 * unicode_escape decoded, <marinLinesAmount> made LADX_MARIN_LINE_COUNT,
 * then every <rnd...>, then every <ord...> drawn from global
 * (utils.randomNumber), then formatText. UNSUPPORTED for an escape,
 * number or empty range the pinned file never holds.
 */
LadxTextStatus ladx_marin_text(const char *line, size_t len, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
			       LadxOutcome *raised);

/* ladx_marin_text of line index, rnd.choice's pick; UNSUPPORTED out of range. */
LadxTextStatus ladx_marin_line_text(size_t index, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
				    LadxOutcome *raised);

/* getMarinStartingText(rnd): rnd.choice of a line, then ladx_marin_line_text. */
LadxTextStatus ladx_marin_starting_text(LadxRng *rnd, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
					LadxOutcome *raised);

/*
 * hints.py addHints: the 37 hint texts in rnd.shuffle's order, then the
 * books 0x200 to 0x20A. Needs the placement and p->rnd, not the ROM.
 */
int ladx_group_hints(LadxEncoder *enc, LadxPipeline *p);

/* A spot as addHints reads it; area NUL-terminated. */
typedef struct LadxHintSpot {
	const char *name_id;
	int item;
	const char *area;
} LadxHintSpot;

#define LADX_HINT_TEXT_COUNT 37
#define LADX_HINT_WRITE_COUNT (LADX_HINT_TEXT_COUNT + 6)

typedef enum LadxHintsStatus {
	LADX_HINTS_OK = 0,
	LADX_HINTS_RAISED, /* formatText raised: *raised filled */
	LADX_HINTS_FAILED  /* over LADX_LOGIC_SPOT_CAP spots, a draw or text C cannot do, or write returned 0 */
} LadxHintsStatus;

/* Called once per text in LADXR's write order; 0 stops the run as FAILED. */
typedef int (*LadxHintWrite)(void *ctx, unsigned text_id, const uint8_t *bytes, size_t n);

/* addHints(rom, rnd, spots) with spots in iteminfo_list order. */
LadxHintsStatus ladx_hints_add(LadxRng *rnd, const LadxHintSpot *spots, size_t count, LadxHintWrite write,
			       void *ctx, LadxOutcome *raised);

/*
 * patches/music.py randomizeMusic, when music is random: bank 0x02 bytes
 * 0x000..0x11F written in one record, with no origin. Needs
 * p->rnd and p->rom; draws nothing and writes nothing on another music.
 */
int ladx_group_music(LadxEncoder *enc, LadxPipeline *p);

/* music's draws without the ROM: the same draws, nothing written. */
int ladx_group_music_draws(LadxEncoder *enc, LadxPipeline *p);

#define LADX_MUSIC_TRACK_COUNT 63  /* _LOOPING_MUSIC */
#define LADX_MUSIC_REGION_COUNT 25 /* _OVERWORLD_MUSIC_REGIONS */
#define LADX_MUSIC_SLOT_COUNT 0x20
#define LADX_MUSIC_TABLE_SIZE 0x120 /* bank 0x02 0x000..0x11F, every byte randomizeMusic writes */

/* randomizeMusic(rom, rnd) on table, bank 0x02's first 0x120 bytes; table untouched on a failed draw. */
LadxRngStatus ladx_music_randomize(LadxRng *rnd, uint8_t table[LADX_MUSIC_TABLE_SIZE]);

/*
 * patches/titleScreen.py setRomInfo: bank 0 offset 7 (0x01 on race, else
 * 0x52), then title backgrounds 3 and 4, tiles then attributes, each
 * decoded, edited by ladx_title_edit and re-encoded, with no origin. The
 * text is the seed's upper-case hex, or "Race". Needs p->rom and the
 * randomizer; 0 on a malformed background.
 */
int ladx_group_title(LadxEncoder *enc, LadxPipeline *p);

/* titleScreen._encode: each of text's n bytes as a title tile into out. */
void ladx_title_encode_text(const char *text, size_t n, uint8_t *out);

/*
 * setRomInfo's seednr modulo LADX_TITLE_SEED_MODULUS (all its pattern
 * reads): text's hex digits when n > 0 and all are, else the MD5 of text,
 * each byte over 0x7F made '?'. Python int()'s sign, spaces, 0x and
 * underscores are not read.
 */
#define LADX_TITLE_SEED_MODULUS 17623416832ull /* 112 ** 5 */
uint64_t ladx_title_seed_number(const char *text, size_t n);

/*
 * setRomInfo's edits of one background (attributes: its attribute map):
 * text's two lines of 16, the guards removed, the seed number's pattern.
 * 0 when text is over 64 bytes.
 */
int ladx_title_edit(LadxBackground *bg, int attributes, const char *text, size_t n, uint64_t seed_number);

/*
 * patches/endscreen.py updateEndScreen: rom.patch's writes (unit 0 of
 * data/ladx/endscreen.txt), random.choice of LADXR's sorted cats from
 * p->global, then the chosen cat's image (unit 1 + its index), each byte
 * written with no origin. Needs p->rom; 0 unless the file has 1 +
 * LADX_ENDSCREEN_CAT_COUNT units.
 */
int ladx_group_endscreen(LadxEncoder *enc, LadxPipeline *p);

/* endscreen's draw without the ROM, nothing written. */
int ladx_group_endscreen_draws(LadxEncoder *enc, LadxPipeline *p);

#define LADX_ENDSCREEN_CAT_COUNT 24 /* the .bin files of patches/cats at the pin */

/* updateEndScreen's random.choice(cats) on global: the index into the sorted cats. */
LadxRngStatus ladx_endscreen_draw(LadxRng *global, size_t *cat);

#endif
