"""Usage: python3 -I test_oracle.py

Unit tests of ladx_oracle.py, the LADX oracle's shared helpers, with no LADXR and no network.
"""

import contextlib
import importlib.util
import io
import os
import random
import sys
import tempfile
import types
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("ladx_oracle", os.path.join(HERE, "ladx_oracle.py"))
ladx_oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ladx_oracle)

SEED = bytes(range(16))


def mixed(rng, rewind=True):
    out = []
    for n in range(40):
        out.append(rng.random())
        out.append(rng.getrandbits(1 + n))
        out.append(rng.randrange(0, 100))
        out.append(rng.randrange(2**32 + 1))
        out.append(rng.randint(-1, 8))
        out.append(rng.choice("abcdefg"))
        seq = list(range(n % 10))
        rng.shuffle(seq)
        out.append(seq)
        out.append(rng.choices("abcde", (10, 5, 10, 10, 1)))
        out.append(rng.choices("abc", k=3))
        out.append(rng.uniform(0, 100))
        if n == 20:
            state = rng.getstate()
        if n == 30 and rewind:
            rng.setstate(state)
    return out


def shuffled(rng, n):
    perm = list(range(n))
    rng.shuffle(perm)
    return ",".join(map(str, perm))


def words_between(before, after):
    rng = random.Random()
    rng.setstate(before)
    for words in range(1000):
        if rng.getstate() == after:
            return words
        rng.getrandbits(32)
    raise AssertionError("states more than 1000 words apart")


class TracingRandomTest(unittest.TestCase):
    def test_same_stream_as_plain_random(self):
        traced = ladx_oracle.Tracer(HERE).Random(SEED)
        self.assertEqual(mixed(traced), mixed(random.Random(SEED)))

    def test_trace_words_are_the_words_drawn(self):
        tracer = ladx_oracle.Tracer(HERE)
        traced = tracer.Random(SEED)
        mixed(traced, rewind=False)
        plain = random.Random(SEED)
        for line in tracer.lines:
            words = int(line.rsplit(" ", 1)[1].strip("[]"))
            for _ in range(words):
                plain.getrandbits(32)
        self.assertEqual([traced.getrandbits(32) for _ in range(4)], [plain.getrandbits(32) for _ in range(4)])

    def test_one_line_per_outermost_call(self):
        tracer = ladx_oracle.Tracer(HERE)
        rng = tracer.Random(b"\x00\x01")
        rng.randint(-1, 8)
        rng.choice("abcdefg")
        seq = list("abcd")
        rng.shuffle(seq)
        rng.choices("abc", (10, 5, 10), k=2)
        rng.uniform(0, 100)
        rng.random()
        rng.getrandbits(33)
        rng.getstate()
        plain = random.Random(b"\x00\x01")
        calls = [
            lambda: plain.randint(-1, 8),
            lambda: plain.choice(range(7)),
            lambda: shuffled(plain, 4),
            lambda: ",".join(map(str, plain.choices(range(3), (10, 5, 10), k=2))),
            lambda: plain.uniform(0, 100).hex(),
            lambda: plain.random().hex(),
            lambda: plain.getrandbits(33),
        ]
        results = []
        for call in calls:
            before = plain.getstate()
            results.append(call())
            results.append(words_between(before, plain.getstate()))
        perm = [int(j) for j in results[4].split(",")]
        self.assertEqual(seq, ["abcd"[j] for j in perm])
        stage = "test_oracle:TracingRandomTest.test_one_line_per_outermost_call"
        self.assertEqual(tracer.lines, [
            "0 rnd %s seed 0001 -> - [0]" % stage,
            "1 rnd %s randint -1 8 -> %s [%d]" % (stage, *results[0:2]),
            "2 rnd %s choice 7 -> %s [%d]" % (stage, *results[2:4]),
            "3 rnd %s shuffle 4 -> %s [%d]" % (stage, *results[4:6]),
            "4 rnd %s choices 3 10,5,10 2 -> %s [%d]" % (stage, *results[6:8]),
            "5 rnd %s uniform 0 100 -> %s [%d]" % (stage, *results[8:10]),
            "6 rnd %s random - -> %s [%d]" % (stage, *results[10:12]),
            "7 rnd %s getrandbits 33 -> %s [%d]" % (stage, *results[12:14]),
            "8 rnd %s getstate - -> - [0]" % stage,
        ])

    def test_uncovered_methods_are_refused(self):
        rng = ladx_oracle.Tracer(HERE).Random(SEED)
        for name, args in (("sample", (range(10), 2)), ("gauss", ()), ("randbytes", (4,)),
                           ("triangular", ()), ("expovariate", (1.0,))):
            with self.assertRaisesRegex(ladx_oracle.OracleError,
                                        "^oracle: RNG method %s is not covered by pyrandom_vectors.txt$" % name):
                getattr(rng, name)(*args)

    def test_install_traces_new_generators_and_the_module_functions(self):
        tracer = ladx_oracle.Tracer(HERE)
        plain = random.Random(SEED)
        original = random.Random
        with tracer.installed():
            rng = random.Random(SEED)
            random.setstate(rng.getstate())
            picked = random.choice("abcdefg")
            with self.assertRaises(ladx_oracle.OracleError):
                random.sample(range(10), 2)
        self.assertIs(random.Random, original)
        self.assertIs(random.choice.__self__, random._inst)
        self.assertEqual(picked, plain.choice("abcdefg"))
        self.assertEqual([line.split(" ")[1] + " " + line.split(" ")[3] for line in tracer.lines],
                         ["rnd seed", "rnd getstate", "global setstate", "global choice"])


