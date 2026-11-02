#include "ui_genflow.h"

#include <assert.h>
#include <string.h>

void ui_genflow_init(UiGenFlow *flow, const UiStrTable *strings, UiGenRunFn run, void *ctx)
{
	assert(run != NULL);
	memset(flow, 0, sizeof *flow);
	flow->phase = UI_GENFLOW_PHASE_IDLE;
	flow->strings = strings;
	flow->run = run;
	flow->ctx = ctx;
}

void ui_genflow_start(UiGenFlow *flow, const UiGenRequest *request)
{
	assert(flow->phase == UI_GENFLOW_PHASE_IDLE);
	flow->request = *request;
	flow->phase = UI_GENFLOW_PHASE_GENERATING;
	flow->ran = false;
}

bool ui_genflow_pending(const UiGenFlow *flow)
{
	return flow->phase == UI_GENFLOW_PHASE_GENERATING && !flow->ran;
}

static void clear_outcome(UiGenOutcome *outcome)
{
	memset(outcome, 0, sizeof *outcome);
	outcome->status = UI_STR_NONE;
	outcome->status_arg = UI_STR_NONE;
	outcome->advice = UI_STR_NONE;
}

void ui_genflow_run(UiGenFlow *flow)
{
	const char *pattern;

	assert(ui_genflow_pending(flow));
	clear_outcome(&flow->outcome);
	flow->ran = true;
	flow->run(flow->ctx, &flow->request, &flow->outcome);
	flow->outcome.rom[sizeof flow->outcome.rom - 1] = '\0';
	pattern = ui_str(flow->strings, flow->outcome.status);
	if (ui_str_arity(flow->outcome.status) == 1)
		ui_str_fill(pattern, ui_str(flow->strings, flow->outcome.status_arg), flow->status, sizeof flow->status);
	else
		ui_str_fill(pattern, NULL, flow->status, sizeof flow->status);
	flow->phase = UI_GENFLOW_PHASE_DONE;
}

UiGenFlowPhase ui_genflow_phase(const UiGenFlow *flow)
{
	return flow->phase;
}

UiGenFlowEvent ui_genflow_key(UiGenFlow *flow, UiButton button)
{
	if (flow->phase != UI_GENFLOW_PHASE_DONE)
		return UI_GENFLOW_NONE;
	switch (button) {
	case UI_BUTTON_A:
	case UI_BUTTON_B:
		flow->phase = UI_GENFLOW_PHASE_IDLE;
		return UI_GENFLOW_BACK;
	case UI_BUTTON_MENU:
		return UI_GENFLOW_QUIT;
	default:
		return UI_GENFLOW_NONE;
	}
}

static void add_text(UiScreen *out, const char *text)
{
	UiRow row = {UI_ROW_TEXT, NULL, NULL, false};

	row.label = text;
	ui_screen_add_row(out, row);
}

void ui_genflow_screen(const UiGenFlow *flow, UiScreen *out)
{
	const UiStrTable *t = flow->strings;
	const UiGenOutcome *o = &flow->outcome;

	assert(flow->phase != UI_GENFLOW_PHASE_IDLE);
	ui_screen_init(out);
	out->tabs[0] = ui_str(t, UI_STR_TAB_GENERATE);
	out->tabs[1] = ui_str(t, UI_STR_TAB_HISTORY);
	out->tab_count = 2;
	out->active_tab = 0;
	if (flow->phase == UI_GENFLOW_PHASE_GENERATING) {
		out->title = ui_str(t, UI_STR_GENFLOW_GENERATING_TITLE);
		add_text(out, ui_str(t, UI_STR_GENFLOW_WAIT));
		add_text(out, ui_str(t, UI_STR_GENFLOW_FEW_SECONDS));
		return;
	}
	out->footer = ui_str(t, UI_STR_GENFLOW_FOOTER);
	if (o->ok) {
		out->title = ui_str(t, UI_STR_GENFLOW_READY_TITLE);
		add_text(out, ui_str(t, UI_STR_GENFLOW_SAVED_IN));
		add_text(out, o->rom);
		add_text(out, ui_str(t, o->spoiler ? UI_STR_GENFLOW_SPOILER_KEPT : UI_STR_GENFLOW_SPOILER_NOT_KEPT));
		out->message_kind = UI_MESSAGE_INFO;
		out->message = ui_str(t, o->list_stale ? UI_STR_GENFLOW_REFRESH : UI_STR_GENFLOW_PLAY);
		return;
	}
	out->title = ui_str(t, UI_STR_GENFLOW_FAILED_TITLE);
	add_text(out, ui_str(t, o->advice));
	out->message_kind = UI_MESSAGE_ERROR;
	out->message = flow->status;
}
