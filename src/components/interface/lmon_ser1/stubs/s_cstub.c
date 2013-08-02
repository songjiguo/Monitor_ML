#include <cos_component.h>
#include <print.h>

#include <lmon_ser1.h>

#include <monitor.h>

extern vaddr_t lm_init(spdid_t spdid);

#ifdef MEAS_WITH_LOG
vaddr_t __sg_lmon_ser1_test(spdid_t spdid, int event_id)
{
	vaddr_t ret = 0;

	if (unlikely(!cli_ring)) {
		cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(lm_init(cos_spd_id()));
		assert(cli_ring);
	}

	monevt_enqueue(cos_spd_id());

	ret = lmon_ser1_test();

	monevt_enqueue(0);  // the return spd should be popped from stack

	return ret;
}
#endif


#ifdef MEAS_WITHOUT_LOG
vaddr_t __sg_lmon_ser1_test(spdid_t spdid, int event_id)
{
	vaddr_t ret = 0;
	ret = lmon_ser1_test();
	return ret;
}
#endif
