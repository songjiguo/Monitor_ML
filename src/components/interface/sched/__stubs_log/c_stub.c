/* Scheduler log monitor: client stub interface */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>
#include <cstub.h>
#include <print.h>

#include <monitor.h>

CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)

        /* monevt_sched_enqueue(uc->cap_no); */

CSTUB_ASM_2(sched_block, spdid, thd_id)

        /* monevt_sched_enqueue(cos_spd_id()); */

CSTUB_POST

CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)

        /* monevt_sched_enqueue(uc->cap_no); */

CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

        /* monevt_sched_enqueue(cos_spd_id()); */

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)

CSTUB_ASM_1(sched_component_take, spdid)

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)

CSTUB_ASM_1(sched_component_release, spdid)

CSTUB_POST
