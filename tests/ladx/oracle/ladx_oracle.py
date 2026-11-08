"""Shared helpers of the LADX oracle: the RNG tracer, the fixtures' line encoders, the double run's
comparator and the order audit.

Imports only the standard library and stays importable on Python older than 3.11, so that
make check runs everywhere; the oracle scripts themselves need 3.11 (co_qualname).
"""

import contextlib
import io
import os
import random
import re
import sys
import tempfile

_THIS = os.path.abspath(__file__)


class OracleError(Exception):
    pass


COVERED = ("seed", "random", "getrandbits", "randrange", "randint", "choice", "choices", "shuffle",
           "uniform", "getstate", "setstate")


def render(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise OracleError("oracle: cannot render %r" % (value,))
    if isinstance(value, float):
        return value.hex()
    return str(value)


def render_list(values):
    return ",".join(render(v) for v in values) if values else "-"


def seed_hex(seed):
    if not isinstance(seed, (bytes, bytearray)):
        raise OracleError("oracle: seed %r is not bytes" % (seed,))
    return seed.hex() or "-"


def field(text):
    if any(c in text for c in "\t\n\r\""):
        raise OracleError("oracle: field %r holds a tab, a newline or a quote" % (text,))
    return text or '""'


def item_name(name):
    if not re.fullmatch(r"[A-Z0-9_]+", name):
        raise OracleError("oracle: item name %r is not [A-Z0-9_]+" % (name,))
    return name


def oracle_env(hashseed, base=None):
    env = dict(os.environ if base is None else base)
    env["PYTHONHASHSEED"] = str(hashseed)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    return env


# A new empty directory under work_root, so LADXR's stray files never land in the repository.
def enter_checkout(ladxr_dir, work_root):
    ladxr_dir = os.path.abspath(ladxr_dir)
    if not os.path.isfile(os.path.join(ladxr_dir, "randomizer.py")):
        raise OracleError("oracle: %s is not a LADXR checkout" % ladxr_dir)
    sys.dont_write_bytecode = True
    os.makedirs(work_root, exist_ok=True)
    work = tempfile.mkdtemp(dir=work_root)
    sys.path.insert(0, ladxr_dir)
    os.chdir(work)
    return work


class TracingRandom(random.Random):
    tracer = None
    gen = "rnd"
    _depth = 0
    _words = 0

    def _call(self, name, body):
        start = self._words
        self._depth += 1
        try:
            value, args, result = body()
        finally:
            self._depth -= 1
        if self._depth == 0 and self.tracer is not None:
            self.tracer.record(self.gen, name, args, result, self._words - start)
        return value

    def seed(self, a=None, version=2):
        if version != 2:
            raise OracleError("oracle: seed version %r is not covered" % (version,))
        if a is None:
            shown = "none"
        elif isinstance(a, int):
            shown = render(a)
        else:
            shown = seed_hex(a)
        return self._call("seed", lambda: (super(TracingRandom, self).seed(a, version), shown, "-"))

    # Trap: overriding `random` without `getrandbits` makes CPython's __init_subclass__ pick
    # _randbelow_without_getrandbits, silently changing randrange, choice and shuffle.
    def random(self):
        def body():
            self._words += 2
            value = super(TracingRandom, self).random()
            return value, "-", render(value)
        return self._call("random", body)

    def getrandbits(self, k):
        def body():
            self._words += (k + 31) // 32
            value = super(TracingRandom, self).getrandbits(k)
            return value, render(k), render(value)
        return self._call("getrandbits", body)

    def randrange(self, start, stop=None, step=1):
        args = [start] + ([] if stop is None else [stop]) + ([] if step == 1 else [step])

        def body():
            value = super(TracingRandom, self).randrange(start, stop, step)
            return value, " ".join(map(render, args)), render(value)
        return self._call("randrange", body)

    def randint(self, a, b):
        def body():
            value = super(TracingRandom, self).randint(a, b)
            return value, "%s %s" % (render(a), render(b)), render(value)
        return self._call("randint", body)

    def choice(self, seq):
        def body():
            index = super(TracingRandom, self).choice(range(len(seq)))
            return seq[index], render(len(seq)), render(index)
        return self._call("choice", body)

    def choices(self, population, weights=None, *, cum_weights=None, k=1):
        if cum_weights is not None:
            raise OracleError("oracle: choices with cum_weights is not covered")

        def body():
            n = len(population)
            picks = super(TracingRandom, self).choices(range(n), weights, k=k)
            shown_weights = "-" if weights is None else ",".join(map(render, weights))
            return ([population[i] for i in picks], "%d %s %s" % (n, shown_weights, render(k)),
                    render_list(picks))
        return self._call("choices", body)

    def shuffle(self, x):
        def body():
            perm = list(range(len(x)))
            super(TracingRandom, self).shuffle(perm)
            before = list(x)
            for to, frm in enumerate(perm):
                x[to] = before[frm]
            return None, render(len(x)), render_list(perm)
        return self._call("shuffle", body)

    def uniform(self, a, b):
        def body():
            value = super(TracingRandom, self).uniform(a, b)
            return value, "%s %s" % (render(a), render(b)), render(value)
        return self._call("uniform", body)

    def getstate(self):
        return self._call("getstate", lambda: (super(TracingRandom, self).getstate(), "-", "-"))

    def setstate(self, state):
        return self._call("setstate", lambda: (super(TracingRandom, self).setstate(state), "-", "-"))


class Tracer:
    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.lines = []
        self.Random = type("TracingRandom", (TracingRandom,), {"tracer": self})

    @contextlib.contextmanager
    def installed(self):
        glob = self.Random.__new__(self.Random)
        glob.gauss_next = None
        glob.gen = "global"
        saved = {"Random": random.Random}
        for name in dir(random._inst):
            if not name.startswith("_") and callable(getattr(random, name, None)) and name != "Random":
                saved[name] = getattr(random, name)
        try:
            random.Random = self.Random
            for name in saved:
                if name != "Random":
                    setattr(random, name, getattr(glob, name))
            yield glob
        finally:
            for name, value in saved.items():
                setattr(random, name, value)

    def stage(self):
        frame = sys._getframe(1)
        while frame is not None:
            path = os.path.abspath(frame.f_code.co_filename)
            if path != _THIS and path.startswith(self.root + os.sep):
                module = os.path.splitext(os.path.relpath(path, self.root))[0].replace(os.sep, ".")
                code = frame.f_code
                return "%s:%s" % (module, getattr(code, "co_qualname", code.co_name))
            frame = frame.f_back
        return "-"

    def record(self, gen, method, args, result, words):
        self.lines.append("%d %s %s %s %s -> %s [%d]" % (len(self.lines), gen, self.stage(), method,
                                                        args, result, words))


def _uncovered(name):
    def method(self, *args, **kwargs):
        raise OracleError("oracle: RNG method %s is not covered by pyrandom_vectors.txt" % name)
    method.__name__ = name
    return method


for _name in dir(random.Random):
    if not _name.startswith("_") and _name not in COVERED and callable(getattr(random.Random, _name)):
        setattr(TracingRandom, _name, _uncovered(_name))


def setting_kind(default):
    for kind in (bool, str, float):
        if isinstance(default, kind):
            return kind.__name__
    raise OracleError("oracle: setting default %r is not a bool, a str or a float" % (default,))


def setting_value(value):
    kind = setting_kind(value)
    if kind == "bool":
        return "1" if value else "0"
    if kind == "float":
        return render(value)
    if " " in value or "," in value or "=" in value:
        raise OracleError("oracle: setting value %r holds a space, a comma or an =" % (value,))
    return field(value)


def settings_lines(settings):
    for s in settings:
        visible = s.visible_if or []
        if any("," in v for v in visible):
            raise OracleError("oracle: visible_if %r holds a comma" % (visible,))
        yield "setting %s\t%s\t%s\t%s\t%s\t%s\t%d\t%s" % (
            field(s.key), field(s.category), field(s.short_key), setting_kind(s.default),
            setting_value(s.default), field(s.label), 1 if s.aesthetic else 0,
            ",".join(field(v) for v in visible) or "-")
        for key, short, label in s.options or []:
            label, bar, meta = label.partition("|")
            yield "option %s\t%s\t%s\t%s\t%s" % (field(s.key), setting_value(key), field(short),
                                                   field(label), field(meta) if bar else "-")


def setting_values(s):
    if s.options:
        return [setting_value(key) for key, _, _ in s.options]
    if setting_kind(s.default) == "bool":
        return ["0", "1"]
    return [setting_value(s.default)]


def read_tier1(text, settings):
    order = [s.key for s in settings]
    by_key = {s.key: s for s in settings}
    tier1 = []
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        key, *values = line.split(" ")
        s = by_key.get(key)
        if s is None or not values:
            raise OracleError("oracle: tier1 line %r: no such setting, or no value" % line)
        if not s.options and setting_kind(s.default) != "bool":
            raise OracleError("oracle: tier1 setting %s has free values" % key)
        known = setting_values(s)
        if values == ["all"]:
            values = known
        if len(set(values)) != len(values) or any(v not in known for v in values):
            raise OracleError("oracle: tier1 line %r: a value repeated or not one of %s"
                              % (line, " ".join(known)))
        if tier1 and order.index(key) <= order.index(tier1[-1][0]):
            raise OracleError("oracle: tier1 setting %s repeated or out of LADXR's order" % key)
        tier1.append((key, values))
    return tier1


def _python_values(s):
    if s.options:
        return [key for key, _, _ in s.options]
    if setting_kind(s.default) == "bool":
        return [False, True]
    return [s.default]


def validate_fixture(settings_class):
    touched = set()

    class Recording(settings_class):
        def __getattr__(self, item):
            if not item.startswith("_"):
                touched.add(item)
            return settings_class.__getattr__(self, item)

        def __setattr__(self, key, value):
            if not key.startswith("_"):
                touched.add(key)
            settings_class.__setattr__(self, key, value)

    def snapshot(settings):
        return [(s.key, s.value) for s in settings]

    def run(inputs):
        settings = Recording()
        for key, value in inputs:
            setattr(settings, key, value)
        before = snapshot(settings)
        touched.clear()
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        reached = set(touched)
        changed = ["%s=%s" % (key, setting_value(value))
                   for (key, value), (_, old) in zip(snapshot(settings), before) if value != old]
        line = "validate %s -> %s\t%s" % (
            ",".join("%s=%s" % (key, setting_value(value)) for key, value in inputs),
            ",".join(changed) or "=", field(settings.getShortString()))
        return line, reached

    values = [(s.key, _python_values(s)) for s in settings_class()]
    union = set()
    for key, vals in values:
        for value in vals:
            union |= run([(key, value)])[1]
    values = [(key, vals) for key, vals in values if key in union]
    keys = [key for key, _ in values]
    lines = [run([(key, value)])[0] for key, vals in values for value in vals]
    for i, (key_a, vals_a) in enumerate(values):
        for key_b, vals_b in values[i + 1:]:
            for a in vals_a:
                for b in vals_b:
                    line, reached = run([(key_a, a), (key_b, b)])
                    if not reached <= union:
                        raise OracleError("oracle: validate with %s=%r, %s=%r touches %s, outside %s"
                                          % (key_a, a, key_b, b, " ".join(sorted(reached - union)),
                                             " ".join(keys)))
                    lines.append(line)
    return keys, lines


AUDIT_SITE = re.compile(r"os\.listdir\(|os\.scandir\(|os\.walk\(|\bglob\b|\.iterdir\("
                        r"|(?<![.\w])(?<!def )(?:frozen)?set\(|\bfor\s+[\w\s,()]+?\s+in\s*\{"
                        r"|getAccessableLocations\(")
AUDIT_ENV_SITE = re.compile(r"\b(?:subprocess|datetime|time|platform|uuid|socket|socketserver)\."
                            r"|\bos\.(?:environ|getenv|urandom|getcwd)\b|\bsys\.(?:version|platform)"
                            r"|^\s*from\s+(?:subprocess|datetime|time|platform|uuid|socket|socketserver)\s+import\b")
AUDIT_VERDICTS = ("patched", "unreached", "order-free", "inert")


def audit_sites(root):
    sites = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not (dirpath == root and d in ("www", ".git")))
        for name in sorted(filenames):
            if name.endswith(".py"):
                path = os.path.join(dirpath, name)
                rel = os.path.relpath(path, root).replace(os.sep, "/")
                with open(path, encoding="utf-8") as f:
                    for n, line in enumerate(f, 1):
                        if AUDIT_SITE.search(line) or AUDIT_ENV_SITE.search(line):
                            sites.append((rel, n, line.strip()))
    return sorted(sites)


