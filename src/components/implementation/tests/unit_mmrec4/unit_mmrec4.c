#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

vaddr_t mm_test4()
{
	vaddr_t addr;
	addr  = (vaddr_t)cos_get_vas_page();
	if (!addr) BUG();
	return addr;
}

#ifdef CLI_UPCALL_ENABLE
void alias_replay(vaddr_t s_addr);
#endif
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
#ifdef CLI_UPCALL_ENABLE
		/* printc("UNIT_MMREC 4 upcall: thread %d arg3 %x\n", cos_get_thd_id(), (unsigned int)arg3); */
		alias_replay((vaddr_t)arg3);
#endif
		break;
	default:
		break;
	}

	return;
}
