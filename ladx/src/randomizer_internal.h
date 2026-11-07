#ifndef LADX_RANDOMIZER_INTERNAL_H
#define LADX_RANDOMIZER_INTERNAL_H

#include "ladx_outcome.h"

/* LADXR's KeyError on a dict lookup of item: CRASH, message '<ITEM>'. */
void ladx_outcome_key_error(LadxOutcome *outcome, int item);

#endif