def read_audit(text):
    table = {}
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4 or fields[2] not in AUDIT_VERDICTS or not fields[3].strip():
            raise OracleError("oracle: bad audit line %r: want <file>\\t<text>\\t<%s>\\t<reason>"
                              % (line, "|".join(AUDIT_VERDICTS)))
        key = (fields[0], fields[1])
        if key in table:
            raise OracleError("oracle: audit entry %s %s given twice" % key)
        table[key] = (fields[2], fields[3])
    return table


def audit_order(root, table_text):
    table = read_audit(table_text)
    sites = audit_sites(root)
    keys = {(rel, text) for rel, _, text in sites}
    lines = ["UNAUDITED %s:%d %s" % site for site in sites if (site[0], site[2]) not in table]
    lines += ["STALE %s %s" % key for key in sorted(table) if key not in keys]
    if lines:
        return False, lines
    return True, ["order audit: %d sites, all audited" % len(sites)]


def requirement_text(req):
    if req is None:
        return "-"
    if isinstance(req, bool):
        return "T" if req else "F"
    if isinstance(req, str):
        return item_name(req)
    kind = type(req).__name__

    def attr(name):
        return getattr(req, "_%s__%s" % (kind, name))
    if kind in ("AND", "OR"):
        parts = [item_name(i) for i in attr("items")] + [requirement_text(c) for c in attr("children")]
        return "%s(%s)" % (kind.lower(), ",".join(parts))
    if kind in ("COUNT", "FOUND"):
        return "%s(%s,%s)" % (kind.lower(), item_name(attr("item")), render(attr("amount")))
    if kind == "COUNTS":
        return "counts(%s)" % ",".join([render(attr("amount"))] + [item_name(i) for i in attr("items")])
    raise OracleError("oracle: requirement %r of type %s is not covered" % (req, kind))


