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

/* #define LOCK_TAKE()   if(sched_component_take(cos_spd_id())) BUG(); */
/* #define LOCK_RELEASE() if(sched_component_release(cos_spd_id())) BUG(); */
/* #define LOCK_INIT() */

volatile int spin = 1;
volatile int kevin, andy, qq;

static void try_crisec_hp(void)
{
	int i;
	while(1) {
		printc("thread h : %d is doing something\n", cos_get_thd_id());
		/* periodic_wake_wait(cos_spd_id()); */

		timed_event_block(cos_spd_id(), 9);
		spin = 0;
		printc("thread h : %d try to take lock\n", cos_get_thd_id());
		LOCK_TAKE();
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

vaddr_t lmon_ser1_test(void)
{
	if (cos_get_thd_id() == hig) try_crisec_hp();
	if (cos_get_thd_id() == mid) try_crisec_mp();
	if (cos_get_thd_id() == low) try_crisec_lp();

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



