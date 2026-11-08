#!/usr/bin/env python3
"""check-e2e's failure ranker: every failure of a game's saved e2e outputs, cheapest first.

Usage: rank_e2e.py <game test dir> <output file>...
Contract (lines, ranking, exit codes): .claude/skills/check-e2e/SKILL.md, "The ranker"."""

import os
import re
import sys

VERDICT = re.compile(r"^(e2e|e2e-rom): \d+ pass, \d+ divergent")
TARGET_PREFIXES = ("KS DIFFERENT", "PROJECTION ", "run_e2e:", "run_e2e.sh:", "e2e-rom: KS:")
LADX_GENERATOR = ("no C generator", "c generator exited", "c generator timed out", "c wrote no run.txt",
                  "c run.txt malformed")
LADX_NO_LOG = ("c wrote no rom_writes.txt", "c rom_writes.txt malformed")
SECTION = re.compile(r"^(.+): line (\d+):$")
ALTTP_GROUP = re.compile(r"^rom_writes\.txt: (\S+): first differing write (\d+):$")
ALTTP_SHORT = re.compile(r"^rom_writes\.txt: c ends after (\d+) writes")
INF = float("inf")
RUN_ORDER = ["e2e", "e2e-combos", "e2e-rom"]


class Usage(Exception):
    pass


