---
title: "History tab"
description: "The player cannot see the seeds they generated, read a seed's settings or spoiler on the console, or delete a seed's ROM, save and save states without hunting through three folders with a file manager."
status: needs-triage
---

## The step (Plan 1, Phase 3, step 6, copied from `docs/plan-1.md`)

> 6. **History tab.** The list is newest first and reconciled at start with the ROMs actually present. It has a detail view (settings, spoiler if kept, scrollable) and a delete action with confirmation, which removes the ROM, `.srm` and save states, then the record. Verified by snapshot tests on a temporary directory tree mimicking the SD card, checking the files removed and the files kept.

Context the step relies on, from the plan and the repository at HEAD (`eb4fe80`):

- §2 *History tab*: "It lists the generated seeds and shows each one's settings and spoiler." "Deleting a seed, after a confirmation, removes the ROM, its RetroArch `.srm` save and its RetroArch save states." "A ROM deleted by hand with Onion's file manager simply disappears from History at the next start." §2 *Spoiler*: a seed made without it "still appears in History, with its settings but no item locations". §2 *UI*: two tabs, Generate and History, Onion-style lists. §2 *UI architecture*: a pure C state machine, a key in, a screen description out; every shown string through the translation table. §4: the snapshots cover "History list, detail and delete"; the owner checks on the device that "delete from History removes the ROM, save and states".
- `docs/onion.md` §7–§10, the facts this step builds on:
  - save `Saves/CurrentProfile/saves/Supafaust/<base>.srm`; states in `Saves/CurrentProfile/states/Supafaust/`, every one starting with `<base>.state` (`.state`, `.stateN` for any N, `.state.auto`, an optional `.png` after each); `<base>` is the file name minus its last extension, the content folder ignored; `Open-Ganon-1CA10A-2.state` does not start with `Open-Ganon-1CA10A.state`;
  - Guest Mode renames folders: the inactive profile's files sit under `Saves/MainProfile/` or `Saves/GuestProfile/`; History and `Roms/` are not per profile;
  - other per-ROM files: RetroArch's `content_history.lpl`, MainUI's `Roms/recentlist.json`, the game switcher's `romScreens/<hash>.png`, the play-activity SQLite, the per-ROM core override `<rom dir>/.game_config/<base>.cfg`;
  - **C4** (steps 5 and 6): after deleting a ROM, delete `Roms/SFC/SFC_cache6.db` and `SFC_cache2.db`; **C5–C8**: decisions of this step (made below, decision 7).
- Step 2.3: `rando_record_parse` is strict and refuses a record whose `rom` differs from the name its settings and code give, so "History can only delete files the app named"; `rando_seed_save` treats a name as taken when its ROM *or* its record exists. Records are `App/ALttPR/history/<code>.json`, at most `RANDO_RECORD_TEXT_MAX` (64 KiB), the spoiler a span into the text.
- Step 3.1's note (via step 2.3): tests that list directories need `-D_FILE_OFFSET_BITS=64` under qemu-arm; `make unit-arm` already sets it.
- Step 3.2: `UiScreen` (tabs, title, up to 64 rows of kind setting/action/text, cursor −1 for none, scroll, `more_above`/`more_below` "set by a state machine handing over a slice of a longer list", info or error message, footer); the 40 × 15 grid, page = lines − 4, a text field of `cols − 2` = 38 characters; hand-written snapshots, no update mode.
- Step 3.3: `UiStrId` from one X-macro list with arities; `UI_STR_HISTORY_DELETE_CONFIRM` ("Delete {}?", arity 1) already exists; `ui_strings_check` (printable ASCII, ≤ 38 characters with `{}` counting zero); `no_literals.py` refuses a letter in any `app/core` literal outside `ui_strings_en.c`.
- Step 3.4: `UiButton`/`UiInput`; the Generate tab returns `UI_GENERATE_QUIT` on B and Menu, `TAB_PREV`/`TAB_NEXT` on L1/R1; `ui_setting_label`/`ui_setting_value` give each setting's label and value ids.
- Step 3.5 at HEAD: chunks 1–3 are committed — the three status enums end with `_COUNT`, `rando_out_read_capped` / `rando_out_random_seed` / `RANDO_OUT_PATCH_MAX` are public in `rando_out.h`, and `ui_genflow` (with `UiGenOutcome.ok`, `.rom`, `.list_stale`, ids only) and its snapshots exist. Chunks 4 (SD runner, status tables) and 5 (end to end) are not done.
- `notes/phase2-warn-review.md`: "`record.c`'s JSON parser beside `case_reader.c`'s (a third is prevented …)"; "repeated read/write-file helpers".
- Build fact: `app/sd` sources are compiled with `app/core` on their include path (`TEST_INCLUDES`), `app/core` with only the library and itself.

## Hypotheses about Phase 3 step 5 (not finished when this PRD was written)

Step 5's PRD is `docs/prd/triage/phase3-step5-generation-flow.md`. Each assumption about the outcome of its chunks 4–5 is labelled, with what this step does if it fails.

