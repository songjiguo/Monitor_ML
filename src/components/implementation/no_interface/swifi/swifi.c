#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <swifi.h>

int high, low;
unsigned long counter = 0;

/* #define TEST_LOCAL */

//#define TARGET_COMPONENT 21   /*22 in web server case */

//#define TARGET_COMPONENT 14   /* ramfs (turn off some_delay in mm!!!), also have the client just looping */
//#define TARGET_COMPONENT  3   /* mm (turn on some_delay in mm, should not turn on when normal operation) */
//#define TARGET_COMPONENT  2   /* sched */

#ifndef TARGET_COMPONENT   
#define TARGET_COMPONENT 0    	/* no fault injection */
#endif

static int entry_cnt = 0;

int fault_inject()
{
	int ret = 0;
	int tid, spdid;

	entry_cnt++;
	/* printc("\n{\n"); */
	/* printc("thread %d in fault injector %ld (%d) ... TARGET_COMPONENT %d\n", cos_get_thd_id(), cos_spd_id(), entry_cnt, TARGET_COMPONENT); */

	if (TARGET_COMPONENT == 0) return 0;
	
	struct cos_regs r;

	for (tid = 1; tid <= MAX_NUM_THREADS; tid++) {
		spdid = cos_thd_cntl(COS_THD_FIND_SPD_TO_FLIP, tid, TARGET_COMPONENT, 0);
		if (spdid == -1 || tid == 4) continue;
		counter++;
		printc("<<%lu>> flip the register in component %d (tid %d)!!!\n", counter, TARGET_COMPONENT, tid);
		cos_regs_read(tid, spdid, &r);
		/* cos_regs_print(&r); */
		flip_all_regs(&r);
		/* cos_regs_print(&r); */
	}
	/* printc("}\n\n"); */
	return 0;
}


void cos_init(void)
{
	static int first = 0, flag = 0;
	union sched_param sp;
	int rand;
	int num = 0;

	return;  //just not create any thread here
	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 4;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

#ifdef TEST_LOCAL
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
#endif
	} else {
#if (SWIFI_ENABLE == 1)
		if (cos_get_thd_id() == high) {
			printc("\nfault injector %ld (high %d thd %d)\n", cos_spd_id(), high, cos_get_thd_id());
			timed_event_block(cos_spd_id(), 30);
			periodic_wake_create(cos_spd_id(), 5);
			/* periodic_wake_create(cos_spd_id(), 1); */
			/* while(num++ < 500) { */
			while(1){
				periodic_wake_wait(cos_spd_id()); /*  run this first to update the wakeup time */
				if (flag == 1) fault_inject();
				flag = 1;
			}
		}
#ifdef TEST_LOCAL
		if (cos_get_thd_id() == low){
			timed_event_block(cos_spd_id(), 200);
			while(1) {
				sched_block(cos_spd_id(), 99);
			}
		}
#endif
#endif
	}
}
