"""Usage: python3 -I tests/ladx/oracle/nintendo_bytes.py [--rom <file>] [--ladxr <dir>]
       [--scope strict|general] [--min <n>] [--verbose] <file-or-dir>...

The Nintendo-bytes check: prints one `LITERAL <path> <view> @<offset> <length> bytes <scope>` line
per run of a file that also occurs in the ROM (--rom, default tmp/ladx/base.gbc) and is not
cleared, then `nintendo-bytes: <n> files, rom <sha1>, ladxr <short>, <k> literal runs, cleared <t>
trivial, <l> low-info, <p> ladxr-public`; exit 1 when k > 0, 2 on a `setup:` line. Never prints
the bytes themselves. --verbose adds `CLEARED <path> <view> @<offset> <length> bytes <category>`.

Policy. Scope `strict` for paths under STRICT_PREFIXES (ROM-derived artifacts), `general` for any
other (--scope overrides). Views: `raw`; `hex`, maximal hex-digit runs decoded at both nibble
alignments; `bytelist`, lists of 0xNN or $NN tokens separated by commas and whitespace. Minimum
contiguous match (THRESHOLDS): strict 4 in every view; general raw 16, hex and bytelist 8; --min
replaces them all and the summary says `min <n> (override)`. Clearances, counted: `trivial` (an
arithmetic progression mod 256), `low-info` (at most 3 distinct values, general only),
`ladxr-public` (the whole run occurs in the text files of the LADXR checkout --ladxr, default
LADXR_DIR or tests/ladx/oracle/ladxr, at LADXR_PIN, or in their decoded hex-digit runs; else a
`setup:` line). Proves itself first, on every run, on planted files against a planted ROM and
corpus.
"""

import argparse
import bisect
import hashlib
import os
import random
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_ROM = os.path.join(TOP, "tmp", "ladx", "base.gbc")
DEFAULT_LADXR = os.path.join(TOP, "tests", "ladx", "oracle", "ladxr")
LADXR_PIN = "4325207e4e55076e05f209fbdf16b860a90e42ef"


STRICT_PREFIXES = ("data/ladx/", "tests/ladx/cases/", "tests/ladx/combos/")
THRESHOLDS = {
    ("strict", "raw"): 4, ("strict", "hex"): 4, ("strict", "bytelist"): 4,
    ("general", "raw"): 16, ("general", "hex"): 8, ("general", "bytelist"): 8,
}


class RomIndex:
    """The ROM's n-byte windows, each length's set built on first use."""

    def __init__(self, rom_bytes):
        self.rom = rom_bytes
        self._windows = {}

    def windows(self, n):
        assert n >= 1, "window length must be positive"
        if n not in self._windows:
            rom = self.rom
            self._windows[n] = {rom[i:i + n] for i in range(len(rom) - n + 1)}
        return self._windows[n]


def scope_of(path):
    """'strict' for a repository-relative path under STRICT_PREFIXES, else 'general'."""
    path = path.replace(os.sep, "/")
    return "strict" if path.startswith(STRICT_PREFIXES) else "general"


def _merged_hits(data, windows, n):
    """[(offset, length)] of the n-byte window hits in data, overlapping or touching hits merged."""
    merged = []
    for i in range(len(data) - n + 1):
        if data[i:i + n] not in windows:
            continue
        if merged and i <= merged[-1][0] + merged[-1][1]:
            start = merged[-1][0]
            merged[-1] = (start, i + n - start)
        else:
            merged.append((i, n))
    return merged


def _raw_runs(data, index, n):
    hits = _merged_hits(data, index.windows(n), n)
    return [(off, data[off:off + length]) for off, length in hits]


def _hex_decodings(data, min_digits):
    """[(offset, bytes)] that each maximal run of min_digits hex digits or more of data decodes
    to, at nibble alignments 0 and 1; offsets in data's characters."""
    found = []
    for match in re.finditer(rb"[0-9A-Fa-f]{%d,}" % min_digits, data):
        digits = match.group()
        for align in (0, 1):
            usable = (len(digits) - align) // 2 * 2
            if usable:
                decoded = bytes.fromhex(digits[align:align + usable].decode("ascii"))
                found.append((match.start() + align, decoded))
    return found


