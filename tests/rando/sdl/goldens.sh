#!/bin/sh
# Usage: goldens.sh ALTTPR THEME SD_FIXTURE --write DIR | --compare DIR | --draw DIR [--states FILE]
# Plays every state of golden_states.txt into the SDL app (dummy video driver), each from a fresh copy
# of its fixture, and checks the captured text holds the state's strings. --write renders every state
# twice, refuses unless both runs agree byte for byte and DIR holds no golden, then fills DIR with
# lossless PNGs of the BMP captures and the environment fingerprint. --compare proves itself on
# planted goldens, checks the fingerprint, then renders each state once and compares its decoded
# pixels with DIR's PNG. golden_states.txt is tab-separated: name,
# fixture, frame cap, extra alttpr flags, keys, strings joined by " | "; "-" for no cap or no flags.
# --draw renders each state once into DIR with any theme, checks each BMP is a 640x480 picture of more
# than one colour, and skips the strings, which are spelled on classic's grid; --states plays FILE,
# in the same format, instead of golden_states.txt (--draw only: goldens come from golden_states.txt).
# Every alttpr run gets --theme THEME; the fingerprint holds one font line per *.ttf of THEME.
set -eu
set -f
export LC_ALL=C
export SDL_VIDEODRIVER=dummy

usage() {
	echo "usage: goldens.sh ALTTPR THEME SD_FIXTURE --write DIR | --compare DIR | --draw DIR [--states FILE]" >&2
	exit 2
}

[ $# -eq 5 ] || { [ $# -eq 7 ] && [ "$4" = --draw ] && [ "$6" = --states ]; } || usage
case "$4" in --write | --compare | --draw) ;; *) usage ;; esac
abs() {
	echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
}
bin="$(abs "$1")"
theme="$(cd "$2" && pwd)"
fixture="$(abs "$3")"
mode="$4"
goldens="$5"
here="$(cd "$(dirname "$0")" && pwd)"
states="$here/golden_states.txt"
if [ $# -eq 7 ]; then
	[ -f "$7" ] || usage
	states="$(abs "$7")"
fi
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
	python3 -I - "$here" "$@" <<'PY'
import os, sys

sys.path.insert(0, sys.argv[1])
import theme_compare as codec

cmd, args = sys.argv[2], sys.argv[3:]


def pixels(path, decoder):
    try:
        return decoder(open(path, "rb").read())
    except codec.InputError:
        return None

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
        golden = os.path.join(goldens, name + ".png")
        if not os.path.isfile(golden):
            lines.append("MISSING " + name)
            continue
        want = pixels(golden, codec.decode_png)
        if want is None or want != pixels(os.path.join(rendered, name + ".bmp"), codec.decode_bmp):
            lines.append("DIFFERENT " + name)
    expected = {n + ".png" for n in names} | {fp}
    for f in sorted(os.listdir(goldens)):
        if f not in expected:
            lines.append("EXTRA " + f)
    for line in lines:
        print(line)
    sys.exit(1 if lines else 0)

if cmd == "flip":
    # flip PNG: inverts the red channel of the top-left pixel.
    width, height, rows = codec.decode_png(open(args[0], "rb").read())
    rows[0] = bytes([rows[0][0] ^ 0xFF]) + rows[0][1:]
    open(args[0], "wb").write(codec.encode_png((width, height, rows)))
    sys.exit(0)

if cmd == "bmp":
    # bmp PNG BMP: writes PNG's pixels as a 24-bit BMP, standing in for a capture in the self-proof.
    width, height, rows = codec.decode_png(open(args[0], "rb").read())
    open(args[1], "wb").write(codec.planted_bmp(width, height, rows))
    sys.exit(0)

if cmd == "png":
    # png BMP PNG: writes PNG, refused unless it decodes to exactly BMP's pixels.
    image = codec.decode_bmp(open(args[0], "rb").read())
    data = codec.encode_png(image)
    if codec.decode_png(data) != image:
        sys.exit("goldens: %s: the PNG does not decode to the BMP's pixels" % args[0])
    open(args[1], "wb").write(data)
    sys.exit(0)

if cmd == "picture":
    # picture BMP: fails unless BMP is a 640x480 picture of more than one colour.
    data = open(args[0], "rb").read()
    if len(data) < 54 or data[:2] != b"BM":
        sys.exit("not a BMP")
    w = int.from_bytes(data[18:22], "little", signed=True)
    h = abs(int.from_bytes(data[22:26], "little", signed=True))
    bpp = int.from_bytes(data[28:30], "little")
    if (w, h) != (640, 480):
        sys.exit("%dx%d, not 640x480" % (w, h))
    step = bpp // 8
    offset = int.from_bytes(data[10:14], "little")
    stride = (w * step + 3) & ~3
    first = data[offset:offset + step]
    for y in range(h):
        row = data[offset + y * stride:offset + y * stride + w * step]
        if row != first * w:
            sys.exit(0)
    sys.exit("a single colour")

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
	find "$theme" -maxdepth 1 -type f -name '*.ttf' | sort | while IFS= read -r f; do
		echo "font $(basename "$f") $(py sha256 "$f")"
	done
}

