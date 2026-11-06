"""LADX's case list: the expected cases (tier1.txt's values alone, plus a baseline) and combos
(combos.txt), derived from the committed settings fixtures, and the ROM-free check of the case
directories tests/ladx/cases/ and tests/ladx/combos/ against them. Standard library only; pick
and verify (the seeds, with the ROM) also load LADXR's Settings and run oracle_run.py.
"""

import hashlib
import os
import shutil
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
    def __init__(self, cases, combos, skipped, refused=()):
        self.cases = cases
        self.combos = combos
        self.skipped = skipped
        self.refused = list(refused)


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

    combos, skipped, names, crosses, refused = [], [], set(), [], []

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
        elif kind == "refused" and len(words) == 1 and case_name(words) in dict(cases[1:]) \
                and case_name(words) not in dict(refused):
            refused.append((case_name(words), words))
        else:
            raise CasesError("ladx-cases: bad combos.txt line %r" % line)
    cases = [c for c in cases if c[0] not in dict(refused)]
    refused = [(name, assignments, [n for n, a in combos if assignments[0] in a])
               for name, assignments in refused]
    for name, _, covering in refused:
        if not covering:
            raise CasesError("ladx-cases: refused %s is covered by no combo" % name)
    covered = {n for n, _ in cases} | names
    for a, b in crosses:
        for va in tier1[a]:
            for vb in tier1[b]:
                cell = ["%s=%s" % (k, v) for k, v in ((a, va), (b, vb)) if v != defaults[k]]
                if case_name(cell) not in covered:
                    raise CasesError("ladx-cases: cross cell %s=%s %s=%s not covered"
                                     % (a, va, b, vb))
    return Expected(cases, combos, skipped, refused)


DIRS = ("cases", "combos")
GOLDENS = ("run.txt", "rom_writes.txt", "spoiler.json")
ALLOWED = ("config.txt", "seed") + GOLDENS
KB = 1024
LIMITS = {"rom_writes_case": 224 * KB, "rom_writes_all": 22 * KB * KB, "all": 34 * KB * KB}


def config_text(assignments):
    return "".join(a + "\n" for a in assignments)


def write_case(path, assignments, seed):
    """Creates the case directory <path> with its config.txt and seed; refuses an existing one."""
    os.makedirs(path)
    with open(os.path.join(path, "config.txt"), "w", encoding="utf-8", newline="\n") as f:
        f.write(config_text(assignments))
    with open(os.path.join(path, "seed"), "w", encoding="utf-8", newline="\n") as f:
        f.write(seed.hex() + "\n")


def refusal(seed, nintendo_ok, run):
    """Why candidate seed is refused (`nintendo-bytes`, or the run's outcome without `outcome `),
    or None when it is accepted. nintendo_ok(seed file text) -> bool; run(seed) -> outcome."""
    if not nintendo_ok(seed.hex() + "\n"):
        return "nintendo-bytes"
    outcome = run(seed)
    return None if outcome == "ok" else outcome


def refusal_kind(reason):
    """`nintendo-bytes`, `error` or `crash <exception type>`."""
    words = reason.split(" ")
    return " ".join(words[:2]) if words[0] == "crash" else words[0]


def pick_case(path, name, assignments, nintendo_ok, run):
    """Writes the case directory <path> with the first accepted candidate k < BOUND. Returns
    (k, [(j, refusal)...]), k None (nothing written) when all are refused. Refuses an existing
    directory, before running anything."""
    if os.path.lexists(path):
        raise CasesError("ladx-cases: %s exists, not touched (delete it by hand to re-pick)" % path)
    refusals = []
    for k in range(BOUND):
        seed = derive(name, k)
        reason = refusal(seed, nintendo_ok, run)
        if reason is None:
            write_case(path, assignments, seed)
            return k, refusals
        refusals.append((k, reason))
    return None, refusals


def seed_index(name, text):
    """The k whose derive(name, k) the seed file text holds, or None."""
    for k in range(BOUND):
        if text == derive(name, k).hex() + "\n":
            return k
    return None


def verify_case(path, name, nintendo_ok, run):
    """(k, problems): every candidate below the stored seed's k must be refused, k accepted."""
    k = seed_index(name, _read(os.path.join(path, "seed")))
    if k is None:
        return None, ["the seed is not derive(%s, k) for any k < %d" % (name, BOUND)]
    problems = []
    for j in range(k + 1):
        reason = refusal(derive(name, j), nintendo_ok, run)
        if j < k and reason is None:
            problems.append("candidate %d is acceptable" % j)
        if j == k and reason is not None:
            problems.append("candidate %d is refused: %s" % (j, reason))
    return k, problems


