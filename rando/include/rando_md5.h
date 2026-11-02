#ifndef RANDO_MD5_H
#define RANDO_MD5_H

#include <stddef.h>
#include <stdint.h>

#define RANDO_MD5_SIZE 16

void rando_md5(const uint8_t *bytes, size_t size, uint8_t digest[RANDO_MD5_SIZE]);

#endif
