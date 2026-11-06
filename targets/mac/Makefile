# The SDL app on macOS, with a fake SD card in tmp/sd (targets/mac/README.md).
#   make -C targets/mac            build the app
#   make -C targets/mac rom ROM=<file.sfc>
#   make -C targets/mac run

ROOT := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)
DEPS := $(ROOT)/tmp/sdl-deps
PREFIX := $(DEPS)/prefix
TTF_LIB := $(PREFIX)/lib/libSDL_ttf.a
TTF_PC := $(PREFIX)/lib/pkgconfig/SDL_ttf.pc
IMG_LIB := $(PREFIX)/lib/libSDL_image.a
IMG_PC := $(PREFIX)/lib/pkgconfig/SDL_image.pc
SD := $(ROOT)/tmp/sd
SD_APP := $(SD)/App/ZeldaRando
APP := $(ROOT)/tests/alttp/build/sdl/zrando
ROM_SIZE := 1048576
HEADERED_SIZE := 1049088

# make -C changes directory, not $PWD: a relative ROM is resolved from where make was typed.
ROM_PATH = $(if $(filter /%,$(ROM)),$(ROM),$(PWD)/$(ROM))

export PKG_CONFIG_PATH := $(PREFIX)/lib/pkgconfig

.PHONY: all build deps sd rom run clean

all: build

deps:
	@for f in sdl12-compat freetype libpng pkg-config; do \
		brew list --versions $$f >/dev/null || { echo "setup: brew install $$f" >&2; exit 1; }; \
	done

# Homebrew no longer has SDL_ttf for SDL 1.2: one C file, built as a static lib.
$(TTF_PC): | deps
	mkdir -p $(PREFIX)/lib/pkgconfig $(PREFIX)/include/SDL
	[ -d $(DEPS)/SDL_ttf ] || git clone -q --depth 1 --branch SDL-1.2 https://github.com/libsdl-org/SDL_ttf.git $(DEPS)/SDL_ttf
	cc -O2 -c $(DEPS)/SDL_ttf/SDL_ttf.c -o $(DEPS)/SDL_ttf.o $$(pkg-config --cflags sdl freetype2)
	ar rcs $(TTF_LIB) $(DEPS)/SDL_ttf.o
	cp $(DEPS)/SDL_ttf/SDL_ttf.h $(PREFIX)/include/SDL/
	printf '%s\n' 'prefix=$(PREFIX)' 'Name: SDL_ttf' 'Description: SDL_ttf 2.0 built locally' 'Version: 2.0.11' \
		'Requires: sdl freetype2' 'Cflags: -I$${prefix}/include/SDL' 'Libs: -L$${prefix}/lib -lSDL_ttf' > $@

# Nor SDL_image for SDL 1.2: PNG only, on the common backend (the ImageIO one is Objective-C, IMG_ImageIO.m).
$(IMG_PC): | deps
	mkdir -p $(PREFIX)/lib/pkgconfig $(PREFIX)/include/SDL $(DEPS)/img
	[ -d $(DEPS)/SDL_image ] || git clone -q --depth 1 --branch SDL-1.2 https://github.com/libsdl-org/SDL_image.git $(DEPS)/SDL_image
	for f in $(DEPS)/SDL_image/IMG*.c; do \
		cc -O2 -DLOAD_PNG -DSDL_IMAGE_USE_COMMON_BACKEND -c "$$f" -o $(DEPS)/img/$$(basename "$$f" .c).o \
			$$(pkg-config --cflags sdl libpng) || exit 1; \
	done
	ar rcs $(IMG_LIB) $(DEPS)/img/*.o
	cp $(DEPS)/SDL_image/SDL_image.h $(PREFIX)/include/SDL/
	printf '%s\n' 'prefix=$(PREFIX)' 'Name: SDL_image' 'Description: SDL_image 1.2 built locally, PNG only' 'Version: 1.2.13' \
		'Requires: sdl libpng' 'Cflags: -I$${prefix}/include/SDL' 'Libs: -L$${prefix}/lib -lSDL_image' > $@

# <SDL/SDL.h> needs the parent include dirs; Apple clang flags an intentional
# string concatenation in text_strings.c that gcc accepts.
build: $(TTF_PC) $(IMG_PC) sd
	$(MAKE) -C $(ROOT)/tests/alttp build/sdl/zrando \
		CFLAGS="-std=c99 -Wall -Wextra -Werror -DRANDO_FAULTS -Wno-string-concatenation" \
		SDL_CFLAGS="$$(pkg-config --cflags sdl SDL_ttf SDL_image) -I$(PREFIX)/include -I$$(brew --prefix)/include" \
		SDL_LIBS="$$(pkg-config --libs sdl SDL_ttf SDL_image)"
	@[ -f $(SD_APP)/base.sfc ] || echo "no ROM yet: make -C targets/mac rom ROM=<your JP 1.0 rom.sfc>"

sd:
	mkdir -p $(SD_APP)
	cp $(ROOT)/data/base.bps $(SD_APP)/

# Copies ROM to the fake SD card, stripping a 512-byte copier header if there is one.
rom: sd
	@[ -n "$(ROM)" ] || { echo "usage: make -C targets/mac rom ROM=<file.sfc>" >&2; exit 1; }
	@[ -f "$(ROM_PATH)" ] || { echo "rom: $(ROM_PATH) not found" >&2; exit 1; }
	@size=$$(wc -c < "$(ROM_PATH)" | tr -d ' '); \
	case $$size in \
	$(ROM_SIZE)) cp "$(ROM_PATH)" $(SD_APP)/base.sfc; echo "rom: no header, copied" ;; \
	$(HEADERED_SIZE)) tail -c $(ROM_SIZE) "$(ROM_PATH)" > $(SD_APP)/base.sfc; echo "rom: 512-byte header stripped" ;; \
	*) echo "rom: $$size bytes, expected $(ROM_SIZE) (or $(HEADERED_SIZE) with a header)" >&2; exit 1 ;; \
	esac
	@md5 -q $(SD_APP)/base.sfc | grep -qx 03a63945398191337e896e5771f77173 \
		&& echo "rom: JP 1.0, ready" || echo "rom: not the JP 1.0 ROM (MD5 differs): the app will refuse it" >&2

run: build
	cd $(ROOT) && $(APP) --sd tmp/sd --theme themes/alttp

clean:
	rm -rf $(DEPS)/prefix $(DEPS)/SDL_ttf.o $(DEPS)/img $(APP)
