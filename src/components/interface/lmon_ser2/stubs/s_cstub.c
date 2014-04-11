#include <cos_component.h>
#include <print.h>

#include <lmon_ser2.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

vaddr_t __sg_lmon_ser2_test(spdid_t spdid, int event_id)
{
	vaddr_t ret = 0;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SINV);
#endif
	ret = lmon_ser2_test();

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}



