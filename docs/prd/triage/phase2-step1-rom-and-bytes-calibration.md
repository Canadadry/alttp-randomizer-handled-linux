---
title: "Plan 3 step 2.1: fetch and check the ROM, calibrate the Nintendo-bytes check, arm the push gate"
description: "The Nintendo-bytes check flags almost all English text and many binaries at its Phase 1 settings, so it must be calibrated on the real ROM, with a recorded policy, before the push gate is armed and anything depends on it."
status: needs-triage
---

## Problem Statement

Phase 2 of plan 3 (`docs/plan-3.md`) is the first phase that has the real *Link's Awakening DX* ROM. Everything after this step relies on two Phase 1 tools: the Nintendo-bytes check (`tests/ladx/oracle/nintendo_bytes.py`) and the push gate (`tests/ladx/oracle/push_gate.py`), which runs that check on every blob a push uploads.

Phase 1 wrote both tools and proved them only against a synthetic ROM. The check flags any run of 4 bytes or more that also appears in the ROM, in a file's raw bytes or in its decoded hex-digit runs, with no exemption. Against the real ROM (1 MiB, which contains English dialogue, place names and long zero runs), that rule is a coincidence detector. While this PRD was being written, it was run on the real ROM (`tmp/ladx/base.gbc`, SHA-1 `d90ac17e…`):

- `CLAUDE.md`: 1,794 literal runs; `docs/plan-3.md`: 2,634; `notes/ladx-inventory.md`: 1,862. All of them are `raw` hits in ordinary English such as "the library" and "everything". The inventory's quoted `rom.patch` hex strings are 3 bytes long, below the minimum, so they produce no `hex` hits.
- Every tracked file of `tests/ladx/` plus `data/base.bps`: 194,579 runs. `logic_vectors.txt` alone has 92,312. The `hex` hits in `pyrandom_vectors.txt` decode to zero runs and to counting sequences 00 01 02 ….
- The tracked PNGs, BPS and TTF files: 2,105 runs at 4 bytes and 71 at 8. At 16 bytes, 180 runs remain, and every one of them is either an arithmetic progression or a run of at most 3 distinct byte values.
- English prose (`CLAUDE.md`, `docs/plan-3.md`, `notes/ladx-inventory.md`, `README.md`, `docs/onion.md`): 1,636 runs at 8 bytes, 55 at 12, 0 at 16.
- The ROM-free fixtures at 16 bytes: only whitespace runs, `00`/`FF` runs, counting sequences, and LADXR's own location names (for example "Seashell Mansion" in `logic_graphs.txt`).
- The baseline's raw output diff, measured with three seeds (seed `01` is refused by LADXR as "does not produce a valid result"; `02`, `03` and `04` were used): 122,088 changed bytes. 35,478 of them differ between seeds and 86,610 are equal across seeds. In the seed-dependent bytes, 31,230 sit inside runs the check flags (841 runs); in the settings-only bytes, 75,169 (2,311 runs). Most of the output diff is the ROM's own bytes moved or re-serialized, so the golden and table formats must encode those as source references.

At these settings, the armed gate would refuse almost every push of the run, starting with this step's own, and the later "0 literal runs" verifications of steps 5, 6 and 8 could never pass. Plan step 2.1 therefore requires a calibration on real data before anything depends on the check. The calibration must produce a recorded decision on what counts as a violation. That decision must also settle any hit the check cannot clear without the C side needing the ROM.

The ROM itself is already fetched: `fetch_rom.sh` ran and printed `d90ac17e9bf17b6c61624ad9f05447bdb5efc01a`, and the file is in `tmp/ladx/base.gbc`. The link the owner pasted (a Dropbox URL) is blocked by the environment's network policy. The session therefore uses the owner's own upload of the same ROM through a `file://` URL, which the orchestrator keeps in `LADX_ROM_URL`.

## Solution

1. **Confirm the ROM.** Re-run `fetch_rom.sh` (it is idempotent) with `LADX_ROM_URL` exported, check the printed SHA-1, and confirm that `git status` shows nothing new outside `tmp/`.
2. **Measure.** Run the Phase 1 check, unchanged, on every calibration sample the plan names, and record hit counts by category and length.
3. **Decide the policy.** Turn the measurements into a written violation policy, with the decisions below, recorded in the step note.
4. **Implement the policy** in `nintendo_bytes.py`, as the single source the push gate also uses, with self-proofs and unit tests on planted synthetic inputs.
5. **Re-run the samples under the new policy.** Record the counts: every remaining hit is either explained in the step note or settled by `guideline`.
6. **Arm the push gate** (`push_gate.py --install`), and prove it with one real refused planted push. From then on, every push of the run is made with `LADX_ROM_URL` exported.

