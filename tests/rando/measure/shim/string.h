#ifndef RANDO_MEASURE_SHIM_STRING_H
#define RANDO_MEASURE_SHIM_STRING_H

#include <stddef.h>

void *memcpy(void *dst, const void *src, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
int strcmp(const char *a, const char *b);

#endif
