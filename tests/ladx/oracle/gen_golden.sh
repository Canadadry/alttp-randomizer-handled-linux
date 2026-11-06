#!/bin/sh
# Usage: gen_golden.sh [--out <dir> | --root <dir>] [--mirror <dir>] [--cases <file>] [--ladxr <dir>]
#                      [--hashseed <n>] [--rom <file>] [--work <dir>] [<case>...]
# LADX's case runner: runs each named case (every case when none is named) of the case directories
# <root>/cases/ then <root>/combos/ (ladx_cases.py list), or with --cases (--out only) of that case
# list file (determinism_cases.txt's format), through oracle_run.py --rom --reads, one python3
# process per case, with PYTHONHASHSEED <n> (default 0) and bytecode writing off, on the LADXR
# checkout (default tests/ladx/oracle/ladxr, at the pin), the ROM (default tmp/ladx/base.gbc) and
# the work root under which LADXR runs (default tmp/ladx-oracle/), then encodes each ok case's
# write log (write_log.py encode, its I lines from the re-run record invariant.txt). With --out it
# writes <dir>/<case>/ (run.txt, meta.txt, events.txt, invariant.txt, reads.txt, writes.txt, and
# out.gbc, spoiler.json and rom_writes.txt when written).
# Without --out it writes the goldens: every file into the mirror (default tmp/ladx-goldens/)
# <mirror>/<case>/, then into the case folder <root>/<cases|combos>/<case>/ (default root
# tests/ladx) only run.txt, rom_writes.txt and spoiler.json (when written), replacing its old
# ones; a case whose outcome is not ok keeps its folder untouched. It prints `<case>: outcome
# <ok|error|crash>` per case, then `gen_golden: <n> cases written to <dir>` (or `<n> goldens
# written to <root>`); without --cases it then exits 1 with `gen_golden: <n> cases not ok` when a
# case's outcome is not ok or its log does not encode. --out and --mirror must lie inside the
# repository's tmp/ (ROM-derived outputs stay there), --root must be tests/ladx or inside tmp/,
# else exit 2 with nothing written. A missing or wrong ROM, or a missing or unpinned checkout, is
# a `setup:` line and exit 2. Never called by make.
set -eu
export LC_ALL=C

LADXR_PIN=4325207e4e55076e05f209fbdf16b860a90e42ef
LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd -P)"

usage() {
	echo "usage: gen_golden.sh [--out <dir> | --root <dir>] [--mirror <dir>] [--cases <file>] [--ladxr <dir>] [--hashseed <n>] [--rom <file>] [--work <dir>] [<case>...]" >&2
	exit 2
}
setup() {
	echo "setup: gen_golden: $1" >&2
	exit 2
}

out=
root=
mirror="$repo_dir/tmp/ladx-goldens"
cases=
ladxr="$oracle_dir/ladxr"
hashseed=0
rom="$repo_dir/tmp/ladx/base.gbc"
work="$repo_dir/tmp/ladx-oracle"
while [ "$#" -gt 0 ]; do
	case "$1" in
	--out | --root | --mirror | --cases | --ladxr | --hashseed | --rom | --work)
		[ "$#" -ge 2 ] || usage
		case "$1" in
		--out) out="$2" ;;
		--root) root="$2" ;;
		--mirror) mirror="$2" ;;
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
if [ -n "$out" ]; then
	[ -z "$root" ] || usage
else
	[ -z "$cases" ] || usage
	root="${root:-$repo_dir/tests/ladx}"
fi
case "$hashseed" in "" | *[!0-9]*) usage ;; esac

command -v python3 >/dev/null 2>&1 || setup "python3 not found"
oracle="python3 -I $oracle_dir/ladx_oracle.py"
if ! $oracle inside-tmp "$repo_dir" "${out:-$mirror}"; then
	echo "gen_golden: refused, ${out:-$mirror} is not inside $repo_dir/tmp/ (ROM-derived outputs stay under tmp/)" >&2
	exit 2
fi
if [ -n "$root" ]; then
	[ -d "$root" ] || usage
	root="$(cd "$root" && pwd -P)"
	if [ "$root" != "$repo_dir/tests/ladx" ] && ! $oracle inside-tmp "$repo_dir" "$root"; then
		echo "gen_golden: refused, --root $root is neither tests/ladx nor inside $repo_dir/tmp/" >&2
		exit 2
	fi
fi
if [ -n "$cases" ]; then
	[ -f "$cases" ] || usage
	case_lines="$($oracle cases "$cases" "$@")" || usage
else
	case_lines="$(python3 -I "$oracle_dir/ladx_cases.py" list ${root:+--root "$root"} "$@")" || usage
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

run_dir="${out:-$mirror}"
mkdir -p "$run_dir"
run_dir="$(cd "$run_dir" && pwd -P)"
count=0
not_ok=0
while read -r name seed settings; do
	rm -rf "${run_dir:?}/$name"
	# python3 without -I or -E, which would drop PYTHONHASHSEED.
	# shellcheck disable=SC2086
	PYTHONHASHSEED="$hashseed" PYTHONDONTWRITEBYTECODE=1 python3 "$oracle_dir/oracle_run.py" \
		--rom "$rom" --dir "$run_dir/$name" --work "$work" --reads "$ladxr" "$seed" $settings </dev/null
	outcome="$(tail -n 1 "$run_dir/$name/run.txt" | cut -d ' ' -f 2)"
	if [ "$outcome" = ok ] && ! python3 -I -B "$oracle_dir/write_log.py" encode \
		--events "$run_dir/$name/events.txt" --invariant "$run_dir/$name/invariant.txt" \
		--rom "$rom" --out "$run_dir/$name/rom_writes.txt"; then
		outcome="ok, encode failed"
	fi
	echo "$name: outcome $outcome"
	count=$((count + 1))
	if [ "$outcome" != ok ]; then
		not_ok=$((not_ok + 1))
		continue
	fi
	if [ -z "$out" ]; then
		folder="$root/cases/$name"
		[ -d "$folder" ] || folder="$root/combos/$name"
		rm -f "$folder/run.txt" "$folder/rom_writes.txt" "$folder/spoiler.json"
		for file in run.txt rom_writes.txt spoiler.json; do
			[ ! -f "$run_dir/$name/$file" ] || cp "$run_dir/$name/$file" "$folder/$file"
		done
	fi
done <<EOF
$case_lines
EOF
if [ -n "$out" ]; then
	echo "gen_golden: $count cases written to ${out#"$repo_dir/"}"
else
	echo "gen_golden: $((count - not_ok)) goldens written to ${root#"$repo_dir/"}"
fi
if [ -z "$cases" ] && [ "$not_ok" -gt 0 ]; then
	echo "gen_golden: $not_ok cases not ok" >&2
	exit 1
fi
