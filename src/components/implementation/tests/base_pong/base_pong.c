#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <pingpong_test.h>

#ifdef TEST_RETURN_RECOVERY
void base_pong(void)
{
	printc("thd %d blocked in spd %ld\n", cos_get_thd_id(), cos_spd_id());
	sched_block(cos_spd_id(), 0);
	printc("thd %d wake from spd %ld\n", cos_get_thd_id(), cos_spd_id());
	return;
}
#else
void base_pong(void)
{
	return;
}
#endif

