#include <cos_component.h>
#include <sched.h>
#include <print.h>

#include <log.h>

int __sg_sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	/* printc("ser: sched_block (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 11, dependency_thd);
#endif
	ret = sched_block(spdid, dependency_thd);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 11, dependency_thd);
#endif

	return ret;
}

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	/* printc("ser: sched_wakeup (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 12, thd_id);
#endif
	ret = sched_wakeup(spdid, thd_id);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 12, thd_id);
#endif

	return ret;
}

int __sg_sched_component_take(spdid_t spdid)
{
	/* printc("ser: sched_component_take (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 13, 0);
#endif
	ret = sched_component_take(spdid);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 13, 0);
#endif

	return ret;
}

int __sg_sched_component_release(spdid_t spdid)
{
	/* printc("ser: sched_component_release (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 14, 0);
#endif
	ret = sched_component_release(spdid);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 14, 0);
#endif
	return ret;
}


int __sg_sched_timeout(spdid_t spdid, unsigned long amnt)
{
	/* printc("ser: sched_wakeup (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 15, 0);
#endif
	ret = sched_timeout(spdid, amnt);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 15, 0);
#endif
	return ret;
}