The link is obtained only with the command below. It is never written into a file, a commit, a note, a PRD or a PR:

```
export LADX_ROM_URL="$(cat /tmp/claude-0/-home-user-alttp-randomizer-handled-linux/88735a1f-1a5b-530a-b7b2-4fdbf23fec51/scratchpad/ladx_rom_url)"
```

## User Stories

1. As the owner, I want the run to confirm the ROM's SHA-1 `d90ac17e…` before any calibration, so that every measurement is made against the real English 1.0 ROM.
2. As the owner, I want the ROM to stay in `tmp/ladx/` only, with `git status` showing nothing new outside `tmp/`, so that no Nintendo ROM reaches the repository.
3. As the owner, I want the link never written anywhere (file, note, commit, PRD, PR), and read only from the environment, so that my temporary link does not leak into git history.
4. As the owner, I want the step note to say that the Dropbox host was blocked and that my own upload was used through a `file://` URL, without naming the URL or its path, so that the deviation from the plan is on record.
5. As the run's agent, I want a single documented command that exports `LADX_ROM_URL` from the orchestrator's scratchpad file, so that every session and every push after arming uses the same source and never a copy.
6. As the run's agent, I want a warning in the PRD and in `CLAUDE.md` that once the gate is armed, every push fails closed without `LADX_ROM_URL` exported and the ROM present, so that a refused push is read as a setup problem, not a bug.
7. As the owner, I want the check measured on Phase 1's committed text and fixtures before it is tuned, so that the policy rests on the real coincidence rate.
8. As the owner, I want the check measured on a sample of English standing in for future notes and READMEs, so that the policy does not block ordinary prose.
9. As the owner, I want the check measured on the baseline's raw output diff, split into seed-dependent writes and settings-only patch bytes, so that steps 3, 5 and 6 know how much must be encoded as source references.
10. As the owner, I want the hit counts, their length histograms and examples of each category in the step note, so that I can review the calibration at the PR.
11. As the owner, I want examples in the step note to name a category, a file and a length, and quote text only when the note itself passes the check, so that the note cannot leak ROM bytes.
12. As the owner, I want ROM-derived artifacts (`data/ladx/`, the LADX goldens, ROM-derived extraction fixtures) held to §2's strict rule of 4 bytes, so that the files that come from the ROM are checked at full sensitivity.
13. As the owner, I want ordinary repository files (code, notes, docs, ROM-free fixtures, PNGs, fonts) checked by a threshold that coincidence does not reach but a pasted ROM dump or dialogue line does, so that the gate blocks real leaks and lets the run push.
14. As the owner, I want runs that carry no information (one repeated byte, a counting sequence) cleared and counted, never silently dropped, so that `00`/`FF` padding and counters stop failing the check while the totals stay visible.
15. As the owner, I want runs whose exact bytes LADXR already publishes in its pinned MIT sources cleared and counted, so that quoting LADXR (location names, `rom.patch` strings) is not treated as a Nintendo leak.
16. As the owner, I want LADXR's binary files excluded from that "already public" corpus, so that data LADXR redistributes in binary form cannot be used to clear ROM bytes.
17. As the owner, I want the corpus taken only from a LADXR checkout at the pin `4325207e`, so that a later or modified LADXR cannot widen what counts as public.
18. As the owner, I want a decision recorded now for strict-scope hits on bytes LADXR computes without reading the ROM (hint text, item IDs, assembled patches), so that steps 5 and 6 do not run into a "0 literal runs" requirement they cannot meet without the C side needing the ROM.
19. As the owner, I want byte lists written as source code (`0x3E, 0x00, …`, `$3E,$00`) checked as their own view, so that a C array of ROM bytes in Phase 3 cannot pass unseen.
20. As the owner, I want every threshold justified by a reason (one LADX text-box line, the chance of a random match) and not by "whatever makes it green", so that the calibration does not weaken the check.
21. As the owner, I want any hit still uncleared under the new policy, on a sample that cannot contain Nintendo bytes, settled by `guideline` with its evidence in the step note, never by raising a threshold silently.
22. As the run's agent, I want the summary line to keep the `<k> literal runs` substring, plus the counts of cleared runs, so that later steps' "prints `0 literal runs`" checks stay valid.
23. As the run's agent, I want a `--scope` override for files outside the repository (calibration dumps in `tmp/`), so that samples can be checked under either scope's rule.
24. As the run's agent, I want `--min` kept as an explicit experimental override that the summary shows and the gate never uses, so that threshold tables for the step note can be produced with the same tool.
25. As the run's agent, I want the push gate to apply exactly the same policy and scope rules as the check, by importing it, so that the two never disagree.
26. As the run's agent, I want the gate to fail closed when the pinned LADXR checkout is missing or at another commit, with a `setup:` line, so that the "already public" clearance never runs on an unknown corpus.
27. As the run's agent, I want the gate's refusal lines to name the scope and the category of each run, never its bytes, so that a refusal can be acted on without printing Nintendo data.
28. As the owner, I want the tool's self-proof, which runs on every invocation, to cover every scope, view and clearance on planted files against a planted ROM and a planted corpus, so that a broken policy cannot report a false `0 literal runs`.
29. As the owner, I want the unit tests to stay in `make test` with no ROM, no LADXR and no network, so that the fast loop stays fast and runnable anywhere.
30. As the owner, I want the gate's planted-push self-test extended to the new policy, so that arming only happens once the policy is proved inside the gate.
31. As the owner, I want the arming verified by one real refused push, through the armed hook, to a throwaway local remote, so that I know the hook really fires in this repository.
32. As the owner, I want that proof to leave no branch, worktree or commit behind and never touch `origin`, so that the planted ROM bytes stay local and throwaway.
33. As the run's agent, I want this step's own commits pushed through the armed gate, so that the new policy is shown to let legitimate work through.
34. As a future session, I want `CLAUDE.md` to describe the calibrated policy, the scopes, the clearances and the gate's new failure modes, so that I can use the tools without reading this PRD.
35. As a future session, I want the decision on the "computed" clearance for later strict-scope files written into the step note, so that steps 5 and 6 implement it instead of re-deciding it.
36. As the owner, I want the run time and memory of one gate run on this step's push recorded in the step note, so that a slow gate is noticed before it slows every later push.

