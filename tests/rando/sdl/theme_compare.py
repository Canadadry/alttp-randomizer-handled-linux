"""Usage: python3 -I theme_compare.py compare MOCKUP_PNG APP_BMP OUT_PNG
       python3 -I theme_compare.py --self-test

compare: writes OUT_PNG, three 640x480 panels mockup | app | diff with 8 px #808080 bars between
them, and prints the score. Refused with one line on stderr, exit 1 and nothing written, when an
image cannot be read, is not 640x480, the mockup's pixel (2, 2) is not #0c0c34, or the mockup
scored against itself is not 0.
--self-test: proves the decoders, the score, the diff and the composition on planted images; one
line per failing case, exit 1 if any, nothing printed when all pass.
"""

import contextlib
import io
import os
import struct
import sys
import tempfile
import zlib


class InputError(Exception):
    pass


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def unfilter(kind, line, prev, bpp):
    row = bytearray(line)
    n = len(row)
    if kind == 0:
        pass
    elif kind == 1:
        for i in range(bpp, n):
            row[i] = (row[i] + row[i - bpp]) & 0xFF
    elif kind == 2:
        for i in range(n):
            row[i] = (row[i] + prev[i]) & 0xFF
    elif kind == 3:
        for i in range(n):
            a = row[i - bpp] if i >= bpp else 0
            row[i] = (row[i] + ((a + prev[i]) >> 1)) & 0xFF
    elif kind == 4:
        for i in range(n):
            if i >= bpp:
                a, c = row[i - bpp], prev[i - bpp]
            else:
                a = c = 0
            b = prev[i]
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            if pa <= pb and pa <= pc:
                row[i] = (row[i] + a) & 0xFF
            elif pb <= pc:
                row[i] = (row[i] + b) & 0xFF
            else:
                row[i] = (row[i] + c) & 0xFF
    else:
        raise InputError("filter type %d" % kind)
    return row


def decode_png_rgba(data):
    """Returns (width, height, rows), rows a list of width * 4 RGBA bytes (alpha 255 for colour type 2)."""
    if data[:8] != PNG_SIGNATURE:
        raise InputError("not a PNG")
    pos = 8
    header = None
    idat = []
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    if header is None:
        raise InputError("no IHDR")
    width, height, depth, colour, _compression, _filter, interlace = header
    if depth != 8:
        raise InputError("bit depth %d, not 8" % depth)
    if colour not in (2, 6):
        raise InputError("colour type %d, not 2 or 6" % colour)
    if interlace != 0:
        raise InputError("interlace %d, not 0" % interlace)
    bpp = 3 if colour == 2 else 4
    stride = width * bpp
    try:
        raw = zlib.decompress(b"".join(idat))
    except zlib.error as e:
        raise InputError("IDAT: %s" % e)
    if len(raw) < height * (stride + 1):
        raise InputError("IDAT holds %d bytes, not %d" % (len(raw), height * (stride + 1)))
    rows = []
    prev = bytes(stride)
    for y in range(height):
        start = y * (stride + 1)
        prev = unfilter(raw[start], raw[start + 1:start + 1 + stride], prev, bpp)
        if bpp == 4:
            rows.append(bytes(prev))
            continue
        rgba = bytearray(b"\xff" * (width * 4))
        rgba[0::4], rgba[1::4], rgba[2::4] = prev[0::3], prev[1::3], prev[2::3]
        rows.append(bytes(rgba))
    return (width, height, rows)


