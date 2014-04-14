#include <cos_component.h>
#include <print.h>

#include <lmon_ser1.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

vaddr_t __sg_lmon_ser1_test(spdid_t spdid)
{
	/* printc("lmo_ser interface: spdid %d\n", spdid); */
	vaddr_t ret = 0;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), 0, 0, EVT_SINV);
#endif
	ret = lmon_ser1_test();
#ifdef LOG_MONITOR
        // second par is 0 --> does know where to return
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}

