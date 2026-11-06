"""Usage: python3 gen_settings.py <ladxr dir> <out dir>

Writes <out dir>/settings.txt (every setting LADXR's Settings defines, with its options) and
<out dir>/settings_validate.txt (Settings.validate's vectors), after checking tier1.txt against
LADXR's Settings.
"""

import os
import shutil
import sys

import ladx_oracle

HERE = os.path.dirname(os.path.abspath(__file__))
WORK_ROOT = os.path.join(HERE, "..", "..", "..", "tmp", "ladx-oracle")

SETTINGS_HEADER = """\
# Every setting of LADXR's settings.py, tier 1 or not, in LADXR's order, each followed by its
# options in LADXR's order; written by gen_settings.py (gen_fixtures.sh). Tab-separated fields:
#   setting <key>\\t<category>\\t<short>\\t<bool|str|float>\\t<default>\\t<label>\\t<aesthetic 0|1>\\t<visible_if>
#   option <key>\\t<value>\\t<short>\\t<label>\\t<meta>
# Values: a bool as 0 or 1, a float as float.hex, "" for the empty string. <visible_if>: LADXR's
# list comma-separated (the controlling key, then the values that show the setting), - when none.
# <label>/<meta>: the option label split at its first | as toJson does, <meta> - when it has none.
# gfxmod's options are LADXR's own sorted gfx/ names.
"""

VALIDATE_HEADER = """\
# Settings.validate vectors of LADXR's settings.py; written by gen_settings.py (gen_fixtures.sh).
# The settings validate reads or writes, found by recording attribute access while it runs on
# every value of every setting set alone (seed and forwardfactor at their default):
# keys %s
# Vectors: a fresh Settings, the inputs set, then validate (its warning prints dropped); every
# value of each key above alone, then every pair of values of two different keys above.
#   validate <k=v,...> -> <changed k=v,... after validate, in LADXR's order, or => >\\t<getShortString() after>
# Values: a bool as 0 or 1, "" for the empty string (also for an empty short string).
"""


def main(argv):
    if len(argv) != 3:
        print("usage: gen_settings.py <ladxr dir> <out dir>", file=sys.stderr)
        return 2
    out_dir = os.path.abspath(argv[2])
    with open(os.path.join(HERE, "tier1.txt"), encoding="utf-8") as f:
        tier1_text = f.read()
    cwd = os.getcwd()
    work = ladx_oracle.enter_checkout(argv[1], os.path.abspath(WORK_ROOT))
    try:
        import settings
        ladx_oracle.read_tier1(tier1_text, list(settings.Settings()))
        lines = list(ladx_oracle.settings_lines(settings.Settings()))
        keys, vectors = ladx_oracle.validate_fixture(settings.Settings)
    finally:
        os.chdir(cwd)
        shutil.rmtree(work)
    with open(os.path.join(out_dir, "settings.txt"), "w", encoding="utf-8", newline="\n") as out:
        out.write(SETTINGS_HEADER)
        out.writelines(line + "\n" for line in lines)
    with open(os.path.join(out_dir, "settings_validate.txt"), "w", encoding="utf-8",
              newline="\n") as out:
        out.write(VALIDATE_HEADER % " ".join(keys))
        out.writelines(line + "\n" for line in vectors)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