class EncoderTest(unittest.TestCase):
    def test_fields(self):
        self.assertEqual(ladx_oracle.field(""), '""')
        self.assertEqual(ladx_oracle.field("Sword lv 1"), "Sword lv 1")
        for bad in ("a\tb", "a\nb", 'say "hi"'):
            with self.assertRaises(ladx_oracle.OracleError):
                ladx_oracle.field(bad)

    def test_item_names(self):
        self.assertEqual(ladx_oracle.item_name("RUPEES_50"), "RUPEES_50")
        for bad in ("", "rupees", "RUPEES 50", "KEY-1"):
            with self.assertRaises(ladx_oracle.OracleError):
                ladx_oracle.item_name(bad)

    def test_numbers_and_seeds(self):
        self.assertEqual(ladx_oracle.render(-1), "-1")
        self.assertEqual(ladx_oracle.render(0.5), "0x1.0000000000000p-1")
        self.assertEqual(ladx_oracle.seed_hex(b""), "-")
        self.assertEqual(ladx_oracle.seed_hex(b"\x0a\xff"), "0aff")
        for bad in (True, "1", None):
            with self.assertRaises(ladx_oracle.OracleError):
                ladx_oracle.render(bad)


class RunTest(unittest.TestCase):
    def test_env_keeps_the_hash_seed_and_stops_bytecode(self):
        env = ladx_oracle.oracle_env(1, {"PATH": "/bin", "PYTHONHASHSEED": "random"})
        self.assertEqual(env, {"PATH": "/bin", "PYTHONHASHSEED": "1", "PYTHONDONTWRITEBYTECODE": "1"})

    def test_enter_checkout(self):
        cwd, path = os.getcwd(), list(sys.path)
        with tempfile.TemporaryDirectory() as tmp:
            checkout = os.path.join(tmp, "ladxr")
            os.mkdir(checkout)
            with self.assertRaisesRegex(ladx_oracle.OracleError, "not a LADXR checkout"):
                ladx_oracle.enter_checkout(checkout, os.path.join(tmp, "work"))
            open(os.path.join(checkout, "randomizer.py"), "w").close()
            try:
                work = ladx_oracle.enter_checkout(checkout, os.path.join(tmp, "work"))
                self.assertEqual(sys.path[0], checkout)
                self.assertEqual(os.getcwd(), os.path.realpath(work))
                self.assertEqual(os.path.dirname(work), os.path.join(tmp, "work"))
                self.assertEqual(os.listdir(work), [])
                self.assertTrue(sys.dont_write_bytecode)
            finally:
                os.chdir(cwd)
                sys.path[:] = path


