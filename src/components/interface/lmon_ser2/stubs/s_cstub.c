#include <cos_component.h>
#include <print.h>

#include <lmon_ser2.h>

#include <monitor.h>

extern vaddr_t lm_init(spdid_t spdid);

vaddr_t __sg_lmon_ser2_test(spdid_t spdid, int event_id)
{
	vaddr_t ret = 0;
	/* printc("ser (spd %ld) interface -->\n", cos_spd_id()); */
	if (unlikely(!cli_ring)) {
		cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id()));
		assert(cli_ring);
	}

	monevt_enqueue(cos_spd_id());
	ret = lmon_ser2_test();
	monevt_enqueue(0);

	return ret;
}



