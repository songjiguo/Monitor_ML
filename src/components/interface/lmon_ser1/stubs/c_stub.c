#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser1.h>
#include <cstub.h>

#include <stdint.h>
#include <monitor.h>

// FIXME: this rb is per interface, not per component 

unsigned long long start, end;

CSTUB_FN_0(int, lmon_ser1_test)

	if (unlikely(!cli_ring)) {
		if (!(cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id())))) BUG();
	}
//FIXME: check if cli_ring has got correct return
// If not, 1. call the server anyway
//         2. put assert here

// FIXME: use cos_cap_cntl t and add GET_SER to get the ser and move this into log mgr
        /* if(unlikely(!dest_spd)){ */
	/* 	if ((dest_spd = cos_get_dest_by_cap(COS_GET_DEST_BY_CAP, cos_spd_id(), uc->cap_no)) <= 0) BUG(); */
	/* 	printc("dest spd %d\n", dest_spd); */
	/* } */

// FIXME: issue, maybe not need this???
	/* event_id = (uc->invocation_fn & 0xFFFF0000) | (uc->cap_no >> 16); */

        monevt_enqueue(uc->cap_no, INV_CLI1);
CSTUB_ASM_0(lmon_ser1_test)
        monevt_enqueue(cos_spd_id(), INV_CLI2);

CSTUB_POST

