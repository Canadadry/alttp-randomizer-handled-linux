---
title: "LADX logic and locations in C (plan 3, Phase 3 step 3)"
description: "The LADX library has no logic yet, so neither the fill nor the spoiler can be ported; LADXR's logic graph, location kinds and explorer must exist in C, equal to LADXR's on every tier-1 configuration."
status: needs-triage
---

## Problem Statement

Plan 3, Phase 3, step 3, verbatim from `docs/plan-3.md` (§3 Phase 3 item 3):

> 3. **Logic and locations.** Port `logic/` and `locations/` (tier 1 only: `overworld=normal`, no entrance shuffle). Verified by unit tests replaying Phase 1 step 8's logic fixtures, through `make logic`, which this step extends to LADX.

The LADX library (`ladx/`) holds SHA-512, CPython's RNG (step 3.1) and the settings table with `validate` (step 3.2). It has no logic. Step 4 (item pool and fill) places items by asking LADXR's `Explorer` which locations a given inventory reaches, over the graph `logic.main.Logic` builds. Step 6.11 (`items`) patches each location by its kind, and step 6.19 (`spoiler`) rebuilds the logic and walks it again. None of this can start until the graph exists in C, in LADXR's exact order, with its location kinds, their item options and their metadata. LADXR builds that graph from about 3,600 lines of `logic/` and 1,250 of `locations/`, plus `checkMetadata.py` and `explorer.py`.

Matching "roughly" is worthless. The fill draws from the RNG while walking `iteminfo_list` and the explorer's results, so a location out of place, an option list in another order or a requirement folded differently changes every later draw, and the e2e goldens diverge with no clue where. The proof must be the oracle's: Phase 1 step 8 committed `logic_graphs.txt` (the canonical text of the five `logic` baselines) and `logic_vectors.txt` (425 configurations: world setup, the SHA-512 of the canonical graph and six explorer vectors each), and `make logic` must replay them for LADX as it does for ALttP.

## Solution

A C99 port of LADXR's logic at `4325207e`, tier 1 only, in `ladx/`, with no allocation:

- an item table with LADXR's names;
- LADXR's requirement objects (`AND`, `OR`, `COUNT`, `COUNTS`, `FOUND`) with their exact constructor folding, in-place `add`/`remove` and shared identity;
- `Location` with `connect`/`singleConnect` (edge merging into `OR`), the location kinds of `locations/` as far as logic needs them (name id, option list, `configure`, metadata), and `checkMetadata.py`'s table;
- `RequirementsSettings`, the nine dungeons, the `World` overworld and `Logic`'s assembly (entrance loop on the identity mapping, egg requirement, removed dungeon items, discovery order, `configure`);
- the `Explorer`.

A caller hands validated, supported settings and a world setup to one build call and gets a read-only graph equal to LADXR's: the same `location_list` and `iteminfo_list` order, every location's connections in order, and every requirement in its stored order.

`make logic` gains LADX. `tests/ladx`'s `test_logic --fixtures` rebuilds all 425 configurations of `logic_vectors.txt`, checks each graph's SHA-512 and its six explorer vectors, and compares the five baselines of `logic_graphs.txt` line by line. When a graph differs, it writes C's canonical text to a file and names the `extract_logic.py --graph` command that gives LADXR's, so the two can be diffed.

## User Stories

