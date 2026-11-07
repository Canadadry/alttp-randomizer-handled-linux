#!/bin/sh
# Usage: selftest.sh [discovery]
# With "discovery", runs only the case-discovery checks (fast, run by make test).
# Exit 0 when every check passes, 1 on the first failure.
set -eu
export LC_ALL=C

here="$(cd "$(dirname "$0")" && pwd)"
. "$here/cases.sh"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
	echo "selftest: $1" >&2
	exit 1
}

expect() {
	[ "$2" = "$3" ] || fail "$1: expected [$3], got [$2]"
}

nl='
'

mkdir -p "$tmp/cases/b-1" "$tmp/cases/B-2" "$tmp/cases/a-0" "$tmp/cases/no-config"
for c in b-1 B-2 a-0; do : >"$tmp/cases/$c/config.json"; done

expect "sorted listing" "$(discover_cases "$tmp/cases" prog)" "B-2${nl}a-0${nl}b-1"

expect "named cases" "$(discover_cases "$tmp/cases" prog b-1 a-0)" "b-1${nl}a-0"

status=0
out="$(discover_cases "$tmp/cases" prog a-0 no-config 2>"$tmp/err")" || status=$?
expect "unknown case status" "$status" 2
expect "unknown case stdout" "$out" ""
expect "unknown case stderr" "$(cat "$tmp/err")" "prog: no such case: no-config"

mkdir "$tmp/empty"
status=0
out="$(discover_cases "$tmp/empty" prog)" || status=$?
expect "empty status" "$status" 0
expect "empty stdout" "$out" ""

status=0
out="$(discover_cases "$tmp/missing" prog)" || status=$?
expect "missing dir status" "$status" 0
expect "missing dir stdout" "$out" ""

mkdir -p "$tmp/combos/c-0" "$tmp/combos/a-0"
for c in c-0 a-0; do : >"$tmp/combos/$c/config.json"; done

expect "two-dir listing" "$(locate_cases "$tmp/cases" "$tmp/empty" prog)" \
	"$tmp/cases/B-2${nl}$tmp/cases/a-0${nl}$tmp/cases/b-1"

expect "two-dir named" "$(locate_cases "$tmp/cases" "$tmp/combos" prog c-0 b-1)" \
	"$tmp/combos/c-0${nl}$tmp/cases/b-1"

status=0
out="$(locate_cases "$tmp/cases" "$tmp/combos" prog b-1 a-0 2>"$tmp/err")" || status=$?
expect "case in both status" "$status" 2
expect "case in both stdout" "$out" ""
expect "case in both stderr" "$(cat "$tmp/err")" "prog: case in both cases/ and combos/: a-0"

status=0
out="$(locate_cases "$tmp/cases" "$tmp/combos" prog c-0 no-config 2>"$tmp/err")" || status=$?
expect "two-dir unknown status" "$status" 2
expect "two-dir unknown stdout" "$out" ""
expect "two-dir unknown stderr" "$(cat "$tmp/err")" "prog: no such case: no-config"

rm -r "$tmp/combos/a-0"
expect "two-dir full listing" "$(locate_cases "$tmp/cases" "$tmp/combos" prog)" \
	"$tmp/cases/B-2${nl}$tmp/cases/a-0${nl}$tmp/cases/b-1${nl}$tmp/combos/c-0"

expect "two-dir missing combos" "$(locate_cases "$tmp/cases" "$tmp/missing" prog)" \
	"$tmp/cases/B-2${nl}$tmp/cases/a-0${nl}$tmp/cases/b-1"

echo "selftest: discovery passed"
if [ "${1:-}" = discovery ]; then exit 0; fi

g="$tmp/gate"
mkdir -p "$g/cases/one" "$g/cases/two" "$g/out/two"
for c in one two; do
	echo '{}' >"$g/cases/$c/config.json"
	echo 1 >"$g/cases/$c/seed"
