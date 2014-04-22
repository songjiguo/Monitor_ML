#include <cos_component.h>
#include <sched.h>
#include <print.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

int __sg_sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	/* printc("ser: sched_block (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_SCHED_BLOCK, dependency_thd, EVT_SINV);
#endif
	ret = sched_block(spdid, dependency_thd);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, FN_SCHED_BLOCK, dependency_thd, EVT_SRET);
#endif
	return ret;
}

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	/* printc("ser: sched_wakeup (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_SCHED_WAKEUP, thd_id, EVT_SINV);
#endif
	ret = sched_wakeup(spdid, thd_id);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, FN_SCHED_WAKEUP, thd_id, EVT_SRET);
#endif

	return ret;
}

int __sg_sched_component_take(spdid_t spdid)
{
	/* printc("ser: sched_component_take (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), 0, 0, EVT_SINV);
#endif
	ret = sched_component_take(spdid);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}

int __sg_sched_component_release(spdid_t spdid)
{
	/* printc("ser: sched_component_release (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), 0, 0, EVT_SINV);
#endif
	ret = sched_component_release(spdid);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, 0, 0, EVT_SRET);
#endif
	return ret;
}


int __sg_sched_timeout(spdid_t spdid, unsigned long amnt)
{
	/* printc("ser: sched_wakeup (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_SCHED_TIMEOUT, amnt, EVT_SINV);
#endif
	ret = sched_timeout(spdid, amnt);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, FN_SCHED_TIMEOUT, amnt, EVT_SRET);
#endif
	return ret;
}
