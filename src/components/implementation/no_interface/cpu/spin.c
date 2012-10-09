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

#define SCHEDULER_TEST  		/* test purpose only */

#ifdef SCHEDULER_TEST
#include <res_spec.h>

int high, low;
static int num = 0;

volatile unsigned long long start, end;

void cos_init(void *arg)
{
	/* printc("\n <<< Testing: thd %d running >>>\n", cos_get_thd_id()); */
	int i = 4;
	static int first = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		num = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 9;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		while (num < 10) {
			if (cos_get_thd_id() == high){
				sched_block(cos_spd_id(), 0);
				/* sched_component_take(cos_spd_id()); */
				/* sched_component_release(cos_spd_id()); */
			}
			
			if (cos_get_thd_id() == low){
				num++;
				/* sched_component_take(cos_spd_id()); */
				sched_wakeup(cos_spd_id(), high);
				/* sched_component_release(cos_spd_id()); */
			}
		}
		
		printc("THE ending......thd %d\n", cos_get_thd_id());
	}
	
	return;

}

#else

int spin_var = 0, other;

void cos_init(void *arg)
{
	printc("\n<<**Running!**>>\n");
	while(1);

	spin_var = *(int*)NULL;
	while (1) if (spin_var) other = 1;
	return;
}

#endif
