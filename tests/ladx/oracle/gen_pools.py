"""Usage: python3 gen_pools.py <ladxr dir> <out dir>

Writes <out dir>/item_pools.txt: the item pool LADXR builds for the baseline and for every
non-default tier-1 value alone (tier1.txt), each under three seeds, through oracle_run.py's driver
stopped once readItemPool returned.
"""

import os
import sys

import ladx_oracle
import oracle_run

HERE = os.path.dirname(os.path.abspath(__file__))
SEEDS = (bytes(range(16)), b"LADXR", b"\xff" * 16)

HEADER = """\
# LADXR's item pools; written by gen_pools.py (gen_fixtures.sh). Cases: the baseline, then every
# non-default value of every tier-1 setting alone (tier1.txt, LADXR's order), each under the seeds
# %s, in that order. Each case runs LADXR's Randomizer in SKIP_ROM_GENERATION mode
# (oracle_run.py's driver: the setting set, validate, no plan) and stops once readItemPool returned:
#   case <name> <seed hex> <short settings string after validate, "" when empty>
#   words_before <n>                 MT words the Randomizer's generator had drawn when ItemPool was entered
#   pool <item>=<count> ...          ItemPool.toDict() as built, insertion order, zero counts kept
#   words_after <n>                  MT words drawn when ItemPool returned
#   placer_pool <item>=<count> ...   what readItemPool returns (forced and single-option spots taken out)
# or, when LADXR raises before the pool is built (its Error, or another exception: goal=maze's
# buildMaze can raise IndexError):
#   error <message>                  LADXR's Error
#   crash <exception type> <message> any other exception
# <name>: baseline or <key>=<value>, a bool as 0 or 1, "" for the empty string.
"""


def cases(tier1, settings):
    defaults = {s.key: ladx_oracle.setting_value(s.default) for s in settings}
    yield "baseline", []
    for key, values in tier1:
        for value in values:
            if value != defaults[key]:
                yield "%s=%s" % (key, value), ["%s=%s" % (key, "" if value == '""' else value)]


def main(argv):
    if len(argv) != 3:
        print(__doc__.splitlines()[0], file=sys.stderr)
        return 2
    out_dir = os.path.abspath(argv[2])
    with open(os.path.join(HERE, "tier1.txt"), encoding="utf-8") as f:
        tier1_text = f.read()
    driver = oracle_run.Driver(argv[1])
    lines = []
    try:
        settings = list(driver.settings.Settings())
        for name, assignments in cases(ladx_oracle.read_tier1(tier1_text, settings), settings):
            for seed in SEEDS:
                run = driver.run(seed, assignments, pool_only=True)
                outcome = run[-1]
                lines.append("case %s %s %s" % (name, run[0].split(" ", 1)[1], run[1].split(" ", 1)[1]))
                if outcome != "outcome ok":
                    if name == "baseline":
                        raise ladx_oracle.OracleError("oracle: the baseline fails: " + outcome)
                    lines.append(outcome.split(" ", 1)[1])
                    continue
                lines += [line for line in run if line.split(" ", 1)[0] in
                          ("words_before", "pool", "words_after", "placer_pool")]
    finally:
        driver.close()
    with open(os.path.join(out_dir, "item_pools.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write(HEADER % ", ".join(ladx_oracle.seed_hex(seed) for seed in SEEDS))
        out.writelines(line + "\n" for line in lines)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
