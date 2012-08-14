/**
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
/*
 * sched_create_default_thread, sched_create_thd, sched_block,
 * sched_wake, sched_component_take, sched_component_release
 * also use mem_mgr w/o valloc to simplify the test (simple stack)
*/
#include <cos_component.h>
#include <print.h>

#include <sched.h>

#ifdef SCHEDULER_TEST
#include <res_spec.h>

int high, low;
static int num = 0;

void cos_init(void *arg)
{
	printc("thd %d running \n", cos_get_thd_id());

	static int first = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		num = 1;

#ifdef SCHEDULER_TEST
		high = sched_create_thd(cos_spd_id(), 0);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_thd_parameter_set(high, sp.v, 0, 0);


		low = sched_create_thd(cos_spd_id(), 0);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_thd_parameter_set(low, sp.v, 0, 0);
#else
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		high = sched_create_thd(cos_spd_id(), 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		low = sched_create_thd(cos_spd_id(), 0);
#endif

	} else {
		while (num < 10) {
			if (cos_get_thd_id() == high){
				num++;
				printc("high %d running and to block\n", cos_get_thd_id());
				sched_block(cos_spd_id(), 0);
			}
			if (cos_get_thd_id() == low){
				printc("low %d running and to wake up high %d\n", cos_get_thd_id(), high);
				sched_wakeup(cos_spd_id(), high);
			}
		}
	}
	
	return;

}

#else

int spin_var = 0, other;

void cos_init(void *arg)
{
//	BUG();
//	spin_var = *(int*)NULL;
	printc("<<**Running!**>>\n");
//	while (1) if (spin_var) other = 1;
	return;
}

#endif