- **H-3.5-1** (SD root): `rando_app_sd_root(app_dir, out, cap)` exists in `app/sd` as that PRD's decision 5 gives it. History's SD init takes an SD root string, so it does not call it itself; the shell (step 7, Phase 4) passes the same root to the runner and to History. *If it fails* (another name or place), nothing in this step changes; the end-to-end test passes the temporary root directly.
- **H-3.5-2** (cache path constants): the two list caches `Roms/SFC/SFC_cache6.db` and `Roms/SFC/SFC_cache2.db` are constants beside `RANDO_SD_ROM_DIR` in the seed-save header. *If it fails* (not added), this step adds them there.
- **H-3.5-3** (cache removal): step 5's runner removes both caches after a save, "a cache that is absent is fine; one still present after `remove` sets `list_stale`". This step needs exactly that after a delete. *If it is a static of the runner* (the likely outcome), chunk 4 extracts it into one public SD function taking the SD root and returning whether a cache is still present, the runner calling it, with `test_app_gen` passing unchanged; *if it is already public*, History calls it; it is never copied.
- **H-3.5-4** (status tables): step 5's status tables live in `app/sd`, are `const` arrays sized by the enum's `_COUNT` with designated initializers, return `UiStrId`s, and are walked by a test failing on a value without an id. History's delete status follows the same pattern in the same layer. *If they live elsewhere* (say a core-side mapping), History's table goes where step 5 put its own, with the same rule.
- **H-3.5-5** (string ids): step 5's chunk 4 appends its status and advice ids after `UI_STR_GENFLOW_FOOTER`; History's ids are appended after whatever id is last when this step is implemented. Snapshots are text, so no renumbering can change one. *If it fails*, nothing changes.
- **H-3.5-6** (folders): step 5's runner creates `Roms/SFC/ALttPR/` and `App/ALttPR/history/` before its first save. History creates no directory: a missing history folder is an empty list, a missing save or state folder is nothing to remove. This holds whatever step 5 does; it is stated so no one adds a `mkdir` here.
- **H-3.5-7** (POSIX in the SD layer): step 5's runner defines `_POSIX_C_SOURCE` in its own source (for `mkdir`) and `make lint` accepts it under `-std=c99 -pedantic-errors`, as the unit tests already do at HEAD. History's SD source does the same for `opendir`/`readdir`/`stat`. *If step 5 used another mechanism* (a Makefile flag), History follows it.
- **H-3.5-8** (test helpers): step 5's `test_app_gen` builds its temporary SD tree with private helpers (`mkdtemp`, make-directory, write-file, exists). *If it added shared helpers* under `tests/rando/unit/`, `test_history` uses them instead of its own.
- **H-3.5-9** (refresh after generation): step 5's "For step 7" note has the shell show the Generate tab again after a run. This step adds the matching contract: after an outcome with `ok`, the shell rescans History and re-initialises the tab, so the new seed is on top. *If step 5's note says otherwise*, step 7 reconciles the two; this step's interface (scan, init) is unchanged.

## Problem Statement

A player who has made a few seeds on the console has nowhere to see them. The SNES list shows file names such as `Open-Ganon-1CA10A.sfc`, which say the mode and goal but not the swords, the item pool or whether hints were on; the spoiler they asked to keep sits in a JSON file they cannot read on the device. When they finish a seed, or give up on one, getting rid of it means deleting the ROM in one folder, its save in `Saves/CurrentProfile/saves/Supafaust/`, up to a dozen save states in `.../states/Supafaust/`, and the History record — with a file manager, on a d-pad. Miss one and the leftover state auto-loads into nothing, or the record lingers. And if they deleted a ROM by hand already, the app must not keep offering a seed whose game is gone.

## Solution

**A History tab beside Generate.** L1/R1 switch tabs. History lists every generated seed whose ROM is still in `Roms/SFC/ALttPR/`, newest first, one row per ROM file name; the info line under the list shows when the seed under the cursor was generated. An empty History says so and points to the Generate tab.

