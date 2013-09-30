/*
 * Jiguo: Now we will have a log manger layer between llbooter and mm
 *
 *  llbooter  --> log_manager  --> mm/sched/booter/....
 *
 *
*/

#ifndef LL_LOG_H
#define LL_LOG_H

#ifndef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); sched_exit();} } while(0)
#endif

#include <cos_component.h>
#include <res_spec.h>

/* static int     log_thd; */
enum { /* hard-coded spd number */
	LLBOOT_LOG   = 2,
	LLBOOT_SCHED = 3,
	LLBOOT_MM    = 4
};

static int     frame_frontier    = 0; /* which physical frames have we used? */

#include "../../sched/cos_sched_sync.h"
#include "../../sched/cos_sched_ds.h"
/* synchronization... */
#define LOCK()   if (cos_sched_lock_take())    BUG();
#define UNLOCK() if (cos_sched_lock_release()) BUG();

static void
log_ret_thd(void) { return; }

/* memory operations... */
static vaddr_t init_hp = 0; 		/* initial heap pointer */

static inline int
__vpage2frame(vaddr_t addr) { return (addr - init_hp) / PAGE_SIZE; }

static vaddr_t
__mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), addr, frame_frontier++)) BUG();
	if (!init_hp) init_hp = addr;
	return addr;
}

static vaddr_t
__mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	int fp;

	assert(init_hp);
	fp = __vpage2frame(s_addr);
	assert(fp >= 0);
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, d_spd, d_addr, fp)) BUG();
	return d_addr;
}


/*******************************/
/*** The base-case scheduler ***/
/*******************************/

#include <sched_hier.h>

int  sched_init()   { return 0; }
extern void parent_sched_exit(void);
void 
sched_exit(void)   
{
	parent_sched_exit();
}

int sched_isroot(void) { return 1; }

int 
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) { BUG(); return 0; }

extern int parent_sched_child_cntl_thd(spdid_t spdid);

int 
sched_child_cntl_thd(spdid_t spdid) 
{ 
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) BUG();
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();

	return 0;
}

int 
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }

#endif //LL_LOG_H

