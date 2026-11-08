"""Usage: python3 -I theme_art.py normalise TABLE RAW OUT
       python3 -I theme_art.py check TABLE NEW THEME CHROMIUM
       python3 -I theme_art.py --self-test

TABLE is theme_art.txt: per line, tab-separated, the output file, the source mockup, its kind
(frame: checked in every mockup, written RGB; sprite: written RGBA), the clip box x y w h, the
scale, then the CSS selectors of the elements kept.
normalise: decodes RAW/<file> (theme_art.js's screenshot) of every line and writes OUT/<file>:
round(w x scale) x round(h x scale); a frame fully opaque, colour type 2; a sprite with every alpha
0 or 255 and one opaque pixel at least, colour type 6, transparent pixels stored as 0,0,0,0. IHDR,
one IDAT (filter 0, zlib level 9) and IEND only. One "theme-art: <file>: <problem>" line per
refusal, exit 1 if any.
check: compares the decoded pixels of NEW/<file> and THEME/<file> for every line; one "DIFFERENT
<file>", "MISSING <file>" (not in THEME) or "EXTRA <file>" (a PNG of THEME no line produces) line
per mismatch, then the summary naming CHROMIUM; exit 1 on any mismatch.
--self-test: proves the decoder, the encoder, the refusals and check on planted images; one line
per failing case, exit 1 if any, nothing printed when all pass.
"""

import contextlib
import io
import os
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import theme_compare as tc  # noqa: E402

InputError = tc.InputError
ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))


class Line:
    def __init__(self, fields):
        if len(fields) < 9 or fields[2] not in ("frame", "sprite"):
            raise InputError("table line %r: want file, mockup, frame|sprite, x y w h, scale, selectors"
                             % "\t".join(fields))
        self.file, self.mockup, self.kind = fields[0], fields[1], fields[2]
        try:
            self.box = [float(v) for v in fields[3:7]]
            self.scale = float(fields[7])
        except ValueError:
            raise InputError("table line of %s: box or scale not a number" % self.file)
        self.selectors = fields[8:]
        self.size = (round(self.box[2] * self.scale), round(self.box[3] * self.scale))


def read_table(path):
    with open(path, encoding="utf-8") as f:
        return [Line(line.rstrip("\n").split("\t")) for line in f if line.strip()]


def normalise(kind, size, data):
    """Returns the PNG bytes the theme ships for a screenshot; raises InputError."""
    width, height, rows = tc.decode_png_rgba(data)
    if (width, height) != size:
        raise InputError("%dx%d, not %dx%d" % ((width, height) + size))
    alphas = b"".join(row[3::4] for row in rows)
    if kind == "frame":
        if alphas != b"\xff" * len(alphas):
            raise InputError("a frame pixel is not opaque")
        rgb = []
        for row in rows:
            out = bytearray(width * 3)
            out[0::3], out[1::3], out[2::3] = row[0::4], row[1::4], row[2::4]
            rgb.append(bytes(out))
        return tc.encode_png((width, height, rgb), 2)
    half = [a for a in set(alphas) if a not in (0, 255)]
    if half:
        raise InputError("half-transparent pixel (alpha %d)" % min(half))
    if 255 not in alphas:
        raise InputError("no opaque pixel")
    clean = []
    for row in rows:
        out = bytearray(row)
        for i in range(0, len(out), 4):
            if out[i + 3] == 0:
                out[i:i + 4] = b"\0\0\0\0"
        clean.append(bytes(out))
    return tc.encode_png((width, height, clean), 6)


def normalise_all(table, raw, out):
    lines = []
    os.makedirs(out, exist_ok=True)
    for line in table:
        try:
            with open(os.path.join(raw, line.file), "rb") as f:
                png = normalise(line.kind, line.size, f.read())
        except (InputError, OSError) as e:
            lines.append("theme-art: %s: %s" % (line.file, e))
            continue
        with open(os.path.join(out, line.file), "wb") as f:
            f.write(png)
    return lines


