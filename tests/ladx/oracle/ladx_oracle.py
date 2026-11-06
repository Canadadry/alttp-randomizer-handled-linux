"""Shared helpers of the LADX oracle: the RNG tracer, the fixtures' line encoders, the double run's
comparator and the order audit.

Imports only the standard library and stays importable on Python older than 3.11, so that
make test runs everywhere; the oracle scripts themselves need 3.11 (co_qualname).
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
    """A number as a trace or fixture field: integers in decimal, floats as float.hex."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise OracleError("oracle: cannot render %r" % (value,))
    if isinstance(value, float):
        return value.hex()
    return str(value)


def render_list(values):
    return ",".join(render(v) for v in values) if values else "-"


def seed_hex(seed):
    """A byte seed as lower-case hex, "-" when empty."""
    if not isinstance(seed, (bytes, bytearray)):
        raise OracleError("oracle: seed %r is not bytes" % (seed,))
    return seed.hex() or "-"


def field(text):
    """A text field of a fixture: '""' when empty; a tab, a newline or a quote is refused."""
    if any(c in text for c in "\t\n\r\""):
        raise OracleError("oracle: field %r holds a tab, a newline or a quote" % (text,))
    return text or '""'


def item_name(name):
    if not re.fullmatch(r"[A-Z0-9_]+", name):
        raise OracleError("oracle: item name %r is not [A-Z0-9_]+" % (name,))
    return name


def oracle_env(hashseed, base=None):
    """The environment of an oracle run: `base` (default os.environ) with PYTHONHASHSEED set and
    bytecode writing off. Run python3 without -I or -E: both drop PYTHONHASHSEED."""
    env = dict(os.environ if base is None else base)
    env["PYTHONHASHSEED"] = str(hashseed)
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    return env


def enter_checkout(ladxr_dir, work_root):
    """Puts the LADXR checkout first on sys.path and chdirs into a new empty directory under
    work_root, so that LADXR's stray files never land in the repository. Returns that directory."""
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
    """A random.Random that counts MT words and traces each outermost public call into `tracer`
    (set by the subclass Tracer makes). Only the methods in COVERED are allowed."""

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
    """Collects trace lines `<n> <gen> <stage> <method> <args> -> <result> [<words>]`; <stage> is
    `<module>:<qualname>` of the nearest calling frame under `root`, else "-"."""

    def __init__(self, root):
        self.root = os.path.abspath(root)
        self.lines = []
        self.Random = type("TracingRandom", (TracingRandom,), {"tracer": self})

    @contextlib.contextmanager
    def installed(self):
        """Within: random.Random is this tracer's class, and random's module functions are bound
        to one traced generator whose <gen> is "global"."""
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
    """A setting's type in settings.txt: bool, str or float, from its default as LADXR types it."""
    for kind in (bool, str, float):
        if isinstance(default, kind):
            return kind.__name__
    raise OracleError("oracle: setting default %r is not a bool, a str or a float" % (default,))


def setting_value(value):
    """A setting's value as a fixture field: 0/1 for a bool, float.hex for a float, else field()."""
    kind = setting_kind(value)
    if kind == "bool":
        return "1" if value else "0"
    if kind == "float":
        return render(value)
    if " " in value or "," in value or "=" in value:
        raise OracleError("oracle: setting value %r holds a space, a comma or an =" % (value,))
    return field(value)


def settings_lines(settings):
    """settings.txt's lines for LADXR's Setting objects (or stand-ins with the same attributes):
    one setting line each, then its option lines, the label's |meta split as toJson does."""
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
    """Every value a setting can take, rendered: its options, 0 1 for a bool, else its default."""
    if s.options:
        return [setting_value(key) for key, _, _ in s.options]
    if setting_kind(s.default) == "bool":
        return ["0", "1"]
    return [setting_value(s.default)]


def read_tier1(text, settings):
    """tier1.txt as [(key, [rendered value, ...])], in file order. Refuses a key or a value that
    the settings do not define, a setting with free values, a repeat, and an order not LADXR's."""
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
    """settings_validate.txt's vectors for LADXR's Settings class (or a stand-in): the keys that
    validate reads or writes when one setting is set alone (in LADXR's order), and one line
    `validate <k=v,...> -> <changed k=v,... or => >\t<short string after>` per value of each of
    them alone, then per pair of values of two of them. validate's prints are dropped. Refuses a
    pair whose validate touches a key outside the singles' set."""
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
AUDIT_VERDICTS = ("patched", "unreached", "order-free")


