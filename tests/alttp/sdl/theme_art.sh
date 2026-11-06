#!/bin/sh
# Usage: theme_art.sh --check THEME OUT | --write THEME OUT
# Renders every line of theme_art.txt from design/mockups/ (theme_art.js into OUT/.../raw/, then
# theme_art.py normalise into OUT/.../png/). A setup: line (exit 1) without the browser tools; then
# theme_art.py's self-test (theme_art.js proves itself on each run). --check: one run into OUT/,
# its pixels compared with THEME's PNGs by theme_art.py check. --write: two runs, OUT/run1 and
# OUT/run2, refused with nothing written unless byte-identical, then the PNGs copied into THEME.
# OUT is emptied first.
set -eu
set -f
export LC_ALL=C

usage() {
	echo "usage: theme_art.sh --check THEME OUT | --write THEME OUT" >&2
	exit 2
}

[ $# -eq 3 ] || usage
mode="$1"
case "$mode" in --check|--write) ;; *) usage ;; esac
[ -n "$3" ] || usage
theme="$(cd "$2" && pwd)"
out="$3"
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
mockups="$root/design/mockups"
table="$here/theme_art.txt"
art_py="$here/theme_art.py"

. "$here/mockup_common.sh"
browser_setup

python3 -I "$art_py" --self-test || { echo "theme-art: theme_art.py self-test failed"; exit 1; }

rm -rf "$out"
mkdir -p "$out"
out="$(cd "$out" && pwd)"

render() {
	node "$here/theme_art.js" "$table" "$mockups" "$1/raw" || exit 1
	python3 -I "$art_py" normalise "$table" "$1/raw" "$1/png" || exit 1
}

files="$(cut -f1 "$table")"
count="$(printf '%s\n' "$files" | wc -l | tr -d ' ')"
shown="$theme"
case "$theme" in "$root"/*) shown="${theme#"$root"/}" ;; esac

if [ "$mode" = "--check" ]; then
	render "$out"
	python3 -I "$art_py" check "$table" "$out/png" "$theme" "$(cat "$out/raw/chromium-version")"
	exit
fi

render "$out/run1"
render "$out/run2"
bad=0
for f in $files chromium-version; do
	for d in raw png; do
		[ "$d/$f" != png/chromium-version ] || continue
		cmp -s "$out/run1/$d/$f" "$out/run2/$d/$f" || { echo "theme-art: $d/$f differs between two runs"; bad=1; }
	done
done
[ "$bad" -eq 0 ] || { echo "theme-art: two runs differ, nothing written to $shown"; exit 1; }
for f in $files; do
	cp "$out/run1/png/$f" "$theme/$f"
done
echo "theme-art: $count images written to $shown"
