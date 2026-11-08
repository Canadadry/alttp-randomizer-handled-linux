"""Usage: python3 -I tests/ladx/oracle/inventory_check.py [--ladxr <dir>] [--inventory <file>] [--print]

The check of notes/ladx-inventory.md against LADXR's frozen (patched) sources (--ladxr, default
tests/ladx/oracle/ladxr). It extracts every statement of the sections below and diffs them, row
by row, against the inventory's table under `## <file> <function>`, then validates the tables
under `## Groups`, `## Read coverage` and `## Blind spots`; prints one
`MISSING`, `STALE`, `MISMATCH` or `BAD` line per finding, else
`inventory: <rows> rows in <n> sections match <commit>`. Exit 1 on a finding, 2 on `setup:`.
`--print` writes the extracted section tables instead (Reads (trace), Notes, Side and Group kept
from the inventory when it has the row, else `-`, empty, `?`, `?`). Proves itself first, on
every run, on a planted tree and inventory.

Section columns: COLUMNS; Side one of SIDES. Validation: every row's group is listed under Groups
with the row's side, and every listed group is used by a row; a `C` group's Evidence (comma
separated) names the group itself (its write-log group) and no `extract:<name>` (ROM content is
never committed: a value read from the source ROM is Evidence `rom`, read from the user's ROM at
runtime and verified by rom-check); a coverage Kind is one of COVERAGE_KINDS or `cut:<id>`, and
its group is listed; the Blind spots rows (File, `Line`) equal read_trace.py's `blind_spots` one to one (`MISSING`,
`STALE`), each with a Verdict.

Sections: generator.py generateRom; randomizer.py Randomizer.__init__ after
`item_placer.run(self.rnd)`; romTables.py ROMWithTables.__init__ and ROMWithTables.save; rom.py
ROM.save and ROM.fixHeader. A function body ends at the first non-blank line indented no deeper
than its `def`, a line continuing a statement (multi-line string, bracket) never ending it;
`\\r` is stripped. A row is every statement's first line but blanks, comments, `if`/`elif`/
`else`/`for`/`while`/`try`/`except` headers, `return`, `import` and `print(` lines, greped with
its continuation lines. Its key is its stripped text, ` #<k>` on the k-th occurrence of that
text in the whole body (read_trace.py's site key).

Grep columns, over the line and its scope (every enclosing header, and for an `elif`/`else`
every earlier header of its chain): `Settings` the `settings.<name>`s, `*` when the line passes
`settings` whole; `Args` the same for `args`; `RNG (grep)` `rnd` and `seed` named by the line,
`global` when a callee `patches.<m>.<f>`, `hints.<f>` or `mapgen.<f>` has a body using the
module `random` (`random.Random(...)` is not global use; one level only); `Reads (grep)`
`placement` (item_list, iteminfo_list, spot), `world_setup`, `logic` (passed whole).
"""

import argparse
import difflib
import io
import itertools
import os
import re
import subprocess
import sys
import tempfile
import tokenize

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_LADXR = os.path.join(HERE, "ladxr")
DEFAULT_INVENTORY = os.path.join(TOP, "notes", "ladx-inventory.md")

SECTIONS = [
    ("generator.py", "generateRom", None),
    ("randomizer.py", "Randomizer.__init__", "item_placer.run(self.rnd)"),
    ("romTables.py", "ROMWithTables.__init__", None),
    ("romTables.py", "ROMWithTables.save", None),
    ("rom.py", "ROM.save", None),
    ("rom.py", "ROM.fixHeader", None),
]
COLUMNS = ["Call", "Settings", "Args", "RNG (grep)", "Reads (grep)", "Reads (trace)", "Notes",
           "Side", "Group"]
GREP_COLUMNS = COLUMNS[1:5]
KEPT_COLUMNS = COLUMNS[5:]
SIDES = ("table", "C", "none", "unused")
GROUP_COLUMNS = ["Group", "Side", "Settings key", "Depends on", "Evidence"]
COVERAGE_COLUMNS = ["Group", "Row", "Target", "Range", "Kind", "Reason"]
BLIND_COLUMNS = ["File", "Line", "Verdict"]
NAMED_TABLES = {"Groups": GROUP_COLUMNS, "Read coverage": COVERAGE_COLUMNS,
                "Blind spots": BLIND_COLUMNS}
