#include <cos_component.h>
#include <cos_debug.h>

#include <lock.h>
#include <cstub.h>
#include <print.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

CSTUB_FN_ARGS_3(int, lock_component_take, spdid_t, spdid, unsigned long, lock_id, unsigned short int, thd_id)

CSTUB_ASM_3(lock_component_take, spdid, lock_id, thd_id)

CSTUB_POST


CSTUB_FN_ARGS_2(int, lock_component_release, spdid_t, spdid, unsigned long, lock_id)

CSTUB_ASM_2(lock_component_release, spdid, lock_id)

CSTUB_POST

CSTUB_FN_ARGS_1(int, lock_component_alloc, spdid_t, spdid)

CSTUB_ASM_1(lock_component_alloc, spdid)

CSTUB_POST
