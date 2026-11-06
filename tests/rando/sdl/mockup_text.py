"""Usage: python3 -I mockup_text.py compare SVG TEXT
       python3 -I mockup_text.py grid TEXT
       python3 -I mockup_text.py generating EVERY_OUTPUT
       python3 -I mockup_text.py --self-test

compare: one MISMATCH line per difference between a mockup's <text> elements and the grid text of
the app's screen (ui_cli or alttpr --shot-text), exit 1 if any.
grid: prints "<cols> <lines>" of a grid text, exit 1 if its lines are not all the same length.
generating: prints the last "@ generating" block of a ui_cli --every output, exit 1 if none.
--self-test: proves compare, grid and generating on planted pairs embedded here.

The rules, in one place:
- Mockup side: every <text> outside <defs>, zoned by its baseline y: tabs y < 60, title
  60 <= y < 90, body 90 <= y < 400, message 400 <= y < 422, footer y >= 422. A zone's line is its
  texts sorted by x, joined by one space. Body row 0 is the smallest body baseline, the pitch the
  smallest gap between distinct body baselines, row = (y - first) / pitch, a whole number; a row
  with no text is blank.
- App side: the border lines, the tag column and the two '|' are dropped; line 1 tabs, line 2
  title, lines 3 to page + 2 body, then message, then footer. Dropped as the mockups draw them as
  sprites or styles: '[' ']' around the active tab, '< ' ' >' around the selected value, a '^' or
  'v' in a body row's last column, the footer's '</>'. Body rows lose their pad column, their
  leading spaces are counted, then runs of spaces become one and every line is stripped.
- Tabs, title, message, footer: equal strings. Body: row k equal for every k up to the larger of
  the mockup's last row and the app's last non-blank row.
- Indent, on rows whose strings are equal: on a 12 px list body (every body text font-size="12")
  the row's leftmost start-anchored text is at base + 12 x leading spaces, base 60 with a
  <use href="#triforce"> in the body zone, else 30; any other body needs no leading space.
- A message-zone text with font-size="10" is the help line: left out, and the app's message must
  then be empty.
- Colours, scroll markers, the cursor row and the disabled state are not text: not compared.
"""

import re
import sys
import xml.etree.ElementTree as ET

SVG = "{http://www.w3.org/2000/svg}"
XLINK_HREF = "{http://www.w3.org/1999/xlink}href"
TABS, TITLE, BODY, MESSAGE, FOOTER = "tabs", "title", "body", "message", "footer"
LIST_FONT_SIZE = "12"
HELP_FONT_SIZE = "10"
CHAR_PX = 12
BASE_WITH_CURSOR, BASE_WITHOUT_CURSOR = 60, 30
GENERATING = "@ generating"


class InputError(Exception):
    pass


def zone_of(y):
    if y < 60:
        return TABS
    if y < 90:
        return TITLE
    if y < 400:
        return BODY
    if y < 422:
        return MESSAGE
    return FOOTER


def squeeze(text):
    return " ".join(text.split())


def number(element, name):
    value = element.get(name)
    if value is None:
        raise InputError("a <%s> has no %s" % (element.tag.replace(SVG, ""), name))
    try:
        return float(value)
    except ValueError:
        raise InputError("a <%s> has %s=%r" % (element.tag.replace(SVG, ""), name, value))


def outside_defs(root):
    """The elements of the tree, <defs> subtrees excluded."""
    stack = [root]
    while stack:
        element = stack.pop()
        if element.tag == SVG + "defs":
            continue
        yield element
        stack.extend(reversed(list(element)))


def translate_y(use):
    m = re.search(r"translate\(\s*([-\d.]+)[\s,]+([-\d.]+)\s*\)", use.get("transform", ""))
    if m:
        return float(m.group(2))
    return float(use.get("y", "0"))


