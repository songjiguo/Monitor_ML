#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <lmon_ser1.h>

int high, low;

#define ITER 5

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i, j;
	
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
			timed_event_block(cos_spd_id(), 50);
			i = 0;
			while(i++ < ITER) lmon_ser1_test();
		}
		if (cos_get_thd_id() == low) {
			timed_event_block(cos_spd_id(), 50);
			j = 0;
			while(j++ < ITER) lmon_ser1_test();
		}
	}

	return;
}
