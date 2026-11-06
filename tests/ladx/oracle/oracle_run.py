"""Usage: python3 oracle_run.py [--out <file>] [--meta <file>] [--work <dir>] [--rom <file> --dir <dir> [--plain | --reads [--inventory <file>]]] <ladxr dir> <seed hex> [k=v ...]

Runs LADXR's Randomizer in SKIP_ROM_GENERATION mode (no ROM, no plan, no spoiler) on the seed
(hex bytes, - for empty; LADXR's seed setting stays empty) and the settings (set in order, then
validate), and writes (to <file>, else stdout) its sections in order:
  seed <hex>
  settings <short settings string after validate, "" when empty>
  ws <field> <value>                 the world setup after randomize (ladx_oracle.ws_lines)
  words_before <n>                   MT words the generator had drawn when ItemPool was entered
  pool <item>=<count> ...            ItemPool.toDict() as built, in insertion order, zero counts kept
  words_after <n>                    MT words drawn when ItemPool returned
  placer_pool <item>=<count> ...     what readItemPool returns
  place <i> <class> <nameId> <item>  every spot of iteminfo_list once RandomItemPlacer.run returned
  trace <n> <gen> <stage> <method> <args> -> <result> [<words>]   every RNG call (ladx_oracle.Tracer)
  outcome ok | outcome error <LADXR's Error message> | outcome crash <exception type> <message>
A section LADXR did not reach is left out. A crash is any other exception LADXR raises (goal=maze
can raise IndexError from patches/maze.py's buildMaze on some seeds; LADXR's main.py does not
catch it).
--meta writes what must differ between the double run's two runs and is never compared:
PYTHONHASHSEED, hash("ladxr"), the checkout's path, the Python version and the working
directory's `git describe --tags` answer (- when none), read before LADXR runs.
--work is the root under which LADXR runs in a new empty directory (default tmp/ladx-oracle/).

--rom turns on full generation with that ROM, read only through LADXR: the same hooks and tracer,
then generateRom and the JSON spoiler, written into the case directory --dir:
  run.txt       the sections above (place taken before generateRom; trace covering the whole
                generation, spoiler included), then rom_sha1 <hex> | rom none,
                spoiler_sha1 <hex> | spoiler none, before the outcome line
  out.gbc       the output ROM, when the outcome is ok
  spoiler.json  the spoiler, when the outcome is ok and race is off
  meta.txt      as --meta
--reads (with --rom, off by default) also installs read_trace.py over the whole run, spoiler
included, with inventory_check.py's sections and the groups of --inventory (default
notes/ladx-inventory.md), and writes into --dir, whatever the outcome: reads.txt (aggregated
reads: group, site, caller, channel, target, provenance, n= and range) and writes.txt (per
writing site: group, site, n= positions, sha1= of its net writes) and events.txt (the event
stream of read_trace.py's Trace.event_lines: every write in order with its values, group, site
and each byte's origin, a source offset or known, after the parsed entries) and invariant.txt
(invariant_run.py's re-run of every C-group call of generateRom that writes a computed byte, on
a scrambled copy of the ROM state: per event, one flag per byte, write_log.py's
parse_invariant). The other files are the same with or without it.
--plain (with --rom) runs LADXR's own main.main instead, with no hook and no tracer, os.urandom
handing it the seed on its first call; it writes out.gbc, spoiler.json and plain.txt:
rom_sha1 | rom none, spoiler_sha1 | spoiler none, then outcome ok | outcome retry (a second
os.urandom call: the fill failed and main.py would retry) | outcome exit <code> |
outcome crash <exception type> <message>.
Run python3 without -I or -E (both drop PYTHONHASHSEED).
"""

import argparse
import contextlib
import hashlib
import io
import os
import shutil
import subprocess
import sys

import importlib
import inventory_check
import invariant_run
import ladx_oracle
import read_trace

HERE = os.path.dirname(os.path.abspath(__file__))
WORK_ROOT = os.path.join(HERE, "..", "..", "..", "tmp", "ladx-oracle")


