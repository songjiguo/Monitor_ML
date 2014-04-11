#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

#ifdef LOG_MONITOR
#define LOG_LOCK_LOCk
#include <log.h>
#endif

vaddr_t __sg_timed_event_block(spdid_t spdid, unsigned int amnt)
{
	vaddr_t ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SINV);
#endif
	ret = timed_event_block(spdid, amnt);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}

vaddr_t __sg_timed_event_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	vaddr_t ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SINV);
#endif
	ret = timed_event_wakeup(spdid, thd_id);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}
