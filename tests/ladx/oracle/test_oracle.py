"""Usage: python3 -I test_oracle.py

Unit tests of ladx_oracle.py, the LADX oracle's shared helpers, with no LADXR and no network.
"""

import contextlib
import importlib.util
import io
import os
import random
import subprocess
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

    def test_every_environment_read_is_a_site(self):
        source = "".join(line + "\n" for line in (
            "v = subprocess.run(['git'])", "os.environ['X']", "os.getenv('X')", "datetime.now()",
            "time.sleep(1)", "os.urandom(16)", "os.getcwd()", "platform.system()", "sys.version_info",
            "sys.platform", "uuid.uuid4()", "socket.gethostname()", "socketserver.TCPServer(a)",
            "from datetime import datetime", "from time import time",
            "import subprocess", "import time", "# into the socket", "the second platform",
            "very time consuming.", "self.runtime.x", "x = mytime.now()"))
        ok, lines = self.audit({"a.py": source}, "")
        self.assertEqual([line.split(" ", 1)[1].split(" ", 1)[0] for line in lines],
                         ["a.py:%d" % n for n in range(1, 16)])

    def test_environment_sites_take_the_inert_verdict_and_go_stale(self):
        source = "x = 1\nfilename = datetime.now()\n"
        table = "a.py\tfilename = datetime.now()\tinert\tonly with --logdirectory\n"
        self.assertEqual(self.audit({"a.py": source}, table), (True, ["order audit: 1 sites, all audited"]))
        self.assertEqual(self.audit({"a.py": source}, ""),
                         (False, ["UNAUDITED a.py:2 filename = datetime.now()"]))
        self.assertEqual(self.audit({"a.py": "x = 1\n"}, table),
                         (False, ["STALE a.py filename = datetime.now()"]))

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


class WorldSetupTest(unittest.TestCase):
    IDENTITY = {"d1": "d1:inside", "d1:inside": "d1"}
    GOALS = [object(), object(), object()]

    def world(self, **fields):
        ws = types.SimpleNamespace(entrance_mapping=dict(self.IDENTITY), boss_mapping=[0, 2, 1],
                                   miniboss_mapping={0: "HINOX", "c1": "AVALAUNCH"}, goal="=1234",
                                   goal_count=4, bingo_goals=None, sign_maze=None, multichest="BOMB",
                                   map=None, inside_to_outside=True, one_on_one=False)
        for key, value in fields.items():
            setattr(ws, key, value)
        return ws

    def test_every_field_in_order(self):
        self.assertEqual(list(ladx_oracle.ws_lines(self.world(), self.IDENTITY, self.GOALS)), [
            "ws entrance_mapping identity", "ws boss_mapping 0,2,1", "ws miniboss_mapping 0=HINOX,c1=AVALAUNCH",
            "ws goal =1234", "ws goal_count 4", "ws bingo_goals -", "ws sign_maze -", "ws multichest BOMB",
            "ws map -", "ws inside_to_outside 1", "ws one_on_one 0"])

    def test_bingo_goals_maze_and_a_shuffled_mapping(self):
        ws = self.world(bingo_goals=[self.GOALS[2], self.GOALS[0]], sign_maze=([(27, 84), (30, 84)], [3, 0x82]),
                        entrance_mapping={"d1": "d2:inside", "d2:inside": "d1"})
        lines = list(ladx_oracle.ws_lines(ws, self.IDENTITY, self.GOALS))
        self.assertEqual(lines[0], "ws entrance_mapping d1=d2:inside,d2:inside=d1")
        self.assertIn("ws bingo_goals 2,0", lines)
        self.assertIn("ws sign_maze 27:84,30:84;3,130", lines)

    def test_unrenderable_fields_are_refused(self):
        for fields in ({"map": object()}, {"bingo_goals": [object()]}, {"goal": "a b"}, {"boss_mapping": []},
                       {"multichest": 0.5}):
            with self.assertRaises(ladx_oracle.OracleError, msg=fields):
                list(ladx_oracle.ws_lines(self.world(**fields), self.IDENTITY, self.GOALS))


