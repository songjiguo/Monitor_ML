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

#define TEST_PI
//#define NORMAL

#ifdef NORMAL
// this is the original test function that supports serv1 and serv2
vaddr_t lmon_ser1_test(void)
{
	printc("thd %d\n", cos_get_thd_id());
	lmon_ser2_test();
	return 0;
}
#endif

#ifdef TEST_PI

// Use the following to test PI
// hard code the info in scheduler (sched_switch_thread_target function)

#include <cos_synchronization.h>
cos_lock_t t_lock;
#define LOCK_TAKE()    lock_take(&t_lock)
#define LOCK_RELEASE() lock_release(&t_lock)
#define LOCK_INIT()    lock_static_init(&t_lock);

volatile int spin = 1;
unsigned long long start, end, sum;

static void try_crisec_hp(void)
{
	int i;
	while(1) {
		printc("thread h : %d is doing something\n", cos_get_thd_id());
		/* periodic_wake_wait(cos_spd_id()); */

		timed_event_block(cos_spd_id(), 9);
		spin = 0;
		printc("thread h : %d try to take lock\n", cos_get_thd_id());
		/* rdtscll(start); */
		LOCK_TAKE();
		/* rdtscll(end); */
		/* printc("it takes %llu for high thread to get the lock\n", end-start); */

		printc("thread h : %d has the lock\n", cos_get_thd_id());

		LOCK_RELEASE();
		printc("thread h : %d released lock\n", cos_get_thd_id());
		/* periodic_wake_wait(cos_spd_id()); */
	}
	return;
}

static void try_crisec_mp(void)
{
	volatile int jj, kk;
	while (1) {
		printc("thread m : %d sched_block\n", cos_get_thd_id());
		sched_block(cos_spd_id(), 0);
		printc("thread m : %d is doing something\n", cos_get_thd_id());
		jj = 0;
		kk = 0;
		while(jj++ < 1000000){
			while(kk++ < 1000000);
		}
	}
	return;
}


static void try_crisec_lp(void)
{
	volatile int jj = 0;
	while (1) {
		printc("<<< thread l : %d is doing something \n", cos_get_thd_id());
		printc("thread l : %d try to take lock\n", cos_get_thd_id());
		LOCK_TAKE();
		printc("thread l : %d has the lock\n", cos_get_thd_id());

		printc("thread l : %d spinning\n", cos_get_thd_id());
		while (spin);
		spin = 1;

		printc("thread l : %d is doing something\n", cos_get_thd_id());
		jj = 0;
		/* while(jj++ < 100000); */
		sched_wakeup(cos_spd_id(), mid);
		/* while(1); */
		printc("thread l : %d try to release lock\n", cos_get_thd_id());
		LOCK_RELEASE();
		/* jj = 0; */
		/* while(jj++ < 100000); */
		/* periodic_wake_wait(cos_spd_id()); */
	}
	return;
}

#define US_PER_TICK 10000

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
	/* if (cos_get_thd_id() == hig) try_crisec_hp(); */
	/* if (cos_get_thd_id() == mid) try_crisec_mp(); */
	/* if (cos_get_thd_id() == low) try_crisec_lp(); */

	if (cos_get_thd_id() == hig) hp_deadline();
	if (cos_get_thd_id() == mid) mp_deadline();
	if (cos_get_thd_id() == low) lp_deadline();

	return 0;
}
#endif

void 
cos_init(void) 
{
	printc("thd %d is trying to init lock\n", cos_get_thd_id());
	LOCK_INIT();
	printc("after init LOCK\n");
}



