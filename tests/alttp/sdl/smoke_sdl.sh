#!/bin/sh
# Usage: smoke_sdl.sh ZRANDO THEME
# Starts the SDL main headless (dummy video driver) on THEME, renders one frame offscreen and exits;
# checks its exit statuses, the saved frame, and that starting touches nothing on the SD tree. Then
# plays scripted keys with the fixed-run options and checks the captured frame's text. Then runs it
# on edited copies of THEME, checking the grid size reaching the core and each refusal. Last, runs
# a copy of ZRANDO with THEME as its themes/default, checking the default lookup.
set -eu
export LC_ALL=C
export SDL_VIDEODRIVER=dummy

bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
run=
theme="$(cd "$2" && pwd)"
features="$(cd "$(dirname "$0")" && pwd)/themes/features"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
tree="$tmp/sd"
mkdir -p "$tree/App/ZeldaRando"

fail() {
	echo "smoke_sdl: $*" >&2
	exit 1
}

# listing [DIR]: the sorted tree of DIR, the SD tree by default.
listing() {
	(cd "${1:-$tree}" && find . | sort)
}

# expect STATUS ARGS...: runs $run (ZRANDO when empty) in the current directory, standard error kept in $tmp/err.
expect() {
	want="$1"
	shift
	got=0
	"${run:-$bin}" "$@" >"$tmp/out" 2>"$tmp/err" || got=$?
	[ "$got" -eq "$want" ] || { cat "$tmp/err" >&2; fail "${run:-$bin} $*: exit $got, expected $want"; }
}

# picture BMP: fails unless BMP is a 640x480 picture of more than one colour.
picture() {
	python3 -I - "$1" <<'PY' || fail "$1 is not a 640x480 picture of more than one colour"
import struct, sys
data = open(sys.argv[1], "rb").read()
assert data[:2] == b"BM"
offset = struct.unpack_from("<I", data, 10)[0]
w, h = struct.unpack_from("<ii", data, 18)
bpp = struct.unpack_from("<H", data, 28)[0]
assert (w, abs(h)) == (640, 480) and bpp in (24, 32), (w, h, bpp)
step = bpp // 8
row = (w * step + 3) // 4 * 4
colours = set()
for y in range(abs(h)):
    line = data[offset + y * row: offset + y * row + w * step]
    colours.update(line[i:i + 3] for i in range(0, len(line), step))
    if len(colours) > 1:
        break
assert len(colours) > 1
PY
}

before="$(listing)"
expect 0 --sd "$tree" --theme "$theme" --frames 1 --shot "$tmp/frame.bmp"
[ "$(listing)" = "$before" ] || fail "starting changed the SD tree"
picture "$tmp/frame.bmp"

(cd "$tree/App/ZeldaRando" && expect 0 --theme "$theme" --frames 1)
[ "$(listing)" = "$before" ] || fail "starting from App/ZeldaRando changed the SD tree"

expect 2 --sd "$tree" --theme "$theme" --bogus
expect 2 --sd "$tree" --theme "$theme" --frames 0
expect 2 --sd "$tree" --theme "$theme" --frames 1x
expect 2 --sd

fixed="--seed 1 --date 2026-11-01T14:03:27 --source-md5 00112233445566778899aabbccddeeff --base-md5 ffeeddccbbaa99887766554433221100"
# shown FILE TEXT...: fails unless each TEXT is a line fragment of FILE.
shown() {
	file="$1"
	shift
	for want in "$@"; do
		grep -qF -- "$want" "$file" || { cat "$file" >&2; fail "$file lacks: $want"; }
	done
}

play="$tmp/play"
mkdir -p "$play/App/ZeldaRando"
# shellcheck disable=SC2086
expect 0 --sd "$play" --theme "$theme" $fixed --shot "$tmp/history.bmp" --shot-text "$tmp/history.txt" -- start a r1
shown "$tmp/history.txt" "[History]" "No seeds yet"
cmp -s "$tmp/history.bmp" "$tmp/frame.bmp" && fail "the scripted frame is the first frame"
# shellcheck disable=SC2086
expect 0 --sd "$play" --theme "$theme" $fixed --shot-text "$tmp/failed.txt" start
shown "$tmp/failed.txt" "Generation Failed" "Base ROM not found"
expect 0 --sd "$play" --theme "$theme" --frames 5 --shot-text "$tmp/generating.txt" down start
shown "$tmp/generating.txt" "Generating Seed" "Please wait..."
printf 'r1\n' | expect 0 --sd "$play" --theme "$theme" --shot-text "$tmp/stdin.txt" -
shown "$tmp/stdin.txt" "No seeds yet"
expect 0 --sd "$play" --theme "$theme" --shot "$tmp/quit.bmp" --shot-text "$tmp/quit.txt" r1 menu down
shown "$tmp/quit.txt" "No seeds yet"
[ -s "$tmp/quit.bmp" ] || fail "a quit wrote no --shot"