def decode_png(data):
    """Returns (width, height, rows), rows a list of width * 3 RGB bytes; refused unless opaque."""
    width, height, rgba_rows = decode_png_rgba(data)
    rows = []
    for row in rgba_rows:
        if row[3::4] != b"\xff" * width:
            raise InputError("not opaque")
        rgb = bytearray(width * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = row[0::4], row[1::4], row[2::4]
        rows.append(bytes(rgb))
    return (width, height, rows)


def decode_bmp(data):
    """Returns (width, height, rows) like decode_png, rows top to bottom."""
    if len(data) < 54 or data[:2] != b"BM":
        raise InputError("not a BMP")
    offset, = struct.unpack("<I", data[10:14])
    info, width, height, _planes, bpp, compression = struct.unpack("<IiiHHI", data[14:34])
    if info < 40:
        raise InputError("BMP header of %d bytes, not BITMAPINFOHEADER" % info)
    if bpp != 24:
        raise InputError("%d bpp, not 24" % bpp)
    if compression != 0:
        raise InputError("compression %d, not 0" % compression)
    top_down = height < 0
    height = abs(height)
    stride = (width * 3 + 3) & ~3
    if len(data) < offset + height * stride:
        raise InputError("BMP holds %d bytes, not %d" % (len(data), offset + height * stride))
    rows = []
    for y in range(height):
        start = offset + y * stride
        bgr = data[start:start + width * 3]
        rgb = bytearray(width * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = bgr[2::3], bgr[1::3], bgr[0::3]
        rows.append(bytes(rgb))
    if not top_down:
        rows.reverse()
    return (width, height, rows)


def planted_bmp(width, height, rows, top_down=False, bpp=24, compression=0):
    stride = (width * 3 + 3) & ~3
    pixels = b""
    for row in (rows if top_down else rows[::-1]):
        bgr = bytearray(len(row))
        bgr[0::3], bgr[1::3], bgr[2::3] = row[2::3], row[1::3], row[0::3]
        pixels += bytes(bgr) + b"\0" * (stride - len(row))
    info = struct.pack("<IiiHHIIiiII", 40, width, -height if top_down else height, 1, bpp, compression,
                       len(pixels), 2835, 2835, 0, 0)
    return b"BM" + struct.pack("<IHHI", 54 + len(pixels), 0, 0, 54) + info + pixels


def same_size(a, b):
    if a[:2] != b[:2]:
        raise InputError("%dx%d against %dx%d" % (a[0], a[1], b[0], b[1]))


def score(a, b):
    """100 x the sum of |A[c] - B[c]| over every pixel and channel / (W x H x 3 x 255)."""
    same_size(a, b)
    total = 0
    for ra, rb in zip(a[2], b[2]):
        if ra != rb:
            total += sum(abs(x - y) for x, y in zip(ra, rb))
    return 100.0 * total / (a[0] * a[1] * 3 * 255)


def diff(a, b):
    same_size(a, b)
    rows = []
    for ra, rb in zip(a[2], b[2]):
        out = bytearray(len(ra))
        for i in range(0, len(ra), 3):
            r, g, bl = ra[i], ra[i + 1], ra[i + 2]
            d = max(abs(r - rb[i]), abs(g - rb[i + 1]), abs(bl - rb[i + 2]))
            if d == 0:
                out[i] = out[i + 1] = out[i + 2] = (299 * r + 587 * g + 114 * bl) // 1000 // 4
            else:
                out[i] = out[i + 2] = 128 + 127 * d // 255
        rows.append(bytes(out))
    return (a[0], a[1], rows)


BAR_PX = 8
BAR = b"\x80\x80\x80" * BAR_PX


def compose(panels):
    for panel in panels[1:]:
        same_size(panels[0], panel)
    width = len(panels) * panels[0][0] + (len(panels) - 1) * BAR_PX
    rows = [BAR.join(panel[2][y] for panel in panels) for y in range(panels[0][1])]
    return (width, panels[0][1], rows)


def encode_png(image, colour=2):
    """colour 2: rows of width * 3 RGB bytes; colour 6: width * 4 RGBA bytes."""
    width, height, rows = image
    ihdr = struct.pack(">IIBBBBB", width, height, 8, colour, 0, 0, 0)
    idat = zlib.compress(b"".join(b"\0" + row for row in rows), 9)
    return PNG_SIGNATURE + png_chunk(b"IHDR", ihdr) + png_chunk(b"IDAT", idat) + png_chunk(b"IEND", b"")


SIZE = (640, 480)
BACKGROUND = bytes([0x0c, 0x0c, 0x34])


def read_image(path, decoder):
    try:
        with open(path, "rb") as f:
            image = decoder(f.read())
    except (InputError, OSError) as e:
        raise InputError("%s: %s" % (path, e))
    if image[:2] != SIZE:
        raise InputError("%s: %dx%d, not %dx%d" % ((path,) + image[:2] + SIZE))
    return image


def compare(mockup_path, app_path, out_path):
    """Writes the side-by-side PNG and returns the score; raises InputError, nothing written."""
    mockup = read_image(mockup_path, decode_png)
    corner = mockup[2][2][6:9]
    if corner != BACKGROUND:
        raise InputError("%s: pixel (2, 2) is #%s, not #%s" % (mockup_path, corner.hex(), BACKGROUND.hex()))
    self_score = score(mockup, mockup)
    if self_score != 0:
        raise InputError("%s: self-score %.3f, not 0" % (mockup_path, self_score))
    app = read_image(app_path, decode_bmp)
    png = encode_png(compose([mockup, app, diff(mockup, app)]))
    with open(out_path, "wb") as f:
        f.write(png)
    return score(mockup, app)


def png_chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))


