#ifndef UI_GENFLOW_H
#define UI_GENFLOW_H

#include "rando_name.h"
#include "rando_options.h"
#include "rando_settings.h"
#include "ui_input.h"
#include "ui_screen.h"
#include "ui_strings.h"

#include <stdbool.h>

#define UI_GENFLOW_STATUS_MAX 96

typedef struct UiGenRequest {
	RandoOptions options; /* hints included */
	RandoHeartBeep heart_beep;
	bool spoiler;
} UiGenRequest;

/* Ids and the ROM name only, never a path or a text. */
typedef struct UiGenOutcome {
	bool ok;
	/* Filled on success. */
	char rom[RANDO_NAME_ROM_MAX];
	bool spoiler;
	/* Success, but a SNES list cache could not be removed. */
	bool list_stale;
	/* On failure: the error line, the id filled into its {} (UI_STR_NONE when it has none), the
	 * what-to-do line. */
	UiStrId status, status_arg, advice;
} UiGenOutcome;

/* Synchronous and blocking; must always fill outcome. */
typedef void (*UiGenRunFn)(void *ctx, const UiGenRequest *request, UiGenOutcome *outcome);

typedef enum UiGenFlowPhase {
	UI_GENFLOW_PHASE_IDLE,
	UI_GENFLOW_PHASE_GENERATING,
	UI_GENFLOW_PHASE_DONE
} UiGenFlowPhase;

typedef enum UiGenFlowEvent {
	UI_GENFLOW_NONE,
	UI_GENFLOW_BACK,
	UI_GENFLOW_QUIT
} UiGenFlowEvent;

/* Caller-owned, no allocation. Read only through the functions below. */
typedef struct UiGenFlow {
	UiGenFlowPhase phase;
	bool ran;
	UiGenRequest request;
	UiGenOutcome outcome;
	const UiStrTable *strings;
	UiGenRunFn run;
	void *ctx;
	/* The filled status text the failure screen borrows. */
	char status[UI_GENFLOW_STATUS_MAX];
} UiGenFlow;

/* strings is borrowed for the flow's life (NULL means English); run and ctx too. Phase IDLE. */
void ui_genflow_init(UiGenFlow *flow, const UiStrTable *strings, UiGenRunFn run, void *ctx);

/* Phase GENERATING, the run still pending. Asserts IDLE. */
void ui_genflow_start(UiGenFlow *flow, const UiGenRequest *request);

/* True in GENERATING before the run. The shell renders and presents ui_genflow_screen once, then calls
 * ui_genflow_run: the run blocks, so the Generating screen is what the player sees meanwhile. */
bool ui_genflow_pending(const UiGenFlow *flow);

/* Clears the outcome, calls the run function once, phase DONE. Asserts a pending run. */
void ui_genflow_run(UiGenFlow *flow);

UiGenFlowPhase ui_genflow_phase(const UiGenFlow *flow);

/* In DONE, A and B give BACK (phase IDLE), Menu QUIT; anything else gives NONE and changes nothing. */
UiGenFlowEvent ui_genflow_key(UiGenFlow *flow, UiButton button);

/* Fills out; its texts are borrowed from the flow and its string table. Asserts a phase other than IDLE. */
void ui_genflow_screen(const UiGenFlow *flow, UiScreen *out);

#endif