# shellcheck disable=SC2086
expect 0 --sd "$play" --theme "$theme" $fixed --unsupported goal=ganon,state=open --shot-text "$tmp/greyed.txt" up
shown "$tmp/greyed.txt" "X| Generate Seed" "Not supported with"

expect 2 --sd "$play" --theme "$theme" bogus
grep -q "unknown key: bogus" "$tmp/err" || fail "a bad key is not named"
expect 2 --sd "$play" --theme "$theme" down*0
expect 2 --sd "$play" --theme "$theme" --seed x down
grep -q "bad --seed: x" "$tmp/err" || fail "a bad --seed is not named"
expect 2 --sd "$play" --theme "$theme" --date 2026-13-01T14:03:27 down
expect 2 --sd "$play" --theme "$theme" --source-md5 00 down
expect 2 --sd "$play" --theme "$theme" --base-md5 zz112233445566778899aabbccddeeff down
expect 2 --sd "$play" --theme "$theme" --unsupported goal=ganon down
grep -q "bad --unsupported: goal=ganon" "$tmp/err" || fail "a bad --unsupported is not named"
expect 2 --sd "$play" --theme "$theme" down --frames 1
grep -q "usage: zrando" "$tmp/err" || fail "a usage error does not print the usage"
# variant NAME SECTION KEY VALUE...: copies THEME to $tmp/NAME, each KEY of [SECTION] set to VALUE.
variant() {
	dir="$tmp/$1"
	shift
	rm -rf "$dir"
	cp -R "$theme" "$dir"
	while [ $# -gt 0 ]; do
		awk -v sec="[$1]" -v key="$2" -v val="$3" '
			/^\[/ { in_sec = ($0 == sec) }
			in_sec && index($0, key " =") == 1 { $0 = key " = " val; done = 1 }
			{ print }
			END { if (!done) exit 1 }
		' "$dir/theme.ini" >"$dir/theme.new" || fail "no $2 in [$1] of $theme/theme.ini"
		mv "$dir/theme.new" "$dir/theme.ini"
		shift 3
	done
}

# one_line PREFIX: fails unless standard error is exactly one line starting with PREFIX.
one_line() {
	[ "$(wc -l <"$tmp/err" | tr -d ' ')" -eq 1 ] || { cat "$tmp/err" >&2; fail "standard error is not one line"; }
	case "$(cat "$tmp/err")" in "$1"*) ;; *) cat "$tmp/err" >&2; fail "standard error does not start with: $1" ;; esac
}

# grid_size FILE LINES COLS: fails unless FILE is a text capture of a LINES x COLS grid.
grid_size() {
	python3 -I - "$@" <<'PY' || fail "$1 is not a text capture of $2 lines of $3 columns"
import sys
rows = open(sys.argv[1], encoding="utf-8").read().split("\n")
lines, cols = int(sys.argv[2]), int(sys.argv[3])
assert rows[-1] == "" and len(rows) - 1 == lines + 2, len(rows)
assert all(len(r) == cols + 3 for r in rows[:-1]), [len(r) for r in rows]
PY
}

themed="$tmp/themed"
mkdir -p "$themed/App/ZeldaRando"
expect 0 --sd "$themed" --theme "$theme" --frames 1 --shot "$tmp/classic.bmp" --shot-text "$tmp/classic.txt"
grid_size "$tmp/classic.txt" 15 40
picture "$tmp/classic.bmp"
variant tall list row_height 64 list label_x 208
expect 0 --sd "$themed" --theme "$tmp/tall" --frames 1 --shot-text "$tmp/tall.txt"
grid_size "$tmp/tall.txt" 9 28
variant nocell font.mono size 16 font.mono cell none
expect 0 --sd "$themed" --theme "$tmp/nocell" --frames 1 --shot-text "$tmp/nocell.txt"
[ "$(head -n 1 "$tmp/nocell.txt" | wc -c | tr -d ' ')" -gt 44 ] ||
	{ cat "$tmp/nocell.txt" >&2; fail "a list font with no cell does not give a grid wider than 40 columns"; }

