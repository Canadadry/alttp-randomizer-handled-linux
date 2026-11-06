---
title: "Phase 2's ROM-free tooling, self-proven"
description: "Phase 2 needs a Nintendo-bytes check, a ROM read trace, a ROM fetcher, a push gate and an inventory checker it can trust the moment the ROM arrives, and none of them exists yet."
status: needs-triage
---

## Plan step (verbatim, `docs/plan-3.md`, Phase 1, step 9)

> 9. **Phase 2's ROM-free tooling, self-proven.** Write and prove on planted synthetic inputs: the Nintendo-bytes check (planted literal runs), the read-trace instrumentation (planted reads, one per channel), `fetch_rom.sh`'s SHA-1 refusal (a planted wrong file), Phase 2's push gate as a pre-push hook chained with `no_claude_branch.sh` (it reads the link from the environment, never a file, and refuses a planted link in a commit, a planted literal run against a planted ROM and a planted stray untracked file, while a clean push passes) and the script that diffs the inventory against `grep` of the frozen sources. Commit a static draft of `notes/ladx-inventory.md`: `generateRom`'s call list, each call's setting dependencies and RNG use, with the grep-diff script passing on it. Phase 2 then only calibrates, traces and decides on real data. Verified by each tool's planted-input self-proof and the grep-diff script passing on the draft.

The §2 rules that apply: "How the ROM reaches the oracle" (link only in `LADX_ROM_URL`, never in a tracked file, a note, a commit message or a PR; download into `tmp/ladx/`, refused unless SHA-1 `d90ac17e…`; a dead link is a `setup:` line); "Nothing in `data/ladx/` or in the LADX goldens contains Nintendo bytes" (a check failing on any literal run of 4 bytes or more that also appears in the source ROM); "The LADX oracle needs the real ROM" (`ROM.patch`'s old-byte check, `ROMWithTables`' parsed tables); "`CLAUDE.md` is updated in the same steps as the code it describes" (the "No Nintendo ROM" ground rule rewritten for the temporary link). Phase 2's preamble defines the gate: `git log -p` of the branch searched for the link with `grep -F`, the Nintendo-bytes check on every file the push adds or changes, and refusal on an untracked file outside the git-ignored paths. Phase 2 step 3 defines the read trace (ROM bytes: indexing, slicing, iteration, search, whole-bank sums; and the `ROMWithTables` parsed tables; every read outside `ROM.patch`'s old-byte check; tagged with the inventory group running).

## Problem Statement

Phase 2 is the only phase that holds the real *Link's Awakening DX* ROM, through a temporary link the owner pasted once. Everything it does with that ROM rests on five tools that do not exist yet: a fetcher that gets the ROM and refuses any other file, a check that no committed byte run comes from the ROM, a push gate that keeps the link and those bytes out of the remote, a trace of every ROM read LADXR makes, and a script proving the inventory of `generateRom` matches LADXR's sources. If those tools are written during Phase 2, they are written and debugged while the ROM is on disk and the link is live, and the first thing that could leak the link or Nintendo bytes is the tool meant to stop it. Writing them now, ROM-free, and proving each one on planted synthetic inputs, means Phase 2 only calibrates them on real data.

The same goes for the inventory: Phase 2 step 3 must classify every call of `generator.generateRom`. Its static half (which calls exist, under which settings, which ones take the RNG or read the placement) needs no ROM and can be drafted and machine-checked now, so that Phase 2 spends its ROM time on tracing and deciding.

## Solution

Five tools in `tests/ladx/oracle/`, next to step 8's oracle, each proving itself on planted inputs before it does real work, and one committed draft:

- **`nintendo_bytes.py`**, the Nintendo-bytes check: given a ROM and files or directories, it prints one `LITERAL` line per byte run of 4 or more bytes that also occurs in the ROM, looking at each file's raw bytes and at the bytes its hex-digit runs decode to, then a summary ending in `<k> literal runs`. It proves itself first on planted files against a planted synthetic ROM.
- **`fetch_rom.sh`**, the ROM fetcher: reads the URL from its argument or `LADX_ROM_URL`, never from a file and never echoing it; downloads into `tmp/ladx/base.gbc`; refuses any SHA-1 but `d90ac17e…`, deleting the file; prints only the SHA-1. Idempotent: a correct ROM already there is not downloaded again. It proves its refusal first on a planted wrong file served by a `file://` URL.
- **`push_gate.py`**, Phase 2's push gate, installed as a pre-push hook chained with `no_claude_branch.sh`: it refuses a push when the link from `LADX_ROM_URL` is in `git log -p` of a pushed commit, when any blob the push uploads holds a literal run from the ROM, or when an untracked file sits outside the git-ignored paths (the PRD queue `docs/prd/` excepted). It fails closed: no link in the environment, or no checked ROM, refuses. It proves itself on planted repositories before it installs. It is written and proven now, and armed on the real repository only in Phase 2 step 1.
- **`read_trace.py`**, the read-trace instrumentation: it instruments LADXR's `ROM` and `ROMWithTables` so that every read of the ROM's bytes or of a parsed table, outside `ROM.patch`, is recorded with its channel, its target, the LADXR function that made it and the inventory site running (resolved to its group through the inventory). It proves itself on planted reads, one per channel, against LADXR's real `ROM` class and a synthetic ROM.
- **`inventory_check.py`**, the grep-diff script: it extracts from the frozen (patched) LADXR sources every statement of `generateRom`, of `Randomizer.__init__` after the fill, of `ROMWithTables.__init__` (the tables) and `ROMWithTables.save`, and of `ROM.save`, with the settings, arguments, RNG and placement each one names, and diffs that against `notes/ladx-inventory.md` row by row. It proves itself on a planted source tree and inventory.
- **`notes/ladx-inventory.md`**, a static draft: every row the script extracts, in source order, with its grep columns, a free notes column for what reading the callee showed (transitive RNG use, reads), a draft side and a draft group. The script passes on it.