## Implementation Decisions

### The ROM and the link

- The ROM is already in `tmp/ladx/base.gbc`. The step re-runs `fetch_rom.sh` with `LADX_ROM_URL` exported. The script finds the checked file and prints the SHA-1. Verified by the printed SHA-1 and by `git status --porcelain` listing nothing outside `tmp/` other than this step's own edits.
- Deviation from plan step 2.1, recorded in the step note: the owner's Dropbox link is blocked by the environment's network policy, and no environment change is requested. The owner uploaded the same ROM, and the session fetches it through a `file://` URL. The step note says exactly that, without the URL, its path or any part of either.
- `LADX_ROM_URL` comes only from `export LADX_ROM_URL="$(cat /tmp/claude-0/-home-user-alttp-randomizer-handled-linux/88735a1f-1a5b-530a-b7b2-4fdbf23fec51/scratchpad/ladx_rom_url)"`, in the same shell command as the tool that needs it. It is never echoed, written to a file or put on a command line that gets logged in a committed file.

### Calibration samples (measured before the policy changes)

The Phase 1 check runs, unchanged and at its default settings, on the following samples. A row of `--min` values (4, 8, 12, 16) gives the threshold tables. The step note gets counts, length histograms, and the share of each category: one repeated byte, arithmetic progression, at most 3 distinct values, all printable ASCII, other.

1. **Phase 1's committed files**: every file tracked at this step's starting commit that changed in plan 3's Phase 1, plus every tracked binary file (PNG, BPS, TTF), which the gate will meet in Phase 4.
2. **The English stand-in** for notes and READMEs: the repository's own prose (`README.md`, `CLAUDE.md`, `docs/*.md`, `notes/*.md`). No network text is fetched.
3. **The baseline's raw output diff.**
   - LADXR at the pin, with `ladxr.patch` applied, is run with the real ROM on the default settings for three seeds that LADXR accepts. Each run's working directory and output stay under `tmp/`, with `--spoilerformat none`, so no `LADXR_<seed>.*` file lands in the working tree.
   - The byte offsets the outputs change are split into seed-dependent ones (the value differs between seeds) and settings-only ones (the same value in every output, different from the source).
   - Each class is written to `tmp/` both as raw changed bytes (the worst case, every byte a literal) and as hex. The check runs on both.
   - The split is a proxy, and the note says so: a seed-dependent write that happens to be equal across the three seeds lands in the settings-only class. Exact tagging is step 3's and step 5's.
