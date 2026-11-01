#ifndef RANDO_CRC32_H
#define RANDO_CRC32_H

#include <stddef.h>
#include <stdint.h>

/* zlib's CRC32 of bytes[0..size). bytes may be NULL when size is 0, which returns 0. */
uint32_t rando_crc32(const uint8_t *bytes, size_t size);

#endif
