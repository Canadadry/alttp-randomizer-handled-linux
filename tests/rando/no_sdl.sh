#!/bin/sh
# Usage: no_sdl.sh DIR...
# Fails, naming file and line, when a .c or .h file under a DIR includes a header whose
# name or first path component starts with SDL (any case). Proves itself first on planted files.
set -eu
export LC_ALL=C

# Prints file:line: text for each SDL include under the given dirs; exit 1 when any.
scan() {
	found=0
	for f in $(find "$@" -type f \( -name '*.c' -o -name '*.h' \) | sort); do
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

if ! out="$(scan "$@")"; then
	echo "$out" >&2
	echo "no_sdl: the app's UI core and SD layer must not include SDL" >&2
	exit 1
fi