**Reconciled at start.** At start (and after each successful generation) the app reads `App/ALttPR/history/`, keeps only the records it can fully trust (strict parse, file name equal to the record's code, ROM name equal to what the settings make) and only those whose ROM is present. A ROM deleted by hand simply disappears from the list; its record stays on the card, so the seed's name is never reused and the seed comes back if the ROM is put back. Unreadable records are skipped, never deleted, with a one-time notice.

**A scrollable detail view.** A opens the seed: its date, seed number, the thirteen settings and the spoiler option as the Generate tab names them, then the spoiler — every section of it, as an indented outline wrapped to the screen — or "No spoiler kept for this seed". Up/Down scroll a line, L2/R2 a page, B goes back.

**Delete with confirmation.** X, from the list or the detail, asks "Delete 1CA10A?" with the ROM name and what goes. A deletes: the ROM, its per-ROM core override, its `.srm` and every save state in each profile folder on the card (current, main and guest), then the record, then Onion's SNES list caches so the list forgets it. B keeps it. The list then says "Seed deleted" (or asks for a list refresh when a cache would not go), or names what could not be deleted and keeps the seed listed so delete can be tried again.

**Tested without a console.** The tab's state machine is tested with fake SD functions and hand-written snapshots; the SD side on a temporary directory tree mimicking the card, checking every file removed and every look-alike kept; one end-to-end test drives the tab through the real SD functions on such a tree.

## User Stories

1. As the player, I want a History tab next to Generate, so that I can find my seeds without leaving the app.
2. As the player, I want L1 and R1 to switch between Generate and History, so that tabs work like Onion's menus.
3. As the player, I want the newest seed at the top, so that the one I just made is the first I see.
4. As the player, I want each row to show the ROM's file name, so that I recognise it from the SNES list.
5. As the player, I want the date the seed under the cursor was generated shown under the list, so that I can tell similar names apart.
6. As the player, I want Up/Down to move through the list and wrap, so that long lists are quick to cross.
7. As the player, I want L2/R2 to jump a page, so that I can reach old seeds fast.
8. As the player, I want arrows when there are seeds above or below the screen, so that I know the list goes on.
9. As the player, I want an empty History to tell me to make a seed in the Generate tab, so that an empty screen is not a mystery.
10. As the player, I want a ROM I deleted with Onion's file manager to vanish from History at the next start, so that History matches what I can play.
11. As the player, I want a ROM I put back to reappear in History, so that moving files around never loses a seed's record.
12. As the player, I want a seed I just generated to be in History when I switch tabs, so that I do not need to restart the app.
13. As the player, I want a notice when some History records cannot be read, so that I know why a seed is missing.
14. As the player, I want unreadable records left on the card, so that the app never deletes something it does not understand.
15. As the player, I want A to open a seed's details, so that I can see what I am about to play.
16. As the player, I want the details to show the seed's date and seed number, so that I can tell two seeds of the same settings apart.
17. As the player, I want the details to list all thirteen settings with the same names and values as the Generate tab, so that I can read them without learning a second vocabulary.
18. As the player, I want the details to say whether the spoiler was kept, so that I know whether to look for it.
19. As the player, I want the spoiler shown in the details when I kept it, so that I can look up where an item is when I am stuck.
20. As the player, I want every spoiler section (locations by region, bosses, equipment, specials, shops, the playthrough), so that nothing the spoiler holds is hidden.
21. As the player, I want the playthrough's spheres in numeric order (1, 2, … 10, 11), so that it reads in play order.
22. As the player, I want location and item names without VT's `:1` world suffix, so that the text reads like the game.
23. As the player, I want long spoiler lines wrapped instead of cut, so that I can read every location name.
24. As the player, I want the spoiler's settings duplicate (VT's `meta`) left out, so that the settings appear once.
25. As the player, I want "No spoiler kept for this seed" when I generated without it, so that its absence is explained.
26. As the player, I want a clear line when the spoiler cannot be read, so that a broken record does not look empty.
27. As the player, I want Up/Down to scroll the details a line and L2/R2 a page, so that I can read a 500-line spoiler comfortably.
28. As the player, I want B to go back from the details to the list with the cursor where it was, so that I keep my place.
29. As the player, I want X to delete from the list and from the details, so that I can delete what I am looking at.
30. As the player, I want a confirmation naming the seed before anything is deleted, so that a stray press never loses a run.
31. As the player, I want the confirmation to say the ROM, its save and its save states go, and that it cannot be undone, so that I know what I am agreeing to.
32. As the player, I want only A to confirm and B to keep the seed, so that the safe answer is the usual back button.
33. As the player, I want delete to remove the ROM, its `.srm` and every save state, auto state and thumbnail, so that nothing of the seed is left to auto-load.
34. As the player, I want delete to also remove the seed's save and states in the other Guest Mode profile, so that no stale save of a deleted seed waits there.
35. As the player, I want delete to remove a per-ROM core override I set for that seed, so that the seed's folder holds nothing of it.
36. As the player, I want delete never to touch another seed's files, even `-2` siblings of the same code, so that deleting one seed never harms another.
37. As the player, I want the deleted seed gone from Onion's SNES list without pressing Refresh, so that I do not try to launch it.
38. As the player, I want to be told to refresh the SNES list when the app could not clear its cache, so that a ghost entry is explained.
39. As the player, I want "Seed deleted" after a delete and the cursor on the next seed, so that I can delete several in a row.
40. As the player, I want a clear message naming what could not be deleted (the ROM, some save files, the record), so that I can check the card.
41. As the player, I want a seed whose delete failed to stay listed, so that I can try again.
42. As the player, I want Menu to quit from any History screen without deleting anything, so that leaving works the same everywhere.
43. As the player, I want every History text to fit on one line, so that nothing reads as cut off.
44. As the owner, I want the delete's file set built from the ROM's base name only, as RetroArch names saves, so that it matches `docs/onion.md` §7 exactly (C5).
45. As the owner, I want delete to act only on names a strictly parsed record gave, so that the app can only delete files it named.
46. As the owner, I want the record removed last, so that a delete interrupted half-way can be retried from History.
47. As the owner, I want Onion's shared lists (RetroArch history, Recents, play activity, game-switcher screenshots) left untouched, so that the app never corrupts a file it does not own (C8).
48. As the owner, I want the C5–C8 decisions written down with their reasons, so that the README and the device checks can cite them.
49. As the owner, I want every delete status mapped to a string id by a table sized by the enum's count, checked by a test, so that a new status cannot reach the screen without a text.
50. As the owner, I want the spoiler outline built on `record.c`'s parser, so that no third JSON parser appears.
51. As the owner, I want the scan to read records through `rando_out_read_capped`, so that the file helpers are not copied again.
52. As the owner, I want History to create no directory, so that the runner stays the one place that does.
53. As the owner, I want the cache removal shared with the generation runner, so that C4 is implemented once.
54. As the agent, I want snapshot tests of the list, the empty list, a scrolled list, the details with and without a spoiler, the confirmation and each delete outcome, so that I see what the player sees.
55. As the agent, I want SD tests on a temporary tree mimicking the card that list every file removed and every file kept, so that the delete's file set is proven, not assumed.
56. As the agent, I want an end-to-end test from the History list through the real SD functions, so that the whole tab is proven on the host.
57. As the net, I want the new sources built by `make lint` with `-pedantic-errors` and the new tests run by `make unit-arm` under qemu-arm, so that directory listing behaves the same on the device.
58. As a future translator, I want each History text to be its own id, so that my language can word each one.

## Implementation Decisions

### 1. Interface between the core and the SD layer

