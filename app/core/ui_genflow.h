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
	RandoOptions options;
	RandoHeartBeep heart_beep;
	bool spoiler;
} UiGenRequest;

typedef struct UiGenOutcome {
	bool ok;
	char rom[RANDO_NAME_ROM_MAX];
	bool spoiler;
	bool list_stale;
	UiStrId status, status_arg, advice;
} UiGenOutcome;

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

typedef struct UiGenFlow {
	UiGenFlowPhase phase;
	bool ran;
	UiGenRequest request;
	UiGenOutcome outcome;
	const UiStrTable *strings;
	UiGenRunFn run;
	void *ctx;
	char status[UI_GENFLOW_STATUS_MAX];
} UiGenFlow;

void ui_genflow_init(UiGenFlow *flow, const UiStrTable *strings, UiGenRunFn run, void *ctx);

void ui_genflow_start(UiGenFlow *flow, const UiGenRequest *request);

bool ui_genflow_pending(const UiGenFlow *flow);

void ui_genflow_run(UiGenFlow *flow);

UiGenFlowPhase ui_genflow_phase(const UiGenFlow *flow);

UiGenFlowEvent ui_genflow_key(UiGenFlow *flow, UiButton button);

void ui_genflow_screen(const UiGenFlow *flow, UiScreen *out);

#endif
