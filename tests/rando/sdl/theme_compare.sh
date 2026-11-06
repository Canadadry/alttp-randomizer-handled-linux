#!/bin/sh
# Usage: theme_compare.sh ALTTPR SD_FIXTURE THEME OUT
# For each mockup of design/mockups/, paired by mockup_states.txt: OUT/<mockup>.png, the mockup
# rendered offline (mockup_render.js, into OUT/render/), the app's capture with THEME (goldens.sh
# --draw, into OUT/app/) and their diff side by side, and a "theme-compare: <mockup>: score <s>"
# line, also in OUT/scores.txt. A setup: line (exit 1) without the browser tools; then
# theme_compare.py's self-test and the pairing check. OUT is emptied first. Exit 1 when an image
# could not be produced; never on a score.
set -eu
set -f
export LC_ALL=C

usage() {
	echo "usage: theme_compare.sh ALTTPR SD_FIXTURE THEME OUT" >&2
	exit 2
}

[ $# -eq 4 ] || usage
[ -n "$4" ] || usage
abs() {
	echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}
alttpr="$(abs "$1")"
fixture="$(abs "$2")"
theme="$(cd "$3" && pwd)"
out="$4"
here="$(cd "$(dirname "$0")" && pwd)"
mockups="$(cd "$here/../../../design/mockups" && pwd)"
states="$here/mockup_states.txt"
compare_py="$here/theme_compare.py"

. "$here/mockup_common.sh"
browser_setup

python3 -I "$compare_py" --self-test || { echo "theme-compare: theme_compare.py self-test failed"; exit 1; }
pairing_check theme-compare "$mockups" "$states" || exit 1

rm -rf "$out"
mkdir -p "$out"
out="$(cd "$out" && pwd)"

set +f
svgs="$(ls "$mockups"/*.svg)"
set -f
# shellcheck disable=SC2086
node "$here/mockup_render.js" "$out/render" $svgs
"$here/goldens.sh" "$alttpr" "$theme" "$fixture" --draw "$out/app" --states "$states"

count=0
: >"$out/scores.txt"
for svg in $svgs; do
	name="$(basename "$svg" .svg)"
	s="$(python3 -I "$compare_py" compare "$out/render/$name.png" "$out/app/$name.bmp" "$out/$name.png")" ||
		{ echo "theme-compare: $name: no image"; exit 1; }
	echo "theme-compare: $name: score $s" | tee -a "$out/scores.txt"
	count=$((count + 1))
done
echo "theme-compare: $count mockups compared with $(basename "$theme") in $out (mockup | app | diff; scores informational, no threshold)"
