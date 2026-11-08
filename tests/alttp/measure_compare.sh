#!/bin/sh
# Usage: measure_compare.sh [--guard]
# Run by `make measure-compare`, which first creates or reuses REF_DIR and builds both drivers.
# Exit 0 whatever the figures; 1 when a measurement cannot be taken; 2 on a usage error.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../.." && pwd)"
self="$here/$(basename "$0")"
. "$here/cases.sh"

ref_commit="${REF_COMMIT:-40314c3}"
ref_dir="${REF_DIR:-$root/tmp/measure-ref-$ref_commit}"
passes="${COMPARE_PASSES:-6}"
reps="${COMPARE_REPS:-21}"
recheck_reps="${COMPARE_RECHECK_REPS:-51}"
dir="${MEASURE_DIR:-$here/build/measure}"
case "$dir" in
/*) ;;
*) dir="$PWD/$dir" ;;
esac

die() {
	echo "measure-compare: $1" >&2
	exit 1
}

usage() {
	echo "usage: measure_compare.sh [--guard]" >&2
	exit 2
}

for n in "$passes" "$reps" "$recheck_reps"; do
	case "$n" in
	'' | *[!0-9]*) usage ;;
	esac
	[ "$n" -ge 1 ] || usage
done
[ "$reps" -le 101 ] && [ "$recheck_reps" -le 101 ] || usage

guard() {
	top="$(git -C "$ref_dir" rev-parse --show-toplevel 2>/dev/null || true)"
	want="$(git -C "$root" rev-parse --verify --quiet "$ref_commit^{commit}" 2>/dev/null || true)"
	head="$(git -C "$ref_dir" rev-parse --verify --quiet HEAD 2>/dev/null || true)"
	if [ -z "$top" ] || [ "$(cd "$top" && pwd -P)" != "$(cd "$ref_dir" 2>/dev/null && pwd -P)" ] ||
		[ -z "$want" ] || [ "$head" != "$want" ] ||
		[ -n "$(git -C "$ref_dir" status --porcelain 2>/dev/null || echo unreadable)" ]; then
		die "reference worktree is not a clean $ref_commit: $ref_dir"
	fi
}

# measure <driver> <mode> <reps> <case dir> <out>: appends "<status> <attempt> <draws> <median ns>".
measure() {
	if [ "$2" = fill ]; then
		"$1" cases-until fill "$3" "$4" >"$tmp/one.out" 2>"$tmp/one.err" ||
			die "$1 failed on ${4##*/}: $(cat "$tmp/one.err")"
	else
		"$1" cases "$3" "$4" >"$tmp/one.out" 2>"$tmp/one.err" ||
			die "$1 failed on ${4##*/}: $(cat "$tmp/one.err")"
	fi
	awk '$1 == "case" { print $3, $4, $5, $NF; n++ } END { exit n != 1 }' "$tmp/one.out" >>"$5" ||
		die "$1 printed no single case line for ${4##*/}"
}

# run_set <set> <reps> <case>...: P interleaved passes, the order rotating with the pass.
run_set() {
	set_name="$1"
	set_reps="$2"
	shift 2
	p=1
	while [ "$p" -le "$passes" ]; do
		case $(((p - 1) % 3)) in
		0) order="ref full fill" ;;
		1) order="full fill ref" ;;
		*) order="fill ref full" ;;
		esac
		for c in "$@"; do
			for k in $order; do
				: >"$tmp/m"
				case "$k" in
				ref) measure "$ref_driver" ref "$set_reps" "$ref_cases/$c" "$tmp/m" ;;
				*) measure "$cur_driver" "$k" "$set_reps" "$cur_cases/$c" "$tmp/m" ;;
				esac
				echo "m $set_name $p $c $k $(cat "$tmp/m")" >>"$tmp/results"
			done
			work_check "$set_name" "$p" "$c"
		done
		p=$((p + 1))
	done
}