The core stays pure C99 with only the library and itself on its include path; the SD layer may include core headers (the build already allows it; step 5's runner fills `UiGenOutcome`). New core header (names indicative):

- `UiHistoryDate`: year, month, day, hour, minute, second (the record's local date, copied field by field by the SD layer; the core never includes `rando_record.h`).
- `UiHistoryEntry`: the `RandoName` (code and ROM), the date, the seed, the `RandoOptions` (hints included), the `RandoHeartBeep`, `has_spoiler`.
- `UiHistoryList`: up to `UI_HISTORY_MAX` = 256 entries, newest first, a count, and three notice flags: `unreadable` (at least one record skipped), `truncated` (more than 256 listable seeds), `folder_unreadable` (the history folder exists but cannot be listed).
- `UiHistorySpoiler`: `text[UI_HISTORY_SPOILER_MAX]` (64 KiB, the same size as `RANDO_RECORD_TEXT_MAX`, which the SD layer asserts), its length, and `cut` (the outline did not fit). The text is the outline of decision 3: lines separated by `\n`, each starting with its indentation in spaces.
- `UiHistorySpoilerFn(ctx, const UiHistoryEntry *, UiHistorySpoiler *)` → bool: false when the record or its spoiler cannot be read. Called only for an entry with `has_spoiler`.
- `UiHistoryDeleteOutcome`: `ok` (the record is gone: the entry leaves the list), `list_stale` (a SNES list cache is still present), `status` (a `UiStrId`, the result line).
- `UiHistoryDeleteFn(ctx, const UiHistoryEntry *, UiHistoryDeleteOutcome *)`: synchronous, must always fill the outcome. Deleting a handful of small files takes milliseconds, so there is no "Deleting…" screen.
- Spoiler texts are VT's data (region, location and item names), shown verbatim like the ROM name; only the outline's layout is added. Translating game names is out of scope; every text the app itself words is a string id.

### 2. The History tab state machine (core)

- **State** (`UiHistory`, caller-owned, no allocation, ≈ 200 KiB, so a static object in production): the string table, page and width, a *copy* of the list (so the tab can drop a deleted entry), the view (`LIST`, `DETAIL`, `CONFIRM`) and the view the confirmation came from, the list cursor and scroll, the detail scroll, the spoiler buffer and its wrapped-line index (`UI_HISTORY_LINES_MAX` = 8192 entries of offset, length, indent), the spoiler state of the open entry (`NONE` kept, `OK`, `UNREADABLE`, `CUT`), the one-shot message (kind and id, plus its filled text), the injected functions and context, and the buffers the screen borrows (date line, seed line, confirmation title).
- `ui_history_init(state, const UiHistoryInit *)`: init holds strings (NULL = English), `page` (`ui_grid_page`, ≥ 1), `width` (the text field, `cols − 2`, ≥ 20), the list (copied), the two functions and their context. View `LIST`, cursor 0, scroll 0; the notice of the first set flag (`folder_unreadable`, then `unreadable`, then `truncated`) is the one-shot message until the first key. Called at start and again after each successful generation (H-3.5-9): the newest seed is then under the cursor.
- `ui_history_key(state, UiButton)` → `UI_HISTORY_NONE`, `_QUIT`, `_TAB_NEXT`, `_TAB_PREV`. Every key first clears the one-shot message.
  - **List**: Up/Down move and wrap; L2/R2 move a page, clamped; A opens the detail (no-op on an empty list); X opens the confirmation (no-op when empty); B and Menu give `QUIT` (the Generate tab's rule); L1/R1 give the tab events; everything else nothing.
  - **Detail**: Up/Down scroll one line, L2/R2 one page, clamped to `[0, max(0, lines − page)]`; B back to the list, cursor unchanged; X opens the confirmation; Menu `QUIT`; L1/R1 the tab events (the view is kept, so switching back returns to it); everything else nothing. Opening the detail sets scroll 0 and, when `has_spoiler`, calls the spoiler function once and wraps the result; the detail is not re-read while open.
  - **Confirm**: A calls the delete function exactly once for the entry; B returns to the view it came from; Menu `QUIT` (nothing deleted); L1/R1 and everything else nothing.
  - **After a delete**: `ok` → the entry is removed from the copy (order kept), view `LIST`, cursor on the same index clamped to the new count (so the next older seed), scroll followed, message info `Seed deleted` or `Seed deleted: refresh the SNES list` when `list_stale`. Not `ok` → back to the view the confirmation came from, entry kept, message error = the outcome's status text.
- `ui_history_screen(state, UiScreen *)`: tabs `Generate` / `History`, History active, in every view.
  - **List**: title `Generated Seeds`; one action row per entry, label = the ROM file name (at most 36 characters, fits the 38 field); the list hands the screen the page-sized slice `[scroll, scroll + page)` (the list can exceed `UI_ROWS_MAX`), the cursor relative to it, `scroll` 0 and `more_above`/`more_below` set; the scroll follows the cursor with `ui_scroll_follow`. Message: the one-shot message if any, else info `Generated <date>` for the cursor entry (`YYYY-MM-DD HH:MM`, formatted by the core from digits). Footer `A Details  X Delete`. **Empty**: text rows `No seeds yet` and `Make one in the Generate tab`, cursor −1, no info line (a notice still shows), no footer.
  - **Detail**: title = the ROM file name; a virtual list sliced to the page like the list, cursor −1: text `Generated <date>`, text `Seed <decimal seed>`, the thirteen setting rows (label and value ids of `ui_setting_label`/`ui_setting_value`, in the table's order), a setting row `Spoiler in History` with `On`/`Off` (existing ids), a blank text row, then: when `has_spoiler` and read, a text row `Spoiler` then one text row per wrapped outline line, and after them `Spoiler cut: too long` if `cut` or the line index filled; when `has_spoiler` but unreadable, `Spoiler could not be read`; otherwise `No spoiler kept for this seed`. Footer `B Back  X Delete`. Message: the one-shot message only.
  - **Confirm**: title `Delete {}?` (the existing `UI_STR_HISTORY_DELETE_CONFIRM`) filled with the entry's *code* (`Delete 1CA10A-99?` is 17 characters; the ROM name would overflow 38); text rows: the ROM file name, `Removes the ROM, its save and states`, `This cannot be undone`; cursor −1; footer `A Delete  B Keep`.
- **Wrapping** (pure, in the core): a line holding at most `width` characters is one row. A longer one breaks at the last space within `width` that is after its indentation, or at `width` when there is none; each continuation row is indented two spaces more than the line's own indentation (no extra when that would leave fewer than eight characters). Non-ASCII bytes are left for the grid, which draws them as `?`.
- **Rule compliance**: no stdio, no allocation, no SDL; no letter in any core literal (the date's `-` and `:` are character constants); every text above is an id with its English entry.

### 3. The spoiler outline (SD layer, inside `record.c`)

- A new public function beside `rando_record_parse`, on the same lexer: `rando_record_spoiler_outline(text, offset, size, out, cap, *len, *cut)` → bool, given the span a parse returned. False only on a malformed span (a parsed record never has one) or a nesting deeper than `RANDO_RECORD_OUTLINE_DEPTH` = 8 (VT's spoiler reaches 5: Shops → element → `item_0` → `item`).
- **Layout**: one line per member, indented two spaces per depth from 0; an object or array member is its key alone, followed by its members one level deeper; a scalar member is `key: value`; an array's elements are headed by their 1-based index; strings are copied byte for byte between their quotes (an escape sequence is shown as written), numbers and literals as written. A string ending in `:` followed by decimal digits only (VT's world suffix, `Blacksmith:1`) loses that suffix, in keys and values. The top-level `meta` member is skipped (it repeats the settings shown above it). Within an object, members whose key is all decimal digits come first in numeric order, then the others in record order (the record is bytewise sorted, so the playthrough's `"10"` would come before `"2"`).
- **Cut**: the outline stops at the last whole line that fits `cap` (newline included) and sets `*cut`; it never fails for size.
- Why here: `record.c` already owns a JSON lexer for this exact text; a walker anywhere else would be the third JSON parser the warn-review rules out.

### 4. The History store (SD layer)

- `RandoHistory` (caller-owned, long-lived, static in production): the SD root and joined paths (ROM folder, history folder, the three profile folders `Saves/CurrentProfile`, `Saves/MainProfile`, `Saves/GuestProfile`, the `.game_config` folder), a `RANDO_RECORD_TEXT_MAX` + 1 read buffer. New constants beside the existing SD paths, relative to the root: the three profile folders, the core folder name `Supafaust`, `saves` and `states`. `rando_history_init(h, sd_root)` → false when any joined path would exceed `RANDO_SEED_DIR_MAX`.
- `rando_history_scan(h, UiHistoryList *out)` — the reconciliation, run at start and after a successful generation:
  1. `opendir` the history folder. Missing → empty list, no notice. Any other failure → empty list, `folder_unreadable`.
  2. Each entry named `<code>.json`, `<code>` passing `rando_name_code_valid`, is a candidate; every other name (`._1CA10A.json` from macOS, `*.tmp`, sub-folders) is ignored silently.
  3. A candidate is read with `rando_out_read_capped` at `RANDO_RECORD_TEXT_MAX` and parsed with `rando_record_parse`; a read failure, a parse failure, or a record whose code differs from the file's sets `unreadable` and skips it. The file is never touched.
  4. The record's ROM, `<ROM folder>/<rom>`, must be a regular file (`stat`); otherwise the record is hidden: not listed, not counted as unreadable, **not deleted** (decision 6).
  5. Listed entries are sorted newest first: date descending, then code descending bytewise. Beyond 256, only the 256 newest are kept and `truncated` is set.
- `rando_history_spoiler` is a `UiHistorySpoilerFn` (ctx = `RandoHistory`): re-reads `<history>/<code>.json`, parses it, requires its ROM to equal the entry's, then writes the outline into the `UiHistorySpoiler`. Any failure → false.
- **Cost, for Phase 4 step 3**: the scan reads every record whole (the strict parse needs it): a record without spoiler is under 1 KiB, one with a spoiler about 25–40 KiB; 100 spoiler records are ≈ 3 MiB, under a second at 5 MB/s. Recorded, not gated.

### 5. Delete (SD layer)

- `rando_history_delete` is a `UiHistoryDeleteFn` (ctx = `RandoHistory`). It asserts the entry's code is valid and its ROM name ends in `.sfc`; `<base>` is the ROM name without `.sfc`. In order:
  1. **ROM**: `remove(<ROM folder>/<rom>)`. Absent afterwards (removed, or already gone) → go on; still present → status `ROM`, **stop**: nothing else is touched, no cache is removed.
  2. **Per-ROM core override** (C8): `<ROM folder>/.game_config/<base>.cfg`, removed if present.
  3. **Saves and states** (C5, C6): for each of the three profile folders that exists: `<profile>/saves/Supafaust/<base>.srm`, removed if present; then in `<profile>/states/Supafaust/` every *regular file* whose name starts with `<base>.state` (any slot number, `.auto`, `.png`). The state folder is re-listed after each removal (open, find the first match, close, remove, again), so no removal happens while a listing is open; a match that will not go ends that folder's loop. Directories and other names are left alone and are not failures. Exact-name paths (`.srm`, `.cfg`) count as a failure when still present after `remove`, whatever their type.
  4. Any failure in 2–3 → status `FILES`, the remaining files of 2–3 still tried, the record **kept**.
  5. **Record**: `remove(<history>/<code>.json)`; still present → status `RECORD`.
  6. **Caches** (C4): whenever step 1 removed the ROM (even if 2–5 then failed, since the list changed), the two SNES list caches are removed through the shared cache function (H-3.5-3); one still present sets `list_stale`.
- `ok` is true only after step 5 succeeded. A failed delete leaves the record, so the seed stays listed (in the session) or, when the ROM is already gone, hidden at the next start with its record still reserving its name; retrying from the list in the session redoes 1–6, where absent files are not failures.
- **Status** `RandoHistoryDeleteStatus` (`OK`, `ROM`, `FILES`, `RECORD`, count) and its enum-sized table to `UiStrId` (H-3.5-4): `OK` → `Seed deleted`, `ROM` → `ROM could not be deleted`, `FILES` → `Some save files were not deleted`, `RECORD` → `History record was not deleted`.

### 6. Reconciliation: hide, do not delete

- "A ROM deleted by hand … simply disappears from History at the next start" is met by hiding the record (decision 4, step 4). Deleting it was rejected: the user may have *moved* the ROM (to `Roms/SFC/` itself, say), where it still plays with the same save (the content folder is ignored, `docs/onion.md` §7); and a kept record keeps the name taken (`rando_seed_save` checks the ROM *and* the record), so no later seed can ever be given a name whose stale save or states are still on the card. A ROM put back reappears. The cost — orphan records of a few hundred bytes to 40 KiB each — is accepted.
- The orphan's saves and states are never deleted by the scan, for the same reason (a moved ROM still uses them). The Phase 5 README states that deleting from History is the way to remove a seed's saves.

### 7. C5–C8, decided

- **C5 — the file set.** Built from the ROM's base name only, never from the content folder: `saves/Supafaust/<base>.srm` and every regular file of `states/Supafaust/` starting with `<base>.state`, in each profile folder present. The prefix covers `.state`, `.stateN` for any N (auto-index 0–10 and hand-picked slots), `.state.auto` and every `.png` thumbnail; `<base>-2.state…` never matches `<base>.state` (§7), so `-<n>` siblings are safe. Only the `Supafaust` folder (the default core's `library_name`): a save made with another core picked through a per-ROM override lives under that core's own folder and is left (recorded as a limit; device check 3 confirms the folder name).
- **C6 — Guest Mode.** Delete also removes the seed's `.srm` and states under `Saves/MainProfile/` and `Saves/GuestProfile/` when they exist, not only `Saves/CurrentProfile/`. History is not per profile, so the seed is deleted for both players; keeping the other profile's files would leave a save for a ROM that no longer exists, which a later seed reusing the freed name would load when that profile comes back. The confirmation's wording ("its save and states") covers both.
- **C7 — same-name ROMs elsewhere.** No other `Roms/SFC/` folder is scanned or touched: only `Roms/SFC/ALttPR/<rom>` is removed. A copy of the same file elsewhere shares the `.srm` and states (§7), so deleting the seed deletes the save that copy uses. Generated names (a 24-bit code of the seed and settings) make an unrelated clash practically impossible; only a hand-made copy shares them. Recorded here and for the Phase 5 README; not tested beyond "the other copy's ROM is kept".
- **C8 — other per-ROM files.** Delete removes the per-ROM core override `Roms/SFC/ALttPR/.game_config/<base>.cfg` (one file, in the app's own ROM folder, owned by that ROM alone). It leaves RetroArch's `content_history.lpl`, MainUI's `Roms/recentlist.json` (and `recentlist-hidden.json`), the game switcher's `romScreens/<hash>.png` and the play-activity SQLite untouched: each is one shared file per profile owned by Onion or RetroArch, rewriting it while Onion may hold it risks corrupting the user's whole history, SQLite is not available to a pure C99 SD layer, and a stale entry only points at a missing file, which Onion handles as for any ROM deleted by hand. Recorded for the Phase 5 README and as a device check for the owner: what Recents and the game switcher show for a deleted seed.

### 8. English texts

New ids in the X-macro list, after the last id at implementation time (H-3.5-5), each ≤ 38 characters with `{}` counting zero; filled lines checked by a test, not by this table:

| Id (indicative) | English |
|---|---|
| `HISTORY_TITLE` | `Generated Seeds` |
| `HISTORY_EMPTY` | `No seeds yet` |
| `HISTORY_EMPTY_HINT` | `Make one in the Generate tab` |
| `HISTORY_GENERATED_ON` (1) | `Generated {}` |
| `HISTORY_FOOTER` | `A Details  X Delete` |
| `HISTORY_NOTICE_FOLDER` | `History folder could not be read` |
| `HISTORY_NOTICE_UNREADABLE` | `Some History records are unreadable` |
| `HISTORY_NOTICE_TRUNCATED` (1) | `Showing the newest {} seeds` |
| `HISTORY_SEED` (1) | `Seed {}` |
| `HISTORY_SPOILER_TITLE` | `Spoiler` |
| `HISTORY_NO_SPOILER` | `No spoiler kept for this seed` |
| `HISTORY_SPOILER_UNREADABLE` | `Spoiler could not be read` |
| `HISTORY_SPOILER_CUT` | `Spoiler cut: too long` |
| `HISTORY_DETAIL_FOOTER` | `B Back  X Delete` |
| existing `HISTORY_DELETE_CONFIRM` (1) | `Delete {}?` (unchanged) |
| `HISTORY_DELETE_WHAT` | `Removes the ROM, its save and states` |
| `HISTORY_DELETE_FOREVER` | `This cannot be undone` |
| `HISTORY_CONFIRM_FOOTER` | `A Delete  B Keep` |
| `HISTORY_DELETED` | `Seed deleted` (also the `OK` status) |
| `HISTORY_DELETED_REFRESH` | `Seed deleted: refresh the SNES list` |
| `HISTORY_DELETE_ROM` | `ROM could not be deleted` |
| `HISTORY_DELETE_FILES` | `Some save files were not deleted` |
| `HISTORY_DELETE_RECORD` | `History record was not deleted` |

`Generated 2026-11-01 14:05` is 26 characters, `Seed 4294967295` 15, `Showing the newest 256 seeds` 28, `Delete 1CA10A-99?` 17. The agent may reword a text only to satisfy a table rule, stating it.

## Implementation Chunks

1. **Spoiler outline.** `rando_record_spoiler_outline` in `record.c` (decision 3). New `test_record` cases on hand-written spans (layout, suffix stripping, `meta` skipped, numeric-first order, arrays, escapes kept, depth refusal, cut at a line boundary, cut never splits a line) and on the baseline golden's spoiler (pinned lines and the total line count, computed independently with a throwaway Python script, never from the C output); `--fixtures` outlines every golden's spoiler with no cut and no `meta` line. Done when `test_record`'s existing cases pass unchanged and `make check` is green.
2. **History tab state machine (core).** The interface types of decision 1, `UiHistory` (decision 2), wrapping, the ids and English texts, `test_ui_history` with fake spoiler and delete functions and its snapshots. Done when it passes, `no_literals.py`, `no_sdl.sh` and `test_ui_strings` pass, `make lint` builds it, and `make check` is green.
3. **History store: scan and spoiler (SD).** `RandoHistory`, its constants, init, `rando_history_scan`, `rando_history_spoiler`; `test_history`'s scan and spoiler cases on a temporary tree. Done when it passes and `make check` is green.
4. **Delete (SD).** The shared cache function (extracted from the runner per H-3.5-3, `test_app_gen` unchanged), `rando_history_delete` with the C5–C8 file set, `RandoHistoryDeleteStatus` and its table; `test_history`'s delete and table cases. Done when they pass and `make check` is green.
5. **End to end.** The tab driven through the real SD functions on a temporary tree (decision list in Testing Decisions), its snapshots, and `make unit-arm` with the two new tests (counts for the step note). Done when they pass and `make check` is green.

Chunks 1 and 2 are independent; 3 needs 1 (outline) and 2 (types); 4 needs 3; 5 needs all.

## Testing Decisions

- **What a good test is here**: it goes through public functions only — `rando_record_spoiler_outline`, `ui_history_*` rendered by `ui_grid_render` and `ui_grid_text` at 40 × 15, `rando_history_init`/`scan`/`spoiler`/`delete`, the status table — and checks screens, outcomes and files on disk. Expected screens are hand-written snapshots in the snapshot directory, never generated from the code's output, with no update mode (step 3.2's practice). Expected texts come from decision 8's table, expected outline lines from an independent computation.
- **`test_ui_history`** (core; fake spoiler function returning a hand-written outline or false; fake delete function recording its calls and returning a scripted outcome):
  - Keys per view as decision 2: wrap and page in the list, clamped scroll in the detail, every ignored button leaves the screen identical, Menu quits from each view, L1/R1 events from list and detail but not confirm, the tab returning to the detail after a tab switch.
  - The delete function is called exactly once, only on A in the confirmation, with the cursor's entry; B in the confirmation calls nothing; the spoiler function is called once per detail opening and never for an entry without a spoiler.
  - After `ok`: the entry gone, the others in order, the cursor on the next older one (and on the new last one when the last was deleted, and an empty list after the only one); after a failure: entry kept, the view it came from, the status text as an error; the one-shot message gone after the next key.
  - Wrapping: a line of exactly `width` characters is one row, `width + 1` two, a line with no space is cut at `width`, continuations indented two more, the eight-character floor; the line index full gives the cut line.
  - Every new id renders uncut in its screen; `ui_strings_check` passes on English.
- **Snapshots** (40 × 15, hand-written): list of three seeds (newest first, date line); list scrolled past a page with arrows both ways; empty list; the unreadable notice; detail top (date, seed, settings); detail scrolled into the spoiler with a wrapped line; detail of a seed without spoiler, at its end; detail with an unreadable spoiler; confirmation from the list; after a delete (`Seed deleted`); after a delete with a stale list; a failed delete (`Some save files were not deleted`). Twelve.
- **`test_history`** (SD; a temporary tree under `/tmp` via `mkdtemp`, built by helpers as `test_seed_save` and `test_last_settings` do (H-3.5-8); records written by hand from the record's settings writer and `rando_name_make`, with spoilers taken from golden `spoiler.json` files, so no generation runs):
  - **Scan**: no history folder → empty, no notice; three records with crafted dates → newest first, a same-second tie ordered by code; a record whose ROM is absent → not listed, its file still there, no notice; a ROM path that is a directory → hidden; a garbage record, a record whose `rom` does not match, a file `ABCDEF.json` holding code `123456` → skipped, `unreadable`, files kept; `._1CA10A.json`, `notes.txt`, `1CA10A.json.tmp` and a sub-folder → ignored, no notice; 257 listable records → 256 newest and `truncated`.
  - **Spoiler**: the outline of a scanned spoiler record equals `rando_record_spoiler_outline` on the golden span; a record changed on disk after the scan (made unparsable, or its `rom` changed) → false.
  - **Delete, full tree**: the seed's ROM, its `.game_config/<base>.cfg`, in each of `CurrentProfile`, `MainProfile` and `GuestProfile`: the `.srm`, `.state`, `.state1`, `.state10`, `.state27`, `.state.auto`, `.state3.png`, `.state.auto.png`; the record; both caches. **Kept**, each checked present afterwards: a `<base>-2.sfc` sibling with its `-2.srm` and `-2.state`, `<base>.srm.bak`, a sub-folder named `<base>.state9`, another seed's ROM, record and states, the same base name under `saves/Snes9x/`, a same-name ROM in `Roms/SFC/`, `settings.json`, `content_history.lpl` and `recentlist.json` stand-ins. Outcome `ok`, no `list_stale`. A profile folder absent is fine.
  - **Delete failures** (a path made a non-empty directory where an exact-name remove must fail; as root, permissions cannot be used): ROM → `ROM`, every other file and both caches still present; `.srm` → `FILES`, ROM and the other states gone, the record kept, the caches removed; record → `RECORD`, everything else gone; a cache → `ok` with `list_stale`; a retry after removing the obstacle → `ok`, an already-absent ROM not a failure.
  - **Table**: every value of `RandoHistoryDeleteStatus` from 0 to count − 1 has an id other than `UI_STR_NONE`, the ids distinct.
- **End to end** (in `test_history`, which links both layers): a temporary tree with three seeds (one with a spoiler), one ROM missing; scan, `ui_history_init` with the real functions; snapshots of the list (two seeds), A into the spoiler seed's detail scrolled to a spoiler line, X, A; then the list snapshot after the delete, and the full set of files removed and kept as above. This is the step's "snapshot tests on a temporary directory tree mimicking the SD card".
- **Registration**: `test_ui_history` and `test_history` in `UNIT_TESTS` and in the `LOGIC_TESTS` filter-out list (no fixture mode); `test_record --fixtures` gains the outline replay. `make lint` builds the new core and SD sources with `-pedantic-errors`; `make unit-arm` runs both new tests under qemu-arm, `readdir` covered by the existing `-D_FILE_OFFSET_BITS=64`; `no_literals.py` covers the tab; `no_sdl.sh` both directories.
- **Prior art**: `test_ui_generate` and `test_ui_genflow` (hand-written snapshots, fake functions), `test_seed_save` and `test_last_settings` (temporary SD trees, files left after each failure), `test_record` (`--fixtures` over every golden), `test_ui_strings` (the table check).

## Out of Scope

- The shell that routes keys between the two tabs and the generation flow, rescans after a generation, draws frames and quits: step 7 (`ui_cli`) and Phase 4's main. This step fixes the contracts (scan then init at start and after an `ok` outcome; the tab returns tab events, never switches itself).
- Finding the app folder and the SD root: step 5 (H-3.5-1), step 7 and Phase 4.
- Creating any directory (H-3.5-6).
- Editing Onion's or RetroArch's shared lists (C8), saves of cores other than Supafaust (C5), other `Roms/SFC/` folders (C7).
- Deleting orphan records or their saves, a "clean up" action, deleting several seeds at once, renaming or exporting seeds.
- Sorting or filtering History by settings; searching the spoiler.
- Translating game names in the spoiler.
- The step 5 chunks still open (runner, status tables, end to end), other than the cache function's extraction (H-3.5-3).
- The other warnings of `notes/phase2-warn-review.md`.
- Editing the plan file: the step note is `plan-pilot`'s.

## Further Notes

- **For the step note** (`plan-pilot`): the C5–C8 decisions in one line each; that reconciliation hides and never deletes; the outcome of H-3.5-1 … H-3.5-9; whether the cache function was extracted from the runner; `make unit-arm` counts; the scan cost estimate for Phase 4 step 3; that `test_record`'s and `test_app_gen`'s existing cases were untouched.
- **For the Phase 5 README** ("deletion behavior"): delete removes the ROM, its core override, its save and states in every profile, the record and the list caches; a ROM deleted by hand disappears from History but keeps its save and record; a copy of a seed's ROM elsewhere shares its save; Recents and RetroArch history may still name a deleted seed.
- **New device check for the owner** (add to `docs/onion.md`'s list in the step note): after a delete from History, what Recents and the game switcher show for the seed, and that `Saves/GuestProfile/` held no leftover after deleting in the main profile.
- **For step 7 and Phase 4**: one static `RandoHistory` and `UiHistory`; `rando_history_init(<SD root>)`, `rando_history_scan` into a `UiHistoryList`, `ui_history_init` with `rando_history_spoiler`, `rando_history_delete` and the `RandoHistory`; again after each `ok` generation outcome; route L1/R1 events between the tabs.
- **Decisions taken without a human** (to-prd ran unattended): hide orphan records rather than delete them; the delete reaches all three profile folders (C6), the `.game_config` override but no shared Onion list (C8), only the `Supafaust` folder (C5), no other ROM folder (C7); X deletes, only A confirms, B keeps; the confirmation names the code, not the ROM, in its title; Menu quits everywhere; delete is synchronous with no progress screen; the detail shows the whole spoiler as a generic outline, `meta` left out, numeric keys in numeric order, VT's `:1` suffix dropped, lines wrapped; the outline is built in `record.c`; the list is capped at 256 seeds; the tab owns a copy of the list; a failed delete keeps the record and the entry.
