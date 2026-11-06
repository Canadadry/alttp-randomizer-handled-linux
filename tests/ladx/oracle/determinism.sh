#!/bin/sh
# Usage: determinism.sh
# Runs every case of determinism_cases.txt through oracle_run.py twice (A: the setup checkout,
# PYTHONHASHSEED=0; B: a fresh clone in another directory, PYTHONHASHSEED=1) and compares the two
# outputs: one `same <case>` or `DIFFERENT <case>: line <n>: <A> | <B>` line per case, then a
# summary line. It first proves its comparator on planted runs, and fails unless every case's two
# runs saw another hash("ladxr") and another checkout. Needs the network; never called by make.
set -eu
export LC_ALL=C

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd)"
work_root="$repo_dir/tmp/ladx-oracle"
out_root="$work_root/determinism"
clone_b="$work_root/ladxr-determinism"
compare="python3 -I $oracle_dir/ladx_oracle.py"

[ "$#" -eq 0 ] || {
	echo "usage: determinism.sh" >&2
	exit 2
}

rm -rf "$out_root"
mkdir -p "$out_root/proof" "$out_root/a" "$out_root/b"
printf 'seed 00\ntrace 0 rnd x:y choice 3 -> 1 [1]\noutcome ok\n' >"$out_root/proof/a.txt"
printf 'seed 00\ntrace 0 rnd x:y choice 3 -> 2 [1]\noutcome ok\n' >"$out_root/proof/b.txt"
if proof="$($compare compare planted "$out_root/proof/a.txt" "$out_root/proof/b.txt")" ||
	[ "$proof" != "DIFFERENT planted: line 2: trace 0 rnd x:y choice 3 -> 1 [1] | trace 0 rnd x:y choice 3 -> 2 [1]" ]; then
	echo "determinism: self-proof failed: the comparator did not catch a planted trace difference" >&2
	exit 1
fi

"$oracle_dir/setup.sh"
LADXR_DIR="$clone_b" "$oracle_dir/setup.sh" --fresh

# run <ladxr dir> <hash seed> <out dir> <case> <seed> <settings...>: python3 without -I or -E,
# which would drop PYTHONHASHSEED.
run() {
	ladxr="$1" hashseed="$2" dir="$3" name="$4" seed="$5"
	shift 5
	work="$(mktemp -d "$work_root/run.XXXXXX")"
	(cd "$work" && PYTHONHASHSEED="$hashseed" PYTHONDONTWRITEBYTECODE=1 \
		python3 "$oracle_dir/oracle_run.py" --out "$dir/$name.txt" --meta "$dir/$name.meta" \
		"$ladxr" "$seed" "$@")
	rm -rf "$work"
}

status=0
count=0
names=
while read -r name seed settings; do
	case "$name" in "" | "#"*) continue ;; esac
	# shellcheck disable=SC2086
	run "$oracle_dir/ladxr" 0 "$out_root/a" "$name" "$seed" $settings
	# shellcheck disable=SC2086
	run "$clone_b" 1 "$out_root/b" "$name" "$seed" $settings
	$compare compare "$name" "$out_root/a/$name.txt" "$out_root/b/$name.txt" || status=1
	count=$((count + 1))
	names="$names $name"
done <"$oracle_dir/determinism_cases.txt"

for name in $names; do
	if ! $compare varied "$out_root/a/$name.meta" "$out_root/b/$name.meta"; then
		echo "determinism: the two runs did not vary ($name)"
		status=1
	fi
done
[ "$status" -eq 0 ] || exit 1
echo "determinism: $count cases identical (PYTHONHASHSEED 0 vs 1, tests/ladx/oracle/ladxr vs ${clone_b#"$repo_dir/"})"