state_names() {
	while IFS='	' read -r name _rest; do
		[ -n "$name" ] && printf '%s\n' "$name"
	done <"$states"
}

# render OUT [nostrings]: plays every state into OUT/<state>.bmp and OUT/<state>.txt.
render() {
	out="$1"
	check_strings="${2:-strings}"
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
		"$bin" --sd "$sd" --theme "$theme" --seed 1 --date 2026-11-01T14:03:27 $(cat "$tmp/flags-$fx") $cap $flags \
			--shot "$out/$name.bmp" --shot-text "$out/$name.txt" -- $keys </dev/null >"$tmp/out" 2>"$tmp/err" || st=$?
		[ "$st" -eq 0 ] || { cat "$tmp/err" >&2; fail "$name: alttpr exited $st"; }
		rm -rf "$sd"
		if [ "$check_strings" != strings ]; then
			why="$(py picture "$out/$name.bmp" 2>&1)" || fail "$name: $why"
			continue
		fi
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
	if [ -d "$goldens" ] && [ -n "$(find "$goldens" \( -name '*.png' -o -name '*.bmp' \) | head -n 1)" ]; then
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
	for n in $names; do py png "$work/run1/$n.bmp" "$goldens/$n.png" || fail "$n: PNG refused"; done
	fingerprint >"$goldens/$fingerprint_file"
	echo "goldens: $count states written to $goldens, rendered twice byte-identical"
	exit 0
fi

if [ "$mode" = "--draw" ]; then
	mkdir -p "$goldens"
	render "$(cd "$goldens" && pwd)" nostrings
	echo "goldens: $count states drawn with $(basename "$theme") in $goldens"
	exit 0
fi

[ -f "$goldens/$fingerprint_file" ] || fail "no $fingerprint_file in $goldens (make sdl-goldens writes the goldens)"
sample="$(find "$goldens" -name '*.png' | sort | head -n 1)"
[ -n "$sample" ] || fail "no PNG in $goldens for the self-proof"
proof="$tmp/proof"
mkdir -p "$proof/rendered" "$proof/clean" "$proof/planted"
py bmp "$sample" "$proof/rendered/a.bmp"
cp "$proof/rendered/a.bmp" "$proof/rendered/b.bmp"
for d in clean planted; do
	cp "$sample" "$proof/$d/a.png"
	cp "$sample" "$proof/$d/b.png"
	: >"$proof/$d/$fingerprint_file"
done
py compare "$proof/rendered" "$proof/clean" "$fingerprint_file" a b >"$proof/out" ||
	{ cat "$proof/out" >&2; fail "self-proof: an untouched copy does not compare equal"; }
py flip "$proof/planted/a.png"
rm "$proof/planted/b.png"
cp "$sample" "$proof/planted/planted.png"
if py compare "$proof/rendered" "$proof/planted" "$fingerprint_file" a b >"$proof/out"; then
	fail "self-proof: planted goldens compare equal"
fi
printf 'DIFFERENT a\nMISSING b\nEXTRA planted.png\n' >"$proof/want"
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
echo "goldens: $count states pixel-identical"
