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
	out="$(RANDO_CASES="$g/cases" RANDO_CLAIMS="$g/claims.txt" RANDO_STAGES="$g/stages.txt" \
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
gate "malformed claim, extra field" "two beta x$nl" 2 "" "run_e2e.sh: $c:1: malformed line"
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

echo "selftest: gate passed"
