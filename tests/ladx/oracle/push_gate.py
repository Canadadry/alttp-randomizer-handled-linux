"""Usage: python3 -I tests/ladx/oracle/push_gate.py [--install | --uninstall | --self-test]
       python3 -I tests/ladx/oracle/push_gate.py <pre-push arguments>   (ref lines on stdin)

Phase 2's push gate, a pre-push hook chained after .claude/hooks/no_claude_branch.sh. It refuses
a push (exit 1) when the link held in LADX_ROM_URL is in `git log -p --text` of a pushed commit,
when a blob the push uploads holds a literal run of the ROM under nintendo_bytes.py's policy (scope
from the blob's path; the `computed` clearance from the working tree's notes/ladx-inventory.md,
a blob that needs it refused when it is missing; the `invariant` clearance from the blob's own
`I` lines), or when an untracked file sits outside the git-ignored paths (docs/prd/
allowed, but searched for the link). It fails closed: no LADX_ROM_URL, no tmp/ladx/base.gbc of
SHA-1 d90ac17e..., or no LADXR checkout at the pin (LADXR_DIR, default tests/ladx/oracle/ladxr)
refuses. Its messages name commits, paths, blobs, views and scopes, never the link or the bytes.

--install proves the gate on planted repositories, then writes the chained hook into the common
hooks directory if the pre-push hook there is absent, no_claude_branch.sh's own or already the
chain (else exit 1: the gate is not armed). --uninstall removes the chain, then runs
no_claude_branch.sh --install. --self-test runs the planted pushes only.
"""

import hashlib
import importlib.util
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
ROM_PATH = os.path.join("tmp", "ladx", "base.gbc")
GATE_PATH = "tests/ladx/oracle/push_gate.py"
BYTES_PATH = "tests/ladx/oracle/nintendo_bytes.py"
BRANCH_HOOK_PATH = ".claude/hooks/no_claude_branch.sh"
LADXR_PATH = "tests/ladx/oracle/ladxr"
INVENTORY_PATH = "notes/ladx-inventory.md"
CHECK_MODULES = ("write_log.py", "inventory_check.py", "known_source.py", "tables.py")
ALLOWED_UNTRACKED = "docs/prd/"
MIN_LINK = 8

MARKER = "# chained pre-push hook: no_claude_branch.sh, then tests/ladx/oracle/push_gate.py"
CHAIN_HOOK = f"""#!/bin/sh
{MARKER}
cd "$(git rev-parse --show-toplevel)" || exit 1
refs=$(mktemp) || exit 1
trap 'rm -f "$refs"' EXIT
cat >"$refs"
status=0
sh {BRANCH_HOOK_PATH} "$@" <"$refs" || status=1
python3 -I -B {GATE_PATH} "$@" <"$refs" || status=1
exit $status
"""
# Must stay byte-identical to what no_claude_branch.sh --install writes (checked by the tests).
PLAIN_HOOK = """#!/bin/sh
exec "$(git rev-parse --show-toplevel)/.claude/hooks/no_claude_branch.sh" "$@"
"""

_spec = importlib.util.spec_from_file_location("nintendo_bytes", os.path.join(HERE, "nintendo_bytes.py"))
nintendo_bytes = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(nintendo_bytes)


