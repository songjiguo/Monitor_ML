#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <lmon_ser1.h>
#include <cstub.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

CSTUB_FN_0(int, lmon_ser1_test)

CSTUB_ASM_0(lmon_ser1_test)

CSTUB_POST


CSTUB_FN_0(int, try_cs_lp)

CSTUB_ASM_0(try_cs_lp)

CSTUB_POST

CSTUB_FN_0(int, try_cs_mp)

CSTUB_ASM_0(try_cs_mp)

CSTUB_POST

CSTUB_FN_0(int, try_cs_hp)

CSTUB_ASM_0(try_cs_hp)

CSTUB_POST
