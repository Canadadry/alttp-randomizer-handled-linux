"""Usage: python3 -I no_literals.py [--no-table] DIR

Fails, naming file and line, when a .c or .h file under DIR holds a string literal with an ASCII
letter once its escapes are decoded and its printf conversions removed: text shown to the user must
come from the string table. --no-table is for a directory without the table (app/sdl): nothing is
exempt there. Proves itself first on planted sources.
"""

import os
import re
import sys
import tempfile

# Relative to DIR. A translated table is added here, deliberately.
EXEMPT = ("ui_strings_en.c",)

SKIPPED_DIRECTIVES = ("include", "pragma", "line", "error")
PREFIXES = ("L", "u", "U", "u8")
SIMPLE_ESCAPES = {"n": "\n", "t": "\t", "r": "\r", "a": "\a", "b": "\b", "f": "\f", "v": "\v",
                  "\\": "\\", "'": "'", '"': '"', "?": "?"}
CONVERSION = re.compile(r"%(%|[-+ #0]*(\*|\d+)?(\.(\*|\d+)?)?(hh|h|ll|l|j|z|t|L)?[diouxXfFeEgGaAcspn])")
LETTER = re.compile(r"[A-Za-z]")


class ScanError(Exception):
    pass


def splice(text):
    """The characters with backslash-newlines removed, each with its 1-based source line."""
    chars, lines = [], []
    line, i = 1, 0
    while i < len(text):
        if text[i] == "\\" and text[i + 1:i + 2] == "\n":
            line, i = line + 1, i + 2
            continue
        if text[i] == "\\" and text[i + 1:i + 3] == "\r\n":
            line, i = line + 1, i + 3
            continue
        chars.append(text[i])
        lines.append(line)
        if text[i] == "\n":
            line += 1
        i += 1
    return "".join(chars), lines


def skip_quoted(s, i, quote):
    """Index just past the closing quote of the quoted run starting at s[i] == quote."""
    i += 1
    while i < len(s) and s[i] != quote and s[i] != "\n":
        i += 2 if s[i] == "\\" else 1
    return i + 1


def tokens(text):
    """Yields ('str', line, raw, body) for each string literal and ('other',) between them."""
    s, lines = splice(text)
    i, bol = 0, True
    while i < len(s):
        c = s[i]
        if c == "\n":
            bol = True
            i += 1
        elif c in " \t\r\f\v":
            i += 1
        elif s.startswith("/*", i):
            end = s.find("*/", i + 2)
            i = len(s) if end < 0 else end + 2
        elif s.startswith("//", i):
            end = s.find("\n", i)
            i = len(s) if end < 0 else end
        elif c == "#" and bol:
            m = re.compile(r"#[ \t]*([A-Za-z_]*)").match(s, i)
            bol = False
            yield ("other",)
            if m.group(1) in SKIPPED_DIRECTIVES:
                end = s.find("\n", i)
                i = len(s) if end < 0 else end
            else:
                i = m.end()
        elif c.isalnum() or c == "_":
            j = i
            while j < len(s) and (s[j].isalnum() or s[j] == "_"):
                j += 1
            word, bol = s[i:j], False
            if word in PREFIXES and j < len(s) and s[j] == '"':
                end = skip_quoted(s, j, '"')
                yield ("str", lines[i], s[i:end], s[j + 1:end - 1])
                i = end
            elif word in PREFIXES[:3] and j < len(s) and s[j] == "'":
                i = skip_quoted(s, j, "'")
                yield ("other",)
            else:
                i = j
                yield ("other",)
        elif c == '"':
            end = skip_quoted(s, i, '"')
            yield ("str", lines[i], s[i:end], s[i + 1:end - 1])
            i, bol = end, False
        elif c == "'":
            i, bol = skip_quoted(s, i, "'"), False
            yield ("other",)
        else:
            i, bol = i + 1, False
            yield ("other",)


def literals(text):
    """Each literal as (line, raw pieces joined by a space, decoded body), adjacent pieces joined."""
    group = []
    for tok in list(tokens(text)) + [("other",)]:
        if tok[0] == "str":
            group.append(tok)
        elif group:
            yield group[0][1], " ".join(t[2] for t in group), "".join(decode(t[3]) for t in group)
            group = []


