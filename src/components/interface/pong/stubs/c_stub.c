#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <cstub.h>


static int fault_test = 0;

CSTUB_FN_0(int, pong)

redo:
printc("\n<<<<thd %d in spd %ld call pong >>>>>\n", cos_get_thd_id(), cos_spd_id());
        
CSTUB_ASM_0(pong)

        if (unlikely (fault)){
		/* if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
		/* 	printc("set cap_fault_cnt failed\n"); */
		/* 	BUG(); */
		/* } */
		if (fault_test++ > 3) goto done;
		goto redo;
	}

done:

CSTUB_POST
