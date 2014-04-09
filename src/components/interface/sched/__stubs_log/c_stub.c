/* Scheduler log monitor: client stub interface */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>
#include <cstub.h>
#include <print.h>

#include <log.h>

CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)

#ifdef LOG_MONITOR
func_num = FN_SCHED_BLOCK;
para     = thd_id;
#endif

/* printc("cli: sched_block (thd %d)\n", cos_get_thd_id()); */
CSTUB_ASM_2(sched_block, spdid, thd_id)

CSTUB_POST

CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)

#ifdef LOG_MONITOR
func_num = FN_SCHED_WAKEUP;
para     = dep_thd;
#endif

CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)

CSTUB_ASM_1(sched_component_take, spdid)

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)

CSTUB_ASM_1(sched_component_release, spdid)

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_timeout, spdid_t, spdid, unsigned long, amnt)

CSTUB_ASM_2(sched_timeout, spdid, amnt)

CSTUB_POST