1. As the plan pilot, I want LADX's logic in C equal to LADXR's on every tier-1 configuration, so that step 4 can port the fill on top of it.
2. As the plan pilot, I want `make logic` to replay LADX's logic fixtures next to ALttP's, so that the net proves the logic on every push.
3. As an implementer, I want each chunk to end on a claim against the committed fixtures, so that every TDD cycle has an objective green.
4. As an implementer, I want LADXR's item names as one C table with lookup by name, so that fixtures, requirements and inventories share one id space.
5. As an implementer, I want `OR(...)` to fold to true when any argument is `True`, and to false when every argument is `False`, exactly as LADXR's `__new__`, so that edges are added or skipped as LADXR does.
6. As an implementer, I want `AND(...)` to fold to false when any argument is `False`, and to true when every argument is `True`, keeping `None` arguments as LADXR does (an `AND` with no items and no children, which tests true), so that the graph text matches.
7. As an implementer, I want requirement nodes to keep LADXR's stored order (items first, then children, duplicates kept), so that the canonical text and the explorer's `getItems` order match.
8. As an implementer, I want a requirement node shared by several edges to stay one node, with `remove` (first occurrence, by name for items and by identity for children) visible through every reference, so that `RequirementsSettings`' late removals (sword, powder and bomb under some settings) reach every edge that holds them.
9. As an implementer, I want `COUNT`, `COUNTS` and `FOUND` with LADXR's tests, so that rupee prices and key counts gate as in LADXR.
10. As an implementer, I want an inventory where an item is present once added, even with a count of 0, and where `RUPEES_<n>` items add `n × count` to `RUPEES`, so that item tests and rupee counts match `Explorer.addItem`.
11. As an implementer, I want `Location.connect` with LADXR's defaults (no forward requirement means none; the back requirement defaults to the forward one, the same node), so that each dungeon line ports one for one.
12. As an implementer, I want `singleConnect` to merge a second connection to the same target into `OR(new, old)` (or no requirement when either is none) and to skip a boolean false, so that edge lists match LADXR's.
13. As an implementer, I want `Logic`'s discovery to be LADXR's preorder walk from the start location, in connection order, without unbounded recursion, so that `location_list` and `iteminfo_list` come out in LADXR's order on the device's stack too.
14. As an implementer, I want each location kind of `locations/` (28 classes appear in the baselines) with its name id, its class's base option list and its `configure`, so that every spot's options equal LADXR's after `configure`.
15. As an implementer, I want `checkMetadata.py`'s 288 entries in the library, looked up by name id at construction as LADXR does, so that the graph's metadata columns and later the spoiler match.
16. As an implementer, I want `RequirementsSettings` ported for the five `logic` values and the settings it reads, its enemy, boss and miniboss requirement tables included, so that the dungeons and the overworld read the same requirements.
17. As an implementer, I want each dungeon (D1 to D8 and the colour dungeon) buildable on its own through the public interface, so that a chunk porting dungeons can be checked before the overworld exists, and a later divergence can be narrowed to one dungeon.
18. As an implementer, I want the `World` overworld for `overworld=normal`, its entrances and their enter and exit requirements, so that `Logic`'s entrance loop connects outsides and insides as LADXR does on the identity mapping.
19. As an implementer, I want `Logic`'s egg requirement for every tier-1 goal (`vanilla`, `instruments` with any count including -1 and 0, `open`, `seashells`, `maze`, specific instruments `=<digits>`), so that the `goal` configurations match.
20. As an implementer, I want the removed dungeon keys, nightmare keys and beaks added to the start location exactly as LADXR does (81 key locations for `dungeon_keys=removed`, LADXR's nested loop kept), so that those configurations match.
21. As an implementer, I want the explorer ported with LADXR's semantics (visit, todo list, process to a fixpoint, items picked up from visited spots, the required items of the remaining todo), so that `explore` vectors match and step 4 can drive the fill with it.
22. As an implementer, I want a world setup struct holding what the logic reads (boss mapping, miniboss mapping, goal and goal count), with LADXR's `WorldSetup()` defaults, so that the fixture's `ws` lines feed the build and step 4 can fill it from `randomize`.
23. As an implementer, I want the build to refuse settings that `ladx_settings_supported` refuses, so that the unported tier-2 and tier-3 branches can never run.
24. As an implementer, I want the build to report a full capacity as an error instead of writing past an array, so that a sizing mistake fails a test instead of corrupting memory.
25. As an implementer, I want the test to print the largest counts any configuration reached against each capacity, so that the headroom is visible in the step note.
26. As a debugger, I want a mismatching graph's C text written to `tests/ladx/build/logic/<config>.txt`, with the `extract_logic.py --graph` command that writes LADXR's, so that I can diff the two texts line by line.
27. As a debugger, I want a dungeon mismatch reported with the dungeon, the logic value and the first differing location (by walk position and name), so that I know which dungeon line to read.
28. As a debugger, I want an explorer mismatch reported with the configuration, the vector index and the first differing reached index or required item, so that I can tell a graph bug from an explorer bug.
29. As the owner, I want `make test` to stay fast: the synthetic tests and the five baselines there, and the 425-configuration replay only in `make logic`, so that the fast loop stays in seconds.
30. As the owner, I want `make lint` to build every new `ladx/` source with `-pedantic-errors`, so that the library stays strict C99.
31. As the owner, I want `make unit-arm` to run LADX's logic replay under qemu-arm with `--fixtures`, as it does for ALttP's, so that the logic is proven on the device's architecture.
32. As the owner, I want the logic library to contain no ROM byte and no allocation, so that §2's rules hold.
33. As a step-4 implementer, I want each spot to carry its placed item (none after the build) and its kind-specific constructor data (room, default reward or item, shop index), so that the fill and later the patches can use them without rebuilding.
34. As a step-6.19 implementer, I want location names, dungeons and spot metadata readable from the graph, so that the spoiler can be built from the same structure.
35. As a reader of `tests/ladx/README.md`, I want one section stating `test_logic`'s contract (fixtures read, claims, output lines, mismatch files), with one line in `CLAUDE.md` pointing to it, so that the contract has one home (Phase 3's rule).