# work_check <set> <pass> <case>: the three runs did the same work, and full the golden's.
work_check() {
	golden="$(awk 'END { print NR }' "$cur_cases/$3/trace.txt")"
	awk -v s="$1" -v p="$2" -v c="$3" -v golden="$golden" '
	$1 == "m" && $2 == s && $3 == p && $4 == c { st[$5] = $6; att[$5] = $7; dr[$5] = $8 }
	END {
		bad = ""
		if (st["ref"] != "complete") bad = bad " ref status " st["ref"]
		if (st["full"] != "complete") bad = bad " full status " st["full"]
		if (st["fill"] != "incomplete") bad = bad " fill status " st["fill"]
		if (att["ref"] != att["full"] || att["ref"] != att["fill"])
			bad = bad " attempts ref " att["ref"] " full " att["full"] " fill " att["fill"]
		if (dr["ref"] != dr["fill"]) bad = bad " draws ref " dr["ref"] " fill " dr["fill"]
		if (dr["full"] != golden) bad = bad " draws full " dr["full"] " golden " golden
		if (bad != "") { printf "work check failed on %s (%s pass %d):%s\n", c, s, p, bad; exit 1 }
	}' "$tmp/results" >"$tmp/wc" || die "$(cat "$tmp/wc")"
}

# figures <set>: per case and kind, the minimum of the pass medians, with ratios and verdicts.
figures() {
	awk -v s="$1" '
	function verdict(r) { return r > 1.10 ? "over" : r < 0.90 ? "under" : "within" }
	$1 == "m" && $2 == s {
		k = $4 SUBSEP $5
		if (!(k in mn) || $9 < mn[k]) mn[k] = $9
		if (!($4 in seen)) { seen[$4] = 1; order[++n] = $4 }
	}
	END {
		for (i = 1; i <= n; i++) {
			c = order[i]
			ref = mn[c, "ref"]; full = mn[c, "full"]; fill = mn[c, "fill"]
			rf = full / ref; rl = fill / ref
			printf "fig %s %s %.1f %.1f %.4f %s %.1f %.4f %s %.1f %.1f\n", s, c, ref / 1000, full / 1000, rf, verdict(rf),
				fill / 1000, rl, verdict(rl), (full - fill) / 1000, 100 * (full - fill) / full
		}
	}' "$tmp/results"
}