class Mockup:
    def __init__(self, svg_text):
        try:
            root = ET.fromstring(svg_text)
        except ET.ParseError as e:
            raise InputError("not an SVG: %s" % e)
        self.texts = {zone: [] for zone in (TABS, TITLE, BODY, MESSAGE, FOOTER)}
        self.cursor = False
        for element in outside_defs(root):
            if element.tag == SVG + "text":
                y = number(element, "y")
                self.texts[zone_of(y)].append({
                    "x": number(element, "x"), "y": y,
                    "size": element.get("font-size"),
                    "anchor": element.get("text-anchor", "start"),
                    "text": squeeze("".join(element.itertext())),
                })
            elif element.tag == SVG + "use":
                href = element.get("href", element.get(XLINK_HREF))
                if href == "#triforce" and zone_of(translate_y(element)) == BODY:
                    self.cursor = True
        self.help = [t for t in self.texts[MESSAGE] if t["size"] == HELP_FONT_SIZE]
        self.texts[MESSAGE] = [t for t in self.texts[MESSAGE] if t["size"] != HELP_FONT_SIZE]
        if self.help and self.texts[MESSAGE]:
            raise InputError("a help line and another message text")
        self.rows = self._rows(self.texts[BODY])
        self.list_body = bool(self.texts[BODY]) and all(
            t["size"] == LIST_FONT_SIZE for t in self.texts[BODY])

    @staticmethod
    def _rows(texts):
        baselines = sorted({t["y"] for t in texts})
        if not baselines:
            return {}
        gaps = [b - a for a, b in zip(baselines, baselines[1:])]
        pitch = min(gaps) if gaps else 1.0
        rows = {}
        for t in texts:
            index = (t["y"] - baselines[0]) / pitch
            if abs(index - round(index)) > 1e-9:
                raise InputError("body baseline %g is not on the row pitch %g" % (t["y"], pitch))
            rows.setdefault(int(round(index)), []).append(t)
        return rows

    def line(self, zone):
        return join(self.texts[zone])

    def row(self, k):
        return join(self.rows.get(k, []))

    def last_row(self):
        return max(self.rows) if self.rows else -1

    def row_start(self, k):
        starts = [t["x"] for t in self.rows.get(k, []) if t["anchor"] == "start"]
        return min(starts) if starts else None


def join(texts):
    return " ".join(t["text"] for t in sorted(texts, key=lambda t: t["x"]) if t["text"])


def grid_lines(grid_text):
    lines = grid_text.split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    if len(lines) < 2 or len({len(line) for line in lines}) != 1:
        raise InputError("not a grid: lines of different lengths")
    return lines


def grid_size(grid_text):
    lines = grid_lines(grid_text)
    return len(lines[0]) - 3, len(lines) - 2


class AppScreen:
    def __init__(self, grid_text):
        lines = grid_lines(grid_text)[1:-1]
        if len(lines) < 5:
            raise InputError("not a grid: fewer than 5 lines inside the border")
        for line in lines:
            if line[1:2] != "|" or line[-1:] != "|":
                raise InputError("not a grid: a line without its two '|'")
        cells = [line[2:-1] for line in lines]
        page = len(cells) - 4
        self.tabs = squeeze(re.sub(r"\[([^\]]*)\]", r"\1", cells[0]))
        self.title = squeeze(cells[1][1:])
        self.rows = [self._body_row(c) for c in cells[2:2 + page]]
        self.message = squeeze(cells[2 + page][1:])
        self.footer = squeeze(" ".join(w for w in cells[3 + page].split() if w != "</>"))

    @staticmethod
    def _body_row(cell):
        if cell[-1:] in ("^", "v"):
            cell = cell[:-1] + " "
        field = cell[1:]
        m = re.match(r"^(.*)< (.*) >\s*$", field)
        if m:
            field = m.group(1) + m.group(2)
        return len(field) - len(field.lstrip(" ")), squeeze(field)

    def row(self, k):
        return self.rows[k][1] if k < len(self.rows) else ""

    def leading(self, k):
        return self.rows[k][0] if k < len(self.rows) else 0

    def last_row(self):
        filled = [k for k, (_, text) in enumerate(self.rows) if text]
        return max(filled) if filled else -1


