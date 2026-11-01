---
title: "Screen model and text renderer"
description: "The app has no UI yet: nothing describes a screen, nothing draws one the agent can read, and the plan has not decided where the app's own code lives in the repository."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 2, copied from `docs/plan-1.md`)

> 2. **Screen model and text renderer.** The screen description struct and a renderer to a fixed character grid. Verified by a snapshot test of a hand-built screen; by `make lint` building the UI core's sources with `-pedantic-errors`; by a check in `make test` that no UI-core source includes an SDL header; and by `make unit-arm` running the snapshot test under qemu-arm (its sources added to `ARM_UNIT_SRC` if outside `rando/`).

This PRD also settles §5 open question 2, which the plan hands to this step:

> 2. Whether the app's UI core and host tools live under `rando/host/` or a new top-level `app/` directory, and where the device-bound `record.c` and `seed_save.c` (today in `rando/host/`) live. Decided by Phase 3 step 2's PRD, recorded in its step note and struck through here with the answer. Wherever it lands, the UI core is built by `make lint` and `make unit-arm`, and includes no SDL header.

Context the step relies on, from the plan:

- §2 *UI architecture*: the UI is a pure C state machine with no SDL, a key in and a screen description out (tab, rows, cursor, values, message); an SDL renderer (Miyoo and PC) and a text renderer (a character grid) draw that description; tests play key sequences and compare the text screen against a snapshot; a host CLI prints the text screen (step 7).
- §2 *UI*: two tabs, Generate and History, in the style of Onion's menus (a clean font, lists, a cursor, no game graphics); every string through a translation table (step 3).
- Later consumers of this step's model: step 4 (Generate rows, left/right values, greyed rows with a reason, the Generate action), step 5 ("Generating…", success with the file name, error messages), step 6 (History list, scrollable detail view, delete confirmation), step 7 (`ui_cli`), Phase 4 step 1 (the SDL renderer).
- Step 2.3's note: `make unit-arm` builds every unit test plus `ARM_UNIT_SRC` with `-D_FILE_OFFSET_BITS=64` and runs them from `tests/rando` (relative fixture paths). Plan review after Phase 2: `ui_cli` (step 7) links only the objects `make test` already builds, never the `-O2` set.

## Hypotheses about Phase 3 step 1 (not done when this PRD was written)

Phase 3 step 1 (Onion facts in `docs/onion.md`, PRD `docs/prd/triage/phase3-step1-onion-facts.md`) is not done. This step keeps the screen model free of pixel sizes and of keys, so it rests on few of its facts; each assumption is labelled, and its consequence when it fails is stated.