COVERAGE_KINDS = ("copy", "written", "computed", "table", "rom")

HEADER = re.compile(r"(if|elif|else|for|while|try|except)\b")
SKIPPED = re.compile(r"(#|return\b|import\b|from\s+\S+\s+import\b|print\()")
CALLEE = re.compile(r"\b(?:patches\.(\w+)\.(\w+)|(hints)\.(\w+)|(mapgen)\.(\w+))\s*\(")
# random.Random(seed) builds a private generator: it does not touch the global state.
GLOBAL_RANDOM = re.compile(r"(?<![\w.])random\.(?!Random\b)\w")
PLACEMENT = re.compile(r"\b(item_list|iteminfo_list|spot)\b")
LOGIC_WHOLE = re.compile(r"(?<![\w.])(?:self\.)?_*logic(?![\w.])")


class SetupError(Exception):
    pass


def section_name(file, function):
    return "%s %s" % (file, function)


def _read_lines(path):
    with open(path, "rb") as f:
        return f.read().decode("utf-8", "replace").replace("\r", "").split("\n")


def _indent(line):
    return len(line) - len(line.lstrip())


def _continuations(lines):
    found = set()
    start = None
    skipped = (tokenize.NL, tokenize.COMMENT, tokenize.INDENT, tokenize.DEDENT,
               tokenize.ENCODING, tokenize.ENDMARKER)
    try:
        for tok in tokenize.generate_tokens(io.StringIO("\n".join(lines) + "\n").readline):
            if tok.type == tokenize.NEWLINE:
                if start is not None:
                    found.update(range(start, tok.start[0]))
                start = None
            elif tok.type not in skipped and start is None:
                start = tok.start[0]
    except (tokenize.TokenError, SyntaxError):
        return set()
    return found


def _function_body(lines, qualname, continued):
    stack = []
    for n, line in enumerate(lines):
        text = line.strip()
        if not text or text.startswith("#") or n in continued:
            continue
        while stack and _indent(line) <= stack[-1][0]:
            stack.pop()
        m = re.match(r"(?:async\s+)?(class|def)\s+(\w+)", text)
        if not m:
            continue
        stack.append((_indent(line), m.group(2)))
        if m.group(1) == "def" and ".".join(name for _, name in stack) == qualname:
            body = []
            for k in range(n + 1, len(lines)):
                if k not in continued and lines[k].strip() and _indent(lines[k]) <= _indent(line):
                    break
                body.append((k, lines[k], k in continued))
            return body
    return None


def _scope(body, i):
    texts = []
    current = _indent(body[i][1])
    for j in range(i - 1, -1, -1):
        _, line, continues = body[j]
        text = line.strip()
        if not text or text.startswith("#") or continues or _indent(line) >= current:
            continue
        current = _indent(line)
        texts.append(text)
        if re.match(r"(elif|else)\b", text):
            for k in range(j - 1, -1, -1):
                _, other, continues = body[k]
                if not other.strip() or continues or _indent(other) > current:
                    continue
                if _indent(other) < current:
                    break
                if re.match(r"elif\b", other.strip()):
                    texts.append(other.strip())
                    continue
                if re.match(r"if\b", other.strip()):
                    texts.append(other.strip())
                break
    return texts


def _names(pattern, texts):
    return sorted({m for text in texts for m in re.findall(pattern, text)})


def _whole(name, text):
    return re.search(r"(?<![\w.])%s(?![\w.])" % name, text) is not None


def _column(names, whole):
    names = list(names) + (["*"] if whole else [])
    return ", ".join(names) if names else "-"


