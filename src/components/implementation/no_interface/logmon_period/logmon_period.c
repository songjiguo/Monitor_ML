#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <logmonitor.h>

int high, low;

void cos_init(void)
{
	static int first = 0, flag = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 5;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		if (cos_get_thd_id() == high) {
			timed_event_block(cos_spd_id(), 30);
			int lm_sync_period;
			lm_sync_period = lm_get_sync_period();
			periodic_wake_create(cos_spd_id(), lm_sync_period);
			while(1){
				periodic_wake_wait(cos_spd_id());
				/* lm_sync_process(); */
				lm_process(cos_spd_id());
			}
		}
	}
}
