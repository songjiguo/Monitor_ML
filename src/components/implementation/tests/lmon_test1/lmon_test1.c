#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <logmonitor.h>
#include <lmon_test2.h>

int high, low;

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i;
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 12;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		if (cos_get_thd_id() == high) {
			printc("thd %d is blocked\n", cos_get_thd_id());
			sched_block(cos_spd_id(),0);
			printc("thd %d is back\n", cos_get_thd_id());

			lmon_test2();
			lmon_test2();

			while(1);
		}
		if (cos_get_thd_id() == low) {
			printc("thd %d wakes thd %d up\n", cos_get_thd_id(), high);
			sched_wakeup(cos_spd_id(),high);
		}

	}

	return;
}