def _hex_runs(data, index, n):
    """[(offset, run bytes)] found in the bytes data's hex-digit runs decode to; offsets in data's
    characters."""
    found = []
    for start, decoded in _hex_decodings(data, 2 * n):
        for off, length in _merged_hits(decoded, index.windows(n), n):
            found.append((start + 2 * off, decoded[off:off + length]))
    return found


_TOKEN = rb"(?<![0-9A-Za-z_$])(?:0[xX]|\$)([0-9A-Fa-f]{2})(?![0-9A-Za-z_])"
_TOKEN_LIST = re.compile(rb"%s(?:[\s,]+%s)*" % (_TOKEN, _TOKEN))


def _bytelist_runs(data, index, n):
    """[(offset, run bytes)] found in the bytes of every list of 0xNN or $NN tokens separated by
    commas and whitespace; offsets in data's characters, of the run's first token."""
    found = []
    for match in _TOKEN_LIST.finditer(data):
        tokens = list(re.finditer(_TOKEN, match.group()))
        if len(tokens) < n:
            continue
        decoded = bytes(int(token.group(1), 16) for token in tokens)
        for off, length in _merged_hits(decoded, index.windows(n), n):
            found.append((match.start() + tokens[off].start(), decoded[off:off + length]))
    return found


CLEARANCES = ("trivial", "low-info", "ladxr-public")
VIEWS = (("raw", _raw_runs), ("hex", _hex_runs), ("bytelist", _bytelist_runs))


def _clearance(run, scope, corpus):
    """The clearance category of a run's bytes, or None when the run is a literal."""
    step = (run[1] - run[0]) % 256 if len(run) > 1 else 0
    if all((b - a) % 256 == step for a, b in zip(run, run[1:])):
        return "trivial"
    if scope == "general" and len(set(run)) <= 3:
        return "low-info"
    if run in corpus:
        return "ladxr-public"
    # TODO plan 3 step 5: the strict scope's `computed` clearance (the step 2.1 note) goes here.
    return None


def check_blob(path, data, index, corpus, scope=None, min_len=None):
    """(literals, cleared) for the blob data at the repository-relative path: literals as
    [(view, offset, length, scope)], cleared as [(view, offset, length, category)], both sorted by
    offset. scope overrides the path's; min_len overrides every threshold."""
    scope = scope or scope_of(path)
    assert scope in ("strict", "general"), scope
    literals, cleared = [], []
    for view, runs in VIEWS:
        n = min_len or THRESHOLDS[(scope, view)]
        for off, run in runs(data, index, n):
            category = _clearance(run, scope, corpus)
            if category:
                cleared.append((view, off, len(run), category))
            else:
                literals.append((view, off, len(run), scope))
    key = lambda run: (run[1], run[0], run[2])
    return sorted(literals, key=key), sorted(cleared, key=key)


def _is_text(data):
    if b"\0" in data:
        return False
    try:
        data.decode("utf-8")
    except UnicodeDecodeError:
        return False
    return True


class Corpus:
    """The bytes LADXR already publishes: each text file's (valid UTF-8, no NUL) raw bytes and the
    bytes its hex-digit runs decode to at both alignments. Binary files are left out."""

    def __init__(self, files, commit=""):
        """files: [(name, bytes)]; commit: the checkout's commit, for the summary."""
        self.commit = commit
        pieces = []
        for _, data in files:
            if not _is_text(data):
                continue
            pieces.append(data)
            pieces.extend(decoded for _, decoded in _hex_decodings(data, 2))
        self._joined = b"".join(pieces)
        self._starts = []
        start = 0
        for piece in pieces:
            self._starts.append(start)
            start += len(piece)

    def __contains__(self, run):
        """Whether run lies whole inside one piece, never across two."""
        pos = self._joined.find(run)
        while pos != -1:
            i = bisect.bisect_right(self._starts, pos) - 1
            end = self._starts[i + 1] if i + 1 < len(self._starts) else len(self._joined)
            if pos + len(run) <= end:
                return True
            pos = self._joined.find(run, pos + 1)
        return False


class SetupError(Exception):
    pass


