#include <cos_component.h>
#include <sched.h>
#include <print.h>

#include <log.h>

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	printc("ser: sched_wakeup (thd %d)\n", cos_get_thd_id());
	int ret;
	monevt_enqueue(cos_spd_id());
	ret = sched_wakeup(spdid, thd_id);
	monevt_enqueue(0);

	return ret;
}


int __sg_sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	printc("ser: sched_block (thd %d)\n", cos_get_thd_id());
	int ret;
	monevt_enqueue(cos_spd_id());
	ret = sched_block(spdid, dependency_thd);
	monevt_enqueue(0);

	return ret;
}

int __sg_sched_component_take(spdid_t spdid)
{
	printc("ser: sched_component_take (thd %d)\n", cos_get_thd_id());
	int ret;
	monevt_enqueue(cos_spd_id());
	ret = sched_component_take(spdid);
	monevt_enqueue(0);

	return ret;
}

int __sg_sched_component_release(spdid_t spdid)
{
	printc("ser: sched_component_release (thd %d)\n", cos_get_thd_id());
	int ret;
	monevt_enqueue(cos_spd_id());
	ret = sched_component_release(spdid);
	monevt_enqueue(0);

	return ret;
}
