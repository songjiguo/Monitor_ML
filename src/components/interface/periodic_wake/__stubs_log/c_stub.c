#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <periodic_wake.h>
#include <cstub.h>

#include <log.h>

CSTUB_FN_ARGS_2(int, periodic_wake_create, spdid_t, spdinv, unsigned int, period)

#ifdef LOG_MONITOR
monevt_enqueue(uc->cap_no, 21, 0);
#endif

CSTUB_ASM_2(periodic_wake_create, spdinv, period)

#ifdef LOG_MONITOR
monevt_enqueue(cos_spd_id(),21, 0);
#endif

CSTUB_POST


CSTUB_FN_ARGS_1(int, periodic_wake_wait, spdid_t, spdinv)

#ifdef LOG_MONITOR
monevt_enqueue(uc->cap_no, 22, 0);
#endif

printc("cli: thd %d is calling periodic_wake\n", cos_get_thd_id());
CSTUB_ASM_1(periodic_wake_wait, spdinv)

#ifdef LOG_MONITOR
monevt_enqueue(cos_spd_id(),22, 0);
#endif

CSTUB_POST


