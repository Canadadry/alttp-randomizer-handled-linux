#!/bin/sh
# Usage: gen_golden.sh --out <dir> [--cases <file>] [--ladxr <dir>] [--hashseed <n>] [--rom <file>] [--work <dir>] [<case>...]
# LADX's case runner: runs each named case (every case when none is named) of the case directories
# tests/ladx/cases/ then tests/ladx/combos/ (ladx_cases.py list), or with --cases of that case list
# file (determinism_cases.txt's format), through oracle_run.py --rom, one python3 process per case, with
# PYTHONHASHSEED <n> (default 0) and bytecode writing off, on the LADXR checkout (default
# tests/ladx/oracle/ladxr, at the pin), the ROM (default tmp/ladx/base.gbc) and the work root under
# which LADXR runs (default tmp/ladx-oracle/). It writes <dir>/<case>/ (run.txt, meta.txt, and
# out.gbc and spoiler.json when LADXR wrote them), prints `<case>: outcome <ok|error|crash>` per
# case, then `gen_golden: <n> cases written to <dir>`; without --cases it then exits 1 with
# `gen_golden: <n> cases not ok` when a case's outcome is not ok. <dir> must lie inside the repository's tmp/
# (ROM-derived outputs stay there), else exit 2 with nothing written. A missing or wrong ROM, or a
# missing or unpinned checkout, is a `setup:` line and exit 2. Never called by make.
set -eu
export LC_ALL=C

LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef
LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd -P)"

usage() {
	echo "usage: gen_golden.sh --out <dir> [--cases <file>] [--ladxr <dir>] [--hashseed <n>] [--rom <file>] [--work <dir>] [<case>...]" >&2
	exit 2
}
setup() {
	echo "setup: gen_golden: $1" >&2
	exit 2
}

out=
cases=
ladxr="$oracle_dir/ladxr"
hashseed=0
rom="$repo_dir/tmp/ladx/base.gbc"
work="$repo_dir/tmp/ladx-oracle"
while [ "$#" -gt 0 ]; do
	case "$1" in
	--out | --cases | --ladxr | --hashseed | --rom | --work)
		[ "$#" -ge 2 ] || usage
		case "$1" in
		--out) out="$2" ;;
		--cases) cases="$2" ;;
		--ladxr) ladxr="$2" ;;
		--hashseed) hashseed="$2" ;;
		--rom) rom="$2" ;;
		--work) work="$2" ;;
		esac
		shift
		;;
	-*) usage ;;
	*) break ;;
	esac
	shift
done
[ -n "$out" ] || usage
case "$hashseed" in "" | *[!0-9]*) usage ;; esac

command -v python3 >/dev/null 2>&1 || setup "python3 not found"
oracle="python3 -I $oracle_dir/ladx_oracle.py"
if ! $oracle inside-tmp "$repo_dir" "$out"; then
	echo "gen_golden: refused, --out $out is not inside $repo_dir/tmp/ (ROM-derived outputs stay under tmp/)" >&2
	exit 2
fi
if [ -n "$cases" ]; then
	[ -f "$cases" ] || usage
	case_lines="$($oracle cases "$cases" "$@")" || usage
else
	case_lines="$(python3 -I "$oracle_dir/ladx_cases.py" list "$@")" || usage
fi
[ -n "$case_lines" ] || {
	echo "gen_golden: no cases" >&2
	exit 2
}

python3 -c 'import sys; sys.exit(sys.version_info < (3, 11))' || setup "python3 is older than 3.11 (run setup.sh)"
command -v git >/dev/null 2>&1 || setup "git not found"
[ -f "$ladxr/randomizer.py" ] || setup "$ladxr is not a LADXR checkout (run setup.sh)"
head="$(git -C "$ladxr" rev-parse HEAD 2>/dev/null || true)"
[ "$head" = "$LADXR_PIN" ] || setup "LADXR checkout $ladxr is not at the pin $(printf '%.8s' "$LADXR_PIN") (run setup.sh)"
[ -f "$rom" ] || setup "no ROM at $rom (run fetch_rom.sh)"
sha1="$(python3 -I -c 'import hashlib, sys; print(hashlib.sha1(open(sys.argv[1], "rb").read()).hexdigest())' "$rom")"
[ "$sha1" = "$LADX_SHA1" ] || setup "ROM SHA-1 $sha1 is not the LADX English 1.0 ROM ($(printf '%.8s' "$LADX_SHA1")…)"

mkdir -p "$out"
out="$(cd "$out" && pwd -P)"
count=0
not_ok=0
while read -r name seed settings; do
	rm -rf "${out:?}/$name"
	# python3 without -I or -E, which would drop PYTHONHASHSEED.
	# shellcheck disable=SC2086
	PYTHONHASHSEED="$hashseed" PYTHONDONTWRITEBYTECODE=1 python3 "$oracle_dir/oracle_run.py" \
		--rom "$rom" --dir "$out/$name" --work "$work" "$ladxr" "$seed" $settings </dev/null
	outcome="$(tail -n 1 "$out/$name/run.txt" | cut -d ' ' -f 2)"
	echo "$name: outcome $outcome"
	[ "$outcome" = ok ] || not_ok=$((not_ok + 1))
	count=$((count + 1))
done <<EOF
$case_lines
EOF
echo "gen_golden: $count cases written to ${out#"$repo_dir/"}"
if [ -z "$cases" ] && [ "$not_ok" -gt 0 ]; then
	echo "gen_golden: $not_ok cases not ok" >&2
	exit 1
fi
