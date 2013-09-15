#include <cos_component.h>
#include <lock.h>
#include <print.h>

#include <log.h>

extern int lock_component_take(spdid_t spd, unsigned long lock_id, unsigned short int thd_id);
extern int lock_component_release(spdid_t spd, unsigned long lock_id);
extern unsigned long lock_component_alloc(spdid_t spd);

int __sg_lock_component_take(spdid_t spdid, unsigned long lock_id, unsigned short int thd_id)
{
	/* printc("ser lock: take (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 11, 0);
#endif
	ret = lock_component_take(spdid, lock_id, thd_id);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 11, 0);
#endif
	return ret;
}


int __sg_lock_component_release(spdid_t spdid, unsigned long lock_id)
{
	/* printc("ser lock: release (thd %d)\n", cos_get_thd_id()); */
	int ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 12, 0);
#endif
	ret = lock_component_release(spdid, lock_id);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 12, 0);
#endif
	return ret;
}


unsigned long  __sg_lock_component_alloc(spdid_t spdid)
{
	/* printc("ser lock: alloc (thd %d)\n", cos_get_thd_id()); */
	unsigned long ret;
#ifdef LOG_MONITOR
	monevt_enqueue(cos_spd_id(), 13, 0);
#endif
	ret = lock_component_alloc(spdid);
#ifdef LOG_MONITOR
	monevt_enqueue(0, 13, 0);
#endif

	return ret;
}