class PoolTest(unittest.TestCase):
    def test_pool_line_keeps_order_and_zero_counts(self):
        self.assertEqual(ladx_oracle.pool_line("pool", {"SWORD": 2, "BOMB": 0, "KEY1": 3}),
                         "pool SWORD=2 BOMB=0 KEY1=3")

    def test_pool_line_refuses_bad_names_and_counts(self):
        for pool in ({"sword": 1}, {"SWORD": "1"}, {"SWORD": True}):
            with self.assertRaises(ladx_oracle.OracleError, msg=pool):
                ladx_oracle.pool_line("pool", pool)

    def test_place_lines(self):
        class Chest:
            nameId, item = "0x011", "SWORD"
        spot = Chest()
        empty = types.SimpleNamespace(nameId="None", item=None)
        self.assertEqual(list(ladx_oracle.place_lines([spot, empty])),
                         ["place 0 Chest 0x011 SWORD", "place 1 SimpleNamespace None -"])


class AND:
    def __init__(self, items, children):
        self.__items, self.__children = items, children


class OR:
    def __init__(self, items, children):
        self.__items, self.__children = items, children


class COUNT:
    def __init__(self, item, amount):
        self.__item, self.__amount = item, amount


class COUNTS:
    def __init__(self, items, amount):
        self.__items, self.__amount = items, amount


class FOUND:
    def __init__(self, item, amount):
        self.__item, self.__amount = item, amount


class RequirementTest(unittest.TestCase):
    def test_every_requirement_type(self):
        req = OR(["SWORD", "BOW"], [AND(["FEATHER"], [COUNT("SHIELD", 2)]), COUNTS(["INSTRUMENT1", "INSTRUMENT2"], 1),
                                    FOUND("SEASHELL", 20)])
        self.assertEqual(ladx_oracle.requirement_text(req),
                         "or(SWORD,BOW,and(FEATHER,count(SHIELD,2)),counts(1,INSTRUMENT1,INSTRUMENT2),"
                         "found(SEASHELL,20))")
        for value, text in ((None, "-"), (True, "T"), (False, "F"), ("MEDICINE2", "MEDICINE2")):
            self.assertEqual(ladx_oracle.requirement_text(value), text)

    def test_unknown_types_and_bad_names_are_refused(self):
        class XOR:
            pass
        for req in (XOR(), 3, "sword", AND(["SWORD"], [XOR()]), COUNT("SWORD", True)):
            with self.assertRaises(ladx_oracle.OracleError, msg=req):
                ladx_oracle.requirement_text(req)


class Chest:
    def __init__(self, room, location, options):
        self.nameId, self._location, self.options = room, location, options
        self.metadata = types.SimpleNamespace(name="Tarin's Gift", area="Mabe Village")

    def getOptions(self):
        return self.options


class GraphTest(unittest.TestCase):
    def logic(self):
        start = types.SimpleNamespace(name="Start House", dungeon=None, items=[], connections=[])
        room = types.SimpleNamespace(name=None, dungeon=1, items=[], connections=[])
        start.connections = [(room, AND(["SWORD"], [])), (start, None)]
        room.connections = [(start, True)]
        room.items = [Chest("0x2A3", room, ["SWORD", "KEY1"])]
        return types.SimpleNamespace(location_list=[start, room], iteminfo_list=room.items)

    def test_graph_lines(self):
        self.assertEqual(list(ladx_oracle.graph_lines(self.logic())), [
            "loc 0 -\tStart House", "loc 1 1\t-",
            "spot 1 Chest 0x2A3 SWORD,KEY1\tTarin's Gift\tMabe Village",
            "edge 0 1 and(SWORD)", "edge 0 0 -", "edge 1 0 T"])

    def test_a_location_outside_the_list_is_refused(self):
        logic = self.logic()
        logic.location_list[1].connections = [(types.SimpleNamespace(), None)]
        with self.assertRaises(ladx_oracle.OracleError):
            list(ladx_oracle.graph_lines(logic))

    def test_index_ranges(self):
        self.assertEqual(ladx_oracle.index_ranges([5, 0, 1, 2, 7, 8, 10]), "0-2,5,7-8,10")
        self.assertEqual(ladx_oracle.index_ranges([]), "-")


