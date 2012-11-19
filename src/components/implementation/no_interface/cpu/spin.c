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
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#define SCHEDULER_TEST  		/* test purpose only */

#ifdef SCHEDULER_TEST
#include <res_spec.h>

int high, low;
static int num = 0;

volatile unsigned long long start, end;

void cos_init(void *arg)
{
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
		/* man, I failed here 3 times!!! 99, manually */
		/* low = sched_create_thd(cos_spd_id(), sp.v, 99, 0); */
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		/* sp.c.type = SCHEDP_PRIO; */
		/* sp.c.value = 11; */
		/* sched_create_thd(cos_spd_id(), sp.v, 99, 0); */

		/* sp.c.type = SCHEDP_PRIO; */
		/* sp.c.value = 12; */
		/* sched_create_thd(cos_spd_id(), sp.v, 0, 0); */

		/* sp.c.type = SCHEDP_PRIO; */
		/* sp.c.value = 13; */
		/* sched_create_thd(cos_spd_id(), sp.v, 99, 0); */

	} else {
		printc("\n <<< Testing: thd %d running >>>\n", cos_get_thd_id());
		if (cos_get_thd_id() == high || cos_get_thd_id() == low){
			/* printc("\n <<< Testing: thd %d running >>>\n", cos_get_thd_id()); */
			timed_event_block(cos_spd_id(), 1);
			/* periodic_wake_create(cos_spd_id(), 1); */
			i = 0;
			while (i++ < 600) {
				if (cos_get_thd_id() == high){
					printc("thd %d call block num %d\n", cos_get_thd_id(), num);
					sched_block(cos_spd_id(), 0);
					/* sched_component_take(cos_spd_id()); */
					/* sched_component_release(cos_spd_id()); */
				}
			
				if (cos_get_thd_id() == low){
					num++;
					/* sched_component_take(cos_spd_id()); */
					printc("thd %d call wakeup num %d\n", cos_get_thd_id(), num);
					sched_wakeup(cos_spd_id(), high);
					/* sched_component_release(cos_spd_id()); */
				}
				/* periodic_wake_wait(cos_spd_id()); */
			}
			
			printc("THE ending......thd %d\n", cos_get_thd_id());
		}
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