def decode(body):
    out, i = [], 0
    while i < len(body):
        if body[i] != "\\" or i + 1 == len(body):
            out.append(body[i])
            i += 1
            continue
        e = body[i + 1]
        if e in SIMPLE_ESCAPES:
            out.append(SIMPLE_ESCAPES[e])
            i += 2
        elif e in "01234567":
            m = re.compile(r"[0-7]{1,3}").match(body, i + 1)
            out.append(chr(int(m.group(0), 8)))
            i = m.end()
        elif e in "xuU":
            m = re.compile(r"[0-9A-Fa-f]*").match(body, i + 2)
            out.append(chr(int(m.group(0), 16) % 0x110000) if m.group(0) else "")
            i = m.end()
        else:
            out.append(e)
            i += 2
    return "".join(out)


def shows_text(decoded):
    return LETTER.search(CONVERSION.sub("", decoded)) is not None


def scan(root, exempt):
    """The findings under root, as 'file:line: ...' strings. Raises ScanError when root is missing,
    holds no source, or an exempt path does not exist."""
    if not os.path.isdir(root):
        raise ScanError("%s: no such directory" % root)
    files = []
    for d, _, names in os.walk(root):
        files += [os.path.join(d, n) for n in names if n.endswith((".c", ".h"))]
    if not files:
        raise ScanError("%s: no .c or .h file to scan" % root)
    rel = {os.path.relpath(f, root).replace(os.sep, "/"): f for f in files}
    for e in exempt:
        if e not in rel:
            raise ScanError("%s: exempt file %s does not exist" % (root, e))
    findings = []
    for r in sorted(rel):
        if r in exempt:
            continue
        with open(rel[r], encoding="latin-1") as f:
            text = f.read()
        for line, raw, decoded in literals(text):
            if shows_text(decoded):
                findings.append("%s:%d: user-visible string literal: %s" % (rel[r], line, raw))
    return findings


CAUGHT = [
    ("a plain word", 'const char *s = "Generate";\n'),
    ("adjacent pieces", 'const char *s = "Gen" /* x */ "erate";\n'),
    ("a wide literal", 'const wchar_t *s = L"Generate";\n'),
    ("a hex escape", 'const char *s = "\\x41";\n'),
    ("a word after a conversion", 'const char *s = "%d items";\n'),
    ("a #define body", '#define TITLE "Generate"\n'),
    ("a spliced #define", '#define TITLE \\\n\t"Generate"\n'),
    ("after a comment holding a quote", '/* say "hi */ const char *s = "Generate";\n'),
    ("after a line comment holding a quote", '// a " quote\nconst char *s = "Generate";\n'),
    ("the English table under another name", '[UI_STR_TAB_GENERATE] = "Generate",\n'),
]

PASSING = (
    '/* "Generate" */\n'
    '// "History"\n'
    '// a spliced comment \\\n "Generate"\n'
    '#include "ui_screen.h"\n'
    '#include <string.h>\n'
    "char a = '\"', b = 'v', c = '[', d = '\\'', e = L'x';\n"
    'const char *s[] = { "", "\\n", "%d", "%-20s", "%%", "%s / %s", u8"%c", "\\x0A\\x20" };\n'
)


def self_proof():
    with tempfile.TemporaryDirectory() as tmp:
        cases = [(name, {"core.c": src}, (), "caught") for name, src in CAUGHT]
        cases += [("literals with no word", {"core.c": PASSING}, (), "pass"),
                  ("the exempt file", {"core.c": PASSING, EXEMPT[0]: CAUGHT[0][1]}, EXEMPT, "pass"),
                  ("a missing exempt file", {"core.c": PASSING}, EXEMPT, "error"),
                  ("an empty directory", {}, (), "error")]
        for n, (name, files, exempt, expect) in enumerate(cases):
            root = os.path.join(tmp, str(n))
            os.mkdir(root)
            for fname, src in files.items():
                with open(os.path.join(root, fname), "w") as f:
                    f.write(src)
            try:
                got = "caught" if scan(root, exempt) else "pass"
            except ScanError:
                got = "error"
            if got != expect:
                sys.exit("literal-check self-proof failed: %s" % name)
        if not os.path.isdir(os.path.join(tmp, "absent")):
            try:
                scan(os.path.join(tmp, "absent"), ())
                sys.exit("literal-check self-proof failed: a missing directory")
            except ScanError:
                pass


def main(argv):
    args = argv[1:]
    exempt = EXEMPT
    if args[:1] == ["--no-table"]:
        args, exempt = args[1:], ()
    if len(args) != 1:
        sys.exit("usage: no_literals.py [--no-table] DIR")
    self_proof()
    try:
        findings = scan(args[0], exempt)
    except ScanError as e:
        sys.exit("literal check: %s" % e)
    if findings:
        print("\n".join(findings), file=sys.stderr)
        sys.exit("literal check: text shown to the user must come from the string table (%s)" % EXEMPT[0])


if __name__ == "__main__":
    main(sys.argv)
