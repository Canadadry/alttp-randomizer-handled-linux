"""LADX's full-ROM check: runs a generator (Phase 3's ladx_patch, or any binary keeping its
contract) on every case and combo of tests/ladx with a base ROM, and compares the SHA-1 of each
output with the golden's rom_sha1 (run.txt). It proves itself first on a synthetic ROM, a planted
case tree and fake generators. It prints verdicts and digests only, never ROM bytes; the
generator's own output goes to a log under tmp/. Standard library only.

Generator contract: <gen> --base <rom> --out <file> --seed <32 lowercase hex> [<key>=<value>...],
the settings in LADXR's order; exit 0 with <file> written, exit 3 when the base is refused (no
output), anything else a failure. It runs in its case's work directory, without LADX_ROM_URL.
"""

import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
TOP = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
sys.path.insert(0, HERE)

LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
LINK_VARIABLE = "LADX_ROM_URL"
TIMEOUT = 120
EXIT_REFUSED = 3
DIRS = ("cases", "combos")
HEX = "0123456789abcdef"


class Usage(Exception):
    pass


def _sha1_file(path):
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 16), b""):
            h.update(block)
    return h.hexdigest()


def golden(path):
    try:
        with open(os.path.join(path, "run.txt"), encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
    except OSError:
        return None, "no run.txt"
    values = {}
    for line in lines:
        key, _, value = line.partition(" ")
        if key in ("rom_sha1", "outcome"):
            values[key] = value
    if "outcome" not in values:
        return None, "no outcome line in run.txt"
    if values["outcome"] != "ok":
        return None, "outcome %s in run.txt, not ok" % values["outcome"]
    digest = values.get("rom_sha1")
    if digest is None:
        return None, "no rom_sha1 line in run.txt"
    if len(digest) != 40 or any(c not in HEX for c in digest):
        return None, "rom_sha1 in run.txt is not 40 lowercase hex digits"
    return digest, None


def _inside(path, parent):
    path, parent = os.path.realpath(path), os.path.realpath(parent)
    return path.startswith(parent + os.sep)


def _run_one(gen, rom, seed, assignments, directory, timeout, env):
    out = os.path.join(directory, "out.gbc")
    argv = [gen, "--base", rom, "--out", out, "--seed", seed] + list(assignments)
    with open(os.path.join(directory, "gen.log"), "wb") as log:
        try:
            done = subprocess.run(argv, cwd=directory, env=env, stdin=subprocess.DEVNULL,
                                  stdout=log, stderr=subprocess.STDOUT, timeout=timeout)
        except subprocess.TimeoutExpired:
            return "failed", "timeout %g s" % timeout
        except OSError as err:
            return "failed", "not started (%s)" % err.strerror
    if done.returncode < 0:
        return "failed", "signal %d" % -done.returncode
    if done.returncode == EXIT_REFUSED:
        return "refused", None
    if done.returncode != 0:
        return "failed", "exit %d" % done.returncode
    if not os.path.isfile(out):
        return "failed", "no output"
    return "output", out


def check(root, entries, rom, gen, work, top, out, timeout=TIMEOUT, env=None):
    if not _inside(work, os.path.join(top, "tmp")):
        raise Usage("rom-check: --work %s is outside %s" % (work, os.path.join(top, "tmp", "")))
    if not os.path.isfile(rom) or not os.access(rom, os.R_OK):
        raise Usage("setup: rom-check: no ROM at %s (LADX_ROM=<file>, or "
                    "tests/ladx/oracle/fetch_rom.sh)" % rom)
    if not os.path.isfile(gen) or not os.access(gen, os.X_OK):
        raise Usage("setup: rom-check: no generator at %s (LADX_GEN=<binary>; Phase 3 builds "
                    "ladx_patch)" % gen)
    for path in (rom, gen):
        if _inside(path, work):
            raise Usage("rom-check: --work %s holds %s" % (work, path))
    base = _sha1_file(rom)
    print("rom-check: base %s (%s), generator %s"
          % (base, "the LADX English 1.0 ROM" if base == LADX_SHA1 else "not the LADX ROM", gen),
          file=out, flush=True)
    env = dict(os.environ if env is None else env)
    env.pop(LINK_VARIABLE, None)
    rom, gen, work = os.path.abspath(rom), os.path.abspath(gen), os.path.abspath(work)
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    counts = dict.fromkeys(("ok", "mismatched", "refused", "failed", "nogolden"), 0)
    subs = dict.fromkeys(DIRS, 0)
    for sub, name, seed, assignments in entries:
        subs[sub] += 1
        want, reason = golden(os.path.join(root, sub, name))
        if want is None:
            counts["nogolden"] += 1
            print("NOGOLDEN %s: %s" % (name, reason), file=out, flush=True)
            continue
        directory = os.path.join(work, name)
        os.makedirs(directory)
        kind, detail = _run_one(gen, rom, seed, assignments, directory, timeout, env)
        if kind == "refused":
            counts["refused"] += 1
            line = "REFUSED %s: base refused (exit %d)" % (name, EXIT_REFUSED)
        elif kind == "failed":
            counts["failed"] += 1
            line = "FAILED %s: %s" % (name, detail)
        else:
            got = _sha1_file(detail)
            if got == want:
                counts["ok"] += 1
                os.remove(detail)
                line = "ok " + name
            else:
                counts["mismatched"] += 1
                line = "MISMATCH %s: sha1 %s, golden %s" % (name, got, want)
        print(line, file=out, flush=True)
    print("rom-check: %d cases, %d combos: %d ok, %d mismatched, %d refused, %d failed, "
          "%d no golden" % (subs["cases"], subs["combos"], counts["ok"], counts["mismatched"],
                            counts["refused"], counts["failed"], counts["nogolden"]),
          file=out, flush=True)
    return 0 if counts["ok"] == len(entries) else 1


# The self-test's fake generator: its output is a deterministic function of base, seed and
# settings (FAKE_OUTPUT, run by the generator and by the self-test), its behaviour picked by a
# `fake=<mode>` setting.
FAKE_OUTPUT = """
import hashlib
def fake_output(base, seed, settings):
    h = hashlib.sha256(base + seed.encode() + "\\0".join(settings).encode()).digest()
    return h * 64
"""

FAKE_GEN = FAKE_OUTPUT + """
import json, os, sys, time
args = sys.argv[1:]
with open("args.json", "w") as f:
    json.dump({"argv": args, "link": os.environ.get("LADX_ROM_URL")}, f)
base, out, seed, settings = args[1], args[3], args[5], args[6:]
mode = ([s[5:] for s in settings if s.startswith("fake=")] or ["ok"])[0]
with open(base, "rb") as f:
    data = f.read()
if mode == "leak":
    sys.stdout.write(data.hex())
    sys.stdout.flush()
    sys.stderr.buffer.write(data)
if mode == "refuse":
    with open(out, "wb") as f:
        f.write(fake_output(data, seed, settings))
    sys.exit(3)
if mode == "exit1":
    sys.exit(1)
if mode == "hang":
    time.sleep(30)
if mode == "nooutput":
    sys.exit(0)
with open(out, "wb") as f:
    f.write(fake_output(data, seed, settings))
"""

PLANTED_LINK = "https://planted.invalid/rom-check-self-test/base.gbc"


def _synthetic_rom():
    data = bytearray(hashlib.sha256(b"rom-check synthetic %d" % i).digest()[j]
                     for i in range(2048) for j in range(32))
    data[0x134:0x144] = b"ROMCHECK-MARKER!"
    return bytes(data)


def _leaks(text, rom):
    raw = text.encode("utf-8", "surrogateescape")
    windows = {rom[i:i + 4] for i in range(len(rom) - 3)}
    if any(raw[i:i + 4] in windows for i in range(len(raw) - 3)):
        return True
    hexes = {w.hex() for w in windows}
    lower = text.lower()
    return any(lower[i:i + 8] in hexes for i in range(len(lower) - 7))


def _plant(root, sub, name, run_txt):
    path = os.path.join(root, sub, name)
    os.makedirs(path)
    if run_txt is not None:
        with open(os.path.join(path, "run.txt"), "w", encoding="utf-8") as f:
            f.write(run_txt)


def _run_txt(digest, outcome="ok"):
    lines = ["trace 1 rnd planted choice 2 -> 1 [1]"]
    if digest is not None:
        lines.append("rom_sha1 " + digest)
    return "\n".join(lines + ["spoiler_sha1 " + "0" * 40, "outcome " + outcome, ""])


def self_test():
    namespace = {}
    exec(FAKE_OUTPUT, namespace)
    fake_output = namespace["fake_output"]
    failures, runs = [], 0
    scratch = tempfile.mkdtemp(prefix="rom-check-self-test-")
    try:
        rom = os.path.join(scratch, "base.gbc")
        data = _synthetic_rom()
        with open(rom, "wb") as f:
            f.write(data)
        gen = os.path.join(scratch, "fake_gen")
        with open(gen, "w", encoding="utf-8") as f:
            f.write("#!%s -IS\n%s" % (sys.executable, FAKE_GEN))
        os.chmod(gen, 0o755)
        root, work = os.path.join(scratch, "ladx"), os.path.join(scratch, "tmp", "rom-check")
        seed = "0123456789abcdef0123456789abcdef"

        def digest(settings):
            return hashlib.sha1(fake_output(data, seed, settings)).hexdigest()

        def off_by_one(d):
            return d[:-1] + HEX[(HEX.index(d[-1]) + 1) % 16]

        planted = [
            # (sub, name, settings, run.txt, expected line)
            ("cases", "baseline", [], _run_txt(digest([])), "ok baseline"),
            ("cases", "logic-hell", ["logic=hell", "fake=leak"],
             _run_txt(digest(["logic=hell", "fake=leak"])), "ok logic-hell"),
            ("cases", "mismatch", ["fake=ok"], _run_txt(off_by_one(digest(["fake=ok"]))),
             "MISMATCH mismatch: sha1 %s, golden %s" % (digest(["fake=ok"]),
                                                        off_by_one(digest(["fake=ok"])))),
            ("cases", "refused", ["fake=refuse"], _run_txt(digest(["fake=refuse"])),
             "REFUSED refused: base refused (exit 3)"),
            ("cases", "exit1", ["fake=exit1"], _run_txt(digest([])), "FAILED exit1: exit 1"),
            ("cases", "nooutput", ["fake=nooutput"], _run_txt(digest([])),
             "FAILED nooutput: no output"),
            ("cases", "hang", ["fake=hang"], _run_txt(digest([])), "FAILED hang: timeout 0.3 s"),
            ("cases", "nosha", [], _run_txt(None),
             "NOGOLDEN nosha: no rom_sha1 line in run.txt"),
            ("cases", "crashed", [], _run_txt(digest([]), "crash"),
             "NOGOLDEN crashed: outcome crash in run.txt, not ok"),
            ("combos", "logic-hell+race-1", ["logic=hell", "race=1"],
             _run_txt(digest(["logic=hell", "race=1"])), "ok logic-hell+race-1"),
        ]
        for sub, name, _, run_txt, _ in planted:
            _plant(root, sub, name, run_txt)
        entries = [(sub, name, seed, settings) for sub, name, settings, _, _ in planted]
        env = dict(os.environ, **{LINK_VARIABLE: PLANTED_LINK})

        def run(entries, rom=rom, gen=gen, work=work):
            out = __import__("io").StringIO()
            try:
                status = check(root, entries, rom, gen, work, scratch, out, timeout=0.3, env=env)
            except Usage as err:
                return 2, out.getvalue(), str(err)
            return status, out.getvalue(), ""

        def expect(name, condition):
            if not condition:
                failures.append(name)

        status, text, _ = run(entries)
        lines = text.splitlines()
        runs += len(planted)
        expect("header", lines[:1] == ["rom-check: base %s (not the LADX ROM), generator %s"
                                       % (hashlib.sha1(data).hexdigest(), gen)])
        for (sub, name, settings, _, line), got in zip(planted, lines[1:] + [""] * len(planted)):
            expect(name, got == line)
        expect("summary", lines[-1:] == ["rom-check: 9 cases, 1 combos: 3 ok, 1 mismatched, "
                                         "1 refused, 3 failed, 2 no golden"])
        expect("exit 1", status == 1)

        def files(name):
            path = os.path.join(work, name)
            return sorted(os.listdir(path)) if os.path.isdir(path) else None
        expect("ok output deleted", files("baseline") == ["args.json", "gen.log"])
        expect("mismatch output kept", files("mismatch") == ["args.json", "gen.log", "out.gbc"])
        expect("refused output kept", files("refused") == ["args.json", "gen.log", "out.gbc"])
        expect("no golden not run", files("nosha") is None)
        try:
            with open(os.path.join(work, "logic-hell", "args.json"), encoding="utf-8") as f:
                recorded = __import__("json").load(f)
        except (OSError, ValueError):
            recorded = {}
        expect("arguments", recorded.get("argv") == [
            "--base", rom, "--out", os.path.join(work, "logic-hell", "out.gbc"), "--seed", seed,
            "logic=hell", "fake=leak"])
        expect("no link passed", "link" in recorded and recorded["link"] is None)
        expect("leaky generator logged", os.path.getsize(
            os.path.join(work, "logic-hell", "gen.log")) >= 3 * len(data))
        expect("no link printed", PLANTED_LINK not in text)
        expect("no ROM bytes printed", not _leaks(text, data))

        status, text, _ = run(entries[:2] + entries[-1:])
        runs += 1
        expect("all ok exit 0", status == 0 and text.splitlines()[-1:] == [
            "rom-check: 2 cases, 1 combos: 3 ok, 0 mismatched, 0 refused, 0 failed, 0 no golden"])

        missing = os.path.join(scratch, "none.gbc")
        status, text, err = run(entries[:1], rom=missing)
        runs += 1
        expect("missing ROM", status == 2 and text == "" and err.startswith(
            "setup: rom-check: no ROM at %s " % missing))
        for label, bad in (("missing generator", os.path.join(scratch, "ladx_patch")),
                           ("generator not executable", os.path.join(root, "cases", "baseline",
                                                                     "run.txt"))):
            status, text, err = run(entries[:1], gen=bad)
            runs += 1
            expect(label, status == 2 and text == "" and err.startswith(
                "setup: rom-check: no generator at %s " % bad))
        for label, bad in (("work outside tmp", os.path.join(scratch, "work")),
                           ("work is tmp", os.path.join(scratch, "tmp")),
                           ("work holds the ROM", scratch)):
            status, text, err = run(entries[:1], work=bad)
            runs += 1
            expect(label, status == 2 and text == "" and err.startswith("rom-check: --work "))
        expect("nothing outside tmp", not os.path.exists(os.path.join(scratch, "work")))
    finally:
        shutil.rmtree(scratch, ignore_errors=True)
    return failures, runs


USAGE = """usage: rom_check.py [--self-test] [--rom <file>] [--gen <binary>] [--work <dir>]
                    [--timeout <s>] [--root <dir>] [--inputs <dir>] [<case>...]
Proves itself on planted runs, then runs <gen> on every case and combo of <root> (or the named
ones) with <rom>, and compares each output's SHA-1 with its golden rom_sha1. --self-test stops
after the self-test. <rom> defaults to tmp/ladx/base.gbc, <gen> to tests/ladx/build/ladx_patch,
<work> to tmp/ladx-rom-check (inside the repository's tmp/), the timeout to 120 s per case,
<root> to tests/ladx, <inputs> to tests/ladx/oracle. Exit 0 when every case is ok, 1 otherwise,
2 on a usage error or a `setup:` line."""


def _entries(root, inputs, names):
    import ladx_cases
    try:
        texts = []
        for name in ("tier1.txt", "settings.txt", "settings_validate.txt", "combos.txt"):
            with open(os.path.join(inputs, name), encoding="utf-8") as f:
                texts.append(f.read())
        e = ladx_cases.expected_list(*texts)
        lines = ladx_cases.mismatches(e, root)
        if lines:
            return None, lines
        listed = ladx_cases.listing(e, root, names or None)
    except (ladx_cases.CasesError, OSError) as err:
        raise Usage(str(err))
    combos = {n for n, _ in e.combos}
    entries = []
    for line in listed:
        name, seed, *assignments = line.split(" ")
        entries.append(("combos" if name in combos else "cases", name, seed, assignments))
    return entries, None


def main(argv):
    options = {"--rom": os.path.join("tmp", "ladx", "base.gbc"),
               "--gen": os.path.join("tests", "ladx", "build", "ladx_patch"),
               "--work": os.path.join(TOP, "tmp", "ladx-rom-check"),
               "--timeout": str(TIMEOUT), "--root": os.path.dirname(HERE), "--inputs": HERE}
    names, only_self_test = [], False
    while argv:
        if argv[0] == "--self-test":
            only_self_test, argv = True, argv[1:]
        elif argv[0] in options and len(argv) >= 2:
            options[argv[0]], argv = argv[1], argv[2:]
        elif argv[0].startswith("-"):
            print(USAGE, file=sys.stderr)
            return 2
        else:
            names.append(argv[0])
            argv = argv[1:]
    try:
        timeout = float(options["--timeout"])
    except ValueError:
        timeout = 0
    if timeout <= 0 or (only_self_test and names):
        print(USAGE, file=sys.stderr)
        return 2
    failures, runs = self_test()
    if failures:
        for name in failures:
            print("rom-check: self-test failed: %s" % name, file=sys.stderr)
        return 1
    print("rom-check: self-test, %d planted runs as expected" % runs, flush=True)
    if only_self_test:
        return 0
    try:
        entries, lines = _entries(options["--root"], options["--inputs"], names)
        if lines:
            print("\n".join(lines), file=sys.stderr)
            return 1
        return check(options["--root"], entries, options["--rom"], options["--gen"],
                     options["--work"], TOP, sys.stdout, timeout=timeout)
    except Usage as err:
        print(err, file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