def compare(name, svg_text, grid_text):
    """The MISMATCH lines of a mockup against an app grid, in screen order."""
    mockup, app = Mockup(svg_text), AppScreen(grid_text)
    out = []

    def check(where, want, got):
        if want != got:
            out.append('MISMATCH %s: %s: mockup "%s" app "%s"' % (name, where, want, got))

    check(TABS, mockup.line(TABS), app.tabs)
    check(TITLE, mockup.line(TITLE), app.title)
    base = BASE_WITH_CURSOR if mockup.cursor else BASE_WITHOUT_CURSOR
    for k in range(max(mockup.last_row(), app.last_row()) + 1):
        want, got = mockup.row(k), app.row(k)
        check("body row %d" % k, want, got)
        start = mockup.row_start(k)
        if want != got or start is None:
            continue
        if mockup.list_body:
            check("indent row %d" % k, "x %g" % start, "x %g" % (base + CHAR_PX * app.leading(k)))
        elif app.leading(k):
            check("indent row %d" % k, "no indent", "%d leading spaces" % app.leading(k))
    check(MESSAGE, mockup.line(MESSAGE), app.message)
    check(FOOTER, mockup.line(FOOTER), app.footer)
    return out


def last_generating(every_text):
    """The lines of the last "@ generating" block, each with its newline."""
    block, current = None, None
    for line in every_text.splitlines(True):
        if line.startswith("@ "):
            current = [] if line.rstrip("\n") == GENERATING else None
            if current is not None:
                block = current
        elif current is not None:
            current.append(line)
    if block is None:
        raise InputError("no %s block" % GENERATING)
    return block


COLS = 30
DEFS = ('<defs><g id="triforce"><polygon points="8,0 12,6 4,6"/></g>'
        '<text x="0" y="200">Inside defs</text></defs>')


def svg(body, message='<text x="30" y="412" font-size="12">Not supported</text>',
        cursor='<use href="#triforce" transform="translate(32 103)"/>'):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="640" height="480">'
            '<title>Planted</title>' + DEFS +
            '<text x="30" y="42" font-size="16">Generate</text>'
            '<use href="#triforce" transform="translate(186 27)"/>'
            '<text x="214" y="42" font-size="16">History</text>'
            '<text x="30" y="82" font-size="14">Seed Settings</text>' + cursor + body + message +
            '<text x="41" y="449" font-size="12" text-anchor="middle">Y</text>'
            '<text x="62" y="448" font-size="12">Default</text>'
            '<text x="157" y="449" font-size="12" text-anchor="middle">B</text>'
            '<text x="178" y="448" font-size="12">Back</text>'
            '</svg>')


def body_text(x, y, text, size="12", anchor=None):
    a = ' text-anchor="%s"' % anchor if anchor else ""
    return '<text x="%s" y="%s" font-size="%s"%s>%s</text>' % (x, y, size, a, text)


LIST_BODY = (body_text(60, 114, "Swords") + body_text(584, 114, "Swordless", anchor="end") +
             body_text(60, 138, "Hints") + body_text(584, 138, "On", anchor="end") +
             body_text(84, 186, "Ganon: Ganon"))


def grid(rows, message=" Not supported", footer=" </> Y Default  B Back", tabs="[Generate]  History"):
    def line(tag, cell, marker=" "):
        assert len(cell) <= COLS - 1, cell
        return tag + "|" + cell.ljust(COLS - 1) + marker + "|"

    border = " +" + "-" * COLS + "+"
    lines = [border, line("=", tabs), line("#", " Seed Settings")]
    lines += [line(tag, cell, marker) for tag, cell, marker in rows]
    lines += [line("E", message), line("_", footer), border]
    return "\n".join(lines) + "\n"


LIST_ROWS = [(">", " Swords  < Swordless >", "^"), (" ", " Hints              On", " "),
             (" ", "", " "), (" ", "   Ganon: Ganon", "v"), (" ", "", " ")]


def with_row(rows, k, row):
    return rows[:k] + [row] + rows[k + 1:]


HELP = '<text x="34" y="418" font-size="10">Press A to pick</text>'
NOTICE_BODY = body_text(30, 120, "Saved as:", "14") + body_text(50, 152, "Open.sfc", "14")
NOTICE_ROWS = [(" ", " Saved as:", " "), (" ", " Open.sfc", " "), (" ", "", " ")]
CURSORLESS_BODY = (body_text(30, 114, "Bosses") + body_text(54, 138, "Ganon: Ganon"))
CURSORLESS_ROWS = [(" ", " Bosses", "^"), (" ", "   Ganon: Ganon", " "), (" ", "", "v")]

