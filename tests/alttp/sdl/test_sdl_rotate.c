#include "sdl_rotate.h"
#include "support.h"

#include <stdint.h>

#define TEST "test_sdl_rotate"

#define CHECK(cond) \
	do { \
		if (!(cond)) \
			test_fail(TEST, "%s:%d: %s", __FILE__, __LINE__, #cond); \
	} while (0)

static SDL_Surface *surface(int w, int h, int bpp)
{
	SDL_Surface *s = bpp == 32 ? SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0)
				   : SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F, 0);

	if (s == NULL)
		test_fail(TEST, "SDL_CreateRGBSurface: %s", SDL_GetError());
	return s;
}

static uint32_t *pixel(SDL_Surface *s, int x, int y)
{
	return (uint32_t *)((uint8_t *)s->pixels + (size_t)y * s->pitch) + x;
}

static void turns_every_pixel(void)
{
	SDL_Surface *src = surface(3, 2, 32), *dst = surface(3, 2, 32);
	int x, y;

	for (y = 0; y < 2; y++)
		for (x = 0; x < 3; x++)
			*pixel(src, x, y) = (uint32_t)(0x10 * y + x + 1);
	CHECK(sdl_rotate_180(src, dst));
	for (y = 0; y < 2; y++)
		for (x = 0; x < 3; x++)
			if (*pixel(dst, 2 - x, 1 - y) != *pixel(src, x, y))
				test_fail(TEST, "pixel %d,%d: %08x landed as %08x", x, y, (unsigned)*pixel(src, x, y),
					  (unsigned)*pixel(dst, 2 - x, 1 - y));
	SDL_FreeSurface(src);
	SDL_FreeSurface(dst);
}

static void refuses_what_it_cannot_turn(void)
{
	SDL_Surface *src = surface(3, 2, 32), *wide = surface(4, 2, 32), *tall = surface(3, 3, 32),
		    *shallow = surface(3, 2, 16);

	*pixel(wide, 0, 0) = 0xABCDEFu;
	CHECK(!sdl_rotate_180(src, wide));
	CHECK(*pixel(wide, 0, 0) == 0xABCDEFu);
	CHECK(!sdl_rotate_180(src, tall));
	CHECK(!sdl_rotate_180(src, shallow));
	CHECK(!sdl_rotate_180(shallow, src));
	CHECK(!sdl_rotate_180(src, src));
	CHECK(!sdl_rotate_180(NULL, src));
	CHECK(!sdl_rotate_180(src, NULL));
	SDL_FreeSurface(src);
	SDL_FreeSurface(wide);
	SDL_FreeSurface(tall);
	SDL_FreeSurface(shallow);
}

int main(void)
{
	turns_every_pixel();
	refuses_what_it_cannot_turn();
	puts(TEST ": every pixel turned by 180 degrees; other sizes, depths, NULL and src == dst refused");
	return 0;
}