def plant(root, files):
    for path, text in files.items():
        full = os.path.join(root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "w") as f:
            f.write(text)


class AuditTest(unittest.TestCase):
    def audit(self, files, table):
        with tempfile.TemporaryDirectory() as tmp:
            plant(tmp, files)
            return ladx_oracle.audit_order(tmp, table)

    def test_audited_site_passes(self):
        ok, lines = self.audit({"patches/a.py": "import os\n    names = sorted(os.listdir(d))\n"},
                               "# header\npatches/a.py\tnames = sorted(os.listdir(d))\tpatched\tsorted\n")
        self.assertTrue(ok)
        self.assertEqual(lines, ["order audit: 1 sites, all audited"])

    def test_unaudited_site_is_caught(self):
        ok, lines = self.audit({"a.py": "x = 1\nfor f in os.listdir(d):\n"}, "")
        self.assertFalse(ok)
        self.assertEqual(lines, ["UNAUDITED a.py:2 for f in os.listdir(d):"])

    def test_stale_entry_is_caught(self):
        ok, lines = self.audit({"a.py": "x = 1\n"}, "a.py\tfor f in os.listdir(d):\torder-free\tgone\n")
        self.assertFalse(ok)
        self.assertEqual(lines, ["STALE a.py for f in os.listdir(d):"])

    def test_every_pattern_is_a_site(self):
        source = "".join(line + "\n" for line in (
            "os.listdir(d)", "os.scandir(d)", "os.walk(d)", "import glob", "p.iterdir()",
            "s = set()", "t = frozenset(x)", "for k in {1: 2}.items():", "e.getAccessableLocations()",
            "def set(self, v):", "global flag", "x.set(1)", "reset(1)", "if a in {1, 2}:",
            "ri.getTileset(1)"))
        ok, lines = self.audit({"a.py": source}, "")
        self.assertEqual([line.split(" ", 1)[1].split(" ", 1)[0] for line in lines],
                         ["a.py:%d" % n for n in range(1, 10)])

    def test_identical_lines_share_one_entry_and_www_is_skipped(self):
        ok, lines = self.audit({"a.py": "s = set()\ns = set()\n", "www/b.py": "os.listdir(d)\n",
                                "c.txt": "os.listdir(d)\n"},
                               "a.py\ts = set()\torder-free\tmembership only\n")
        self.assertEqual((ok, lines), (True, ["order audit: 2 sites, all audited"]))

    def test_bad_table_is_refused(self):
        for table in ("a.py\ts = set()\tmaybe\twhy\n", "a.py\ts = set()\tpatched\n",
                      "a.py\ts = set()\tpatched\t\n",
                      "a.py\ts = set()\tpatched\tx\na.py\ts = set()\tunreached\ty\n"):
            with self.assertRaises(ladx_oracle.OracleError):
                self.audit({"a.py": "s = set()\n"}, table)


def stand_in(key, default, options=None, short="s", label="Label", category="Main", aesthetic=False,
             visible_if=None):
    return types.SimpleNamespace(key=key, category=category, short_key=short, label=label,
                                 aesthetic=aesthetic, options=options, default=default,
                                 visible_if=visible_if, value=default)


