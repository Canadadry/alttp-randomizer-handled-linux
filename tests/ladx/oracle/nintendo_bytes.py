"""Usage: python3 -I tests/ladx/oracle/nintendo_bytes.py [--rom <file>] [--ladxr <dir>]
       [--inventory <file>] [--scope strict|general|writes|tables] [--min <n>] [--verbose] <file-or-dir>...

The Nintendo-bytes check: prints one `LITERAL <path> <view> @<offset> <length> bytes <scope>` line
per run of a file that also occurs in the ROM (--rom, default tmp/ladx/base.gbc) and is not
cleared, then `nintendo-bytes: <n> files, rom <sha1>, ladxr <short>, <k> literal runs, cleared <t>
trivial, <l> low-info, <p> ladxr-public, <c> computed, <i> invariant`; exit 1 when k > 0, 2 on a `setup:` line.
Never prints the bytes themselves. --verbose adds `CLEARED <path> <view> @<offset> <length> bytes
<category>`.

Policy. Every file under `data/ladx/` (the patch tables) is `tables`. Inside a case folder
(`tests/ladx/cases|combos/<case>/`), `run.txt` and `spoiler.json` are `general` and
`rom_writes.txt` is `writes`; any other path under STRICT_PREFIXES (ROM-derived
artifacts) is `strict`, any other path `general` (--scope overrides). Views: `raw`; `hex`, maximal
hex-digit runs decoded at both nibble alignments; `bytelist`, lists of 0xNN or $NN tokens
separated by commas and whitespace. Minimum contiguous match (THRESHOLDS): strict 4 in every
view; general raw 16, hex and bytelist 8; --min replaces them all and the summary says `min <n>
(override)`. Scope `writes`: the file must parse as a write log (write_log.py), else it is one
`writes ... unparsed` literal; its literal tokens are decoded at their byte alignment (view
`writes`, WRITES_MIN bytes), the rest of its text (offsets, tags, references) is checked at the
general thresholds. Scope `tables` likewise: the file must parse with tables.py, else one `tables
... unparsed` literal; the literal tokens of its B and T records at WRITES_MIN (view `tables`),
with neither the `computed` nor the `invariant` clearance; the rest at the general thresholds. Clearances, counted: `trivial` (an arithmetic progression mod 256),
`low-info` (at most 3 distinct values, general only), `ladxr-public` (the whole run occurs in the
text files of the LADXR checkout --ladxr, default LADXR_DIR or tests/ladx/oracle/ladxr, at
LADXR_PIN, or in their decoded hex-digit runs; else a `setup:` line), `computed` (view `writes`
only: the run occurs nowhere whole in the source bytes its token's group reads, ReadSets from
--inventory, default notes/ladx-inventory.md; read only when a writes run needs it, else a
`setup:` line), `invariant` (view `writes` only: every byte of the run lies in its record's
`I` ranges, the bytes the oracle's re-run of the writing C-group call on a scrambled copy of
the ROM state gave unchanged (write_log.py), and the run occurs nowhere whole inside the
C-knowable source map of known_source.py). Proves itself first, on every run, on planted files against a planted ROM,
corpus and inventory.
"""

import argparse
import bisect
import hashlib
import importlib.util
import os
import random
import re
import subprocess
import sys

TOP = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
DEFAULT_ROM = os.path.join(TOP, "tmp", "ladx", "base.gbc")
DEFAULT_LADXR = os.path.join(TOP, "tests", "ladx", "oracle", "ladxr")
DEFAULT_INVENTORY = os.path.join(TOP, "notes", "ladx-inventory.md")
LADXR_PIN = "4325207e4e55076e05f209fbdf16b860a90e42ef"


STRICT_PREFIXES = ("tests/ladx/cases/", "tests/ladx/combos/")
TABLES_PREFIX = "data/ladx/"
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


CASE_FOLDER = re.compile(r"tests/ladx/(?:cases|combos)/[^/]+/([^/]+)")
CASE_FILE_SCOPES = {"run.txt": "general", "spoiler.json": "general", "rom_writes.txt": "writes"}


