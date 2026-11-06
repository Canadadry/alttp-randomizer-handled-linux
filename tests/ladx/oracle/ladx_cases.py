"""LADX's case list: the expected cases (tier1.txt's values alone, plus a baseline) and combos
(combos.txt), derived from the committed settings fixtures, and the ROM-free check of the case
directories tests/ladx/cases/ and tests/ladx/combos/ against them. Standard library only.
"""

import hashlib
import os
import sys
import types

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import ladx_oracle  # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
BOUND = 64


class CasesError(Exception):
    pass


def derive(name, k):
    """Candidate seed k of a case: the first 16 bytes of SHA-256 of `ladx-case <name> <k>`."""
    if not 0 <= k < BOUND:
        raise CasesError("ladx-cases: candidate %r outside 0..%d" % (k, BOUND - 1))
    return hashlib.sha256(("ladx-case %s %d" % (name, k)).encode("utf-8")).digest()[:16]


def _python_value(kind, text):
    if kind == "bool":
        return {"0": False, "1": True}[text]
    if kind == "float":
        return float.fromhex(text)
    return "" if text == '""' else text


def read_settings(text):
    """settings.txt as stand-ins of LADXR's Setting objects (key, default, options), in order."""
    settings = []
    try:
        for line in text.splitlines():
            if not line or line.startswith("#"):
                continue
            tag, *fields = line.split("\t")
            if tag.startswith("setting ") and len(fields) == 7:
                kind = fields[2]
                settings.append(types.SimpleNamespace(
                    key=tag[len("setting "):], kind=kind,
                    default=_python_value(kind, fields[3]), options=[]))
            elif tag.startswith("option ") and len(fields) == 4 and settings \
                    and settings[-1].key == tag[len("option "):]:
                s = settings[-1]
                s.options.append((_python_value(s.kind, fields[0]), fields[1], fields[2]))
            else:
                raise CasesError("ladx-cases: bad settings line %r" % line)
    except (KeyError, ValueError) as e:
        raise CasesError("ladx-cases: bad settings value: %s" % e)
    for s in settings:
        s.options = s.options or None
    return settings


def case_name(assignments):
    """`<key>-<value>` per assignment joined with +, or baseline when there is none."""
    return "+".join(a.replace("=", "-", 1) for a in assignments) or "baseline"


class Validate:
    """settings_validate.txt: what LADXR's validate changes for one value, or a pair of values,
    of the keys it reads or writes."""

    def __init__(self, text):
        self.keys = None
        self.changes = {}
        for line in text.splitlines():
            if line.startswith("# keys "):
                self.keys = set(line.split()[2:])
            if not line or line.startswith("#"):
                continue
            head, tab, _ = line.partition("\t")
            parts = head.split(" ")
            if not tab or len(parts) != 4 or parts[0] != "validate" or parts[2] != "->":
                raise CasesError("ladx-cases: bad validate line %r" % line)
            changes = [] if parts[3] == "=" else parts[3].split(",")
            self.changes[frozenset(parts[1].split(","))] = changes
        if self.keys is None:
            raise CasesError("ladx-cases: settings_validate has no '# keys' line")

    def own_changes(self, assignments):
        """The changes validate makes to the assignments' own keys, from the fixture's single and
        pair vectors only (a larger set is checked pair by pair)."""
        own = {a.split("=", 1)[0] for a in assignments}
        relevant = [a for a in assignments if a.split("=", 1)[0] in self.keys]
        groups = [[a] for a in relevant] + [[a, b] for i, a in enumerate(relevant)
                                            for b in relevant[i + 1:]]
        found = []
        for group in groups:
            changes = self.changes.get(frozenset(group))
            if changes is None:
                raise CasesError("ladx-cases: settings_validate has no vector for %s"
                                 % ",".join(group))
            found += [c for c in changes if c.split("=", 1)[0] in own and c not in found]
        return found


class Expected:
    def __init__(self, cases, combos, skipped):
        self.cases = cases
        self.combos = combos
        self.skipped = skipped