`no_claude_branch.sh --install` learns to leave a chained pre-push hook alone, so that the `SessionStart` hook never silently disarms the gate. The pure parts of every tool are unit-tested in `make test` with no LADXR, no ROM and no network. `CLAUDE.md` describes the tools and rewrites the "No Nintendo ROM" ground rule for the temporary link.

## User Stories

1. As a Phase 2 session, I want a script that downloads the ROM from `LADX_ROM_URL`, so that I never type or store the link myself.
2. As the owner, I want the fetcher to never print the link, not even on a download error, so that the link cannot reach a transcript excerpt, a step note or a commit through copied output.
3. As the owner, I want the fetcher to read the link only from its argument or the environment, never from a file, so that no file in the working tree ever holds it.
4. As a Phase 2 session, I want the fetcher to refuse and delete any file whose SHA-1 is not `d90ac17e…`, so that no step runs on a wrong ROM.
5. As a Phase 2 session, I want the fetcher to print only the SHA-1 on success, so that I can check it at a glance and paste it into a note safely.
6. As a resumed session, I want the fetcher to skip the download when `tmp/ladx/base.gbc` is already the right ROM, so that every session start can re-run it cheaply.
7. As a Phase 2 session, I want a dead link, a blocked host or a missing `curl` reported as a `setup:` line with curl's exit code and the HTTP status, so that I know it is a setup problem and whether to request the host through the environment tools.
8. As the owner, I want the fetcher to prove its refusal on a planted wrong file before every real fetch, so that a broken check cannot accept a wrong ROM.
9. As an agent, I want the download written to a partial file and moved into place only after the SHA-1 check, so that an interrupted or refused download never leaves a `base.gbc`.
10. As the owner, I want a check that finds any run of 4 or more bytes in a file that also occurs in the ROM, so that §2's "no Nintendo bytes" rule is enforced, not hoped for.
11. As the owner, I want the check to also decode hex-digit runs in text files, so that ROM bytes written as hex in a golden or a table are caught too.
12. As the owner, I want the hex view decoded at both nibble alignments, so that a hex run starting mid-byte cannot hide a literal.
13. As a Phase 2 session, I want each hit reported with file, view, offset and length, and never with the bytes themselves, so that the report holds no Nintendo bytes either.
14. As a Phase 2 session, I want the summary to end in `<k> literal runs`, so that Phase 2's "prints `0 literal runs`" verification reads one line.
15. As a Phase 2 session, I want the check to accept any `--rom` file and print the ROM's SHA-1 in its summary, so that a run against the wrong ROM is visible in the note.
16. As a Phase 2 session calibrating the check, I want the minimum run length as a parameter (default 4), so that step 1's calibration can measure hit counts at other lengths without editing the tool.
17. As the owner, I want the check to prove itself on planted files against a planted synthetic ROM on every run, so that a check that finds nothing because it is broken never prints `0 literal runs`.
18. As the owner, I want a pre-push gate that searches every pushed commit's `git log -p` for the link, so that the link never reaches the remote in a message or a diff.
19. As the owner, I want that search to treat binary files as text, so that a link inside a binary blob is not hidden behind "Binary files differ".
20. As the owner, I want the gate to run the Nintendo-bytes check on every blob the push uploads, intermediate commits included, so that a literal committed then removed is still refused.
21. As the owner, I want the gate to refuse a push while an untracked file sits outside the git-ignored paths, so that a stray `LADXR_<seed>.*` LADXR wrote into the working directory is noticed before it is ever committed.
22. As plan-pilot, I want the gate to accept untracked files under `docs/prd/` (the PRD queue, untracked by design while a step runs), while still refusing one that holds the link, so that the normal workflow is not blocked.
23. As the owner, I want the gate to fail closed (no `LADX_ROM_URL`, no ROM, wrong ROM SHA-1, or its own self-proof failing), so that a misconfigured gate refuses rather than waves pushes through.
24. As the owner, I want the gate's refusal messages to name the commit, path or blob, never the link or the bytes, so that the refusal itself leaks nothing.
25. As the owner, I want the gate chained with `no_claude_branch.sh` in one pre-push hook, both fed the same ref lines, so that a `claude/*` branch is still refused while the gate is armed.
26. As the owner, I want both hooks of the chain to run and report even when the first refuses, so that one push shows every reason at once.
27. As the owner, I want the `SessionStart` hook's `no_claude_branch.sh --install` to leave the chained hook in place, so that starting a session never silently disarms the gate.
28. As the owner, I want the gate's install to leave the `prepare-commit-msg` hook (`progress.md`'s guard) untouched, so that the two guards coexist.
29. As a Phase 2 session, I want `push_gate.py --install` to prove the gate on planted repositories first and to refuse to install if that fails, so that "armed" always means "proven".
30. As the session closing the run (Phase 4 step 8), I want `push_gate.py --uninstall` to restore the plain `no_claude_branch.sh` hook, so that pushes work again after the ROM is gone.
31. As the owner, I want the gate's self-proof to plant a link in a commit message, a link in a file diff, a link in a binary file and a link only in an older commit, so that each place the link can hide is shown to be searched.
32. As the owner, I want the self-proof to show a clean push passing through the real installed chain, so that the gate is shown not to block everything.
33. As a Phase 2 session, I want a read trace of every ROM read LADXR makes outside `ROM.patch`, so that step 3's inventory knows which seed-dependent calls read Nintendo data.
34. As that session, I want the trace to cover indexing, slicing, iteration (whole-bank sums included), search, comparison and conversion of the ROM's banks, so that every way Python reads a `bytearray` from Python code is seen.
35. As that session, I want the trace to cover reads of the `ROMWithTables` parsed tables (texts, entities, rooms, room sprite data, backgrounds), so that a decision made on parsed data is seen too.
36. As that session, I want reads made while the tables are parsed tagged with the parse site, so that parsing is told apart from the groups.
37. As that session, I want every read tagged with the inventory site running (the innermost frame of an inventory section) and its group from the inventory, so that step 3's coverage check is a lookup.
38. As that session, I want `ROM.patch`'s own reads (old-byte check, int-length form, the mismatch search) excluded, and writes never recorded, so that the trace holds only reads that can influence output.
39. As that session, I want the trace aggregated per group, site, caller, channel and target with a count and an offset range, and a raw mode for debugging, so that a whole-ROM run stays readable and deterministic.
40. As that session, I want a list of the places in LADXR's sources that read a bank through the buffer protocol (`memoryview`, `struct.unpack_from`, `hashlib`, file writes), which a Python-level trace cannot see, so that the trace's blind spots are reviewed, not assumed empty.
41. As the owner, I want the trace to prove itself on planted reads, one per channel, each reported with its planted group, plus a planted `ROM.patch` read and a planted write that must not appear, so that a channel the tracer misses is caught before Phase 2 relies on it.
42. As a Phase 2 session, I want a static draft of `notes/ladx-inventory.md` already committed, listing every statement of `generateRom` in order, so that step 3 starts from a complete, checked list.
43. As that session, I want each row to state the settings its condition and arguments name, the `args` flags it depends on, whether it takes `rnd` or `seed` or its callee uses the global `random`, and whether it reads the placement or the world setup, so that the settings-only/seed-dependent split starts from facts.
44. As that session, I want a notes column where the drafter records what reading the callee showed (for example `hints.addHints` reaching `utils.randomNumber`, which draws from the global `random`), so that transitive RNG use found by reading is not lost.
45. As the owner, I want a script that diffs the inventory against the frozen sources, so that the inventory can never drift from LADXR or from `ladxr.patch`.
46. As the owner, I want that script to report a missing row, a stale row and a wrong grep column separately, so that a failure says exactly what to fix.
47. As the owner, I want the script to key rows by stripped line text in source order, not line numbers, so that `ladxr.patch` shifting lines does not break it.
48. As the owner, I want the script to prove itself on a planted source tree and inventory first, so that a script that matches nothing cannot pass.
49. As the owner, I want the inventory to also list every `ROMWithTables` table and every statement of `ROMWithTables.save` and `ROM.save`, so that step 3's re-serialization analysis starts from a checked list.
50. As an agent, I want `make test` to unit-test the pure parts of every tool without LADXR, a ROM or the network, in about a second or two, so that the fast loop stays fast and the tools stay trusted.
51. As a later agent, I want `CLAUDE.md` to describe each tool, that it is outside the net, and the rewritten "No Nintendo ROM" ground rule, so that I know how the ROM arrives and what guards it.
52. As the owner, I want nothing this step commits to come from the ROM or to contain the link, so that the step is safe under the ROM guard and the gate it builds.

