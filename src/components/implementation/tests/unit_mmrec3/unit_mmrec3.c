#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>
#include <valloc.h>

vaddr_t mm_test3()
{
	vaddr_t addr;
	addr  = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
	if (!addr) BUG();
	return addr;
}

/* void  */
/* cos_init(void) */
/* { */
/* 	while(1); */
/* } */

void alias_replay(vaddr_t s_addr);

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
		printc("UNIT_MMREC3 upcall: thread %d arg3 %x\n", cos_get_thd_id(), (unsigned int)arg3);
		alias_replay((vaddr_t)arg3); break;
	default:
		break;//cos_init();
	}

	return;
}