def scope_of(path):
    """The policy of a repository-relative path: 'tables' under TABLES_PREFIX, 'writes' or
    'general' for a case folder's golden files (CASE_FILE_SCOPES), else 'strict' under
    STRICT_PREFIXES, else 'general'."""
    path = path.replace(os.sep, "/")
    if path.startswith(TABLES_PREFIX):
        return "tables"
    m = CASE_FOLDER.fullmatch(path)
    if m and m.group(1) in CASE_FILE_SCOPES:
        return CASE_FILE_SCOPES[m.group(1)]
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


CLEARANCES = ("trivial", "low-info", "ladxr-public", "computed", "invariant")
VIEWS = (("raw", _raw_runs), ("hex", _hex_runs), ("bytelist", _bytelist_runs))
WRITES_MIN = 4


def _clearance(run, scope, corpus):
    """The clearance category of a run's bytes, or None when the run is a literal."""
    step = (run[1] - run[0]) % 256 if len(run) > 1 else 0
    if all((b - a) % 256 == step for a, b in zip(run, run[1:])):
        return "trivial"
    if scope == "general" and len(set(run)) <= 3:
        return "low-info"
    if run in corpus:
        return "ladxr-public"
    return None


_SIBLINGS = {}


def _sibling(name):
    """The module name.py next to this file, loaded once."""
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), name + ".py")
    if path not in _SIBLINGS:
        spec = importlib.util.spec_from_file_location("nintendo_bytes_" + name, path)
        module = importlib.util.module_from_spec(spec)
        no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
        try:
            spec.loader.exec_module(module)
        finally:
            sys.dont_write_bytecode = no_bytecode
        _SIBLINGS[path] = module
    return _SIBLINGS[path]


_READ_ENTRY = re.compile(r"(?:bank 0x([0-9A-F]+)(?:\.\.0x([0-9A-F]+))?|table (\S+)) "
                         r"(source|written:\S+|mixed:\S+) (all|0x([0-9A-F]+)\.\.0x([0-9A-F]+))")
BANK = 0x4000