def audit_sites(root):
    """Every order-sensitive site of the .py files under root, www/ excepted, as
    (file, line number, stripped text), in file then line order."""
    sites = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if not (dirpath == root and d in ("www", ".git")))
        for name in sorted(filenames):
            if name.endswith(".py"):
                path = os.path.join(dirpath, name)
                rel = os.path.relpath(path, root).replace(os.sep, "/")
                with open(path, encoding="utf-8") as f:
                    for n, line in enumerate(f, 1):
                        if AUDIT_SITE.search(line):
                            sites.append((rel, n, line.strip()))
    return sorted(sites)


def read_audit(text):
    """order_audit.txt as {(file, stripped text): (verdict, reason)}; "#" lines are comments."""
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
    """Checks the sites under root against the audit table. Returns (ok, report lines): one
    UNAUDITED line per site without an entry and one STALE line per entry without a site, else
    the summary line."""
    table = read_audit(table_text)
    sites = audit_sites(root)
    keys = {(rel, text) for rel, _, text in sites}
    lines = ["UNAUDITED %s:%d %s" % site for site in sites if (site[0], site[2]) not in table]
    lines += ["STALE %s %s" % key for key in sorted(table) if key not in keys]
    if lines:
        return False, lines
    return True, ["order audit: %d sites, all audited" % len(sites)]


def compare_runs(name, a_text, b_text):
    """The double run's verdict on one case: `same <name>`, or `DIFFERENT <name>: line <n>: <A line>
    | <B line>` at the first differing line, `<end of file>` standing for a missing one."""
    a_lines, b_lines = a_text.splitlines(), b_text.splitlines()
    for n in range(max(len(a_lines), len(b_lines))):
        a = a_lines[n] if n < len(a_lines) else "<end of file>"
        b = b_lines[n] if n < len(b_lines) else "<end of file>"
        if a != b:
            return "DIFFERENT %s: line %d: %s | %s" % (name, n + 1, a, b)
    return "same %s" % name


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
    """The world setup as `ws <field> <value>` lines, in its attribute order: entrance_mapping as
    `identity` when it equals `identity`, else k=v,...; bingo_goals as indexes into bingo_table;
    sign_maze as x:y,...;direction,...; lists comma-separated, dicts k=v,..., None as -."""
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
    """`<tag> <item>=<count> ...` for an item pool dict, in its order, zero counts kept."""
    return " ".join([tag] + ["%s=%s" % (item_name(item), render(count)) for item, count in pool.items()])


def place_lines(spots):
    """`place <i> <class> <nameId> <item or ->` for each spot of LADXR's iteminfo_list."""
    for i, spot in enumerate(spots):
        yield "place %d %s %s %s" % (i, type(spot).__name__, field(spot.nameId),
                                     "-" if spot.item is None else item_name(spot.item))


def runs_varied(a_meta, b_meta):
    """Whether the double run's two meta files show another hash("ladxr") and another checkout."""
    def read(text):
        return dict(line.split(" ", 1) for line in text.splitlines() if " " in line)
    a, b = read(a_meta), read(b_meta)
    return all(key in a and key in b and a[key] != b[key] for key in ("hash_ladxr", "checkout"))


def main(argv):
    """`compare <name> <run A> <run B>`: prints compare_runs' verdict, exits 1 when DIFFERENT.
    `varied <meta A> <meta B>`: exits 1 unless runs_varied."""
    def text(path):
        with open(path, encoding="utf-8") as f:
            return f.read()
    if len(argv) == 4 and argv[0] == "compare":
        verdict = compare_runs(argv[1], text(argv[2]), text(argv[3]))
        print(verdict)
        return 0 if verdict.startswith("same ") else 1
    if len(argv) == 3 and argv[0] == "varied":
        return 0 if runs_varied(text(argv[1]), text(argv[2])) else 1
    print("usage: ladx_oracle.py compare <name> <run A> <run B> | varied <meta A> <meta B>",
          file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