class _PoolDone(Exception):
    pass


class _Retry(BaseException):
    pass


def ladxr_args(rom, out_rom, spoiler):
    """The Namespace LADXR's main.py parser gives with no option but the input ROM, -o,
    --spoilerformat json and --spoilerfilename (attributes read from main.py at the pin)."""
    return argparse.Namespace(
        input_filename=rom, output_filename=out_rom, dump=None, spoilerformat="json",
        spoiler_filename=spoiler, test=False, romdebugmode=False, exportmap=None, emptyplan=None,
        gfxtemplate=None, timeout=None, log_directory=None, settings=None, shortsettings=None,
        settingjson=False, plan=None, doubletrouble=False, pymod=None)


def describe_cwd():
    """`git describe --tags` in the working directory, - when it gives nothing."""
    try:
        done = subprocess.run(["git", "describe", "--tags"], stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, check=False)
    except OSError:
        return "-"
    return done.stdout.decode("ascii", "replace").strip() or "-"


def file_records(out_rom, spoiler, ok):
    """The rom_sha1/spoiler_sha1 lines; the two files are removed unless the outcome is ok."""
    lines = []
    for path, tag in ((out_rom, "rom"), (spoiler, "spoiler")):
        if not ok and os.path.exists(path):
            os.remove(path)
        if os.path.exists(path):
            with open(path, "rb") as f:
                lines.append("%s_sha1 %s" % (tag, hashlib.sha1(f.read()).hexdigest()))
        else:
            lines.append("%s none" % tag)
    return lines