def planted_png(width, height, raw, depth=8, colour=2, interlace=0):
    ihdr = struct.pack(">IIBBBBB", width, height, depth, colour, 0, 0, interlace)
    return (PNG_SIGNATURE + png_chunk(b"IHDR", ihdr) + png_chunk(b"IDAT", zlib.compress(raw))
            + png_chunk(b"IEND", b""))


def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def filtered(kind, row, prev, bpp):
    out = bytearray()
    for i, x in enumerate(row):
        a = row[i - bpp] if i >= bpp else 0
        b = prev[i]
        c = prev[i - bpp] if i >= bpp else 0
        predictor = (0, a, b, (a + b) // 2, paeth(a, b, c))[kind]
        out.append((x - predictor) & 0xFF)
    return bytes([kind]) + bytes(out)


def planted_raw(rows, bpp, kinds):
    prev = bytes(len(rows[0]))
    raw = b""
    for row, kind in zip(rows, kinds):
        raw += filtered(kind, row, prev, bpp)
        prev = row
    return raw


def planted_rows(width, height):
    state = 12345
    rows = []
    for _ in range(height):
        row = bytearray()
        for _ in range(width * 3):
            state = (1103515245 * state + 12345) & 0x7FFFFFFF
            row.append(state >> 23)
        rows.append(bytes(row))
    return rows


def with_alpha(rows, alpha=255):
    out = []
    for row in rows:
        r = bytearray()
        for i in range(0, len(row), 3):
            r += row[i:i + 3] + bytes([alpha])
        out.append(bytes(r))
    return out


def self_test():
    failures = []

    def expect(case, want, got):
        if want != got:
            failures.append("theme_compare: self-test: %s: want %r, got %r" % (case, want, got))

    def refused(case, run):
        try:
            run()
        except InputError:
            return
        failures.append("theme_compare: self-test: %s: accepted" % case)

    def decoded(case, run):
        try:
            return run()
        except InputError as e:
            failures.append("theme_compare: self-test: %s: refused: %s" % (case, e))
            return None

    rows = planted_rows(5, 4)
    image = (5, 4, rows)
    for kind in range(5):
        png = planted_png(5, 4, planted_raw(rows, 3, [kind] * 4))
        expect("png filter %d" % kind, image, decoded("png filter %d" % kind, lambda: decode_png(png)))
    png = planted_png(5, 4, planted_raw(rows, 3, [4, 1, 3, 2]))
    expect("png mixed filters", image, decoded("png mixed filters", lambda: decode_png(png)))
    big = planted_rows(64, 16)
    png = planted_png(64, 16, planted_raw(big, 3, [4] * 16))
    expect("png paeth ties", (64, 16, big), decoded("png paeth ties", lambda: decode_png(png)))
    rgba = with_alpha(rows)
    png = planted_png(5, 4, planted_raw(rgba, 4, [0, 1, 2, 4]), colour=6)
    expect("png rgba", image, decoded("png rgba", lambda: decode_png(png)))
    rgba[2] = rgba[2][:7] + b"\xfe" + rgba[2][8:]
    png = planted_png(5, 4, planted_raw(rgba, 4, [0] * 4), colour=6)
    refused("png alpha 254", lambda: decode_png(png))
    refused("png bit depth 16", lambda: decode_png(planted_png(5, 4, b"\0" * 31 * 4, depth=16)))
    refused("png palette", lambda: decode_png(planted_png(5, 4, b"\0" * 6 * 4, colour=3)))
    refused("png interlaced", lambda: decode_png(planted_png(5, 4, planted_raw(rows, 3, [0] * 4),
                                                              interlace=1)))

    rows = planted_rows(3, 2)
    image = (3, 2, rows)
    expect("bmp bottom-up", image, decoded("bmp bottom-up", lambda: decode_bmp(planted_bmp(3, 2, rows))))
    expect("bmp top-down", image,
           decoded("bmp top-down", lambda: decode_bmp(planted_bmp(3, 2, rows, top_down=True))))
    refused("bmp 32 bpp", lambda: decode_bmp(planted_bmp(3, 2, rows, bpp=32)))
    refused("bmp compressed", lambda: decode_bmp(planted_bmp(3, 2, rows, compression=1)))

    def scored(case, a, b):
        return decoded(case, lambda: "%.3f" % score(a, b))

    expect("score identical", "0.000", scored("score identical", image, image))
    off = (2, 1, [bytes([10, 20, 30, 40, 50, 60])])
    off_by_51 = (2, 1, [bytes([10, 20, 30, 40, 101, 60])])
    expect("score off by 51", "3.333", scored("score off by 51", off, off_by_51))
    black, white = (2, 1, [b"\0" * 6]), (2, 1, [b"\xff" * 6])
    expect("score black against white", "100.000", scored("score black against white", black, white))
    refused("score of different sizes", lambda: score(image, off))

    mockup = (3, 1, [bytes([200, 100, 50, 10, 20, 30, 0, 255, 9])])
    app = (3, 1, [bytes([200, 100, 50, 10, 21, 29, 255, 0, 9])])
    expect("diff of equal, off by 1 and off by 255 pixels",
           (3, 1, [bytes([31, 31, 31, 128, 0, 128, 255, 0, 255])]),
           decoded("diff", lambda: diff(mockup, app)))

    panels = [(2, 1, [bytes([1, 2, 3, 4, 5, 6])]), (2, 1, [bytes([7, 8, 9, 10, 11, 12])]),
              (2, 1, [bytes([13, 14, 15, 16, 17, 18])])]
    bar = b"\x80" * 24
    side_by_side = (22, 1, [bytes([1, 2, 3, 4, 5, 6]) + bar + bytes([7, 8, 9, 10, 11, 12]) + bar
                            + bytes([13, 14, 15, 16, 17, 18])])
    expect("composition of three 2x1 panels", side_by_side, decoded("composition", lambda: compose(panels)))
    encoded = decoded("encode", lambda: encode_png(side_by_side))
    if encoded is not None:
        expect("encoded PNG decoded back", side_by_side, decoded("decode back", lambda: decode_png(encoded)))
        expect("two encodings byte-identical", encoded, encode_png(side_by_side))

    def solid(width, height, rgb, at=None, other=None):
        rows = [bytes(rgb) * width for _ in range(height)]
        if at is not None:
            x, y = at
            rows[y] = rows[y][:3 * x] + bytes(other) + rows[y][3 * x + 3:]
        return (width, height, rows)

    def run(*argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            status = main(list(argv))
        return status, out.getvalue(), err.getvalue()

    background = (0x0c, 0x0c, 0x34)
    with tempfile.TemporaryDirectory() as folder:
        def path(name):
            return os.path.join(folder, name)

        def write(name, data):
            with open(path(name), "wb") as f:
                f.write(data)

        mockup = solid(640, 480, background, (100, 200), (255, 255, 255))
        write("mockup.png", encode_png(mockup))
        app = solid(640, 480, background, (100, 201), (255, 255, 255))
        write("app.bmp", planted_bmp(640, 480, app[2]))
        status, out, err = run("compare", path("mockup.png"), path("app.bmp"), path("out.png"))
        expect("compare: status, stdout, stderr", (0, "%.3f\n" % (100.0 * 2 * (243 + 243 + 203) / (640 * 480 * 3 * 255)), ""),
               (status, out, err))
        if os.path.exists(path("out.png")):
            with open(path("out.png"), "rb") as f:
                written = decode_png(f.read())
            expect("compare: written image", compose([mockup, app, diff(mockup, app)]), written)
        else:
            failures.append("theme_compare: self-test: compare: nothing written")

        write("bad-mockup.png", encode_png(solid(640, 480, background, (2, 2), (0x0c, 0x0c, 0x35))))
        write("narrow.bmp", planted_bmp(639, 480, solid(639, 480, background)[2]))
        for case, args, want in [
                ("compare: mockup pixel (2, 2)", ("bad-mockup.png", "app.bmp"),
                 "bad-mockup.png: pixel (2, 2) is #0c0c35, not #0c0c34"),
                ("compare: 639x480 capture", ("mockup.png", "narrow.bmp"), "narrow.bmp: 639x480, not 640x480")]:
            status, out, err = run("compare", path(args[0]), path(args[1]), path("refused.png"))
            expect(case, (1, "", "theme_compare: %s\n" % path(want)),
                   (status, out, err))
            if os.path.exists(path("refused.png")):
                failures.append("theme_compare: self-test: %s: an image written" % case)

    for line in failures:
        print(line)
    return 1 if failures else 0


def main(argv):
    if argv == ["--self-test"]:
        return self_test()
    if len(argv) == 4 and argv[0] == "compare":
        try:
            print("%.3f" % compare(*argv[1:]))
        except (InputError, OSError) as e:
            print("theme_compare: %s" % e, file=sys.stderr)
            return 1
        return 0
    sys.stdout.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