def graph_lines(logic):
    index = {id(loc): i for i, loc in enumerate(logic.location_list)}

    def where(loc):
        if id(loc) not in index:
            raise OracleError("oracle: location %r is not in location_list" % (loc,))
        return index[id(loc)]
    for i, loc in enumerate(logic.location_list):
        if loc.name == "-":
            raise OracleError("oracle: a location is named -")
        yield "loc %d %s\t%s" % (i, "-" if loc.dungeon is None else render(loc.dungeon),
                                 "-" if loc.name is None else field(loc.name))
    for spot in logic.iteminfo_list:
        yield "spot %d %s %s %s\t%s\t%s" % (
            where(spot._location), type(spot).__name__, field(spot.nameId),
            ",".join(item_name(o) for o in spot.getOptions()) or "-",
            field(spot.metadata.name), field(spot.metadata.area))
    for i, loc in enumerate(logic.location_list):
        for target, req in loc.connections:
            yield "edge %d %d %s" % (i, where(target), requirement_text(req))


def index_ranges(indexes):
    parts = []
    for i in sorted(indexes):
        if parts and parts[-1][1] == i - 1:
            parts[-1][1] = i
        else:
            parts.append([i, i])
    return ",".join(str(a) if a == b else "%d-%d" % (a, b) for a, b in parts) or "-"


