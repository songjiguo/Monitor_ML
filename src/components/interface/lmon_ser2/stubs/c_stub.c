#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser2.h>
#include <cstub.h>

#include <stdint.h>
#include <ll_log.h>

CSTUB_FN_0(int, lmon_ser2_test)

	monevt_enqueue(uc->cap_no);
CSTUB_ASM_0(lmon_ser2_test)
	monevt_enqueue(cos_spd_id());

CSTUB_POST

