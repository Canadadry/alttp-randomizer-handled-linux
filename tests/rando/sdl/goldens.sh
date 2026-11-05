#!/bin/sh
# Usage: goldens.sh ALTTPR FONT SD_FIXTURE --write DIR | --compare DIR [--theme THEME]
# Plays every state of golden_states.txt into the SDL app (dummy video driver), each from a fresh copy
# of its fixture, and checks the captured text holds the state's strings. --write renders every state
# twice, refuses unless both runs agree byte for byte and DIR holds no BMP, then fills DIR with the
# BMPs and the environment fingerprint. --compare proves itself on planted goldens, checks the
# fingerprint, then renders each state once against DIR. golden_states.txt is tab-separated: name,
# fixture, frame cap, extra alttpr flags, keys, strings joined by " | "; "-" for no cap or no flags.
# --theme THEME is passed to every alttpr run.
set -eu
set -f
export LC_ALL=C
export SDL_VIDEODRIVER=dummy

usage() {
	echo "usage: goldens.sh ALTTPR FONT SD_FIXTURE --write DIR | --compare DIR [--theme THEME]" >&2
	exit 2
}

[ $# -eq 5 ] || { [ $# -eq 7 ] && [ "$6" = "--theme" ]; } || usage
case "$4" in --write | --compare) ;; *) usage ;; esac
abs() {
	echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}
