#ifndef RANDO_CRC32_H
#define RANDO_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t rando_crc32(const uint8_t *bytes, size_t size);

#endif