class CompareTest(unittest.TestCase):
    RUN = "seed 00\nplace 0 Chest 0x011 SWORD\ntrace 0 rnd randomizer:Randomizer.__init__ choice 3 -> 1 [1]\n"

    def test_identical_runs_are_the_same(self):
        self.assertEqual(ladx_oracle.compare_runs("baseline", self.RUN, self.RUN), "same baseline")

    def test_first_differing_line_is_shown(self):
        other = self.RUN.replace("-> 1 [1]", "-> 2 [1]")
        self.assertEqual(ladx_oracle.compare_runs("race", self.RUN, other),
                         "DIFFERENT race: line 3: trace 0 rnd randomizer:Randomizer.__init__ choice 3 -> 1 [1]"
                         " | trace 0 rnd randomizer:Randomizer.__init__ choice 3 -> 2 [1]")

    def test_a_shorter_run_differs_at_its_end(self):
        self.assertEqual(ladx_oracle.compare_runs("x", self.RUN, "seed 00\n"),
                         "DIFFERENT x: line 2: place 0 Chest 0x011 SWORD | <end of file>")


    def test_runs_varied_needs_another_hash_and_another_checkout(self):
        a = "hashseed 0\nhash_ladxr 12\ncheckout /a/ladxr\npython 3.13.1\n"
        b = "hashseed 1\nhash_ladxr -7\ncheckout /b/ladxr\npython 3.13.1\n"
        self.assertTrue(ladx_oracle.runs_varied(a, b))
        self.assertFalse(ladx_oracle.runs_varied(a, b.replace("-7", "12")))
        self.assertFalse(ladx_oracle.runs_varied(a, b.replace("/b/", "/a/")))
        self.assertFalse(ladx_oracle.runs_varied(a, "hashseed 1\n"))

    def test_runs_varied_with_the_describe_key(self):
        a = "hashseed 0\nhash_ladxr 12\ncheckout /a/ladxr\npython 3.13.1\ndescribe -\n"
        b = "hashseed 1\nhash_ladxr -7\ncheckout /b/ladxr\npython 3.13.1\ndescribe planted-tag\n"
        self.assertTrue(ladx_oracle.runs_varied(a, b, describe=True))
        self.assertTrue(ladx_oracle.runs_varied(a, b.replace("planted-tag", "-")))
        self.assertFalse(ladx_oracle.runs_varied(a, b.replace("planted-tag", "-"), describe=True))
        self.assertFalse(ladx_oracle.runs_varied(a.replace("describe -\n", ""), b, describe=True))


