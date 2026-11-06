#!/bin/sh
# Usage: inventory_trace.sh [--out <dir>] [--cases <file>] [--seeds <file>] [--ladxr <dir>] [--rom <file>] [--work <dir>] [--inventory <file>] [<case>...]
# The inventory's real-data trace: runs each named case (every case when none is named) of the case
# directories tests/ladx/cases/ then tests/ladx/combos/ (ladx_cases.py list), then, when no case is
# named, every case of determinism_cases.txt as det-<name>; or, with --cases, of that case list file
# (determinism_cases.txt's format, names kept). Each runs through oracle_run.py --rom --reads, one
# python3 process per run, PYTHONHASHSEED 0, under its own seed into <dir>/<case>/seed-0/ and
# under the k-th seed of --seeds (default side_seeds.txt) into <dir>/<case>/seed-<k>/, with the
# same settings; prints `<case> seed-<k>: outcome <ok|error|crash>` per run, then runs
# read_check.py on <dir> with the inventory (default notes/ladx-inventory.md) and exits with its
# code. <dir> (default tmp/ladx-inventory) must lie inside the repository's tmp/, else exit 2 with
# nothing written. A missing ROM (default tmp/ladx/base.gbc) or a wrong one, or a missing or
# unpinned checkout (default tests/ladx/oracle/ladxr), is a `setup:` line and exit 2. Never
# called by make.
set -eu
export LC_ALL=C

LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef
LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd -P)"

usage() {
	echo "usage: inventory_trace.sh [--out <dir>] [--cases <file>] [--seeds <file>] [--ladxr <dir>] [--rom <file>] [--work <dir>] [--inventory <file>] [<case>...]" >&2
	exit 2
}
setup() {
	echo "setup: inventory_trace: $1"
	exit 2
}

out="$repo_dir/tmp/ladx-inventory"
cases=
determinism="$oracle_dir/determinism_cases.txt"
seeds="$oracle_dir/side_seeds.txt"
ladxr="$oracle_dir/ladxr"
rom="$repo_dir/tmp/ladx/base.gbc"
work="$repo_dir/tmp/ladx-oracle"
inventory="$repo_dir/notes/ladx-inventory.md"
while [ "$#" -gt 0 ]; do
	case "$1" in
	--out | --cases | --seeds | --ladxr | --rom | --work | --inventory)
		[ "$#" -ge 2 ] || usage
		case "$1" in
		--out) out="$2" ;;
		--cases) cases="$2" ;;
		--seeds) seeds="$2" ;;
		--ladxr) ladxr="$2" ;;
		--rom) rom="$2" ;;
		--work) work="$2" ;;
		--inventory) inventory="$2" ;;
		esac
		shift
		;;
	-*) usage ;;
	*) break ;;
	esac
	shift
done

command -v python3 >/dev/null 2>&1 || setup "python3 not found"
oracle="python3 -I $oracle_dir/ladx_oracle.py"
if ! $oracle inside-tmp "$repo_dir" "$out"; then
	echo "inventory_trace: refused, --out $out is not inside $repo_dir/tmp/ (ROM-derived outputs stay under tmp/)"
	exit 2
fi
if [ -n "$cases" ]; then
	[ -f "$cases" ] || usage
	case_lines="$($oracle cases "$cases" "$@")" || usage
else
	case_lines="$(python3 -I "$oracle_dir/ladx_cases.py" list "$@")" || usage
	if [ "$#" -eq 0 ]; then
		det_lines="$($oracle cases "$determinism")" || usage
		case_lines="$(printf '%s\n%s\n' "$case_lines" "$det_lines" | grep -v '^$' |
			awk -v n="$(printf '%s\n' "$case_lines" | grep -c .)" 'NR > n { $0 = "det-" $0 } { print }')"
	fi
fi
[ -f "$seeds" ] || usage
side_seeds="$(grep -v '^#' "$seeds" | grep -v '^$' || true)"
for seed in $side_seeds; do
	printf '%s\n' "$seed" | grep -Eqx '([0-9a-f][0-9a-f])+' || {
		echo "inventory_trace: bad seed line $seed in $seeds: want hex bytes"
		exit 2
	}
done
[ -n "$side_seeds" ] || {
	echo "inventory_trace: no seed in $seeds"
	exit 2
}
[ -f "$inventory" ] || setup "no inventory $inventory"

[ -f "$rom" ] || setup "no ROM at $rom (run fetch_rom.sh)"
python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' || setup "python3 is older than 3.11 (run setup.sh)"
command -v git >/dev/null 2>&1 || setup "git not found"
[ -f "$ladxr/randomizer.py" ] || setup "$ladxr is not a LADXR checkout (run setup.sh)"
head="$(git -C "$ladxr" rev-parse HEAD 2>/dev/null || true)"
[ "$head" = "$LADXR_PIN" ] || setup "LADXR checkout $ladxr is not at the pin $(printf '%.8s' "$LADXR_PIN") (run setup.sh)"
sha1="$(python3 -I -c 'import hashlib, sys; print(hashlib.sha1(open(sys.argv[1], "rb").read()).hexdigest())' "$rom")"
[ "$sha1" = "$LADX_SHA1" ] || setup "ROM SHA-1 $sha1 is not the LADX English 1.0 ROM ($(printf '%.8s' "$LADX_SHA1")…)"

mkdir -p "$out"
out="$(cd "$out" && pwd -P)"
while read -r name seed settings; do
	rm -rf "${out:?}/$name"
	k=0
	for run_seed in $seed $side_seeds; do
		# python3 without -I or -E, which would drop PYTHONHASHSEED.
		# shellcheck disable=SC2086
		PYTHONHASHSEED=0 PYTHONDONTWRITEBYTECODE=1 python3 "$oracle_dir/oracle_run.py" \
			--rom "$rom" --dir "$out/$name/seed-$k" --work "$work" --reads \
			--inventory "$inventory" "$ladxr" "$run_seed" $settings </dev/null
		outcome="$(tail -n 1 "$out/$name/seed-$k/run.txt" | cut -d ' ' -f 2)"
		echo "$name seed-$k: outcome $outcome"
		k=$((k + 1))
	done
done <<EOF
$case_lines
EOF
python3 -I "$oracle_dir/read_check.py" --ladxr "$ladxr" --inventory "$inventory" --traces "$out"