def _git(top, *args, env=None, data=None):
    """Bytes of git's stdout, run in top; raises on failure."""
    return subprocess.run(["git", *args], cwd=top, env=env, input=data, check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def _link_commits(top, sha, link):
    """Commits (full sha, in log order) under which `git log -p --text` of sha holds link."""
    found = []
    current = None
    proc = subprocess.Popen(["git", "log", "-p", "--text", "--format=fuller", sha], cwd=top,
                            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    for line in proc.stdout:
        header = re.match(rb"commit ([0-9a-f]{40})\b", line)
        if header:
            current = header.group(1).decode()
        if link in line and current not in found:
            found.append(current)
    if proc.wait() != 0:
        raise RuntimeError(f"git log failed on {sha[:12]}")
    return found


def _pushed_blobs(top, sha):
    """[(blob sha, path)] of every blob reachable from sha that no remote ref has."""
    listing = _git(top, "rev-list", "--objects", sha, "--not", "--remotes").decode()
    paths = {}
    for line in listing.splitlines():
        obj, _, path = line.partition(" ")
        if path:
            paths.setdefault(obj, path)
    if not paths:
        return []
    kinds = _git(top, "cat-file", "--batch-check=%(objectname) %(objecttype)",
                 data="".join(o + "\n" for o in paths).encode()).decode().split()
    return [(obj, paths[obj]) for obj, kind in zip(kinds[0::2], kinds[1::2]) if kind == "blob"]


def _blob(top, obj):
    return _git(top, "cat-file", "blob", obj)


def _untracked(top):
    """Paths of the untracked, non-ignored files of the working tree."""
    out = _git(top, "status", "--porcelain", "-z", "--untracked-files=all")
    return [entry[3:].decode("utf-8", "surrogateescape")
            for entry in out.split(b"\0") if entry.startswith(b"?? ")]


def check_push(top, ref_lines, link, rom, corpus):
    """(findings, summary): refusal lines for a push of ref_lines from top, and the success
    line used when there are none. link and rom are bytes, corpus a nintendo_bytes.Corpus."""
    findings = []
    index = nintendo_bytes.RomIndex(rom)
    cleared = {category: 0 for category in nintendo_bytes.CLEARANCES}
    commits = 0
    blobs = {}
    for ref_line in ref_lines:
        fields = ref_line.split()
        if len(fields) != 4 or set(fields[1]) == {"0"}:
            continue
        sha = fields[1]
        commits += int(_git(top, "rev-list", "--count", sha, "--not", "--remotes"))
        for commit in _link_commits(top, sha, link):
            line = f"push-gate: refused: the link is in commit {commit[:12]}"
            if line not in findings:
                findings.append(line)
        for obj, path in _pushed_blobs(top, sha):
            blobs.setdefault(obj, path)
    reads = nintendo_bytes.ReadSets(
        lambda: nintendo_bytes.read_inventory(os.path.join(top, INVENTORY_PATH)), rom)
    for obj, path in sorted(blobs.items(), key=lambda item: (item[1], item[0])):
        try:
            literals, clears = nintendo_bytes.check_blob(path, _blob(top, obj), index, corpus,
                                                         reads=reads)
        except nintendo_bytes.SetupError as e:
            findings.append(f"push-gate: refused: {path} (blob {obj[:12]}): setup: {e}")
            continue
        for view, off, length, scope in literals:
            findings.append(f"push-gate: refused: {path} (blob {obj[:12]}): literal run of "
                            f"{length} bytes at @{off} ({view}, {scope})")
        for _, _, _, category in clears:
            cleared[category] += 1
    for path in _untracked(top):
        if not path.startswith(ALLOWED_UNTRACKED):
            findings.append(f"push-gate: refused: untracked file {path}")
            continue
        with open(os.path.join(top, path), "rb") as f:
            if link in f.read():
                findings.append(f"push-gate: refused: the link is in untracked file {path}")
    summary = (f"push-gate: ok, {commits} commits, {len(blobs)} blobs, no link, 0 literal runs, "
               "cleared " + ", ".join(f"{n} {category}" for category, n in cleared.items())
               + ", no stray untracked file")
    return findings, summary


def hook(top, ref_lines, environ):
    """Exit status of the gate for a push; prints its verdict on stderr."""
    link = environ.get("LADX_ROM_URL", "")
    if len(link) < MIN_LINK:
        print("push-gate: setup: LADX_ROM_URL is not set, push refused", file=sys.stderr)
        return 1
    rom_file = os.path.join(top, ROM_PATH)
    rom = None
    if os.path.isfile(rom_file):
        with open(rom_file, "rb") as f:
            rom = f.read()
    if rom is None or hashlib.sha1(rom).hexdigest() != LADX_SHA1:
        print("push-gate: setup: no checked ROM in tmp/ladx/ (run tests/ladx/oracle/fetch_rom.sh)"
              ", push refused", file=sys.stderr)
        return 1
    try:
        ladxr = os.path.join(top, environ.get("LADXR_DIR") or LADXR_PATH)
        corpus = nintendo_bytes.load_ladxr(ladxr)
    except nintendo_bytes.SetupError as e:
        print(f"push-gate: setup: {e}, push refused", file=sys.stderr)
        return 1
    findings, summary = check_push(top, ref_lines, link.encode(), rom, corpus)
    for line in findings or [summary]:
        print(line, file=sys.stderr)
    return 1 if findings else 0


def _pre_push(top):
    common = _git(top, "rev-parse", "--git-common-dir").decode().strip()
    return os.path.join(top, common, "hooks", "pre-push")


def _read(path):
    if not os.path.isfile(path):
        return None
    with open(path) as f:
        return f.read()


def install_chain(top):
    """Writes the chained pre-push hook for the repository at top; returns (status, message).
    Never touches another hook."""
    path = _pre_push(top)
    existing = _read(path)
    if existing not in (None, PLAIN_HOOK) and MARKER not in existing:
        return 1, (f"push-gate: {path} is neither no_claude_branch.sh's hook nor the chain, "
                   "left alone: the gate is NOT armed")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(CHAIN_HOOK)
    os.chmod(path, 0o755)
    return 0, f"push-gate: armed, {path} chains no_claude_branch.sh and push_gate.py"


def uninstall_chain(top):
    """Removes the chained hook of the repository at top, then reinstalls no_claude_branch.sh's
    own; returns (status, message)."""
    path = _pre_push(top)
    existing = _read(path)
    removed = existing is not None and MARKER in existing
    if removed:
        os.remove(path)
    result = subprocess.run(["sh", os.path.join(top, BRANCH_HOOK_PATH), "--install"], cwd=top,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    message = result.stdout.strip()
    if result.returncode != 0:
        return 1, f"push-gate: no_claude_branch.sh --install failed: {message}"
    if not removed:
        return 0, f"push-gate: {path} was not the chain, nothing removed" + (
            f"; {message}" if message else "")
    return 0, "push-gate: disarmed, the pre-push hook is no_claude_branch.sh's own again"


# Split so that the gate's planted copy, committed in the planted clone, does not hold it.
PLANTED_LINK = "planted-link://" + "rom-host/0123456789abcdef"
PLANTED_COMMIT_MSG_HOOK = "#!/bin/sh\n# planted prepare-commit-msg hook\nexit 0\n"


def planted_rom():
    return nintendo_bytes.planted_rom()


def planted_text(rom):
    return rom[0x2300:0x2300 + len(nintendo_bytes.PLANTED_TEXT)]


class PlantedRepo:
    """A bare remote and a clone in base, with copies of the gate, nintendo_bytes.py (and the
    CHECK_MODULES it loads), a planted inventory and no_claude_branch.sh at their repository paths, the gate's ROM SHA-1 constant rewritten to rom's,
    rom at tmp/ladx/base.gbc, a planted LADXR checkout (git-ignored, one text file holding
    planted_text(rom)[20:70]) with the copy's LADXR pin rewritten to its commit, a planted
    prepare-commit-msg hook and the chain installed through the copy's install_chain. Branch main
    is pushed; every case branches from it."""

    def __init__(self, base, rom):
        self.rom = rom
        self.remote = os.path.join(base, "remote.git")
        self.top = os.path.join(base, "clone")
        self.env = {k: v for k, v in os.environ.items()
                    if not k.startswith("GIT_") and k != "LADX_ROM_URL"}
        self.env.update(GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1",
                        LADX_ROM_URL=PLANTED_LINK)
        _git(base, "init", "-q", "--bare", self.remote, env=self.env)
        _git(base, "init", "-q", "-b", "main", self.top, env=self.env)
        self.git("config", "user.email", "planted@example.invalid")
        self.git("config", "user.name", "planted")
        self.git("remote", "add", "origin", self.remote)
        gate = _read(os.path.join(HERE, "push_gate.py"))
        quoted = f'"{LADX_SHA1}"'
        assert gate.count(quoted) == 1, "the ROM SHA-1 constant must occur exactly once"
        self.write(GATE_PATH, gate.replace(quoted, f'"{hashlib.sha1(rom).hexdigest()}"'))
        self.ladxr = os.path.join(self.top, LADXR_PATH)
        self.write(os.path.join(LADXR_PATH, "names.py"),
                   b"NAME = '" + planted_text(rom)[20:70] + b"'\n")
        ladxr_git = lambda *args: _git(self.ladxr, *args, env=self.env).decode().strip()
        ladxr_git("init", "-q")
        ladxr_git("add", "-A")
        ladxr_git("-c", "user.name=planted", "-c", "user.email=planted@example.invalid",
                  "commit", "-q", "-m", "pin")
        checker = _read(os.path.join(HERE, "nintendo_bytes.py"))
        pin = f'"{nintendo_bytes.LADXR_PIN}"'
        assert checker.count(pin) == 1, "the LADXR pin constant must occur exactly once"
        self.write(BYTES_PATH, checker.replace(pin, f'"{ladxr_git("rev-parse", "HEAD")}"'))
        for name in CHECK_MODULES:
            self.write(os.path.join(os.path.dirname(BYTES_PATH), name),
                       _read(os.path.join(HERE, name)))
        self.write(INVENTORY_PATH, nintendo_bytes.planted_inventory([
            ("reader(rom)", "bank 0x00 source 0x1000..0x10FF", "reader"),
            ("maker(rom)", "-", "maker")]))
        self.write(BRANCH_HOOK_PATH, _read(os.path.join(TOP, BRANCH_HOOK_PATH)))
        self.write(".gitignore", f"tmp/\n{LADXR_PATH}/\n")
        self.write(ROM_PATH, rom)
        self.git("add", "-A")
        self.git("commit", "-q", "-m", "base")
        self.git("push", "-q", "origin", "main")
        hooks = os.path.join(self.top, ".git", "hooks")
        self.commit_msg_hook = os.path.join(hooks, "prepare-commit-msg")
        self.write(self.commit_msg_hook, PLANTED_COMMIT_MSG_HOOK)
        os.chmod(self.commit_msg_hook, 0o755)
        spec = importlib.util.spec_from_file_location("planted_push_gate",
                                                      os.path.join(self.top, GATE_PATH))
        self.gate = importlib.util.module_from_spec(spec)
        no_bytecode, sys.dont_write_bytecode = sys.dont_write_bytecode, True
        try:
            spec.loader.exec_module(self.gate)
        finally:
            sys.dont_write_bytecode = no_bytecode
        status, message = self.gate.install_chain(self.top)
        assert status == 0, message

    def git(self, *args, check=True):
        return subprocess.run(["git", *args], cwd=self.top, env=self.env, check=check,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def write(self, rel, data):
        path = os.path.join(self.top, rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb" if isinstance(data, bytes) else "w") as f:
            f.write(data)

    def commit(self, message, files=(), removed=()):
        for rel, data in files:
            self.write(rel, data)
        for rel in removed:
            self.git("rm", "-q", rel)
        self.git("add", "-A")
        self.git("commit", "-q", "--allow-empty", "-m", message)

    def push(self, branch, setup, unset_link=False):
        """(exit status, output) of pushing a new branch made from main by setup(self)."""
        self.git("checkout", "-q", "-b", branch, "main")
        try:
            setup(self)
            env = dict(self.env)
            if unset_link:
                del env["LADX_ROM_URL"]
            result = subprocess.run(["git", "push", "origin", branch], cwd=self.top, env=env,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            return result.returncode, result.stdout.decode("utf-8", "replace")
        finally:
            self.git("clean", "-fdq")
            self.git("checkout", "-q", "main")


def _planted_log(group, data):
    """A write log of one segment of group writing data as one literal token at offset 0x10."""
    return ("ladx-rom-writes 1\ntables texts\nG %s\nB 10 %s\n" % (group, data.hex())).encode()


def _planted_table(data_hex):
    """A patch table of group maker: one unit keyed by race whose entry writes data_hex as one
    literal token at offset 0x10."""
    return ("ladx-tables 1\ngroup maker\ntables texts\nunit maker.1 race\nE 0\nB 10 %s\n"
            % data_hex).encode()


def _cases(rom):
    """[(name, setup, unset_link, expected substring or None for a pass)]."""
    link = PLANTED_LINK.encode()
    text = planted_text(rom)
    bytelist = ", ".join("0x%02X" % b for b in rom[6000:6008])
    return [
        ("clean", lambda r: r.commit("clean", [("a.txt", "plain text\n")]), False, None),
        ("link-in-message", lambda r: r.commit("see " + PLANTED_LINK), False,
         "the link is in commit"),
        ("link-in-file", lambda r: r.commit("file", [("notes.txt", "at " + PLANTED_LINK + "\n")]),
         False, "the link is in commit"),
        ("link-in-binary", lambda r: r.commit("bin", [("blob.bin", b"\0\1\2" + link + b"\0\xff")]),
         False, "the link is in commit"),
        ("link-in-parent", lambda r: (r.commit("add", [("old.txt", PLANTED_LINK + "\n")]),
                                      r.commit("drop", removed=["old.txt"])),
         False, "the link is in commit"),
        ("strict-rom-4", lambda r: r.commit("slice", [("tests/ladx/extract/s.bin",
                                                        b"<" + rom[2000:2004] + b">")]),
         False, "literal run of 4 bytes at @1 (raw, strict)"),
        ("general-english-12", lambda r: r.commit("say", [("notes/n.md", b"<" + text[:12] + b">")]),
         False, None),
        ("general-rom-16", lambda r: r.commit("slice", [("notes/r.bin", rom[5000:5016])]),
         False, "literal run of 16 bytes at @0 (raw, general)"),
        ("ladxr-public", lambda r: r.commit("name", [("notes/names.md", text[20:70])]),
         False, None),
        ("general-bytelist-8",
         lambda r: r.commit("c", [("src/t.c", "u8 t[] = {" + bytelist + "};")]),
         False, "literal run of 8 bytes at @10 (bytelist, general)"),
        ("case-run-general", lambda r: r.commit("run", [("tests/ladx/cases/c/run.txt",
                                                          b"<" + rom[2000:2008] + b">")]),
         False, None),
        ("case-writes-leak", lambda r: r.commit("log", [("tests/ladx/cases/c/rom_writes.txt",
                                                         _planted_log("reader", rom[0x1080:0x1088]))]),
         False, "literal run of 8 bytes at @45 (writes, strict)"),
        ("case-writes-computed", lambda r: r.commit("log", [(
            "tests/ladx/cases/c/rom_writes.txt", _planted_log("maker", rom[0x1080:0x1088]))]),
         False, None),
        ("case-writes-unparsed", lambda r: r.commit("log", [("tests/ladx/cases/c/rom_writes.txt",
                                                             b"ladx-rom-writes 2\n")]),
         False, "(writes, unparsed)"),
        ("table-literal", lambda r: r.commit("table", [(
            "data/ladx/maker.txt", _planted_table(rom[0x1080:0x1084].hex()))]),
         False, "literal run of 4 bytes at @66 (tables, strict)"),
        ("rom-slice-removed", lambda r: (r.commit("add", [("s.bin", rom[3000:3020])]),
                                         r.commit("drop", removed=["s.bin"])),
         False, "literal run of 20 bytes"),
        ("stray-untracked", lambda r: (r.commit("clean"), r.write("LADXR_1.gbc.txt", "x\n")),
         False, "untracked file LADXR_1.gbc.txt"),
        ("prd-untracked", lambda r: (r.commit("clean"), r.write("docs/prd/next/x.md", "# next\n")),
         False, None),
        ("prd-untracked-link", lambda r: (r.commit("clean"),
                                          r.write("docs/prd/next/x.md", PLANTED_LINK + "\n")),
         False, "the link is in untracked file docs/prd/next/x.md"),
        ("no-link-in-environment", lambda r: r.commit("clean"), True,
         "LADX_ROM_URL is not set, push refused"),
        ("claude/planted", lambda r: r.commit("clean"), False, "claude/* branch"),
    ]


def self_test():
    """[] when every planted push behaves as expected, else one line per failing case."""
    failures = []
    rom = planted_rom()
    with tempfile.TemporaryDirectory() as base:
        repo = PlantedRepo(base, rom)
        for name, setup, unset_link, expected in _cases(rom):
            branch = name if "/" in name else "case/" + name
            status, output = repo.push(branch, setup, unset_link)
            if expected is None and status != 0:
                failures.append(f"push-gate: self-test failed: {name}: refused")
            elif expected is not None and (status == 0 or expected not in output):
                failures.append(f"push-gate: self-test failed: {name}: not refused as expected")
            if PLANTED_LINK in output:
                failures.append(f"push-gate: self-test failed: {name}: the link was echoed")
        if _read(repo.commit_msg_hook) != PLANTED_COMMIT_MSG_HOOK:
            failures.append("push-gate: self-test failed: prepare-commit-msg changed by install")
    return failures


def _proved():
    failures = self_test()
    for line in failures:
        print(line)
    if not failures:
        print(f"push-gate: self-test, {len(_cases(b''))} planted pushes as expected")
    return not failures


def main(argv):
    if argv[:1] == ["--self-test"]:
        return 0 if _proved() else 1
    if argv[:1] == ["--install"]:
        if not _proved():
            print("push-gate: self-test failed, the gate is NOT armed")
            return 1
        status, message = install_chain(TOP)
        print(message)
        return status
    if argv[:1] == ["--uninstall"]:
        status, message = uninstall_chain(TOP)
        print(message)
        return status
    if argv[:1] and argv[0].startswith("--"):
        print(__doc__.split("\n\n")[0], file=sys.stderr)
        return 2
    return hook(TOP, sys.stdin.read().splitlines(), os.environ)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
