#include <cos_component.h>
#include <periodic_wake.h>
#include <print.h>

#include <log.h>

int __sg_periodic_wake_create(spdid_t spdinv, unsigned int period)
{
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 211, 0);
#endif
	ret = periodic_wake_create(spdinv, period);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 211, 0);
#endif

	return ret;
}


int __sg_periodic_wake_wait(spdid_t spdinv)
{
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 212, 0);
#endif
	printc("ser: thd %d is calling periodic_wake\n", cos_get_thd_id());
	ret = periodic_wake_wait(spdinv);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 212, 0);
#endif

	return ret;
}
