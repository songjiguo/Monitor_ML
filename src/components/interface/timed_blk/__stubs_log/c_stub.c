#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <timed_blk.h>
#include <cstub.h>

#include <log.h>

CSTUB_FN_ARGS_2(int, timed_event_block, spdid_t, spdinv, unsigned int, amnt)

#ifdef LOG_MONITOR
monevt_enqueue(uc->cap_no, 1, 0);
#endif

CSTUB_ASM_2(timed_event_block, spdinv, amnt)

#ifdef LOG_MONITOR
monevt_enqueue(cos_spd_id(),1, 0);
#endif

CSTUB_POST


CSTUB_FN_ARGS_2(int, timed_event_wakeup, spdid_t, spdinv, unsigned short int, thd_id)

#ifdef LOG_MONITOR
monevt_enqueue(uc->cap_no,2, 0);
#endif

CSTUB_ASM_2(timed_event_wakeup, spdinv, thd_id)

#ifdef LOG_MONITOR
monevt_enqueue(cos_spd_id(),2, 0);
#endif

CSTUB_POST


