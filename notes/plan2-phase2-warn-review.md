# Plan 2, phase 2 — warn-review

Run after step 2.14 on phase 2's diff (from the parent of `01bb42a`, "feat:
add an optional [help] section to the theme format", to the end of the
plan) over `app tests/rando themes data targets Makefile app/package
design`. No ground-rule violation except one deliberate deviation. The
plan was finished, so no plan-review followed; not acted on.

- tests/rando/sdl/goldens.sh:91: re-parses BMP headers inline although
  tests/rando/sdl/theme_compare.py has `decode_bmp`.
- tests/rando/sdl/theme_art.js:18: repeats mockup_render.js's Playwright
  bootstrap, `/opt/pw-browsers` default, file:// route guard and Press
  Start 2P face check (also in mockup_common.sh's `browser_setup`).
- tests/rando/sdl/theme_art.js: a third PNG decoder (`decodePng`) next to
  theme_compare.py's and theme_art.py's.
- tests/rando/sdl/theme_art.py:27: uses the `theme_compare` tool as its
  PNG codec library.
- tests/rando/Makefile:123: the same pkg-config setup check line in four
  rules (123, 135, 141, 154).
- tests/rando/Makefile:79: the shipped theme path `../../themes/alttp`
  spelled separately in DRAW_THEME, MOCKUP_THEME, COMPARE_THEME,
  ART_THEME and PKG_THEME (also Makefile:89, targets/mac/Makefile,
  test_theme.c's ALTTP_DIR, test_sdl_alttp.c's ALTTP).
- tests/rando/sdl/theme_compare.py:224: the 640x480 screen size hard-coded
  apart from goldens.sh:98 and mockup_render.js:32.
- [off-checklist] app/core/ui_strings.h:248: a UI-core constant
  (`UI_STR_HELP_LEN_MAX 58`) tied to one theme's geometry (test_theme
  checks it against alttp's help columns).
- **Deliberate deviation:** tests/rando/sdl/mockup_text.py:1 keeps a
  docstring well over 40 words holding the comparison rules, against
  CLAUDE.md's comment rule: CLAUDE.md points to it as their single home.
