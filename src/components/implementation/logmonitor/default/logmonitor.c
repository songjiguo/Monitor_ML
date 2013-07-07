/**
 Jiguo: log monitor for latent fault: tracking all events
 */

#include <cos_synchronization.h>
#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <mem_mgr_large.h>
#include <valloc.h>
#include <sched.h>

#include <logmonitor.h>

static cos_lock_t lm_lock;
#define LOCK() if (lock_take(&lm_lock)) BUG();
#define UNLOCK() if (lock_release(&lm_lock)) BUG();

typedef int uid;

vaddr_t lm_init(spdid_t spdid)
{
	printc("lm_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid);
	return 0;
}
int lm_process(spdid_t spdid)
{
	LOCK();
	printc("lm_process...(thd %d spd %d)\n", cos_get_thd_id(), spdid);
	UNLOCK();
	return 0;
}

void 
cos_init(void *d)
{
	lock_static_init(&lm_lock);
	printc("log monitor starts...(thd %d spd %ld)\n", cos_get_thd_id(), cos_spd_id());

	return;
}
 