def verify_refused(name, run):
    """({refusal kind: count}, problems) over every candidate of a refused single: each run must
    be not ok (the Nintendo-bytes test plays no part: the refusal is LADXR's)."""
    counts, problems = {}, []
    for k in range(BOUND):
        outcome = run(derive(name, k))
        if outcome == "ok":
            problems.append("candidate %d is ok" % k)
        else:
            counts[refusal_kind(outcome)] = counts.get(refusal_kind(outcome), 0) + 1
    return counts, problems


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
    if seed_index(name, text) is None:
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


def _size(path):
    try:
        return os.path.getsize(path)
    except FileNotFoundError:
        return None


def golden_lines(e, root, pending=False, limits=LIMITS):
    """(problems, summary) of the case folders' golden files: NOGOLDEN, BADGOLDEN and BUDGET
    lines; with pending, a folder holding none of them is counted, not a problem."""
    write_log = _write_log()
    problems, folders, waiting, writes_total, total = [], 0, 0, 0, 0
    for sub, entries in zip(DIRS, (e.cases, e.combos)):
        for name, assignments in entries:
            path, shown = os.path.join(root, sub, name), "%s/%s" % (sub, name)
            if not os.path.isdir(path):
                continue
            sizes = {f: _size(os.path.join(path, f)) for f in GOLDENS}
            if pending and all(n is None for n in sizes.values()):
                waiting += 1
                continue
            folders += 1
            race = "race=1" in assignments
            for f in GOLDENS:
                if sizes[f] is None and not (race and f == "spoiler.json"):
                    problems.append("NOGOLDEN %s: %s missing" % (shown, f))
            if race and sizes["spoiler.json"] is not None:
                problems.append("BADGOLDEN %s: spoiler.json on a race case" % shown)
            total += sum(n for n in sizes.values() if n is not None)
            n = sizes["rom_writes.txt"]
            if n is None:
                continue
            writes_total += n
            try:
                write_log.parse(_read(os.path.join(path, "rom_writes.txt")))
            except write_log.WriteLogError as err:
                problems.append("BADGOLDEN %s: rom_writes.txt: %s" % (shown, err))
            if n > limits["rom_writes_case"]:
                problems.append("BUDGET %s: rom_writes.txt %d bytes, limit %d"
                                % (shown, n, limits["rom_writes_case"]))
    if writes_total > limits["rom_writes_all"]:
        problems.append("BUDGET rom_writes.txt: %d bytes in all, limit %d"
                        % (writes_total, limits["rom_writes_all"]))
    if total > limits["all"]:
        problems.append("BUDGET golden files: %d bytes in all, limit %d" % (total, limits["all"]))
    summary = ("ladx-cases: goldens in %d folders, %srom_writes.txt %d bytes (limits %d per "
               "case, %d in all), golden files %d bytes (limit %d)"
               % (folders, "%d pending, " % waiting if pending else "", writes_total,
                  limits["rom_writes_case"], limits["rom_writes_all"], total, limits["all"]))
    return problems, summary


