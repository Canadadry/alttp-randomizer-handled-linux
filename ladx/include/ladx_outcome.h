#ifndef LADX_OUTCOME_H
#define LADX_OUTCOME_H

typedef enum LadxOutcomeKind {
	LADX_OUTCOME_UNFINISHED = 0,
	LADX_OUTCOME_OK,
	LADX_OUTCOME_ERROR,
	LADX_OUTCOME_CRASH
} LadxOutcomeKind;

#define LADX_OUTCOME_TEXT_MAX 96

typedef struct LadxOutcome {
	LadxOutcomeKind kind;
	const char *type;
	const char *message;
	char text[LADX_OUTCOME_TEXT_MAX];
} LadxOutcome;

typedef enum LadxRandomizeStatus {
	LADX_RANDOMIZE_OK = 0,
	LADX_RANDOMIZE_UNSUPPORTED,
	LADX_RANDOMIZE_BROKEN
} LadxRandomizeStatus;

#endif
