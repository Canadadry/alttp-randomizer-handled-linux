#!/bin/sh
# Usage: gen_extract.sh [--out <dir>]
#        gen_extract.sh --check
# Runs setup.sh, then writes the six extraction fixtures (extract.py generate: the ROM
# tmp/ladx/base.gbc read through LADXR at the pin, cross-checked, refused on any literal run
# under the strict Nintendo-bytes scope) into <dir> (default tests/ladx/extract; else inside
# tmp/, exit 2 otherwise), PYTHONHASHSEED=0.
# --check renders twice under tmp/ladx-extract/ (A: the setup checkout, PYTHONHASHSEED=0; B: a
# fresh clone in tmp/ladx-oracle/ladxr-extract-b, PYTHONHASHSEED=1) and prints one DIFFERENT,
# MISSING or EXTRA line per file unlike the other run or tests/ladx/extract (exit 1), else
# `extract: 6 files byte-identical on two runs and to tests/ladx/extract`.
# A missing or wrong ROM, an unpinned checkout or python3 older than 3.11 is a `setup:` line,
# exit 2. Needs the network for setup.sh; never called by make.
set -eu
export LC_ALL=C

LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a
FIXTURES="room-entities.txt room-objects.txt boss-sources.txt backgrounds.txt save-layout.txt source-flags.txt"

oracle_dir="$(cd "$(dirname "$0")" && pwd -P)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd -P)"
committed="$repo_dir/tests/ladx/extract"
extract_dir="$repo_dir/tmp/ladx-extract"
rom="$repo_dir/tmp/ladx/base.gbc"

usage() {
	echo "usage: gen_extract.sh [--out <dir>] | --check" >&2
	exit 2
}
setup() {
	echo "setup: gen_extract: $1" >&2
	exit 2
}

out=
check=0
while [ "$#" -gt 0 ]; do
	case "$1" in
	--check) check=1 ;;
	--out)
		[ "$#" -ge 2 ] || usage
		out="$2"
		shift
		;;
	*) usage ;;
	esac
	shift
done
[ "$check" -eq 0 ] || [ -z "$out" ] || usage

command -v python3 >/dev/null 2>&1 || setup "python3 not found"
python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' || setup "python3 is older than 3.11 (run setup.sh)"
[ -f "$rom" ] || setup "no ROM at $rom (run fetch_rom.sh)"
sha1="$(python3 -I -c 'import hashlib, sys; print(hashlib.sha1(open(sys.argv[1], "rb").read()).hexdigest())' "$rom")"
[ "$sha1" = "$LADX_SHA1" ] || setup "ROM SHA-1 $sha1 is not the LADX English 1.0 ROM ($(printf '%.8s' "$LADX_SHA1")…)"

if [ "$check" -eq 0 ]; then
	out="${out:-$committed}"
	mkdir -p "$out"
	out="$(cd "$out" && pwd -P)"
	if [ "$out" != "$committed" ] && ! python3 -I "$oracle_dir/ladx_oracle.py" inside-tmp "$repo_dir" "$out"; then
		echo "gen_extract: refused, $out is neither tests/ladx/extract nor inside $repo_dir/tmp/ (ROM-derived outputs stay under tmp/)" >&2
		exit 2
	fi
fi

# render <ladxr dir> <hash seed> <out dir>: python3 without -I or -E, which would drop PYTHONHASHSEED.
render() {
	PYTHONHASHSEED="$2" PYTHONDONTWRITEBYTECODE=1 \
		python3 "$oracle_dir/extract.py" generate --out "$3" --rom "$rom" --ladxr "$1" </dev/null
}

"$oracle_dir/setup.sh"

if [ "$check" -eq 0 ]; then
	render "$oracle_dir/ladxr" 0 "$out"
	exit 0
fi

dir_a="$extract_dir/check-a"
dir_b="$extract_dir/check-b"
clone_b="$repo_dir/tmp/ladx-oracle/ladxr-extract-b"
rm -rf "$dir_a" "$dir_b"
mkdir -p "$repo_dir/tmp/ladx-oracle"
LADXR_DIR="$clone_b" "$oracle_dir/setup.sh" --fresh
render "$oracle_dir/ladxr" 0 "$dir_a"
render "$clone_b" 1 "$dir_b"

status=0
for dir in "$dir_a" "$dir_b" "$committed"; do
	for path in "$dir"/*; do
		[ -e "$path" ] || continue
		name="${path##*/}"
		case " $FIXTURES " in
		*" $name "*) ;;
		*)
			echo "EXTRA $name"
			status=1
			;;
		esac
	done
done
for name in $FIXTURES; do
	if [ ! -f "$dir_a/$name" ] || [ ! -f "$dir_b/$name" ] || [ ! -f "$committed/$name" ]; then
		echo "MISSING $name"
		status=1
	elif ! cmp -s "$dir_a/$name" "$dir_b/$name" || ! cmp -s "$dir_a/$name" "$committed/$name"; then
		echo "DIFFERENT $name"
		status=1
	fi
done
[ "$status" -eq 0 ] || exit 1
set -- $FIXTURES
echo "extract: $# files byte-identical on two runs and to tests/ladx/extract"