def _write_log():
    import importlib.util
    spec = importlib.util.spec_from_file_location("ladx_cases_write_log",
                                                  os.path.join(HERE, "write_log.py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def report(e, root, goldens=None, limits=LIMITS):
    """Prints the mismatches and returns 1, or the skips and the summary and returns 0. goldens
    'required' or 'pending' adds golden_lines' problems, and its summary."""
    lines = mismatches(e, root)
    golden_summary = None
    if goldens is not None:
        problems, golden_summary = golden_lines(e, root, goldens == "pending", limits)
        lines += problems
    status = 1 if lines else 0
    if not lines:
        lines = ["skip %s: validate sets %s" % (name, ",".join(changes))
                 for name, changes in e.skipped]
        lines += ["refused %s: LADXR refuses every seed, covered by %s" % (name, " ".join(covering))
                  for name, _, covering in e.refused]
        refused = ", %d singles refused by LADXR" % len(e.refused) if e.refused else ""
        if golden_summary:
            lines.append(golden_summary)
        lines.append("ladx-cases: %d cases, %d combos, %d cells skipped by validate%s, every "
                     "tier-1 value and cross cell covered"
                     % (len(e.cases), len(e.combos), len(e.skipped), refused))
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
        ("missing", lambda r: shutil.rmtree(at(r, "cases", "race-1")),
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
        ("missing golden", lambda r: os.remove(at(r, "cases", "baseline", "rom_writes.txt")),
         ["NOGOLDEN cases/baseline: rom_writes.txt missing"]),
        ("race spoiler", lambda r: _overwrite(at(r, "cases", "race-1", "spoiler.json"), "{}\n"),
         ["BADGOLDEN cases/race-1: spoiler.json on a race case"]),
        ("bad write log", lambda r: _overwrite(at(r, "cases", "goal-5-8", "rom_writes.txt"),
                                               _PLANT_LOG.replace("=2", "=0x2")),
         ["BADGOLDEN cases/goal-5-8: rom_writes.txt: bad length '0x2'"]),
        ("over budget", lambda r: _overwrite(at(r, "cases", "baseline", "rom_writes.txt"),
                                             _PLANT_BIG_LOG),
         ["BUDGET cases/baseline: rom_writes.txt %d bytes, limit %d"
          % (len(_PLANT_BIG_LOG), _PLANT_LIMITS["rom_writes_case"]),
          "BUDGET rom_writes.txt: %d bytes in all, limit %d"
          % (len(_PLANT_BIG_LOG) + 4 * len(_PLANT_LOG), _PLANT_LIMITS["rom_writes_all"])]),
    ]


_PLANT_LOG = "ladx-rom-writes 1\ntables texts\nG fixed\nB 10 =2 00\n"
_PLANT_BIG_LOG = _PLANT_LOG + "".join("B %x 00\n" % (0x20 + 2 * i) for i in range(400))
_PLANT_LIMITS = {"rom_writes_case": 1024, "rom_writes_all": 2048, "all": KB * KB}


def _plant_goldens(root, e):
    for sub, entries in zip(DIRS, (e.cases, e.combos)):
        for case, assignments in entries:
            for f, text in (("run.txt", "outcome ok\n"), ("rom_writes.txt", _PLANT_LOG),
                            ("spoiler.json", None if "race=1" in assignments else "{}\n")):
                if text is not None:
                    _overwrite(os.path.join(root, sub, case, f), text)


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
            _plant_goldens(root, e)
            change(root)
            if mismatches(e, root) + golden_lines(e, root, False, _PLANT_LIMITS)[0] != lines:
                return name
        finally:
            shutil.rmtree(root)
    return None


LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))


class Rom:
    """The ROM-backed side of pick and verify: the ROM at LADX_SHA1, the LADXR checkout at the pin
    (nintendo_bytes.load_ladxr), the strict Nintendo-bytes test of a seed file and the oracle
    run of a candidate, as gen_golden.sh runs it, under tmp/ladx-cases/. Raises CasesError with
    a `setup:` message when an input is missing."""

    def __init__(self, rom, ladxr, root):
        import nintendo_bytes
        if sys.version_info < (3, 11):
            raise CasesError("setup: ladx-cases: python3 is older than 3.11 (run setup.sh)")
        if not os.path.isfile(rom):
            raise CasesError("setup: ladx-cases: no ROM at %s (run fetch_rom.sh)" % rom)
        with open(rom, "rb") as f:
            data = f.read()
        if hashlib.sha1(data).hexdigest() != LADX_SHA1:
            raise CasesError("setup: ladx-cases: %s is not the LADX English 1.0 ROM (%s...)"
                             % (rom, LADX_SHA1[:8]))
        try:
            self.corpus = nintendo_bytes.load_ladxr(ladxr)
        except nintendo_bytes.SetupError as e:
            raise CasesError("setup: ladx-cases: %s" % e)
        self.nb = nintendo_bytes
        self.index = nintendo_bytes.RomIndex(data)
        self.rom, self.ladxr, self.root = os.path.abspath(rom), os.path.abspath(ladxr), root
        self.work = os.path.join(TOP, "tmp", "ladx-cases")

    def nintendo_ok(self, path):
        """The strict scope whatever the root, so a trial root under tmp/ picks the same seeds."""
        rel = os.path.relpath(os.path.abspath(path), TOP)
        return lambda text: not self.nb.check_blob(rel, text.encode("utf-8"), self.index,
                                                   self.corpus, scope="strict")[0]

    def run(self, name, assignments):
        import shutil
        import subprocess

        def one(seed):
            out = os.path.join(self.work, "runs", name)
            shutil.rmtree(out, ignore_errors=True)
            # python3 without -I or -E, which would drop PYTHONHASHSEED.
            done = subprocess.run(
                ["python3", os.path.join(HERE, "oracle_run.py"), "--rom", self.rom, "--dir", out,
                 "--work", self.work, self.ladxr, seed.hex()] + assignments,
                env=ladx_oracle.oracle_env(0), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT, check=False)
            last = (_read(os.path.join(out, "run.txt")) or "").splitlines()[-1:]
            shutil.rmtree(out, ignore_errors=True)
            if done.returncode != 0 or not last or not last[0].startswith("outcome "):
                raise CasesError("ladx-cases: oracle_run.py failed on %s %s: %s"
                                 % (name, seed.hex(), done.stdout.decode("utf-8", "replace")))
            return last[0][len("outcome "):]
        return one

    def validate_changes(self, entries):
        """{name: [k=v...]}: what the live LADXR validate changes of each case's own values."""
        cwd = os.getcwd()
        work = ladx_oracle.enter_checkout(self.ladxr, os.path.join(self.work, "validate"))
        try:
            import contextlib
            import io
            import settings
            found = {}
            for name, assignments in entries:
                s = settings.Settings()
                for a in assignments:
                    s.set(a)
                with contextlib.redirect_stdout(io.StringIO()):
                    s.validate()
                values = {x.key: ladx_oracle.setting_value(x.value) for x in s}
                found[name] = ["%s=%s" % (k, values[k]) for k, _, v in
                               (a.partition("=") for a in assignments) if values[k] != v]
            return found
        finally:
            os.chdir(cwd)
            import shutil
            shutil.rmtree(work, ignore_errors=True)