class CaseListTest(unittest.TestCase):
    LIST = ("# header\n\nbaseline 000102\n"
            "race 4c41 race=1\n"
            "hell 0123 logic=hell dungeon_keys=keysanity\n")

    def test_valid_lines_comments_and_blank_lines(self):
        self.assertEqual(ladx_oracle.read_cases(self.LIST), [
            ("baseline", bytes.fromhex("000102"), []),
            ("race", bytes.fromhex("4c41"), ["race=1"]),
            ("hell", bytes.fromhex("0123"), ["logic=hell", "dungeon_keys=keysanity"])])

    def test_combo_names_join_assignments_with_plus(self):
        self.assertEqual(ladx_oracle.read_cases("logic-hell+goal-5-8 00 logic=hell goal=5-8\n"),
                         [("logic-hell+goal-5-8", b"\x00", ["logic=hell", "goal=5-8"])])

    def test_name_selection_keeps_the_list_order(self):
        self.assertEqual([c[0] for c in ladx_oracle.read_cases(self.LIST, ["hell", "baseline"])],
                         ["baseline", "hell"])

    def test_an_unknown_name_is_refused(self):
        with self.assertRaisesRegex(ladx_oracle.OracleError, "no case nope"):
            ladx_oracle.read_cases(self.LIST, ["baseline", "nope"])

    def test_bad_lines_are_refused(self):
        for text in ("a 00\na 01\n", "a 001\n", "a 0g\n", "a\n", "a - x=1\n", "a 00 race\n",
                     "a 00 =1\n", " a 00\n", "a  00\n", "a/b 00\n"):
            with self.assertRaises(ladx_oracle.OracleError, msg=text):
                ladx_oracle.read_cases(text)


class CompareBytesTest(unittest.TestCase):
    def test_equal(self):
        self.assertEqual(ladx_oracle.compare_bytes("x", "out.gbc", b"abc", b"abc"), "same x")

    def test_sizes(self):
        self.assertEqual(ladx_oracle.compare_bytes("x", "out.gbc", b"abc", b"ab"),
                         "DIFFERENT x: out.gbc: sizes 3 vs 2")

    def test_one_flipped_byte(self):
        a = bytes(0x200)
        b = bytearray(a)
        b[0x123] ^= 1
        self.assertEqual(ladx_oracle.compare_bytes("x", "out.gbc", a, bytes(b)),
                         "DIFFERENT x: out.gbc: 1 bytes differ, first at 0x123")

    def test_several_differences_and_no_byte_value(self):
        a = bytes([0x11] * 0x40)
        b = bytearray(a)
        for i, value in ((0x30, 0xd7), (0x08, 0xe5), (0x3f, 0xc9)):
            b[i] = value
        verdict = ladx_oracle.compare_bytes("x", "spoiler.json", a, bytes(b))
        self.assertEqual(verdict, "DIFFERENT x: spoiler.json: 3 bytes differ, first at 0x8")
        for value in ("11", "d7", "e5", "c9", "D7", "E5", "C9", "215", "229", "201"):
            self.assertNotIn(value, verdict)


def plant_bytes(root, files):
    for path, data in files.items():
        full = os.path.join(root, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)


RUN_TXT = b"seed 00\ntrace 0 rnd x:y choice 3 -> 1 [1]\nrom_sha1 ab\nspoiler_sha1 cd\noutcome ok\n"
CASE_DIR = {"run.txt": RUN_TXT, "out.gbc": bytes(64), "spoiler.json": b'{"a": 1}', "meta.txt": b"hashseed 0\n"}


class CompareDirsTest(unittest.TestCase):
    def compare(self, a, b):
        with tempfile.TemporaryDirectory() as tmp:
            plant_bytes(os.path.join(tmp, "a"), a)
            plant_bytes(os.path.join(tmp, "b"), b)
            return ladx_oracle.compare_dirs("x", os.path.join(tmp, "a"), os.path.join(tmp, "b"))

    def changed(self, **files):
        out = dict(CASE_DIR)
        for name, data in files.items():
            name = name.replace("_", ".")
            if data is None:
                del out[name]
            else:
                out[name] = data
        return out

    def test_all_equal_and_meta_is_never_compared(self):
        self.assertEqual(self.compare(CASE_DIR, self.changed(meta_txt=b"hashseed 1\n")), ["same x"])

    def test_a_file_only_in_a_or_in_b(self):
        self.assertEqual(self.compare(CASE_DIR, self.changed(spoiler_json=None)),
                         ["DIFFERENT x: only in A: spoiler.json"])
        self.assertEqual(self.compare(self.changed(spoiler_json=None), self.changed(extra=b"")),
                         ["DIFFERENT x: only in B: extra", "DIFFERENT x: only in B: spoiler.json"])

    def test_run_txt_through_compare_runs(self):
        self.assertEqual(self.compare(CASE_DIR, self.changed(run_txt=RUN_TXT.replace(b"-> 1", b"-> 2"))),
                         ["DIFFERENT x: run.txt: line 2: trace 0 rnd x:y choice 3 -> 1 [1]"
                          " | trace 0 rnd x:y choice 3 -> 2 [1]"])

    def test_binary_differences_one_line_per_file(self):
        rom = bytearray(64)
        rom[9] = 1
        self.assertEqual(self.compare(CASE_DIR, self.changed(out_gbc=bytes(rom), spoiler_json=b'{"a": 2}')),
                         ["DIFFERENT x: out.gbc: 1 bytes differ, first at 0x9",
                          "DIFFERENT x: spoiler.json: 1 bytes differ, first at 0x6"])