class Sources:
    def __init__(self, root):
        self.root = root
        self._lines = {}
        self._continuations = {}

    def lines(self, rel):
        if rel not in self._lines:
            path = os.path.join(self.root, rel)
            self._lines[rel] = _read_lines(path) if os.path.isfile(path) else None
        return self._lines[rel]

    def continuations(self, rel):
        if rel not in self._continuations:
            self._continuations[rel] = _continuations(self.lines(rel))
        return self._continuations[rel]

    def uses_global_random(self, module, function):
        for rel in (module + ".py", os.path.join(module, "__init__.py")):
            lines = self.lines(rel)
            if lines is None:
                continue
            body = _function_body(lines, function, self.continuations(rel))
            if body is not None:
                return any(GLOBAL_RANDOM.search(line) for _, line, _ in body)
        return False


def _rng(line, sources):
    found = [name for name in ("rnd", "seed") if re.search(r"\b%s\b" % name, line)]
    for m in CALLEE.finditer(line):
        if m.group(1):
            module, function = "patches/" + m.group(1), m.group(2)
        else:
            module, function = (m.group(3) or m.group(5)), (m.group(4) or m.group(6))
        if "global" not in found and sources.uses_global_random(module, function):
            found.append("global")
    return ", ".join(found) if found else "-"


def _reads(texts, line):
    found = []
    if any(PLACEMENT.search(t) for t in texts):
        found.append("placement")
    if any(re.search(r"\bworld_setup\b", t) for t in texts):
        found.append("world_setup")
    if LOGIC_WHOLE.search(line):
        found.append("logic")
    return ", ".join(found) if found else "-"


def extract_section(sources, file, function, start):
    lines = sources.lines(file)
    body = (_function_body(lines, function, sources.continuations(file)) if lines is not None
            else None)
    if body is None:
        raise SetupError("no %s in %s" % (function, os.path.join(sources.root, file)))
    seen = {}
    rows = []
    started = start is None
    for i, (_, line, continues) in enumerate(body):
        text = line.strip()
        if not text:
            continue
        seen[text] = seen.get(text, 0) + 1
        if not started:
            started = text == start
            continue
        if continues or HEADER.match(text) or SKIPPED.match(text):
            continue
        assert "`" not in text, "a backtick in a source line cannot be quoted: " + text
        statement = " ".join([text] + [l.strip() for _, l, c in
                                       itertools.takewhile(lambda b: b[2], body[i + 1:])])
        scope = [statement] + _scope(body, i)
        rows.append({
            "text": text, "k": seen[text],
            "Settings": _column(_names(r"\bsettings\.(\w+)", scope),
                                _whole("settings", statement)),
            "Args": _column(_names(r"\bargs\.(\w+)", scope), _whole("args", statement)),
            "RNG (grep)": _rng(statement, sources),
            "Reads (grep)": _reads(scope, statement),
        })
    if not started:
        raise SetupError("no line %r in %s %s" % (start, file, function))
    return rows


def extract(root):
    sources = Sources(root)
    return [(section_name(f, fn), extract_section(sources, f, fn, start))
            for f, fn, start in SECTIONS]


def row_key(row):
    return row["text"] if row["k"] == 1 else "%s #%d" % (row["text"], row["k"])


def _split_cells(line):
    cells = re.split(r"(?<!\\)\|", line.strip())
    if cells and cells[0] == "":
        cells = cells[1:]
    if cells and cells[-1] == "":
        cells = cells[:-1]
    return [c.strip().replace("\\|", "|") for c in cells]


def _escape(text):
    return text.replace("|", "\\|")


def _tables(text, columns_of):
    tables, bad = {}, []
    heading = None
    lines = text.replace("\r", "").split("\n")
    n = 0
    while n < len(lines):
        line = lines[n]
        if line.startswith("## "):
            heading = line[3:].strip()
            n += 1
            continue
        if heading not in columns_of or not line.startswith("|"):
            n += 1
            continue
        name, heading = heading, None
        columns = columns_of[name]
        if name in tables:
            bad.append("BAD %s: a second table" % name)
        table = []
        tables[name] = table
        header = _split_cells(line)
        if header != columns:
            bad.append("BAD %s: columns %s, expected %s" % (name, " | ".join(header),
                                                            " | ".join(columns)))
        n += 2
        while n < len(lines) and lines[n].startswith("|"):
            cells = _split_cells(lines[n])
            n += 1
            if len(cells) != len(columns):
                bad.append("BAD %s: malformed row %s" % (name, lines[n - 1]))
                continue
            table.append(cells)
    return tables, bad


