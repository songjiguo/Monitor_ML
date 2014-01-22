#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser2.h>
#include <cstub.h>

#include <stdint.h>
#include <log.h>

CSTUB_FN_0(int, lmon_ser2_test)

int fn_seq = 1;
CSTUB_ASM_0(lmon_ser2_test)

CSTUB_POST

