#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>
#include <base_pong.h>
#include <pgfault.h>

#include <pingpong_test.h>

static int number = 0;

#ifdef TEST_INVOCATION_RECOVERY
int pong(void)
{
	if (number++ == 3) assert(0);
	return 0;
}
#endif
#ifdef TEST_RETURN_RECOVERY
int pong(void)
{
	if(cos_get_thd_id() == 12) {
		if (number++ == 3) assert(0);
		sched_wakeup(cos_spd_id() + 1, 11);
		return 0;
	}

	while(cos_get_thd_id() == 11) base_pong();
	return 0;
}
#endif
#ifdef TEST_SWITCH_RECOVERY
unsigned long long counter = 0;
int pong(void)
{
	if(cos_get_thd_id() == 12) {
		while(1) {
			counter++;
			if (counter%5000000 == 0) printc("thd %d counter %llu\n", cos_get_thd_id(), counter);
		}
	}
	if (cos_get_thd_id() == 11) {
		if (number++ == 3) assert(0);
	}

	return 0;
}
#endif
#ifdef TEST_INTERRUPT_RECOVERY
/* ... */
#endif

#ifdef SIMPLE_PPONG
void pong(void)
{
	return;
	number = 0;
}
#endif