def compare_runs(name, a_text, b_text):
    a_lines, b_lines = a_text.splitlines(), b_text.splitlines()
    for n in range(max(len(a_lines), len(b_lines))):
        a = a_lines[n] if n < len(a_lines) else "<end of file>"
        b = b_lines[n] if n < len(b_lines) else "<end of file>"
        if a != b:
            return "DIFFERENT %s: line %d: %s | %s" % (name, n + 1, a, b)
    return "same %s" % name


# Never a byte value: the files may be ROM-derived.
def compare_bytes(name, label, a, b):
    if a == b:
        return "same %s" % name
    if len(a) != len(b):
        return "DIFFERENT %s: %s: sizes %d vs %d" % (name, label, len(a), len(b))
    offsets = [i for i, (x, y) in enumerate(zip(a, b)) if x != y]
    return "DIFFERENT %s: %s: %d bytes differ, first at 0x%x" % (name, label, len(offsets), offsets[0])


def _read_bytes(path):
    with open(path, "rb") as f:
        return f.read()


def compare_dirs(name, a_dir, b_dir):
    a_files, b_files = (set(os.listdir(d)) - {"meta.txt"} for d in (a_dir, b_dir))
    lines = ["DIFFERENT %s: only in A: %s" % (name, f) for f in sorted(a_files - b_files)]
    lines += ["DIFFERENT %s: only in B: %s" % (name, f) for f in sorted(b_files - a_files)]
    for f in sorted(a_files & b_files, key=lambda f: (f != "run.txt", f)):
        a, b = _read_bytes(os.path.join(a_dir, f)), _read_bytes(os.path.join(b_dir, f))
        if f == "run.txt":
            verdict = compare_runs("%s: run.txt" % name, a.decode("utf-8"), b.decode("utf-8"))
        else:
            verdict = compare_bytes(name, f, a, b)
        if verdict.startswith("DIFFERENT "):
            lines.append(verdict)
    return lines or ["same %s" % name]


