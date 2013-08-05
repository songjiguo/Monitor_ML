#include <cos_component.h>
#include <sched.h>
#include <print.h>

#include <monitor.h>

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	int ret;
	/* monevt_sched_enqueue(cos_spd_id()); */

	ret = sched_wakeup(spdid, thd_id);

	/* monevt_sched_enqueue(0); */

	return ret;
}


int __sg_sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	int ret;
	/* monevt_sched_enqueue(cos_spd_id()); */

	ret = sched_block(spdid, dependency_thd);

	/* monevt_sched_enqueue(0); */

	return ret;
}
