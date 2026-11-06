#!/bin/sh
# Usage: gen_fixtures.sh [--out <dir>] [--check]
# Runs setup.sh, then every fixture generator against its checkout, writing the fixtures into <dir>
# (default tests/ladx/oracle). --check renders twice under tmp/ladx-oracle/ (A: the setup checkout,
# PYTHONHASHSEED=0; B: a fresh clone in another directory, PYTHONHASHSEED=1) and compares both with
# each other and with the fixtures of <dir>. Needs the network; never called by make test or check.
set -eu
export LC_ALL=C

FIXTURES="pyrandom_vectors.txt settings.txt settings_validate.txt item_pools.txt"

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd)"
work_root="$repo_dir/tmp/ladx-oracle"

usage() {
	echo "usage: gen_fixtures.sh [--out <dir>] [--check]" >&2
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
if [ -n "$out" ]; then
	mkdir -p "$out"
	out="$(cd "$out" && pwd)"
fi

# render <ladxr dir> <hash seed> <out dir>: python3 without -I or -E, which would drop PYTHONHASHSEED.
render() {
	mkdir -p "$3" "$work_root"
	work="$(mktemp -d "$work_root/run.XXXXXX")"
	for generator in gen_pyrandom.py gen_settings.py gen_pools.py; do
		(cd "$work" && PYTHONHASHSEED="$2" PYTHONDONTWRITEBYTECODE=1 \
			python3 "$oracle_dir/$generator" "$1" "$3")
	done
	rm -rf "$work"
}

"$oracle_dir/setup.sh"

if [ "$check" -eq 0 ]; then
	dest="${out:-$oracle_dir}"
	render "$oracle_dir/ladxr" 0 "$dest"
	set -- $FIXTURES
	echo "fixtures: $# files written to $dest"
	exit 0
fi

reference="${out:-$oracle_dir}"
shown="${out:-tests/ladx/oracle}"
dir_a="$work_root/check-a"
dir_b="$work_root/check-b"
clone_b="$work_root/ladxr-b"
rm -rf "$dir_a" "$dir_b"
LADXR_DIR="$clone_b" "$oracle_dir/setup.sh" --fresh
render "$oracle_dir/ladxr" 0 "$dir_a"
render "$clone_b" 1 "$dir_b"

status=0
for dir in "$dir_a" "$dir_b"; do
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
	if [ ! -f "$dir_a/$name" ] || [ ! -f "$dir_b/$name" ] || [ ! -f "$reference/$name" ]; then
		echo "MISSING $name"
		status=1
	elif ! cmp -s "$dir_a/$name" "$dir_b/$name" || ! cmp -s "$dir_a/$name" "$reference/$name"; then
		echo "DIFFERENT $name"
		status=1
	fi
done
[ "$status" -eq 0 ] || exit 1
set -- $FIXTURES
echo "fixtures: $# files byte-identical on two runs and to $shown"
