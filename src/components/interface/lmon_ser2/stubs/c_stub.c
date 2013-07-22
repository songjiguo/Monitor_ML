#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_ser2.h>
#include <cstub.h>

#include <stdint.h>
#include <ck_ring_cos.h>

#include <logmonitor.h>

unsigned long long start, end;

extern vaddr_t lm_init(spdid_t spdid);

CSTUB_FN_ARGS_1(int, lmon_ser2_test, spdid_t, spdid)

	int event_id;
	/* printc("cli (spd %ld) interface -->\n", cos_spd_id()); */
	if (unlikely(!cli_ring)) {
#ifdef TEST_INIT_RING
		rdtscll(start);
		cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id()));
		rdtscll(end);
		printc("overhead of init shared ring %llu\n", end-start);
#else
		cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id()));
#endif
	}
	assert(cli_ring);
	event_id = (uc->invocation_fn & 0xFFFF0000) | (uc->cap_no >> 16);

#ifdef TEST_ENQU_RING
rdtscll(start);
        monevt_enqueue(event_id, 1);
rdtscll(end);
printc("overhead of enqueue %llu\n", end-start);
#else
        monevt_enqueue(event_id, 1);
#endif

CSTUB_ASM_2(lmon_ser2_test, spdid, event_id)

	/* printc("cli (spd %ld) interface <--\n", cos_spd_id()); */
	monevt_enqueue(event_id, 2);

CSTUB_POST