compare() {
	ref_driver="${COMPARE_REF_DRIVER:-$ref_dir/tests/rando/build/measure/rando_measure}"
	cur_driver="${COMPARE_CUR_DRIVER:-$dir/rando_measure}"
	ref_cases="${COMPARE_REF_CASES:-$ref_dir/tests/rando/cases}"
	cur_cases="${COMPARE_CUR_CASES:-$here/cases}"
	[ -x "$ref_driver" ] || die "missing reference driver: $ref_driver"
	[ -x "$cur_driver" ] || die "missing current driver: $cur_driver"
	ref_names="$(discover_cases "$ref_cases" measure-compare)"
	cur_names="$(discover_cases "$cur_cases" measure-compare)"
	shared=
	new=
	for c in $cur_names; do
		if printf '%s\n' "$ref_names" | grep -Fqx -- "$c"; then
			shared="$shared $c"
		else
			new="$new $c"
		fi
	done
	[ -n "$shared" ] || die "no case shared by $ref_cases and $cur_cases"
	for c in $shared; do
		[ -f "$cur_cases/$c/trace.txt" ] || die "unreadable case: $c has no trace.txt"
	done

	if [ -n "${_COMPARE_INNER:-}" ]; then
		echo "compare.ref self-check"
		echo "compare.current self-check"
	else
		echo "compare.ref $(git -C "$ref_dir" rev-parse --short HEAD) $("${CC:-cc}" --version | head -n 1)"
		commit="$(git -C "$root" rev-parse --short HEAD 2>/dev/null || echo unknown)"
		if [ "$commit" != unknown ] && ! git -C "$root" diff-index --quiet HEAD -- 2>/dev/null; then
			commit="$commit dirty"
		fi
		echo "compare.current $commit"
		cpu="$(awk -F ': *' '/^model name/ { print $2; exit }' /proc/cpuinfo 2>/dev/null || true)"
		echo "compare.cpu ${cpu:-$(uname -m)}"
	fi
	echo "compare.passes $passes reps $reps recheck-reps $recheck_reps"
	[ -z "$new" ] || echo "compare.new$new (no reference)"

	: >"$tmp/results"
	# shellcheck disable=SC2086
	run_set first "$reps" $shared
	figures first >"$tmp/first"
	flagged="$(awk '$7 != "within" || $10 != "within" { printf " %s", $3 }' "$tmp/first")"
	if [ -n "$flagged" ]; then
		# shellcheck disable=SC2086
		run_set recheck "$recheck_reps" $flagged
	fi
	figures recheck >"$tmp/recheck"
	awk '
	function lines(c, tag) {
		printf "compare.%s%s.ref %s us\n", c, tag, ref[c]
		printf "compare.%s%s.full %s us ratio %.3f %s\n", c, tag, full[c], rf[c], vf[c]
		printf "compare.%s%s.fill %s us ratio %.3f %s\n", c, tag, fill[c], rl[c], vl[c]
		printf "compare.%s%s.post-fill %s us share %.1f%%\n", c, tag, post[c], share[c]
	}
	function load() { c = $3; ref[c] = $4; full[c] = $5; rf[c] = $6; vf[c] = $7; fill[c] = $8; rl[c] = $9; vl[c] = $10; post[c] = $11; share[c] = $12 }
	function summary(kind, i, c, r, v, sum, n, lo, hi, loc, hic, nw, no, nu, ov, un) {
		for (i = 1; i <= nc; i++) {
			c = order[i]; r = fr[kind, c]; v = fv[kind, c]
			sum += r; n++
			if (n == 1 || r < lo) { lo = r; loc = c }
			if (n == 1 || r > hi) { hi = r; hic = c }
			if (v == "within") nw++
			else if (v == "over") { no++; ov = ov " " c }
			else { nu++; un = un " " c }
		}
		printf "compare.%s.summary cases %d mean %.3f min %.3f %s max %.3f %s within %d over %d under %d\n", kind, n, sum / n, lo, loc, hi, hic, nw, no, nu
		sv[kind, "over"] = ov == "" ? " none" : ov
		sv[kind, "under"] = un == "" ? " none" : un
	}
	FILENAME == ARGV[1] {
		load(); order[++nc] = c; lines(c, "")
		fr["full", c] = rf[c]; fv["full", c] = vf[c]; fr["fill", c] = rl[c]; fv["fill", c] = vl[c]
		next
	}
	{
		load(); lines(c, ".recheck")
		fr["full", c] = rf[c]; fv["full", c] = vf[c]; fr["fill", c] = rl[c]; fv["fill", c] = vl[c]
	}
	END {
		summary("full"); summary("fill")
		printf "compare.full.over%s\ncompare.full.under%s\n", sv["full", "over"], sv["full", "under"]
		printf "compare.fill.over%s\ncompare.fill.under%s\n", sv["fill", "over"], sv["fill", "under"]
	}' "$tmp/first" "$tmp/recheck"
	if [ -n "${_COMPARE_INNER:-}" ]; then
		# shellcheck disable=SC2086
		set -- $shared
		awk -v c="$1" '$1 == "m" && $2 == "first" && $4 == c { printf "%s%s", (n++ ? " " : ""), $5 }' "$tmp/results" >"$_COMPARE_INNER/rotation"
	fi
}

# A fake driver: medians planted per case, kind and call, counted in its state directory.
write_fake() {
	cat >"$1" <<'EOF'
#!/bin/sh
set -eu
kind="$FAKE_KIND"
if [ "$1" = cases-until ]; then kind=fill; reps="$3"; d="$4"; else reps="$2"; d="$3"; fi
c="${d##*/}"
n="$(cat "$FAKE_STATE/$kind.$c" 2>/dev/null || echo 0)"
n=$((n + 1))
echo "$n" >"$FAKE_STATE/$kind.$c"
status=complete
draws=3
case "$kind" in
ref) draws=2; [ -z "${FAKE_MISMATCH:-}" ] || draws=9 ;;
fill) status=incomplete; draws=2 ;;
esac
case "$c.$kind" in
a-within.ref) set -- 2000000 1000000 2000000; shift $(((n - 1) % 3)); t="$1" ;;
a-within.full) t=1050000 ;;
a-within.fill) t=1000000 ;;
b-recheck.ref) t=1000000 ;;
b-recheck.full) if [ "$reps" = "$FAKE_RECHECK" ]; then t=1020000; else t=1200000; fi ;;
b-recheck.fill) t=1000000 ;;
c-under.ref) t=1000000 ;;
*) t=800000 ;;
esac
echo "empty 1"
echo "overhead 1"
echo "selfcheck 1 1 1"
echo "case $c $status 0 $draws $draws 0/0 10 9 $t"
EOF
	chmod +x "$1"
}

