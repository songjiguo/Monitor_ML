#include <cos_component.h>
#include <evt.h>
#include <cstub.h>
#include <print.h>

CSTUB_FN_ARGS_1(long, evt_create, spdid_t, spdid)

CSTUB_ASM_1(evt_create, spdid)

CSTUB_POST