- **H-3.1-1** (resolution an app gets): an app on the Mini Flip draws on a 640×480 surface by default (Onion's `DEFAULT_WIDTH`/`DEFAULT_HEIGHT` under `PLATFORM_MIYOOMINI`; 752×560 only behind the `full_resolution` opt-in). *If it holds*, the default grid is 40 columns × 15 lines (a 16×32 px cell at 640×480; the cell size itself is Phase 4's). *If it fails* (another size, or the Flip forces 752×560), only the pair of default-grid constants changes (for example 47×17 at 752×560), in its own commit with every default-grid snapshot rewritten on purpose; no type, function or layout rule changes, because the grid size is a parameter everywhere.
- **H-3.1-2** (Onion's menu look): Onion's list menus have a header line at the top, a vertical list with one highlighted row and a scroll indication, and a button-legend line at the bottom. *If it holds*, the layout below matches it line for line. *If it fails* (for example no legend line, or a header at the bottom), only the layout function's line assignment changes; the screen struct keeps its fields (an unused field is drawn nowhere).
- **H-3.1-3** (no key fact needed): this step needs no button mapping. The screen carries no key; the footer is a text the state machine composes (step 4 onwards), so Onion's A/B/L/R conventions enter at step 4. *If step 1 finds something that forces the model to know a key* (for example Onion drawing button glyphs from an icon font), the footer gains a list of (button id, text) pairs in step 4, not here.
- **H-3.1-4** (ASCII is enough to draw): the text the app shows in English is printable ASCII, and Onion apps draw text with SDL_ttf from a TTF font, so a grid of ASCII cells is drawable by Phase 4. *If it fails*, Phase 4 ships a font or a glyph map; the grid model is unchanged.
- **H-3.1-5** (SD path constants): step 1 checks §2's SD paths against the constants `RANDO_SD_ROM_DIR` and `RANDO_SD_HISTORY_DIR` (H-2.3-1 of its PRD) and may cite the header that holds them by its path under `rando/host/`. This step moves that header (below) without changing any value. *If `docs/onion.md` cites the old path*, this step updates that link to the new path in the same commit as the move (the only edit to `docs/onion.md`); *if step 1 changed a path value on the owner's decision*, the moved constants keep the decided value.

## Problem Statement

The owner wants an app driven by the d-pad, in the style of Onion's menus, and the agent building it can neither play nor look at a screen. Today nothing describes what a screen of the app shows, nothing draws it, and so nothing can be tested: the Generate tab, the generation flow and the History tab (steps 4 to 6) would each invent their own way of putting text on screen, the SDL renderer would lay out lines its own way, and the agent's "eyes" (the text renderer and `ui_cli`) would show something other than what the device shows.

The repository also has no place for the app's own code. `rando/host/` holds host-side code on top of the library, but two of its modules (`record.c`, the history record, and `seed_save.c`, which writes the ROM and record to the SD card) exist only for the app, and the UI core must be pure C, linted strictly and cross-built, while `rando/host/` is neither linted nor meant for the device. Every later step of Phases 3 and 4 needs this settled first.

## Solution

**Where the app lives (open question 2, decided).** A new top-level `app/` directory holds the Onion app's own code; `rando/` stays the randomizer and its host tools.

- `app/core/` is the **UI core**: pure C99 like the library (no allocation, no platform library, no SDL, no file I/O), depending only on the library's public headers. This step puts the screen model and the grid renderer there; steps 3 to 6 add the string table and the state machines.
- `app/sd/` is the app's **SD-card layer**: the device-bound code that reads and writes the card. `record.c` and `seed_save.c` and their headers move there unchanged; step 6's directory scan and deletes will join them.
- `rando/host/` keeps what the oracle tools and CLIs share with the app (case reader, spoiler JSON, playthrough, output writers, `rando_patch`, `rando_gen`).
- Reserved for later steps, not created here: `app/sdl/` for Phase 4's SDL renderer and input, `app/main/` for the binaries (`ui_cli` in step 7, the app's `main` in Phase 4).

**What a screen is.** A screen description is a plain struct the state machines fill: a tab bar, an optional title, a list of rows (settings with a value, actions, plain text lines; any of them greyed out), a cursor, a scroll position, a message (info or error) and a footer line. It knows nothing about pixels or keys.

**How it is drawn.** One layout function turns a screen into a character grid of a given size (columns × lines): every character of every line, plus one style per line (tab bar, title, row, selected row, disabled row, message, footer). The text renderer prints that grid with a frame and a one-character style tag per line; Phase 4's SDL renderer will draw the same grid's cells and styles in pixels. So the agent's text screen is, line for line, what the Miyoo shows.

**How it is checked.** Snapshot tests render hand-built screens and compare the text form with committed snapshot files; `make lint` builds `app/` with `-pedantic-errors`; `make test` fails if a UI-core source includes an SDL header; `make unit-arm` runs the new tests under qemu-arm.

## User Stories

