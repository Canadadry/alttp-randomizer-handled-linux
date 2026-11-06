#!/bin/sh
# Usage: determinism.sh [--full [--all]]
# Runs every case of determinism_cases.txt through oracle_run.py twice (A: the setup checkout,
# PYTHONHASHSEED=0; B: a fresh clone in another directory, PYTHONHASHSEED=1) and compares the two
# outputs: one `same <case>` or `DIFFERENT <case>: line <n>: <A> | <B>` line per case, then a
# summary line. It first proves its comparator on planted runs, and fails unless every case's two
# runs saw another hash("ladxr") and another checkout. Needs the network; never called by make.
# --full runs the cases with the ROM instead (gen_golden.sh; B's work root a throwaway tagged git
# repository), plus LADXR's own CLI once per case (oracle_run.py --plain), and compares every file:
# `same <case>` or `DIFFERENT <case>: <file>: ...` / `DIFFERENT <case> (plain): ...` lines. It also
# fails unless the runs differed in the working directory's `git describe`, and on a new untracked
# file of the repository (`STRAY <path>`). --full --all does the same on every case directory of
# tests/ladx/cases/ and tests/ladx/combos/ (ladx_cases.py list) instead of determinism_cases.txt.
set -eu
export LC_ALL=C

oracle_dir="$(cd "$(dirname "$0")" && pwd)"
repo_dir="$(cd "$oracle_dir/../../.." && pwd)"
work_root="$repo_dir/tmp/ladx-oracle"
out_root="$work_root/determinism"
clone_b="$work_root/ladxr-determinism"
compare="python3 -I $oracle_dir/ladx_oracle.py"

full=0
all=0
case "$#:${1-}:${2-}" in
0::) ;;
1:--full:) full=1 ;;
2:--full:--all) full=1 all=1 ;;
*)
	echo "usage: determinism.sh [--full [--all]]" >&2
	exit 2
	;;
esac

# expect <what> <expected output> <command...>: fails the self-proof unless the command exits
# non-zero with exactly that output (exit 0 when the expected output is a `same` line).
expect() {
	what="$1" want="$2"
	shift 2
	if got="$("$@")"; then code=0; else code=1; fi
	case "$want" in same*) want_code=0 ;; *) want_code=1 ;; esac
	if [ "$got" != "$want" ] || [ "$code" -ne "$want_code" ]; then
		echo "determinism: self-proof failed: the comparator did not report $what" >&2
		exit 1
	fi
}

# plant <dir> <outcome> [<file>=<size>:<flip offset or ->...]: a planted case directory, with
# run.txt (or plain.txt for a `plain` dir) ending in the outcome and synthetic binary files.
plant() {
	python3 -I - "$@" <<'EOF'
import os, sys
d, outcome = sys.argv[1], sys.argv[2]
os.makedirs(d, exist_ok=True)
name = "plain.txt" if os.path.basename(d).startswith("plain") else "run.txt"
with open(os.path.join(d, name), "w") as f:
    f.write("seed 00\ntrace 0 rnd x:y choice 3 -> 1 [1]\noutcome %s\n" % outcome)
for spec in sys.argv[3:]:
    file, rest = spec.split("=")
    size, flip = rest.split(":")
    data = bytearray(i % 251 for i in range(int(size)))
    if flip != "-":
        data[int(flip, 16)] ^= 0xff
    with open(os.path.join(d, file), "wb") as f:
        f.write(data)
EOF
}

if [ "$full" -eq 0 ]; then
	rm -rf "$out_root"
	mkdir -p "$out_root/proof" "$out_root/a" "$out_root/b"
	printf 'seed 00\ntrace 0 rnd x:y choice 3 -> 1 [1]\noutcome ok\n' >"$out_root/proof/a.txt"
	printf 'seed 00\ntrace 0 rnd x:y choice 3 -> 2 [1]\noutcome ok\n' >"$out_root/proof/b.txt"
	if proof="$($compare compare planted "$out_root/proof/a.txt" "$out_root/proof/b.txt")" ||
		[ "$proof" != "DIFFERENT planted: line 2: trace 0 rnd x:y choice 3 -> 1 [1] | trace 0 rnd x:y choice 3 -> 2 [1]" ]; then
		echo "determinism: self-proof failed: the comparator did not catch a planted trace difference" >&2
		exit 1
	fi
