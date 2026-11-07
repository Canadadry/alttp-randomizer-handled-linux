#!/bin/sh
# Usage: gen_tables.sh [--out <dir>] [--ladxr <dir>] [--rom <file>]
#        gen_tables.sh --check [--rom <file>]
# Builds LADX's patch tables (tables.py) with the ROM (default tmp/ladx/base.gbc) and the LADXR
# checkout at the pin (default tests/ladx/oracle/ladxr): gen_tables.py build, PYTHONHASHSEED=0,
# its coverage runs under tmp/ladx-tables/runs/, the tables into <dir> (default data/ladx/; else
# inside tmp/, exit 2 otherwise). Exit 1 on a CONFLICT, UNREACHED, DEPENDS-C, FUSE-ACROSS or SPLIT
# line, nothing written.
# --check rebuilds them into tmp/ladx-tables/check/ with PYTHONHASHSEED=1 on a fresh clone
# (setup.sh --fresh into tmp/ladx-tables/ladxr-check/, the network needed) and prints one
# DIFFERENT, MISSING or EXTRA line per file unlike data/ladx/ (exit 1), else
# `tables: <n> files byte-identical to data/ladx`.
# A missing or wrong ROM, an unpinned checkout or python3 older than 3.11 is a `setup:` line,
# exit 2. Never called by make.
set -eu
export LC_ALL=C

LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef
LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd -P)"

usage() {
	echo "usage: gen_tables.sh [--out <dir>] [--ladxr <dir>] [--rom <file>] | --check [--rom <file>]" >&2
	exit 2
}
setup() {
	echo "setup: gen_tables: $1" >&2
	exit 2
}

out=
check=0
ladxr="$oracle_dir/ladxr"
ladxr_set=0
rom="$repo_dir/tmp/ladx/base.gbc"
while [ "$#" -gt 0 ]; do
	case "$1" in
	--check) check=1 ;;
	--out | --ladxr | --rom)
		[ "$#" -ge 2 ] || usage
		case "$1" in
		--out) out="$2" ;;
		--ladxr)
			ladxr="$2"
			ladxr_set=1
			;;
		--rom) rom="$2" ;;
		esac
		shift
		;;
	*) usage ;;
	esac
	shift
done
[ "$check" -eq 0 ] || { [ -z "$out" ] && [ "$ladxr_set" -eq 0 ]; } || usage

command -v python3 >/dev/null 2>&1 || setup "python3 not found"
python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' || setup "python3 is older than 3.11 (run setup.sh)"
command -v git >/dev/null 2>&1 || setup "git not found"
[ -f "$rom" ] || setup "no ROM at $rom (run fetch_rom.sh)"
sha1="$(python3 -I -c 'import hashlib, sys; print(hashlib.sha1(open(sys.argv[1], "rb").read()).hexdigest())' "$rom")"
[ "$sha1" = "$LADX_SHA1" ] || setup "ROM SHA-1 $sha1 is not the LADX English 1.0 ROM ($(printf '%.8s' "$LADX_SHA1")…)"
rom="$(cd "$(dirname "$rom")" && pwd -P)/$(basename "$rom")"

tables_dir="$repo_dir/tmp/ladx-tables"
committed="$repo_dir/data/ladx"
if [ "$check" -eq 1 ]; then
	ladxr="$tables_dir/ladxr-check"
	mkdir -p "$tables_dir"
	LADXR_DIR="$ladxr" "$oracle_dir/setup.sh" --fresh >&2
	out="$tables_dir/check"
	runs="$tables_dir/check-runs"
	hashseed=1
else
	out="${out:-$committed}"
	mkdir -p "$out"
	out="$(cd "$out" && pwd -P)"
	if [ "$out" != "$committed" ] && ! python3 -I "$oracle_dir/ladx_oracle.py" inside-tmp "$repo_dir" "$out"; then
		echo "gen_tables: refused, $out is neither data/ladx nor inside $repo_dir/tmp/ (ROM-derived outputs stay under tmp/)" >&2
		exit 2
	fi
	runs="$tables_dir/runs"
	hashseed=0
fi
[ -f "$ladxr/randomizer.py" ] || setup "$ladxr is not a LADXR checkout (run setup.sh)"
head="$(git -C "$ladxr" rev-parse HEAD 2>/dev/null || true)"
[ "$head" = "$LADXR_PIN" ] || setup "LADXR checkout $ladxr is not at the pin $(printf '%.8s' "$LADXR_PIN") (run setup.sh)"

rm -rf "$runs"
[ "$check" -eq 0 ] || rm -rf "$out"
mkdir -p "$runs" "$out"
# python3 without -I or -E, which would drop PYTHONHASHSEED.
PYTHONHASHSEED="$hashseed" PYTHONDONTWRITEBYTECODE=1 python3 "$oracle_dir/gen_tables.py" build \
	--out "$out" --runs "$runs" --ladxr "$ladxr" --rom "$rom" </dev/null
[ "$check" -eq 1 ] || exit 0

status=0
count=0
for path in "$out"/* "$committed"/*; do
	[ -f "$path" ] || continue
	name="$(basename "$path")"
	case "$path" in
	"$out"/*)
		count=$((count + 1))
		if [ ! -f "$committed/$name" ]; then
			echo "EXTRA $name"
			status=1
		elif ! cmp -s "$path" "$committed/$name"; then
			echo "DIFFERENT $name"
			status=1
		fi
		;;
	*)
		if [ ! -f "$out/$name" ]; then
			echo "MISSING $name"
			status=1
		fi
		;;
	esac
done
[ "$status" -ne 0 ] || echo "tables: $count files byte-identical to data/ladx"
exit "$status"