def _outcome(path):
    try:
        with open(path, encoding="utf-8") as f:
            lines = [line for line in f.read().splitlines() if line.startswith("outcome ")]
    except FileNotFoundError:
        return None
    return lines[-1][len("outcome "):] if lines else None


def _outcomes_match(driver, plain):
    if driver == "ok" or plain == "ok":
        return driver == plain
    if driver.startswith("error ") and plain == "retry":
        return True
    return driver.startswith("crash ") and plain.startswith("crash ") and \
        driver.split(" ")[1] == plain.split(" ")[1]


def compare_plain(name, driver_dir, plain_dir):
    label = "%s (plain)" % name
    lines = []
    for f in ("out.gbc", "spoiler.json"):
        a, b = os.path.join(driver_dir, f), os.path.join(plain_dir, f)
        if os.path.exists(a) != os.path.exists(b):
            lines.append("DIFFERENT %s: %s only in the %s run" % (label, f, "driver" if os.path.exists(a) else "plain"))
        elif os.path.exists(a):
            verdict = compare_bytes(label, f, _read_bytes(a), _read_bytes(b))
            if verdict.startswith("DIFFERENT "):
                lines.append(verdict)
    driver = _outcome(os.path.join(driver_dir, "run.txt"))
    plain = _outcome(os.path.join(plain_dir, "plain.txt"))
    if driver is None or plain is None:
        lines.append("DIFFERENT %s: no outcome in %s" % (label, "run.txt" if driver is None else "plain.txt"))
    elif not _outcomes_match(driver, plain):
        lines.append("DIFFERENT %s: outcome %s vs %s" % (label, driver, plain))
    return lines or ["same %s" % label]


def inside_tmp(path, repo):
    tmp = os.path.realpath(os.path.join(repo, "tmp"))
    return os.path.realpath(path).startswith(tmp + os.sep)


def _ws_scalar(value):
    if value is None:
        return "-"
    if isinstance(value, bool):
        return "1" if value else "0"
    if isinstance(value, int):
        return render(value)
    if isinstance(value, str) and value and not any(c in value for c in " ,;"):
        return field(value)
    raise OracleError("oracle: world setup value %r cannot be rendered" % (value,))


def _ws_list(values, render_one=_ws_scalar):
    if not isinstance(values, list) or not values:
        raise OracleError("oracle: world setup value %r is not a non-empty list" % (values,))
    return ",".join(render_one(v) for v in values)


def ws_lines(ws, identity, bingo_table):
    for name, value in vars(ws).items():
        if name == "entrance_mapping" and value == identity:
            shown = "identity"
        elif value is None:
            shown = "-"
        elif name == "bingo_goals":
            def index(goal):
                for i, known in enumerate(bingo_table):
                    if known is goal:
                        return str(i)
                raise OracleError("oracle: bingo goal %r is not in the goal table" % (goal,))
            shown = _ws_list(value, index)
        elif name == "sign_maze":
            signs, directions = value
            shown = "%s;%s" % (_ws_list(signs, lambda xy: "%s:%s" % tuple(map(_ws_scalar, xy))),
                               _ws_list(directions))
        elif isinstance(value, dict):
            shown = ",".join("%s=%s" % (_ws_scalar(k), _ws_scalar(v)) for k, v in value.items())
        elif isinstance(value, list):
            shown = _ws_list(value)
        else:
            shown = _ws_scalar(value)
        yield "ws %s %s" % (name, shown)