def _targets(e, root, names, existing):
    """[(sub, name, assignments)] of the named cases (default: those with (existing) or without
    a directory), in the expected order; refuses an unknown name. A refused single's name is
    known and selects nothing."""
    entries = [("cases", n, a) for n, a in e.cases] + [("combos", n, a) for n, a in e.combos]
    refused = {r[0] for r in e.refused}
    for name in names:
        if name not in {n for _, n, _ in entries} | refused:
            raise CasesError("ladx-cases: no case %s" % name)
    have = {n for sub in DIRS for n in _entries(os.path.join(root, sub))}
    if not names:
        return [t for t in entries if (t[1] in have) == existing]
    wrong = [n for n in names if n not in refused and (n in have) != existing]
    if wrong:
        raise CasesError("ladx-cases: %s %s" % ("no directory for" if existing else
                                                "exists, not touched (delete it by hand to "
                                                "re-pick):", " ".join(wrong)))
    return [t for t in entries if t[1] in names]


def _validate_refused(rom, targets):
    changes = rom.validate_changes([(n, a) for _, n, a in targets])
    return ["%s: validate sets %s" % (n, ",".join(changes[n])) for _, n, _ in targets if changes[n]]


def pick(e, rom, names):
    """Picks every target case's seed; prints one line per case, then the refusal counts."""
    targets = _targets(e, rom.root, names, existing=False)
    refused = _validate_refused(rom, targets)
    if refused:
        print("\n".join(refused))
        return 1
    status, counts, picked = 0, {}, 0
    for sub, name, assignments in targets:
        path = os.path.join(rom.root, sub, name)
        k, refusals = pick_case(path, name, assignments,
                                rom.nintendo_ok(os.path.join(path, "seed")),
                                rom.run(name, assignments))
        for _, reason in refusals:
            counts[refusal_kind(reason)] = counts.get(refusal_kind(reason), 0) + 1
        detail = "%d refused%s" % (len(refusals), "".join(
            (": " if i == 0 else "; ") + "k=%d %s" % r for i, r in enumerate(refusals)))
        if k is None:
            status = 1
            print("%s: no seed below %d (%s)" % (name, BOUND, detail), flush=True)
        else:
            picked += 1
            print("%s: seed k=%d (%s)" % (name, k, detail), flush=True)
    print("ladx-cases: %d of %d cases picked, %d candidates refused%s"
          % (picked, len(targets), sum(counts.values()),
             "".join(", %d %s" % (n, kind) for kind, n in sorted(counts.items()))))
    return status


