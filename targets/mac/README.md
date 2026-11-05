# Running the PC build on macOS

## Build

```sh
brew install sdl12-compat freetype pkg-config
make -C targets/mac
```

This builds SDL_ttf for SDL 1.2 in `tmp/sdl-deps` (Homebrew no longer has
it), builds the app, and prepares `tmp/sd`, the fake SD card.

## ROM

```sh
make -C targets/mac rom ROM=/path/to/your/rom.sfc
```

It copies the ROM to `tmp/sd/App/ALttPR/base.sfc`, strips the 512-byte
header if the ROM has one, and checks that it is the Japanese 1.0.

## Run

```sh
make -C targets/mac run
```

Seeds go to `tmp/sd/Roms/SFC/ALttPR/`, History records to
`tmp/sd/App/ALttPR/history/`.

## Key bindings

| Key | Miyoo button | Does |
|-----|--------------|------|
| Arrow keys | D-pad | Up / Down: move between rows, seeds, or scroll the details. Left / Right: change the row's value |
| Space or `a` | A | Next value of the row; on **Generate Seed**, generate. In History: open the seed, confirm a delete. On Seed Ready / Failed: back to the settings |
| Left Ctrl or `b` | B | Quit (settings, History list). Back (seed details, delete confirmation, Seed Ready / Failed) |
| Left Shift or `x` | X | Delete the seed in History (asks first) |
| Left Option (Alt) or `y` | Y | Put the settings row back to its default |
| `e` | L1 | Switch tab (Generate / History) |
| `t` | R1 | Switch tab (Generate / History) |
| Tab or Page Up | L2 | Page up |
| Backspace or Page Down | R2 | Page down |
| Right Ctrl | Select | Nothing |
| Return | Start | Generate, from any settings row |
| Escape | Menu | Quit, from any screen |

Return, Right Ctrl and Escape act when the key is released, and only if no
other key is held. Closing the window quits.

## Limits

Only the app is built, not the SDL tests: on macOS, `SDL_main.h` renames
`main`, and the tests declare `main(void)`. `make check` does not cover this
build.
