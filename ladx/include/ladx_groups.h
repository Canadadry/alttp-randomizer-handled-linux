#ifndef LADX_GROUPS_H
#define LADX_GROUPS_H

#include "ladx_pipeline.h"
#include "ladx_text.h"

int ladx_group_bank3e(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_marin(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_marin_draws(LadxEncoder *enc, LadxPipeline *p);

#define LADX_MARIN_LINE_COUNT 480

const char *ladx_marin_line(size_t i, size_t *len);

LadxTextStatus ladx_marin_text(const char *line, size_t len, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
			       LadxOutcome *raised);

LadxTextStatus ladx_marin_line_text(size_t index, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
				    LadxOutcome *raised);

LadxTextStatus ladx_marin_starting_text(LadxRng *rnd, LadxRng *global, uint8_t *out, size_t cap, size_t *n,
					LadxOutcome *raised);

int ladx_group_hints(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxHintSpot {
	const char *name_id;
	int item;
	const char *area;
} LadxHintSpot;

#define LADX_HINT_TEXT_COUNT 37
#define LADX_HINT_WRITE_COUNT (LADX_HINT_TEXT_COUNT + 6)

typedef enum LadxHintsStatus {
	LADX_HINTS_OK = 0,
	LADX_HINTS_RAISED,
	LADX_HINTS_FAILED
} LadxHintsStatus;

typedef int (*LadxHintWrite)(void *ctx, unsigned text_id, const uint8_t *bytes, size_t n);

LadxHintsStatus ladx_hints_add(LadxRng *rnd, const LadxHintSpot *spots, size_t count, LadxHintWrite write,
			       void *ctx, LadxOutcome *raised);

int ladx_group_music(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_music_draws(LadxEncoder *enc, LadxPipeline *p);

#define LADX_MUSIC_TRACK_COUNT 63
#define LADX_MUSIC_REGION_COUNT 25
#define LADX_MUSIC_SLOT_COUNT 0x20
#define LADX_MUSIC_TABLE_SIZE 0x120

LadxRngStatus ladx_music_randomize(LadxRng *rnd, uint8_t table[LADX_MUSIC_TABLE_SIZE]);

int ladx_group_title(LadxEncoder *enc, LadxPipeline *p);

void ladx_title_encode_text(const char *text, size_t n, uint8_t *out);

#define LADX_TITLE_SEED_MODULUS 17623416832ull
uint64_t ladx_title_seed_number(const char *text, size_t n);

int ladx_title_edit(LadxBackground *bg, int attributes, const char *text, size_t n, uint64_t seed_number);

int ladx_group_endscreen(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_endscreen_draws(LadxEncoder *enc, LadxPipeline *p);

#define LADX_ENDSCREEN_CAT_COUNT 24

LadxRngStatus ladx_endscreen_draw(LadxRng *global, size_t *cat);

int ladx_group_notext(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_goal(LadxEncoder *enc, LadxPipeline *p);

int ladx_group_items(LadxEncoder *enc, LadxPipeline *p);

int ladx_items_chest_id(int item);

int ladx_items_inventory_id(int item);

int ladx_items_dungeon_item(int item, int dungeon);

int ladx_group_entrances(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxEntranceInfo {
	const char *name;
	int room;
	const char *alt_room;
	int index;
	int target;
	int instrument_room;
	const char *instrument_alt;
} LadxEntranceInfo;

#define LADX_ENTRANCE_INDEX_NONE (-1)
#define LADX_ENTRANCE_INDEX_ALL (-2)
#define LADX_ENTRANCE_INFO_COUNT 104
#define LADX_ENTRANCES_MAX 128

const LadxEntranceInfo *ladx_entrance_info(size_t *count);

typedef enum LadxEntrancesStatus {
	LADX_ENTRANCES_OK = 0,
	LADX_ENTRANCES_RAISED,
	LADX_ENTRANCES_FAILED
} LadxEntrancesStatus;

LadxEntrancesStatus ladx_entrances_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room,
					  const LadxEntranceInfo *info, size_t count, const int *mapping,
					  LadxOutcome *raised);

int ladx_group_bosses(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxBossesAsm {
	uint8_t genie[2];
	uint8_t eagle_check[2];
	uint8_t fish_flag[3];
	uint8_t eagle_flag[3];
	uint8_t facade[3][2];
} LadxBossesAsm;

#define LADX_BOSS_ROOM_COUNT 9

int ladx_boss_room(int dungeon);
int ladx_miniboss_room(int key);

typedef enum LadxBossesStatus {
	LADX_BOSSES_OK = 0,
	LADX_BOSSES_RAISED,
	LADX_BOSSES_FAILED
} LadxBossesStatus;

LadxBossesStatus ladx_bosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxBossesAsm *code,
				    const int *mapping, LadxOutcome *raised);

LadxBossesStatus ladx_minibosses_change(LadxEncoder *enc, LadxSpan group, LadxRoom *room, const LadxMiniboss *mapping,
					LadxOutcome *raised);

int ladx_group_enemizer(LadxEncoder *enc, LadxPipeline *p);

#define LADX_ENEMY_TYPE_COUNT 43

LadxRngStatus ladx_enemizer_pick(LadxRoom *r, LadxRng *rng, int evilshop, int *found);

int ladx_enemizer_rooms(LadxEncoder *enc, LadxSpan group, LadxRoom *room, LadxRng *rng, int evilshop);

int ladx_group_warp_home(LadxEncoder *enc, LadxPipeline *p);

#define LADX_WARP_HOME_ROOM 0x2A3
#define LADX_WARP_HOME_OPERANDS 5

typedef enum LadxWarpHomeStatus {
	LADX_WARP_HOME_OK = 0,
	LADX_WARP_HOME_RAISED
} LadxWarpHomeStatus;

LadxWarpHomeStatus ladx_warp_home_target(const LadxRoom *r, int force_inside, uint8_t out[LADX_WARP_HOME_OPERANDS],
					 LadxOutcome *raised);

LadxWarpHomeStatus ladx_warp_home_edit(LadxBackground *bg, LadxOutcome *raised);

int ladx_group_sprites(LadxEncoder *enc, LadxPipeline *p);

#define LADX_SPRITES_SLOTS 4
#define LADX_SPRITES_SET_MAX 4
#define LADX_SPRITES_ROOMS 0x316

size_t ladx_sprites_set_order(const int *values, size_t n, int *out);

size_t ladx_sprites_set_intersection(const int *so, size_t so_n, const int *other, size_t other_n, int *out);

typedef enum LadxSpritesStatus {
	LADX_SPRITES_OK = 0,
	LADX_SPRITES_RAISED
} LadxSpritesStatus;

LadxSpritesStatus ladx_sprites_room(const LadxRoom *r, int evilshop, uint8_t out[LADX_SPRITES_SLOTS],
				    LadxOutcome *raised);

int ladx_group_save(LadxEncoder *enc, LadxPipeline *p);

typedef struct LadxHeaderIo {
	int (*read)(void *ctx, uint32_t offset, uint8_t *out, size_t n);
	int (*write)(void *ctx, uint32_t offset, const uint8_t *bytes, size_t n);
	void *ctx;
} LadxHeaderIo;

#define LADX_HEADER_NAME_LEN 15

int ladx_header_fix(const LadxHeaderIo *io, size_t len, const uint8_t *name, size_t n);

int ladx_group_header(LadxEncoder *enc, LadxPipeline *p);

#endif
