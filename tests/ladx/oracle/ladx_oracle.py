"""Shared helpers of the LADX oracle: the RNG tracer, the fixtures' line encoders and the order audit.

Imports only the standard library and stays importable on Python older than 3.11, so that
make test runs everywhere; the oracle scripts themselves need 3.11 (co_qualname).
"""

import contextlib
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
