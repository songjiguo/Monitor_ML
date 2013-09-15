#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

#include <log.h>

vaddr_t __sg_timed_event_block(spdid_t spdinv, unsigned int amnt)
{
	vaddr_t ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 11, 0);
#endif
	ret = timed_event_block(spdinv, amnt);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 11, 0);
#endif

	return ret;
}

vaddr_t __sg_timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id)
{
	vaddr_t ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 12, 0);
#endif
	ret = timed_event_wakeup(spdinv, thd_id);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 12, 0);
#endif

	return ret;
}
