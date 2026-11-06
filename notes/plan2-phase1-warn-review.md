# Plan 2, phase 1 — warn-review

Run after step 1.7 on `git diff 2d74d7f..HEAD -- app tests/rando themes
targets Makefile app/package`. No ground-rule violation. Fed to plan-review,
which proposed no plan change (phase 2 changes none of these values; step
9's ui_cli/SDL page check covers 6 and 7). Not acted on.

- [off-checklist] app/theme/theme.h:8: THEME_TABS_MAX kept equal to
  UI_TABS_MAX by a comment alone, with no compile-time check or test.
- [off-checklist] app/theme/theme.h:133: ThemeButtonIndex kept in
  UiHintButton's order by a comment alone while app/sdl/sdl_render.c:533
  indexes `footer.buttons` with a UiHintButton.
- app/main/sdl_main.c:22: SCREEN_W/SCREEN_H (640x480) redefined beside
  THEME_SCREEN_W/THEME_SCREEN_H.
- app/main/sdl_main.c:306: the offscreen pixel masks repeated inline, also
  in app/sdl/sdl_render.c:63 and :643 and sdl_render.h's contract.
- app/sdl/sdl_render.c:8: the font point range 8..64 defined apart from
  theme.c:532's `size` range, and hard-coded a third time in
  app/main/sdl_main.c:190's message.
- app/main/sdl_main.c:221: the grid layout as bare numbers 4 and 2
  (`UI_GRID_LINES_MAX - 4`, `*page + 4`, `*width + 2`), repeated at :224
  and :227.
- [off-checklist] app/sdl/sdl_loop.h: still requires a UiGrid and renders
  it every frame for a presenter that ignores it (it serves only
  `--shot-text`).
- app/core/ui_genflow.c:75: its own text-row builder beside
  app/core/ui_history.c:351/:360's `add_text_row`/`add_text`.