class ReadSets:
    """The source bytes each inventory group reads, for the `computed` clearance: the Reads
    (trace) entries of the group's rows (bank ranges as source bytes, table ranges as their
    entries' source spans, `written:` and `mixed:` ones also followed to the writer groups' sets,
    transitively; `mixed:` ones also as source), less the reads of rows a `computed` Read
    coverage entry names. `(group, run) in reads`: run occurs whole in them. inventory: the
    inventory's text, or a callable returning it on first use; tables(rom) -> {table: {key:
    (start, stop) or int}} (default known_source.parse). Raises SetupError on first use when the
    inventory cannot be read or parsed. `knows(run)`, for the `invariant` clearance: run occurs
    whole inside the C-knowable source map, known(rom) (default known_source.build) taken as
    its maximal runs of contiguous offsets."""

    def __init__(self, inventory, rom, tables=None, known=None):
        self._inventory, self.rom, self._tables = inventory, rom, tables
        self._known = known
        self._known_runs = None
        self._parsed = None
        self._pieces = {}

    def knows(self, run):
        if self._known_runs is None:
            known = (self._known or _sibling("known_source").build)(self.rom)
            self._known_runs = _sibling("write_log")._KnownRuns(self.rom, known)
        return self._known_runs.find(run) >= 0

    def _parse(self):
        if self._parsed is not None:
            return self._parsed
        text = self._inventory() if callable(self._inventory) else self._inventory
        check = _sibling("inventory_check")
        sections, bad = check.parse_inventory(text)
        named, named_bad = check.parse_named(text)
        if bad or named_bad or not sections:
            raise SetupError("the inventory does not parse (run inventory_check.py)")
        computed = set()
        for entry in named["Read coverage"]:
            if entry["Kind"] != "computed":
                continue
            m = re.fullmatch(r"`(.+)`(?: #(\d+))?", entry["Row"])
            row = "*" if entry["Row"] == "*" else m and (
                m.group(1) if m.group(2) is None else "%s #%s" % m.groups())
            computed.add((entry["Group"], row, entry["Target"]))
        reads = {}
        for rows in sections.values():
            for row in rows:
                group, cell = row["Group"], row["Reads (trace)"]
                own = reads.setdefault(group, [])
                if cell == "-":
                    continue
                for entry in cell.split("; "):
                    m = _READ_ENTRY.fullmatch(entry)
                    if not m:
                        raise SetupError("the inventory's Reads (trace) entry %r does not parse"
                                         % entry)
                    target = entry[:m.start(4) - 1]
                    if any((group, r, t) in computed for r in ("*", row["key"])
                           for t in ("*", target)):
                        continue
                    own.append(m.groups())
        self._parsed = reads
        return reads

    def _spans(self, entry, table_spans):
        first, last, table, prov, whole, lo, hi = entry
        if table is None:
            banks = range(int(first, 16), int(last or first, 16) + 1)
            return [(b * BANK + (0 if whole == "all" else int(lo, 16)),
                     b * BANK + (BANK if whole == "all" else int(hi, 16) + 1)) for b in banks]
        entries = table_spans().get(table)
        if entries is None:
            raise SetupError("the inventory reads table %s, which the ROM parse lacks" % table)
        keys = entries if whole == "all" else [k for k in entries if isinstance(k, int)
                                               and int(lo, 16) <= k <= int(hi, 16)]
        return [entries[k] for k in keys if isinstance(entries[k], tuple)]

    def spans(self, group):
        """[(start, stop)] of the source bytes group reads, merged, sorted."""
        reads = self._parse()
        parsed = []

        def table_spans():
            if not parsed:
                parse = self._tables or _sibling("known_source").parse
                parsed.append(parse(self.rom))
            return parsed[0]

        spans, seen, todo = [], {group}, [group]
        while todo:
            for entry in reads.get(todo.pop(), []):
                prov = entry[3]
                if prov == "source" or prov.startswith("mixed:"):
                    spans += self._spans(entry, table_spans)
                if prov != "source":
                    for writer in prov.partition(":")[2].split(","):
                        if writer not in seen:
                            seen.add(writer)
                            todo.append(writer)
        merged = []
        for start, stop in sorted(spans):
            if merged and start <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], stop)
            else:
                merged.append([start, stop])
        return [tuple(s) for s in merged]

    def __contains__(self, item):
        """(group, run): whether run occurs whole in the source bytes group reads."""
        group, run = item
        if group not in self._pieces:
            self._pieces[group] = [self.rom[a:b] for a, b in self.spans(group)]
        return any(run in piece for piece in self._pieces[group])


_LITERAL_TOKEN = re.compile(rb"(?:[0-9a-f]{2})+")


def _write_tokens(data):
    """([(offset, group, bytes, invariant flags)] of a write log's literal tokens, [(offset,
    bytes)] of the text around them), offsets in data's characters, a flag per byte for its `I`
    ranges; None when data is not a valid write log."""
    write_log = _sibling("write_log")
    try:
        marks = write_log.invariants(data.decode("ascii"))
    except (UnicodeDecodeError, ValueError, write_log.WriteLogError):
        return None
    literals, rest = [], []
    group, kept, pos = None, 0, 0
    for number, line in enumerate(data.split(b"\n"), 1):
        fields = line.split(b" ")
        if fields[0] == b"G":
            group = fields[1].decode()
        first = {b"B": 2, b"T": 3}.get(fields[0])
        flags = bytearray(len(line))
        for start, n in marks.get(number, ()):
            flags[start:start + n] = b"\x01" * n
        at, atom = pos, 0
        for i, field in enumerate(fields):
            if first is not None and i >= first and _LITERAL_TOKEN.fullmatch(field):
                if at > kept:
                    rest.append((kept, data[kept:at]))
                decoded = bytes.fromhex(field.decode())
                literals.append((at, group, decoded, flags[atom:atom + len(decoded)]))
                kept = at + len(field)
            if first is not None and i >= first:
                atom += _token_length(field)
            at += len(field) + 1
        pos += len(line) + 1
    if kept < len(data):
        rest.append((kept, data[kept:]))
    return literals, rest


def _token_length(field):
    """The number of bytes a valid write-log token stands for."""
    if field[:1] == b"=":
        return int(field[1:], 16)
    if field[:1] == b"@":
        return int(field.partition(b"+")[2], 16)
    if field[:1] == b"p":
        return 0
    return len(field) // 2