def pool_line(tag, pool):
    return " ".join([tag] + ["%s=%s" % (item_name(item), render(count)) for item, count in pool.items()])


def place_lines(spots):
    for i, spot in enumerate(spots):
        yield "place %d %s %s %s" % (i, type(spot).__name__, field(spot.nameId),
                                     "-" if spot.item is None else item_name(spot.item))


def runs_varied(a_meta, b_meta, describe=False):
    def read(text):
        return dict(line.split(" ", 1) for line in text.splitlines() if " " in line)
    a, b = read(a_meta), read(b_meta)
    keys = ("hash_ladxr", "checkout") + (("describe",) if describe else ())
    return all(key in a and key in b and a[key] != b[key] for key in keys)


_CASE_LINE = re.compile(r"([A-Za-z0-9_.+-]+) ((?:[0-9a-fA-F]{2})+)((?: [A-Za-z0-9_]+=\S*)*)")


def read_cases(text, names=None):
    cases = []
    for line in text.splitlines():
        if not line or line.startswith("#"):
            continue
        match = _CASE_LINE.fullmatch(line)
        if match is None:
            raise OracleError("oracle: bad case line %r: want <name> <seed hex> <k=v ...>" % line)
        name, seed, settings = match.groups()
        if any(name == case[0] for case in cases):
            raise OracleError("oracle: case %s given twice" % name)
        cases.append((name, bytes.fromhex(seed), settings.split()))
    if names is None:
        return cases
    known = {case[0] for case in cases}
    for name in names:
        if name not in known:
            raise OracleError("oracle: no case %s in the case list" % name)
    return [case for case in cases if case[0] in names]


USAGE = """usage: ladx_oracle.py <command> ...
  compare <name> <run A> <run B>               compare_runs' verdict, exit 1 when DIFFERENT
  compare-bytes <name> <label> <file A> <file B>   compare_bytes' verdict, exit 1 when DIFFERENT
  compare-dirs <name> <dir A> <dir B>          compare_dirs' lines, exit 1 when DIFFERENT
  compare-plain <name> <driver dir> <plain dir>    compare_plain's lines, exit 1 when DIFFERENT
  varied [--describe] <meta A> <meta B>        exit 1 unless runs_varied
  cases <case list> [<name>...]                the cases as `<name> <seed hex> <k=v ...>`, exit 2
                                               on a bad list or an unknown name
  inside-tmp <repo> <path>                     exit 1 unless inside_tmp"""


def main(argv):
    def text(path):
        with open(path, encoding="utf-8") as f:
            return f.read()

    def verdicts(lines):
        print("\n".join(lines))
        return 0 if all(line.startswith("same ") for line in lines) else 1
    command, args = (argv[0], argv[1:]) if argv else (None, [])
    if command == "compare" and len(args) == 3:
        return verdicts([compare_runs(args[0], text(args[1]), text(args[2]))])
    if command == "compare-bytes" and len(args) == 4:
        return verdicts([compare_bytes(args[0], args[1], _read_bytes(args[2]), _read_bytes(args[3]))])
    if command == "compare-dirs" and len(args) == 3:
        return verdicts(compare_dirs(*args))
    if command == "compare-plain" and len(args) == 3:
        return verdicts(compare_plain(*args))
    if command == "varied" and len(args) in (2, 3) and args[:-2] in ([], ["--describe"]):
        return 0 if runs_varied(text(args[-2]), text(args[-1]), describe=len(args) == 3) else 1
    if command == "cases" and args:
        try:
            cases = read_cases(text(args[0]), args[1:] or None)
        except OracleError as e:
            print(e, file=sys.stderr)
            return 2
        for name, seed, settings in cases:
            print(" ".join([name, seed.hex()] + settings))
        return 0
    if command == "inside-tmp" and len(args) == 2:
        return 0 if inside_tmp(args[1], args[0]) else 1
    print(USAGE, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
