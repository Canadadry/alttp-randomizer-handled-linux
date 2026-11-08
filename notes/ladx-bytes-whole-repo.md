# Nintendo-bytes check over the whole repository

Phase 2 step 8 ran `nintendo_bytes.py` (LADX ROM, path scopes) on every
file Phase 2 added or changed (702 files, `git diff 7bdfd94 HEAD`):
0 literal runs.

The same check on every tracked file (1689) gives 535 `LITERAL` lines in
154 files, none written by plan 3:

- 534 in ALttP files (`tests/alttp/cases|combos/*/rom_writes.txt` and
  `rom_buffers.txt`, `tests/alttp/oracle/text_tables.txt`,
  `alttp/src/text_defaults.c`), all view `hex` at the general scope
  (8-byte threshold), runs of 8 to 880 bytes. They are ALttP data
  produced by VT, coinciding with LADX ROM bytes (padding and fill
  patterns the `trivial` and `low-info` clearances do not cover); they
  are not LADX content.
- 1 in `.claude/skills/rework-comments/SKILL.md`: 17 bytes of English
  prose (raw, general) coinciding with LADX dialogue text.

The push gate checks only the blobs a push uploads, so these files never
reach it. The check's policy targets LADX-derived artifacts; whether to
extend a clearance to these ALttP coincidences is an open question for
the owner, not a Phase 2 defect.

## Plan 3's whole diff, at the close of the ROM window (Phase 4 step 8)

Command, at ea99f39, every existing file plan 3 added, changed or
renamed:

    python3 -I tests/ladx/oracle/nintendo_bytes.py $(git diff --name-only --diff-filter=AMR fa0039e^ HEAD)

Summary: `nintendo-bytes: 1811 files, rom d90ac17e…, ladxr 4325207e,
526 literal runs, cleared 14267 trivial, 1042 low-info, 10855
ladxr-public, 96126 computed, 552 invariant`. All 526 runs are the
pre-existing ones above: 525 in 150 ALttP files that plan 3 only renamed
(`rando/` to `alttp/`, unchanged content) and the one prose run of
`.claude/skills/rework-comments/SKILL.md`, on a line plan 3 did not
change. Without the pure renames (1022 files) the only run is that
SKILL.md one. No LADX-derived file holds a literal run, and since the
owner's decision of commit 782e21a nothing committed holds ROM content
in another encoding either.
