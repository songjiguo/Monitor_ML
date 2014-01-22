#include <cos_component.h>
#include <evt.h>
#include <cstub.h>
#include <print.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

CSTUB_FN_ARGS_1(long, evt_create, spdid_t, spdid)

int fn_seq = 1;
CSTUB_ASM_1(evt_create, spdid)

CSTUB_POST