self_check() {
	st="$tmp/selfcheck"
	mkdir -p "$st/ref" "$st/cur" "$st/state" "$st/bin"
	for c in a-within b-recheck c-under d-new; do
		mkdir -p "$st/cur/$c"
		echo '{}' >"$st/cur/$c/config.json"
		printf 'x\nx\nx\n' >"$st/cur/$c/trace.txt"
		[ "$c" = d-new ] || { mkdir -p "$st/ref/$c" && echo '{}' >"$st/ref/$c/config.json"; }
	done
	write_fake "$st/bin/fake"
	printf '#!/bin/sh\nFAKE_KIND=ref exec "%s" "$@"\n' "$st/bin/fake" >"$st/bin/ref"
	printf '#!/bin/sh\nFAKE_KIND=full exec "%s" "$@"\n' "$st/bin/fake" >"$st/bin/cur"
	chmod +x "$st/bin/ref" "$st/bin/cur"
	if ! _COMPARE_INNER="$st" FAKE_STATE="$st/state" FAKE_RECHECK=7 COMPARE_PASSES=3 COMPARE_REPS=5 \
		COMPARE_RECHECK_REPS=7 COMPARE_REF_DRIVER="$st/bin/ref" COMPARE_CUR_DRIVER="$st/bin/cur" \
		COMPARE_REF_CASES="$st/ref" COMPARE_CUR_CASES="$st/cur" TMPDIR="$st" "$self" >"$st/out" 2>"$st/err"; then
		die "self-check failed: the planted run exited non-zero: $(cat "$st/err")"
	fi
	expect() {
		grep -Fqx -- "$1" "$st/out" || die "self-check failed: $2 (no line: $1)"
	}
	expect "compare.a-within.full 1050.0 us ratio 1.050 within" "a case at 1.05 is not within on the minimum of medians"
	expect "compare.a-within.ref 1000.0 us" "the figure is not the minimum of the medians"
	expect "compare.b-recheck.full 1200.0 us ratio 1.200 over" "a case at 1.20 is not over"
	expect "compare.b-recheck.recheck.full 1020.0 us ratio 1.020 within" "the recheck did not turn 1.20 into within"
	expect "compare.c-under.full 800.0 us ratio 0.800 under" "a case at 0.80 is not under"
	expect "compare.c-under.recheck.full 800.0 us ratio 0.800 under" "an under case was not rechecked"
	expect "compare.c-under.post-fill 0.0 us share 0.0%" "the post-fill share is wrong"
	expect "compare.new d-new (no reference)" "a case with no reference is not listed as new"
	expect "compare.full.summary cases 3 mean 0.957 min 0.800 c-under max 1.050 a-within within 2 over 0 under 1" "the full summary is not on final verdicts"
	expect "compare.full.over none" "the full over list is wrong"
	expect "compare.full.under c-under" "the full under list is wrong"
	grep -Fq "compare.a-within.recheck" "$st/out" && die "self-check failed: a within case was rechecked"
	rot="$(cat "$st/rotation" 2>/dev/null || true)"
	[ "$rot" = "ref full fill full fill ref fill ref full" ] ||
		die "self-check failed: the order does not rotate with the pass ($rot)"
	rm -rf "$st/state"
	mkdir "$st/state"
	if _COMPARE_INNER="$st" FAKE_STATE="$st/state" FAKE_RECHECK=7 FAKE_MISMATCH=1 COMPARE_PASSES=1 COMPARE_REPS=5 \
		COMPARE_RECHECK_REPS=7 COMPARE_REF_DRIVER="$st/bin/ref" COMPARE_CUR_DRIVER="$st/bin/cur" \
		COMPARE_REF_CASES="$st/ref" COMPARE_CUR_CASES="$st/cur" TMPDIR="$st" "$self" >"$st/out" 2>"$st/err"; then
		die "self-check failed: planted mismatched draws did not exit 1"
	fi
	grep -Fq "work check failed on a-within" "$st/err" ||
		die "self-check failed: planted mismatched draws were not named ($(cat "$st/err"))"
}

[ "$#" -le 1 ] || usage
if [ "$#" -eq 1 ]; then
	[ "$1" = --guard ] || usage
	guard
	exit 0
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/compare.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

if [ -n "${_COMPARE_INNER:-}" ]; then
	compare
	exit 0
fi

self_check
guard
mkdir -p "$dir"
compare >"$tmp/report"
cp "$tmp/report" "$dir/compare.txt"
cat "$tmp/report"
