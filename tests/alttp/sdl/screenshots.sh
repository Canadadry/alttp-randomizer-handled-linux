#!/bin/sh
# Usage: screenshots.sh ZRANDO THEME SD_FIXTURE OUT STATE...
# Draws each STATE of golden_states.txt with THEME twice (goldens.sh --draw --states), refuses
# unless both runs are byte-identical, then writes OUT/<state>.png, lossless, over any old one.
set -eu
export LC_ALL=C

[ $# -ge 5 ] || { echo "usage: screenshots.sh ZRANDO THEME SD_FIXTURE OUT STATE..." >&2; exit 2; }
bin="$1"
theme="$2"
fixture="$3"
out="$4"
shift 4
here="$(cd "$(dirname "$0")" && pwd)"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
	echo "screenshots: $*" >&2
	exit 1
}

: >"$tmp/states.txt"
for n in "$@"; do
	awk -F '\t' -v n="$n" '$1 == n' "$here/golden_states.txt" >"$tmp/line"
	[ -s "$tmp/line" ] || fail "$n: no such state in golden_states.txt"
	cat "$tmp/line" >>"$tmp/states.txt"
done

for run in run1 run2; do
	"$here/goldens.sh" "$bin" "$theme" "$fixture" --draw "$tmp/$run" --states "$tmp/states.txt" >/dev/null
done
for n in "$@"; do
	cmp -s "$tmp/run1/$n.bmp" "$tmp/run2/$n.bmp" || fail "$n: two runs differ, nothing written"
done

mkdir -p "$out"
for n in "$@"; do
	python3 -I - "$here" "$tmp/run1/$n.bmp" "$out/$n.png" <<'PY' || fail "$n: PNG refused"
import sys

sys.path.insert(0, sys.argv[1])
import theme_compare as codec

image = codec.decode_bmp(open(sys.argv[2], "rb").read())
data = codec.encode_png(image)
if codec.decode_png(data) != image:
    sys.exit("the PNG does not decode to the BMP's pixels")
open(sys.argv[3], "wb").write(data)
PY
done
echo "screenshots: $# states drawn with $(basename "$(cd "$theme" && pwd)") twice byte-identical, written to $out"