def pixels(path):
    with open(path, "rb") as f:
        return tc.decode_png_rgba(f.read())


def shown(path):
    path = os.path.abspath(path)
    return os.path.relpath(path, ROOT) if path.startswith(ROOT + os.sep) else path


def check(table, new, theme, chromium):
    """Returns (lines, status)."""
    lines = []
    produced = set()
    for line in table:
        produced.add(line.file)
        committed = os.path.join(theme, line.file)
        if not os.path.isfile(committed):
            lines.append("MISSING %s" % line.file)
            continue
        try:
            same = pixels(os.path.join(new, line.file)) == pixels(committed)
        except (InputError, OSError):
            same = False
        if not same:
            lines.append("DIFFERENT %s" % line.file)
    for name in sorted(os.listdir(theme)):
        if name.endswith(".png") and name not in produced:
            lines.append("EXTRA %s" % name)
    if lines:
        lines.append("theme-art: %d mismatches against %s (Chromium %s)" % (len(lines), shown(theme), chromium))
        return lines, 1
    lines.append("theme-art: %d images pixel-identical to %s (Chromium %s)" % (len(table), shown(theme), chromium))
    return lines, 0


def self_test():
    failures = []

    def expect(case, want, got):
        if want != got:
            failures.append("theme_art: self-test: %s: want %r, got %r" % (case, want, got))

    def refused(case, want, run):
        try:
            run()
        except InputError as e:
            expect(case, want, str(e))
            return
        failures.append("theme_art: self-test: %s: accepted" % case)

    def kept(case, run):
        try:
            return run()
        except InputError as e:
            failures.append("theme_art: self-test: %s: refused: %s" % (case, e))
            return None

    def colour_type(png):
        return png[25]

    rgb_rows = tc.planted_rows(5, 4)
    for kind in range(5):
        png = tc.planted_png(5, 4, tc.planted_raw(rgb_rows, 3, [kind] * 4))
        expect("rgb filter %d" % kind, (5, 4, tc.with_alpha(rgb_rows)),
               kept("rgb filter %d" % kind, lambda: tc.decode_png_rgba(png)))
    rgba_rows = [r[:16] for r in tc.planted_rows(6, 4)]
    for kind in range(5):
        png = tc.planted_png(4, 4, tc.planted_raw(rgba_rows, 4, [kind] * 4), colour=6)
        expect("rgba filter %d" % kind, (4, 4, rgba_rows),
               kept("rgba filter %d" % kind, lambda: tc.decode_png_rgba(png)))

    def screenshot(width, height, pixel):
        rows = [b"".join(bytes(pixel(x, y)) for x in range(width)) for y in range(height)]
        return tc.planted_png(width, height, tc.planted_raw(rows, 4, [4] * height), colour=6), rows

    frame, frame_rows = screenshot(3, 2, lambda x, y: (10 * x, 20 * y, 30, 255))
    written = kept("frame", lambda: normalise("frame", (3, 2), frame))
    if written is not None:
        expect("frame written RGB", 2, colour_type(written))
        expect("frame decoded back", (3, 2, frame_rows), kept("frame back", lambda: tc.decode_png_rgba(written)))
        expect("frame chunks", [b"IHDR", b"IDAT", b"IEND"], chunks(written))
        expect("two encodes byte-identical", written, normalise("frame", (3, 2), frame))

    sprite, _ = screenshot(2, 2, lambda x, y: (200, 100, 50, 255) if x == y else (9, 8, 7, 0))
    written = kept("sprite", lambda: normalise("sprite", (2, 2), sprite))
    if written is not None:
        expect("sprite written RGBA", 6, colour_type(written))
        expect("sprite transparent as 0,0,0,0",
               (2, 2, [bytes([200, 100, 50, 255, 0, 0, 0, 0]), bytes([0, 0, 0, 0, 200, 100, 50, 255])]),
               kept("sprite back", lambda: tc.decode_png_rgba(written)))
        expect("sprite chunks", [b"IHDR", b"IDAT", b"IEND"], chunks(written))

    half, _ = screenshot(2, 1, lambda x, y: (1, 2, 3, 255 if x else 128))
    refused("half-transparent sprite", "half-transparent pixel (alpha 128)", lambda: normalise("sprite", (2, 1), half))
    empty, _ = screenshot(2, 1, lambda x, y: (1, 2, 3, 0))
    refused("empty sprite", "no opaque pixel", lambda: normalise("sprite", (2, 1), empty))
    refused("non-opaque frame", "a frame pixel is not opaque", lambda: normalise("frame", (2, 1), half))
    refused("wrong size", "2x2, not 3x2", lambda: normalise("sprite", (3, 2), sprite))

    with tempfile.TemporaryDirectory() as folder:
        table_path = os.path.join(folder, "table.txt")
        with open(table_path, "w", encoding="utf-8") as f:
            f.write("frame.png\tm1\tframe\t0\t0\t3\t2\t1\trect\n"
                    "dot.png\tm1\tsprite\t0\t0\t4\t4\t0.5\trect\n"
                    "gone.png\tm1\tsprite\t0\t0\t2\t2\t1\trect\n")
        raw, new, theme = (os.path.join(folder, d) for d in ("raw", "new", "theme"))
        for d in (raw, theme):
            os.mkdir(d)

        def put(folder_path, name, data):
            with open(os.path.join(folder_path, name), "wb") as f:
                f.write(data)

        put(raw, "frame.png", frame)
        put(raw, "dot.png", sprite)
        put(raw, "gone.png", sprite)
        status, out = run(["normalise", table_path, raw, new])
        expect("normalise: status, output", (0, ""), (status, out))
        put(theme, "frame.png", tc.encode_png((3, 2, [r[:9] for r in tc.planted_rows(3, 2)])))
        put(theme, "dot.png", open(os.path.join(new, "dot.png"), "rb").read())
        put(theme, "stray.png", sprite)
        put(theme, "theme.ini", b"")
        status, out = run(["check", table_path, new, theme, "1.2.3"])
        expect("check: mismatches", (1, "DIFFERENT frame.png\nMISSING gone.png\nEXTRA stray.png\n"
                                        "theme-art: 3 mismatches against %s (Chromium 1.2.3)\n" % theme),
               (status, out))
        for name in ("frame.png", "gone.png"):
            put(theme, name, open(os.path.join(new, name), "rb").read())
        os.remove(os.path.join(theme, "stray.png"))
        status, out = run(["check", table_path, new, theme, "1.2.3"])
        expect("check: equal", (0, "theme-art: 3 images pixel-identical to %s (Chromium 1.2.3)\n" % theme),
               (status, out))
        put(raw, "dot.png", half)
        status, out = run(["normalise", table_path, raw, new])
        expect("normalise: refused", (1, "theme-art: dot.png: 2x1, not 2x2\n"), (status, out))

    for line in failures:
        print(line)
    return 1 if failures else 0


def chunks(png):
    kinds = []
    pos = 8
    while pos + 8 <= len(png):
        length = int.from_bytes(png[pos:pos + 4], "big")
        kinds.append(png[pos + 4:pos + 8])
        pos += 12 + length
    return kinds


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        status = main(argv)
    return status, out.getvalue()


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    try:
        if len(argv) == 4 and argv[0] == "normalise":
            lines = normalise_all(read_table(argv[1]), argv[2], argv[3])
            for line in lines:
                print(line)
            return 1 if lines else 0
        if len(argv) == 5 and argv[0] == "check":
            lines, status = check(read_table(argv[1]), argv[2], argv[3], argv[4])
            for line in lines:
                print(line)
            return status
    except (InputError, OSError) as e:
        print("theme-art: %s" % e)
        return 1
    sys.stdout.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
