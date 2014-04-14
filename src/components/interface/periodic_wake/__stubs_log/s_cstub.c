#include <cos_component.h>
#include <periodic_wake.h>
#include <print.h>

#ifdef LOG_MONITOR
#define TEST_LOCK_LOCK
#include <log.h>
#endif

int __sg_periodic_wake_create(spdid_t spdid, unsigned int period)
{
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), 0, 0, EVT_SINV);
#endif
	ret = periodic_wake_create(spdid, period);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, 0, 0, EVT_SRET);
#endif
	return ret;
}


int __sg_periodic_wake_wait(spdid_t spdid)
{
	int ret;
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), 0, 0, EVT_SINV);
#endif
	/* printc("ser: thd %d is calling periodic_wake\n", cos_get_thd_id()); */
	ret = periodic_wake_wait(spdid);
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, 0, 0, EVT_SRET);
#endif

	return ret;
}
