#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <periodic_wake.h>
#include <cstub.h>

#include <log.h>

CSTUB_FN_ARGS_2(int, periodic_wake_create, spdid_t, spdinv, unsigned int, period)

CSTUB_ASM_2(periodic_wake_create, spdinv, period)

CSTUB_POST


CSTUB_FN_ARGS_1(int, periodic_wake_wait, spdid_t, spdinv)

/* printc("cli: thd %d is calling periodic_wake\n", cos_get_thd_id()); */
CSTUB_ASM_1(periodic_wake_wait, spdinv)

CSTUB_POST


