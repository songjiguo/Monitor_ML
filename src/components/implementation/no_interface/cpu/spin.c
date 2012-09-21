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
#include <pgfault.h>
#include <pong.h>

int high, low;
static int num = 0;

void cos_init(void *arg)
{
	/* printc("\n <<< Testing: thd %d running >>>\n", cos_get_thd_id()); */

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

		/* pong(); */
	} else {
		while (num < 5) {
			if (cos_get_thd_id() == high){
				printc("high %d running and to block\n", cos_get_thd_id());
				printc("..\n");
				sched_block(cos_spd_id(), 0);
			}
			if (cos_get_thd_id() == low){
				num++;
				printc("low %d running and to wake up high %d\n", cos_get_thd_id(), high);
				printc("..\n");
				sched_wakeup(cos_spd_id(), high);
			}
		}
		/* printc("THE ending......thd %d\n", cos_get_thd_id()); */
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