done
cat >"$g/cases/two/trace.txt" <<'EOF2'
0 1 alpha 0 6 -> 3
0 2 alpha 0 6 -> 4
0 3 beta 0 1 -> 1
0 4 gamma 0 2 -> 2
1 1 alpha 0 6 -> 5
1 2 beta 0 1 -> 0
EOF2
echo 1 >"$g/cases/two/attempts"
printf '{\n    "meta": {\n        "seed": 1\n    }\n}\n' >"$g/cases/two/spoiler.json"
printf 'stage alpha\nstage zero\nstage beta\nstage gamma\n' >"$g/stages.txt"
cat >"$g/gen" <<'EOF2'
#!/bin/sh
src="$FAKE_OUT/$(basename "$1")"
if [ -f "$src/sleep" ]; then sleep "$(cat "$src/sleep")"; fi
if [ -f "$src/stderr" ]; then cat "$src/stderr" >&2; fi
for f in "$src"/*; do if [ -f "$f" ]; then cp "$f" "$2"/; fi; done
if [ -f "$src/status" ]; then exit "$(cat "$src/status")"; fi
EOF2
chmod +x "$g/gen"

# gate <label> <claims> <expected status> <expected stdout> [<expected stderr>]
gate() {
	printf '%s' "$2" >"$g/claims.txt"
	status=0
	out="$(RANDO_CASES="$g/cases" RANDO_CLAIMS="$g/claims.txt" RANDO_STAGES="${stages_override:-$g/stages.txt}" RANDO_GROUPS="${groups_override:-$here/write_groups.txt}" \
		RANDO_GEN="${gen_override:-$g/gen}" FAKE_OUT="$g/out" "$here/run_e2e.sh" ${named:-} 2>"$tmp/err")" || status=$?
	expect "$1 status" "$status" "$3"
	expect "$1 stdout" "$out" "$4"
	expect "$1 stderr" "$(cat "$tmp/err")" "${5:-}"
}

# fake <file> <content>: what the fake generator writes for case two
fake() {
	printf '%s' "$2" >"$g/out/two/$1"
}

golden_lines() {
	head -n "$1" "$g/cases/two/trace.txt"
}

fake trace.txt ""
gate "no claim" "# comment$nl$nl" 0 "PENDING one${nl}PENDING two${nl}e2e: 0 pass, 0 divergent, 2 pending"

fake trace.txt "$(golden_lines 3)$nl"
gate "stage pass, prefix within attempt 0" "two beta$nl" 0 \
	"PENDING one${nl}PASS two (beta)${nl}e2e: 1 pass, 0 divergent, 1 pending"

fake trace.txt "$(golden_lines 4)$nl"
gate "prefix ends at the last attempt-0 line" "two gamma$nl" 0 \
	"PENDING one${nl}PASS two (gamma)${nl}e2e: 1 pass, 0 divergent, 1 pending"

fake trace.txt "$(cat "$g/cases/two/trace.txt")${nl}2 1 alpha 0 6 -> 0$nl"
gate "longer C trace" "two alpha$nl" 0 \
	"PENDING one${nl}PASS two (alpha)${nl}e2e: 1 pass, 0 divergent, 1 pending"

fake trace.txt "0 1 alpha 0 6 -> 3${nl}0 2 alpha 0 6 -> 5${nl}0 3 beta 0 1 -> 1$nl"
gate "first differing draw" "two beta$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  first differing draw:${nl}    golden: 0 2 alpha 0 6 -> 4${nl}    c: 0 2 alpha 0 6 -> 5${nl}e2e: 0 pass, 1 divergent, 1 pending"

fake trace.txt "$(golden_lines 2)$nl"
gate "short C trace" "two gamma$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  c trace ends after 2 draws, claim needs 4${nl}e2e: 0 pass, 1 divergent, 1 pending"

rm "$g/out/two/trace.txt"
gate "no C trace" "two alpha$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  c trace ends after 0 draws, claim needs 2${nl}e2e: 0 pass, 1 divergent, 1 pending"

gen_override="$g/missing"
gate "missing generator" "two alpha$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  no C generator: $g/missing${nl}e2e: 0 pass, 1 divergent, 1 pending"
gen_override=

cp "$g/cases/two/trace.txt" "$g/cases/two/attempts" "$g/cases/two/spoiler.json" "$g/out/two/"
gate "all pass" "two all$nl" 0 \
	"PENDING one${nl}PASS two (all)${nl}e2e: 1 pass, 0 divergent, 1 pending"

named=two
gate "one case named" "two all$nl" 0 "PASS two (all)${nl}e2e: 1 pass, 0 divergent, 0 pending"
named=

fake status 3
gate "all, generator fails" "two all$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  C generator exited 3${nl}e2e: 0 pass, 1 divergent, 1 pending"
rm "$g/out/two/status"

fake attempts "0$nl"
gate "attempts mismatch" "two all$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  attempts: first differing line 1:${nl}    golden: 1${nl}    c: 0${nl}e2e: 0 pass, 1 divergent, 1 pending"
cp "$g/cases/two/attempts" "$g/out/two/"

fake trace.txt "$(golden_lines 5)$nl"
gate "all, short C trace" "two all$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  c trace ends after 5 draws, claim needs 6${nl}e2e: 0 pass, 1 divergent, 1 pending"
cp "$g/cases/two/trace.txt" "$g/cases/two/attempts" "$g/cases/two/spoiler.json" "$g/out/two/"

fake spoiler.json "{$nl    \"meta\": {$nl        \"seed\": 2$nl    }$nl}$nl"
gate "spoiler mismatch" "two all$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  spoiler.json: first differing line 3:${nl}    golden:         \"seed\": 1${nl}    c:         \"seed\": 2${nl}e2e: 0 pass, 1 divergent, 1 pending"

rm "$g/out/two/spoiler.json"
gate "missing C spoiler" "two all$nl" 1 \
	"PENDING one${nl}DIVERGENT two${nl}  c wrote no spoiler.json${nl}e2e: 0 pass, 1 divergent, 1 pending"

c="$g/claims.txt"
gate "malformed claim" "two$nl" 2 "" "run_e2e.sh: $c:1: malformed line"
gate "malformed claim, four fields" "two all items x$nl" 2 "" "run_e2e.sh: $c:1: malformed line"
gate "claimed case unknown" "# x${nl}three all$nl" 2 "" "run_e2e.sh: $c:2: no such case: three"
gate "duplicate claim" "two all${nl}two beta$nl" 2 "" "run_e2e.sh: $c:2: duplicate case: two"
gate "unknown stage" "two delta$nl" 2 "" "run_e2e.sh: $c:1: unknown stage: delta"
gate "zero-draw stage" "two zero$nl" 2 "" "run_e2e.sh: $c:1: zero makes no draw in attempt 0 of two"

s="$g/stages.txt"
printf 'stage beta\nstage alpha\nstage zero\nstage gamma\n' >"$s"
gate "golden out of table order" "two all$nl" 2 "" "run_e2e.sh: two: golden stage beta out of table order"
gate "out of order but unclaimed" "" 0 "PENDING one${nl}PENDING two${nl}e2e: 0 pass, 0 divergent, 2 pending"
printf 'stage alpha\nstage zero\nstage gamma\n' >"$s"
gate "golden stage not in table" "two alpha$nl" 2 "" "run_e2e.sh: two: golden stage beta not in table"

printf 'stage alpha\nstage\n' >"$s"
gate "malformed stage line" "" 2 "" "run_e2e.sh: $s:2: malformed line"
printf 'stage alpha\nstep beta\n' >"$s"
gate "unknown stage-table line" "" 2 "" "run_e2e.sh: $s:2: malformed line"
printf 'stage alpha\nstage alpha\n' >"$s"
gate "duplicate stage" "" 2 "" "run_e2e.sh: $s:2: duplicate stage: alpha"
printf 'stage alpha\ncheck delta pool\n' >"$s"
gate "check on unknown stage" "" 2 "" "run_e2e.sh: $s:2: check names unknown stage: delta"
printf 'stage alpha\ncheck alpha lines x\n' >"$s"
gate "unknown check kind" "" 2 "" "run_e2e.sh: $s:2: unknown check kind: lines"
printf 'stage alpha\ncheck alpha section\n' >"$s"
gate "section without name" "" 2 "" "run_e2e.sh: $s:2: malformed line"
printf 'stage alpha\ncheck alpha attempts x\n' >"$s"
gate "attempts with an argument" "" 2 "" "run_e2e.sh: $s:2: malformed line"


cat >"$g/cases/two/spoiler.json" <<'EOF2'
{
    "Bosses": {
        "Eastern Palace": "Armos Knights",
        "Ganon": "Ganon"
    },
    "Eastern Palace": {
        "Eastern Palace - Big Chest:1": "Bow:1",
        "Eastern Palace - Prize:1": "PendantOfCourage:1"
    },
    "Light World": {
        "Link's House:1": "Arrow:1",
        "Sahasrala:1": "Arrow:1",
        "Uncle:1": "Sword:1"
    },
    "Special": {
        "Misery Mire Medallion:1": "Ether:1",
        "Waterfall Bottle:1": "Bottle:1"
    },
    "meta": {
        "seed": 1
    },
    "playthrough": {
        "1": {
            "Eastern Palace": {
                "Eastern Palace - Prize:1": "PendantOfCourage:1"
            }
        }
    }
}
EOF2
cat >"$s" <<'EOF2'
stage alpha
check zero section Bosses
stage zero
check alpha keys *Medallion:1
stage beta
check beta keys * - Prize:1
stage gamma
check gamma pool
EOF2

bosses='    "Bosses": {
        "Eastern Palace": "Armos Knights",
        "Ganon": "Ganon"
    },'
prize='    "Eastern Palace": {
        "Eastern Palace - Prize:1": "PendantOfCourage:1"
    },'
medallion='    "Special": {
        "Misery Mire Medallion:1": "Ether:1"
    }'
# partial <section>...: the C partial spoiler holding these section blocks
partial() {
	printf '{\n' >"$g/out/two/spoiler.json"
	for b in "$@"; do printf '%s\n' "$b" >>"$g/out/two/spoiler.json"; done
	printf '}\n' >>"$g/out/two/spoiler.json"
}

named=two
rm -f "$g/out/two/attempts"
fake trace.txt "$(golden_lines 3)$nl"
partial "$bosses" "$prize" "$medallion"
gate "checks pass, zero-draw stage's check due, later check ignored" "two beta$nl" 0 \
	"PASS two (beta)${nl}e2e: 1 pass, 0 divergent, 0 pending"

partial "$(printf '%s' "$bosses" | sed 's/"Ganon": "Ganon"/"Ganon": "Agahnim"/')" "$prize" "$medallion"
gate "section differs" "two beta$nl" 1 \
	"DIVERGENT two${nl}  zero: section Bosses:${nl}    golden:         \"Ganon\": \"Ganon\"${nl}    c:         \"Ganon\": \"Agahnim\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$prize" "$medallion"
gate "section missing in C" "two beta$nl" 1 \
	"DIVERGENT two${nl}  zero: section Bosses:${nl}    c lacks section Bosses${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$prize" "$(printf '%s' "$medallion" | sed 's/Ether/Bombos/')"
gate "keys differ, reported before a later failing check" "two beta$nl" 1 \
	"DIVERGENT two${nl}  alpha: keys *Medallion:1:${nl}    golden: [Special] \"Misery Mire Medallion:1\": \"Ether:1\"${nl}    c: [Special] \"Misery Mire Medallion:1\": \"Bombos:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$bosses" "$medallion"
gate "key missing in C" "two beta$nl" 1 \
	"DIVERGENT two${nl}  beta: keys * - Prize:1:${nl}    c lacks [Eastern Palace] \"Eastern Palace - Prize:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$bosses" "$prize" '    "Light World": {
        "Sahasrala - Prize:1": "Arrow:1"
    },' "$medallion"
gate "extra key in C" "two beta$nl" 1 \
	"DIVERGENT two${nl}  beta: keys * - Prize:1:${nl}    golden: <end>${nl}    c: [Light World] \"Sahasrala - Prize:1\": \"Arrow:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake trace.txt "0 1 alpha 0 6 -> 0$nl"
gate "trace checked before spoiler checks" "two beta$nl" 1 \
	"DIVERGENT two${nl}  first differing draw:${nl}    golden: 0 1 alpha 0 6 -> 3${nl}    c: 0 1 alpha 0 6 -> 0${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake trace.txt "$(golden_lines 4)$nl"
partial "$bosses" "$prize" "$medallion"
gate "missing pool.txt" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    c wrote no pool.txt${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake pool.txt "Sword:1${nl}Arrow:1${nl}Bow:1${nl}Arrow:1$nl"
gate "pool pass" "two gamma$nl" 0 "PASS two (gamma)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake pool.txt "Sword:1${nl}Arrow:1${nl}Bow:1$nl"
gate "pool lacks an item" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: Arrow:1 x2${nl}    c: Arrow:1 x1${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake pool.txt "Sword:1${nl}Arrow:1${nl}Bow:1${nl}Arrow:1${nl}Bomb:1$nl"
gate "pool has an extra item" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: Bomb:1 x0${nl}    c: Bomb:1 x1${nl}e2e: 0 pass, 1 divergent, 0 pending"

uncle='    "Light World": {
        "Uncle:1": "Sword:1"
    },'
partial "$bosses" "$prize" "$uncle" "$medallion"
fake pool.txt "Arrow:1${nl}Bow:1${nl}Arrow:1$nl"
gate "pre-fill leaves the pool" "two gamma$nl" 0 "PASS two (gamma)${nl}e2e: 1 pass, 0 divergent, 0 pending"

partial "$bosses" "$prize" "$(printf '%s' "$uncle" | sed 's/Sword/Bow/')" "$medallion"
gate "pre-fill differs" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: [Light World] \"Uncle:1\": \"Sword:1\"${nl}    c: [Light World] \"Uncle:1\": \"Bow:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$bosses" "$prize" '    "Light World": {
        "Zora:1": "Arrow:1"
    },' "$medallion"
gate "pre-fill on a key the golden lacks" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: <none>${nl}    c: [Light World] \"Zora:1\": \"Arrow:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$bosses" "$prize" '    "Light World": {
        "Zora:1": "Arrow:1",
        "Uncle:1": "Bow:1",
        "Witch:1": "Arrow:1"
    },' "$medallion"
gate "first offending pre-fill, bytewise" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: [Light World] \"Uncle:1\": \"Sword:1\"${nl}    c: [Light World] \"Uncle:1\": \"Bow:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$bosses" "$prize" '    "Eastern Palace": {
        "Eastern Palace - Big Chest:1": "Nothing"
    },
    "Light World": {
        "Link'"'"'s House:1": "Nothing",
        "Sahasrala:1": "Nothing",
        "Uncle:1": "Nothing"
    },' "$medallion"
fake pool.txt "Sword:1${nl}Arrow:1${nl}Bow:1${nl}Arrow:1$nl"
gate "Nothing is no pre-fill" "two gamma$nl" 0 "PASS two (gamma)${nl}e2e: 1 pass, 0 divergent, 0 pending"

partial "$bosses" "$prize" "$uncle" "$medallion"
fake pool.txt "Arrow:1${nl}Bow:1${nl}Arrow:1${nl}PendantOfCourage:1$nl"
gate "a due keys entry is no pre-fill" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: pool:${nl}    golden: PendantOfCourage:1 x0${nl}    c: PendantOfCourage:1 x1${nl}e2e: 0 pass, 1 divergent, 0 pending"

partial "$prize" "$(printf '%s' "$medallion" | sed 's/Ether/Bombos/')"
gate "a later stage's failing check is not due" "two alpha$nl" 1 \
	"DIVERGENT two${nl}  alpha: keys *Medallion:1:${nl}    golden: [Special] \"Misery Mire Medallion:1\": \"Ether:1\"${nl}    c: [Special] \"Misery Mire Medallion:1\": \"Bombos:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake trace.txt "$(golden_lines 3)"
gate "trace prefix lacks its last newline" "two beta$nl" 1 \
	"DIVERGENT two${nl}  first differing draw: line endings differ${nl}e2e: 0 pass, 1 divergent, 0 pending"

cp "$g/cases/two/trace.txt" "$g/cases/two/spoiler.json" "$g/out/two/"
fake attempts "1"
gate "attempts lack their newline" "two all$nl" 1 \
	"DIVERGENT two${nl}  attempts: line endings differ${nl}e2e: 0 pass, 1 divergent, 0 pending"

cp "$g/cases/two/attempts" "$g/out/two/"
gate "all pass after stage checks" "two all$nl" 0 "PASS two (all)${nl}e2e: 1 pass, 0 divergent, 0 pending"

cat >"$g/gen-args" <<'EOF2'
#!/bin/sh
printf '%s %s %s\n' "$#" "$(basename "$1")" "${3-}" >"$FAKE_ARGS"
exec "$(dirname "$0")/gen" "$@"
EOF2
chmod +x "$g/gen-args"
export FAKE_ARGS="$tmp/args"
gen_override="$g/gen-args"
gate "all claim, generator arguments" "two all$nl" 0 "PASS two (all)${nl}e2e: 1 pass, 0 divergent, 0 pending"
expect "all claim passes no stage" "$(cat "$FAKE_ARGS")" "2 two "
fake trace.txt "$(golden_lines 3)$nl"
gate "stage claim, generator arguments" "two beta$nl" 0 "PASS two (beta)${nl}e2e: 1 pass, 0 divergent, 0 pending"
expect "stage claim passes its stage" "$(cat "$FAKE_ARGS")" "3 two beta"
gen_override=

printf 'stage alpha\nstage beta\nstage gamma\ncheck gamma pool\ncheck gamma keys Zora:1\n' >"$s"
fake trace.txt "$(golden_lines 4)$nl"
partial '    "Light World": {
        "Zora:1": "Arrow:1"
    },'
fake pool.txt "Sword:1${nl}Arrow:1${nl}Bow:1${nl}Arrow:1${nl}PendantOfCourage:1$nl"
gate "a pre-fill on a due keys glob is left to that check" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  gamma: keys Zora:1:${nl}    golden: <end>${nl}    c: [Light World] \"Zora:1\": \"Arrow:1\"${nl}e2e: 0 pass, 1 divergent, 0 pending"

printf 'stage alpha\nstage beta\ncheck beta attempts\nstage gamma\n' >"$s"
fake trace.txt "$(golden_lines 3)$nl"
cp "$g/cases/two/attempts" "$g/out/two/"
gate "attempts pass" "two beta$nl" 0 "PASS two (beta)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake attempts "0$nl"
gate "attempts differ" "two beta$nl" 1 \
	"DIVERGENT two${nl}  beta: attempts:${nl}    golden: 1${nl}    c: 0${nl}e2e: 0 pass, 1 divergent, 0 pending"

rm "$g/out/two/attempts"
gate "C wrote no attempts" "two beta$nl" 1 \
	"DIVERGENT two${nl}  beta: attempts:${nl}    c wrote no attempts${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake trace.txt "$(golden_lines 4)$nl"
gate "a later claim still owes attempts" "two gamma$nl" 1 \
	"DIVERGENT two${nl}  beta: attempts:${nl}    c wrote no attempts${nl}e2e: 0 pass, 1 divergent, 0 pending"
cp "$g/cases/two/attempts" "$g/out/two/"

printf 'stage alpha\ncheck alpha pool\nstage beta\nstage gamma\n' >"$s"
partial "$bosses" "$prize" "$medallion"
fake pool.txt "Bomb:1$nl"
fake trace.txt "$(golden_lines 3)$nl"
gate "pool not due to a later claim" "two beta$nl" 0 "PASS two (beta)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake trace.txt "$(golden_lines 2)$nl"
gate "pool still due at its own stage" "two alpha$nl" 1 \
	"DIVERGENT two${nl}  alpha: pool:${nl}    golden: Arrow:1 x2${nl}    c: Arrow:1 x0${nl}e2e: 0 pass, 1 divergent, 0 pending"

# The committed stages.txt: a fill claim compares the whole placement.
stages_override="$here/stages.txt"
mkdir "$g/keep"
cp "$g/cases/two/trace.txt" "$g/cases/two/spoiler.json" "$g/keep/"
cat >"$g/cases/two/trace.txt" <<'EOF2'
0 1 crystals 0 6 -> 3
0 2 fill 0 6 -> 4
1 1 crystals 0 6 -> 5
1 2 fill 0 6 -> 1
EOF2
cat >"$g/cases/two/spoiler.json" <<'EOF2'
{
    "Eastern Palace": {
        "Eastern Palace - Big Chest:1": "Bow:1"
    },
    "Light World": {
        "Link's House:1": "Arrow:1",
        "Uncle:1": "Sword:1"
    },
    "meta": {
        "seed": 1
    },
    "playthrough": {
        "1": {
            "Light World": {
                "Uncle:1": "Sword:1"
            }
        }
    }
}
EOF2
cp "$g/cases/two/trace.txt" "$g/cases/two/attempts" "$g/cases/two/spoiler.json" "$g/out/two/"
rm -f "$g/out/two/pool.txt"
gate "fill claim, real table, pass" "two fill$nl" 0 "PASS two (fill)${nl}e2e: 1 pass, 0 divergent, 0 pending"

sed 's/"Link'"'"'s House:1": "Arrow:1"/"Link'"'"'s House:1": "Bomb:1"/' "$g/cases/two/spoiler.json" >"$g/out/two/spoiler.json"
gate "fill claim, real table, placement differs" "two fill$nl" 1 \
	"DIVERGENT two${nl}  fill: section Light World:${nl}    golden:         \"Link's House:1\": \"Arrow:1\",${nl}    c:         \"Link's House:1\": \"Bomb:1\",${nl}e2e: 0 pass, 1 divergent, 0 pending"

cp "$g/cases/two/spoiler.json" "$g/out/two/"
fake attempts "0$nl"
gate "fill claim, real table, attempts differ" "two fill$nl" 1 \
	"DIVERGENT two${nl}  fill: attempts:${nl}    golden: 1${nl}    c: 0${nl}e2e: 0 pass, 1 divergent, 0 pending"
stages_override=
cp "$g/keep/trace.txt" "$g/keep/spoiler.json" "$g/cases/two/"
named=

mkdir -p "$g/cases/one" "$g/out/one"
printf 'stage alpha\nstage beta\nstage gamma\n' >"$s"
for f in trace.txt attempts spoiler.json; do cp "$g/cases/two/$f" "$g/cases/one/"; cp "$g/cases/two/$f" "$g/out/one/"; done
cp "$g/cases/two/trace.txt" "$g/cases/two/attempts" "$g/cases/two/spoiler.json" "$g/out/two/"
printf '0.2' >"$g/out/one/sleep"
gate "slowest first case still reported first" "two all${nl}one all$nl" 0 \
	"PASS one (all)${nl}PASS two (all)${nl}e2e: 2 pass, 0 divergent, 0 pending"

printf '0\n' >"$g/out/one/attempts"
fake spoiler.json "{$nl}$nl"
gate "two divergent cases keep their own details" "one all${nl}two all$nl" 1 \
	"DIVERGENT one${nl}  attempts: first differing line 1:${nl}    golden: 1${nl}    c: 0${nl}DIVERGENT two${nl}  spoiler.json: first differing line 2:${nl}    golden:     \"Bosses\": {${nl}    c: }${nl}e2e: 0 pass, 2 divergent, 0 pending"

cp "$g/cases/two/attempts" "$g/out/one/"
cp "$g/cases/two/spoiler.json" "$g/out/two/"
printf 'one says\n' >"$g/out/one/stderr"
printf '4' >"$g/out/one/status"
fake stderr "two says$nl"
gate "generator stderr reaches stderr, grouped in case order" "one all${nl}two all$nl" 1 \
	"DIVERGENT one${nl}  C generator exited 4${nl}PASS two (all)${nl}e2e: 1 pass, 1 divergent, 0 pending" \
	"one says${nl}two says"

# Write-log claims, on a test table of three groups.
rm -f "$g/out/one/stderr" "$g/out/one/status" "$g/out/two/stderr"
named=two
w="$g/write_groups.txt"
printf '# test table\ngroup first a\ngroup middle *\ngroup last z\n' >"$w"
groups_override="$w"
cat >"$g/cases/two/rom_writes.txt" <<'EOF2'
000001 01 a
000002 02 a
000010 0A m1
000011 0B0C m2
000020 FF z
EOF2
golden_writes() {
	head -n "$1" "$g/cases/two/rom_writes.txt"
}

fake rom_writes.txt "$(cat "$g/cases/two/rom_writes.txt")$nl"
gate "write claim pass" "two all middle$nl" 0 "PASS two (all middle)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake rom_writes.txt "$(golden_writes 2)${nl}000010 0A m1${nl}000011 0B0D m2${nl}000020 FF z$nl"
gate "divergence in the claimed group" "two all middle$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: middle: first differing write 4:${nl}    golden: 000011 0B0C m2${nl}    c: 000011 0B0D m2${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt "$(golden_writes 4)${nl}000020 FE z$nl"
gate "divergence in a later, unclaimed group" "two all middle$nl" 0 \
	"PASS two (all middle)${nl}e2e: 1 pass, 0 divergent, 0 pending"

gate "no write claim, log not compared" "two all$nl" 0 "PASS two (all)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake rom_writes.txt "000001 01 a${nl}000002 03 a${nl}000010 0A m1${nl}000011 0B0C m2$nl"
gate "earlier group's divergence under a later claim" "two all middle$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: first: first differing write 2:${nl}    golden: 000002 02 a${nl}    c: 000002 03 a${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt "$(golden_writes 3)$nl"
gate "short C log" "two all middle$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: c ends after 3 writes, claim needs 4${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt ""
gate "empty C log" "two all first$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: c ends after 0 writes, claim needs 2${nl}e2e: 0 pass, 1 divergent, 0 pending"

rm "$g/out/two/rom_writes.txt"
gate "missing C log" "two all first$nl" 1 \
	"DIVERGENT two${nl}  c wrote no rom_writes.txt${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt "$(cat "$g/cases/two/rom_writes.txt")$nl"
gate "last-group claim pass" "two all last$nl" 0 "PASS two (all last)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake rom_writes.txt "$(cat "$g/cases/two/rom_writes.txt")${nl}000030 01 m1$nl"
gate "last-group claim, one extra C write" "two all last$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: last: first differing write 6:${nl}    golden: <end>${nl}    c: 000030 01 m1${nl}e2e: 0 pass, 1 divergent, 0 pending"
gate "extra C write ignored before the last group" "two all middle$nl" 0 \
	"PASS two (all middle)${nl}e2e: 1 pass, 0 divergent, 0 pending"

fake rom_writes.txt "$(cat "$g/cases/two/rom_writes.txt")"
gate "C log lacks its last newline" "two all last$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: line endings differ${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt "$(golden_writes 2)"
gate "C prefix lacks its last newline" "two all first$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: line endings differ${nl}e2e: 0 pass, 1 divergent, 0 pending"

fake rom_writes.txt "000001 FF a$nl"
fake spoiler.json "{$nl}$nl"
gate "seed-level failure reported alone" "two all middle$nl" 1 \
	"DIVERGENT two${nl}  spoiler.json: first differing line 2:${nl}    golden:     \"Bosses\": {${nl}    c: }${nl}e2e: 0 pass, 1 divergent, 0 pending"
cp "$g/cases/two/spoiler.json" "$g/out/two/"

fake rom_writes.txt "$(cat "$g/cases/two/rom_writes.txt")$nl"
gen_override="$g/gen-args"
gate "write claim, generator arguments" "two all last$nl" 0 "PASS two (all last)${nl}e2e: 1 pass, 0 divergent, 0 pending"
expect "write claim passes no stage" "$(cat "$FAKE_ARGS")" "2 two "
gen_override=

gate "unknown write group" "two all fourth$nl" 2 "" "run_e2e.sh: $c:1: unknown write group: fourth"
gate "write group on a stage claim" "two beta first$nl" 2 "" "run_e2e.sh: $c:1: write group first needs an all claim"
cp "$g/cases/two/rom_writes.txt" "$g/keep/"
printf '000001 01 a\n000020 FF z\n' >"$g/cases/two/rom_writes.txt"
gate "group with no write in the golden" "two all middle$nl" 2 "" "run_e2e.sh: $c:1: middle makes no write in two"
printf '000010 0A m1\n000001 01 a\n' >"$g/cases/two/rom_writes.txt"
gate "golden write group out of table order" "two all middle$nl" 2 "" "run_e2e.sh: two: golden write group first out of table order"
gate "golden out of order without a write claim" "two all$nl" 2 "" "run_e2e.sh: two: golden write group first out of table order"
gate "golden out of order on a stage claim" "two beta$nl" 2 "" "run_e2e.sh: two: golden write group first out of table order"
gate "golden out of order but unclaimed" "" 0 "PENDING two${nl}e2e: 0 pass, 0 divergent, 1 pending"
rm "$g/cases/two/rom_writes.txt"
gate "write claim on a golden with no log" "two all first$nl" 2 "" "run_e2e.sh: two: cannot read $g/cases/two/rom_writes.txt"
gate "no golden log, no write claim" "two all$nl" 0 "PASS two (all)${nl}e2e: 1 pass, 0 divergent, 0 pending"
mv "$g/keep/rom_writes.txt" "$g/cases/two/"

printf 'group first a\ngroup\n' >"$w"
gate "malformed group line" "" 2 "" "run_e2e.sh: $w:2: malformed line"
printf 'group first a\nstage middle *\n' >"$w"
gate "unknown group-table line" "" 2 "" "run_e2e.sh: $w:2: malformed line"
printf 'group first a\ngroup first *\n' >"$w"
gate "duplicate group" "" 2 "" "run_e2e.sh: $w:2: duplicate group: first"
printf 'group first a b\ngroup middle *\ngroup last b\n' >"$w"
gate "tag in two groups" "" 2 "" "run_e2e.sh: $w:3: tag in two groups: b"
printf 'group first a\ngroup middle * m\n' >"$w"
gate "default beside another tag" "" 2 "" "run_e2e.sh: $w:2: * beside another tag"
printf 'group first a\ngroup last z\n' >"$w"
gate "no default group" "" 2 "" "run_e2e.sh: $w: no default group"
gate "no default group, with a claim" "two all$nl" 2 "" "run_e2e.sh: $w: no default group"
printf 'group first *\ngroup middle *\n' >"$w"
gate "two default groups" "" 2 "" "run_e2e.sh: $w:2: second default group: middle"
groups_override=

# The committed write_groups.txt: one write per group, a settings claim.
cat >"$g/cases/two/rom_writes.txt" <<'EOF2'
00E9B0 17 writeItem
180032 01 setGameState
180033 02 setSwordlessMode
181500 0102 writeCredits
0E0000 FFFF writeText
183000 00 writeInitialSram
180070 05 setTotalItemCount
180033 40 setHeartBeepSpeed
EOF2
sed 's/FFFF writeText/FFFE writeText/' "$g/cases/two/rom_writes.txt" >"$g/out/two/rom_writes.txt"
gate "settings claim, real table, later group differs" "two all settings$nl" 0 \
	"PASS two (all settings)${nl}e2e: 1 pass, 0 divergent, 0 pending"
sed 's/01 setGameState/00 setGameState/' "$g/cases/two/rom_writes.txt" >"$g/out/two/rom_writes.txt"
gate "settings claim, real table, setter differs" "two all settings$nl" 1 \
	"DIVERGENT two${nl}  rom_writes.txt: settings: first differing write 2:${nl}    golden: 180032 01 setGameState${nl}    c: 180032 00 setGameState${nl}e2e: 0 pass, 1 divergent, 0 pending"
cp "$g/cases/two/rom_writes.txt" "$g/out/two/"
gate "heart_beep claim, real table, pass" "two all heart_beep$nl" 0 \
	"PASS two (all heart_beep)${nl}e2e: 1 pass, 0 divergent, 0 pending"
named=

echo "selftest: gate passed"
