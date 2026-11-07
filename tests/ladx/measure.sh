#!/bin/sh
# Usage: measure.sh <ladx_gen> <out dir>
# Times ladx_gen --time (the run stopped after the fill) on every case and combo, and projects
# the slowest onto the Cortex-A7 with tests/alttp/measure.sh's factors. Contract in
# tests/ladx/README.md, "Measure". Run from tests/ladx.
set -eu
export LC_ALL=C

die() {
	echo "measure: $*" >&2
	exit 1
}

[ "$#" -eq 2 ] || { echo "usage: measure.sh <ladx_gen> <out dir>" >&2; exit 2; }
gen="$1"
dir="$2"
reps=3
budget_ms=10000
[ -x "$gen" ] || die "no generator at $gen"
lo="$(sed -n 's/^a7_factor_lo=\([0-9][0-9]*\)$/\1/p' ../alttp/measure.sh)"
hi="$(sed -n 's/^a7_factor_hi=\([0-9][0-9]*\)$/\1/p' ../alttp/measure.sh)"
[ -n "$lo" ] && [ -n "$hi" ] || die "no a7_factor_lo/hi in tests/alttp/measure.sh"
mkdir -p "$dir"
times="$dir/times"
: >"$times"

for sub in cases combos; do
	for case_dir in "$sub"/*/; do
		case_dir="${case_dir%/}"
		[ -f "$case_dir/seed" ] || continue
		best=
		i=0
		while [ "$i" -lt "$reps" ]; do
			us="$("$gen" --time "$case_dir" 2>"$dir/gen.err")" || die "ladx_gen --time $case_dir failed: $(head -n 1 "$dir/gen.err")"
			case "$us" in '' | *[!0-9]*) die "ladx_gen --time $case_dir printed no time" ;; esac
			if [ -z "$best" ] || [ "$us" -lt "$best" ]; then best="$us"; fi
			i=$((i + 1))
		done
		echo "${case_dir#*/} $best" >>"$times"
	done
done
[ -s "$times" ] || die "no case under cases/ or combos/"

probe="$dir/sizeof_randomizer"
printf '#include "ladx_randomizer.h"\n#include <stdio.h>\nint main(void)\n{\n\tprintf("%%lu\\n", (unsigned long)sizeof(LadxRandomizer));\n\treturn 0;\n}\n' >"$probe.c"
${CC:-cc} -std=c99 -I../../ladx/include -o "$probe" "$probe.c" || die "cannot build the sizeof probe"
state="$("$probe")" || die "the sizeof probe failed"

report="$dir/report.txt"
{
	awk '{ printf "fill.time %s %d us\n", $1, $2 }' "$times"
	sort -k2,2n "$times" | awk -v reps="$reps" -v state="$state" -v lo="$lo" -v hi="$hi" -v budget="$budget_ms" '
	function ms(us, f) { return int(us * f / 1000 + 0.5) }
	{ t[NR] = $2; c[NR] = $1 }
	END {
		med = NR % 2 ? t[(NR + 1) / 2] : int((t[NR / 2] + t[NR / 2 + 1]) / 2 + 0.5)
		printf "fill.time.cases %d (each the minimum of %d runs of ladx_gen --time, -O2)\n", NR, reps
		printf "fill.time.median %d us\n", med
		printf "fill.time.max %d us %s\n", t[NR], c[NR]
		printf "fill.time.total %d us\n", total()
		printf "fill.state.bytes %d (sizeof LadxRandomizer)\n", state
		printf "miyoo.note Cortex-A7 factor %d..%d is tests/alttp/measure.sh'"'"'s assumption: a device timing replaces it\n", lo, hi
		printf "miyoo.fill.projected %d..%d ms (host x %d..%d) budget %d%s\n", ms(t[NR], lo), ms(t[NR], hi), lo, hi, budget, (ms(t[NR], hi) > budget ? " over" : "")
	}
	function total(   i, s) { for (i = 1; i <= NR; i++) s += t[i]; return s }'
} >"$report"
cat "$report"
