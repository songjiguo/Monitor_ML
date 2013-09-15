#include <cos_component.h>
#include <print.h>

#include <lmon_ser1.h>

#include <ll_log.h>


#ifdef MEAS_WITH_LOG
vaddr_t __sg_lmon_ser1_test(spdid_t spdid, int event_id)
{
	vaddr_t ret = 0;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 11, 0);
#endif
	ret = lmon_ser1_test();
#ifdef LOG_MONITOR
	monevt_enqueue(0, 11, 0);  // the return spd should be popped from stack
#endif

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
