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

/*
 * patches/aesthetics.py noText, when textmode is none: every texts index
 * key but 0x217..0x21A whose current value is not an integer pointer,
 * ascending, written as its last byte with that byte's origin (empty
 * stays empty). Needs p->rom; 0 on an unknown entry.
 */
int ladx_group_notext(LadxEncoder *enc, LadxPipeline *p);

/*
 * The goal group (patches/maze.py patchMaze, patches/goal.py
 * setSpecificInstruments and setRequiredInstrumentCount) on the world
 * setup's goal: text 0x1A3 and data/ladx/goal.txt's unit for the count or
 * the sorted instruments (open: the count -1); for the maze, its unit, each
 * sign's room with the sign entity inserted first and its sign table bytes,
 * then room 6 without its 0xDE entities. Nothing for seashells (goal-fixed).
 * Needs p->rom; 0 unless the file has 265 units, on a malformed room or
 * a goal it cannot write.
 */
int ladx_group_goal(LadxEncoder *enc, LadxPipeline *p);

/*
 * The items group (patches/chest.py setMultiChest, then every spot's
 * patch in iteminfo_list order, locations/ at the pin; tests/ladx/README.md,
 * "Pipeline"): each kind's item bytes and texts, assembled patches from
 * data/ladx/items.txt (12 units), room 0xF2 through the room editor. Needs
 * p->rom; draws nothing; returns 0 with p->raised set where LADXR raises.
 */
int ladx_group_items(LadxEncoder *enc, LadxPipeline *p);

/* locations/constants.py CHEST_ITEMS[item]; -1 when it lacks the item. */
int ladx_items_chest_id(int item);

/* int(INVENTORY_MAP[item], 16); -1 when it lacks the item. */
int ladx_items_inventory_id(int item);

/* The dungeon-item reduction: KEYn, NIGHTMARE_KEYn, MAPn, COMPASSn, STONE_BEAKn with n == dungeon as its generic item, else item. */
int ladx_items_dungeon_item(int item, int dungeon);

/*
 * patches/entrances.py changeEntrances on the identity entrance_mapping of
 * WorldSetup.__init__ (tests/ladx/README.md, "Pipeline"): every entrance,
 * alt, target and instrument room loaded from the current state, its warps
 * to a changed room replaced, stored. Needs p->rom; draws nothing; returns
 * 0 with p->raised set where LADXR raises, 0 alone on a room it cannot load
 * or store.
 */
int ladx_group_entrances(LadxEncoder *enc, LadxPipeline *p);

/* entranceInfo.py's EntranceInfo, the fields changeEntrances reads. */
typedef struct LadxEntranceInfo {
	const char *name;
	int room;
	const char *alt_room; /* NULL: None */
	int index;	      /* LADX_ENTRANCE_INDEX_NONE, _ALL, or the warp's index */
	int target;
	int instrument_room;	    /* -1: None or an alt key */
	const char *instrument_alt; /* the alt key, else NULL */
} LadxEntranceInfo;

#define LADX_ENTRANCE_INDEX_NONE (-1)
#define LADX_ENTRANCE_INDEX_ALL (-2)
#define LADX_ENTRANCE_INFO_COUNT 104
#define LADX_ENTRANCES_MAX 128

/* ENTRANCE_INFO at the pin, in its order; *count LADX_ENTRANCE_INFO_COUNT. */
const LadxEntranceInfo *ladx_entrance_info(size_t *count);

typedef enum LadxEntrancesStatus {
	LADX_ENTRANCES_OK = 0,
	LADX_ENTRANCES_RAISED, /* LADXR's IndexError or AssertionError: *raised filled */
	LADX_ENTRANCES_FAILED  /* over LADX_ENTRANCES_MAX, a mapping value out of range, a room that does not load or store */
} LadxEntrancesStatus;

/*
 * changeEntrances(rom, mapping) over info[0..count-1]: mapping holds
 * 2 * count target keys, key k < count the entrance k and count + k its
 * ":inside", in the dict's order. room is the scratch the rooms are edited in.
 */