def parse_inventory(text):
    raw, bad = _tables(text, {section_name(f, fn): COLUMNS for f, fn, _ in SECTIONS})
    tables = {}
    for section, rows in raw.items():
        tables[section] = []
        for cells in rows:
            m = re.fullmatch(r"`(.+)`(?: #(\d+))?", cells[0])
            if not m:
                bad.append("BAD %s: malformed row | %s |" % (section, " | ".join(cells)))
                continue
            row = dict(zip(COLUMNS, cells))
            row["key"] = m.group(1) if m.group(2) is None else "%s #%s" % m.groups()
            tables[section].append(row)
    return tables, bad


def parse_named(text):
    raw, bad = _tables(text, NAMED_TABLES)
    named = {}
    for name, columns in NAMED_TABLES.items():
        if name not in raw:
            bad.append("BAD %s: no table under ## %s" % (name, name))
        named[name] = [dict(zip(columns, cells)) for cells in raw.get(name, [])]
    return named, bad


def site_groups(text):
    tables, _ = parse_inventory(text)
    return {"%s\t%s" % (section, row["key"]): row["Group"]
            for section, rows in tables.items() for row in rows}


def diff(extracted, tables):
    findings = []
    for section, rows in extracted:
        inventory = tables.get(section, [])
        for row in inventory:
            if row["Side"] not in SIDES:
                findings.append("BAD %s: %s: Side %r is not one of %s"
                                % (section, row["key"], row["Side"], ", ".join(SIDES)))
            if not row["Group"]:
                findings.append("BAD %s: %s: empty Group" % (section, row["key"]))
        source_keys = [row_key(r) for r in rows]
        inventory_keys = [r["key"] for r in inventory]
        matcher = difflib.SequenceMatcher(None, inventory_keys, source_keys, autojunk=False)
        for op, i1, i2, j1, j2 in matcher.get_opcodes():
            if op == "equal":
                for a, b in zip(inventory[i1:i2], rows[j1:j2]):
                    for column in GREP_COLUMNS:
                        if a[column] != b[column]:
                            findings.append("MISMATCH %s: %s: %s inventory %s, source %s"
                                            % (section, a["key"], column, a[column], b[column]))
                continue
            for key in source_keys[j1:j2]:
                findings.append("MISSING %s: %s" % (section, key))
            for key in inventory_keys[i1:i2]:
                findings.append("STALE %s: %s" % (section, key))
    return findings


def _items(cell):
    return [item.strip() for item in cell.split(",") if item.strip() not in ("", "-")]


def _unquote(cell):
    m = re.fullmatch(r"`(.*)`", cell)
    return m.group(1) if m else cell