_TABLE_RECORDS = {b"B": 2, b"T": 3}


def _table_tokens(data):
    """_write_tokens' shape for a patch-table file (tables.py), with no group and no invariant
    flag; None when tables.py's parser refuses data."""
    tables = _sibling("tables")
    try:
        tables.parse(data.decode("ascii"))
    except (UnicodeDecodeError, tables.TablesError):
        return None
    literals, rest = [], []
    kept, pos = 0, 0
    for line in data.split(b"\n"):
        fields = line.split(b" ")
        first = _TABLE_RECORDS.get(fields[0])
        at = pos
        for i, field in enumerate(fields):
            if first is not None and i >= first and _LITERAL_TOKEN.fullmatch(field):
                if at > kept:
                    rest.append((kept, data[kept:at]))
                decoded = bytes.fromhex(field.decode())
                literals.append((at, None, decoded, bytes(len(decoded))))
                kept = at + len(field)
            at += len(field) + 1
        pos += len(line) + 1
    if kept < len(data):
        rest.append((kept, data[kept:]))
    return literals, rest


def _check_tokens(view, data, index, corpus, min_len, reads):
    """(literals, cleared) of a write log (view `writes`) or a patch-table file (view `tables`):
    its literal tokens at their byte alignment at WRITES_MIN, the text around them at the general
    thresholds; one `unparsed` literal for the whole file when it does not parse. reads (writes
    only) enables the `computed` and `invariant` clearances."""
    parsed = (_write_tokens if view == "writes" else _table_tokens)(data)
    if parsed is None:
        return [(view, 0, len(data), "unparsed")], []
    tokens, rest = parsed
    literals, cleared = [], []
    n = min_len or WRITES_MIN
    for start, group, decoded, flags in tokens:
        for off, length in _merged_hits(decoded, index.windows(n), n):
            run = decoded[off:off + length]
            category = _clearance(run, "strict", corpus) or \
                ("computed" if reads is not None and (group, run) not in reads else None) or \
                ("invariant" if reads is not None and all(flags[off:off + length])
                 and not reads.knows(run) else None)
            where = start + 2 * off
            if category:
                cleared.append((view, where, length, category))
            else:
                literals.append((view, where, length, "strict"))
    starts = [start for start, _ in rest]
    joined = b"\n".join(piece for _, piece in rest)
    bases, at = [], 0
    for start, piece in rest:
        bases.append(at)
        at += len(piece) + 1

    def where(off):
        i = bisect.bisect_right(bases, off) - 1
        return starts[i] + off - bases[i]

    for general_view, runs in VIEWS:
        m = min_len or THRESHOLDS[("general", general_view)]
        for off, run in runs(joined, index, m):
            category = _clearance(run, "general", corpus)
            if category:
                cleared.append((general_view, where(off), len(run), category))
            else:
                literals.append((general_view, where(off), len(run), "general"))
    return literals, cleared


def check_blob(path, data, index, corpus, scope=None, min_len=None, reads=None):
    """(literals, cleared) for the blob data at the repository-relative path: literals as
    [(view, offset, length, scope)], cleared as [(view, offset, length, category)], both sorted by
    offset. scope ('strict', 'general' or 'writes') overrides the path's; min_len overrides every
    threshold; reads, a ReadSets, enables the `computed` and `invariant` clearances of the writes
    view (never of the tables view)."""
    scope = scope or scope_of(path)
    assert scope in ("strict", "general", "writes", "tables"), scope
    key = lambda run: (run[1], run[0], run[2])
    if scope in ("writes", "tables"):
        literals, cleared = _check_tokens(scope, data, index, corpus, min_len,
                                          reads if scope == "writes" else None)
        return sorted(literals, key=key), sorted(cleared, key=key)
    literals, cleared = [], []
    for view, runs in VIEWS:
        n = min_len or THRESHOLDS[(scope, view)]
        for off, run in runs(data, index, n):
            category = _clearance(run, scope, corpus)
            if category:
                cleared.append((view, off, len(run), category))
            else:
                literals.append((view, off, len(run), scope))
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