class Driver:
    """Loads LADXR from a checkout once (chdir into a throwaway directory under tmp/ladx-oracle/),
    hooks it from outside, and runs cases. close() goes back and removes the directory."""

    def __init__(self, ladxr_dir, work_root=WORK_ROOT):
        self.ladxr_dir = os.path.abspath(ladxr_dir)
        self.cwd = os.getcwd()
        self.work = ladx_oracle.enter_checkout(self.ladxr_dir, os.path.abspath(work_root))
        self.describe = describe_cwd()
        self.tracer = ladx_oracle.Tracer(self.ladxr_dir)
        self._install = self.tracer.installed()
        self._install.__enter__()
        import randomizer
        import settings
        import itempool
        import worldSetup
        import utils
        from patches import bingo
        self.randomizer, self.settings, self.utils = randomizer, settings, utils
        self.identity = worldSetup.WorldSetup().entrance_mapping
        self.bingo_goals = bingo.BINGO_GOALS
        self.case = None
        self._hook(worldSetup.WorldSetup, "randomize", self._after_randomize)
        self._hook_pool(itempool.ItemPool)
        self._hook(randomizer.Randomizer, "readItemPool", self._after_read_pool)
        self._hook(randomizer.RandomItemPlacer, "run", self._after_place)

    def close(self):
        self._install.__exit__(None, None, None)
        os.chdir(self.cwd)
        shutil.rmtree(self.work, ignore_errors=True)

    @staticmethod
    def _hook(cls, name, after):
        original = getattr(cls, name)

        def hooked(self, *args, **kwargs):
            result = original(self, *args, **kwargs)
            after(self, args, result)
            return result
        setattr(cls, name, hooked)

    def _hook_pool(self, cls):
        original = cls.__init__
        driver = self

        def hooked(self, logic, settings, rnd, plando):
            before = rnd._words
            original(self, logic, settings, rnd, plando)
            driver.case["pool"] = ["words_before %d" % before,
                                   ladx_oracle.pool_line("pool", self.toDict()),
                                   "words_after %d" % rnd._words]
        cls.__init__ = hooked

    def _after_randomize(self, ws, args, result):
        self.case["ws"] = list(ladx_oracle.ws_lines(ws, self.identity, self.bingo_goals))

    def _after_read_pool(self, randomizer, args, pool):
        self.case["placer_pool"] = [ladx_oracle.pool_line("placer_pool", pool)]
        if self.case["pool_only"]:
            raise _PoolDone()

    def _after_place(self, placer, args, result):
        self.case["place"] = list(ladx_oracle.place_lines(placer._logic.iteminfo_list))

    def run(self, seed, assignments, pool_only=False, rom=None, case_dir=None, reads=None):
        """One case: the output lines (without the trace when pool_only, which stops once
        readItemPool returned and then has outcome ok). With rom, a full generation writing
        out.gbc and spoiler.json into case_dir, their SHA-1 lines before the outcome; with
        reads too ({site: group}), reads.txt, writes.txt and events.txt as well."""
        self.case = {"ws": [], "pool": [], "placer_pool": [], "place": [], "pool_only": pool_only}
        del self.tracer.lines[:]
        settings = self.settings.Settings()
        for assignment in assignments:
            settings.set(assignment)
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        if rom is None:
            args = argparse.Namespace(input_filename="SKIP_ROM_GENERATION", plan=None, spoilerformat="none",
                                      log_directory=None, output_filename=None)
        else:
            out_rom, spoiler = os.path.join(case_dir, "out.gbc"), os.path.join(case_dir, "spoiler.json")
            args = ladxr_args(rom, out_rom, spoiler)
        outcome = "outcome ok"
        trace = self._trace(reads) if reads is not None and rom is not None else None
        rerun = self._rerun(trace, reads) if trace is not None else None
        try:
            with contextlib.redirect_stdout(io.StringIO()):
                self.randomizer.Randomizer(args, settings, seed=seed)
        except _PoolDone:
            pass
        except self.utils.Error as e:
            outcome = "outcome error %s" % ladx_oracle.field(str(e))
        except ladx_oracle.OracleError:
            raise
        except Exception as e:
            outcome = "outcome crash %s %s" % (type(e).__name__, ladx_oracle.field(str(e)))
        finally:
            if rerun is not None:
                rerun.uninstall()
                write_lines(os.path.join(case_dir, "invariant.txt"), rerun.lines())
            if trace is not None:
                trace.uninstall()
                trace.write(os.path.join(case_dir, "reads.txt"))
                trace.write_writes(os.path.join(case_dir, "writes.txt"))
                trace.write_events(os.path.join(case_dir, "events.txt"))
        lines = ["seed %s" % ladx_oracle.seed_hex(seed),
                 "settings %s" % ladx_oracle.field(settings.getShortString())]
        for section in ("ws", "pool", "placer_pool", "place"):
            lines += self.case[section]
        if not pool_only:
            lines += ["trace " + line for line in self.tracer.lines]
        if rom is not None:
            lines += file_records(out_rom, spoiler, outcome == "outcome ok")
        return lines + [outcome]

    def _rerun(self, trace, reads):
        """invariant_run over the C groups' calls of generateRom (reads: the Reads object)."""
        targets = [(importlib.import_module(module), attr)
                   for module, attr in invariant_run.targets_of(reads.text)]
        return invariant_run.install(trace, targets, invariant_run.sides_of(reads.text),
                                     tracer=self.tracer)

    @staticmethod
    def _trace(groups):
        import rom
        import romTables
        import pointerTable
        modules = {"ROM": rom.ROM, "ROMWithTables": romTables.ROMWithTables,
                   "PointerTable": pointerTable.PointerTable}
        sections = [(f, fn) for f, fn, _ in inventory_check.SECTIONS]
        return read_trace.install(modules, sections, groups)

    def meta(self):
        return ["hashseed %s" % os.environ.get("PYTHONHASHSEED", "unset"),
                "hash_ladxr %d" % hash("ladxr"),
                "checkout %s" % self.ladxr_dir,
                "python %d.%d.%d" % sys.version_info[:3],
                "describe %s" % self.describe]


class Reads(dict):
    """The inventory's {site: group}, with its text (`text`) for the re-run's groups."""

    def __init__(self, text):
        super().__init__(inventory_check.site_groups(text))
        self.text = text


