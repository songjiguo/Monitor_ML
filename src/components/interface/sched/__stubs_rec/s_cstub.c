#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>

int __sg_sched_create_thd(spdid_t spdid, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2)
{
	return sched_create_thd(spdid, sched_param0, sched_param1, sched_param2);
}

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	return sched_wakeup(spdid, thd_id);
}

int __sg_sched_block(spdid_t spdid, unsigned short int dep_thd)
{
	return sched_block(spdid, dep_thd);
}
