#!/bin/sh
# Usage: measure.sh [--rom <rom> --patch <ladx_patch> [--qemu <arm ladx_patch>]] <ladx_gen> <out dir>
# Times ladx_gen --time (the run stopped after the fill) on every case and combo and, with --rom,
# ladx_patch --time (the whole pipeline); projects the slowest onto the Cortex-A7 with
# tests/alttp/measure.sh's factors. Contract in tests/ladx/README.md, "Measure". Run from tests/ladx.
set -eu
export LC_ALL=C

die() {
	echo "measure: $*" >&2
	exit 1
}

usage() {
	echo "usage: measure.sh [--rom <rom> --patch <ladx_patch> [--qemu <arm ladx_patch>]] <ladx_gen> <out dir>" >&2
	exit 2
}

rom=
patch=
qemu_patch=
while [ "$#" -gt 0 ]; do
	case "$1" in
	--rom | --patch | --qemu)
		[ "$#" -ge 2 ] || usage
		case "$1" in --rom) rom="$2" ;; --patch) patch="$2" ;; --qemu) qemu_patch="$2" ;; esac
		shift 2
		;;
	-*) usage ;;
	*) break ;;
	esac
done
[ "$#" -eq 2 ] || usage
[ -z "$rom" ] && { [ -n "$patch" ] || [ -n "$qemu_patch" ]; } && usage
[ -n "$rom" ] && [ -z "$patch" ] && usage
gen="$1"
dir="$2"
reps=3
budget_ms=10000
tables=../../data/ladx
[ -x "$gen" ] || die "no generator at $gen"
lo="$(sed -n 's/^a7_factor_lo=\([0-9][0-9]*\)$/\1/p' ../alttp/measure.sh)"
hi="$(sed -n 's/^a7_factor_hi=\([0-9][0-9]*\)$/\1/p' ../alttp/measure.sh)"
[ -n "$lo" ] && [ -n "$hi" ] || die "no a7_factor_lo/hi in tests/alttp/measure.sh"
if [ -n "$rom" ]; then
	[ -f "$rom" ] || die "no ROM at $rom"
	[ -x "$patch" ] || die "no generator at $patch"
	if [ -n "$qemu_patch" ]; then
		[ -f "$qemu_patch" ] || die "no ARM generator at $qemu_patch"
		command -v qemu-arm >/dev/null 2>&1 || die "setup: qemu-arm not found on PATH (install qemu-user)"
	fi
fi
mkdir -p "$dir"
times="$dir/times"
pipe="$dir/pipeline"
qemu="$dir/qemu"
: >"$times"
: >"$pipe"
: >"$qemu"

case_dirs() {
	for sub in cases combos; do
		for case_dir in "$sub"/*/; do
			case_dir="${case_dir%/}"
			[ -f "$case_dir/seed" ] && echo "$case_dir"
		done
	done
}

patch_run() {
	out="$dir/out.gbc"
	rm -f "$out"
	# shellcheck disable=SC2046
	"$@" --base "$rom" --out "$out" --tables "$tables" --seed "$(cat "$case_dir/seed")" --time $(cat "$case_dir/config.txt") \
		>"$dir/patch.out" 2>"$dir/gen.err" || die "ladx_patch $case_dir failed: $(head -n 1 "$dir/gen.err")"
	t_asm="$(sed -n 's/^time\.assemble \([0-9][0-9]*\) us$/\1/p' "$dir/patch.out")"
	t_all="$(sed -n 's/^time\.total \([0-9][0-9]*\) us$/\1/p' "$dir/patch.out")"
	rss="$(sed -n 's/^maxrss \([0-9][0-9]*\) KiB$/\1/p' "$dir/patch.out")"
	[ -n "$t_asm" ] && [ -n "$t_all" ] && [ -n "$rss" ] || die "ladx_patch --time $case_dir printed no time"
	want="$(sed -n 's/^rom_sha1 //p' "$case_dir/run.txt")"
	got="$(sha1sum "$out" | cut -c1-40)"
	rm -f "$out"
	[ "$got" = "$want" ] || die "ladx_patch $case_dir: output SHA-1 $got, golden $want"
}

for case_dir in $(case_dirs); do
	best=
	i=0
	while [ "$i" -lt "$reps" ]; do
		us="$("$gen" --time "$case_dir" 2>"$dir/gen.err")" || die "ladx_gen --time $case_dir failed: $(head -n 1 "$dir/gen.err")"
		case "$us" in '' | *[!0-9]*) die "ladx_gen --time $case_dir printed no time" ;; esac
		if [ -z "$best" ] || [ "$us" -lt "$best" ]; then best="$us"; fi
		i=$((i + 1))
	done
	echo "${case_dir#*/} $best" >>"$times"
	[ -n "$rom" ] || continue
	b_asm=
	b_all=
	b_rss=0
	i=0
	while [ "$i" -lt "$reps" ]; do
		patch_run "$patch"
		if [ -z "$b_asm" ] || [ "$t_asm" -lt "$b_asm" ]; then b_asm="$t_asm"; fi
		if [ -z "$b_all" ] || [ "$t_all" -lt "$b_all" ]; then b_all="$t_all"; fi
		if [ "$rss" -gt "$b_rss" ]; then b_rss="$rss"; fi
		i=$((i + 1))
	done
	echo "${case_dir#*/} $b_asm $b_all $b_rss" >>"$pipe"
	[ -n "$qemu_patch" ] || continue
	patch_run qemu-arm "$qemu_patch"
	echo "${case_dir#*/} $t_asm $t_all $rss" >>"$qemu"
