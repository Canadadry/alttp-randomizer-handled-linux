#ifndef THEME_H
#define THEME_H

#include <stdbool.h>

#define THEME_FONTS_MAX 8
/* Equals UI_TABS_MAX: this module does not include the core. */
#define THEME_TABS_MAX 4
#define THEME_FILE_MAX 64
#define THEME_NAME_MAX 16
#define THEME_MARKER_TEXT_MAX 8
#define THEME_ERROR_MAX 512
#define THEME_LINE_MAX 255
#define THEME_SCREEN_W 640
#define THEME_SCREEN_H 480

typedef struct ThemeColour {
	unsigned char r, g, b;
} ThemeColour;

typedef enum ThemeMarkerKind {
	THEME_MARKER_NONE,
	THEME_MARKER_TEXT,
	THEME_MARKER_SPRITE
} ThemeMarkerKind;

/* text holds the chars for THEME_MARKER_TEXT, file the .png for THEME_MARKER_SPRITE; the other is "". */
typedef struct ThemeMarker {
	ThemeMarkerKind kind;
	char text[THEME_MARKER_TEXT_MAX];
	char file[THEME_FILE_MAX];
} ThemeMarker;

typedef struct ThemeImage {
	bool present;
	char file[THEME_FILE_MAX];
} ThemeImage;

typedef struct ThemeScreen {
	ThemeColour colour;
	ThemeImage image;
	ThemeMarker cut;
} ThemeScreen;

/* px is 0 when fit is set. */
typedef struct ThemeSize {
	bool fit;
	int px;
} ThemeSize;

typedef struct ThemeCell {
	bool present;
	int w, h;
} ThemeCell;

typedef enum ThemeAnchor {
	THEME_ANCHOR_TOP,
	THEME_ANCHOR_BASELINE
} ThemeAnchor;

typedef struct ThemeFont {
	char name[THEME_NAME_MAX];
	char file[THEME_FILE_MAX];
	ThemeSize size;
	ThemeCell cell;
	ThemeAnchor anchor;
} ThemeFont;

/* Fields an optional section leaves out are zero; present says whether it was given. */
typedef struct ThemeBand {
	bool present;
	int x, y, w, h;
	ThemeColour colour;
	int top_line, bottom_line;
	ThemeColour line_colour;
} ThemeBand;

typedef struct ThemeTabSlot {
	int x, active_x, marker_x;
} ThemeTabSlot;

/* font fields are indexes into Theme.fonts. */
typedef struct ThemeTabs {
	int font;
	int y, right;
	ThemeColour colour, active_colour;
	ThemeMarker active_before, active_after;
	int marker_y;
	ThemeBand band;
	ThemeTabSlot slots[THEME_TABS_MAX];
	int slot_count;
} ThemeTabs;

typedef struct ThemeRule {
	bool present;
	ThemeColour colour;
	int y, thickness, dash, gap, offset, end;
} ThemeRule;

typedef struct ThemeTitle {
	int font;
	int x, y, right;
	ThemeColour colour, alert_colour;
	ThemeRule rule;
} ThemeTitle;

/* Row bands' y is relative to the row's top. */
typedef struct ThemeList {
	int font;
	int top, bottom, row_height, text_y;
	int label_x, label_x_cursor, right, value_right, value_max;
	int label_gap, arrow_left_gap, arrow_right_gap;
	ThemeColour label_colour, value_colour, text_colour, disabled_colour, selected_colour,
		selected_disabled_colour;
	ThemeMarker cursor, cursor_disabled, arrow_left, arrow_right, scroll_up, scroll_down;
	int cursor_x, scroll_x, cursor_y, arrow_y, scroll_y;
	ThemeBand selected, selected_disabled;
} ThemeList;

typedef struct ThemeNotice {
	int font;
	int x, emphasis_x, right, top, row_height, text_y;
	ThemeColour colour, emphasis_colour;
} ThemeNotice;

typedef struct ThemeMessage {
	int font;
	int x, y, right;
	ThemeColour info_colour, error_colour;
} ThemeMessage;

/* In UiHintButton's order: this module does not include the core. */
typedef enum ThemeButtonIndex {
	THEME_BUTTON_LEFT_RIGHT,
	THEME_BUTTON_A,
	THEME_BUTTON_B,
	THEME_BUTTON_X,
	THEME_BUTTON_Y,
	THEME_BUTTON_START,
	THEME_BUTTON_COUNT
} ThemeButtonIndex;

typedef enum ThemeButtonKind {
	THEME_BUTTON_HINT_TEXT,
	THEME_BUTTON_SPRITE
} ThemeButtonKind;

/* file is "" for THEME_BUTTON_HINT_TEXT. */
typedef struct ThemeButton {
	ThemeButtonKind kind;
	char file[THEME_FILE_MAX];
} ThemeButton;

typedef struct ThemeFooter {
	int font;
	int x, y, key_y, right, key_gap, hint_gap;
	ThemeColour label_colour, primary_colour;
	ThemeButton buttons[THEME_BUTTON_COUNT];
	ThemeBand band;
} ThemeFooter;

typedef struct Theme {
	ThemeScreen screen;
	ThemeFont fonts[THEME_FONTS_MAX];
	int font_count;
	ThemeTabs tabs;
	ThemeTitle title;
	ThemeList list;
	ThemeNotice notice;
	ThemeMessage message;
	ThemeFooter footer;
} Theme;

typedef struct ThemeError {
	char message[THEME_ERROR_MAX];
} ThemeError;

/* text is NUL-terminated; file only names it in messages. On false, err holds the first
   problem and out's content is unspecified. */
bool theme_parse(Theme *out, const char *text, const char *file, ThemeError *err);

#define THEME_FILE_BYTES_MAX 16384

/* Reads <dir>/theme.ini (at most THEME_FILE_BYTES_MAX bytes) into a static buffer, so it is not
   reentrant, and parses it; the same contract as theme_parse. */
bool theme_load(Theme *out, const char *dir, ThemeError *err);

/* Whole list rows between list.top and list.bottom. */
int theme_page(const Theme *theme);

/* Whole characters of advance px (>= 1) between list.label_x and list.right; label_x_cursor is
   not used. */
int theme_columns(const Theme *theme, int advance);

#endif
