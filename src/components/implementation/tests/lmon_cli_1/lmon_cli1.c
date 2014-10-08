#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <log.h>
#include <lmon_ser1.h>

int high, low, med;
int warm;

#define ITER 5

#define RUN 100
#define RUNITER 10000

#define US_PER_TICK 10000

void test_iploop()
{
	printc("set eip back testing....\n");
	assert(cos_thd_cntl(COS_THD_IP_LFT, med, 0, 0) != -1);
	timed_event_block(cos_spd_id(), 5);
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i, j, k;
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		warm = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		printc("<<<high thd %d>>>\n", high);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		med = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		printc("<<<med thd %d>>>\n", med);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		printc("<<<low thd %d>>>\n", low);

	} else {
/**************************************/
/**************************************/
/**************************************/
/********  PI Overrun Fault*********/
#if defined MON_PI_OVERRUN 
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 5);
			timed_event_block(cos_spd_id(), 3);
			try_cs_hp();
		}
		/* if (cos_get_thd_id() == med) { */
		/* 	printc("<<<med thd %d>>>\n", cos_get_thd_id()); */
		/* 	periodic_wake_create(cos_spd_id(), 8); */
		/* 	timed_event_block(cos_spd_id(), 6); */
		/* 	try_cs_mp(); */
		/* } */

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 13);
			try_cs_lp();
		}
/********  PI Scheduling Fault *********/
#elif defined MON_PI_SCHEDULING
		/* if (cos_get_thd_id() == warm) { */
		/* 	printc("<<<worm thd %d (highest prio)>>>\n", cos_get_thd_id()); */
		/* 	sched_block(cos_spd_id(), 0); */
		/* 	printc("<<<worm thd %d is back...>>>\n", cos_get_thd_id()); */
		/* 	while(1); */
		/* } */
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 5);
			timed_event_block(cos_spd_id(), 10);
			try_cs_hp();
		}
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 8);
			timed_event_block(cos_spd_id(), 6);
			try_cs_mp();
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 13);
			try_cs_lp();
		}
/********  Deadlock Fault*********/
#elif defined MON_DEADLOCK
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			printc("\n[[[[ MON_DEADLOCK Test....]]]]\n");
			periodic_wake_create(cos_spd_id(), 5);
			timed_event_block(cos_spd_id(), 3);
			try_cs_hp();
		}
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 8);
			timed_event_block(cos_spd_id(), 1);
			try_cs_mp();
		}
		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 13);
			try_cs_lp();
		}
/********  Overrun in Scheduler *********/
#elif defined MON_SCHED  // 2 threads, bloc/wakeup N time, random delay in sched
		printc("thread %d is in MON_SCHED_DELAY\n", cos_get_thd_id());
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 5);
			try_cs_hp();
		}
		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 13);
			try_cs_lp();
		}
/********  Overrun in MM *********/
#elif defined MON_MM  // 1 threads, do alloc/alias/revoke
		if (cos_get_thd_id() == high) {
			periodic_wake_create(cos_spd_id(), 5);
			printc("thread %d is in MON_MM\n", cos_get_thd_id());
			try_cs_hp();
		}
#elif defined MON_CAS_TEST
		printc("thread %d is in MON_CAS_TEST\n", cos_get_thd_id());
		if (cos_get_thd_id() == high) {
			timed_event_block(cos_spd_id(), 6);
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			test_iploop();
			return;
		}
#elif defined MON_PPONG
		if (cos_get_thd_id() == high) {
			printc("<<<high thd %d --- PPONG>>>\n", cos_get_thd_id());
			int i = 0;
			unsigned long long start_pp, end_pp;
			
			while(i++ < RUNITER) {
				rdtscll(start_pp);
				try_cs_hp();
				rdtscll(end_pp);
				printc("one invocation in cmon is %llu\n", end_pp-start_pp);
			}
		}
#endif		
	}

	return;
}