LadxEntrancesStatus ladx_entrances_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room,
					  const LadxEntranceInfo *info, size_t count, const int *mapping,
					  LadxOutcome *raised);

/*
 * The bosses group (patches/enemies.py changeBosses on the world setup's
 * boss_mapping, then changeMiniBosses on its miniboss_mapping;
 * tests/ladx/README.md, "Pipeline"), with data/ladx/bosses.txt's assembled
 * patches (3 units). Needs p->rom; draws nothing; returns 0 with p->raised
 * set where LADXR raises, 0 alone on a room it cannot load or store.
 */
int ladx_group_bosses(LadxEncoder *enc, LadxPipeline *p);

/* changeBosses' assembled instructions, each written whole with its operand replaced (bosses.txt units 2 and 3). */
typedef struct LadxBossesAsm {
	uint8_t genie[2];	 /* bank 4 0x0521: ld [hl], $91 */
	uint8_t eagle_check[2];	 /* bank 2 0x1FC8: cp n */
	uint8_t fish_flag[3];	 /* bank 3 0x1A0F: ld hl, nn */
	uint8_t eagle_flag[3];	 /* bank 3 0x1A04: ld hl, nn */
	uint8_t facade[3][2];	 /* bank 4 0x121D, 0x1226, 0x127F: cp $00 */
} LadxBossesAsm;

#define LADX_BOSS_ROOM_COUNT 9

/* enemies.py BOSS_ROOMS[dungeon] at the pin; -1 out of range. */
int ladx_boss_room(int dungeon);
/* MINIBOSS_ROOMS of miniboss_mapping's key i (ladx_miniboss_key's order); -1 out of range. */
int ladx_miniboss_room(int key);

typedef enum LadxBossesStatus {
	LADX_BOSSES_OK = 0,
	LADX_BOSSES_RAISED, /* LADXR's KeyError or IndexError: *raised filled */
	LADX_BOSSES_FAILED  /* a mapping value out of range, a room that does not load or store, the encoder refused */
} LadxBossesStatus;

/* changeBosses(rom, mapping), mapping LADX_BOSS_ROOM_COUNT targets 0..8; room is the scratch the rooms are edited in. */
LadxBossesStatus ladx_bosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxBossesAsm *code,
				    const int *mapping, LadxOutcome *raised);

/* changeMiniBosses(rom, mapping) after its fixed patches (bosses.txt unit 1, the caller's), mapping LADX_MINIBOSS_KEY_COUNT minibosses. */
LadxBossesStatus ladx_minibosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxMiniboss *mapping,
					LadxOutcome *raised);

/*
 * patches/enemies.py randomizeEnemies, when enemies is overworld: its own
 * rnd-labelled generator seeded from the run's seed (p->rnd's sink, stage
 * patches.enemies:randomizeEnemies), every overworld room but the five
 * logic-important ones picked and, when it holds an enemy, stored; then
 * data/ladx/enemizer.txt's one unit (the colour ghouls' patches). Needs
 * p->rom and p->rnd; draws and writes nothing on another enemies value.
 */
int ladx_group_enemizer(LadxEncoder *enc, LadxPipeline *p);

#define LADX_ENEMY_TYPE_COUNT 43 /* ENEMY_TYPES */

/*
 * One room's pick: the sprite slots its entities need (evilshop:
 * addEvilShop's SPRITE_DATA), a pushable gravestone fixing slot 3, then
 * each enemy type in ascending order drawn by rng.choice over the sorted
 * ENEMY_TYPES until its slots fit, every entity of that type rewritten.
 * *found 1 when the room holds an enemy (LADXR stores it), else 0 and
 * nothing drawn. Any draw error is returned with r's entities unchanged.
 */
LadxRngStatus ladx_enemizer_pick(LadxRoom *r, LadxRng *rng, int evilshop, int *found);

/* randomizeEnemies' room loop: rooms 0..0xFF less 0x24, 0xE2, 0xF2, 0x5A and 0x58, loaded from enc's state; 0 on a room that does not load or store, or a draw error. */
int ladx_enemizer_rooms(LadxEncoder *enc, LadxSpan group, LadxRoom *room, LadxRng *rng, int evilshop);