def read_lines(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read().splitlines()
    except OSError as e:
        raise Usage("cannot read %s: %s" % (path, e.strerror))


def ladx_order(game_dir):
    """Each write-log group (spoiler included) at its first row of data/ladx/order.txt, in order."""
    path = os.path.join(game_dir, "..", "..", "data", "ladx", "order.txt")
    order = []
    for line in read_lines(path):
        fields = line.split(" ")
        if fields[0] in ("table", "C") and len(fields) > 1 and fields[1] not in order:
            order.append(fields[1])
    return order


def alttp_groups(game_dir):
    """({tag: group}, default group, [groups in order]) of write_groups.txt."""
    tags, default, order = {}, None, []
    for line in read_lines(os.path.join(game_dir, "write_groups.txt")):
        fields = line.split()
        if not fields or fields[0] != "group" or len(fields) < 3:
            continue
        order.append(fields[1])
        for tag in fields[2:]:
            if tag == "*":
                default = fields[1]
            else:
                tags[tag] = fields[1]
    return tags, default, order


class Failure:
    def __init__(self, run, run_index, case, block, target=False):
        self.run, self.run_index, self.case, self.block, self.target = run, run_index, case, block, target
        self.key, self.reason = None, None


def split_output(run, run_index, lines):
    """([Failure], [setup line]) of one saved output."""
    failures, setups, verdict, i = [], [], False, 0
    target = []
    while i < len(lines):
        line = lines[i]
        if line.startswith("DIVERGENT "):
            block = [line]
            i += 1
            while i < len(lines) and lines[i].startswith(" "):
                block.append(lines[i])
                i += 1
            failures.append(Failure(run, run_index, line.split(" ", 1)[1].strip(), block))
            continue
        if line.startswith("setup:") or line.startswith("e2e-rom: setup:"):
            setups.append(line)
        elif line.startswith(TARGET_PREFIXES):
            target.append(line)
        elif VERDICT.match(line):
            verdict = True
        i += 1
    if target:
        failures.insert(0, Failure(run, run_index, None, target, True))
    elif not verdict and not failures and not setups:
        failures.append(Failure(run, run_index, None, ["no verdict line (a build or driver failure): see %s's output"
                                                       % run], True))
    return failures, setups


def details(block):
    """[(top-level detail line, [its sub-lines])] of a DIVERGENT block."""
    out = []
    for line in block[1:]:
        if line.startswith("    ") and out:
            out[-1][1].append(line.strip())
        elif line.startswith("  "):
            out.append((line.strip(), []))
    return out


def ladx_rank(detail, order):
    """(class, group index, sub, n) and a reason of one LADX detail line."""
    if detail.startswith(LADX_GENERATOR):
        return (0, 0, 0, 0), "seed generator: %s" % detail
    if detail.startswith(LADX_NO_LOG):
        return (1, -1, 0, 0), "write log: %s" % detail
    if detail.startswith("net:"):
        return (2, 0, 0, 0), "net"
    if detail.startswith("spoiler:"):
        index = order.index("spoiler") if "spoiler" in order else len(order)
        return (1, index, 1, 0), "spoiler"
    m = SECTION.match(detail)
    if not m:
        return (0, 0, 0, INF), "seed %s" % detail
    section, n = m.group(1), int(m.group(2))
    if section.startswith("trace "):
        group = section[len("trace "):]
        index = order.index(group) if group in order else len(order)
        return (1, index, 0, n), "trace %s line %d" % (group, n)
    if section in order:
        return (1, order.index(section), 1, n), "write log %s line %d" % (section, n)
    return (0, 0, 0, n), "seed %s line %d" % (section, n)


def _draw(subs):
    for line in subs:
        for prefix in ("golden: ", "c: "):
            if line.startswith(prefix):
                fields = line[len(prefix):].split(" ")
                if len(fields) > 1 and fields[0].isdigit() and fields[1].isdigit():
                    return int(fields[0]), int(fields[1])
    return INF, INF


def alttp_rank(detail, subs, groups, golden_log):
    """(class, group index, n) and a reason of one ALttP detail line."""
    tags, default, order = groups

    def group_of(line):
        fields = line.split()
        return tags.get(fields[2], default) if len(fields) > 2 else default

    def at(group, n, what):
        index = order.index(group) if group in order else len(order)
        return (1, index, n, 0), "write log %s %s %d" % (group, what, n)

    m = ALTTP_GROUP.match(detail)
    if m:
        group = m.group(1) if m.group(1) in order else group_of(subs[0][len("golden: "):] if subs else "")
        return at(group, int(m.group(2)), "write")
    m = ALTTP_SHORT.match(detail)
    if m or detail.startswith("c wrote no rom_writes.txt"):
        k = int(m.group(1)) + 1 if m else 1
        log = golden_log()
        group = group_of(log[k - 1]) if len(log) >= k else default
        return at(group, k, "write")
    if detail.startswith("rom_writes.txt:"):
        return (1, len(order), INF, 0), "write log %s" % detail
    attempt, draw = _draw(subs)
    where = "draw %d %d" % (attempt, draw) if draw != INF else detail.rstrip(":")
    return (0, attempt, draw, 0), "seed %s" % where


def cases_subdir(game_dir, run, case):
    if run == "e2e-combos":
        return "combos"
    if run == "e2e":
        return "cases"
    return "combos" if os.path.isdir(os.path.join(game_dir, "combos", case)) else "cases"


def rank_failure(failure, game, game_dir, context):
    if failure.target:
        failure.key, failure.reason = (0, failure.run_index), "target: %s" % failure.block[0]
        return
    ranks = []
    for detail, subs in details(failure.block):
        if game == "ladx":
            ranks.append(ladx_rank(detail, context))
        else:
            path = os.path.join(game_dir, cases_subdir(game_dir, failure.run, failure.case), failure.case,
                                "rom_writes.txt")
            ranks.append(alttp_rank(detail, subs, context,
                                    lambda: read_lines(path) if os.path.isfile(path) else []))
    key, reason = min(ranks) if ranks else ((0, 0, 0, INF), "seed (no detail line)")
    failure.key = (1, 0 if failure.case == "baseline" else 1) + key + (failure.run_index, failure.case)
    failure.reason = reason


def repro(failure, game, game_dir):
    root = os.path.normpath(os.path.join(game_dir, "..", ".."))
    if failure.target:
        if failure.block[0].startswith("PROJECTION ") and game == "ladx":
            case = failure.block[0].split(" ")[1]
            return "python3 -I -B %s --projections --cases %s --gen %s %s" % (
                os.path.join(game_dir, "run_e2e.py"), os.path.join(game_dir, cases_subdir(game_dir, failure.run, case)),
                os.path.join(game_dir, "build", "ladx_gen"), case)
        return "make -C %s %s" % (game_dir, failure.run)
    if game == "alttp":
        script = os.path.join(game_dir, "run_e2e.sh")
        if failure.run == "e2e-combos":
            return "RANDO_CASES=%s RANDO_CLAIMS=%s %s %s" % (
                os.path.join(game_dir, "combos"), os.path.join(game_dir, "combo_claims.txt"), script, failure.case)
        return "%s %s" % (script, failure.case)
    if failure.run in ("e2e", "e2e-combos"):
        return "make -C %s %s CASES=%s" % (game_dir, failure.run, failure.case)
    return "python3 -I -B %s --rom %s --no-ks --cases %s --gen %s --tables %s %s" % (
        os.path.join(game_dir, "run_e2e.py"), os.path.normpath(os.path.join(root, "tmp", "ladx", "base.gbc")),
        os.path.join(game_dir, cases_subdir(game_dir, failure.run, failure.case)),
        os.path.join(game_dir, "build", "ladx_gen"), os.path.normpath(os.path.join(root, "data", "ladx")), failure.case)


def main(argv):
    if len(argv) < 2:
        raise Usage("usage: rank_e2e.py <game test dir> <output file>...")
    game_dir, files = argv[0].rstrip("/"), argv[1:]
    game = os.path.basename(game_dir)
    if game not in ("alttp", "ladx"):
        raise Usage("%s is not a game's test directory (tests/alttp or tests/ladx)" % argv[0])
    context = ladx_order(game_dir) if game == "ladx" else alttp_groups(game_dir)
    named = [(os.path.splitext(os.path.basename(path))[0], path) for path in files]
    named.sort(key=lambda item: RUN_ORDER.index(item[0]) if item[0] in RUN_ORDER else len(RUN_ORDER))
    failures, setups, runs = [], [], []
    for index, (run, path) in enumerate(named):
        runs.append(run)
        found, setup = split_output(run, index, read_lines(path))
        failures += found
        setups += ["SETUP %s: %s" % (run, line) for line in setup]
    for failure in failures:
        rank_failure(failure, game, game_dir, context)
    failures.sort(key=lambda f: f.key)
    where = "%s (%s)" % (game_dir, ", ".join(runs))
    for line in setups:
        print(line)
    if not failures:
        print("rank-e2e: no failure in %s" % where)
        return 3 if setups else 0
    print("rank-e2e: %d failure%s in %s" % (len(failures), "" if len(failures) == 1 else "s", where))
    for i, failure in enumerate(failures):
        print("%d. %s %s: %s" % (i + 1, failure.run, failure.case or "(target)", failure.reason))
    top = failures[0]
    print("TOP %s %s" % (top.run, top.case or "(target)"))
    for line in top.block:
        print(line)
    print("REPRO %s" % repro(top, game, game_dir))
    return 1


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except Usage as e:
        print("rank-e2e: %s" % e, file=sys.stderr)
        sys.exit(2)
