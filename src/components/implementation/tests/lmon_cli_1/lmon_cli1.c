#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <test_monitor.h>
#include <lmon_ser1.h>

int high, low, med;
int warm;

#define ITER 5

#define RUN 100
#define RUNITER 10000

#define US_PER_TICK 10000

//#define CAS_TEST

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

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 15;
		med = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 20;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
#if defined EXAMINE_PI 
		if (cos_get_thd_id() == high) {
			timed_event_block(cos_spd_id(), 6);
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 13);
			try_cs_hp();
		}
		if (cos_get_thd_id() == med) {
			printc("<<<med thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 21);
			timed_event_block(cos_spd_id(), 3);
			try_cs_mp();
		}

		if (cos_get_thd_id() == low) {
			printc("<<<low thd %d>>>\n", cos_get_thd_id());
			periodic_wake_create(cos_spd_id(), 37);
			try_cs_lp();
		}
#elif defined CAS_TEST
		if (cos_get_thd_id() == high) {
			timed_event_block(cos_spd_id(), 6);
			printc("<<<high thd %d>>>\n", cos_get_thd_id());
			test_iploop();
			return;
		}
#elif defined EXAMINE_SCHED  // 2 threads, bloc/wakeup N time
		if (cos_get_thd_id() == high) {
			try_cs_hp();
		}
		if (cos_get_thd_id() == low) {
			try_cs_lp();
		}
#elif defined EXAMINE_MM  // 1 threads, do alloc/alias/revoke
		if (cos_get_thd_id() == high) {
			try_cs_hp();
		}
#elif defined NORM  // first example, 2 clients and 1 ser1 and 1 ser2
		if (cos_get_thd_id() == high) {
			timed_event_block(cos_spd_id(), 50);
			j = 0;
			while(j++ < ITER) lmon_ser1_test();
		}

		if (cos_get_thd_id() == low) {
			timed_event_block(cos_spd_id(), 50);
			j = 0;
			while(j++ < ITER) lmon_ser1_test();
		}

		if (cos_get_thd_id() == warm) {
			printc("<<<warm thd %d>>>\n", cos_get_thd_id());
			i = 0;
			while(i++ < ITER) lmon_ser1_test();
		}
#endif		
	}

	return;
}