/*
 * patches/core.py warpHome (tests/ladx/README.md, "Pipeline"):
 * data/ladx/warp-home.txt's one unit (the S&Q menu's assembled patches
 * less five operands, the menu sprite), the operands from the start house
 * 0x2A3's first warp as the encoder's state holds it, then background
 * 0x0D's tiles edited, with no origin. Needs p->rom; draws nothing;
 * returns 0 with p->raised set where LADXR raises.
 */
int ladx_group_warp_home(LadxEncoder *enc, LadxPipeline *p);

#define LADX_WARP_HOME_ROOM 0x2A3
#define LADX_WARP_HOME_OPERANDS 5

typedef enum LadxWarpHomeStatus {
	LADX_WARP_HOME_OK = 0,
	LADX_WARP_HOME_RAISED /* IndexError (no warp), AssemblerException (room 0x100), KeyError (a missing tile): *raised filled */
} LadxWarpHomeStatus;

/* warpHome's handler operands (type, map, room, x, y) from the start house r. */
LadxWarpHomeStatus ladx_warp_home_target(const LadxRoom *r, int force_inside, uint8_t out[LADX_WARP_HOME_OPERANDS],
					 LadxOutcome *raised);

/* warpHome's edits of background 0x0D's tiles: the S&Q screen's rows moved down, its third option written. */
LadxWarpHomeStatus ladx_warp_home_edit(LadxBackground *bg, LadxOutcome *raised);

/*
 * patches/aesthetics.py updateSpriteData (tests/ladx/README.md,
 * "Pipeline"): data/ladx/sprites.txt's one unit (the exception patches),
 * then every room 0..0x315 but 0x2FF loaded from the encoder's state and
 * its four sprite values written to room_sprite_data_overworld or _indoor,
 * with no origin. Needs p->rom; draws nothing; returns 0 with p->raised
 * set where LADXR raises, 0 alone on a room that does not load.
 */
int ladx_group_sprites(LadxEncoder *enc, LadxPipeline *p);

#define LADX_SPRITES_SLOTS 4
#define LADX_SPRITES_SET_MAX 4
#define LADX_SPRITES_ROOMS 0x316

/*
 * CPython's iteration order of a set of ints 0..255 built by adding
 * values[0..n-1] in order (n <= LADX_SPRITES_SET_MAX), into out; returns
 * its size, a repeated value added once.
 */
size_t ladx_sprites_set_order(const int *values, size_t n, int *out);

/*
 * so.intersection(other) of two sets given as their adding orders: CPython
 * walks the smaller (other on a tie) and adds the members the other holds.
 * Writes the result's adding order into out; returns its size.
 */
size_t ladx_sprites_set_intersection(const int *so, size_t so_n, const int *other, size_t other_n, int *out);

typedef enum LadxSpritesStatus {
	LADX_SPRITES_OK = 0,
	LADX_SPRITES_RAISED /* AssertionError (no common graphics) or KeyError (an entity SPRITE_DATA lacks): *raised filled */
} LadxSpritesStatus;

/* One room's four sprite values as updateSpriteData computes them (evilshop: addEvilShop's SPRITE_DATA). */
LadxSpritesStatus ladx_sprites_room(const LadxRoom *r, int evilshop, uint8_t out[LADX_SPRITES_SLOTS],
				    LadxOutcome *raised);

/*
 * romTables.py ROMWithTables.save: the eleven tables' storage regions as
 * LADXR's parse computes them from p->rom->tables, then each table stored
 * (ladx_store_table) in LADXR's order on its entries' current values and
 * origins in enc; the overworld sprite data's leftover added to the indoor
 * table, the ld hl at bank 0 0x0DFA patched to its start | 0x4000 and the
 * indoor data_addr moved there. Returns 0 with p->raised set when LADXR
 * raises. Needs p->rom; draws nothing.
 */
int ladx_group_save(LadxEncoder *enc, LadxPipeline *p);

#endif
