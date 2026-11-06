"""Usage: python3 oracle_run.py [--out <file>] [--meta <file>] <ladxr dir> <seed hex> [k=v ...]

Runs LADXR's Randomizer in SKIP_ROM_GENERATION mode (no ROM, no plan, no spoiler) on the seed
(hex bytes, - for empty) and the settings (set in order, then validate), and writes (to <file>,
else stdout) its sections in order:
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
PYTHONHASHSEED, hash("ladxr"), the checkout's path and the Python version.
Run python3 without -I or -E (both drop PYTHONHASHSEED).
"""

import argparse
import contextlib
import io
import os
import shutil
import sys

import ladx_oracle

HERE = os.path.dirname(os.path.abspath(__file__))
WORK_ROOT = os.path.join(HERE, "..", "..", "..", "tmp", "ladx-oracle")


class _PoolDone(Exception):
    pass


class Driver:
    """Loads LADXR from a checkout once (chdir into a throwaway directory under tmp/ladx-oracle/),
    hooks it from outside, and runs cases. close() goes back and removes the directory."""

    def __init__(self, ladxr_dir):
        self.ladxr_dir = os.path.abspath(ladxr_dir)
        self.cwd = os.getcwd()
        self.work = ladx_oracle.enter_checkout(self.ladxr_dir, os.path.abspath(WORK_ROOT))
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

    def run(self, seed, assignments, pool_only=False):
        """One case: the output lines (without the trace when pool_only, which stops once
        readItemPool returned and then has outcome ok)."""
        self.case = {"ws": [], "pool": [], "placer_pool": [], "place": [], "pool_only": pool_only}
        del self.tracer.lines[:]
        settings = self.settings.Settings()
        for assignment in assignments:
            settings.set(assignment)
        with contextlib.redirect_stdout(io.StringIO()):
            settings.validate()
        args = argparse.Namespace(input_filename="SKIP_ROM_GENERATION", plan=None, spoilerformat="none",
                                  log_directory=None, output_filename=None)
        outcome = "outcome ok"
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
        lines = ["seed %s" % ladx_oracle.seed_hex(seed),
                 "settings %s" % ladx_oracle.field(settings.getShortString())]
        for section in ("ws", "pool", "placer_pool", "place"):
            lines += self.case[section]
        if not pool_only:
            lines += ["trace " + line for line in self.tracer.lines]
        return lines + [outcome]

    def meta(self):
        return ["hashseed %s" % os.environ.get("PYTHONHASHSEED", "unset"),
                "hash_ladxr %d" % hash("ladxr"),
                "checkout %s" % self.ladxr_dir,
                "python %d.%d.%d" % sys.version_info[:3]]


def parse_seed(text):
    return b"" if text == "-" else bytes.fromhex(text)


def main(argv):
    parser = argparse.ArgumentParser(usage=__doc__.splitlines()[0][len("Usage: "):])
    parser.add_argument("--out")
    parser.add_argument("--meta")
    parser.add_argument("ladxr_dir")
    parser.add_argument("seed")
    parser.add_argument("settings", nargs="*")
    args = parser.parse_args(argv)
    out = os.path.abspath(args.out) if args.out else None
    meta_out = os.path.abspath(args.meta) if args.meta else None
    driver = Driver(args.ladxr_dir)
    try:
        lines = driver.run(parse_seed(args.seed), args.settings)
        meta = driver.meta()
    finally:
        driver.close()
    text = "".join(line + "\n" for line in lines)
    if out:
        with open(out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    else:
        sys.stdout.write(text)
    if meta_out:
        with open(meta_out, "w", encoding="utf-8", newline="\n") as f:
            f.writelines(line + "\n" for line in meta)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