## Implementation Decisions

### Scope

- Ported: `logic/requirements.py`, `logic/location.py`, `logic/main.py` (the `World` path only), `logic/overworld.py`'s `World` and `EntranceExterior`, `logic/dungeon1.py` to `dungeon8.py` and `dungeonColor.py` (the `DungeonN` classes, not `NoDungeonN`), `explorer.py`, `checkMetadata.py`, the item names of `locations/items.py` and `locations/constants.py`, and every `locations/` class used by those files, as far as logic reads it: constructor data, `nameId`, `OPTIONS`, `configure`, `getOptions` and `metadata`.
- Not ported (tier 2 or 3, refused through `ladx_settings_supported`): `DungeonDiveOverworld`, `DungeonChain`, `ALttP`, `mapgen`, `dungeonChain.construct`, `NoDungeonN`, the shopsanity branches (`_createShopSanity` returns none on tier 1), the `randomstartlocation` and `entranceshuffle` branches, and every non-identity entrance mapping.
- Deferred, owner-level decision taken here: `WorldSetup.randomize` goes to step 4, not this step. It draws from `rnd`, and step 4's claim is the `ws` lines and the RNG trace of `run.txt` for every case. Porting it here would claim it only against `logic_vectors.txt`'s single fixed seed. In this step, the world setup comes from the fixture's `ws` lines.
- Deferred: each location kind's `patch` and `read` (ROM writes and reads) go to steps 6.11 (`items`) and 6.19 (`spoiler`). The kinds keep the constructor data those steps will need.

### Modules (all in `ladx/`, pure C99, no allocation, `-ffp-contract=off` as every LADX build)