class SettingsTest(unittest.TestCase):
    def test_settings_lines(self):
        settings = [
            stand_in("race", False, short="V", label="Race mode"),
            stand_in("gfx", "", [("", "", "Default"), ("A.bin", "A.bin>", "A|Ann Name")], short="c",
                     category="User options", label="Graphics", aesthetic=True),
            stand_in("ff", 0.5, short="F", label="Forward Factor"),
            stand_in("count", "4", [("4", "4", "4 things"), ("x", "R", "Random")], short="i",
                     visible_if=["goal", "instruments", "specific"]),
        ]
        self.assertEqual(list(ladx_oracle.settings_lines(settings)), [
            "setting race\tMain\tV\tbool\t0\tRace mode\t0\t-",
            "setting gfx\tUser options\tc\tstr\t\"\"\tGraphics\t1\t-",
            "option gfx\t\"\"\t\"\"\tDefault\t-",
            "option gfx\tA.bin\tA.bin>\tA\tAnn Name",
            "setting ff\tMain\tF\tfloat\t0x1.0000000000000p-1\tForward Factor\t0\t-",
            "setting count\tMain\ti\tstr\t4\tLabel\t0\tgoal,instruments,specific",
            "option count\t4\t4\t4 things\t-",
            "option count\tx\tR\tRandom\t-",
        ])

    def test_settings_lines_refuse_bad_text(self):
        for bad in (stand_in("a", False, label="tab\there"), stand_in("a", None),
                    stand_in("a", "", [("", "", "x")], visible_if=["goal", "a,b"])):
            with self.assertRaises(ladx_oracle.OracleError):
                list(ladx_oracle.settings_lines([bad]))

    TIER1_SETTINGS = [
        stand_in("seed", ""),
        stand_in("logic", "", [("casual", "c", "Casual"), ("", "", "Normal"), ("hard", "h", "Hard")]),
        stand_in("ff", 0.0),
        stand_in("race", False),
        stand_in("music", "", [("", "", "Default"), ("random", "r", "Random"), ("ffa", "f", "FFA")]),
    ]

    def test_read_tier1(self):
        text = "# header\nlogic all\nrace all\nmusic \"\" random\n"
        self.assertEqual(ladx_oracle.read_tier1(text, self.TIER1_SETTINGS),
                         [("logic", ["casual", '""', "hard"]), ("race", ["0", "1"]),
                          ("music", ['""', "random"])])

    def test_read_tier1_refuses_what_ladxr_does_not_define(self):
        for text in ("overworld all\n", "music random nope\n", "ff all\n", "seed \"\"\n",
                     "race all\nrace all\n", "music random random\n", "race all\nlogic all\n",
                     "logic\n"):
            with self.assertRaises(ladx_oracle.OracleError, msg=text):
                ladx_oracle.read_tier1(text, self.TIER1_SETTINGS)


class FakeSettings:
    """LADXR's Settings in miniature: values behind __getattr__/__setattr__, validate printing."""
    deep_read = False

    def __init__(self):
        self._by_key = {s.key: s for s in (
            stand_in("goal", "vanilla", [("vanilla", "", "V"), ("bingo", "b", "B")], short="G"),
            stand_in("race", False, short="V"),
            stand_in("steal", "default", [("default", "", "D"), ("never", "n", "N")], short="t"))}

    def __getattr__(self, item):
        return self.__dict__["_by_key"][item].value

    def __setattr__(self, key, value):
        if not key.startswith("_") and key in self._by_key:
            self._by_key[key].value = value
        else:
            super().__setattr__(key, value)

    def __iter__(self):
        return iter(self._by_key.values())

    def getShortString(self):
        return "".join(s.short_key + ("" if s.default is False else
                                      dict((k, sh) for k, sh, _ in s.options)[s.value])
                       for s in self if s.value != s.default)

    def validate(self):
        if self.goal == "bingo":
            print("Warning: bingo")
            if self.steal == "never":
                if self.deep_read and self.race:
                    pass
                self.steal = "default"


class ValidateTest(unittest.TestCase):
    def test_validate_fixture(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            keys, lines = ladx_oracle.validate_fixture(FakeSettings)
        self.assertEqual(out.getvalue(), "")
        self.assertEqual(keys, ["goal", "steal"])
        self.assertEqual(lines, [
            'validate goal=vanilla -> =\t""',
            "validate goal=bingo -> =\tGb",
            'validate steal=default -> =\t""',
            "validate steal=never -> =\ttn",
            'validate goal=vanilla,steal=default -> =\t""',
            "validate goal=vanilla,steal=never -> =\ttn",
            "validate goal=bingo,steal=default -> =\tGb",
            "validate goal=bingo,steal=never -> steal=default\tGb",
        ])

    def test_a_pair_reading_outside_the_singles_set_is_refused(self):
        deep = type("DeepSettings", (FakeSettings,), {"deep_read": True})
        with self.assertRaisesRegex(ladx_oracle.OracleError, "race"):
            ladx_oracle.validate_fixture(deep)


if __name__ == "__main__":
    unittest.main()
