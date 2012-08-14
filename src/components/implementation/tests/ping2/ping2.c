#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>
#include <pong.h>

#define THREAD1 11
#define NUM 10

static void
ping_pong_test()
{
	int i = 0;
	while(i++ <= NUM) {
		if (i == 4) {
			printc("to fail....");
		}
		pong();
	}
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int thdid;
	
	if(first == 0){
		first = 1;

#ifdef SCHEDULER_TEST
		thdid = sched_create_thd(cos_spd_id(), 0);
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_thd_parameter_set(thdid, sp.v, 0, 0);
#else
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
#endif
	} else {
		ping_pong_test();
	}
	
	return;
}