def expected_list(tier1_text, settings_text, validate_text, spec_text):
    """The expected cases and combos, each [(name, [k=v, ...])], and the skipped cross cells
    [(name, [validate's changes])]. Raises CasesError on a malformed or inconsistent input."""
    settings = read_settings(settings_text)
    validate = Validate(validate_text)
    try:
        tier1 = dict(ladx_oracle.read_tier1(tier1_text, settings))
    except ladx_oracle.OracleError as e:
        raise CasesError(str(e))
    order = [s.key for s in settings]
    defaults = {s.key: ladx_oracle.setting_value(s.default) for s in settings}
    non_default = {key: [v for v in values if v != defaults[key]] for key, values in tier1.items()}
    for key, values in non_default.items():
        if '""' in values:
            raise CasesError("ladx-cases: %s's non-default value \"\" has no k=v form" % key)

    def normalised(assignments):
        return sorted(assignments, key=lambda a: order.index(a.split("=", 1)[0]))

    cases = [("baseline", [])]
    for key in order:
        for value in non_default.get(key, []):
            assignments = ["%s=%s" % (key, value)]
            if validate.own_changes(assignments):
                raise CasesError("ladx-cases: validate rewrites the single case %s"
                                 % case_name(assignments))
            cases.append((case_name(assignments), assignments))

    combos, skipped, names, crosses = [], [], set(), []

    def add(assignments, line):
        name = case_name(assignments)
        if name in names:
            raise CasesError("ladx-cases: combos.txt line %r repeats %s" % (line, name))
        names.add(name)
        changes = validate.own_changes(assignments)
        if not changes:
            combos.append((name, assignments))
        elif line.startswith("cross "):
            skipped.append((name, changes))
        else:
            raise CasesError("ladx-cases: combos.txt line %r: validate sets %s"
                             % (line, ",".join(changes)))

    for line in spec_text.splitlines():
        if not line or line.startswith("#"):
            continue
        kind, *words = line.split(" ")
        if kind == "cross" and len(words) == 2 and words[0] != words[1] \
                and all(w in non_default for w in words):
            a, b = sorted(words, key=order.index)
            crosses.append((a, b))
            for va in non_default[a]:
                for vb in non_default[b]:
                    add(["%s=%s" % (a, va), "%s=%s" % (b, vb)], line)
        elif kind == "case" and len(words) >= 2:
            keys = [w.partition("=")[0] for w in words]
            if len(set(keys)) != len(keys) or any(
                    "=" not in w or w.partition("=")[2] not in non_default.get(k, [])
                    for k, w in zip(keys, words)):
                raise CasesError("ladx-cases: combos.txt line %r: want distinct keys, each with "
                                 "a non-default tier-1 value" % line)
            add(normalised(words), line)
        else:
            raise CasesError("ladx-cases: bad combos.txt line %r" % line)
    covered = {n for n, _ in cases} | names
    for a, b in crosses:
        for va in tier1[a]:
            for vb in tier1[b]:
                cell = ["%s=%s" % (k, v) for k, v in ((a, va), (b, vb)) if v != defaults[k]]
                if case_name(cell) not in covered:
                    raise CasesError("ladx-cases: cross cell %s=%s %s=%s not covered"
                                     % (a, va, b, vb))
    return Expected(cases, combos, skipped)


DIRS = ("cases", "combos")
ALLOWED = ("config.txt", "seed")


def config_text(assignments):
    return "".join(a + "\n" for a in assignments)


