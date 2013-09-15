#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser1.h>
#include <cstub.h>

#include <stdint.h>
#include <ll_log.h>

#ifdef MEAS_WITH_LOG
CSTUB_FN_0(int, lmon_ser1_test)
#ifdef LOG_MONITOR
monevt_enqueue(uc->cap_no,1, 0);
#endif
CSTUB_ASM_0(lmon_ser1_test)
#ifdef LOG_MONITOR
monevt_enqueue(cos_spd_id(),1, 0);
#endif
CSTUB_POST
#endif

#ifdef MEAS_WITHOUT_LOG
CSTUB_FN_0(int, lmon_ser1_test)

CSTUB_ASM_0(lmon_ser1_test)

CSTUB_POST
#endif
