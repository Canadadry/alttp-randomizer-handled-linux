#ifndef LADX_CHECK_METADATA_H
#define LADX_CHECK_METADATA_H

/* checkMetadata.py's checkMetadataTable: entry 0 is "None". */
int ladx_check_metadata_find(const char *name_id);
const char *ladx_check_metadata_name(int entry);
const char *ladx_check_metadata_area(int entry);

#endif
