#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>
/* #include <timed_blk.h> */
#include <pong_test.h>
/* #include <ping2.h> */
/* #include <pgfault.h> */

//#include <cos_alloc.h>
#include <pingpong_test.h>
#include <valloc.h>

#define NUM 6
#define THREAD1 14
#define THREAD2 15

int high, low;

struct test{
	int a;
	int b;
};


#ifdef SIMPLE_PPONG
/* simple ping pong test */
/* 1. w/ stack, change both Makefile and interface .S */
/* 2. w/ simple stack, change both Makefile and interface .S */
#define ITER 100000
static void
ping_pong_test()
{
	int i, k;
	unsigned long long start, end;
	printc("<<< SIMPLE PPONG TEST BEGIN! >>>\n");

	void *addr;
	addr = valloc_alloc(cos_spd_id(), 9, 1);
	printc("ping: addr %p\n", addr);

	for (k = 0; k < 2; k++){
		rdtscll(start);
		for (i = 0; i<2; i++) {
			pong();
		}
		rdtscll(end);
		printc("avg invocations cost: %llu\n", (end-start)/ITER);
	}
	printc("\n<<< PPONG TEST END! >>>\n");
	return;
}
#endif


#ifdef TEST_INVOCATION_RECOVERY
static void
ping_pong_test()
{
	printc("<<< thd %d TEST_INVOCATION_RECOVERY BEGIN! >>>\n", cos_get_thd_id());
	int i = 0;
	
	if (cos_get_thd_id() == 12) ping2();	
	while(i++ <= 3) pong();
	
	return;
}
#endif
#ifdef TEST_RETURN_RECOVERY
static void
ping_pong_test()
{
	printc("<<< thd %d TEST_RETURN_RECOVERY BEGIN! >>>\n", cos_get_thd_id());
	int i = 0;
	while(i++ <= NUM) pong();
	return;
}
#endif
#ifdef TEST_SWITCH_RECOVERY
static void
ping_pong_test()
{
	printc("<<< thd %d TEST_SWITCH_RECOVERY BEGIN! >>>\n", cos_get_thd_id());
	int i = 0;
	while(i++ <= NUM) {
		if (cos_get_thd_id() == 11) {
			printc("thd %d wait for 10 ticks\n", cos_get_thd_id());
			timed_event_block(cos_spd_id(), 2);
			printc("thd %d wake\n", cos_get_thd_id());
		}
		printc("thread %d calls pong\n", cos_get_thd_id());
		pong();
	}
	return;
}
#endif
#ifdef TEST_INTERRUPT_RECOVERY
static void
ping_pong_test()
{
	printc("<<< thd %d TEST_INTERRUPT_RECOVERY BEGIN! >>>\n", cos_get_thd_id());
	int i = 0;
	if (cos_get_thd_id() == 11) {
		while(1) pong();
	}
	return;
}
#endif

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD2;
		low = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		ping_pong_test();
	}
	
	return;
}
