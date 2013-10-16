#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <cstub.h>


CSTUB_FN_0(int, base_pong)

redo:

printc("<<<thd %d call base_pong>>>\n", cos_get_thd_id());

CSTUB_ASM_0(base_pong)

        if (unlikely (fault)){
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}

		goto redo;
	}

CSTUB_POST
