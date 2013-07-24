#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser1.h>
#include <cstub.h>

#include <stdint.h>
#include <monitor.h>

/* printc("cap no is %p\n", (void *)uc->cap_no); */
/* printc("inv fn addr is %p\n", (void *)uc->invocation_fn); */
/* printc("cap no is %p\n", (void *)(uc->cap_no >> 16)); */
/* printc("inv fn addr is %p\n", (void *) (uc->invocation_fn & 0xFFFF0000)); */
/* printc("event_id %p\n", (void *)mon_evt.event_id); */

unsigned long long start, end;

CSTUB_FN_ARGS_1(int, lmon_ser1_test, spdid_t, spdid)

	int event_id;
	/* printc("cli (spd %ld) interface -->\n", cos_spd_id()); */
	if (unlikely(!cli_ring)) {
		cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id()));
	}
	assert(cli_ring);
	event_id = (uc->invocation_fn & 0xFFFF0000) | (uc->cap_no >> 16);

        monevt_enqueue(event_id, 1);

CSTUB_ASM_2(lmon_ser1_test, spdid, event_id)

	/* printc("cli (spd %ld) interface <--\n", cos_spd_id()); */
	monevt_enqueue(event_id, 2);

CSTUB_POST

