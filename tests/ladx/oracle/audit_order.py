"""Usage: python3 audit_order.py <ladxr dir> [--table <file>]

Lists every directory listing, set construction, dict-literal loop, explorer visited-set read and
environment read (subprocess, environment variables, clock, working directory, os.urandom,
platform and Python version) in the LADXR checkout's .py files (www/ excepted) and fails unless each one has a verdict in the
audit table (default order_audit.txt next to this script): one UNAUDITED or STALE line per
mismatch, else `order audit: <n> sites, all audited`. It first proves itself on a planted tree.
"""

import importlib.util
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.dont_write_bytecode = True
_spec = importlib.util.spec_from_file_location("ladx_oracle", os.path.join(HERE, "ladx_oracle.py"))
ladx_oracle = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(ladx_oracle)

PLANTED = {
    "patches/audited.py": "names = sorted(os.listdir(d))\n",
    "patches/new.py": "import os\nnames = os.listdir(d)\n",
    "patches/clock.py": "import time\nstamp = time.time()\n",
}
PLANTED_TABLE = ("patches/audited.py\tnames = sorted(os.listdir(d))\tpatched\tsorted\n"
                 "patches/gone.py\tfor f in os.walk(d):\torder-free\tremoved\n")
PLANTED_EXPECTED = (False, ["UNAUDITED patches/clock.py:2 stamp = time.time()",
                            "UNAUDITED patches/new.py:2 names = os.listdir(d)",
                            "STALE patches/gone.py for f in os.walk(d):"])


def self_proof():
    with tempfile.TemporaryDirectory() as tmp:
        for path, text in PLANTED.items():
            full = os.path.join(tmp, path)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "w") as f:
                f.write(text)
        got = ladx_oracle.audit_order(tmp, PLANTED_TABLE)
    if got != PLANTED_EXPECTED:
        sys.exit("order audit: self-proof failed on the planted tree: %r" % (got,))


def main(argv):
    table = os.path.join(HERE, "order_audit.txt")
    args = list(argv)
    if len(args) == 3 and args[1] == "--table":
        table = args.pop(2)
        args.pop(1)
    if len(args) != 1:
        sys.exit(__doc__.splitlines()[0])
    ladxr_dir = args[0]
    if not os.path.isfile(os.path.join(ladxr_dir, "randomizer.py")):
        sys.exit("setup: %s is not a LADXR checkout (run setup.sh)" % ladxr_dir)
    self_proof()
    with open(table, encoding="utf-8") as f:
        ok, lines = ladx_oracle.audit_order(ladxr_dir, f.read())
    print("\n".join(lines))
    return 0 if ok else 1


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except ladx_oracle.OracleError as e:
        sys.exit(str(e))