def validate(tables, named, blind):
    findings = []
    groups = {}
    for row in named["Groups"]:
        name = row["Group"]
        if name in groups:
            findings.append("BAD Groups: %s: listed twice" % name)
            continue
        groups[name] = row
        if row["Side"] not in SIDES:
            findings.append("BAD Groups: %s: Side %r is not one of %s"
                            % (name, row["Side"], ", ".join(SIDES)))
    used = set()
    for section, rows in tables.items():
        for row in rows:
            group = groups.get(row["Group"])
            used.add(row["Group"])
            if not row["Group"] or row["Side"] not in SIDES:
                continue
            if group is None:
                findings.append("BAD %s: %s: group %s is not in Groups"
                                % (section, row["key"], row["Group"]))
            elif group["Side"] != row["Side"]:
                findings.append("BAD %s: %s: Side %s, its group %s is %s"
                                % (section, row["key"], row["Side"], row["Group"],
                                   group["Side"]))
    for name, row in groups.items():
        if name not in used:
            findings.append("BAD Groups: %s: no row uses it" % name)
        evidence = _items(row["Evidence"])
        if row["Side"] == "C" and name not in evidence:
            findings.append("BAD Groups: %s: a C group whose Evidence does not name its "
                            "write-log group %s" % (name, name))
        for item in evidence:
            if item.startswith("extract:"):
                findings.append("BAD Groups: %s: Evidence %s: ROM content is never committed, "
                                "use rom" % (name, item))
    for row in named["Read coverage"]:
        where = "BAD Read coverage: %s %s: " % (row["Group"], row["Row"])
        kind = row["Kind"]
        if row["Group"] not in groups:
            findings.append(where + "group %s is not in Groups" % row["Group"])
        if not (kind in COVERAGE_KINDS or kind.startswith("cut:") and len(kind) > 4):
            findings.append(where + "kind %r is not one of %s, cut:<id>"
                            % (kind, ", ".join(COVERAGE_KINDS)))
    listed = []
    for row in named["Blind spots"]:
        spot = (row["File"], _unquote(row["Line"]))
        listed.append(spot)
        if not row["Verdict"]:
            findings.append("BAD Blind spots: %s: %s: empty Verdict" % spot)
    want = list(blind)
    for spot in listed:
        if spot in want:
            want.remove(spot)
        else:
            findings.append("STALE Blind spots: %s: %s" % spot)
    for spot in want:
        findings.append("MISSING Blind spots: %s: %s" % spot)
    return findings


