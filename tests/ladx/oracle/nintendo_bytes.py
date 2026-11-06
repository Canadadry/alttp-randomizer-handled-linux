"""Usage: python3 -I tests/ladx/oracle/nintendo_bytes.py [--rom <file>] [--min <n>] <file-or-dir>...

The Nintendo-bytes check: prints one `LITERAL <path> <view> @<offset> <length> bytes` line per run
of --min (default 4) or more bytes of a file that also occurs in the ROM (--rom, default
tmp/ladx/base.gbc), then `nintendo-bytes: <n> files, rom <sha1>, min <m>, <k> literal runs`;
exit 1 when k > 0, 2 on a `setup:` line. Never prints the bytes themselves.

Views: `raw`, the file's bytes; `hex`, every maximal run of ASCII hex digits of 2 x --min or more,
decoded at nibble alignments 0 and 1, offsets in the file's characters. Proves itself first, on
every run, on planted files against a planted 64 KiB synthetic ROM.

Calibration note for Phase 2 step 1: no exemption for trivial runs (00000000, FFFFFFFF); random
data (SHA-512 digests in logic_vectors.txt, data/base.bps) hits a 1 MB ROM's 4-byte windows by
chance at about 1e-4 per window.
"""

import argparse
import hashlib
import os
import random
import re
import sys
import tempfile

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_ROM = os.path.join(TOP, "tmp", "ladx", "base.gbc")


class RomIndex:
    """Every min_len-byte window of the ROM."""

    def __init__(self, rom_bytes, min_len):
        assert min_len >= 1, "min_len must be positive"
        self.min_len = min_len
        self.windows = {rom_bytes[i:i + min_len] for i in range(len(rom_bytes) - min_len + 1)}


def _merged_hits(data, index):
    """[(offset, length)] of the window hits in data, overlapping or touching hits merged."""
    n = index.min_len
    merged = []
    for i in range(len(data) - n + 1):
        if data[i:i + n] not in index.windows:
            continue
        if merged and i <= merged[-1][0] + merged[-1][1]:
            start = merged[-1][0]
            merged[-1] = (start, i + n - start)
        else:
            merged.append((i, n))
    return merged


def _hex_runs(data, index):
    """[(offset, length)] of the runs found in the bytes data's hex-digit runs decode to, at both
    nibble alignments; offsets in data's characters, lengths in decoded bytes."""
    found = []
    for match in re.finditer(rb"[0-9A-Fa-f]{%d,}" % (2 * index.min_len), data):
        digits = match.group()
        for align in (0, 1):
            usable = (len(digits) - align) // 2 * 2
            decoded = bytes.fromhex(digits[align:align + usable].decode("ascii"))
            for off, length in _merged_hits(decoded, index):
                found.append((match.start() + align + 2 * off, length))
    return found


def literal_runs(data, index):
    """[(view, offset, length)] of the literal runs of the ROM in data, sorted by offset.

    A merged run joins overlapping or touching window hits, so it is reported even when its full
    length does not occur contiguously in the ROM: conservative on purpose.
    """
    runs = [("raw", off, length) for off, length in _merged_hits(data, index)]
    runs += [("hex", off, length) for off, length in _hex_runs(data, index)]
    return sorted(runs, key=lambda run: (run[1], run[0], run[2]))


class SetupError(Exception):
    pass


def files_under(paths):
    """Every file of paths, directories walked in sorted order with .git skipped."""
    files = []
    for path in paths:
        if os.path.isfile(path):
            files.append(path)
        elif os.path.isdir(path):
            for root, dirs, names in os.walk(path):
                dirs[:] = sorted(d for d in dirs if d != ".git")
                files.extend(os.path.join(root, name) for name in sorted(names))
        else:
            raise SetupError(f"no such file {path}")
    return files


def scan(files, index):
    """[(path, view, offset, length)] of every literal run in files, sorted by path then offset."""
    found = []
    for path in files:
        with open(path, "rb") as f:
            data = f.read()
        found.extend((path, view, off, length) for view, off, length in literal_runs(data, index))
    return sorted(found, key=lambda hit: (hit[0], hit[2], hit[1], hit[3]))


def self_test():
    """None when the check finds exactly the planted runs, else the name of the case that failed."""
    rng = random.Random(0x1ADC)
    rom = bytes(rng.getrandbits(8) for _ in range(65536))
    planted = {
        "clean.txt": (b"a clean file, 0000 FFFF deadbeef? no: dead beef\n", []),
        "three.bin": (b"<" + rom[1000:1003] + b">", []),
        "four.bin": (b"<" + rom[2000:2004] + b">", [("raw", 4)]),
        "nine.txt": (b"some text " + rom[3000:3009] + b" more text\n", [("raw", 9)]),
        "upper.txt": (b"x = 7" + rom[4000:4005].hex().upper().encode() + b";\n", [("hex", 5)]),
        "lower.txt": (b"x = " + rom[5000:5005].hex().encode() + b";\n", [("hex", 5)]),
    }
    index = RomIndex(rom, 4)
    with tempfile.TemporaryDirectory() as tmp:
        for name, (data, _) in planted.items():
            with open(os.path.join(tmp, name), "wb") as f:
                f.write(data)
        hits = scan(files_under([tmp]), index)
        for name, (_, expected) in sorted(planted.items()):
            got = [(view, length) for path, view, _, length in hits
                   if os.path.basename(path) == name]
            if got != expected:
                return name
    return None


def main(argv):
    parser = argparse.ArgumentParser(prog="nintendo_bytes.py")
    parser.add_argument("--rom", default=DEFAULT_ROM)
    parser.add_argument("--min", type=int, default=4)
    parser.add_argument("paths", nargs="+")
    args = parser.parse_args(argv)
    if args.min < 1:
        parser.error("--min must be at least 1")
    failed = self_test()
    if failed:
        print(f"nintendo-bytes: self-test failed: {failed}")
        return 1
    print("nintendo-bytes: self-test, 6 planted files as expected")
    try:
        if not os.path.isfile(args.rom):
            raise SetupError(f"no ROM at {args.rom} (run tests/ladx/oracle/fetch_rom.sh)")
        with open(args.rom, "rb") as f:
            rom = f.read()
        files = files_under(args.paths)
    except SetupError as e:
        print(f"setup: nintendo-bytes: {e}")
        return 2
    hits = scan(files, RomIndex(rom, args.min))
    for path, view, off, length in hits:
        print(f"LITERAL {path} {view} @{off} {length} bytes")
    print(f"nintendo-bytes: {len(files)} files, rom {hashlib.sha1(rom).hexdigest()}, "
          f"min {args.min}, {len(hits)} literal runs")
    return 1 if hits else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
