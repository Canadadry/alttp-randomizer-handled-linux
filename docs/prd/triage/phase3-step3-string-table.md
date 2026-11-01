---
title: "String table (i18n)"
description: "Nothing makes the app's text go through a translation table, so English words would be written straight into the UI core, translations would be impossible, and no check would notice."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 3, copied from `docs/plan-1.md`)

> 3. **String table (i18n).** Every UI string goes through an id and the English table; a test fails if the UI core contains a string literal that is shown to the user. Verified by that test.

Context the step relies on, from the plan:

- §2 *UI*: English text, every string looked up through a translation table keyed by id; only English ships.
- §2 *Out of scope*: translations other than English. **The mechanism is in scope, the tables are not.**
- `CLAUDE.md` ground rule: the UI core is a pure C state machine with no SDL; every string shown to the user goes through the translation table (goal: the agent sees the UI through the text renderer, and translations stay possible).
- `CLAUDE.md` § Checks: `make test` is the fast loop and must never get slower than needed; never weaken a check.
- Later consumers: step 4 (Generate rows, value labels, greyed reasons, footer legend), step 5 (the "Generating…" screen, success, and every `RandoSaveStatus`, `RandoAssembleStatus` and `RandoBpsResult` value mapped to a string id by an enum-sized table), step 6 (History list, detail, delete confirmation), step 7 (`ui_cli`).

## Hypotheses about Phase 3 step 2 (not done when this PRD was written)

Phase 3 step 2 (PRD `docs/prd/triage/phase3-step2-screen-model-text-renderer.md`) is not done. Each assumption about its outcome is labelled; its consequence when it fails is stated.

