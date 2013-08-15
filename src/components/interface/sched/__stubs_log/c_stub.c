/* Scheduler log monitor: client stub interface */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>
#include <cstub.h>
#include <print.h>

#include <log.h>

CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)

printc("cli: sched_block (thd %d)\n", cos_get_thd_id());
        monevt_enqueue(uc->cap_no);
/* printc("cli: sched_block (thd %d)...\n", cos_get_thd_id()); */
CSTUB_ASM_2(sched_block, spdid, thd_id)

        monevt_enqueue(cos_spd_id());

CSTUB_POST

CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)
printc("cli: sched_wake (thd %d)\n", cos_get_thd_id());
        monevt_enqueue(uc->cap_no);

CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

        monevt_enqueue(cos_spd_id());

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)
printc("cli: sched_component_take (thd %d)\n", cos_get_thd_id());
        monevt_enqueue(uc->cap_no);
/* printc("cli: sched_component_take (thd %d) ....\n", cos_get_thd_id()); */

CSTUB_ASM_1(sched_component_take, spdid)

        monevt_enqueue(cos_spd_id());

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)
printc("cli: sched_component_release (thd %d)\n", cos_get_thd_id());
        monevt_enqueue(uc->cap_no);

CSTUB_ASM_1(sched_component_release, spdid)

        monevt_enqueue(cos_spd_id());

CSTUB_POST
