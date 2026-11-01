#!/bin/sh
# Usage: measure.sh
# Run by `make measure`, which first builds the host objects under MEASURE_DIR
# (default build/measure): host-O2/ and host-Os/, without RANDO_FAULTS. CC names
# the host compiler. The ARM builds are compiled here, each toolchain only when
# found. MEASURE_REPS (default 5) runs are timed per case, MEASURE_SWEEP
# (default 1024) seeds are swept, both through $MEASURE_DIR/rando_measure.
# Prints the report on stdout and writes it to $MEASURE_DIR/report.txt.
# Exit 0 whatever the figures; 1 when a measurement cannot be taken.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
rando="$root/rando"
cc="${CC:-gcc}"
dir="${MEASURE_DIR:-$here/build/measure}"
case "$dir" in
/*) ;;
*) dir="$PWD/$dir" ;;
esac
jobs="$(nproc 2>/dev/null || echo 4)"

code_budget=131072
state_budget=32768
probes="RandoGen RandoOptions RandoPrng RandoPools RandoWorld RandoItems RandoConfig RandoFillState"

mkdir -p "$dir"
report="$dir/report.txt"
error=
finish() {
	cat "$report" >&3
	if [ -n "$error" ]; then
		echo "measure: $error" >&2
		exit 1
	fi
}
exec 3>&1 >"$report"
trap finish EXIT

die() {
	error="$1"
	exit 1
}

lib_srcs="$(cd "$rando/src" && find . -name '*.c' | sed 's|^\./||' | sort)"
[ -n "$lib_srcs" ] || die "no library source in $rando/src"

# figure <name> <value> <unit> [<budget>]: one report line, "over" past the budget.
figure() {
	if [ "$#" -ge 4 ]; then
		if [ "$2" -gt "$4" ]; then
			echo "$1 $2 $3 budget $4 over"
		else
			echo "$1 $2 $3 budget $4"
		fi
	else
		echo "$1 $2 $3"
	fi
}

# compile_lib <build> <compiler> <flag>...: the library and the size probe into
# $dir/<build>/, from scratch; dies with the compiler's messages on a failure.
compile_lib() {
	b="$dir/$1"
	c="$2"
	shift 2
	rm -rf "$b"
	mkdir -p "$b"
	n=0
	for s in $lib_srcs; do
		o="$b/obj/${s%.c}.o"
		mkdir -p "${o%/*}"
		"$c" "$@" -I"$rando/include" -c -o "$o" "$rando/src/$s" 2>>"$b/errors" &
		n=$((n + 1))
		if [ "$n" -ge "$jobs" ]; then
			wait
			n=0
		fi
	done
	"$c" "$@" -fno-common -I"$rando/include" -c -o "$b/size_probe.o" "$here/measure/size_probe.c" 2>>"$b/errors" &
	wait
	for s in $lib_srcs; do
		[ -f "$b/obj/${s%.c}.o" ] || die "$1 build failed: $(cat "$b/errors")"
	done
	[ -f "$b/size_probe.o" ] || die "$1 size probe failed: $(cat "$b/errors")"
}

# lib_objs <build>: sets objs to the build's library objects.
lib_objs() {
	objs=
	for s in $lib_srcs; do
		o="$dir/$1/obj/${s%.c}.o"
		[ -f "$o" ] || die "missing object: $o (run make measure)"
		objs="$objs $o"
	done
}

# state_sizes <build> <nm>
state_sizes() {
	p="$dir/$1/size_probe.o"
	[ -f "$p" ] || die "missing size probe: $p (run make measure)"
	"$2" -S "$p" >"$dir/$1/probe.nm" || die "$2 cannot read $p"
	parts=0
	for t in $probes; do
		hex="$(awk -v s="rando_probe_$t" '$4 == s { print $2 }' "$dir/$1/probe.nm")"
		[ -n "$hex" ] || die "size probe: no symbol rando_probe_$t in $1"
		v="$(printf '%d' "0x$hex")"
		eval "size_$t=$v"
		case "$t" in
		RandoGen) figure "$1.sizeof.RandoGen" "$v" bytes "$state_budget" ;;
		RandoFillState) echo "$1.sizeof.RandoFillState $v bytes stack-resident (local of run_attempt)" ;;
		*) figure "$1.sizeof.$t" "$v" bytes ;;
		esac
		case "$t" in
		RandoOptions | RandoPrng | RandoPools | RandoWorld) parts=$((parts + v)) ;;
		esac
		if [ "$t" = RandoWorld ]; then
			echo "$1.sizeof.RandoGen-rest $((size_RandoGen - parts)) bytes (counters, trace hook, padding)"
		fi
	done
}

# lib_size <build> <size>: the library objects' sections summed by class.
lib_size() {
	lib_objs "$1"
	: >"$dir/$1/sections"
	for o in $objs; do
		"$2" -A "$o" >>"$dir/$1/sections" || die "$2 cannot read $o"
	done
	awk -v b="$1" -v code_budget="$code_budget" -v rootdir="$dir/$1/obj/" '
	function line(name, v, budget) {
		if (budget == "") printf "%s.lib.%s %d bytes\n", b, name, v
		else printf "%s.lib.%s %d bytes budget %d%s\n", b, name, v, budget, (v > budget ? " over" : "")
	}
	/^[^ \t].* :$/ { obj = $1; sub(rootdir, "", obj); next }
	NF != 3 || $1 == "section" || $1 == "Total" { next }
	$1 ~ /^\.(debug|comment|note|ARM\.attributes|llvm_addrsig|group|rel|symtab|strtab|shstrtab)/ { next }
	$1 ~ /^\.text/ { code += $2; next }
	$1 ~ /^\.(rodata|data\.rel\.ro)/ { ro += $2; next }
	$1 ~ /^\.(data|tdata)/ { rw += $2; if ($2) flags = flags "FLAG: mutable static state in the library: " obj " " $1 "\n"; next }
	$1 ~ /^\.(bss|tbss)/ || $1 == "COMMON" { bss += $2; if ($2) flags = flags "FLAG: mutable static state in the library: " obj " " $1 "\n"; next }
	$2 > 0 { other += $2; if (!($1 in seen)) { seen[$1] = 1; names = names (names == "" ? "" : ", ") $1 } }
	END {
		line("code", code)
		line("rodata", ro)
		line("code+rodata", code + ro, code_budget)
		line("writable", rw, 0)
		line("bss", bss, 0)
		if (other) printf "%s.lib.other %d bytes not counted (%s)\n", b, other, names
		printf "%s", flags
	}' "$dir/$1/sections"
}

# frames <build>: the ten largest frames and every frame that is not static.
frames() {
	lib_objs "$1"
	: >"$dir/$1/su"
	for o in $objs; do
		su="${o%.o}.su"
		[ -f "$su" ] || die "missing stack usage: $su"
		cat "$su" >>"$dir/$1/su"
	done
	awk -F '\t' '{
		n = split($1, p, ":")
		f = p[1]; sub(/.*\//, "", f)
		print $2 "\t" p[n] "\t" f "\t" $3
	}' "$dir/$1/su" | sort -t "$(printf '\t')" -k1,1nr -k2,2 >"$dir/$1/frames"
	head -n 10 "$dir/$1/frames" | awk -F '\t' -v b="$1" '{ printf "%s.frame.%s %d bytes %s %s\n", b, $2, $1, $4, $3 }'
	awk -F '\t' -v b="$1" '
	$4 != "static" {
		n++
		printf "%s.non-static.%s %d bytes %s %s\n", b, $2, $1, $4, $3
		if ($4 ~ /dynamic/ && $4 !~ /bounded/) printf "FLAG: unbounded stack frame: %s (%s)\n", $2, $3
	}
	END { if (!n) printf "%s.non-static none\n", b }' "$dir/$1/frames"
}

dkp=
for d in "${DEVKITPRO:-}" /opt/devkitpro "${HOME:-}/devkitpro"; do
	if [ -n "$d" ] && [ -x "$d/devkitARM/bin/arm-none-eabi-gcc" ] &&
		"$d/devkitARM/bin/arm-none-eabi-gcc" --version >/dev/null 2>&1; then
		dkp="$d"
		break
	fi
done

clang=
clang_nm=
clang_size=
if command -v clang >/dev/null 2>&1; then
	clang="$(command -v clang)"
	clang_dir="$(dirname "$(readlink -f "$clang")")"
	for t in nm size; do
		p="$(command -v "llvm-$t" 2>/dev/null || true)"
		[ -n "$p" ] || [ ! -x "$clang_dir/llvm-$t" ] || p="$clang_dir/llvm-$t"
		eval "clang_$t=\$p"
	done
	if [ -z "$clang_nm" ] || [ -z "$clang_size" ]; then
		clang=
	fi
fi

builds="host-O2 host-Os"
arm_flags="-march=armv5te -mtune=arm946e-s -marm -O2"
if [ -n "$dkp" ]; then
	dkp_bin="$dkp/devkitARM/bin"
	compile_lib arm-dkp "$dkp_bin/arm-none-eabi-gcc" $arm_flags -ffunction-sections -fdata-sections -std=c99 -fstack-usage
	builds="$builds arm-dkp"
fi
if [ -n "$clang" ]; then
	compile_lib arm-clang "$clang" --target=arm-none-eabi $arm_flags -ffreestanding -std=c99 -fstack-usage -I"$here/measure/shim"
	builds="$builds arm-clang"
fi

nm_of() {
	case "$1" in
	host-*) echo nm ;;
	arm-dkp) echo "$dkp_bin/arm-none-eabi-nm" ;;
	arm-clang) echo "$clang_nm" ;;
	esac
}
size_of() {
	case "$1" in
	host-*) echo size ;;
	arm-dkp) echo "$dkp_bin/arm-none-eabi-size" ;;
	arm-clang) echo "$clang_size" ;;
	esac
}

echo "== 1. Environment =="
echo "host.cc $("$cc" --version | head -n 1)"
cpu="$(awk -F ': *' '/^model name/ { print $2; exit }' /proc/cpuinfo 2>/dev/null || true)"
echo "host.cpu ${cpu:-$(uname -m)}"
commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
if [ "$commit" != unknown ] && ! git -C "$root" diff-index --quiet HEAD -- 2>/dev/null; then
	commit="$commit dirty"
fi
echo "commit $commit"
if [ -n "$dkp" ]; then
	echo "arm-dkp.cc $("$dkp_bin/arm-none-eabi-gcc" --version | head -n 1) ($dkp)"
else
	echo "arm-dkp skipped: devkitARM not found"
fi
if [ -n "$clang" ]; then
	echo "arm-clang.cc $("$clang" --version | head -n 1)"
	echo "arm-clang.note asserts compiled out by the shim: its code size is a lower bound"
else
	echo "arm-clang skipped: clang not found"
fi
echo "flags no RANDO_FAULTS, -O2, no stack protector, no fortification"

echo
echo "== 2. State sizes =="
state_sizes host-O2 nm
[ "$size_RandoItems" -eq 716 ] ||
	die "size probe check failed: host RandoItems is $size_RandoItems bytes, expected 716"
for b in $builds; do
	case "$b" in
	host-*) ;;
	*) state_sizes "$b" "$(nm_of "$b")" ;;
	esac
done

echo
echo "== 3. Library size =="
for b in $builds; do
	lib_size "$b" "$(size_of "$b")"
done

echo
echo "== 4. Largest stack frames =="
for b in $builds; do
	frames "$b"
done

. "$here/cases.sh"
cases_dir="$here/cases"
stages_file="$here/stages.txt"
driver="$dir/rando_measure"
reps="${MEASURE_REPS:-5}"
sweep="${MEASURE_SWEEP:-1024}"
stack_budget=8192
time_budget_us=13000
ds_factor=150
[ -x "$driver" ] || die "missing driver: $driver (run make measure)"
stage_list="$(awk '$1 == "stage" { print $2 }' "$stages_file")"
[ -n "$stage_list" ] || die "no stage in $stages_file"

case_names="$(discover_cases "$cases_dir" measure.sh)"
[ -n "$case_names" ] || die "no case in $cases_dir"
: >"$dir/golden"
for c in $case_names; do
	[ -f "$cases_dir/$c/trace.txt" ] || die "unreadable case: $c has no trace.txt"
	# One line: golden <case> <attempts> <draws> <draws per attempt a/b/..> <winning attempt's draws per stage s/s/..>.
	awk -v name="$c" -v stages="$(echo $stage_list)" '
	FILENAME ~ /\/attempts$/ { win = $1 + 0; has = 1; next }
	{ total++; per[$1 + 0]++; if ($1 + 0 > last) last = $1 + 0; st[$1 + 0, $3]++ }
	END {
		if (!has) win = last
		n = split(stages, s, " ")
		pa = ""
		for (a = 0; a <= last; a++) pa = pa (a ? "/" : "") (per[a] + 0)
		ps = ""
		for (i = 1; i <= n; i++) ps = ps (i > 1 ? "/" : "") (st[win, s[i]] + 0)
		printf "golden %s %d %d %s %s\n", name, (has ? win : -1), total, pa, ps
	}' $( [ -f "$cases_dir/$c/attempts" ] && echo "$cases_dir/$c/attempts") "$cases_dir/$c/trace.txt" >>"$dir/golden" ||
		die "unreadable case: $c"
done

# Step 1.1 note: baseline's golden draws.
awk -v stages="$(echo $stage_list)" '
BEGIN {
	want["getAdvancementItems"] = 4; want["setMedallions"] = 2; want["fillPrizes"] = 64; want["setFountains"] = 2
	want["shufflePrizePacks"] = 7; want["prepareWorld"] = 153; want["fill"] = 573
}
$2 == "baseline" {
	found = 1
	if ($4 != 805) { printf "baseline golden has %d draws, expected 805\n", $4; bad = 1 }
	n = split(stages, s, " "); split($6, v, "/")
	for (i = 1; i <= n; i++)
		if (v[i] != want[s[i]] + 0) { printf "baseline golden stage %s has %d draws, expected %d\n", s[i], v[i], want[s[i]]; bad = 1 }
}
END { if (!found) print "no baseline case"; exit !found || bad }' "$dir/golden" >"$dir/golden-check" ||
	die "golden parser check failed: $(cat "$dir/golden-check")"

case_dirs=
for c in $case_names; do
	case_dirs="$case_dirs $cases_dir/$c"
done
"$driver" cases "$reps" $case_dirs >"$dir/cases.out" 2>"$dir/driver.err" ||
	die "stack self-check or case run failed: $(cat "$dir/driver.err")"

ratio_build=
for b in arm-dkp arm-clang; do
	case " $builds " in
	*" $b "*)
		ratio_build="$b"
		break
		;;
	esac
done

echo
echo "== 5. Stack peak =="
awk '
$1 == "overhead" { printf "host-O2.stack.thread-overhead %d bytes (thread start-up and entry function, subtracted)\n", $2 }
$1 == "empty" { printf "host-O2.stack.empty-run %d bytes raw (glibc start-up, not counted)\n", $2 }
$1 == "selfcheck" { printf "host-O2.stack.selfcheck %d bytes net for a known %d-byte use\n", $3, $4 }
$1 == "case" && $3 == "unsupported" { printf "host-O2.stack.peak.%s skipped: unsupported\n", $2 }
$1 == "case" && $3 != "unsupported" {
	printf "host-O2.stack.peak.%s %d bytes raw %d\n", $2, $9, $8
	if ($9 > max) { max = $9; at = $2 }
}
END {
	printf "host-O2.stack.peak.max %d bytes\n", max
	printf "host-O2.stack.peak.max-case %s\n", at
	printf "%d\n", max > "/dev/stderr"
}' "$dir/cases.out" 2>"$dir/peak"
peak="$(cat "$dir/peak")"
if [ -n "$ratio_build" ]; then
	head -n 10 "$dir/host-O2/frames" | awk -F '\t' -v rb="$ratio_build" -v peak="$peak" -v budget="$stack_budget" '
	FILENAME == ARGV[1] { arm[$2 "\t" $3] = $1; next }
	{
		k = $2 "\t" $3
		if (!(k in arm) || $1 == 0) { printf "%s.stack.frame-ratio.%s skipped: not in %s\n", rb, $2, rb; next }
		r = arm[k] / $1
		printf "%s.stack.frame-ratio.%s %.3f arm %d host %d %s\n", rb, $2, r, arm[k], $1, $3
		if (r > max) max = r
	}
	END {
		scaled = int(peak * max + 0.999)
		printf "%s.stack.frame-ratio.max %.3f\n", rb, max
		printf "host-O2.stack.peak.arm-scaled %d bytes (peak x %.3f)\n", scaled, max
		basis = scaled > peak ? scaled : peak
		printf "host-O2.stack.peak.budget-basis %d bytes budget %d%s\n", basis, budget, (basis > budget ? " over" : "")
	}' "$dir/$ratio_build/frames" -
else
	echo "host-O2.stack.frame-ratio skipped: no ARM build"
	figure host-O2.stack.peak.budget-basis "$peak" bytes "$stack_budget"
fi

echo
echo "== 6. Per case =="
echo "stages $(echo $stage_list | tr ' ' '/')"
echo "note same/differs compares draw counts only, not draw values"
awk -v budget="$time_budget_us" '
FILENAME == ARGV[1] { g_att[$2] = $3; g_draws[$2] = $4; g_pa[$2] = $5; g_st[$2] = $6; order[++n] = $2; next }
$1 == "case" { c_st[$2] = $3; c_att[$2] = $4; c_draws[$2] = $5; c_st_d[$2] = $7; c_ns[$2] = $10; seen[$2] = 1 }
function cmp(a, b) { return a == b ? "same" : "differs" }
END {
	for (i = 1; i <= n; i++) {
		c = order[i]
		printf "golden.%s.attempts %s attempts\n", c, (g_att[c] < 0 ? "none" : g_att[c])
		printf "golden.%s.draws %d draws\n", c, g_draws[c]
		printf "golden.%s.attempt-draws %s draws\n", c, g_pa[c]
		printf "golden.%s.stages %s draws\n", c, g_st[c]
		if (!(c in seen)) { printf "host-O2.%s missing from the driver output\n", c; continue }
		printf "host-O2.%s.status %s\n", c, c_st[c]
		printf "host-O2.%s.attempts %d attempts %s\n", c, c_att[c], cmp(c_att[c], g_att[c])
		printf "host-O2.%s.draws %d draws %s\n", c, c_draws[c], cmp(c_draws[c], g_draws[c])
		printf "host-O2.%s.stages %s draws %s\n", c, c_st_d[c], cmp(c_st_d[c], g_st[c])
		us = int(c_ns[c] / 1000 + 0.5)
		printf "host-O2.%s.time %d us median\n", c, us
		if (g_draws[c] < gmin || i == 1) gmin = g_draws[c]
		if (g_draws[c] > gmax) gmax = g_draws[c]
		if (c_draws[c] != g_draws[c] || c_st_d[c] != g_st[c]) { nd++; dl = dl " " c }
		if (us > tmax) { tmax = us; tat = c }
		if (c_st[c] != "unsupported") ns++
		else nu++
		if (c_st[c] == "complete") nc++
	}
	printf "golden.draws.min %d draws\n", gmin
	printf "golden.draws.max %d draws\n", gmax
	printf "host-O2.cases.supported %d cases\n", ns
	printf "host-O2.cases.unsupported %d cases\n", nu
	printf "host-O2.cases.complete %d cases\n", nc
	printf "host-O2.cases.differs %d cases%s\n", nd, dl
	printf "host-O2.cases.time.max %d us budget %d%s\n", tmax, budget, (tmax > budget ? " over" : "")
	printf "host-O2.cases.time.max-case %s\n", tat
	if (c_st["baseline"] != "complete" || c_att["baseline"] != 0 || c_draws["baseline"] != 805)
		printf "WARNING: baseline C run is %s at attempt %d with %d draws, expected complete at attempt 0 with 805 draws\n", c_st["baseline"], c_att["baseline"], c_draws["baseline"]
}' "$dir/golden" "$dir/cases.out"

echo
echo "== 7. Seed sweep =="
"$driver" sweep "$cases_dir/baseline" "$sweep" >"$dir/sweep.out" 2>"$dir/driver.err" ||
	die "seed sweep failed: $(cat "$dir/driver.err")"
sort -k6,6n "$dir/sweep.out" | awk -v n="$sweep" -v budget="$time_budget_us" -v factor="$ds_factor" '
{
	t[++k] = $6
	if ($3 == "gave-up") { wd = wd sprintf("watchdog %d\n", $2); nwd++ }
	else if ($3 != "complete") other[$3]++
	else att[$4]++
	if ($4 > 0 && $3 == "complete") { nr++; if (nr <= 10) retry = retry " " $2 }
	if ($4 > amax) amax = $4
	if ($5 > dmax || k == 1) { dmax = $5; dat = $2 }
	if ($6 >= tmax) { tmax = $6; tat = $2 }
}
END {
	printf "host-O2.sweep.seeds %d seeds (1..%d, baseline config)\n", k, n
	printf "host-O2.sweep.watchdog 1000 attempts\n"
	for (a = 0; a <= amax; a++) if (a in att) printf "host-O2.sweep.attempt.%d %d seeds\n", a, att[a]
	for (s in other) printf "host-O2.sweep.status.%s %d seeds\n", s, other[s]
	printf "host-O2.sweep.retry-seeds %d seeds%s\n", nr, nr ? retry (nr > 10 ? " ..." : "") : ""
	printf "host-O2.sweep.watchdog-hits %d seeds\n", nwd
	printf "%s", wd
	printf "host-O2.sweep.attempts.max %d attempts (A-4: no budget, every attempt counts in A-3)\n", amax
	printf "host-O2.sweep.draws.max %d draws\n", dmax
	printf "host-O2.sweep.draws.max-seed %d\n", dat
	med = int(t[int((k + 1) / 2)] / 1000 + 0.5)
	mx = int(tmax / 1000 + 0.5)
	printf "host-O2.sweep.time.median %d us\n", med
	printf "host-O2.sweep.time.max %d us budget %d%s\n", mx, budget, (mx > budget ? " over" : "")
	printf "host-O2.sweep.time.max-seed %d\n", tat
	printf "ds.time.projected.median %d ms (host x %d, A-3)\n", int(med * factor / 1000 + 0.5), factor
	printf "ds.time.projected.max %d ms budget 2000%s (host x %d, A-3)\n", int(mx * factor / 1000 + 0.5), (mx * factor / 1000 > 2000 ? " over" : ""), factor
}'