class ComparePlainTest(unittest.TestCase):
    def compare(self, driver_outcome, plain_outcome, driver_files=None, plain_files=None):
        files = {"out.gbc": bytes(64), "spoiler.json": b"{}"}
        driver = dict(files if driver_files is None else driver_files)
        driver["run.txt"] = b"seed 00\nrom none\nspoiler none\noutcome " + driver_outcome.encode() + b"\n"
        driver["meta.txt"] = b"hashseed 0\n"
        plain = dict(files if plain_files is None else plain_files)
        plain["plain.txt"] = b"rom none\nspoiler none\noutcome " + plain_outcome.encode() + b"\n"
        with tempfile.TemporaryDirectory() as tmp:
            plant_bytes(os.path.join(tmp, "d"), driver)
            plant_bytes(os.path.join(tmp, "p"), plain)
            return ladx_oracle.compare_plain("x", os.path.join(tmp, "d"), os.path.join(tmp, "p"))

    def test_equivalent_outcomes(self):
        self.assertEqual(self.compare("ok", "ok"), ["same x (plain)"])
        self.assertEqual(self.compare("ok", "ok", {"out.gbc": b"r"}, {"out.gbc": b"r"}), ["same x (plain)"])
        self.assertEqual(self.compare("error Item placement failed", "retry", {}, {}), ["same x (plain)"])
        self.assertEqual(self.compare("crash IndexError list index out of range",
                                      "crash IndexError list index out of range", {}, {}), ["same x (plain)"])

    def test_outcome_mismatches(self):
        for driver, plain in (("ok", "retry"), ("error Failed", "ok"), ("crash IndexError a", "crash KeyError a"),
                              ("ok", "exit 1"), ("error Failed", "exit 1"), ("crash IndexError a", "retry")):
            self.assertEqual(self.compare(driver, plain, {}, {}),
                             ["DIFFERENT x (plain): outcome %s vs %s" % (driver, plain)])

    def test_file_mismatches(self):
        self.assertEqual(self.compare("ok", "ok", {"out.gbc": b"ab"}, {"out.gbc": b"ac"}),
                         ["DIFFERENT x (plain): out.gbc: 1 bytes differ, first at 0x1"])
        self.assertEqual(self.compare("ok", "ok", {"out.gbc": b"a", "spoiler.json": b"{}"}, {"out.gbc": b"a"}),
                         ["DIFFERENT x (plain): spoiler.json only in the driver run"])
        self.assertEqual(self.compare("error Failed", "retry", {}, {"out.gbc": b"a"}),
                         ["DIFFERENT x (plain): out.gbc only in the plain run"])

    def test_a_record_without_an_outcome(self):
        with tempfile.TemporaryDirectory() as tmp:
            plant_bytes(os.path.join(tmp, "d"), {"run.txt": b"seed 00\noutcome ok\n"})
            plant_bytes(os.path.join(tmp, "p"), {"plain.txt": b"rom none\n"})
            self.assertEqual(ladx_oracle.compare_plain("x", os.path.join(tmp, "d"), os.path.join(tmp, "p")),
                             ["DIFFERENT x (plain): no outcome in plain.txt"])


