#ifndef RANDO_MEASURE_SHIM_ASSERT_H
#define RANDO_MEASURE_SHIM_ASSERT_H

/* The ARM proxy compiles asserts out, so its code size is a lower bound. */
#define assert(e) ((void)0)

#endif