- **Items** (public header `ladx_items.h`): an enum of every item name used by LADXR's logic, `locations/` option lists and `itempool.py`, in `locations/items.py`'s order first, then the remaining names (logic-only flags such as `D3_BOMBWALL`, the `KEY<n>`/`NIGHTMARE_KEY<n>`/`STONE_BEAK<n>`/`MAP<n>`/`COMPASS<n>`/`INSTRUMENT<n>` families, `RUPEES`) in first-use order. `ladx_item_name(id)` and `ladx_item_find(name)` (none when unknown). Step 4 reuses it for the pool.
- **Requirements** (public header `ladx_requirements.h`, so that their rules are tested through it and step 4 can test inventories): an arena of nodes, embedded in `LadxLogic`. A node is `AND` or `OR` (an item list and a child list, each in stored order), `COUNT(item, n)`, `COUNTS(n, items...)` or `FOUND(item, n)`. A requirement reference is tagged: none, true, false, an item, or a node index. Two node indexes are the same node exactly when LADXR's objects are the same object. The constructors take a term list and apply LADXR's `__new__` rules, including `None` terms kept in `AND` (dropped from the lists) and the `OR` with no items and no children that LADXR asserts against (a build error here). `add` appends unless already present (by name for items, by identity for children); `remove` deletes the first occurrence. `test` and `getItems` follow LADXR, short-circuits included.
- **Inventory** (in `ladx_requirements.h`): `LadxInventory`, per-item counts (`int`, rupees exceed 255) and presence bits; `ladx_inventory_add(inv, item, count)` is LADXR's `Explorer.addItem` (`RUPEES_<n>` adds `n × count` to `RUPEES`; an item is present once added, even with 0); requirement `test` and `getItems` take an inventory. The explorer embeds one.
- **`FOUND` and `_USED`**: `FOUND` tests `count(item) + count(item_USED)`. At the pin no LADXR code adds a `_USED` key (only `requirements.py:174` mentions it), so the item table has no `_USED` names and the port reads 0 for them. `FOUND` stays a distinct node kind because it is rendered `found(...)`.
- **Graph** (public header `ladx_logic.h`): a caller-owned `LadxLogic` struct of fixed arrays (locations, edges, spots, option suffixes, requirement nodes and their term pool), with its `sizeof` documented. Public read-only plain structs, as `LadxSettings` is:
  - a location: its dungeon (-1 for none), its name (NULL for none) and its edge range;
  - an edge: its target index and requirement reference;
  - a spot: its location, its kind (an enum named after LADXR's class, with `ladx_spot_kind_name`), its name id text, its options (the kind's static base list or replacement list, plus a per-spot suffix of appended dungeon items, never a copy of the base list: copies would need 20,444 entries in the worst configuration), its metadata entry, its kind-specific data, and `item` (`LADX_ITEM_NONE` after the build, set by step 4's fill);
  - accessors for the start, egg, nightmare and windfish locations, and for the nodes (kind, items, children, amount).

  Locations are numbered in `location_list` order and spots in `iteminfo_list` order once the build returns.
- **Builder** (internal to the library): `Location` creation, `add`, `connect` (one or several targets, LADXR's `UNSET` defaults) and `singleConnect` (merge into `OR(new, old)`, none when either is none, skip `False`, `True` as none). An error flag stays set on the first capacity overflow or LADXR assertion, and every later builder call is a no-op, so the port does not need a check after every line. Discovery is LADXR's `__recursiveFindAll` preorder, run on an explicit stack bounded by the location capacity, then `configure` on every spot in `iteminfo_list` order.
- **Location kinds** (internal): one constructor per class, with LADXR's `nameId` rules, `OPTIONS` and `configure` (the base `ItemInfo.configure` copy-and-append, and the overrides that replace the list or skip `super`). The metadata is looked up at construction from the name id at that moment. Trap: `KeyLocation` sets its option after `ItemInfo.__init__`, so its metadata is the `"None"` entry ("Unset Room") while its rendered name id is its key.
- **Metadata** (internal table, read through the spot accessor): `checkMetadata.py`'s 288 entries (name id, name, area), in its order, with LADXR's `"None"` fallback. These are LADXR's own English strings, not ROM bytes.
- **`RequirementsSettings`** (internal): every named requirement and the enemy, boss and miniboss tables, built in LADXR's order from the five `logic` values and the settings it reads (`tradequest`, `hardmode`, `bowwow`, `hpmode`, `superweapons`, `nagmessages`, `boomerang` …), with its late `remove` calls on shared nodes.
- **Dungeons and overworld** (internal, one source per LADXR file): `DungeonN(options, world_setup, r)` returning its entrance, and `World`, transcribed line for line in LADXR's order. Python's `bool` arithmetic in requirement arguments (`&`, `|` on settings comparisons) becomes C booleans passed as true or false terms.
- **`Logic` assembly**: World, then the nine dungeons through `updateIndoorLocation` in LADXR's order, then the entrance loop over the identity mapping in `WorldSetup.__init__`'s order (every `ENTRANCE_INFO` key to its `:inside`, then every `:inside` back), with the empty-target cycle walk and the enter and exit requirement connections, then the egg requirement, then the removed dungeon items, then discovery and `configure`. The `ENTRANCE_INFO` key order (104 keys) becomes a library table.
- **World setup** (in `ladx_logic.h`): `LadxWorldSetup` with `boss_mapping[9]`, the miniboss mapping (12 keys in `WorldSetup.__init__`'s order, values a miniboss enum of the 12 names), the goal (an enum: vanilla, instruments, open, seashells, maze, specific, with the specific instruments' digit string) and `goal_count` (any int, -1 included). `ladx_world_setup_defaults` equals `WorldSetup()`. Step 4 adds the other fields and `randomize`.
- **Build call**: `ladx_logic_build(LadxLogic *out, const LadxSettings *validated, const LadxWorldSetup *ws)` returns OK, UNSUPPORTED (`ladx_settings_supported` false; nothing built), or BROKEN (a capacity overflow or a LADXR assertion; the struct is unusable). It draws no random number: LADXR's `Logic` draws none (`extract_logic.py` checks this).
- **Dungeon build**: `ladx_logic_build_dungeon(out, validated, ws, n)`, `n` from 0 (colour) to 8: `RequirementsSettings`, then that dungeon alone with its entrance as the start, then discovery and `configure`. It exists for the tests' per-dungeon claims and for narrowing a divergence. Its graph is not LADXR's whole graph and is documented as such.
- **Explorer** (in `ladx_logic.h`): a caller-owned `LadxExplorer` (per-item counts and presence, the visited set over location indexes, the todo list bounded by the edge capacity). `ladx_explorer_init`, `ladx_explorer_add_item(e, item, count)` (LADXR's `addItem`, `RUPEES_<n>` folding included), `ladx_explorer_visit(e, logic, location)` (LADXR's `_visit` plus `_process` until no progress, the same todo order), `ladx_explorer_reached(e, location)`, and `ladx_explorer_required_next(e, logic, out set)`. No speed-up in this step: a faster fixpoint is step 10's question, and only if it keeps every result step 4 relies on.
- **Capacities**: header constants with headroom over LADXR's measured maxima across the 425 configurations (689 locations, 348 spots, 1,400 edges, 3,462 requirement nodes created, at most 98 options on one spot, an appended suffix of at most 55). The implementer fixes the exact numbers; the test prints the maxima reached.
- **Settings**: the build reads `LadxSettings` values through `ladx_settings.h`'s ids and value names. A comparison against a value name is resolved once through `ladx_setting_value_find`, never through a hard-coded index.

### Test harness (in `tests/ladx/unit/`, not the library)

- A fixture support module (`logic_fixture`), linked only into `test_logic`. It contains:
  - a reader for `logic_vectors.txt` (`config`, 13 `ws` lines, `graph`, 6 `explore` lines per configuration) and `logic_graphs.txt` (`graph <name>`, then `loc`/`spot`/`edge` lines);
  - the canonical text renderer, which writes `loc`, `spot` and `edge` lines exactly as `ladx_oracle.graph_lines` and `requirement_text` do, through the public accessors;
  - the `ws` parser and renderer;
  - the `index_ranges` renderer;
  - the dungeon walk compare.
- The canonical text stays out of the library: only the tests need it, and the spoiler (step 6.19) has its own format.
- `ws` handling: the four logic fields are parsed into `LadxWorldSetup`. `entrance_mapping identity`, `bingo_goals -`, `map -` and `dungeon_chain -` are required. `multichest`, `sign_maze`, `inside_to_outside`, `keep_two_way` and `one_on_one` are read but not modelled (logic does not read them; step 4 claims them). Any other field or order is a malformed fixture.
- The config's settings are applied with `ladx_settings_set` on defaults (`""` written as the empty value, as `extract_logic.py` does), then validated, and must be supported.
- Mismatch files: on a graph mismatch, C's canonical text goes to `tests/ladx/build/logic/<config>.txt` (`/` in the name replaced by `+`). The failure line names that file and the command `python3 tests/ladx/oracle/extract_logic.py --graph '<config>' --out tmp/ladx-logic tests/ladx/oracle/ladxr`.
- Makefile: `tests/ladx/Makefile` gets `LOGIC_TESTS := test_logic`, a `logic` target running each with `--fixtures` from `tests/ladx` and ending with `logic: <n> LADX unit tests replayed the oracle fixtures`, a link rule giving `test_logic` the fixture support object, and `unit-arm` running `LOGIC_TESTS` again with `--fixtures` (support objects compiled with the rest). The root `logic` target runs `tests/alttp`'s then `tests/ladx`'s. Root help and `CLAUDE.md`'s `make logic` line say both games.
- `tests/ladx/unit/support.h` gains `test_fixtures_mode`, as ALttP's.

## Implementation Chunks

Each chunk is one TDD cycle that ends on a claim, and keeps `make test`, `make lint` and `make logic` green. A chunk adds its test claims, its README lines and its `UNIT_TESTS`/`LOGIC_TESTS` registration together.

1. **Items and requirements.** The item table and lookup, the requirement arena with `AND`/`OR` folding, `COUNT`/`COUNTS`/`FOUND`, `add`/`remove` with shared identity, `test`, `getItems`, and `LadxInventory` (presence, `RUPEES_<n>` folding) through `ladx_items.h` and `ladx_requirements.h`. New `test_requirements` (in `UNIT_TESTS`), synthetic only. Claims: every folding rule of `__new__` (true/false/none mixes, the empty `AND`), stored order with duplicates, `remove` seen through two edges sharing a node, presence after a zero-count add, rupee folding, `FOUND` and `COUNTS` thresholds, and `getItems` adding nothing for a satisfied subtree. Each rule has a planted mutation that fails it, listed in the commit message.
2. **Graph core, explorer and the fixture harness.** Locations, `connect`/`singleConnect`, discovery on the explicit stack, the explorer, the `LadxLogic` and `LadxExplorer` public structs and accessors, `LadxWorldSetup` and its defaults, and `test_logic` with `logic_fixture`, wired into `make logic` (root and `tests/ladx`) and `unit-arm`. Claims:
   - synthetic graphs: merge into `OR(new, old)`, the none-wins merge, a false edge skipped, back requirement identity, a discovery order hand-derived from LADXR's algorithm, the explorer's reached set and required items, and an item placed on a spot picked up on visit;
   - the renderer on a hand-built graph equal to a literal text;
   - with `--fixtures`: all 425 configurations read, each config's settings parsed, validated and supported, and every `ws` line round-tripped through `LadxWorldSetup`.

   `make logic` prints the LADX line.
3. **Location kinds and metadata.** Every class appearing in LADXR's tier-1 logic, with its constructor data, `nameId`, base options and `configure`, and the metadata table. Claims:
   - every `spot` line of `logic_graphs.txt` (1,334): its name id's metadata in the C table equals the line's name and area, with `KeyLocation`'s "Unset Room" trap included;
   - synthetic `configure` tests per kind, against expectations read from LADXR's source and cited in the test: a `DungeonChest` in a dungeon under default, `keysanity` and `removed` dungeon items; `HeartPiece`, `Seashell`, `HeartContainer`, `Witch`, `Toadstool`, `Instrument`, `KeyHole`, `TradeSequenceItem`, `BoomerangGuy`, `StartItem`, `ShopItem` and `OwlStatue` under the settings their `configure` reads.
4. **`RequirementsSettings` and dungeons 1 to 4.** `ladx_logic_build_dungeon` for D1 to D4. Claim, in `make test`: for each of the five baselines of `logic_graphs.txt`, each of D1 to D4 built alone equals the fixture's dungeon. Equality is a walk from the location named `D<n> Entrance` in both, preorder, following in order only the edges whose target has dungeon `n`, and pairing locations by walk position. Each paired location's name, dungeon and spots (kind, name id, options, metadata) must be equal, and so must its in-dungeon edges (target walk position and requirement text). Edges leaving the dungeon are skipped, because `Logic`'s entrance loop adds them. A mismatch prints the dungeon, the baseline and the first differing location.
5. **Dungeons 5 to 8 and the colour dungeon.** The same claim for D5 to D8 and D0. All nine dungeons now hold on the five baselines.
6. **Overworld and `Logic` assembly.** `World`, `EntranceExterior`, the `ENTRANCE_INFO` order table, the identity entrance loop, the egg requirement, the removed dungeon items, and `ladx_logic_build`. Claims, in `make test`: the five baselines' canonical text line-identical to `logic_graphs.txt`, and a supported-settings refusal of an unsupported set (UNSUPPORTED, nothing built). A planted swap of two `connect` lines in `World` fails the claim, recorded in the commit message.
7. **Every configuration and the explorer vectors.** Claims, under `make logic`: all 425 configurations' graph SHA-512 (computed with `ladx_sha512` over the rendered text) equal to their `graph` lines, and all 2,550 `explore` lines equal (reached ranges and required items). The test prints, among others, `test_logic: 425 configurations, graphs equal, 2550 explore vectors equal` and the maxima line. `make unit-arm` runs the replay under qemu-arm. `tests/ladx/README.md` and `CLAUDE.md` are final, and the step note records `make test`'s and `make logic`'s times before and after.

## Testing Decisions

- A good test drives the library through its public headers only (`ladx_items.h`, `ladx_requirements.h`, `ladx_logic.h`, `ladx_settings.h`) and checks LADXR's observable results: the graph's order, texts and requirement structure, and the explorer's sets. Internal layout (arena indexes, term pool) is never asserted.
- The claims are the oracle's fixtures, never hand-edited: `logic_graphs.txt` (five baselines, line by line, plus per-dungeon walks) and `logic_vectors.txt` (425 SHA-512 digests, 2,550 explorer vectors). A fixture that seems wrong is fixed at the oracle (`extract_logic.py`, regenerated by `gen_fixtures.sh` in its own commit, with `gen_fixtures.sh --check`), never in the test (§2).
- Hand-written synthetic tests cover what the fixtures cannot isolate: folding rules, `remove` on shared nodes, edge merging, a placed item picked up, and each kind's `configure`. Each expectation cites the LADXR line it comes from.
- Tested modules: items and requirements (`test_requirements`, `make test` only), and graph, kinds, metadata, dungeons, overworld, assembly and explorer (`test_logic`: synthetic tests, the metadata claim, the dungeon walks and the five baselines without a flag, in `make test`; the 425-configuration replay with `--fixtures`, in `make logic` and `make unit-arm`).
- Every claim is proven able to fail: a planted mutation per claim family (a folding rule, an `OR` merge order, a discovery order, an option list, a dungeon edge, an overworld `connect`, an explorer rupee fold), each listed with its failure line in the commit message, then reverted.
- Prior art:
  - ALttP's `LOGIC_TESTS` with `test_fixtures_mode` and `make logic`;
  - `tests/ladx/unit/test_pyrandom.c` and `test_settings.c`, which replay oracle fixtures line by line and print a count summary;
  - `extract_logic.py`/`ladx_oracle.graph_lines`, which define the canonical text the C renderer must reproduce.
- `make test` stays in seconds. The five baseline graphs and the per-dungeon walks are a few milliseconds of C; the 3.8 MB vectors replay runs only in `make logic`. The step note records the times.

## Out of Scope

- `WorldSetup.randomize`, the item pool and the fill (step 4).
- Each location kind's ROM `patch` and `read` (steps 6.11 and 6.19), hints, and the spoiler.
- Non-normal overworlds, entrance shuffle, random start location, shopsanity, dungeon chain and bingo goals (unsupported or cut; refused by the build).
- `Logic.dumpFlatRequirements`, `copyWithModifiedItemNames` (used only by non-tier-1 paths), and `friendlyName` (the spoiler's step ports it if it needs it).
- Speeding up the explorer (step 10, if the measure asks for it).
- Any change to the oracle or its fixtures. A real fixture defect would be fixed at the oracle in its own commit, as §2 requires, not planned here.
- Any app or SD-card change (Phase 4).

## Further Notes

- Traps found while sizing the port. Each is a candidate for a 40-word comment where the code would otherwise invite the "fix":
  - `KeyLocation`'s metadata is "Unset Room", because `nameId` is read before its option is set.
  - `dungeon_keys=removed` adds 81 key locations (LADXR's unused inner loop).
  - `connect`'s back requirement is the same object as the forward one.
  - `RequirementsSettings` removes items from nodes that are already shared.
  - `AND` keeps `None` arguments as "no requirement" instead of failing.
  - `StartItem.configure` appends to the class list under `randomstartlocation` (not tier 1, not ported).
- `Explorer.getAccessableLocations` returns a Python set. Its iteration order is not part of this step's claims, which sort it. If step 4's fill depends on that order, step 4 settles it against `ladxr.patch` and the goldens.
- Sizes measured with LADXR at the pin over the 425 configurations: at most 689 locations, 348 spots, 1,400 edges, 1,067 requirement nodes reachable from edges, and 3,462 created.
- Expected size: roughly 6,000 to 8,000 lines of C, most of it the line-for-line transcription of the dungeons and the overworld.
- The step note (`### Step 3.3`, written by the plan pilot) should record: the chunk claims' counts, the maxima against the capacities, `sizeof(LadxLogic)` and `sizeof(LadxExplorer)`, the planted mutations, `make test`'s and `make logic`'s times, and `make unit-arm`'s LADX line.
- Decisions taken here without an owner, with their reasons above: `randomize` deferred to step 4; the per-dungeon public build; the canonical renderer kept in the tests; `_USED` not modelled; the baselines in `make test` and the 425 configurations in `make logic`.