class InsideTmpTest(unittest.TestCase):
    def test_paths(self):
        with tempfile.TemporaryDirectory() as tmp:
            repo = os.path.join(tmp, "repo")
            os.makedirs(os.path.join(repo, "tmp", "a"))
            os.makedirs(os.path.join(tmp, "elsewhere"))
            os.symlink(os.path.join(tmp, "elsewhere"), os.path.join(repo, "tmp", "out"))
            os.symlink(os.path.join(repo, "tmp", "a"), os.path.join(tmp, "via"))
            cases = {
                os.path.join(repo, "tmp", "a"): True,
                os.path.join(repo, "tmp", "new", "deeper"): True,
                os.path.join(tmp, "via", "x"): True,
                os.path.join(repo, "tmp"): False,
                os.path.join(repo, "tmp2"): False,
                os.path.join(repo, "docs"): False,
                os.path.join(repo, "tmp", "..", "docs"): False,
                os.path.join(repo, "tmp", "a", "..", "..", "x"): False,
                os.path.join(repo, "tmp", "out"): False,
                os.path.join(repo, "tmp", "out", "x"): False,
            }
            for path, inside in cases.items():
                self.assertEqual(ladx_oracle.inside_tmp(path, repo), inside, msg=path)


class MainTest(unittest.TestCase):
    def main(self, *argv):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = ladx_oracle.main(list(argv))
        return code, out.getvalue()

    def test_compare_subcommands(self):
        with tempfile.TemporaryDirectory() as tmp:
            plant_bytes(tmp, {"a/run.txt": b"seed 00\noutcome ok\n", "a/out.gbc": b"ab",
                              "b/run.txt": b"seed 00\noutcome ok\n", "b/out.gbc": b"ac",
                              "p/plain.txt": b"rom none\noutcome ok\n", "p/out.gbc": b"ab"})
            a, b, p = (os.path.join(tmp, d) for d in "abp")
            self.assertEqual(self.main("compare-bytes", "x", "out.gbc", a + "/out.gbc", a + "/out.gbc"),
                             (0, "same x\n"))
            self.assertEqual(self.main("compare-bytes", "x", "out.gbc", a + "/out.gbc", b + "/out.gbc"),
                             (1, "DIFFERENT x: out.gbc: 1 bytes differ, first at 0x1\n"))
            self.assertEqual(self.main("compare-dirs", "x", a, a), (0, "same x\n"))
            self.assertEqual(self.main("compare-dirs", "x", a, b),
                             (1, "DIFFERENT x: out.gbc: 1 bytes differ, first at 0x1\n"))
            self.assertEqual(self.main("compare-plain", "x", a, p), (0, "same x (plain)\n"))
            self.assertEqual(self.main("compare-plain", "x", b, p),
                             (1, "DIFFERENT x (plain): out.gbc: 1 bytes differ, first at 0x1\n"))

    def test_varied_cases_and_inside_tmp(self):
        with tempfile.TemporaryDirectory() as tmp:
            plant(tmp, {"a.meta": "hash_ladxr 1\ncheckout /a\ndescribe -\n",
                        "b.meta": "hash_ladxr 2\ncheckout /b\ndescribe -\n",
                        "cases.txt": "# c\nbase 0001\nrace 0a0B race=1 goal=bingo\n",
                        "repo/tmp/.keep": ""})
            meta = (os.path.join(tmp, "a.meta"), os.path.join(tmp, "b.meta"))
            self.assertEqual(self.main("varied", *meta), (0, ""))
            self.assertEqual(self.main("varied", "--describe", *meta), (1, ""))
            cases = os.path.join(tmp, "cases.txt")
            self.assertEqual(self.main("cases", cases), (0, "base 0001\nrace 0a0b race=1 goal=bingo\n"))
            self.assertEqual(self.main("cases", cases, "race"), (0, "race 0a0b race=1 goal=bingo\n"))
            repo = os.path.join(tmp, "repo")
            self.assertEqual(self.main("inside-tmp", repo, os.path.join(repo, "tmp", "x")), (0, ""))
            self.assertEqual(self.main("inside-tmp", repo, os.path.join(repo, "x")), (1, ""))