def read_inventory(path):
    """The inventory's text; SetupError when it cannot be read."""
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8")
    except (OSError, UnicodeDecodeError):
        raise SetupError(f"no inventory at {path}")


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
    paths = {"strict": "tests/ladx/cases/planted.bin", "general": "notes/planted.c"}
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
    cases += _planted_case_folder(rom)
    cases += _planted_tables(rom)
    return cases


def _planted_tables(rom):
    """The planted files of the patch tables' policy: literal tokens at 4 bytes at their byte
    alignment with no `computed` clearance, record syntax and other text at the general
    thresholds, a file tables.py refuses one literal."""
    path = "data/ladx/maker.txt"

    def table(*records):
        return ("ladx-tables 1\ngroup maker\ntables texts\nunit maker.1 race\nE 0\n"
                + "".join(r + "\n" for r in records)).encode()

    text = rom[0x2300:0x2300 + len(PLANTED_TEXT)]
    unparsed = table("B 10 0")
    return [
        ("tables-below", path, table("B 10 " + rom[0x900:0x903].hex()), [], []),
        ("tables-literal", path, table("B 10 =1 " + rom[0x900:0x904].hex()), [("tables", 4)],
         []),
        ("tables-not-computed", path, table("T texts 2 " + rom[0x900:0x908].hex()),
         [("tables", 8)], []),
        ("tables-misaligned", path, table("B 10 0" + rom[0x900:0x908].hex() + "0"), [], []),
        ("tables-syntax", path, table("B 10 00").replace(b"race\nE 0", b"race\nE "
                                                         + text[51:59]),
         [], []),
        ("tables-order-general", "data/ladx/order.txt",
         b"ladx-tables-order 1\nsection a.py f\nC maker - say('" + text[0:12] + b"')\n", [], []),
        ("tables-unparsed", path, unparsed, [("tables", len(unparsed))], []),
    ]


def planted_inventory(rows, coverage=()):
    """An inventory text of one section table of rows [(call, Reads (trace) cell, group)] and a
    Read coverage table of entries [(group, row, target, kind)]."""
    def table(columns, lines):
        return ("| " + " | ".join(columns) + " |\n|" + "---|" * len(columns) + "\n"
                + "".join("| " + " | ".join(cells) + " |\n" for cells in lines))
    groups = sorted({group for _, _, group in rows})
    return ("## generator.py generateRom\n\n"
            + table(["Call", "Settings", "Args", "RNG (grep)", "Reads (grep)", "Reads (trace)",
                     "Notes", "Side", "Group"],
                    [["`%s`" % call, "-", "-", "-", "-", cell, "", "C", group]
                     for call, cell, group in rows])
            + "\n## Groups\n\n" + table(["Group", "Side", "Settings key", "Depends on",
                                           "Evidence"], [[g, "C", "-", "-", g] for g in groups])
            + "\n## Read coverage\n\n" + table(["Group", "Row", "Target", "Range", "Kind",
                                                  "Reason"],
                                                 [[g, r, t, "*", k, "planted"]
                                                  for g, r, t, k in coverage])
            + "\n## Blind spots\n\n" + table(["File", "Line", "Verdict"], []))


def planted_reads(rom):
    """The ReadSets of the planted case folder: `reader` reads bank 0x00 0x0400..0x04FF and text
    entry 1 (0x0500..0x051F), `maker` reads nothing, `follower` reads what `reader` wrote, and
    `summer` reads 0x0400..0x04FF only through a `computed` coverage row; the C-knowable map is
    0x04C0..0x04CF."""
    text = planted_inventory([
        ("reader(rom)", "bank 0x00 source 0x0400..0x04FF; table texts source 0x0001..0x0001",
         "reader"),
        ("maker(rom)", "-", "maker"),
        ("follower(rom)", "bank 0x00 written:reader 0x0010..0x0011", "follower"),
        ("checksum(rom)", "bank 0x00 mixed:maker 0x0400..0x04FF", "summer"),
    ], [("summer", "`checksum(rom)`", "*", "computed")])
    return ReadSets(text, rom, lambda _: {"texts": {0: (0x600, 0x610), 1: (0x500, 0x520)}},
                    lambda _: set(range(0x4C0, 0x4D0)))