def verify(e, rom, names):
    """Re-runs every target case's candidates up to its seed: prints `verified <name> k=<k>` or
    `UNVERIFIED <name>: <problem>` lines, then the summary."""
    lines = mismatches(e, rom.root)
    if lines:
        print("\n".join(lines))
        return 1
    refused_singles = [r for r in e.refused if not names or r[0] in names]
    targets = _targets(e, rom.root, names, existing=True)
    refused = _validate_refused(rom, targets)
    if refused:
        print("\n".join("UNVERIFIED " + line for line in refused))
        return 1
    status = 0
    for name, assignments, _ in refused_singles:
        counts, problems = verify_refused(name, rom.run(name, assignments))
        for problem in problems:
            status = 1
            print("UNVERIFIED %s: %s" % (name, problem), flush=True)
        if not problems:
            print("refused %s: %d candidates not ok (%s)" % (name, BOUND, ", ".join(
                "%d %s" % (n, kind) for kind, n in sorted(counts.items()))), flush=True)
    for sub, name, assignments in targets:
        path = os.path.join(rom.root, sub, name)
        k, problems = verify_case(path, name, rom.nintendo_ok(os.path.join(path, "seed")),
                                  rom.run(name, assignments))
        for problem in problems:
            status = 1
            print("UNVERIFIED %s: %s" % (name, problem), flush=True)
        if not problems:
            print("verified %s k=%d" % (name, k), flush=True)
    if status == 0:
        print("ladx-cases: %d seeds verified first-ok, %d refused singles not ok on any "
              "candidate" % (len(targets), len(refused_singles)))
    return status


USAGE = """usage: ladx_cases.py <command> [--root <dir>] [--inputs <dir>] [--rom <file>]
                    [--ladxr <dir>] ...
  check [--pending-goldens]
                    proves itself on planted trees, then compares <root>/cases/ and <root>/combos/
                    with the expected list and checks each case folder's goldens (run.txt,
                    rom_writes.txt parsing with its version line, spoiler.json exactly when race
                    is off) and the size budget (LIMITS), printing the totals: exit 1 on a
                    mismatch, 2 on a malformed input; --pending-goldens accepts a folder holding
                    no golden file (until the goldens are generated)
  list [<name>...]  after the same comparison (quiet unless it fails), the case directories as
                    `<name> <seed hex> <k=v ...>` lines, cases then combos
  pick [<name>...]  with the ROM: writes each case without a directory (or the named ones, which
                    must not exist) with its first candidate seed that clears the strict
                    Nintendo-bytes check and gives `outcome ok`; `<name>: seed k=<k> (...)` lines,
                    then the refusal counts by kind; exit 1 when a case has no such seed below 64
  verify [<name>...] with the ROM: after the comparison, proves each stored seed is the first such
                    candidate, and that LADXR refuses every candidate of each `refused` single;
                    `verified <name> k=<k>` and `refused <name>: ...` lines, then the summary
<root> defaults to tests/ladx, <inputs> (tier1.txt, settings.txt, settings_validate.txt,
combos.txt) to tests/ladx/oracle, <rom> to tmp/ladx/base.gbc (SHA-1 d90ac17e...), <ladxr> to
tests/ladx/oracle/ladxr at the pin. A missing ROM or checkout is a `setup:` line, exit 2."""


def main(argv):
    args, options = [], {"--root": os.path.dirname(HERE), "--inputs": HERE,
                         "--rom": os.path.join(TOP, "tmp", "ladx", "base.gbc"),
                         "--ladxr": os.path.join(HERE, "ladxr")}
    pending = "--pending-goldens" in argv
    argv = [a for a in argv if a != "--pending-goldens"]
    while argv:
        if argv[0] in options and len(argv) >= 2:
            options[argv[0]] = argv[1]
            argv = argv[2:]
        else:
            args.append(argv[0])
            argv = argv[1:]
    command, names = (args[0], args[1:]) if args else (None, [])
    if command not in ("check", "list", "pick", "verify") or (command == "check" and names) \
            or (pending and command != "check"):
        print(USAGE, file=sys.stderr)
        return 2
    root = options["--root"]
    try:
        texts = []
        for name in ("tier1.txt", "settings.txt", "settings_validate.txt", "combos.txt"):
            with open(os.path.join(options["--inputs"], name), encoding="utf-8") as f:
                texts.append(f.read())
        e = expected_list(*texts)
        if command == "list" and names:
            listing(e, root, names)
        if command in ("pick", "verify"):
            _targets(e, root, names, existing=command == "verify")
            rom = Rom(options["--rom"], options["--ladxr"], root)
            return (pick if command == "pick" else verify)(e, rom, names)
    except (CasesError, OSError) as err:
        print(err, file=sys.stderr)
        return 2
    if command == "check":
        failed = self_test()
        if failed:
            print("ladx-cases: self-test failed: %s" % failed, file=sys.stderr)
            return 1
        return report(e, root, "pending" if pending else "required")
    lines = mismatches(e, root)
    if lines:
        print("\n".join(lines), file=sys.stderr)
        return 1
    print("\n".join(listing(e, root, names or None)))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