def write_case(path, assignments, seed):
    """Creates the case directory <path> with its config.txt and seed; refuses an existing one."""
    os.makedirs(path)
    with open(os.path.join(path, "config.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(config_text(assignments))
    with open(os.path.join(path, "seed"), "w", encoding="utf-8", newline="\n") as f:
        f.write(seed.hex() + "\n")


def _read(path):
    try:
        with open(path, "rb") as f:
            return f.read().decode("utf-8", "replace")
    except FileNotFoundError:
        return None


def _seed_problem(name, text):
    if text is None:
        return "no seed file"
    hex_digits = text[:-1]
    if len(text) != 33 or not text.endswith("\n") or any(c not in "0123456789abcdef"
                                                         for c in hex_digits):
        return "not 32 lowercase hex digits and a newline"
    if all(derive(name, k).hex() != hex_digits for k in range(BOUND)):
        return "not derive(%s, k) for any k < %d" % (name, BOUND)
    return None


def _entries(directory):
    try:
        return sorted(os.listdir(directory))
    except FileNotFoundError:
        return []


def mismatches(e, root):
    """One line per difference between the case directories under <root> (cases/, combos/) and
    the expected list e: MISSING, EXTRA, WRONG, BADSEED, STRAY or BOTH."""
    found = {sub: [n for n in _entries(os.path.join(root, sub))
                   if os.path.isdir(os.path.join(root, sub, n))] for sub in DIRS}
    both = sorted(set(found["cases"]) & set(found["combos"]))
    lines = []
    for sub, entries in zip(DIRS, (e.cases, e.combos)):
        want = dict(entries)
        names = [n for n, _ in entries] + [n for n in found[sub] if n not in want]
        for name in names:
            path = os.path.join(root, sub, name)
            shown = "%s/%s" % (sub, name)
            if name not in found[sub]:
                lines.append("MISSING " + shown)
                continue
            if name not in want:
                if name not in both:
                    lines.append("EXTRA " + shown)
                continue
            config = _read(os.path.join(path, "config.txt"))
            if config != config_text(want[name]):
                lines.append("WRONG %s: config.txt is %s, expected %r"
                             % (shown, "missing" if config is None else repr(config),
                                config_text(want[name])))
            problem = _seed_problem(name, _read(os.path.join(path, "seed")))
            if problem:
                lines.append("BADSEED %s: %s" % (shown, problem))
            lines += ["STRAY %s/%s" % (shown, f) for f in _entries(path) if f not in ALLOWED]
        lines += ["STRAY %s/%s" % (sub, f) for f in _entries(os.path.join(root, sub))
                  if f not in found[sub]]
    return lines + ["BOTH " + name for name in both]


def report(e, root):
    """Prints the mismatches and returns 1, or the skips and the summary and returns 0."""
    lines = mismatches(e, root)
    status = 1 if lines else 0
    if not lines:
        lines = ["skip %s: validate sets %s" % (name, ",".join(changes))
                 for name, changes in e.skipped]
        lines.append("ladx-cases: %d cases, %d combos, %d cells skipped by validate, every "
                     "tier-1 value and cross cell covered"
                     % (len(e.cases), len(e.combos), len(e.skipped)))
    print("\n".join(lines))
    return status


def listing(e, root, names=None):
    """The case directories under <root> as `<name> <seed hex> <k=v ...>` lines, cases then combos
    in the expected order (only the named ones when names is given), the format ladx_oracle.py
    cases prints. Refuses an unknown name; the caller checks the tree first."""
    entries = [("cases", n, a) for n, a in e.cases] + [("combos", n, a) for n, a in e.combos]
    known = {n for _, n, _ in entries}
    for name in names or []:
        if name not in known:
            raise CasesError("ladx-cases: no case %s" % name)
    lines = []
    for sub, name, assignments in entries:
        if names is None or name in names:
            seed = _read(os.path.join(root, sub, name, "seed"))
            lines.append(" ".join([name, seed.strip()] + assignments))
    return lines


_PLANT_SETTINGS = ("setting logic\tMain\tL\tstr\t\"\"\tLogic\t0\t-\n"
                   "option logic\t\"\"\t\"\"\tNormal\t-\noption logic\thell\tH\tHell\t-\n"
                   "setting race\tMain\tV\tbool\t0\tRace\t0\t-\n"
                   "setting goal\tGameplay\tG\tstr\tvanilla\tGoal\t0\t-\n"
                   "option goal\tvanilla\tv\tVanilla\t-\noption goal\t5-8\t>\tLong\t-\n")


def _overwrite(path, text):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)


def _plant_variants():
    """(name, change to a correct planted tree, the lines the check must print)."""
    combo = "logic-hell+race-1"

    def at(root, sub, name, file=None):
        return os.path.join(root, sub, name, *([file] if file else []))
    return [
        ("missing", lambda r: os.remove(at(r, "cases", "race-1", "seed")) or
         os.remove(at(r, "cases", "race-1", "config.txt")) or os.rmdir(at(r, "cases", "race-1")),
         ["MISSING cases/race-1"]),
        ("extra", lambda r: write_case(at(r, "combos", "race-1+goal-5-8"), ["race=1", "goal=5-8"],
                                       derive("race-1+goal-5-8", 0)),
         ["EXTRA combos/race-1+goal-5-8"]),
        ("order swap", lambda r: _overwrite(at(r, "combos", combo, "config.txt"),
                                            "race=1\nlogic=hell\n"),
         ["WRONG combos/%s: config.txt is 'race=1\\nlogic=hell\\n', expected "
          "'logic=hell\\nrace=1\\n'" % combo]),
        ("wrong value", lambda r: _overwrite(at(r, "cases", "goal-5-8", "config.txt"),
                                             "goal=vanilla\n"),
         ["WRONG cases/goal-5-8: config.txt is 'goal=vanilla\\n', expected 'goal=5-8\\n'"]),
        ("seed not derived", lambda r: _overwrite(at(r, "cases", "race-1", "seed"),
                                                  derive("race-1", 0)[::-1].hex() + "\n"),
         ["BADSEED cases/race-1: not derive(race-1, k) for any k < %d" % BOUND]),
        ("uppercase seed", lambda r: _overwrite(at(r, "cases", "baseline", "seed"),
                                                derive("baseline", 0).hex().upper() + "\n"),
         ["BADSEED cases/baseline: not 32 lowercase hex digits and a newline"]),
        ("extra file", lambda r: _overwrite(at(r, "cases", "baseline", "golden.txt"), ""),
         ["STRAY cases/baseline/golden.txt"]),
        ("both", lambda r: write_case(at(r, "combos", "race-1"), ["race=1"], derive("race-1", 0)),
         ["BOTH race-1"]),
    ]


def self_test():
    """Plants a correct tree, then each broken variant, and returns the first variant whose check
    output is not exactly its lines (the correct tree's name is `correct`), or None."""
    import shutil
    import tempfile
    e = expected_list("logic all\nrace all\ngoal all\n", _PLANT_SETTINGS,
                      "# keys goal\nvalidate goal=vanilla -> =\t\nvalidate goal=5-8 -> =\tG>\n",
                      "case race=1 logic=hell\n")
    for name, change, lines in [("correct", lambda r: None, [])] + _plant_variants():
        root = tempfile.mkdtemp()
        try:
            for sub, entries in zip(DIRS, (e.cases, e.combos)):
                for k, (case, assignments) in enumerate(entries):
                    write_case(os.path.join(root, sub, case), assignments, derive(case, k))
            change(root)
            if mismatches(e, root) != lines:
                return name
        finally:
            shutil.rmtree(root)
    return None


USAGE = """usage: ladx_cases.py <command> [--root <dir>] [--inputs <dir>] ...
  check             proves itself on planted trees, then compares <root>/cases/ and <root>/combos/
                    with the expected list: exit 1 on a mismatch, 2 on a malformed input
  list [<name>...]  after the same comparison (quiet unless it fails), the case directories as
                    `<name> <seed hex> <k=v ...>` lines, cases then combos
<root> defaults to tests/ladx, <inputs> (tier1.txt, settings.txt, settings_validate.txt,
combos.txt) to tests/ladx/oracle."""


def main(argv):
    args, options = [], {"--root": os.path.dirname(HERE), "--inputs": HERE}
    while argv:
        if argv[0] in options and len(argv) >= 2:
            options[argv[0]] = argv[1]
            argv = argv[2:]
        else:
            args.append(argv[0])
            argv = argv[1:]
    command, names = (args[0], args[1:]) if args else (None, [])
    if command not in ("check", "list") or (command == "check" and names):
        print(USAGE, file=sys.stderr)
        return 2
    try:
        texts = []
        for name in ("tier1.txt", "settings.txt", "settings_validate.txt", "combos.txt"):
            with open(os.path.join(options["--inputs"], name), encoding="utf-8") as f:
                texts.append(f.read())
        e = expected_list(*texts)
        if command == "list" and names:
            listing(e, options["--root"], names)
    except (CasesError, OSError) as err:
        print(err, file=sys.stderr)
        return 2
    root = options["--root"]
    if command == "check":
        failed = self_test()
        if failed:
            print("ladx-cases: self-test failed: %s" % failed, file=sys.stderr)
            return 1
        return report(e, root)
    lines = mismatches(e, root)
    if lines:
        print("\n".join(lines), file=sys.stderr)
        return 1
    print("\n".join(listing(e, root, names or None)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
