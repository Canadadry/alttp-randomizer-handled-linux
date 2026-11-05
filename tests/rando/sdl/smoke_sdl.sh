#!/bin/sh
# Usage: smoke_sdl.sh ALTTPR FONT THEME
# Starts the SDL main headless (dummy video driver), renders one frame offscreen and exits; checks
# its exit statuses, the saved frame, and that starting touches nothing on the SD tree. Then plays
# scripted keys with the fixed-run options and checks the captured frame's text. Last, runs it on
# THEME and on edited copies of it, checking the grid size reaching the core and each refusal.
set -eu
export LC_ALL=C
export SDL_VIDEODRIVER=dummy

bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
font="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
theme="$(cd "$3" && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
tree="$tmp/sd"
mkdir -p "$tree/App/ALttPR"

fail() {
	echo "smoke_sdl: $*" >&2
	exit 1
}

listing() {
	(cd "$tree" && find . | sort)
}

# expect STATUS ARGS...: runs the program in the current directory, standard error kept in $tmp/err.
expect() {
	want="$1"
	shift
	got=0
	"$bin" "$@" >"$tmp/out" 2>"$tmp/err" || got=$?
	[ "$got" -eq "$want" ] || { cat "$tmp/err" >&2; fail "alttpr $*: exit $got, expected $want"; }
}

before="$(listing)"
expect 0 --sd "$tree" --font "$font" --frames 1 --shot "$tmp/frame.bmp"
[ "$(listing)" = "$before" ] || fail "starting changed the SD tree"
python3 -I - "$tmp/frame.bmp" <<'PY' || fail "the saved frame is not a 640x480 picture of more than one colour"
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

(cd "$tree/App/ALttPR" && expect 0 --font "$font" --frames 1)
[ "$(listing)" = "$before" ] || fail "starting from App/ALttPR changed the SD tree"

expect 2 --sd "$tree" --font "$font" --bogus
expect 2 --sd "$tree" --font "$font" --frames 0
expect 2 --sd "$tree" --font "$font" --frames 1x
expect 2 --sd
expect 1 --sd "$tree" --font "$tmp/missing.ttf" --frames 1
grep -q "missing.ttf" "$tmp/err" || fail "a missing font is not named on standard error"
[ "$(listing)" = "$before" ] || fail "a failed start changed the SD tree"

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
mkdir -p "$play/App/ALttPR"
# shellcheck disable=SC2086
expect 0 --sd "$play" --font "$font" $fixed --shot "$tmp/history.bmp" --shot-text "$tmp/history.txt" -- start a r1
shown "$tmp/history.txt" "[History]" "No seeds yet"
cmp -s "$tmp/history.bmp" "$tmp/frame.bmp" && fail "the scripted frame is the first frame"
# shellcheck disable=SC2086
expect 0 --sd "$play" --font "$font" $fixed --shot-text "$tmp/failed.txt" start
shown "$tmp/failed.txt" "Generation Failed" "Base ROM not found"
expect 0 --sd "$play" --font "$font" --frames 5 --shot-text "$tmp/generating.txt" down start
shown "$tmp/generating.txt" "Generating Seed" "Please wait..."
printf 'r1\n' | expect 0 --sd "$play" --font "$font" --shot-text "$tmp/stdin.txt" -
shown "$tmp/stdin.txt" "No seeds yet"
expect 0 --sd "$play" --font "$font" --shot "$tmp/quit.bmp" --shot-text "$tmp/quit.txt" r1 menu down
shown "$tmp/quit.txt" "No seeds yet"
[ -s "$tmp/quit.bmp" ] || fail "a quit wrote no --shot"

# shellcheck disable=SC2086
expect 0 --sd "$play" --font "$font" $fixed --unsupported goal=ganon,state=open --shot-text "$tmp/greyed.txt" up
shown "$tmp/greyed.txt" "X| Generate Seed" "Not supported with"

expect 2 --sd "$play" --font "$font" bogus
grep -q "unknown key: bogus" "$tmp/err" || fail "a bad key is not named"
expect 2 --sd "$play" --font "$font" down*0
expect 2 --sd "$play" --font "$font" --seed x down
grep -q "bad --seed: x" "$tmp/err" || fail "a bad --seed is not named"
expect 2 --sd "$play" --font "$font" --date 2026-13-01T14:03:27 down
expect 2 --sd "$play" --font "$font" --source-md5 00 down
expect 2 --sd "$play" --font "$font" --base-md5 zz112233445566778899aabbccddeeff down
expect 2 --sd "$play" --font "$font" --unsupported goal=ganon down
grep -q "bad --unsupported: goal=ganon" "$tmp/err" || fail "a bad --unsupported is not named"
expect 2 --sd "$play" --font "$font" down --frames 1
grep -q "usage: alttpr" "$tmp/err" || fail "a usage error does not print the usage"
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
mkdir -p "$themed/App/ALttPR"
expect 0 --sd "$themed" --font "$font" --frames 1 --shot-text "$tmp/plain.txt"
expect 0 --sd "$themed" --font "$font" --theme "$theme" --frames 1 --shot-text "$tmp/classic.txt"
cmp -s "$tmp/plain.txt" "$tmp/classic.txt" || fail "--theme $theme does not capture the same text as no theme"
grid_size "$tmp/classic.txt" 15 40
variant tall list row_height 64 list label_x 208
expect 0 --sd "$themed" --font "$font" --theme "$tmp/tall" --frames 1 --shot-text "$tmp/tall.txt"
grid_size "$tmp/tall.txt" 9 28
before="$(listing)"
expect 1 --sd "$tree" --font "$font" --theme "$tmp/missing" --frames 1
one_line "alttpr: "
grep -q "theme.ini" "$tmp/err" || fail "a missing theme does not name theme.ini"
[ "$(listing)" = "$before" ] || fail "a missing theme changed the SD tree"
variant unknown
echo "bogus = 1" >>"$tmp/unknown/theme.ini"
expect 1 --sd "$tree" --font "$font" --theme "$tmp/unknown" --frames 1
one_line "alttpr: $tmp/unknown/theme.ini:$(wc -l <"$tmp/unknown/theme.ini" | tr -d ' '): "
variant narrow list label_x 400
expect 1 --sd "$tree" --font "$font" --theme "$tmp/narrow" --frames 1
one_line "alttpr: $tmp/narrow/theme.ini: page 11, width 14: the app shows page 1..36, width 20..94"
variant nocell font.mono size 16 font.mono cell none
expect 1 --sd "$tree" --font "$font" --theme "$tmp/nocell" --frames 1
one_line "alttpr: $tmp/nocell/theme.ini: list.font: no cell, the width needs the font's metrics"
[ "$(listing)" = "$before" ] || fail "a refused theme changed the SD tree"
expect 2 --sd "$tree" --font "$font" --theme
grep -q "usage: alttpr" "$tmp/err" || fail "--theme without a value does not print the usage"
grep -q -- "--theme <dir>" "$tmp/err" || fail "the usage does not list --theme <dir>"

echo "smoke_sdl: alttpr started, rendered one frame offscreen, played scripted keys, read themes and exited"