## Implementation Decisions

### Layout and shared conventions

- Every new tool lives in `tests/ladx/oracle/` (step 8's folder): `nintendo_bytes.py`, `fetch_rom.sh`, `push_gate.py`, `read_trace.py`, `inventory_check.py`, and their unit tests `test_nintendo_bytes.py`, `test_push_gate.py`, `test_read_trace.py`, `test_inventory_check.py`. The draft is `notes/ladx-inventory.md`.
- Python tools use the standard library only and run with `python3 -I`, loading sibling modules by path with `importlib` (prior art `tests/rom_guard.py`, `tests/alttp/test_package.py`). They stay importable on Python 3.8+, so `make test` runs anywhere.
- Each tool that touches LADXR takes the checkout as `--ladxr <dir>`, default `tests/ladx/oracle/ladxr/` (step 8's `setup.sh` checkout, patched), and does not depend on step 8's `ladx_oracle.py` helpers; a missing checkout is a `setup: … run tests/ladx/oracle/setup.sh` line, exit 2.
- LADXR's sources have CRLF line endings (`generator.py` among them): every reader strips `\r` before matching.
- Exit codes: 0 pass, 1 finding (a refusal, a literal run, a diff), 2 `setup:` (missing tool, checkout, ROM or link). A self-proof failure is exit 1 with a line naming the planted case that misbehaved.
- Throwaway files go in `tmp/` (planted repositories and ROMs in a fresh `tempfile` directory removed on exit). The ROM's only home is `tmp/ladx/base.gbc`; `tmp/` is already git-ignored, so `.gitignore` gains nothing.
- None of the tools prints the link, ROM bytes or a hex dump of a hit.

### `nintendo_bytes.py` (the Nintendo-bytes check)

- Interface: `nintendo_bytes.py [--rom <file>] [--min <n>] <file-or-dir>…`. `--rom` defaults to `tmp/ladx/base.gbc`; a missing ROM is a `setup:` line. Any file is accepted as `--rom` (calibration and planted runs); the summary names its SHA-1 so the note shows which ROM was used. Directories are walked in sorted order, `.git` skipped.
- Module interface (what the gate imports): `RomIndex(rom_bytes, min_len)` built once (the set of every `min_len`-byte window of the ROM), and `literal_runs(data, index) -> [(view, offset, length)]`.
- Views: `raw` (the file's bytes) and `hex` (every maximal run of ASCII hex digits at least `2 × min` long, decoded at nibble alignment 0 and 1; offsets are reported in the file's characters). Adjacent and overlapping window hits merge into one run; a merged run is reported even if its full length does not occur contiguously in the ROM (conservative, recorded in the docstring).
- Output: `LITERAL <path> <view> @<offset> <length> bytes` per run, sorted by path then offset, then `nintendo-bytes: <n> files, rom <sha1>, min <m>, <k> literal runs`. Exit 1 when `k > 0`.
- No exemption list and no special case for trivial runs (`00000000`, `FFFFFFFF`): §2 states the rule plainly, and Phase 2 step 1's calibration decides any refinement, in its own commit. Known calibration inputs, recorded in the docstring's "calibration" note for Phase 2: random data (SHA-512 digests in step 8's `logic_vectors.txt`, `data/base.bps`) collides with a 1 MB ROM's 4-byte windows by chance at a rate of about 1e-4 per window.
- Self-proof on every run, before the real scan: a planted 64 KiB synthetic ROM (bytes from Python's `random.Random` with a fixed seed, so no Nintendo data) and planted files: a clean file (0 runs); a 3-byte slice of the ROM (0 runs at `min 4`); a 4-byte slice (1 raw run of 4); a 9-byte slice inside text (1 raw run of 9, merged); a 5-byte slice written as upper-case hex at an odd character offset (1 hex run); the same in lower-case hex (1 hex run). Each count must match exactly.

### `fetch_rom.sh`

- POSIX `sh`, `set -eu`, `LC_ALL=C`, never `set -x`. Usage `fetch_rom.sh [<url>]`; the URL is the argument, else `LADX_ROM_URL`. It never reads a file for the URL and never writes it anywhere.
- Target `<repo>/tmp/ladx/base.gbc` (repository top from the script's own path). Constant `LADX_SHA1=d90ac17e9bf17b6c61624ad9f05447bdb5efc01a`.
- Order: self-proof; then if the target exists with the right SHA-1, print it and exit 0 (no download, no URL needed); else, with no URL, `setup: fetch_rom: no URL (argument or LADX_ROM_URL) and no ROM in tmp/ladx/`, exit 2; else download to `base.gbc.part` with `curl -fsSL --retry 3`, curl's stderr discarded and `-w '%{http_code}'` kept; on failure delete the part and print `setup: fetch_rom: download failed (curl exit <n>, HTTP <code>)`, exit 2; compute the SHA-1 (`sha1sum`, else `shasum -a 1`, else a `setup:` line); a wrong SHA-1 deletes the part and prints `setup: fetch_rom: refused, SHA-1 <x> is not the LADX English 1.0 ROM (d90ac17e…)`, exit 2 (a wrong file behind the link is a setup problem, like a dead link); the right one is moved into place and printed alone on stdout.
- The download-and-check is one shell function taking the URL, the target and the expected SHA-1; the command line always passes the constant. The self-proof calls the function directly on a planted synthetic file (4 KiB, `printf`-made, no Nintendo data) served as `file://`, in a `mktemp -d` directory: with the LADX SHA-1 it must refuse (exit 2, no target, no part, and the planted URL absent from its output); with the planted file's own SHA-1 it must accept (target present, stdout exactly that SHA-1). Self-proof output goes nowhere on success; on failure one `fetch_rom: self-test failed: <case>` line, exit 1.
- `curl` is needed only by this script (proxy-configured in the container); `make test` does not run it, so `make test` still needs only `gcc`, `make` and `python3`.

### `push_gate.py` (Phase 2's push gate)

- Hook mode (no flag): git's pre-push arguments and ref lines on stdin. It reads the link from `LADX_ROM_URL` only; unset, empty or shorter than 8 characters → `push-gate: setup: LADX_ROM_URL is not set, push refused`, exit 1 (fail closed). It reads `tmp/ladx/base.gbc` and requires SHA-1 `d90ac17e…`; missing or wrong → `push-gate: setup: no checked ROM in tmp/ladx/ (run tests/ladx/oracle/fetch_rom.sh), push refused`, exit 1.
- For each ref line whose local sha is not all zeros (deletions are skipped):
  - **Link**: `git log -p --text --format=fuller <local sha>` (the whole history reachable from the pushed commit, messages and diffs, binaries as text) is scanned for the link's exact bytes (`grep -F` semantics, done in Python on the stream); each hit names the commit it falls under: `push-gate: refused: the link is in commit <short sha>`.
  - **Bytes**: every blob the push uploads, `git rev-list --objects <local sha> --not --remotes` filtered to blobs, is run through `nintendo_bytes`'s `literal_runs` against the ROM: `push-gate: refused: <path> (blob <short sha>): literal run of <n> bytes at @<offset> (<view>)`. Blob versions of intermediate commits are included because they are uploaded.
- **Untracked**: `git status --porcelain -z --untracked-files=all`; every `??` entry outside `docs/prd/` refuses: `push-gate: refused: untracked file <path>`. Untracked files under `docs/prd/` are allowed (plan-pilot keeps the next PRD there, untracked, while the current step pushes) but are still scanned for the link. Ignored files (`tmp/`, the LADXR checkout) are never looked at.
- Success: `push-gate: ok, <c> commits, <b> blobs, no link, 0 literal runs, no stray untracked file`, exit 0. Every finding is printed before exiting 1.
- `--install`: runs the self-proof, refusing to install on failure; then writes the chained pre-push hook into `git rev-parse --git-common-dir`'s `hooks/` (shared by every worktree), but only if the existing pre-push is absent, `no_claude_branch.sh`'s own, or already the chain; any other hook is left alone with a message and exit 1 (the gate is then not armed, which must be visible). It never touches `prepare-commit-msg`. The chained hook (marker comment `# chained pre-push hook: no_claude_branch.sh, then tests/ladx/oracle/push_gate.py`) saves stdin to a temporary file, runs `sh .claude/hooks/no_claude_branch.sh "$@"` then `python3 -I tests/ladx/oracle/push_gate.py "$@"`, both from the repository top and both fed the saved ref lines, and exits 1 if either refused. A missing gate script fails the push (fail closed).
- `--uninstall`: removes the pre-push hook only if it is the chain, then runs `no_claude_branch.sh --install`.
- `--self-test` (also run by `--install`): in a temporary directory, a bare remote and a clone; a planted 64 KiB synthetic ROM; a copy of `push_gate.py`, `nintendo_bytes.py` and `no_claude_branch.sh` placed at their repository paths in the clone, the copy's expected ROM SHA-1 constant rewritten to the planted ROM's (asserted to occur exactly once), so the real tool keeps no override; the planted ROM at the clone's `tmp/ladx/base.gbc`, `tmp/` ignored; the chain installed with the copy's `--install` path (minus its own self-proof); a planted `prepare-commit-msg` hook. Then real `git push`es with `LADX_ROM_URL` set to a planted link, each case on its own branch: clean commit → pushed; link in a commit message → refused; link in a file → refused; link in a binary file → refused; link only in the parent of the pushed commit → refused; 4-byte ROM slice in an added file → refused; ROM slice added then removed in the next commit → refused; stray untracked file → refused; untracked `docs/prd/next/x.md` → pushed; the same holding the link → refused; `LADX_ROM_URL` unset → refused; a `claude/*` branch with a clean commit → refused by `no_claude_branch.sh` through the chain; and the planted `prepare-commit-msg` byte-identical after install. Each refused case is also checked for not echoing the planted link. One line per failing case, else `push-gate: self-test, <n> planted pushes as expected`.
- The gate is not armed on the real repository in this step: without the ROM it would refuse every Phase 1 push. Phase 2 step 1 arms it after fetching the ROM.

### `no_claude_branch.sh --install`

- Its rule changes from "overwrite any hook mentioning `no_claude_branch.sh`" to: absent, or byte-identical to its own hook text → write it; mentions `no_claude_branch.sh` but differs → `no_claude_branch: <hook> already chains no_claude_branch.sh, left alone`; otherwise unchanged ("not ours, left alone"). This is what keeps the `SessionStart` hook from replacing the gate's chain with the plain hook at every session start. The hook mode (refusing `claude/*`) is unchanged.

### `read_trace.py` (read-trace instrumentation)

- Module interface: `install(modules, sections, inventory=None) -> Trace`, where `modules` names LADXR's `rom.ROM`, `romTables.ROMWithTables` and `pointerTable.PointerTable` (stand-ins in unit tests), `sections` is the list of (file, function) whose frames mark a site (the inventory's sections, below), and `inventory` maps site keys to groups. `Trace.records()`, `Trace.write(path, raw=False)`, `uninstall()`.
- Bank instrumentation: `ROM.__init__` is wrapped so that, right after it reads the banks and before `ROMWithTables.__init__` parses anything, each bank becomes a `TracedBank` (a `bytearray` subclass holding the same bytes) and `banks` a list subclass that wraps any bank later assigned into it.
- Channels and the methods they cover: `index` (`bank[i]`), `slice` (`bank[a:b]`), `iter` (`__iter__`: `for`, `sum`, `list`, the header checksum), `search` (`find`, `rfind`, `index`, `rindex`, `count`, `startswith`, `endswith`, `__contains__`), `compare` (`__eq__`, `__ne__`, ordering), `convert` (`hex`, `decode`, and `__bytes__`, which `bytes(bank)` calls), and `table` (`__getitem__`, `__contains__`, `__len__` on the `ROMWithTables` tables, made traced by swapping each table instance's class for a traced subclass right after `ROMWithTables.__init__`). Writes (`__setitem__`) are never recorded.
- Exclusion: a read whose call stack holds the frame of `rom.ROM.patch` (matched by code object) is not recorded: that covers the old-byte check, the `old` given as an end offset, and the mismatch search.
- Site: the innermost stack frame whose (file, function) is one of the sections; its key is `<section>\t<stripped source line of that frame>` with an occurrence index when the line text repeats in the section, the same key the inventory uses. No section frame → site `-`. Group: the inventory's group for that key, else `-`. Caller: `<file>:<co_qualname>` of the innermost frame inside the LADXR checkout (`co_name` on Python before 3.11).
- Records: `<group>\t<site>\t<caller>\t<channel>\t<target>\t<detail>` with target `bank 0x1C` or `table texts`, detail the offset, `a:b` range, method name or table key in hex. `write` aggregates identical (group, site, caller, channel, target) with a count and the min/max offset (default), or writes every read in order (`raw=True`). Output is deterministic for a deterministic run.
- `--blind-spots [--ladxr <dir>]`: lists every line in the checkout's `.py` files (except `www/`) using `memoryview`, `unpack_from`, `pack_into`, `hashlib`, `.write(` or `.tobytes(`: buffer-protocol reads a Python-level trace cannot see. Output `BLIND <file>\t<stripped line>` then `read-trace: <n> buffer-protocol sites to review`; informational, for Phase 2 step 3's note.
- `--self-test [--ladxr <dir>]`: imports LADXR's real `rom.ROM` (no parsing) and `pointerTable.PointerTable` from the checkout; builds a ROM from 64 synthetic banks (fixed-seed bytes); instruments it with a planted "ROMWithTables" stand-in (a `rom.ROM` subclass whose constructor reads one byte, standing for the parse) and a planted `PointerTable` subclass over a tiny table crafted in the synthetic bytes; then a planted module, written to a temporary directory and imported, whose section function calls one planted function per channel. Expected: exactly one record per channel, each with its planted group and caller; the parse read tagged with the parse site; a planted `rom.patch` with matching old bytes, and a planted write, absent. Prints `read-trace: self-test, <n> channels traced, patch and writes excluded` or one line per mismatch.
- Not in this step: running the trace on a real `generateRom` (needs the ROM) and the coverage check against the inventory's sides: Phase 2 step 3.

### `inventory_check.py` (the grep-diff script)

- Interface: `inventory_check.py [--ladxr <dir>] [--inventory <file>] [--print]`. Defaults: the step 8 checkout, `notes/ladx-inventory.md`. `--print` writes the extracted rows as Markdown tables (used to start or refresh the draft by hand; never writes the file itself).
- Sections, in this order, each a (file, function, start) triple:
  1. `generator.py` `generateRom`, whole body;
  2. `randomizer.py` `Randomizer.__init__`, after the line `item_placer.run(self.rnd)`;
  3. `romTables.py` `ROMWithTables.__init__` (the table rows);
  4. `romTables.py` `ROMWithTables.save`;
  5. `rom.py` `ROM.save` and `ROM.fixHeader`.
- Extraction ("grep", line-based on the `\r`-stripped text, function bodies bounded by indentation): every non-blank, non-comment line of the section except `if`/`elif`/`else`/`for`/`while`/`try`/`except` headers, `return`, `import` and `print(` lines. Each row records:
  - `Call`: the stripped line text, plus `#<k>` for the k-th repeat of the same text in the section (`patches.aesthetics.fastText(rom)` has three);
  - `Settings`: sorted names matched by `settings\.(\w+)` in the line and in its whole enclosing condition chain (every `if`/`elif` header above it at lower indentation, and for `elif`/`else` every earlier header of the same chain), plus `*` when `settings` is passed whole; `-` if none;
  - `Args`: names matched by `args\.(\w+)` in the same scope; `-` if none;
  - `RNG (grep)`: `rnd` if the line names `rnd`, `seed` if it names `seed`, `global` if the callee (`patches.<m>.<f>`, `hints.<f>`, `mapgen.<f>`) has a `def <f>` body in `<m>.py` naming the module-level `random.`; comma-joined, else `-`. One level only: deeper use is the notes column's job and Phase 2's RNG trace's;
  - `Reads (grep)`: `placement` if the line or its scope names `item_list`, `iteminfo_list` or `spot`; `world_setup` if it names `world_setup`; `logic` if it passes `logic` whole; comma-joined, else `-`.
- Inventory format: one Markdown table per section under a `## <file> <function>` heading, columns `Call | Settings | Args | RNG (grep) | Reads (grep) | Notes | Side (draft) | Group (draft)`, rows in source order, `Call` in backticks, `|` in a cell escaped as `\|`. Text outside these tables is free.
- Diff: per section, the inventory's `Call` sequence against the extracted one (`difflib` alignment): `MISSING <section>: <call>`, `STALE <section>: <call>`, and for matched rows `MISMATCH <section>: <call>: <column> inventory <a>, source <b>` on the four grep columns. `Side (draft)` must be one of `table`, `C`, `none`, `unused`, `?`; `Group (draft)` must be non-empty. Summary: `inventory: <rows> rows in 5 sections match <checkout commit>` or exit 1.
- Self-proof first, on a planted source tree in a temporary directory (planted `generator.py` with CRLF endings, `randomizer.py`, `romTables.py`, `rom.py` and a `patches/` module with one function using global `random`) and a planted inventory: matching passes; a dropped row → `MISSING`; an extra row → `STALE`; a wrong `Settings` → `MISMATCH`; a wrong `RNG (grep)` for the global-`random` callee → `MISMATCH`; two swapped rows → `MISSING` plus `STALE`; a bad `Side (draft)` value → refused.

### `notes/ladx-inventory.md` (static draft)

- Head: what the file is (Phase 2 step 3's inventory, static draft from Phase 1 step 9), the pin and `ladxr.patch` it was checked against, how to check it (`inventory_check.py`), and what Phase 2 adds (ROM reads from the trace, final sides, final groups, extraction fixtures, evidence per seed-dependent group).
- The five section tables, every extracted row, about 160 rows for `generateRom` alone. `Notes` records what reading the callee showed: transitive global RNG (for example `utils.randomNumber` ignoring its `rnd` and drawing from the global `random`, reached through hints), reads of `rom.texts` or rooms, settings read inside the callee. `Side (draft)` is the drafter's guess: `table` for fixed or settings-only patches, `C` for seed- or placement-dependent writes, `none` for statements that write nothing (`assembler.const`, `print`), `unused` for tier-2/3 or `args` paths the oracle never takes (`pymod`, `romdebugmode`, `doubletrouble`, `gfxmod`, alternative overworlds, shopsanity), `?` when unclear. `Group (draft)` is a short name (`parse`, `fixed`, `items`, `hints`, `music`, `goal`, `bosses`, `enemizer`, `frame-counter`, `title`, `save`, …). Both draft columns are Phase 2 step 3's to settle.
- No ROM bytes, no hex dumps of ROM content (the call lines' own hex literals, such as `rom.patch(4, 0x36F9, "FA4EDB", "3E0000")`, are LADXR's MIT source text, quoted as source; the calibration in Phase 2 step 1 runs over this file too).

### Wiring, docs

- `make test`: the four new `test_*.py` join step 8's `test-ladx-oracle` target (its recipe becomes a `unittest discover` over `tests/ladx/oracle` with `-p 'test_*.py'`, under `python3 -I`). No tool's LADXR-, ROM- or network-dependent mode joins `make test` or `make check`.
- `CLAUDE.md`: Layout gains the five tools and `notes/ladx-inventory.md`; Checks gains one bullet per tool (what it does, its self-proof, outside the net, `setup:` lines) and the "Not in the net" list names them; Branches says the pre-push hook may be the gate's chain from Phase 2 on, and that `--uninstall` restores the plain one; the "No Nintendo ROM" ground rule is rewritten: no Nintendo ROM in the repository or in CI; the LADX oracle gets the real ROM from a temporary link held only in `LADX_ROM_URL` (never in a file, note, commit or PR), fetched by `fetch_rom.sh` into `tmp/ladx/` and refused unless SHA-1 `d90ac17e…`; nothing committed holds a literal run of the ROM, checked by `nintendo_bytes.py` and, while the gate is armed, before every push.
- Step note: each tool's self-proof summary line, `inventory_check.py`'s summary on the draft, the `--blind-spots` count, the number of draft rows per section and per `Side (draft)` value, and the `make test` time before and after.

### Hypotheses about step 8 (not done when this PRD was written)

- **H-8-1**: Step 8 creates `tests/ladx/oracle/` with `setup.sh`, which checks out LADXR at `4325207e…` into `tests/ladx/oracle/ladxr/` (git-ignored), applies `ladxr.patch`, honours `LADXR_DIR`, and prints `setup:` lines with exit 2 on a missing tool. This step's tools only read that checkout.
- **H-8-2**: Step 8's root `Makefile` target `test-ladx-oracle`, a prerequisite of `test`, runs `tests/ladx/oracle/test_oracle.py` with `python3 -I`. This step turns its recipe into a discover over `test_*.py`; if the target runs files differently, the four new test files are added to it the same way. If the target does not exist, this step creates it.
- **H-8-3**: `ladxr.patch` holds only determinism fixes (`patches/endscreen.py`'s `cats`, `spoilerLog.py`'s `accessibleItems`, and any the double run demanded) and changes no line of the five inventory sections; if it does, the draft follows the patched text, since the checker runs on the patched checkout.
- **H-8-4**: Step 8's `CLAUDE.md` edits add a Layout entry for `tests/ladx/oracle/` and separate Checks bullets and leave the "No Nintendo ROM" ground rule untouched; this step adds its own bullets next to them and rewrites the ground rule.
- **H-8-5**: Step 8 adds `tests/ladx/oracle/ladxr/` to `.gitignore` and nothing under `tmp/`; this step needs no `.gitignore` line.
- **H-8-6**: Step 8 tracks only text files (`.sh`, `.py`, `.txt`, `.patch`) and no `docs/prd/` file outside the PRD workflow; the gate, not armed here, is unaffected.
- **H-8-7**: The Python recorded by step 8 is 3.11 or later in the container, so `read_trace.py` can use `co_qualname` there; it falls back to `co_name` on older Python.

## Implementation Chunks

1. **Nintendo-bytes check.** `nintendo_bytes.py` with its self-proof, `test_nintendo_bytes.py`, the `test-ladx-oracle` recipe turned into a discover, `CLAUDE.md` bullet. Done when `make test` is green and `nintendo_bytes.py --rom <planted>` on a planted directory prints the expected runs.
2. **ROM fetcher.** `fetch_rom.sh` with its self-proof, `CLAUDE.md` bullet and the ground-rule rewrite. Done when `fetch_rom.sh` with no URL and no ROM prints its `setup:` line after a silent self-proof, and a deliberately broken refusal (local mutant, not committed) makes the self-proof fail.
3. **Push gate.** `push_gate.py` (hook mode, `--install`, `--uninstall`, `--self-test`), `test_push_gate.py`, the `no_claude_branch.sh --install` rule and its new test in `.claude/hooks/tests/`, `CLAUDE.md` Checks and Branches. Done when `push_gate.py --self-test` passes, `make test` is green, and the real repository's pre-push hook is still `no_claude_branch.sh`'s own.
4. **Read trace.** `read_trace.py` (module, `--self-test`, `--blind-spots`), `test_read_trace.py` on stand-in classes, `CLAUDE.md` bullet. Done when `--self-test` passes on the step 8 checkout and `make test` is green.
5. **Inventory checker and draft.** `inventory_check.py` with its self-proof, `test_inventory_check.py`, `notes/ladx-inventory.md`, `CLAUDE.md` bullet, step note. Done when `inventory_check.py` prints its summary on the draft and `make check` is green.

## Testing Decisions

- **A good test** drives a tool from the outside: the runs it reports for given bytes, the verdict of a real `git push` through the installed hook, the records left by planted reads, the diff lines for a planted inventory. Which helper builds a line, or in which order checks run, is not asserted.
- **`make test`** (Python `unittest`, standard library, `python3 -I`, no LADXR, no ROM, no network, under about two seconds for the four files; prior art `tests/rom_guard.py`'s planted repositories and `.claude/hooks/tests/test_no_claude_branch.py`):
  - `test_nintendo_bytes.py`: the self-proof's cases as separate tests (3 vs 4 bytes, merging, hex at both alignments and both cases, offsets), the summary line, exit codes, and that no hit line contains ROM bytes or their hex.
  - `test_push_gate.py`: the self-proof passes; separate tests for a link in a binary file (the `--text` trap), a literal only in an intermediate commit, `docs/prd/` allowed and link-scanned, unset `LADX_ROM_URL` refusing, a wrong ROM SHA-1 refusing, refusals never echoing the link, `--install` leaving `prepare-commit-msg` and a foreign pre-push alone, `--uninstall` restoring `no_claude_branch.sh`'s hook.
  - `.claude/hooks/tests/test_no_claude_branch.py` gains: `--install` leaves a hook that chains `no_claude_branch.sh` byte-identical; `--install` still rewrites its own hook. Its existing tests stay as they are.
  - `test_read_trace.py`: on stand-in `ROM`, `ROMWithTables` and `PointerTable` classes, one test per channel (record present with its site, group and caller), the `ROM.patch` exclusion, writes not recorded, a bank assigned later still traced, the aggregation's count and range, the occurrence index of a repeated site line.
  - `test_inventory_check.py`: the self-proof's cases, plus CRLF sources, the `elif`/`else` condition chain, `settings` passed whole (`*`), `#<k>` repeats, and `|` escaping in a cell.
- **Self-proofs** run on every invocation of `nintendo_bytes.py`, `fetch_rom.sh` and `inventory_check.py`, on `--install` and `--self-test` of `push_gate.py`, and on `--self-test` of `read_trace.py` (prior art: `goldens.sh --compare`, `no_sdl.sh`, `rom_guard.py`).
- **Acceptance**, outside the net, in the step note: every self-proof's summary line; `read_trace.py --self-test` and `--blind-spots` on the step 8 checkout; `inventory_check.py` passing on the draft; `make check` green.
- Nothing existing is weakened; no golden, snapshot or fixture is touched.

## Out of Scope

- Fetching the real ROM, arming the gate on the real repository, calibrating the Nintendo-bytes check, running the read trace on `generateRom`, and the coverage check of trace against inventory: Phase 2 steps 1 and 3.
- Settling the inventory's sides and groups, ROM reads, extraction fixtures and evidence per group: Phase 2 step 3. The draft's two draft columns are guesses.
- `gen_golden.sh`, the golden format, `gen_tables.sh`, `rom-check`: Phase 2 steps 2, 5, 6 and 9.
- Any C code and any `tests/ladx/Makefile`.
- Detecting a link written in another form (URL-encoded, split across lines, case-changed host): the gate searches the exact bytes, as Phase 2's preamble states (`grep -F`); Phase 4 step 8's final search over the whole log is the same.
- A server-side guard: the gate is a client hook, bypassed by `git push --no-verify`, which no skill uses.

## Further Notes

- The gate's link search covers the whole history reachable from each pushed commit, not only new commits: in this repository's history that is about 22 MB of `git log -p` and well under a second, and it also catches a link pushed before the gate was armed.
- Pushing a branch new to the remote checks only blobs no remote ref already has (`--not --remotes`); stale remote-tracking refs make it check more, never less.
- Phase 4 step 8 deletes `tmp/ladx/`; the armed gate then refuses every push (no ROM). That step must run `push_gate.py --uninstall` first; this PRD only provides it.
- Python cannot see reads through the buffer protocol; `--blind-spots` lists the candidates so Phase 2 can review them. The final file write in `ROM.save` is the output itself, not a decision read.
- The study clone used to write this PRD (`tmp/ladxr-study/`, read-only) is throwaway and not referenced by any script.