def run_plain(ladxr_dir, work_root, rom, case_dir, seed, assignments):
    """LADXR's own main.main on the case, in a new directory under work_root (removed after),
    os.urandom handing it the seed once: plain.txt's lines."""
    cwd = os.getcwd()
    work = ladx_oracle.enter_checkout(ladxr_dir, os.path.abspath(work_root))
    try:
        return _plain(rom, case_dir, seed, assignments)
    finally:
        os.chdir(cwd)
        shutil.rmtree(work, ignore_errors=True)


def _plain(rom, case_dir, seed, assignments):
    out_rom, spoiler = os.path.join(case_dir, "out.gbc"), os.path.join(case_dir, "spoiler.json")
    calls = []

    def urandom(n):
        calls.append(n)
        if len(calls) > 1:
            raise _Retry()
        return seed
    import main as ladxr_main
    argv = [rom, "-o", out_rom, "--spoilerformat", "json", "--spoilerfilename", spoiler]
    for assignment in assignments:
        argv += ["-s", assignment]
    real_urandom, os.urandom = os.urandom, urandom
    outcome = "outcome ok"
    try:
        with contextlib.redirect_stdout(io.StringIO()):
            ladxr_main.main(argv)
    except _Retry:
        outcome = "outcome retry"
    except SystemExit as e:
        outcome = "outcome exit %s" % (e.code,)
    except Exception as e:
        outcome = "outcome crash %s %s" % (type(e).__name__, ladx_oracle.field(str(e)))
    finally:
        os.urandom = real_urandom
    return file_records(out_rom, spoiler, outcome == "outcome ok") + [outcome]


def parse_seed(text):
    return b"" if text == "-" else bytes.fromhex(text)


def write_lines(path, lines):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.writelines(line + "\n" for line in lines)


def main(argv):
    parser = argparse.ArgumentParser(usage=__doc__.splitlines()[0][len("Usage: "):])
    parser.add_argument("--out")
    parser.add_argument("--meta")
    parser.add_argument("--work", default=WORK_ROOT)
    parser.add_argument("--rom")
    parser.add_argument("--dir")
    parser.add_argument("--plain", action="store_true")
    parser.add_argument("--reads", action="store_true")
    parser.add_argument("--inventory", default=inventory_check.DEFAULT_INVENTORY)
    parser.add_argument("ladxr_dir")
    parser.add_argument("seed")
    parser.add_argument("settings", nargs="*")
    args = parser.parse_args(argv)
    if (args.rom is None) != (args.dir is None) or (args.plain and args.rom is None) or \
            (args.rom is not None and (args.out or args.meta)):
        parser.error("--rom and --dir go together, --plain needs them, and they replace --out and --meta")
    if args.reads and (args.rom is None or args.plain):
        parser.error("--reads needs --rom and not --plain")
    reads = None
    if args.reads:
        with open(args.inventory, "rb") as f:
            reads = Reads(f.read().decode("utf-8"))
    work = os.path.abspath(args.work)
    if args.rom is not None:
        rom, case_dir = os.path.abspath(args.rom), os.path.abspath(args.dir)
        os.makedirs(case_dir, exist_ok=True)
        if args.plain:
            write_lines(os.path.join(case_dir, "plain.txt"),
                        run_plain(args.ladxr_dir, work, rom, case_dir, parse_seed(args.seed), args.settings))
            return 0
        out = os.path.join(case_dir, "run.txt")
        meta_out = os.path.join(case_dir, "meta.txt")
    else:
        rom = case_dir = None
        out = os.path.abspath(args.out) if args.out else None
        meta_out = os.path.abspath(args.meta) if args.meta else None
    driver = Driver(args.ladxr_dir, work)
    try:
        lines = driver.run(parse_seed(args.seed), args.settings, rom=rom, case_dir=case_dir,
                           reads=reads)
        meta = driver.meta()
    finally:
        driver.close()
    if out:
        write_lines(out, lines)
    else:
        sys.stdout.write("".join(line + "\n" for line in lines))
    if meta_out:
        write_lines(meta_out, meta)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
