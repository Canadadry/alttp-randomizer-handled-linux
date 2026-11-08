#ifndef SDL_ROTATE_H
#define SDL_ROTATE_H

#include <SDL/SDL.h>
#include <stdbool.h>

/* Copies src into dst turned by 180 degrees; false, dst untouched, unless both are 4 bytes per pixel and the same size. */
bool sdl_rotate_180(SDL_Surface *src, SDL_Surface *dst);

#endif