def _planted_case_folder(rom):
    """The planted files of the case folders' policy: run.txt and spoiler.json general, the
    seed strict, rom_writes.txt's literal tokens at 4 bytes and its other text general, and the
    `computed` clearance against planted_reads."""
    folder = "tests/ladx/cases/planted/"
    writes = folder + "rom_writes.txt"

    def log(*lines):
        return ("ladx-rom-writes 1\ntables texts\n" + "".join(l + "\n" for l in lines)).encode()

    leak = rom[0x480:0x488]
    unparsed = log("G maker", "B 10 0")
    elsewhere = rom[0x900:0x908]
    return [
        ("case-run-general", folder + "run.txt", b"rom " + rom[1000:1015] + b"\n", [], []),
        ("case-run-16", folder + "run.txt", rom[1000:1016], [("raw", 16)], []),
        ("case-spoiler-general", folder + "spoiler.json", b'"' + rom[1000:1008].hex().encode()
         [:15] + b'"', [], []),
        ("case-seed-strict", folder + "seed", rom[1000:1004].hex().encode() + b"\n",
         [("hex", 4)], []),
        ("writes-below", writes, log("G maker", "B 10 " + rom[0x900:0x903].hex()), [], []),
        ("writes-literal", writes, log("G reader", "B 10 " + leak.hex()), [("writes", 8)], []),
        ("writes-computed", writes, log("G maker", "B 10 " + elsewhere.hex()), [],
         [("writes", 8, "computed")]),
        ("writes-in-read-set", writes, log("G maker", "B 10 " + elsewhere.hex(), "G reader",
                                           "T texts 2 " + rom[0x508:0x50c].hex()),
         [("writes", 4)], [("writes", 8, "computed")]),
        ("writes-followed", writes, log("G follower", "B 10 =1 " + leak.hex()),
         [("writes", 8)], []),
        ("writes-checksum-excluded", writes, log("G summer", "B 10 " + leak.hex()), [],
         [("writes", 8, "computed")]),
        ("writes-misaligned", writes, log("G maker", "B 10 0" + leak.hex() + "0"), [], []),
        ("writes-trivial", writes, log("G reader", "B 10 " + rom[0x2110:0x2120].hex()), [],
         [("writes", 16, "trivial")]),
        ("writes-reference-general", writes, log("G maker", "B 10 @%s+4" % rom[1000:1008].hex()),
         [("hex", 8)], []),
        ("writes-unparsed", writes, unparsed, [("writes", len(unparsed))], []),
        ("writes-invariant", writes, log("G reader", "B 10 =1 " + leak.hex(), "I 4 1+8"), [],
         [("writes", 8, "invariant")]),
        ("writes-invariant-partly", writes, log("G reader", "B 10 " + leak.hex(), "I 4 1+7"),
         [("writes", 8)], []),
        ("writes-invariant-known", writes, log("G reader", "B 10 " + rom[0x4C4:0x4CC].hex(),
                                               "I 4 0+8"), [("writes", 8)], []),
    ]


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
    reads = planted_reads(rom)
    for name, path, data, literals, cleared in _planted_cases(rom):
        got_literals, got_cleared = check_blob(path, data, index, corpus, None, None, reads)
        if [(view, length) for view, _, length, _ in got_literals] != literals or \
                [(view, length, cat) for view, _, length, cat in got_cleared] != cleared:
            return name
    return None


def main(argv):
    parser = argparse.ArgumentParser(prog="nintendo_bytes.py")
    parser.add_argument("--rom", default=DEFAULT_ROM)
    parser.add_argument("--ladxr", default=os.environ.get("LADXR_DIR") or DEFAULT_LADXR)
    parser.add_argument("--inventory", default=DEFAULT_INVENTORY)
    parser.add_argument("--scope", choices=("strict", "general", "writes", "tables"))
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
    reads = ReadSets(lambda: read_inventory(args.inventory), rom)
    literal_count = 0
    counts = {category: 0 for category in CLEARANCES}
    for path in sorted(files):
        with open(path, "rb") as f:
            data = f.read()
        rel = os.path.relpath(os.path.abspath(path), TOP)
        try:
            literals, cleared = check_blob(rel, data, index, corpus, args.scope, args.min, reads)
        except SetupError as e:
            print(f"setup: nintendo-bytes: {e}")
            return 2
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