def _blind_spots(root):
    import importlib.util
    spec = importlib.util.spec_from_file_location("read_trace",
                                                  os.path.join(HERE, "read_trace.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.blind_spots(root)


def check(root, text):
    tables, bad = parse_inventory(text)
    named, named_bad = parse_named(text)
    return (bad + named_bad + diff(extract(root), tables) +
            validate(tables, named, _blind_spots(root)))


def render(extracted, tables=None):
    out = []
    for section, rows in extracted:
        known = {r["key"]: r for r in (tables or {}).get(section, [])}
        out += ["## " + section, "", "| " + " | ".join(COLUMNS) + " |",
                "|" + "---|" * len(COLUMNS)]
        for row in rows:
            old = known.get(row_key(row), {})
            call = "`%s`" % row["text"] + ("" if row["k"] == 1 else " #%d" % row["k"])
            cells = [call] + [row[c] for c in GREP_COLUMNS] + [
                old.get(c, {"Reads (trace)": "-", "Notes": ""}.get(c, "?"))
                for c in KEPT_COLUMNS]
            out.append("| " + " | ".join(_escape(c) for c in cells) + " |")
        out.append("")
    return "\n".join(out)


PLANTED_SOURCES = {
    "generator.py": """\
import patches.planted


def generateRom(args, settings, seed, logic, *, rnd=None):
    rom = ROMWithTables(open(args.input_filename, 'rb'))
    patches.planted.fixed(rom)
    if settings.music == 'random':
        patches.planted.shuffle(rom)
    elif settings.music == 'off':
        patches.planted.fixed(rom)
    for spot in logic.iteminfo_list:
        spot.patch(rom, spot.item)
    return rom
""",
    "randomizer.py": """\
class Randomizer:
    def __init__(self, args, settings, *, seed=None):
        item_placer.run(self.rnd)
        rom = generator.generateRom(args, settings, self.seed, self.__logic, rnd=self.rnd)
""",
    "romTables.py": """\
class ROMWithTables(ROM):
    def __init__(self, filestream):
        self.texts = Texts(self)

    def save(self, filename, *, name=None):
        self.texts.store(self)
""",
    "rom.py": """\
class ROM:
    def save(self, file, *, name=None):
        self.fixHeader(name=name)

    def fixHeader(self, *, name=None):
        self.banks[0][0x14D] = 0
""",
    "patches/planted.py": """\
import random


def fixed(rom):
    rom.banks[0][0] = 1
    view = memoryview(rom.banks[0])


def shuffle(rom):
    rom.patch(0, 0x10, ASM(\"\"\"
ld a, 1
\"\"\"), ASM(\"\"\"
ld a, 2
\"\"\"))
    rom.banks[0][1] = random.randint(0, 3)
""",
}

PLANTED_COLUMNS = "| " + " | ".join(COLUMNS) + " |\n|" + "---|" * len(COLUMNS) + "\n"


def _planted_table(name, columns, rows):
    return ("## %s\n\n| %s |\n|%s\n" % (name, " | ".join(columns), "---|" * len(columns))
            + "".join(rows))


PLANTED_INVENTORY = ("""\
# Planted inventory

## generator.py generateRom

""" + PLANTED_COLUMNS + """\
| `rom = ROMWithTables(open(args.input_filename, 'rb'))` | - | input_filename | - | - | - |  | table | parse |
| `patches.planted.fixed(rom)` | - | - | - | - | - |  | table | fixed |
| `patches.planted.shuffle(rom)` | music | - | global | - | - |  | C | music |
| `patches.planted.fixed(rom)` #2 | music | - | - | - | - |  | table | music-off |
| `spot.patch(rom, spot.item)` | - | - | - | placement | - |  | C | items |

## randomizer.py Randomizer.__init__

""" + PLANTED_COLUMNS + """\
| `rom = generator.generateRom(args, settings, self.seed, self.__logic, rnd=self.rnd)` | * | * | rnd, seed | logic | - |  | none | generate |

## romTables.py ROMWithTables.__init__

""" + PLANTED_COLUMNS + """\
| `self.texts = Texts(self)` | - | - | - | - | - |  | table | parse |

## romTables.py ROMWithTables.save

""" + PLANTED_COLUMNS + """\
| `self.texts.store(self)` | - | - | - | - | - |  | C | save |

## rom.py ROM.save

""" + PLANTED_COLUMNS + """\
| `self.fixHeader(name=name)` | - | - | - | - | - |  | C | save |

## rom.py ROM.fixHeader

""" + PLANTED_COLUMNS + """\
| `self.banks[0][0x14D] = 0` | - | - | - | - | - |  | C | save |

""" + _planted_table("Groups", GROUP_COLUMNS, [
    "| parse | table | - | - | - |\n",
    "| fixed | table | - | - | - |\n",
    "| music | C | music | - | music |\n",
    "| music-off | table | music | - | - |\n",
    "| items | C | - | music | items |\n",
    "| generate | none | - | - | - |\n",
    "| save | C | - | items | save |\n"]) + "\n"
    + _planted_table("Read coverage", COVERAGE_COLUMNS, [
        "| items | `spot.patch(rom, spot.item)` | bank 0x00 | 0x0000..0x0001 | copy | moved |\n"])
    + "\n"
    + _planted_table("Blind spots", BLIND_COLUMNS, [
        "| patches/planted.py | `view = memoryview(rom.banks[0])` | not a ROM read |\n"]))

_SHUFFLE = "| `patches.planted.shuffle(rom)` | music | - | global | - | - |  | C | music |\n"
_FIXED_2 = "| `patches.planted.fixed(rom)` #2 | music | - | - | - | - |  | table | music-off |\n"
_SPOT = "| `spot.patch(rom, spot.item)` | - | - | - | placement | - |  | C | items |\n"
SELF_TEST_CASES = [
    ("matching", lambda t: t, []),
    ("dropped row", lambda t: t.replace(_SHUFFLE, ""), ["MISSING", "BAD"]),
    ("extra row", lambda t: t.replace(_SPOT, _SPOT + "| `gone(rom)` | - | - | - | - | - |  | C "
                                      "| items |\n"), ["STALE"]),
    ("wrong Settings", lambda t: t.replace("| music | - | global", "| witch | - | global"),
     ["MISMATCH"]),
    ("wrong RNG of a global-random callee", lambda t: t.replace("| global |", "| - |"),
     ["MISMATCH"]),
    ("swapped rows", lambda t: t.replace(_SHUFFLE + _FIXED_2, _FIXED_2 + _SHUFFLE),
     ["MISSING", "STALE"]),
    ("bad Side", lambda t: t.replace("|  | C | music |", "|  | D | music |"), ["BAD"]),
    ("row group not in Groups", lambda t: t.replace("| music-off | table | music | - | - |\n", ""),
     ["BAD"]),
    ("row side unlike its group", lambda t: t.replace("|  | C | music |", "|  | table | music |"),
     ["BAD"]),
    ("C group without evidence", lambda t: t.replace("| - | music |\n", "| - | - |\n"),
     ["BAD"]),
    ("extraction in Evidence", lambda t: t.replace("| music | items |\n",
                                                   "| music | items, extract:x |\n"),
     ["BAD"]),
    ("unknown coverage kind", lambda t: t.replace("| copy | moved |", "| guess | moved |"),
     ["BAD"]),
    ("missing blind spot", lambda t: t.replace("| not a ROM read |", "|  |"), ["BAD"]),
    ("dropped blind spot", lambda t: t.replace("| patches/planted.py |", "| planted.py |"),
     ["MISSING", "STALE"]),
]


def self_test(out=None):
    out = out or sys.stdout
    failures = []
    with tempfile.TemporaryDirectory(prefix="inventory-check-") as root:
        for rel, text in PLANTED_SOURCES.items():
            path = os.path.join(root, rel)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "wb") as f:
                newline = "\r\n" if rel == "generator.py" else "\n"
                f.write(text.replace("\n", newline).encode())
        for name, mutate, expected in SELF_TEST_CASES:
            try:
                found = sorted(line.split()[0] for line in check(root, mutate(PLANTED_INVENTORY)))
            except (SetupError, AssertionError) as e:
                found = ["error: %s" % e]
            if found != sorted(expected):
                failures.append("%s: %s, expected %s" % (name, " ".join(found) or "no finding",
                                                         " ".join(expected) or "no finding"))
    for failure in failures:
        print("inventory: self-test failed: " + failure, file=out)
    if failures:
        return 1
    print("inventory: self-test, %d planted inventories as expected" % len(SELF_TEST_CASES),
          file=out)
    return 0


def _commit(root):
    try:
        result = subprocess.run(["git", "-C", root, "rev-parse", "--short=8", "HEAD"],
                                stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                universal_newlines=True)
    except OSError:
        return "an unknown commit"
    return result.stdout.strip() if result.returncode == 0 else "an unknown commit"


def main(argv):
    parser = argparse.ArgumentParser(prog="inventory_check.py")
    parser.add_argument("--ladxr", default=DEFAULT_LADXR)
    parser.add_argument("--inventory", default=DEFAULT_INVENTORY)
    parser.add_argument("--print", action="store_true", dest="print_")
    try:
        args = parser.parse_args(argv)
    except SystemExit as e:
        return 2 if e.code else 0
    if self_test(sys.stderr if args.print_ else sys.stdout):
        return 1
    if not os.path.isfile(os.path.join(args.ladxr, "generator.py")):
        print("setup: inventory: no LADXR checkout in %s, run tests/ladx/oracle/setup.sh"
              % args.ladxr)
        return 2
    try:
        extracted = extract(args.ladxr)
    except SetupError as e:
        print("setup: inventory: %s" % e)
        return 2
    text = None
    if os.path.isfile(args.inventory):
        with open(args.inventory, "rb") as f:
            text = f.read().decode("utf-8")
    if args.print_:
        print(render(extracted, parse_inventory(text)[0] if text is not None else None), end="")
        return 0
    if text is None:
        print("setup: inventory: no inventory %s" % args.inventory)
        return 2
    try:
        findings = check(args.ladxr, text)
    except SetupError as e:
        print("setup: inventory: %s" % e)
        return 2
    for line in findings:
        print(line)
    if findings:
        return 1
    print("inventory: %d rows in %d sections match %s"
          % (sum(len(rows) for _, rows in extracted), len(extracted), _commit(args.ladxr)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
