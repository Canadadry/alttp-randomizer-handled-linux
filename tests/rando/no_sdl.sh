#!/bin/sh
# Usage: no_sdl.sh [--exempt FILE]... DIR...
# Fails, naming file and line, when a .c or .h file under a DIR includes a header whose
# name or first path component starts with SDL (any case). A FILE is skipped only when find
# prints exactly that path. Proves itself first on planted files.
set -eu
export LC_ALL=C

exempt=""
while [ "${1:-}" = "--exempt" ]; do
	[ $# -ge 2 ] || { echo "no_sdl: --exempt needs a file" >&2; exit 2; }
	exempt="$exempt
$2"
	shift 2
done
[ $# -ge 1 ] || { echo "usage: no_sdl.sh [--exempt FILE]... DIR..." >&2; exit 2; }

is_exempt() {
	printf '%s\n' "$exempt" | grep -Fqx -- "$1"
}

# Prints file:line: text for each SDL include under the given dirs, the exempt files aside; exit 1 when any.
scan() {
	found=0
	for f in $(find "$@" -type f \( -name '*.c' -o -name '*.h' \) | sort); do
		if is_exempt "$f"; then
			continue
		fi
		hits="$(awk '
			/^[ \t]*#[ \t]*include[ \t]*[<"]/ {
				h = $0
				sub(/^[ \t]*#[ \t]*include[ \t]*[<"]/, "", h)
				sub(/[>"].*$/, "", h)
				sub(/\/.*$/, "", h)
				if (tolower(substr(h, 1, 3)) == "sdl")
					printf "%s:%d: %s\n", FILENAME, FNR, $0
			}' "$f")"
		if [ -n "$hits" ]; then
			echo "$hits"
			found=1
		fi
	done
	return "$found"
}

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/planted" "$tmp/clean"
printf '#include <stdio.h>\n  #  include <SDL/SDL.h>\n' >"$tmp/planted/screen.c"
printf '#include <stdio.h>\n#include "ui_screen.h"\n#include "rando_sdl.h"\n' >"$tmp/clean/screen.h"
if scan "$tmp/planted" >/dev/null; then
	echo "no_sdl: self-test: a planted #include <SDL/SDL.h> was not caught" >&2
	exit 1
fi
if ! scan "$tmp/clean" >/dev/null; then
	echo "no_sdl: self-test: a file with no SDL include was refused" >&2
	exit 1
fi
mkdir "$tmp/main" "$tmp/other"
cp "$tmp/planted/screen.c" "$tmp/main/sdl_main.c"
cp "$tmp/planted/screen.c" "$tmp/other/sdl_main.c"
cp "$tmp/clean/screen.h" "$tmp/main/ui_cli.c"
saved="$exempt"
exempt="$tmp/main/sdl_main.c"
if ! scan "$tmp/main" >/dev/null; then
	echo "no_sdl: self-test: the exempt file was refused" >&2
	exit 1
fi
if scan "$tmp/main" "$tmp/other" >/dev/null; then
	echo "no_sdl: self-test: a file of the exempt file's name in another directory was not caught" >&2
	exit 1
fi
cp "$tmp/planted/screen.c" "$tmp/main/ui_cli.c"
if scan "$tmp/main" >/dev/null; then
	echo "no_sdl: self-test: an SDL include beside the exempt file was not caught" >&2
	exit 1
fi
exempt="$saved"

if ! out="$(scan "$@")"; then
	echo "$out" >&2
	echo "no_sdl: the app's UI core, SD layer, theme loader and non-SDL mains must not include SDL" >&2
	exit 1
fi
