#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>
#include <lmon_ser2.h>

static int low = 14;
static int mid = 13;
static int hig = 12;

#include <../lmon_cli_1/test_monitor.h>

#include <cos_synchronization.h>
cos_lock_t lock1, lock2;
#define LOCK1_TAKE()    lock_take(&lock1)
#define LOCK1_RELEASE() lock_release(&lock1)
#define LOCK1_INIT()    lock_static_init(&lock1)
#define LOCK2_TAKE()    lock_take(&lock2)
#define LOCK2_RELEASE() lock_release(&lock2)
#define LOCK2_INIT()    lock_static_init(&lock2)


#ifndef EXAMINE_PI
int try_cs_hp(void) { return 0;}
int try_cs_mp(void) { return 0;}
int try_cs_lp(void) { return 0;}
#endif

#ifdef NORMAL
vaddr_t lmon_ser1_test(void)
{
	lmon_ser2_test();
	return 0;
}
#elif defined MEAS_LOG_OVERHEAD

int try_cs_hp(void)
{
	return 0;
}

int try_cs_mp(void)
{
	return 0;
}

int try_cs_lp(void)
{
	return 0;
}

vaddr_t lmon_ser1_test(void)
{
	return 0;
}
#elif defined EXAMINE_PI

volatile int spin = 1;
volatile int spin2 = 1;
unsigned long long start, end, sum;

int try_cs_hp(void)
{
	int i;
	while(1) {
		/* periodic_wake_wait(cos_spd_id()); */
		/* printc("thread h : %d is doing something\n", cos_get_thd_id()); */
		timed_event_block(cos_spd_id(), 5);
		spin2 = 0;
		printc("thread h : %d try to take lock2\n", cos_get_thd_id());
		LOCK2_TAKE();
		printc("thread h : %d has the lock2\n", cos_get_thd_id());
		LOCK2_RELEASE();
		printc("thread h : %d released lock2\n", cos_get_thd_id());
	}
	return 0;
}

int try_cs_mp(void)
{
	int i;
	while(1) {
		/* periodic_wake_wait(cos_spd_id()); */
		/* printc("thread m : %d is doing something\n", cos_get_thd_id()); */
		timed_event_block(cos_spd_id(), 1);

		LOCK2_TAKE();
		printc("thread m : %d has the lock2\n", cos_get_thd_id());
		spin = 0;
		printc("thread m : %d try to take lock1\n", cos_get_thd_id());
		LOCK1_TAKE();
		printc("thread m : %d has the lock1\n", cos_get_thd_id());
		LOCK1_RELEASE();
		printc("thread m : %d released lock1\n", cos_get_thd_id());
		LOCK2_RELEASE();
		printc("thread m : %d released lock2\n", cos_get_thd_id());
	}
	return 0;
}


int try_cs_lp(void)
{
	volatile int jj, kk;
	while (1) {
		/* periodic_wake_wait(cos_spd_id()); */
		printc("<<< thread l : %d is doing something \n", cos_get_thd_id());
		printc("thread l : %d try to take lock1\n", cos_get_thd_id());
		LOCK1_TAKE();
		printc("thread l : %d has the lock1\n", cos_get_thd_id());
		printc("thread l : %d spin\n", cos_get_thd_id());
		spin = 1;
		while (spin);
		spin = 1;
		printc("thread l : %d after spin\n", cos_get_thd_id());
		jj = 0;
		kk = 0;
		while(jj++ < 10000){
			while(kk++ < 100000);
		}
		printc("thread l : %d spin2\n", cos_get_thd_id());
		spin2 = 1;
		while(spin2);
		spin2 = 1;
		printc("thread l : %d after spin2\n", cos_get_thd_id());
 		printc("thread l : %d release lock1\n", cos_get_thd_id());
		LOCK1_RELEASE();
	}
	return 0;
}

vaddr_t lmon_ser1_test(void)
{
	return 0;
}
#elif defined EXAMINE_DEADLINE

volatile int spin = 1;
unsigned long long start, end, sum;

static int blk_num = 0;

static void periodic_doing(int exe_t, int period)  // in ticks
{
	volatile unsigned long i = 0;

	unsigned long exe_cycle;
	exe_cycle = exe_t*sched_cyc_per_tick();

	printc("In spd %ld Thd %d, period %d, execution time %d in %lu cycles\n", cos_spd_id(),cos_get_thd_id(), period, exe_t, exe_cycle);

	while(1) {
		periodic_wake_wait(cos_spd_id());
		printc("thread %d is doing something\n", cos_get_thd_id());
		start = end = sum = 0;
		while(1) {
			rdtscll(start);
			while(i++ < 1000);
			rdtscll(end);
			sum += end-start;
			if (sum >= exe_cycle) break;
		}
		/* while (i++ < exe_cycle); */
		/* if (cos_get_thd_id() == hig && blk_num++ > 10) sched_block(cos_spd_id(), 0); */
		i = 0;		
	}
	return;
}

static void hp_deadline(void)   // c/t  = 2/10
{
	periodic_doing(2,10);
	return;
}

static void mp_deadline(void)    // ct/t 4/22
{
	periodic_doing(4,22);
	return;
}

static void lp_deadline(void)    // ct/t 12/56
{
	periodic_doing(12,56);
	return;
}

vaddr_t lmon_ser1_test(void)
{
	if (cos_get_thd_id() == hig) hp_deadline();
	if (cos_get_thd_id() == mid) mp_deadline();
	if (cos_get_thd_id() == low) lp_deadline();

	return 0;
}
#endif

void 
cos_init(void) 
{
	LOCK1_INIT();
	LOCK2_INIT();
	return;
}