COMPARE_CASES = [
    ("clean pair", svg(LIST_BODY), grid(LIST_ROWS), []),
    ("word changed in a body row",
     svg(LIST_BODY.replace(">On<", ">Off<")), grid(LIST_ROWS),
     ['MISMATCH self: body row 1: mockup "Hints Off" app "Hints On"']),
    ("row moved one pitch down",
     svg(LIST_BODY.replace('y="186"', 'y="210"')), grid(LIST_ROWS),
     ['MISMATCH self: body row 3: mockup "" app "Ganon: Ganon"',
      'MISMATCH self: body row 4: mockup "Ganon: Ganon" app ""']),
    ("indent off by one column",
     svg(LIST_BODY.replace('x="84"', 'x="72"')), grid(LIST_ROWS),
     ['MISMATCH self: indent row 3: mockup "x 72" app "x 84"']),
    ("app row the mockup lacks",
     svg(LIST_BODY), grid(with_row(LIST_ROWS, 4, (" ", " Heart Beep    Normal", " "))),
     ['MISMATCH self: body row 4: mockup "" app "Heart Beep Normal"']),
    ("changed footer label",
     svg(LIST_BODY), grid(LIST_ROWS, footer=" </> Y Default  B Cancel"),
     ['MISMATCH self: footer: mockup "Y Default B Back" app "Y Default B Cancel"']),
    ("help line, empty app message",
     svg(LIST_BODY, message=HELP), grid(LIST_ROWS, message=""), []),
    ("help line, app message",
     svg(LIST_BODY, message=HELP), grid(LIST_ROWS),
     ['MISMATCH self: message: mockup "" app "Not supported"']),
    ("notice body, clean", svg(NOTICE_BODY, cursor=""), grid(NOTICE_ROWS), []),
    ("notice body, app row indented",
     svg(NOTICE_BODY, cursor=""), grid(with_row(NOTICE_ROWS, 1, (" ", "   Open.sfc", " "))),
     ['MISMATCH self: indent row 1: mockup "no indent" app "2 leading spaces"']),
    ("cursorless list, clean", svg(CURSORLESS_BODY, cursor=""), grid(CURSORLESS_ROWS), []),
]


def self_test():
    failures = []

    def expect(case, want, got):
        if want != got:
            failures.append("mockup_text: self-test: %s: want %r, got %r" % (case, want, got))

    for case, svg_text, grid_text, want in COMPARE_CASES:
        expect(case, want, compare("self", svg_text, grid_text))
    expect("grid size", (COLS, 9), grid_size(grid(LIST_ROWS)))
    for case, text in (("grid of unequal lines", grid(LIST_ROWS) + "short\n"),):
        try:
            grid_size(text)
            failures.append("mockup_text: self-test: %s: accepted" % case)
        except InputError:
            pass
    every = ("@ init\nA\n" + GENERATING + "\nB1\nB2\n@ start\nC\n" + GENERATING + "\nD\n@ a\nE\n")
    expect("last generating block", ["D\n"], last_generating(every))
    try:
        last_generating("@ init\nA\n")
        failures.append("mockup_text: self-test: no generating block: accepted")
    except InputError:
        pass
    for line in failures:
        print(line)
    if failures:
        return 1
    print("mockup_text: self-test: %d planted pairs give their lines" % len(COMPARE_CASES))
    return 0


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def main(argv):
    try:
        if argv == ["--self-test"]:
            return self_test()
        if len(argv) == 3 and argv[0] == "compare":
            name = re.sub(r"\.svg$", "", argv[1].replace("\\", "/").split("/")[-1])
            lines = compare(name, read(argv[1]), read(argv[2]))
            for line in lines:
                print(line)
            return 1 if lines else 0
        if len(argv) == 2 and argv[0] == "grid":
            print("%d %d" % grid_size(read(argv[1])))
            return 0
        if len(argv) == 2 and argv[0] == "generating":
            sys.stdout.write("".join(last_generating(read(argv[1]))))
            return 0
    except (InputError, OSError) as e:
        print("mockup_text: %s" % e)
        return 1
    sys.stdout.write(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
