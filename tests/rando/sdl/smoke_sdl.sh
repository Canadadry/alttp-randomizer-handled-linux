#!/bin/sh
# Usage: smoke_sdl.sh ALTTPR FONT
# Starts the SDL main headless (dummy video driver), renders one frame offscreen and exits; checks
# its exit statuses, the saved frame, and that starting touches nothing on the SD tree. Then plays
# scripted keys with the fixed-run options and checks the captured frame's text.
set -eu
export LC_ALL=C
export SDL_VIDEODRIVER=dummy

bin="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
font="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
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

expect 2 --sd "$play" --font "$font" bogus
grep -q "unknown key: bogus" "$tmp/err" || fail "a bad key is not named"
expect 2 --sd "$play" --font "$font" down*0
expect 2 --sd "$play" --font "$font" --seed x down
grep -q "bad --seed: x" "$tmp/err" || fail "a bad --seed is not named"
expect 2 --sd "$play" --font "$font" --date 2026-13-01T14:03:27 down
expect 2 --sd "$play" --font "$font" --source-md5 00 down
expect 2 --sd "$play" --font "$font" --base-md5 zz112233445566778899aabbccddeeff down
expect 2 --sd "$play" --font "$font" down --frames 1
grep -q "usage: alttpr" "$tmp/err" || fail "a usage error does not print the usage"
echo "smoke_sdl: alttpr started, rendered one frame offscreen, played scripted keys and exited"