STUB_LADXR = {
    "randomizer.py": """\
import generator


class Randomizer:
    def __init__(self, args, settings, *, seed=None):
        rom = generator.generateRom(args, settings, seed, None)
        rom.save(args.output_filename, name="STUB")
        with open(args.spoiler_filename, "w") as f:
            f.write('{"first": %d}\\n' % rom.banks[0][0])

    def readItemPool(self):
        pass


class RandomItemPlacer:
    def run(self, rnd):
        pass
""",
    "settings.py": """\
class Settings:
    def __init__(self):
        self.values = {}

    def set(self, text):
        key, value = text.split("=")
        self.values[key] = value

    def validate(self):
        pass

    def getShortString(self):
        return ""
""",
    "itempool.py": "class ItemPool:\n    def __init__(self, logic, settings, rnd, plando):\n"
                   "        pass\n",
    "worldSetup.py": "class WorldSetup:\n    def __init__(self):\n        self.entrance_mapping = {}\n"
                     "\n    def randomize(self):\n        pass\n",
    "utils.py": "class Error(Exception):\n    pass\n",
    "patches/__init__.py": "",
    "patches/bingo.py": "BINGO_GOALS = []\n",
    "rom.py": """\
class ROM:
    def __init__(self, filestream):
        self.banks = []
        for n in range(4):
            self.banks.append(bytearray(filestream.read(0x100)))

    def patch(self, bank_nr, addr, old, new):
        self.banks[bank_nr][addr:addr + len(new)] = new

    def save(self, file, *, name=None):
        with open(file, "wb") as f:
            for bank in self.banks:
                f.write(bank)
""",
    "pointerTable.py": """\
class PointerTable:
    def __init__(self, rom):
        self.data = [rom.banks[3][0x10], rom.banks[3][0x11]]

    def __getitem__(self, item):
        return self.data[item]

    def __setitem__(self, item, value):
        self.data[item] = value

    def __len__(self):
        return len(self.data)
""",
    "romTables.py": """\
from rom import ROM
from pointerTable import PointerTable


class ROMWithTables(ROM):
    def __init__(self, filestream):
        super().__init__(filestream)
        self.texts = PointerTable(self)
""",
    "generator.py": """\
from romTables import ROMWithTables


def generateRom(args, settings, seed, logic, *, rnd=None):
    rom = ROMWithTables(open(args.input_filename, "rb"))
    rom.banks[1][0x10] = rom.banks[2][0x20]
    if settings.values.get("crash") == "1":
        raise KeyError("stub")
    rom.texts[0] = rom.texts[1]
    return rom
""",
}

COPY_LINE = "rom.banks[1][0x10] = rom.banks[2][0x20]"


def _stub_inventory():
    spec = importlib.util.spec_from_file_location("inventory_check",
                                                  os.path.join(HERE, "inventory_check.py"))
    inventory_check = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(inventory_check)
    columns = inventory_check.COLUMNS
    cells = {c: "-" for c in columns}
    cells.update({"Call": "`%s`" % COPY_LINE, "Notes": "", columns[-2]: "C",
                  columns[-1]: "g-copy"})
    return "\n".join(["## generator.py generateRom", "", "| " + " | ".join(columns) + " |",
                      "|" + "---|" * len(columns),
                      "| " + " | ".join(cells[c] for c in columns) + " |", ""])


class OracleRunReadsTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp(prefix="oracle-run-test-")
        self.addCleanup(__import__("shutil").rmtree, self.dir)
        self.ladxr = os.path.join(self.dir, "ladxr")
        plant(self.ladxr, STUB_LADXR)
        self.rom = os.path.join(self.dir, "base.gbc")
        with open(self.rom, "wb") as f:
            f.write(bytes(range(256)) * 4)
        self.inventory = os.path.join(self.dir, "inventory.md")
        with open(self.inventory, "w") as f:
            f.write(_stub_inventory())

    def run_case(self, name, *extra, settings=()):
        case = os.path.join(self.dir, name)
        proc = subprocess.run(
            [sys.executable, "-B", os.path.join(HERE, "oracle_run.py"), "--rom", self.rom,
             "--dir", case, "--work", os.path.join(self.dir, "work"),
             "--inventory", self.inventory] + list(extra) + [self.ladxr, "00"] + list(settings),
            capture_output=True, text=True)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        return case

    def files(self, case):
        found = {}
        for name in sorted(os.listdir(case)):
            if name != "meta.txt":
                with open(os.path.join(case, name), "rb") as f:
                    found[name] = f.read()
        return found

    def test_without_the_flag_no_trace_and_with_it_the_same_outputs(self):
        plain = self.files(self.run_case("plain"))
        traced = self.files(self.run_case("traced", "--reads"))
        self.assertEqual(sorted(plain), ["out.gbc", "run.txt", "spoiler.json"])
        self.assertEqual(sorted(traced), ["out.gbc", "reads.txt", "run.txt", "spoiler.json",
                                          "writes.txt"])
        for name in plain:
            self.assertEqual(traced[name], plain[name], name)
        self.assertTrue(plain["run.txt"].endswith(b"outcome ok\n"))

    def test_the_trace_tags_reads_and_writes_with_the_inventory_groups(self):
        traced = self.files(self.run_case("traced", "--reads"))
        site = "generator.py generateRom\t" + COPY_LINE
        reads = traced["reads.txt"].decode().splitlines()
        self.assertIn("g-copy\t%s\tgenerator.py:generateRom\tindex\tbank 0x02\tsource\t"
                      "n=1 0x0020..0x0020" % site, reads)
        self.assertIn("-\trandomizer.py Randomizer.__init__\tf.write('{\"first\": %d}\\n' % "
                      "rom.banks[0][0])\trandomizer.py:Randomizer.__init__\tindex\tbank 0x00\t"
                      "source\tn=1 0x0000..0x0000", reads)
        writes = traced["writes.txt"].decode().splitlines()
        self.assertEqual([l.rsplit("\t", 1)[0] for l in writes],
                         ["-\tgenerator.py generateRom\trom.texts[0] = rom.texts[1]\tn=1",
                          "g-copy\t%s\tn=1" % site])

    def test_a_crash_still_writes_the_trace_up_to_where_it_stopped(self):
        case = self.run_case("crash", "--reads", settings=["crash=1"])
        traced = self.files(case)
        self.assertTrue(traced["run.txt"].endswith(b"outcome crash KeyError 'stub'\n"),
                        traced["run.txt"])
        writes = traced["writes.txt"].decode().splitlines()
        self.assertEqual([l.split("\t")[0] for l in writes], ["g-copy"])
        self.assertNotIn(b"table texts", traced["reads.txt"])

    def test_reads_needs_rom_and_not_plain(self):
        for extra in (["--reads", "--out", os.path.join(self.dir, "x")],
                      ["--rom", self.rom, "--dir", self.dir, "--plain", "--reads"]):
            proc = subprocess.run([sys.executable, "-B", os.path.join(HERE, "oracle_run.py")]
                                  + extra + [self.ladxr, "00"], capture_output=True, text=True)
            self.assertEqual(proc.returncode, 2, extra)


if __name__ == "__main__":
    unittest.main()