- **H-3.2-1** (where the UI core lives): the UI core is the directory `app/core/`, pure C99 (no allocation, no platform library, no SDL, no file I/O), compiled with only the library's public include directory and its own on the include path. *If it holds*, the string table is a new module there and the literal check scans exactly that directory. *If it fails* (the core lives elsewhere), both go to wherever step 2 put the core; the check's scanned directory is one argument in the build, and nothing else changes.
- **H-3.2-2** (build wiring picks up new core sources): every `.c` file in the core directory is picked up by wildcard into the app archive, `make lint` (with `-pedantic-errors` and the core's restricted include path) and `ARM_UNIT_SRC`. *If it fails* (sources listed one by one), this step adds its source to each list by hand.
- **H-3.2-3** (public names): the UI core's public names use the prefix `ui_` / `Ui` / `UI_`. *If it fails*, the names below take the core's prefix; no decision changes.
- **H-3.2-4** (the screen holds resolved text): the screen model's text fields are borrowed `const char *` (NULL = empty, printable ASCII drawn as is, any other byte as `?`), and they reach the model already looked up: the model holds no string id and the renderer takes no table. *If it holds*, the table is applied by the state machines (steps 4–6), and this step does not touch the screen model or the renderer. *If it fails* (the model stores ids), the render function gains a table argument in this step, and step 2's snapshot tests pass the English table, their expected text unchanged.
- **H-3.2-5** (the renderer has no word in a literal): the screen model, renderer and text form use character constants for their glyphs (`[`, `]`, `<`, `>`, `^`, `v`, `~`, `?`, the frame and the style tags), and any string literal they hold has no letter. *If it holds*, the literal check passes on step 2's code unchanged. *If it fails*, the offending literal becomes a character constant or a table id in this step's chunk 2 commit; the check is never given an exemption for it.
- **H-3.2-6** (default grid): the default grid is 40 columns × 15 lines, a pair of public constants, and the list field of a row is `columns − 2` wide (38 at the default grid). *If it holds*, every English string is at most 38 characters. *If it fails*, the bound is computed from whatever the constants and field rule are, never written as a literal number in the test.
- **H-3.2-7** (step 2's SDL check, the model for this one): step 2's SDL-include check is a short script beside the e2e scripts in `tests/rando/`, run by the randomizer test `Makefile`'s `test` recipe, and proving itself on each run on planted files in a temporary directory. *If it holds*, the literal check is wired the same way, next to it. *If it fails* (wired elsewhere, for example in the root `Makefile`), the literal check follows the same wiring, so that `make test` runs it.
- **H-3.2-8** (unit tests): new unit tests live in `tests/rando/unit/`, are registered in `UNIT_TESTS`, are listed in the `LOGIC_TESTS` filter-out list when they have no fixture mode, and fail through `test_fail`, as at HEAD. *If step 2 moved app tests elsewhere*, this step's test follows them.
- **H-3.2-9** (the SD layer is not the UI core): `app/sd/` (record and seed save) holds file paths and JSON keys, never text shown to the user; failures leave it as status values, mapped to string ids by step 5. *If it holds*, the literal check does not scan `app/sd/`. *If step 2 put anything shown to the user there*, that text moves into the core's table in this step.

## Problem Statement

The owner wants an app whose every word on screen comes from one translation table, so that the app could be translated later even though only English ships, and the agent wants a guarantee it cannot break by accident. Today there is no table at all: the first state machine (step 4) would write `"Generate"` and `"Heart beep"` straight into C code, every later step would follow, and nothing in `make test` would notice. Once dozens of English words are scattered through the UI core, a translation means a hunt through the code, and the plan's rule "every string shown to the user goes through the translation table" would be a wish, not a fact.

## Solution

**A string table in the UI core.** Every text the app shows has an id. One header lists the ids; the English table gives each id its text; the state machines look a text up by id and put the result in the screen. The table mechanism supports a second language without any code change outside the new table: a table may leave an id empty, and the lookup then falls back to English. Only the English table ships.

**One kind of parameter.** A text may hold one placeholder, `{}`, where a value the app computes goes (a ROM name, a count). Each id declares whether it takes one; a fill function substitutes it. Word order is the table's, so a translation can move the value.

**A check that the core holds no English.** A script run by `make test` reads every UI-core source and fails, naming the file and line, if any string literal outside the table file contains a letter. Literals with no letter (`""`, `"%d"`, `"\n"`) and character constants pass, because they show no word. The script proves itself on every run on planted sources before it scans the real ones, as step 2's SDL check does.

**A test that the table is sound.** A unit test fails if an id has no English text, if an English text is too long for the default screen, holds a byte the renderer cannot draw, or has the wrong number of placeholders.

## User Stories

1. As the player, I want every word on screen to come from the app's English table, so that the wording is consistent across screens.
2. As the player, I want no text cut by the screen edge because a string was too long, so that I can read every label, message and legend in full.
3. As the player, I want messages that name a file (a saved ROM, a seed to delete) to read naturally, with the name where a sentence needs it, so that confirmations are clear.
4. As a future translator, I want all English text in one table file, so that translating the app means writing one new table.
5. As a future translator, I want to leave an entry untranslated and see English instead of a blank, so that a partial translation is still usable.
6. As a future translator, I want the placeholder to sit anywhere in my text, so that my language's word order works.
7. As a future translator, I want my table checked by the same rules as English (placeholder count, drawable bytes, length), so that a mistake fails a test instead of breaking a screen.
8. As the owner, I want `make test` to fail if anyone writes user-visible English into the UI core, so that the translation rule holds without a review.
9. As the owner, I want that check proven to catch a planted literal on every run, so that it cannot pass by accident (a wrong path, an empty scan, a broken parser).
10. As the owner, I want the check to fail when it finds no source to scan, so that a moved directory cannot turn it into a no-op.
11. As the owner, I want the check fast, so that `make test` stays the fast loop.
12. As the owner, I want the exemption for the table file to be an exact file name inside the check, so that exempting another file is a visible, deliberate change.
13. As the owner, I want only English shipped, so that the scope stays the plan's.
14. As the state-machine author (steps 4–6), I want to look a text up by an id with a name that says what it is, so that my code reads like the screen.
15. As the state-machine author, I want a lookup that never returns NULL for a valid id, so that I never need a guard before putting a text in the screen.
16. As the state-machine author, I want to fill a placeholder into a buffer I own, with a clear result when it does not fit, so that I need no allocation and no format-string handling.
17. As the state-machine author, I want to use number formats such as `"%d"` in the core without the check complaining, so that counts and digits stay simple.
18. As the state-machine author, I want character constants such as `'['` allowed, so that glyphs are not forced into the table.
19. As the state-machine author, I want the table passed to my state machine as a pointer, so that a test can hand it a test table and a later build a translated one.
20. As the state-machine author, I want a new text to be one id line and one English line, so that adding a row costs two edits.
21. As the step-5 author, I want a fixed, testable way to map an enum value to a string id, so that every save, assemble and BPS status gets a message.
22. As the agent, I want the check's failure line to give the file, the line and the literal, so that I fix it in one edit.
23. As the agent, I want literals in comments ignored, so that a comment quoting a message does not fail the build.
24. As the agent, I want a literal split into adjacent pieces, a wide literal, a hex escape spelling a letter, or a literal inside a `#define` still caught, so that the check cannot be dodged by syntax.
25. As the agent, I want `#include "x.h"` lines ignored, so that header names are not taken for text.
26. As the agent, I want the table test to name the faulty id and the rule it breaks, so that a table mistake is a one-line fix.
27. As a later agent, I want the English table to be the only place where an expected English word in a UI-core snapshot comes from, so that a wording change is one edit plus the snapshots it changes, stated in the commit.
28. As the net, I want `make lint` to build the table with `-pedantic-errors` and `make unit-arm` to run its test under qemu-arm, so that the table compiles and behaves the same on the device.

## Implementation Decisions

### Scope of "shown to the user"

- **The UI core is the whole scope** (H-3.2-1, H-3.2-9): the text renderer draws only what the core's screen holds, and the SDL renderer (Phase 4) draws the same grid. `app/sd/`, `rando/`, the tests and the future `app/main/` and `app/sdl/` are not scanned.
- **Data is not a string of the app.** A ROM file name, a seed code, a spoiler line or a date reaches the screen as data computed by the library or the SD layer; it is not translated and not in the table. The English table holds every fixed word the app writes around such data.
- **A shown literal is a literal with a letter.** After decoding escape sequences and removing printf conversion specifications, a literal that still holds an ASCII letter is taken as user-visible text. Rejected: an annotation macro marking non-shown literals (it lets any word through with one wrapper, and a reviewer must judge each one); an allowlist of literals (grows with every format string). A literal with no letter can still be shown (`" / "`), but it carries no word to translate.

### The string table module (UI core)

- **Ids**: one header lists every id once, with its arity (0 or 1 placeholders), as an X-macro list from which the id enum and the arity table are both generated, so the two cannot drift. The enum ends with a count constant. Names indicative: `UiStrId`, `UI_STR_<AREA>_<NAME>`, `UI_STR_COUNT`.
- **Table type**: a struct holding one `const char *` per id (`UI_STR_COUNT` entries), written with C99 designated initializers (`[UI_STR_TAB_GENERATE] = …`), so a forgotten id is a NULL entry the table test catches, and the order of lines does not matter.
- **English table**: one source file, the only UI-core file whose literals hold words; reached through an accessor returning a pointer to a `const` table (no mutable global state). No language enum and no runtime language choice: a translated build would add its own table file and accessor, out of scope here.
- **Seed ids** (the texts this step can fix from §2 without designing a later screen):
  - `UI_STR_TAB_GENERATE` → `Generate`, arity 0;
  - `UI_STR_TAB_HISTORY` → `History`, arity 0;
  - `UI_STR_HISTORY_DELETE_CONFIRM` → `Delete {}?`, arity 1 (§2: deleting a seed asks for confirmation; it is the step's real arity-1 id, used by the tests; step 6 may reword it deliberately).

  Steps 4–6 add their own ids as they build their screens; this step adds nothing it would have to guess.
- **Lookup**: `ui_str(table, id)` returns the table's text, or the English text when the table's entry is NULL; a NULL table means English. An id outside `[0, UI_STR_COUNT)` returns the empty string `""`. It never returns NULL.
- **Arity**: `ui_str_arity(id)` returns 0 or 1 (0 for an out-of-range id).
- **Placeholder**: the two characters `{}`, at most one per text. A `{` or `}` anywhere else in a table text is a table fault (no escape syntax: no English text needs a literal brace).
- **Fill**: `ui_str_fill(pattern, arg, buf, cap)` copies the pattern into the caller buffer with its `{}` replaced by `arg` (NULL = empty); a pattern without `{}` is copied as is and `arg` ignored. The buffer is always NUL-terminated when `cap ≥ 1`; the result is true when the whole text fit, false when it was cut (the cut text is still written, so a screen still shows something, and the renderer's own `~` cut applies on top). `cap = 0` writes nothing and returns false. It takes a pattern, not an id, so it is tested on any pattern and works with any table. No printf: a translated table text is never used as a format string.
- **Table check**: `ui_strings_check(table, &bad_id, &fault)` returns true when the table is sound, else false with the first faulty id and one fault code: missing (a NULL entry, refused only in the English table), empty text, a byte outside printable ASCII 0x20–0x7E (H-3.1-4 of step 2's PRD: what the renderer draws), a placeholder count different from the id's arity, a stray brace, and too long (more than the default list field width, counting `{}` as zero characters; H-3.2-6). A non-English table is checked by the same rules on its non-NULL entries. The function is in the core so a future translated table is checked by the same code; the English table's soundness is asserted by the unit test, not at run time.
- **Enum-to-id tables** (for step 5, recorded here so the pattern is one): a table mapping another enum's values to string ids is a `const UiStrId[]` sized by that enum's count constant; its test walks every value and fails on an entry that is still the zero id. To make "not set" detectable, **id 0 is a reserved `UI_STR_NONE`** with an empty English text and arity 0, exempt from the "empty" rule, never shown. `ui_str(table, UI_STR_NONE)` returns `""`.
- **Rule compliance**: pure C99, no allocation, no stdio, `const` data only; built by `make lint` with `-pedantic-errors` and cross-built by `make unit-arm` (H-3.2-2).

### The literal check (`make test`)

- **A Python 3 script** (stdlib only; `python3` is already a check dependency) beside step 2's SDL check (H-3.2-7), run by the randomizer test `Makefile`'s `test` recipe with the UI-core directory as its argument. Python rather than shell: it needs a small C lexer (comments, literals, escapes, character constants, line splices), which shell or awk would get wrong. It runs with `python3 -I`.
- **What it scans**: every `.c` and `.h` file in the given directory and its subdirectories, sorted. **It fails if it finds no source file**, or if the directory does not exist.
- **Lexing rules**:
  - line splices (backslash-newline) joined first;
  - `/* … */` and `// …` comments skipped, outside literals only;
  - character constants (`'x'`, `'\''`, `'"'`, with `L`/`u`/`U` prefixes) skipped;
  - string literals recognised with their prefixes (`L`, `u`, `U`, `u8`); adjacent literals (only whitespace and comments between them) concatenated before judging, so `"Gen" "erate"` is one literal;
  - preprocessor lines `#include`, `#pragma`, `#line`, `#error` skipped; every other line, `#define` bodies included, is scanned.
- **Judgement** of each (concatenated) literal: decode its escapes (`\n`, `\t`, octal, `\x` hex, the simple escapes), remove every printf conversion specification (`%%` and `%[flags][width][.precision][length]conversion`), then fail if any ASCII letter remains. The failure line is `<file>:<line>: user-visible string literal: <literal as written>`; the script reports every finding, then exits 1.
- **The one exemption**: the English table's source file, named by its exact path relative to the scanned directory, in a list inside the script (a translated table would be added to that list, a deliberate change). An exempt path that does not exist is itself a failure, so a rename cannot leave a stale exemption.
- **Self-proof on every run**, before the real scan: it writes planted sources into a fresh temporary directory (removed afterwards) and runs the same scan function on them:
  - must be caught, each on its own: a plain word literal; adjacent pieces spelling a word; a wide `L"…"` literal; a `"\x41"` hex escape; `"%d items"` (a word after a conversion); a literal inside a `#define`; a literal after a comment holding a quote character; the English table's content placed in a file with another name;
  - must pass: a comment holding a quoted word; character constants `'"'`, `'v'`, `'['`; `#include "ui_screen.h"` and `#include <string.h>`; `""`, `"\n"`, `"%d"`, `"%-20s"`, `"%%"`, `"%s / %s"`; the exempt file name holding word literals;
  - the empty directory must fail.

  Any planted case judged wrongly stops the run with `literal-check self-proof failed: <case>` and exit 1, before the real scan. This is the check's own test; it has no separate unit test.
- **Cost**: one Python start and a lexer over a few files: well under a second. A clean `make test` is timed before and after, numbers in the step note.
- **Documentation**: `CLAUDE.md` § Checks says `make test` also runs the UI-core literal check (one line, next to step 2's SDL-check line). If the permission system refuses the `CLAUDE.md` edit, the implementer reports it and `plan-pilot`'s step note carries the text.

## Implementation Chunks

1. **String table module.** The id list with `UI_STR_NONE` and the three seed ids, the English table, lookup with fallback, arity, fill, table check; the unit test `test_ui_strings`, registered in `UNIT_TESTS` and the `LOGIC_TESTS` filter-out list. Done when the test covers every rule under Testing Decisions, `make lint` builds the new source with the core's restricted include path, and `make check` is green.
2. **The literal check.** The script with its self-proof, wired into the randomizer test `test` recipe; any step-2 literal with a letter turned into a character constant or a table id (H-3.2-5); the `CLAUDE.md` § Checks line; `make unit-arm` run with the new test (counts in the step note); a clean `make test` timed before and after. Done when `make test` fails, naming the file and line, on a word literal planted by hand in a UI-core source (checked, then removed), passes without it, and `make check` is green.

## Testing Decisions

- **What a good test is here**: it goes through the public functions only (lookup, arity, fill, check) and checks their results; it never reads the table's array directly to decide what a function should return; expected values are written from the rules above, not copied from the code's output.
- **`test_ui_strings`** (one unit test binary, no fixture mode):
  - the English table passes `ui_strings_check`;
  - every id from 1 to `UI_STR_COUNT − 1` has a non-empty English text through `ui_str`; `ui_str(english, UI_STR_NONE)` and an out-of-range id (`UI_STR_COUNT`, and a negative value cast to the enum) return `""`, never NULL;
  - the seed texts read exactly `Generate`, `History`, `Delete {}?`, with arities 0, 0, 1;
  - fallback: a test table (in the test file) with one entry set and the others NULL returns its own text for the set id and the English text for the others; a NULL table returns English;
  - fill: placeholder at the start, middle and end; no placeholder with an argument (argument ignored); NULL argument (empty); exact fit returns true; one byte short returns false with a NUL-terminated cut text of `cap − 1` characters; `cap = 1` writes only the NUL; `cap = 0` returns false and writes nothing (buffer sentinel unchanged);
  - check, one test table per fault, each reporting the expected id and fault code: a NULL entry in the English role; an empty text; a byte `0x7F`, a byte `0xC3`; a missing `{}` on the arity-1 id; a `{}` on an arity-0 id; two `{}`; a lone `{`; a text one character over the default field width (and one of exactly that width passing; the width taken from step 2's public constants, H-3.2-6); a non-English test table with NULL entries passing.
- **The literal check** is tested by its self-proof on every `make test` run, plus chunk 2's hand check on a planted literal in the real core directory.
- **`make lint`** builds the table source with `-pedantic-errors`; **`make unit-arm`** runs `test_ui_strings` under qemu-arm with the rest.
- **Unchanged by this step**: step 2's snapshot tests and expected text (H-3.2-4), every Phase 1–2 test, the goldens.
- **Prior art**: `test_record` (exact text, strict refusals), `test_name` (rule-written expectations), step 2's SDL check (a check proving itself on planted files), `selftest.sh` (a check proving it catches a planted fault).

## Out of Scope

- Any table other than English, a language setting, a language enum or a runtime language switch.
- The ids and texts of the Generate, generation and History screens (steps 4–6 add them with their screens), and the status-to-id tables (step 5, following the pattern decided here).
- Changing the screen model or the renderer beyond what H-3.2-4 and H-3.2-5 require if they fail.
- UTF-8, non-ASCII text, plural forms, more than one placeholder per text, numbered placeholders.
- Scanning `app/sd/`, `rando/`, the tests, or the future `app/main/` and `app/sdl/`.
- Display names of the settings' values: whether step 4 shows the library's case-format names or table texts is step 4's decision (see Further Notes).
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): the outcome of H-3.2-1 to H-3.2-9; the `make test` times before and after; the `unit-arm` counts; the hand check of the planted literal.
- **For step 4**: the library's value names (`open`, `fast_ganon`, …) are case-format keys in `rando/`, outside the literal check's reach, so the check cannot catch the Generate tab showing them raw. Showing them would bypass the table; step 4 should give each setting and each value a table id (indexed through the settings table it builds) so the player reads `Fast Ganon`, not `fast_ganon`.
- **For step 5**: each status-to-id table follows the enum-sized pattern above, with `UI_STR_NONE` as the "not set" value its test refuses.
- **For Phase 4 step 1**: the SDL renderer draws the grid and holds no text; if it ever needs a word (a fatal "font missing" message before the core runs), that word is outside the UI core and outside this check, and should still come from the table.
- **For translations later**: a new table is one source file with designated initializers, one accessor, one line in the check's exemption list, and its own `ui_strings_check` assertion in the test.