done
[ -s "$times" ] || die "no case under cases/ or combos/"

probe="$dir/sizeof_randomizer"
printf '#include "ladx_assemble.h"\n#include <stdio.h>\nint main(void)\n{\n\tprintf("%%lu %%lu\\n", (unsigned long)sizeof(LadxRandomizer), (unsigned long)sizeof(LadxAssembler));\n\treturn 0;\n}\n' >"$probe.c"
${CC:-cc} -std=c99 -I../../ladx/include -o "$probe" "$probe.c" || die "cannot build the sizeof probe"
sizes="$("$probe")" || die "the sizeof probe failed"
state="${sizes% *}"
assembler="${sizes#* }"

stats() {
	sort -k"$2,$2n" "$1" | awk -v col="$2" -v label="$3" '
	{ t[NR] = $col; c[NR] = $1; s += $col }
	END {
		med = NR % 2 ? t[(NR + 1) / 2] : int((t[NR / 2] + t[NR / 2 + 1]) / 2 + 0.5)
		printf "%s.median %d us\n%s.max %d us %s\n%s.total %d us\n", label, med, label, t[NR], c[NR], label, s
	}'
}

max() {
	sort -k"$2,$2n" "$1" | awk -v col="$2" 'END { print $col }'
}

projected() {
	awk -v us="$1" -v lo="$lo" -v hi="$hi" -v budget="$budget_ms" -v label="$2" 'BEGIN {
		a = int(us * lo / 1000 + 0.5); b = int(us * hi / 1000 + 0.5)
		printf "%s %d..%d ms (host x %d..%d) budget %d%s\n", label, a, b, lo, hi, budget, (b > budget ? " over" : "")
	}'
}

report="$dir/report.txt"
{
	awk '{ printf "fill.time %s %d us\n", $1, $2 }' "$times"
	echo "fill.time.cases $(wc -l <"$times") (each the minimum of $reps runs of ladx_gen --time, -O2)"
	stats "$times" 2 fill.time
	echo "fill.state.bytes $state (sizeof LadxRandomizer)"
	echo "miyoo.note Cortex-A7 factor $lo..$hi is tests/alttp/measure.sh's assumption: a device timing replaces it"
	projected "$(max "$times" 2)" miyoo.fill.projected
	if [ -n "$rom" ]; then
		awk '{ printf "pipeline.time %s %d us\n", $1, $2 }' "$pipe"
		echo "pipeline.time.cases $(wc -l <"$pipe") (each the minimum of $reps runs of ladx_patch --time, -O2, every output's SHA-1 the golden's)"
		stats "$pipe" 2 pipeline.time
		stats "$pipe" 3 pipeline.process
		echo "pipeline.maxrss.max $(sort -k4,4n "$pipe" | awk 'END { printf "%d KiB %s", $4, $1 }')"
		echo "pipeline.state.bytes $assembler (sizeof LadxAssembler)"
		projected "$(max "$pipe" 3)" miyoo.pipeline.projected
		if [ -n "$qemu_patch" ]; then
			echo "qemu.time.cases $(wc -l <"$qemu") (one run each of Onion's ladx_patch --time under qemu-arm)"
			stats "$qemu" 2 qemu.time
			stats "$qemu" 3 qemu.process
			echo "qemu.maxrss.max $(sort -k4,4n "$qemu" | awk 'END { printf "%d KiB %s", $4, $1 }') (qemu-arm's own RSS included)"
			awk -v q="$(max "$qemu" 3)" -v h="$(max "$pipe" 3)" 'BEGIN { printf "qemu.ratio %.1f (slowest process under qemu-arm over the slowest on the host)\n", q / h }'
		fi
	fi
} >"$report"
cat "$report"