1. As the player, I want every screen of the app laid out the same way (tabs at the top, the list in the middle, a message and the button legend at the bottom), so that the app feels like one Onion menu.
2. As the player, I want the active tab marked in the tab bar, so that I always know whether I am on Generate or History.
3. As the player, I want the row under the cursor highlighted, so that I see which setting the d-pad changes.
4. As the player, I want a setting's value shown at the right of its row with arrows when the cursor is on it, so that I know left and right change it.
5. As the player, I want a greyed-out row to look greyed even under the cursor, so that I know it cannot be changed.
6. As the player, I want a scroll mark when rows are hidden above or below, so that I know the list goes on.
7. As the player, I want long names cut with a visible mark instead of overflowing the screen, so that nothing is drawn off the edge.
8. As the player, I want error messages shown on their own line in their own style, so that a failure stands out from a hint.
9. As the player, I want a button legend at the bottom of the screen, so that I know what A, B and the shoulder buttons do here.
10. As the player on a Mini Flip, I want the layout sized for the screen the app actually gets, so that rows are neither cut nor tiny.
11. As the agent, I want a text form of the exact grid the device draws, so that I can "look" at a screen while working.
12. As the agent, I want that text form to show the grid's edges, so that I see the true width and trailing spaces.
13. As the agent, I want each line's style shown as a tag in the text form, so that I see the cursor, greyed rows and errors without colours.
14. As the agent, I want a byte the font cannot draw shown as `?`, so that a bad string is visible instead of corrupting the grid.
15. As the state-machine author (steps 4–6), I want one screen struct for every screen (settings list, progress, result, history list, detail, confirmation), so that I fill fields and never draw.
16. As the state-machine author, I want rows to point at strings I own instead of copying them, so that the struct stays small and allocation-free.
17. As the state-machine author, I want a NULL text treated as empty, so that optional fields need no placeholder string.
18. As the state-machine author, I want to ask how many list lines a grid shows, so that page-down and the scroll position match what is drawn.
19. As the state-machine author, I want a helper that moves the scroll position just enough to keep the cursor visible, so that every list scrolls the same way.
20. As the state-machine author, I want to show a slice of a longer list (a spoiler, a long history) and still get the scroll marks, so that I do not need a row capacity as big as the longest spoiler.
21. As the state-machine author, I want an invalid screen (cursor or scroll out of range, too many rows or tabs) refused by the renderer, so that my bug fails a test instead of drawing garbage.
22. As the state-machine author, I want a row add helper that refuses past capacity, so that a long list cannot overflow the struct.
23. As the string-table author (step 3), I want the renderer to contain no user-visible string literal, only single-character glyphs, so that the literal check of step 3 has nothing to flag in the renderer.
24. As the SDL-renderer author (Phase 4), I want the grid to carry every character and each line's style, so that I only map cells to pixels and styles to colours, with no layout of my own.
25. As the SDL-renderer author, I want the core to know nothing about pixels, so that a resolution change is a cell-size change in my code and a default-grid change in one constant pair.
26. As the owner, I want the screen layout independent of the exact resolution, so that a different Onion resolution changes one constant pair, not the app.
27. As the owner, I want the app's code in its own directory, apart from the randomizer, so that the repository says what ships on the device.
28. As the owner, I want the UI core built strictly as C99 and cross-built for the Miyoo, so that what the tests check is what runs on the device.
29. As the owner, I want `make test` to fail if anyone includes SDL in the UI core, so that the agent's text view can never drift from the core.
30. As the owner, I want that check itself proven to catch an SDL include, so that it cannot pass by accident.
31. As the owner, I want the move of `record.c` and `seed_save.c` to change no behaviour, so that Phase 2's tests stay the proof.
32. As the net, I want `make test` and `make check` green and no slower than the new tests' own time, so that the fast loop stays fast.
33. As a later agent, I want snapshot files I can read as plain text next to the tests, so that a failing snapshot shows me both screens.
34. As a later agent, I want a failing snapshot test to print the actual screen and the first differing line, so that I can tell a bug from a deliberate change.
35. As a later agent, I want the snapshot tests to have no automatic update mode, so that an expected screen only changes by a deliberate edit (ground rule: no edited expected value unless that is the change).
36. As a later agent, I want `CLAUDE.md`'s layout section to name `app/`, so that I find the app's code.

