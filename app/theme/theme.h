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

typedef struct Theme {
	ThemeScreen screen;
	ThemeFont fonts[THEME_FONTS_MAX];
	int font_count;
} Theme;

typedef struct ThemeError {
	char message[THEME_ERROR_MAX];
} ThemeError;

/* text is NUL-terminated; file only names it in messages. On false, err holds the first
   problem and out's content is unspecified. */
bool theme_parse(Theme *out, const char *text, const char *file, ThemeError *err);

#endif
