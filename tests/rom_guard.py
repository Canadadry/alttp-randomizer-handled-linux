"""Usage: python3 -I tests/rom_guard.py [DIR]

Fails, naming each path and why, when git's index in DIR's work tree (default: the current
directory's) holds a .gb, .gbc, .sfc or .smc file, in any letter case, or a blob whose bytes are the
LADX English 1.0 ROM (SHA-1) or the ALttP JP 1.0 ROM (MD5, with or without a 512-byte copier
header), whatever its name. Proves itself first on planted files in throwaway repositories.
"""

import hashlib
import os
import subprocess
import sys
import tempfile

LADX_SHA1 = "d90ac17e9bf17b6c61624ad9f05447bdb5efc01a"
ALTTP_MD5 = "03a63945398191337e896e5771f77173"
EXTENSIONS = (".gb", ".gbc", ".sfc", ".smc")
COPIER_HEADER = 512
ALTTP_SIZE = 1048576
GITLINK = "160000"

R_EXT = "forbidden extension"
R_LADX = "LADX ROM (SHA-1)"
R_ALTTP = "ALttP JP 1.0 ROM (MD5)"
R_ALTTP_HEADER = "ALttP JP 1.0 ROM with copier header (MD5)"


class SetupError(Exception):
    pass


def git(worktree, args, env=None, data=None):
    try:
        result = subprocess.run(["git", "-C", worktree, *args], input=data, capture_output=True,
                                env=env)
    except FileNotFoundError:
        raise SetupError("git not found")
    if result.returncode != 0:
        raise SetupError(f"git {args[0]} in {worktree}: "
                         + result.stderr.decode(errors="replace").strip())
    return result.stdout


def index_entries(worktree, env=None):
    """(mode, object id, path) of every index entry, stage entries included."""
    entries = []
    for record in git(worktree, ["ls-files", "-s", "-z"], env).split(b"\0"):
        if not record:
            continue
        meta, path = record.split(b"\t", 1)
        mode, oid, _stage = meta.decode().split()
        entries.append((mode, oid, path.decode(errors="surrogateescape")))
    return entries


def blobs(worktree, oids, env=None):
    """The bytes of each object id, in order."""
    if not oids:
        return []
    out = git(worktree, ["cat-file", "--batch"], env, ("\n".join(oids) + "\n").encode())
    contents, pos = [], 0
    for _ in oids:
        header_end = out.index(b"\n", pos)
        size = int(out[pos:header_end].split()[2])
        start = header_end + 1
        contents.append(out[start:start + size])
        pos = start + size + 1
    return contents


def scan(worktree, ladx_sha1s, alttp_md5s, env=None):
    """The (path, reason) hits of the index of the work tree holding worktree, every one of them.
    Raises SetupError when git is missing or worktree is not in a git work tree."""
    root = git(worktree, ["rev-parse", "--show-toplevel"], env).decode().strip()
    entries = index_entries(root, env)
    hits = []
    for _mode, _oid, path in entries:
        if path.lower().endswith(EXTENSIONS):
            hits.append((path, R_EXT))
    content_entries = [e for e in entries if e[0] != GITLINK]
    for (_mode, _oid, path), data in zip(content_entries,
                                         blobs(root, [e[1] for e in content_entries], env)):
        if hashlib.sha1(data).hexdigest() in ladx_sha1s:
            hits.append((path, R_LADX))
        if hashlib.md5(data).hexdigest() in alttp_md5s:
            hits.append((path, R_ALTTP))
        if (len(data) == ALTTP_SIZE + COPIER_HEADER
                and hashlib.md5(data[COPIER_HEADER:]).hexdigest() in alttp_md5s):
            hits.append((path, R_ALTTP_HEADER))
    return len(entries), hits


def pattern(size, seed):
    return bytes((i * 7 + seed + (i >> 10)) & 0xFF for i in range(size))