## Implementation Decisions

### Open question 2: the directories

- **New top-level `app/`**, for the Onion app's own code. Rejected: putting the UI core under `rando/host/` (that directory is host-side code, is not linted, and its stdio modules would sit next to a core that must not use them); putting it under `rando/src/` (it is not the randomizer, and `rando/src/`'s wildcard would also put it in the generator's `-O2` set and the e2e cross build).
- **`app/core/`, the UI core.** Pure C99 under the library's rule: no `malloc`, no platform library, no SDL, no file I/O (`stdio.h`'s string formatting is allowed for later steps; no stream is opened). It may include the C standard headers, the library's public headers and its own headers, nothing else: its objects are compiled with only the library's include directory and its own on the include path, so an include of an `app/sd/` or `rando/host/` header fails to build. This step adds the screen model and the grid renderer; steps 3–6 add theirs here.
- **`app/sd/`, the SD-card layer.** `record.c`, `seed_save.c`, `rando_record.h` and `rando_seed_save.h` move there with no content change (a pure move; the history of the file is kept by a rename). It may include the library, `rando/host/` (spoiler JSON, playthrough, output writer, case reader's integer parser until step 4 moves it) and the C library; it never includes `app/core/`, and `app/core/` never includes it. Step 6's directory scan and file removal join it. Its public names keep the `rando_` prefix (no rename in a move).
- **Dependency direction:** `app/core` → library; `app/sd` → library, `rando/host`; nothing in `rando/` includes anything in `app/`. Later: `app/main/` binaries may include all three; `app/sdl/` may include `app/core` and SDL.
- **`rando/host/` keeps** the case reader, spoiler JSON, playthrough, output writers, `patch_cli.c` and both `main`s. The `patch_cli.c` vs `rando_patch.h` naming warning (`notes/phase2-warn-review.md`) does not ride this move: those files do not move.
- **Build wiring** (all in the existing randomizer test `Makefile`; no new Makefile):
  - one new archive of the `app/` objects (core and SD layer), built at the same flags as the host archive, linked into every unit test binary ahead of the host archive; it counts among "the objects `make test` already builds" for step 7's `ui_cli` rule (never `-O2`);
  - the test include path gains `app/core` and `app/sd`;
  - `lint` also builds every `app/` source with `-pedantic-errors` (`record.c` and `seed_save.c` already pass it at HEAD, checked while writing this PRD), core sources with the core's restricted include path; the `lint` summary line counts both sets;
  - `ARM_UNIT_SRC` gains every `app/` source, and the cross build's include path gains both directories;
  - the generator's `-O2` set, `rando_gen`, `rando_patch`, `e2e-arm` and `measure` are unchanged (none uses `app/`);
  - the new tests are added to `UNIT_TESTS` and to the `LOGIC_TESTS` filter-out list (they have no fixture mode).
- **Tests stay in `tests/rando/unit/`** with the same support code; `test_record` and `test_seed_save` keep their names and content. A `tests/app/` split is out of scope (it would duplicate the library build and the `unit-arm` loop).
- **Documentation:** `CLAUDE.md` § Layout gains one bullet for `app/` (core and SD layer, the dependency rule) and its `rando/host/` bullet loses nothing (record and save were not listed); § Checks says `make test` includes the UI-core SDL check. If the permission system refuses the `CLAUDE.md` edit, the implementer reports it and `plan-pilot`'s step note carries the text. The plan's §5 strike-through and step note are `plan-pilot`'s, not this PRD's. `docs/onion.md` only per H-3.1-5.

### The screen model (UI core module)

- **Types** (names indicative; prefix `ui_` / `Ui` for the UI core's public names):
  - a row: kind (`SETTING`: label and value; `ACTION`: label only, such as Generate or Delete; `TEXT`: one plain line, such as a spoiler line), label text, value text (settings only), disabled flag;
  - a message kind: none, info, error;
  - the screen: tab labels (at most 4) with a count and the active index; title text; rows (at most 64) with a count; cursor (a row index, or −1 for none); scroll (index of the first row drawn); two flags "more above" and "more below" for a state machine that hands over a slice of a longer list; message kind and text; footer text.
- **Strings are borrowed.** Every text field is a `const char *` the caller keeps alive until the render returns; NULL means empty. Text is bytes; printable ASCII (0x20–0x7E) is drawn as is, any other byte as `?` (UTF-8 is out of scope while only English ships).
- **Functions:**
  - initialise a screen (no tabs, no title, no rows, cursor −1, scroll 0, no message, no footer);
  - add a row (returns false, adding nothing, when the screen is full);
  - scroll follow: given scroll, cursor, row count and page size, returns the scroll that keeps the cursor on the page with the least movement, clamped to `[0, max(0, rows − page)]`; with no cursor it only clamps;
  - validate: true when counts are within capacity, the active tab is in range (or there are no tabs), the cursor is −1 or a row index, and the scroll is in `[0, max(0, rows − 1)]`.
- **No pixel, no key, no string id** in the model. Strings reach it already looked up (step 3's table is applied by the state machines, not by the renderer).

### The grid and its layout (UI core module)

- **Grid:** a size (columns, lines) and, per line, its characters and one style. Bounds: 20 to 96 columns, 5 to 40 lines; initialising with a size outside them returns false. **Default grid: 40 × 15** (H-3.1-1), one pair of public constants, used by step 7's CLI and by Phase 4; tests also render at other sizes.
- **Line styles:** blank, tab bar, title, row, selected row, disabled row, selected disabled row, text line, info message, error message, footer.
- **Render** (screen + grid → filled grid; false, leaving the grid all blank, on an invalid screen) — the layout, fixed by line position:
  - line 0, *tab bar*: each tab as `[Label]` when active and ` Label ` otherwise, separated by one space, from column 0; blank style when there are no tabs;
  - line 1, *title*: the title from column 1;
  - lines 2 to `lines − 3`, *the list* (`lines − 4` lines, the page size a public function returns for a grid): rows `scroll` onwards, one per line, then blank lines;
  - line `lines − 2`, *message*: the message from column 1, info or error style (blank when none);
  - line `lines − 1`, *footer*: the footer from column 1.
- **A list line** uses columns 1 to `cols − 2` as its field; column 0 is a margin and column `cols − 1` the scroll column:
  - `SETTING`: the label from column 1, the value right-aligned to column `cols − 2`, written as `< value >` on the selected row when it is not disabled and as `  value  ` otherwise (same width, so the value never jumps), with at least one space between label and value;
  - `ACTION` and `TEXT`: the label from column 1;
  - style: row, selected row (cursor), disabled, selected disabled; `TEXT` lines get the text style (a cursor on a text row is allowed and draws the selected style).
- **Scroll column:** `^` on the first list line when rows are hidden above (`scroll > 0` or "more above"), `v` on the last list line when rows are hidden below (rows beyond the page or "more below"), blank otherwise.
- **Truncation:** any text longer than its field is cut and its last kept character replaced by `~`. On a setting row the value is kept whole first, and the label cut to what remains; a value wider than the field minus two columns is cut itself and its label dropped.
- **Glyphs are characters, not strings:** `[`, `]`, `<`, `>`, `^`, `v`, `~`, `?` and the text form's frame and tags are character constants, so step 3's check (no user-visible string literal in the UI core) passes on the renderer.

### The text form (UI core module)

- **Format:** `lines + 2` text lines, each ended by `\n`:
  - a top and bottom border: a space, `+`, `cols` times `-`, `+`;
  - each grid line: its style tag, `|`, exactly `cols` characters, `|`.
- **Style tags:** blank ` `, tab bar `=`, title `#`, row ` `, selected row `>`, disabled row `x`, selected disabled row `X`, text line ` `, info message `i`, error message `E`, footer `_`.
- Written into a caller buffer of at least a public size (the text form of the largest grid, plus the terminating NUL); returns the length, or 0 when the buffer is too small (nothing guaranteed written). No stdio.

### The `make test` SDL check

- A short POSIX shell script beside the e2e scripts, run by the randomizer test target's `test` recipe (so by `make test`), that lists every `#include` line of every `.c` and `.h` file under `app/core/` and `app/sd/` and fails, naming the file and line, on any header whose name or first path component starts with `SDL` (case-insensitive: `SDL.h`, `SDL/SDL.h`, `SDL2/SDL.h`, `SDL_ttf.h`).
- It proves itself on each run: before scanning the real directories, it scans a temporary directory holding one planted file with `#include <SDL/SDL.h>` and fails the run if that scan passes, and one with only standard includes and fails if that scan fails. Total cost: milliseconds.
- `app/sd/` is scanned too (it has no reason to see SDL); `app/sdl/` and `app/main/` (Phase 4) will not be.

## Implementation Chunks

1. **The `app/` directory and the move.** Move `record.c`, `seed_save.c` and their two headers into `app/sd/` (a rename, no content change); add the app archive, the include paths, `lint` over `app/`, `ARM_UNIT_SRC`; update `CLAUDE.md` § Layout (and `docs/onion.md`'s link per H-3.1-5). Done when `make check` is green with every test unchanged and `make unit-arm` passes (needs docker and qemu-user; a missing one is a setup problem, reported as such).
2. **Screen model.** The UI core's first module: types, initialise, add a row, validate, scroll follow; the core's restricted include path in the build; its unit test. Done when the test covers each function's rules and `make lint` builds it.
3. **Grid renderer and text form.** Grid bounds and default size, the page-size function, render with every layout rule above, the text form; the snapshot helper in the test support code; snapshot files; the snapshot and rule tests. Done when every snapshot listed under Testing Decisions matches.
4. **The SDL check and the cross run.** The script with its self-proof, wired into `make test`; `CLAUDE.md` § Checks line; `make unit-arm` run with the new tests (count in the step note). Done when `make test` fails on a planted include in `app/core/` (checked by hand, then removed) and passes without it, and `make unit-arm` passes.

## Testing Decisions

- **What a good test is here:** it builds a screen through the public functions, renders it, and compares the whole text form with a committed snapshot, or checks one public function's result; it never inspects the grid's internal arrays beyond the public accessors, and never depends on how the layout is computed.
- **Modules tested:**
  - *screen model* (one unit test binary): initialise gives the documented empty screen; adding past 64 rows is refused and adds nothing; validation refuses each out-of-range field (cursor −2 and `rows`, scroll past the last row, active tab out of range, counts past capacity) and accepts the boundaries (cursor −1, scroll 0 on an empty list); scroll follow on cursor above, inside and below the page, at both ends, with fewer rows than the page, and with no cursor.
  - *grid renderer and text form* (one unit test binary), snapshots of hand-built screens:
    1. a Generate-like screen at the default grid: two tabs (the first active), a title, more setting rows than the page with the cursor on a setting halfway down (scroll set by scroll follow), one disabled row, one setting the cursor is not on, an action row, an info message and a footer, so both scroll marks show;
    2. the same screen at a small grid (for example 24 × 8): every truncation rule (label cut, value kept, value cut) and both scroll marks;
    3. the same screen at a large grid (for example 60 × 24), no scroll mark;
    4. a detail-like screen: title, text rows only, no cursor, "more below" set on a slice, an error message;
    5. the cursor on a disabled setting (selected disabled style, no arrows);
    6. an empty screen (no tabs, no rows, no text): the blank layout.

    Rule tests: grid sizes just inside and outside the bounds; an invalid screen leaves a blank grid and returns false; a NULL text draws as empty; a byte outside printable ASCII draws as `?`; the text form refuses a buffer one byte too small and fills one of the exact size; the page size is `lines − 4`.
- **Snapshot files** are plain text, one per screen, in a snapshot directory beside the unit tests, read with a path relative to the randomizer test directory, as the oracle fixtures are (so `make unit-arm`, which runs from there, finds them). The helper (test support code, not the UI core) compares the whole text; on a difference it prints the expected and actual text and the first differing line number, and fails through `test_fail`. There is **no update mode**: a snapshot changes only by a deliberate edit, stated in the commit message. The first snapshots are written by hand from the layout rules above and reviewed against them, not copied from the renderer's output.
- **`make lint`** builds the core's sources with the restricted include path and `-pedantic-errors`, and the SD layer's with the full one.
- **The SDL check** is itself tested by its planted-file self-proof on every run (above), plus the hand check of chunk 4.
- **`make unit-arm`** runs both new tests under qemu-arm with the rest; the step note records its counts.
- **The move is tested by the existing tests:** `test_record`, `test_seed_save`, `test_record --fixtures` (in `make logic`) and the whole `make check` stay green and unchanged.
- **Time:** the two new tests run in milliseconds; a clean `make test` is timed before and after chunk 1 and after chunk 4, numbers in the step note (step 2.3's note gives 11 s).
- **Prior art:** `test_record` (exact text compared, strict refusals), `test_rom_text` (expected bytes written from the rules, not from the code), `selftest.sh` (a check proving it catches a planted fault).

## Out of Scope

- The string table and the literal check (step 3); the screen model holds already-resolved text.
- Any state machine, any key, any key mapping (steps 4–6); the footer's legend text is the state machines'.
- `ui_cli` (step 7), the SDL renderer, fonts, colours and cell pixel sizes (Phase 4 step 1).
- Word wrapping of long text (step 6 decides whether the detail view wraps or cuts); the renderer only cuts.
- UTF-8 or any non-ASCII text.
- Moving or renaming anything else in `rando/host/` (case reader, spoiler, playthrough, output writers, `patch_cli.c`), the settings-table work of step 4, creating SD directories (step 5 or Phase 4).
- A `tests/app/` directory or a rename of `tests/rando/`.
- Editing the plan file: the §5 strike-through and the step note are `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): open question 2's answer — the UI core in `app/core/`, the device-bound `record.c` and `seed_save.c` in `app/sd/`, `rando/host/` unchanged otherwise, `app/sdl/` and `app/main/` reserved for Phase 4 and step 7; the outcome of H-3.1-1 to H-3.1-5 once step 1 is done; the `make test` times; the `unit-arm` counts.
- **For step 7 (`ui_cli`):** the plan's rule "links only `$(RANDO_LIB)`, `$(HOST_LIB)`" should read "plus the app archive", which `make test` also builds at the same flags; still never the `-O2` set.
- **For step 3:** the renderer's glyphs are character constants by design, so the "no user-visible string literal in the UI core" check can treat character constants as not shown text, or list these glyphs.
- **For steps 4–6:** a greyed row's reason is drawn by putting it in the message line while the cursor is on that row; the Generate tab's 15 rows (11 settings, hints, heart beep, spoiler, Generate) exceed the default page of 11 lines, so it scrolls; the longest ROM name (`RANDO_NAME_ROM_MAX` 48 bytes, in practice 36) fits the default 38-column field.
- **For Phase 4 step 1:** the SDL renderer draws the grid, not the screen: one cell per character, a line's style as its colours (the selected row as Onion's highlight bar), the frame and tags of the text form not drawn.
- Remaining open warning carried in `notes/phase2-warn-review.md`: "`rando_seed_save.h`'s SD path constants are unused until step 3.5" — still true after the move; only their header's directory changes.