4. The measurement scripts are throwaway, under `tmp/` (a `CLAUDE.md` rule). The step note gives the exact commands, seeds and LADXR invocation, so the measurement can be repeated.

### The violation policy (decided here; recorded in the step note as this step's decision)

The policy has two **scopes**, chosen by the file's repository-relative path:

- **Strict scope (ROM-derived artifacts)**: `data/ladx/`, `tests/ladx/cases/`, `tests/ladx/combos/`, and the ROM-derived extraction fixtures. The fixtures are held as a list of path prefixes inside the check. Step 7 adds its fixture paths to that list when it creates them; until then, the list holds the three directories.
- **General scope**: every other path.

There are three **views**: `raw`; `hex` (as today: maximal hex-digit runs, both nibble alignments); and a new `bytelist` view. `bytelist` decodes maximal sequences of byte tokens written as `0xNN` or `$NN`, separated by commas and/or whitespace (C and Python arrays, LADXR's ASM `db` lines). Reason: Phase 3 writes C and must not carry ROM bytes in arrays that the other two views cannot see.

Thresholds, for a contiguous match with the ROM. The windowing stays exact: a run of at least T bytes exists if and only if one of its T-byte windows is in the ROM's T-window set.

| Scope | View | Minimum contiguous match |
|---|---|---|
| strict | raw, hex, bytelist | 4 bytes (§2 unchanged) |
| general | raw, in a text file (valid UTF-8, no NUL) | 16 bytes |
| general | raw, in a binary file | 16 bytes |
| general | hex, bytelist | 8 bytes |

Reasons:

- **Strict scope keeps §2's 4 bytes.** These are the only files built from the ROM, and their formats are the project's own.
- **16 bytes for general raw.** A LADX text box line is 16 characters, so a copied line of dialogue is caught. The measured English coincidences stop below 16: 55 runs at 12 bytes, 0 at 16, in about 120 KB of the repository's prose. In binaries, at 16 bytes only low-information runs remained.
- **8 bytes for general hex and bytelist.** The chance of a random 8-byte match against a 1 MiB ROM is about 5e-14 per window. A pasted hex dump or array of ROM bytes is the realistic leak in notes and code, and is usually at least that long.

**Clearances.** A cleared run is counted in the summary by category and never silently dropped; `--verbose` lists them as `CLEARED <path> <view> @<off> <len> bytes <category>`.

- `trivial`, in both scopes: the run's bytes form an arithmetic progression mod 256 (one repeated value included, and counting up or down). This covers padding, `00`/`FF`/space runs and the counting sequences measured in the fixtures.
- `low-info`, in the general scope only: the run uses at most 3 distinct byte values. It is not used in the strict scope, because Game Boy tile data with few values can still be Nintendo graphics.
- `ladxr-public`, in both scopes: the run's exact bytes (the whole merged run, decoded in its view) occur in the **LADXR corpus**.
  - The corpus is the raw bytes of every git-tracked file of the LADXR checkout at `4325207e` that is valid UTF-8 with no NUL, plus the hex-decoded bytes of those files' hex-digit runs, at both alignments.
  - Binary files of the checkout (for example `gfx/`) are excluded.
  - The checkout comes from `LADXR_DIR`, default `tests/ladx/oracle/ladxr/`. It is verified at the pin; otherwise the check prints a `setup:` line and exits 2.
  - Reason: these bytes are already published under MIT by the pinned project the port follows, so committing them adds no Nintendo byte. Clearance at window level was rejected: almost every English 4-gram appears in LADXR's sources, so it would clear dialogue too.

**The `computed` clearance, decided now and implemented in step 5.** In the strict scope, a literal run in a golden or table may also be cleared as `computed`, but only when both of these hold:

1. it lies entirely inside writes tagged with one inventory group (from the write log's tags);
2. step 3's read trace shows that group makes no ROM read feeding a write. Reads by `ROM.patch`'s old-byte check are already excluded.

Reason: such bytes come from LADXR's own data, the placement and the RNG, never from the ROM, and the C side can emit them as literals without the ROM. This is how hint text, item IDs and assembled patches that coincide with ROM bytes are settled. Every other strict-scope hit means the format must encode those bytes as a source reference: the fix is at the oracle (§2), never a clearance. This step records the decision, and the check gains a documented, inert extension point for it. With no strict-scope files existing yet, it is not built here.

**Settling remaining hits.** After the policy is implemented, the check is re-run on samples 1 and 2 (and sample 3 for information).

- Any uncleared hit in samples 1 or 2 is on files that never saw the ROM, so it is a coincidence the policy did not foresee. It is examined (category, file, length) and taken to a `guideline` agent, with the plan path and the evidence.
- The decision goes into the step note. A threshold is never raised and a category never widened just to reach zero.
- Sample 3's counts are recorded as the size of what step 5's format must reference.

**Interface of the check** (`nintendo_bytes.py`):

- `--rom` is unchanged.
- `--ladxr <dir>` sets the corpus checkout.
- `--scope strict|general` overrides the path-based scope, for files outside the repository.
- `--min <n>` stays as an experimental override that replaces every threshold. The summary then says `min <n> (override)`, and neither the gate nor any plan check passes it.
- `--verbose` adds the `CLEARED` lines.
- Output: `LITERAL <path> <view> @<offset> <length> bytes <scope>` per violation, then `nintendo-bytes: <n> files, rom <sha1>, ladxr <short>, <k> literal runs, cleared <t> trivial, <l> low-info, <p> ladxr-public`. Exit 1 when k > 0, 2 on `setup:`. It never prints the bytes.
- The module exposes one function that the gate calls: given a path, a blob, the ROM index and the corpus, it returns the violations and the cleared counts. The ROM's window sets (4, 8, 16) and the corpus are built once per run.
- The module docstring's "calibration note" is replaced by the policy, in at most a few lines. The reasons go in the commit message and the step note (the `CLAUDE.md` comment rule).

**Push gate** (`push_gate.py`):

- It uses that function, with the scope taken from each pushed blob's path.
- Its refusal line names the path, blob, view, scope and length.
- It fails closed when the LADXR checkout is missing or not at the pin, as it already does for the link and the ROM.
- Its planted-push self-test (run by `--install`) gains these cases, with a planted corpus:
  - a 4-byte ROM run under a strict path, refused;
  - a 12-byte printable-ASCII coincidence under a general path, passing;
  - a 16-byte ROM run under a general path, refused;
  - a run present in the planted corpus, passing;
  - a `bytelist` of 8 ROM bytes in a general `.c` file, refused.
- The success line keeps `0 literal runs`.

### Arming and the planted proof

- After the policy commits, run `push_gate.py --install` (its self-test first). It must print `push-gate: armed …`.
- **The proof**, with the armed hook:
  1. Create a throwaway worktree under `tmp/`, on a throwaway branch.
  2. Commit one planted file there under a general path, holding 16 bytes or more of non-low-information ROM text taken from `tmp/ladx/base.gbc`.
  3. Push that branch to a throwaway bare repository under `tmp/`, given as a path (never `origin`), with `LADX_ROM_URL` exported.
  4. The push must be refused with a `push-gate: refused: … literal run …` line.
  5. Remove the worktree, the branch and the bare repository.
  6. Confirm that `git branch` and `git worktree list` are back to before and that `origin` never received the branch.
- From then on, **every push of the run is made with `LADX_ROM_URL` exported**, in the same shell command as `git push`. A refusal for a missing link, ROM or LADXR checkout is a setup problem. The gate is re-armed at every Phase 2–4 session start (plan, Phase 2 preamble) until Phase 4 step 8.

### Documentation

- `CLAUDE.md`'s entries for the Nintendo-bytes check and the push gate describe the scopes, views, thresholds, clearances, the `--ladxr`/`--scope`/`--verbose` options, the new `setup:` cases, and the rule that pushes need `LADX_ROM_URL` exported while the gate is armed.
- The plan's §2 sentence is not edited. The step note states that the strict scope keeps it, and that the general scope is the push gate's extension, with its reasons.

### The step note (`### Step 2.1`) records

- The printed SHA-1, and the network deviation without the link.
- The sample counts before and after the policy, with histograms and examples (category, file, length; quoted text only if the note passes the check).
- The policy table and the clearances, with their reasons.
- The `computed` decision for step 5.
- Every `guideline` decision.
- The gate's armed line and the planted push's refusal line.
- The time and peak memory of one gate run.

## Implementation Chunks

1. **The policy in the check.** Add the scopes, the `bytelist` view, the per-scope thresholds, the `trivial`/`low-info`/`ladxr-public` clearances, the new options, the summary line and the extended self-proof. Update `test_nintendo_bytes.py` (planted ROM, planted corpus, no LADXR) and `CLAUDE.md`'s check entry. `make test` stays green. Commit.
2. **The gate on the policy.** `push_gate.py` uses the check's function, takes the scope from the path, fails closed on the LADXR checkout and gains the new planted pushes. Update `test_push_gate.py` and `CLAUDE.md`'s gate entry. `make test` stays green. Commit.
3. **Calibration on real data.** Run samples 1–3 before the policy (Phase 1's tool from the starting commit, run from a throwaway worktree or `git show` copy under `tmp/`) and after it. Settle any remaining hit through `guideline`. No commit of measurement files: the numbers go in the step note.
4. **Arm and prove.** Run `--install`, then the planted refused push. Push this step's commits through the armed gate with `LADX_ROM_URL` exported, and record the gate's time and memory.

## Testing Decisions

- Good tests here check observable behaviour: given a planted ROM, a planted corpus and planted files, which runs are reported, under which scope and view, which are cleared and in which category, the summary line and the exit code. They do not check how windows are stored.
- `test_nintendo_bytes.py`, in `make test` through `test-ladx-oracle` (no ROM, no LADXR, no network, a synthetic ROM generated in the test):
  - every threshold exactly at T−1 (passes) and T (flagged), per scope and view;
  - `bytelist` decoding of `0x` and `$` forms with commas and whitespace;
  - an arithmetic progression and a constant run cleared in both scopes;
  - a 3-value run cleared in general and flagged in strict;
  - a run present in the planted corpus cleared, and one only partly present flagged;
  - a binary file in the planted corpus excluded;
  - a corpus checkout at the wrong commit or missing giving `setup:` and exit 2;
  - `--scope` overriding the path;
  - `--min` override shown in the summary;
  - `--verbose` `CLEARED` lines;
  - no ROM byte ever printed.
- The tool's built-in self-proof (run on every invocation) is extended with one planted file per scope × view × clearance category.
- `test_push_gate.py` and the gate's `--self-test`: the new planted pushes listed above, plus the existing ones still as expected.
- Real-ROM evidence, outside the net: the before/after counts of samples 1–3, the armed line, the planted push refused through the real hook, and this step's own push going through.
- Prior art: the existing `test_nintendo_bytes.py` (planted synthetic ROM), `test_push_gate.py` and its planted repositories, and `rom_guard.py`'s planted-repo self-proof.
- `make check` stays green. `make test` time is noted, and must not grow noticeably (the planted ROM stays small).

## Out of Scope

- `gen_golden.sh`, the full-generation determinism double run, and `ladxr.patch` changes (step 2).
- The inventory's final sides, groups, read trace on real data, and extraction decisions (step 3).
- The golden and table formats, and the implementation of the `computed` clearance (steps 5 and 6). This step only decides it.
- Step 7's fixture paths in the strict list (step 7 adds them).
- Requesting a network-policy change for the Dropbox host. The owner's upload is used instead.
- Editing the plan's §2 text, deleting the ROM, or disarming the gate (Phase 4 step 8).
- Any C code.

## Further Notes

- The numbers in the Problem Statement were measured while this PRD was written, with the Phase 1 tool and the real ROM. They are indicative: the step re-measures and records its own.
- The `low-info` clearance (≤ 3 distinct values) is general-scope only, on purpose. If it ever hides a real leak in a general file, the remedy is to move that file under the strict scope, not to weaken the strict scope.
- Expect the strict-scope hits measured on the raw output diff to be large (sample 3). That is the evidence that step 5's write log must reference every ROM-sourced byte. The check is the backstop for that format, not a substitute for it.
- The gate scans only blobs a push uploads, so pre-plan-3 binaries (for example `data/base.bps`) block nothing unless they change. They are still measured in sample 1 for the record.
- Questions the step cannot settle from this PRD or the plan go to a fresh `guideline` agent with `docs/plan-3.md`, and the decision is recorded in the step note and listed in the PR.