before="$(listing)"
# refused DIR PREFIX: fails unless --theme DIR exits 1 with one stderr line starting PREFIX, SD untouched.
refused() {
	expect 1 --sd "$tree" --theme "$1" --frames 1
	one_line "$2"
	[ "$(listing)" = "$before" ] || fail "--theme $1 changed the SD tree"
}
refused "$tmp/missing" "zrando: "
grep -q "theme.ini" "$tmp/err" || fail "a missing theme does not name theme.ini"
variant unknown
echo "bogus = 1" >>"$tmp/unknown/theme.ini"
refused "$tmp/unknown" "zrando: $tmp/unknown/theme.ini:$(wc -l <"$tmp/unknown/theme.ini" | tr -d ' '): "
variant narrow list label_x 400
refused "$tmp/narrow" "zrando: $tmp/narrow/theme.ini: page 11, width 14: the app shows page 1..36, width 20..94"
variant noimage screen image missing.png
refused "$tmp/noimage" "zrando: $tmp/noimage/missing.png: screen.image: "
variant nofont font.mono file missing.ttf
refused "$tmp/nofont" "zrando: $tmp/nofont/missing.ttf: font.mono.file: "
variant tiny font.mono cell "4 4"
refused "$tmp/tiny" "zrando: $tmp/tiny/DejaVuSansMono.ttf: font.mono.file: no size from 64 to 8 fits the 4x4 cell"
variant small
cp "$features/cursor.png" "$tmp/small/background.png"
size="$(python3 -I -c 'import struct, sys; print("%dx%d" % struct.unpack(">II", open(sys.argv[1], "rb").read()[16:24]))' "$features/cursor.png")"
refused "$tmp/small" "zrando: $tmp/small/background.png: screen.image: $size, the screen is 640x480"
variant onetab
awk '/^\[/ { skip = ($0 == "[tab.2]") } !skip' "$theme/theme.ini" >"$tmp/onetab/theme.ini"
refused "$tmp/onetab" "zrando: $tmp/onetab/theme.ini: the screen has 2 tabs, the theme 1 tab slots"

expect 2 --font x --frames 1
grep -q "unknown option: --font" "$tmp/err" || fail "--font is not refused as an unknown option"
grep -q "usage: zrando" "$tmp/err" || fail "--font does not print the usage"
sed 1d "$tmp/err" | grep -q -- "--font" && fail "the usage still mentions --font"
expect 2 --sd "$tree" --theme
grep -q "usage: zrando" "$tmp/err" || fail "--theme without a value does not print the usage"
grep -q -- "--theme <dir>" "$tmp/err" || fail "the usage does not list --theme <dir>"

# app DIR [THEME]: a copy of ZRANDO in DIR/App/ZeldaRando, with THEME copied as its themes/default.
app() {
	mkdir -p "$1/App/ZeldaRando"
	cp "$bin" "$1/App/ZeldaRando/zrando"
	if [ $# -gt 1 ]; then
		mkdir -p "$1/App/ZeldaRando/themes"
		cp -R "$2" "$1/App/ZeldaRando/themes/default"
	fi
}

app "$tmp/default" "$theme"
here="$tmp/default/App/ZeldaRando"
run=./zrando
(cd "$here" && expect 0 --sd "$themed" --frames 1 --shot "$tmp/here.bmp" --shot-text "$tmp/here.txt")
run="$here/zrando"
expect 0 --sd "$themed" --frames 1 --shot "$tmp/full.bmp" --shot-text "$tmp/full.txt"
for f in here full; do
	cmp -s "$tmp/$f.txt" "$tmp/classic.txt" || fail "the default theme ($f) does not capture the text of --theme $theme"
	cmp -s "$tmp/$f.bmp" "$tmp/classic.bmp" || fail "the default theme ($f) does not capture the frame of --theme $theme"
done
card="$(listing "$tmp/default")"
run=./zrando
(cd "$here" && expect 0 --frames 1)
[ "$(listing "$tmp/default")" = "$card" ] || fail "starting from App/ZeldaRando with the default theme changed the SD tree"

app "$tmp/bare"
run="$tmp/bare/App/ZeldaRando/zrando"
expect 1 --sd "$tree" --frames 1
one_line "zrando: "
grep -qF "$tmp/bare/App/ZeldaRando/themes/default/theme.ini" "$tmp/err" || fail "a missing default theme does not name its theme.ini"
[ "$(listing)" = "$before" ] || fail "a missing default theme changed the SD tree"

app "$tmp/broken" "$theme"
broken="$tmp/broken/App/ZeldaRando/themes/default/theme.ini"
echo "bogus = 1" >>"$broken"
run="$tmp/broken/App/ZeldaRando/zrando"
expect 1 --sd "$tree" --frames 1
one_line "zrando: $broken:$(wc -l <"$broken" | tr -d ' '): "
[ "$(listing)" = "$before" ] || fail "a broken default theme changed the SD tree"

echo "smoke_sdl: zrando started, rendered one frame offscreen, played scripted keys, read themes and exited"
