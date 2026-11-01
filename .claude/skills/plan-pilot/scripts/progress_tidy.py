#!/usr/bin/env python3
"""Move the groups appended at the end of progress.md back under their step.

usage: progress_tidy.py [--stdout] [path]   (path defaults to progress.md)

The hook of progress_check.sh refuses a new line before an existing
[x], so once anything after a step is ticked, a step's new children are
appended at the end, in a top-level group naming the step:

  - [ ] Step <id> chunks       its children become the step's last children;
  - [ ] Added after step <id>  its children become the step's next siblings,
                               after the earlier groups for the same step.

The step is the item whose text starts with "<id> " (one level below the
top). The group lines themselves are dropped, every other line is kept with
its text and tick. Rewrites the file in place (--stdout prints it instead).
A group naming no step leaves the file unchanged and exits 1. The hook
accepts a commit whose only change to progress.md is this tidy-up.
"""

import re
import sys

ITEM = re.compile(r"^((?:  )*)- \[[ x]\] (.*)$")
CHUNKS = re.compile(r"^Step (\S+) chunks$")
ADDED = re.compile(r"^Added after step (\S+)$")


class TidyError(Exception):
    pass


def parse(text):
    """Return the items as [depth, line, children] trees, top level first."""
    roots, stack = [], []
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip():
            continue
        match = ITEM.match(line)
        if not match:
            raise TidyError(f"line {number} is not a list item: {line}")
        depth = len(match.group(1)) // 2
        if depth > len(stack):
            raise TidyError(f"line {number} is indented too deep: {line}")
        node = [depth, line, []]
        del stack[depth:]
        (stack[-1][2] if stack else roots).append(node)
        stack.append(node)
    return roots


def text_of(node):
    return ITEM.match(node[1]).group(2)


def reindent(node, depth):
    node[1] = "  " * depth + node[1].lstrip(" ")
    node[0] = depth
    for child in node[2]:
        reindent(child, depth + 1)


def tidy(text):
    roots = parse(text)
    steps = {}
    for root in roots:
        for step in root[2]:
            steps.setdefault(text_of(step).split(" ", 1)[0], (root, step))
    kept, groups = [], []
    for root in roots:
        name = text_of(root)
        match = CHUNKS.match(name) or ADDED.match(name)
        if match:
            groups.append((match.re is ADDED, match.group(1), root))
        else:
            kept.append(root)
    inserted = {}
    for is_added, step_id, group in groups:
        if step_id not in steps:
            raise TidyError(f"no step {step_id} for group: {group[1].strip()}")
        parent, step = steps[step_id]
        if is_added:
            at = parent[2].index(step) + 1 + inserted.get(id(step), 0)
            parent[2][at:at] = group[2]
            inserted[id(step)] = inserted.get(id(step), 0) + len(group[2])
            depth = 1
        else:
            step[2].extend(group[2])
            depth = 2
        for child in group[2]:
            reindent(child, depth)
    lines = []

    def emit(node):
        lines.append(node[1])
        for child in node[2]:
            emit(child)

    for root in kept:
        emit(root)
    return "".join(line + "\n" for line in lines)


def main(argv):
    args = argv[1:]
    to_stdout = "--stdout" in args
    args = [a for a in args if a != "--stdout"]
    path = args[0] if args else "progress.md"
    try:
        with open(path) as f:
            text = f.read()
        result = tidy(text)
    except (OSError, TidyError) as error:
        print(f"progress_tidy: {error}", file=sys.stderr)
        return 1
    if to_stdout:
        sys.stdout.write(result)
    elif result != text:
        with open(path, "w") as f:
            f.write(result)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
