/* Scheduler log monitor: client stub interface */

#include <cos_component.h>
#include <cos_debug.h>

#include <lock.h>
#include <cstub.h>
#include <print.h>

#include <log.h>

CSTUB_FN_ARGS_3(int, lock_component_take, spdid_t, spdid, unsigned long, lock_id, unsigned short int, thd_id)

/* printc("cli lock: take (thd %d)\n", cos_get_thd_id()); */
int fn_seq = 1;
CSTUB_ASM_3(lock_component_take, spdid, lock_id, thd_id)

CSTUB_POST


CSTUB_FN_ARGS_2(int, lock_component_release, spdid_t, spdid, unsigned long, lock_id)

/* printc("cli lock: release (thd %d)\n", cos_get_thd_id()); */
int fn_seq = 2;
CSTUB_ASM_2(lock_component_release, spdid, lock_id)

CSTUB_POST

CSTUB_FN_ARGS_1(int, lock_component_alloc, spdid_t, spdid)

/* printc("cli lock: alloc (thd %d)\n", cos_get_thd_id()); */
int fn_seq = 3;
CSTUB_ASM_1(lock_component_alloc, spdid)

CSTUB_POST