def _ladxr_git(directory, *args, data=None):
    """Bytes of git's stdout in the checkout directory, or None when git fails."""
    # A pre-push hook runs with GIT_DIR set, which would point git at the pushing repository.
    env = {k: v for k, v in os.environ.items() if not k.startswith("GIT_")}
    result = subprocess.run(["git", *args], cwd=directory, env=env, input=data,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    return result.stdout if result.returncode == 0 else None


def load_ladxr(directory):
    """The Corpus of the files tracked at LADXR_PIN by the git checkout in directory, which must
    be checked out at the pin; raises SetupError otherwise."""
    missing = f"no LADXR checkout in {directory} (run tests/ladx/oracle/setup.sh)"
    if not os.path.isdir(directory):
        raise SetupError(missing)
    top = _ladxr_git(directory, "rev-parse", "--show-toplevel")
    head = _ladxr_git(directory, "rev-parse", "HEAD")
    if top is None or head is None or \
            os.path.realpath(top.decode().strip()) != os.path.realpath(directory):
        raise SetupError(missing)
    head = head.decode().strip()
    if head != LADXR_PIN:
        raise SetupError(f"LADXR checkout in {directory} is at {head[:12]}, not at the pin "
                         f"{LADXR_PIN[:8]} (run tests/ladx/oracle/setup.sh)")
    listing = _ladxr_git(directory, "ls-tree", "-r", "-z", LADXR_PIN) or b""
    blobs = []
    for entry in listing.split(b"\0"):
        if not entry:
            continue
        meta, _, path = entry.partition(b"\t")
        mode, kind, obj = meta.split()
        if kind == b"blob" and mode != b"120000":
            blobs.append((path.decode("utf-8", "surrogateescape"), obj.decode()))
    batch = _ladxr_git(directory, "cat-file", "--batch",
                       data="".join(obj + "\n" for _, obj in blobs).encode())
    if not listing or batch is None:
        raise SetupError(f"cannot read LADXR {LADXR_PIN[:8]} in {directory}")
    files = []
    pos = 0
    for path, _ in blobs:
        header_end = batch.index(b"\n", pos)
        size = int(batch[pos:header_end].split()[2])
        files.append((path, batch[header_end + 1:header_end + 1 + size]))
        pos = header_end + 1 + size + 1
    return Corpus(files, LADXR_PIN)


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


PLANTED_TEXT = b"Hey! You're finally awake! The Wind Fish dreams on Koholint, so be quiet here."


def planted_rom():
    """A 16 KiB synthetic ROM: random bytes, then a zero run, a counting run, a three-value run and
    an English line."""
    rng = random.Random(0x1ADC)
    rom = bytearray(rng.getrandbits(8) for _ in range(16384))
    rom[0x2000:0x2040] = bytes(64)
    rom[0x2100:0x2140] = bytes(range(0x40))
    rom[0x2200:0x2240] = bytes(rng.choice(b"\x11\x5a\xc3") for _ in range(64))
    rom[0x2300:0x2300 + len(PLANTED_TEXT)] = PLANTED_TEXT
    return bytes(rom)


def _planted_cases(rom):
    """[(name, path, data, literals as [(view, length)], cleared as [(view, length, category)])]:
    one planted file per scope, view and clearance, and the corpus's edges."""
    encode = {
        "raw": lambda run: b"<" + run + b">",
        "hex": lambda run: b"x = 7" + run.hex().upper().encode() + b";\n",
        "bytelist": lambda run: b"db " + ", ".join("$%02X" % b for b in run).encode() + b"\n",
    }
    paths = {"strict": "data/ladx/planted.bin", "general": "notes/planted.c"}
    # The policy table restated, not read from THRESHOLDS, so that an edit there fails the proof.
    expected = {"strict": {"raw": 4, "hex": 4, "bytelist": 4},
                "general": {"raw": 16, "hex": 8, "bytelist": 8}}
    cases = []
    for scope, path in paths.items():
        for view, enc in encode.items():
            n = expected[scope][view]
            name = f"{scope}-{view}"
            cases += [
                (name + "-below", path, enc(rom[1000:1000 + n - 1]), [], []),
                (name, path, enc(rom[1000:1000 + n]), [(view, n)], []),
                (name + "-trivial", path, enc(rom[0x2110:0x2130]), [], [(view, 32, "trivial")]),
                (name + "-low-info", path, enc(rom[0x2200:0x2220]),
                 [(view, 32)] if scope == "strict" else [], [] if scope == "strict" else
                 [(view, 32, "low-info")]),
                (name + "-ladxr-public", path, enc(rom[3000:3024]), [],
                 [(view, 24, "ladxr-public")]),
            ]
    text = rom[0x2300:0x2300 + len(PLANTED_TEXT)]
    general = paths["general"]
    cases += [
        ("general-english-12", general, b"say " + text[0:12] + b"\n", [], []),
        ("general-english-public", general, b"<" + text[20:70] + b">", [],
         [("raw", 50, "ladxr-public")]),
        ("general-partly-public", general, text[10:70], [("raw", 60)], []),
        ("general-binary-corpus", general, rom[4000:4040], [("raw", 40)], []),
    ]
    return cases


def planted_corpus(rom):
    """A Corpus of one text file (an English line of rom, rom[3000:3024] in hex) and one binary
    file (rom[4000:4040]), which it must leave out."""
    text = rom[0x2300:0x2300 + len(PLANTED_TEXT)]
    return Corpus([
        ("names.py", b"NAME = '" + text[20:70] + b"'\nPATCH = '" + rom[3000:3024].hex().encode()
         + b"'\n"),
        ("gfx/tiles.bin", b"\0" + rom[4000:4040]),
    ], "planted")


def self_test():
    """None when the check finds exactly the planted runs, else the name of the case that failed."""
    rom = planted_rom()
    index = RomIndex(rom)
    corpus = planted_corpus(rom)
    for name, path, data, literals, cleared in _planted_cases(rom):
        got_literals, got_cleared = check_blob(path, data, index, corpus)
        if [(view, length) for view, _, length, _ in got_literals] != literals or \
                [(view, length, cat) for view, _, length, cat in got_cleared] != cleared:
            return name
    return None


def main(argv):
    parser = argparse.ArgumentParser(prog="nintendo_bytes.py")
    parser.add_argument("--rom", default=DEFAULT_ROM)
    parser.add_argument("--ladxr", default=os.environ.get("LADXR_DIR") or DEFAULT_LADXR)
    parser.add_argument("--scope", choices=("strict", "general"))
    parser.add_argument("--min", type=int)
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("paths", nargs="+")
    args = parser.parse_args(argv)
    if args.min is not None and args.min < 1:
        parser.error("--min must be at least 1")
    failed = self_test()
    if failed:
        print(f"nintendo-bytes: self-test failed: {failed}")
        return 1
    print(f"nintendo-bytes: self-test, {len(_planted_cases(b''))} planted files as expected")
    try:
        if not os.path.isfile(args.rom):
            raise SetupError(f"no ROM at {args.rom} (run tests/ladx/oracle/fetch_rom.sh)")
        with open(args.rom, "rb") as f:
            rom = f.read()
        files = files_under(args.paths)
        corpus = load_ladxr(args.ladxr)
    except SetupError as e:
        print(f"setup: nintendo-bytes: {e}")
        return 2
    index = RomIndex(rom)
    literal_count = 0
    counts = {category: 0 for category in CLEARANCES}
    for path in sorted(files):
        with open(path, "rb") as f:
            data = f.read()
        rel = os.path.relpath(os.path.abspath(path), TOP)
        literals, cleared = check_blob(rel, data, index, corpus, args.scope, args.min)
        literal_count += len(literals)
        lines = [(off, view, f"LITERAL {path} {view} @{off} {length} bytes {scope}")
                 for view, off, length, scope in literals]
        for view, off, length, category in cleared:
            counts[category] += 1
            if args.verbose:
                lines.append((off, view, f"CLEARED {path} {view} @{off} {length} bytes {category}"))
        for _, _, line in sorted(lines):
            print(line)
    override = f"min {args.min} (override), " if args.min else ""
    print(f"nintendo-bytes: {len(files)} files, rom {hashlib.sha1(rom).hexdigest()}, "
          f"ladxr {corpus.commit[:8]}, {override}{literal_count} literal runs, cleared "
          + ", ".join(f"{counts[c]} {c}" for c in CLEARANCES))
    return 1 if literal_count else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
