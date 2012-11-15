#include <cos_component.h>
#include <sched.h>
#include <print.h>

/* if we can tell this happened due to a failure in scheduler,
 * we can simply switch to the blocked thread (will catch the
 * fault when context switch), and let the failure notifier to
 * replay block  */

extern int sched_wakeup_helper(spdid_t spdid, unsigned short int thd_id);

int __sg_sched_wakeup(spdid_t spdid, unsigned short int thd_id, int crash_flag)
{
	printc("in scheduler server interface\n");
	if (unlikely(crash_flag)) sched_wakeup_helper(spdid, thd_id);
	return sched_wakeup(spdid, thd_id);
}
