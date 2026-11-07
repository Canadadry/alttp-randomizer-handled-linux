#include "check_metadata.h"

#include <string.h>

typedef struct CheckMetadata {
	const char *name_id;
	const char *name;
	const char *area;
} CheckMetadata;

/* TODO: the rest of checkMetadata.py's entries (plan 3 step 3, chunk 3). */
static const CheckMetadata table[] = {
	{"None", "Unset Room", "None"},
};

#define TABLE_COUNT ((int)(sizeof table / sizeof table[0]))

/* checkMetadataTable.get(name_id, checkMetadataTable["None"]). */
int ladx_check_metadata_find(const char *name_id)
{
	int i;

	for (i = 0; i < TABLE_COUNT; i++)
		if (strcmp(table[i].name_id, name_id) == 0)
			return i;
	return 0;
}

const char *ladx_check_metadata_name(int entry)
{
	return entry >= 0 && entry < TABLE_COUNT ? table[entry].name : table[0].name;
}

const char *ladx_check_metadata_area(int entry)
{
	return entry >= 0 && entry < TABLE_COUNT ? table[entry].area : table[0].area;
}
