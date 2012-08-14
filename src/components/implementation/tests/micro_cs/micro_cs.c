#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <res_spec.h>
#include <micro_pong.h>

/* 
   changes: max_pool
*/

#define ITER 2000

void cos_init(void)
{
	static int flag = 0;
	union sched_param sp;
	int i, thdid;

	if(flag == 0){
		printc("<<< CONTEXT SWITCH MICRO BENCHMARK TEST  >>>\n");
		flag = 1;

#ifdef SCHEDULER_TEST
		thdid = sched_create_thd(cos_spd_id(), 0);
		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_thd_parameter_set(thdid, sp.v, 0, 0);

		thdid = sched_create_thd(cos_spd_id(), 0);
		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_thd_parameter_set(thdid, sp.v, 0, 0);

#else
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
#endif

	} else {
		for(i=0; i<ITER; i++){
			call_cs();
		}
	}

	printc("<<< CONTEXT SWITCH MICRO BENCHMARK TEST DONE >>>\n");

	return;
}