else
	out_root="$work_root/full"
	rm -rf "$out_root"
	p="$out_root/proof"
	plant "$p/ok" ok out.gbc=64:- spoiler.json=32:-
	plant "$p/same" ok out.gbc=64:- spoiler.json=32:-
	plant "$p/flip" ok out.gbc=64:2a spoiler.json=32:-
	plant "$p/spoiler" ok out.gbc=64:- spoiler.json=32:11
	plant "$p/nospoiler" ok out.gbc=64:-
	plant "$p/extra" ok out.gbc=64:- spoiler.json=32:- extra.bin=4:-
	plant "$p/trace" ok out.gbc=64:- spoiler.json=32:-
	sed 's/-> 1 \[1\]/-> 2 [1]/' "$p/ok/run.txt" >"$p/trace/run.txt"
	plant "$p/plain-ok" ok out.gbc=64:- spoiler.json=32:-
	plant "$p/plain-retry" retry out.gbc=64:- spoiler.json=32:-
	expect "an identical pair" "same planted" $compare compare-dirs planted "$p/ok" "$p/same"
	expect "a flipped ROM byte" "DIFFERENT planted: out.gbc: 1 bytes differ, first at 0x2a" \
		$compare compare-dirs planted "$p/ok" "$p/flip"
	expect "a different spoiler" "DIFFERENT planted: spoiler.json: 1 bytes differ, first at 0x11" \
		$compare compare-dirs planted "$p/ok" "$p/spoiler"
	expect "a missing spoiler" "DIFFERENT planted: only in A: spoiler.json" \
		$compare compare-dirs planted "$p/ok" "$p/nospoiler"
	expect "an extra file" "DIFFERENT planted: only in B: extra.bin" \
		$compare compare-dirs planted "$p/ok" "$p/extra"
	expect "a different trace line" \
		"DIFFERENT planted: run.txt: line 2: trace 0 rnd x:y choice 3 -> 1 [1] | trace 0 rnd x:y choice 3 -> 2 [1]" \
		$compare compare-dirs planted "$p/ok" "$p/trace"
	expect "an equal plain run" "same planted (plain)" $compare compare-plain planted "$p/ok" "$p/plain-ok"
	expect "a plain retry against a driver ok" "DIFFERENT planted (plain): outcome ok vs retry" \
		$compare compare-plain planted "$p/ok" "$p/plain-retry"
	rm -rf "$p"
	untracked() { git -C "$repo_dir" status --porcelain --untracked-files=all | sed -n 's/^?? //p' | sort; }
	untracked >"$out_root/untracked.before"
fi

"$oracle_dir/setup.sh"
LADXR_DIR="$clone_b" "$oracle_dir/setup.sh" --fresh

if [ "$full" -eq 1 ]; then
	tagged="$work_root/tagged"
	rm -rf "$tagged"
	mkdir -p "$tagged"
	git -C "$tagged" init -q
	git -C "$tagged" -c user.name=determinism -c user.email=determinism@invalid \
		commit -q --allow-empty -m "tagged work root"
	git -C "$tagged" tag determinism-tag

	if [ "$all" -eq 1 ]; then
		set --
		case_lines="$(python3 -I "$oracle_dir/ladx_cases.py" list)"
	else
		set -- --cases "$oracle_dir/determinism_cases.txt"
		case_lines="$($compare cases "$oracle_dir/determinism_cases.txt")"
	fi
	"$oracle_dir/gen_golden.sh" --out "$out_root/a" --hashseed 0 "$@"
	"$oracle_dir/gen_golden.sh" --out "$out_root/b" --hashseed 1 --ladxr "$clone_b" --work "$tagged" "$@"
	status=0
	count=0
	names=
	while read -r name seed settings; do
		# python3 without -I or -E, which would drop PYTHONHASHSEED.
		# shellcheck disable=SC2086
		PYTHONHASHSEED=0 PYTHONDONTWRITEBYTECODE=1 python3 "$oracle_dir/oracle_run.py" --plain \
			--rom "$repo_dir/tmp/ladx/base.gbc" --dir "$out_root/plain/$name" --work "$work_root" \
			"$oracle_dir/ladxr" "$seed" $settings </dev/null
		$compare compare-dirs "$name" "$out_root/a/$name" "$out_root/b/$name" || status=1
		$compare compare-plain "$name" "$out_root/a/$name" "$out_root/plain/$name" >"$out_root/plain.verdict" ||
			{
				cat "$out_root/plain.verdict"
				status=1
			}
		if ! $compare varied --describe "$out_root/a/$name/meta.txt" "$out_root/b/$name/meta.txt"; then
			echo "determinism: the two runs did not vary ($name)"
			status=1
		fi
		count=$((count + 1))
		names="$names $name"
	done <<EOF
$case_lines
EOF
	untracked >"$out_root/untracked.after"
	strays="$(comm -13 "$out_root/untracked.before" "$out_root/untracked.after")"
	if [ -n "$strays" ]; then
		printf '%s\n' "$strays" | sed 's/^/STRAY /'
		status=1
	fi
	[ "$status" -eq 0 ] || exit 1
	echo "determinism: $count cases identical with the ROM (PYTHONHASHSEED 0 vs 1, tests/ladx/oracle/ladxr vs ${clone_b#"$repo_dir/"}, untagged vs tagged work directory), driver equal to LADXR's CLI"
	exit 0
fi

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