def planted_repo(base, name, files, env, ignored=(), deleted=()):
    """A git repository base/name with files ({path: bytes}) staged; ignored paths are written,
    git-ignored and left untracked; deleted paths are staged then removed from the work tree."""
    repo = os.path.join(base, name)
    os.makedirs(repo)
    git(repo, ["init", "-q"], env)
    for path, data in files.items():
        full = os.path.join(repo, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)
    if files:
        git(repo, ["add", "-f", "--", *files], env)
    for path in deleted:
        os.remove(os.path.join(repo, path))
    if ignored:
        with open(os.path.join(repo, ".gitignore"), "w") as f:
            f.write("".join(p + "\n" for p in ignored))
        for path in ignored:
            with open(os.path.join(repo, path), "wb") as f:
                f.write(b"ignored")
    return repo


def self_test():
    """The failures of the guard on planted repositories, empty when it works."""
    env = dict(os.environ, GIT_CONFIG_GLOBAL=os.devnull, GIT_CONFIG_NOSYSTEM="1")
    ladx_like = pattern(4096, 1)
    alttp_like = pattern(4096, 2)
    headered = pattern(COPIER_HEADER, 3) + pattern(ALTTP_SIZE, 4)
    ladx = {hashlib.sha1(ladx_like).hexdigest()}
    alttp = {hashlib.md5(alttp_like).hexdigest(), hashlib.md5(headered[COPIER_HEADER:]).hexdigest()}
    caught = [
        ("gb", {"x.gb": b"rom"}, (), "x.gb", R_EXT),
        ("gbc", {"x.gbc": b"rom"}, (), "x.gbc", R_EXT),
        ("sfc", {"x.sfc": b"rom"}, (), "x.sfc", R_EXT),
        ("smc", {"x.smc": b"rom"}, (), "x.smc", R_EXT),
        ("upper", {"a/b/BASE.SFC": b"rom"}, (), "a/b/BASE.SFC", R_EXT),
        ("staged-deleted", {"tmp/ladx/z.gbc": b"rom"}, ("tmp/ladx/z.gbc",), "tmp/ladx/z.gbc",
         R_EXT),
        ("ladx", {"blob.bin": ladx_like}, (), "blob.bin", R_LADX),
        ("alttp", {"data/base": alttp_like}, (), "data/base", R_ALTTP),
        ("alttp-header", {"fixture.dat": headered}, (), "fixture.dat", R_ALTTP_HEADER),
    ]
    failures = []
    with tempfile.TemporaryDirectory() as base:
        for name, files, deleted, path, reason in caught:
            repo = planted_repo(base, name, files, env, deleted=deleted)
            _n, hits = scan(repo, ladx, alttp, env)
            if hits != [(path, reason)]:
                failures.append(f"{name}: expected {path}: {reason}, got {hits}")
        clean = planted_repo(base, "clean", {"data/base.bps": pattern(2048, 5), "x.gba": b"gba",
                                             "notes.sfc.txt": b"text", "README.md": b"readme"},
                             env, ignored=("tmp.sfc",))
        n, hits = scan(clean, ladx, alttp, env)
        if hits:
            failures.append(f"clean repository refused: {hits}")
        if n != 4:
            failures.append(f"clean repository: expected 4 tracked files, counted {n}")
    return failures


def main(argv):
    if len(argv) > 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    worktree = argv[1] if len(argv) == 2 else os.getcwd()
    try:
        failures = self_test()
        for failure in failures:
            print(f"rom_guard: self-test: {failure}", file=sys.stderr)
        if failures:
            return 1
        n, hits = scan(worktree, {LADX_SHA1}, {ALTTP_MD5})
    except SetupError as e:
        print(f"setup: rom_guard: {e}", file=sys.stderr)
        return 2
    for path, reason in hits:
        print(f"rom_guard: {path}: {reason}", file=sys.stderr)
    if hits:
        print("rom_guard: no Nintendo ROM in the repository: unstage each with git rm --cached",
              file=sys.stderr)
        return 1
    print(f"rom_guard: {n} tracked files, no ROM")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
