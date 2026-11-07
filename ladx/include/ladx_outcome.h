#ifndef LADX_OUTCOME_H
#define LADX_OUTCOME_H

typedef enum LadxOutcomeKind {
	/* The run stopped at the end of the stages ported so far: LADXR's outcome is not known yet. */
	LADX_OUTCOME_UNFINISHED = 0,
	LADX_OUTCOME_OK,
	LADX_OUTCOME_ERROR, /* LADXR's utils.Error */
	LADX_OUTCOME_CRASH  /* any other exception LADXR raises */
} LadxOutcomeKind;

/* Room for a message built at run time, with its NUL. */
#define LADX_OUTCOME_TEXT_MAX 96

/*
 * type: the exception's type name, CRASH only, else NULL; message: static
 * text or the outcome's own text, NULL when UNFINISHED or OK. A copy of the
 * struct may point into the original's text: read it through a pointer.
 */
typedef struct LadxOutcome {
	LadxOutcomeKind kind;
	const char *type;
	const char *message;
	char text[LADX_OUTCOME_TEXT_MAX];
} LadxOutcome;

typedef enum LadxRandomizeStatus {
	LADX_RANDOMIZE_OK = 0,
	LADX_RANDOMIZE_UNSUPPORTED, /* a value outside tier 1 (goal=bingo*, scope cut goal-bingo) */
	LADX_RANDOMIZE_BROKEN	    /* a capacity overflow or a logic failure: no outcome, a C bug */
} LadxRandomizeStatus;

#endif
