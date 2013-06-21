#include <cos_component.h>
#include <timed_blk.h>
#include <print.h>

vaddr_t __sg_timed_event_block(spdid_t spdinv, unsigned int amnt)
{
	/* printc("Here is TE ser (spd %ld) interface\n", cos_spd_id()); */
	return timed_event_block(spdinv, amnt);
}

vaddr_t __sg_timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id)
{
	return timed_event_wakeup(spdinv, thd_id);
}
