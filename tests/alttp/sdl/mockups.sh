#!/bin/sh
# Usage: mockups.sh ALTTPR UI_CLI SD_FIXTURE THEME OUT
# Checks design/mockups/*.svg against the app, paired by mockup_states.txt (goldens.sh's format plus
# a sixth column, page: "-" for THEME's page, or a number). A setup: line (exit 1) without python3,
# node, npm, the playwright module or a launchable Chromium. Then: mockup_text.py's self-test;
# mockup_render.js (self-proof, then every mockup offline into OUT/render/); every mockup paired;
# alttpr with THEME plays every pairing into OUT/app/; the ui_cli grid read from those captures;
# ui_cli plays every pairing into OUT/text/, byte-identical to OUT/app/ where the page is THEME's;
# each mockup compared with OUT/text/. OUT is emptied first. Exit 1 on any failure line.
set -eu
set -f
export LC_ALL=C

usage() {
	echo "usage: mockups.sh ALTTPR UI_CLI SD_FIXTURE THEME OUT" >&2
	exit 2
}

[ $# -eq 5 ] || usage
[ -n "$5" ] || usage
abs() {
	echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}
alttpr="$(abs "$1")"
ui_cli="$(abs "$2")"
fixture="$(abs "$3")"
theme="$4"
out="$5"
here="$(cd "$(dirname "$0")" && pwd)"
mockups="$here/../../../design/mockups"
states="$here/mockup_states.txt"
text_py="$here/mockup_text.py"
date=2026-11-01T14:03:27

. "$here/mockup_common.sh"
browser_setup

rm -rf "$out"
mkdir -p "$out/text" "$out/work"
out="$(cd "$out" && pwd)"
bad=0

python3 -I "$text_py" --self-test
set +f
svgs="$(ls "$mockups"/*.svg)"
set -f
# shellcheck disable=SC2086
node "$here/mockup_render.js" "$out/render" $svgs || bad=1
rendered="$(printf '%s\n' "$svgs" | wc -l | tr -d ' ')"

pairing_check mockups "$mockups" "$states" || bad=1

"$here/goldens.sh" "$alttpr" "$theme" "$fixture" --draw "$out/app" --states "$states"

size=""
while IFS='	' read -r name _fx _cap _flags _keys page; do
	[ -n "$name" ] && [ "$page" = "-" ] || continue
	this="$(python3 -I "$text_py" grid "$out/app/$name.txt")" || { echo "mockups: $name: $this"; exit 1; }
	[ -z "$size" ] || [ "$size" = "$this" ] || {
		echo "mockups: $name: alttpr grid $this, not $size like the others"
		exit 1
	}
	size="$this"
done <"$states"
[ -n "$size" ] || { echo "mockups: no pairing on the theme's page"; exit 1; }
cols="${size% *}"
lines="${size#* }"
echo "mockups: ui_cli grid --cols $cols --lines $lines (page $((lines - 4)), width $((cols - 2))) from $theme"

agreed=0
checked=0
while IFS='	' read -r name fx cap flags keys page; do
	[ -n "$name" ] || continue
	sd="$out/work/sd-$name"
	"$fixture" "$sd" "$fx" >"$out/work/flags-$name" || { echo "mockups: $name: sd_fixture $fx failed"; exit 1; }
	n="$lines"
	[ "$page" = "-" ] || n=$((page + 4))
	[ "$flags" != "-" ] || flags=""
	every=""
	[ "$cap" = "-" ] || every="--every"
	st=0
	# shellcheck disable=SC2046,SC2086
	"$ui_cli" --sd "$sd" --seed 1 --date "$date" $(cat "$out/work/flags-$name") --cols "$cols" --lines "$n" \
		$flags $every -- $keys </dev/null >"$out/work/$name.out" 2>"$out/work/$name.err" || st=$?
	[ "$st" -eq 0 ] || { cat "$out/work/$name.err"; echo "mockups: $name: ui_cli exited $st"; exit 1; }
	if [ -n "$every" ]; then
		python3 -I "$text_py" generating "$out/work/$name.out" >"$out/text/$name.txt" ||
			{ echo "mockups: $name: no generating screen in ui_cli --every"; exit 1; }
	else
		cp "$out/work/$name.out" "$out/text/$name.txt"
	fi
	rm -rf "$sd"
	[ "$page" = "-" ] || continue
	checked=$((checked + 1))
	if cmp -s "$out/text/$name.txt" "$out/app/$name.txt"; then
		agreed=$((agreed + 1))
	else
		echo "mockups: $name: ui_cli and alttpr differ"
		diff "$out/app/$name.txt" "$out/text/$name.txt" || true
		bad=1
	fi
done <"$states"

matched=0
for name in $paired; do
	if python3 -I "$text_py" compare "$mockups/$name.svg" "$out/text/$name.txt"; then
		matched=$((matched + 1))
	else
		bad=1
	fi
done

[ "$bad" -eq 0 ] || {
	echo "mockups: failed: $matched of $rendered texts match, ui_cli agrees with alttpr on $agreed of $checked"
	exit 1
}
echo "mockups: $rendered mockups render offline with the local font, ui_cli agrees with alttpr on $agreed, $matched texts match (help line included)"
