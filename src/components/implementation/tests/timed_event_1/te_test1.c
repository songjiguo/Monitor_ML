#include <cos_component.h>
#include <print.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif


#define TEST_TE
//#define TEST_PERIOD

unsigned long long start, end;
int high, low;

void cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i;
	
	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 12;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (cos_get_thd_id() == high) {
#ifdef TEST_TE
			while(1) {
				rdtscll(start);
				timed_event_block(cos_spd_id(), 100);
				rdtscll(end);
				printc("(thd %d)time even blocked for %llu ticks\n", cos_get_thd_id(), (end-start));
			}
#endif
#ifdef TEST_PERIOD
			periodic_wake_create(cos_spd_id(), 100);
			while(1) {
				rdtscll(start);
				periodic_wake_wait(cos_spd_id());
				rdtscll(end);
				printc("time even blocked for %llu ticks\n", (end-start));
			}
#endif
		}
	}
	return;
}

