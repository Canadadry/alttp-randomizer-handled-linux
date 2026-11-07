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
