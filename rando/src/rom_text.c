#include "rando_text.h"
#include "rom_emit.h"

#include <assert.h>
#include <string.h>

/* The table buffer is on the stack (about 30 KB): the library keeps no mutable static state. */
void rando_rom_write_text(RomOut *out, const RandoGen *gen)
{
	uint8_t table[RANDO_TEXT_TABLE_MAX];
	uint8_t entry[RANDO_TEXT_ENTRY_MAX];
	size_t total = 0, n;
	int id;

	for (id = 0; id < RANDO_TEXT_COUNT; id++) {
		n = rando_text_seed(gen, (RandoTextId)id, entry, NULL);
		/* VT's getByteArray throws "Too BIG" here; no table on this path reaches it. */
		assert(n <= sizeof table - total);
		memcpy(table + total, entry, n);
		total += n;
	}
	rando_rom_emit(out, 0x0E0000, table, total, "writeText");
}