bin="$(abs "$1")"
font="$(abs "$2")"
fixture="$(abs "$3")"
mode="$4"
goldens="$5"
theme_flag=""
[ $# -eq 5 ] || theme_flag="--theme $(cd "$7" && pwd)"
here="$(cd "$(dirname "$0")" && pwd)"
states="$here/golden_states.txt"
fingerprint_file=environment.txt
work="$(dirname "$bin")/goldens-work"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail() {
	echo "goldens: $*" >&2
	exit 1
}

# py ARGS...: the comparison and string checks; see each command below.
py() {
	python3 -I - "$@" <<'PY'
import os, sys

cmd, args = sys.argv[1], sys.argv[2:]

if cmd == "strings":
    # strings TEXT FIELD: prints each " | "-separated string of FIELD that TEXT lacks.
    text = open(args[0], encoding="utf-8").read()
    missing = [s for s in args[1].split(" | ") if s not in text]
    for s in missing:
        print(s)
    sys.exit(1 if missing else 0)

if cmd == "compare":
    # compare RENDERED GOLDENS FINGERPRINT STATE...: one line per DIFFERENT, MISSING or EXTRA.
    rendered, goldens, fp, names = args[0], args[1], args[2], args[3:]
    lines = []
    for name in names:
        golden = os.path.join(goldens, name + ".bmp")
        if not os.path.isfile(golden):
            lines.append("MISSING " + name)
        elif open(golden, "rb").read() != open(os.path.join(rendered, name + ".bmp"), "rb").read():
            lines.append("DIFFERENT " + name)
    expected = {n + ".bmp" for n in names} | {fp}
    for f in sorted(os.listdir(goldens)):
        if f not in expected:
            lines.append("EXTRA " + f)
    for line in lines:
        print(line)
    sys.exit(1 if lines else 0)

if cmd == "flip":
    # flip BMP: inverts the first pixel byte, past the header.
    data = bytearray(open(args[0], "rb").read())
    offset = int.from_bytes(data[10:14], "little")
    data[offset] ^= 0xFF
    open(args[0], "wb").write(bytes(data))
    sys.exit(0)

if cmd == "sha256":
    import hashlib
    print(hashlib.sha256(open(args[0], "rb").read()).hexdigest())
    sys.exit(0)

sys.exit("goldens: unknown command " + cmd)
PY
}

fingerprint() {
	for p in sdl SDL_ttf freetype2; do
		echo "$p $(pkg-config --modversion "$p" 2>/dev/null || echo none)"
	done
	if pkg-config --exists sdl2 2>/dev/null; then
		echo "sdl2 $(pkg-config --modversion sdl2)"
	fi
	echo "font $(basename "$font") $(py sha256 "$font")"
}

state_names() {
	while IFS='	' read -r name _rest; do
		[ -n "$name" ] && printf '%s\n' "$name"
	done <"$states"
}

# render OUT: plays every state into OUT/<state>.bmp and OUT/<state>.txt.
render() {
	out="$1"
	rm -rf "$out"
	mkdir -p "$out"
	while IFS='	' read -r name fx frames flags keys strings; do
		[ -n "$name" ] || continue
		template="$tmp/fixture-$fx"
		if [ ! -d "$template" ]; then
			"$fixture" "$template" "$fx" >"$tmp/flags-$fx" || fail "$name: sd_fixture $fx failed"
		fi
		sd="$tmp/sd-$name"
		rm -rf "$sd"
		cp -R "$template" "$sd"
		cap=""
		[ "$frames" = "-" ] || cap="--frames $frames"
		[ "$flags" != "-" ] || flags=""
		st=0
		# shellcheck disable=SC2046,SC2086
		"$bin" --sd "$sd" --font "$font" $theme_flag --seed 1 --date 2026-11-01T14:03:27 $(cat "$tmp/flags-$fx") $cap $flags \
			--shot "$out/$name.bmp" --shot-text "$out/$name.txt" -- $keys </dev/null >"$tmp/out" 2>"$tmp/err" || st=$?
		[ "$st" -eq 0 ] || { cat "$tmp/err" >&2; fail "$name: alttpr exited $st"; }
		rm -rf "$sd"
		if ! py strings "$out/$name.txt" "$strings" >"$tmp/missing"; then
			cat "$out/$name.txt" >&2
			while IFS= read -r s; do echo "UNREACHED $name: lacks $s" >&2; done <"$tmp/missing"
			exit 1
		fi
	done <"$states"
}

names="$(state_names)"
count="$(printf '%s\n' "$names" | wc -l | tr -d ' ')"

if [ "$mode" = "--write" ]; then
	if [ -d "$goldens" ] && [ -n "$(find "$goldens" -name '*.bmp' | head -n 1)" ]; then
		fail "$goldens already holds goldens: refused, goldens are never regenerated to get green"
	fi
	render "$work/run1"
	render "$work/run2"
	bad=0
	for n in $names; do
		cmp -s "$work/run1/$n.bmp" "$work/run2/$n.bmp" && cmp -s "$work/run1/$n.txt" "$work/run2/$n.txt" ||
			{ echo "NONDETERMINISTIC $n"; bad=1; }
	done
	[ "$bad" -eq 0 ] || exit 1
	mkdir -p "$goldens"
	for n in $names; do cp "$work/run1/$n.bmp" "$goldens/$n.bmp"; done
	fingerprint >"$goldens/$fingerprint_file"
	echo "goldens: $count states written to $goldens, rendered twice byte-identical"
	exit 0
fi

[ -f "$goldens/$fingerprint_file" ] || fail "no $fingerprint_file in $goldens (make sdl-goldens writes the goldens)"
sample="$(find "$goldens" -name '*.bmp' | sort | head -n 1)"
[ -n "$sample" ] || fail "no BMP in $goldens for the self-proof"
proof="$tmp/proof"
mkdir -p "$proof/rendered" "$proof/clean" "$proof/planted"
for d in rendered clean planted; do
	cp "$sample" "$proof/$d/a.bmp"
	cp "$sample" "$proof/$d/b.bmp"
	: >"$proof/$d/$fingerprint_file"
done
py compare "$proof/rendered" "$proof/clean" "$fingerprint_file" a b >"$proof/out" ||
	{ cat "$proof/out" >&2; fail "self-proof: an untouched copy does not compare equal"; }
py flip "$proof/planted/a.bmp"
rm "$proof/planted/b.bmp"
cp "$sample" "$proof/planted/planted.bmp"
if py compare "$proof/rendered" "$proof/planted" "$fingerprint_file" a b >"$proof/out"; then
	fail "self-proof: planted goldens compare equal"
fi
printf 'DIFFERENT a\nMISSING b\nEXTRA planted.bmp\n' >"$proof/want"
cmp -s "$proof/out" "$proof/want" ||
	{ cat "$proof/out" >&2; fail "self-proof: planted goldens did not give exactly their three lines"; }

pinned="$(tr '\n' ' ' <"$goldens/$fingerprint_file" | sed 's/ $//')"
host="$(fingerprint | tr '\n' ' ' | sed 's/ $//')"
if [ "$pinned" != "$host" ]; then
	echo "setup: goldens pinned to $pinned, this host is $host"
	exit 1
fi

render "$work/compare"
# shellcheck disable=SC2086
if ! py compare "$work/compare" "$goldens" "$fingerprint_file" $names; then
	exit 1
fi
echo "goldens: $count states byte-identical"
