#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>
#include <valloc.h>

#include <unit_mmrec2.h>
#include <unit_mmrec3.h>

#define TOTAL_AMNT 128

#define PAGE_NUM 10

void *s_addr[PAGE_NUM];
spdid_t spd_root, spd2, spd3;

#define THREAD1 10

static void
revoke_test()
{
	int i;
	printc("<<< REVOKE TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		printc("\nrevoke... # %d\n", i);
		mman_revoke_page(spd_root, (vaddr_t)s_addr[i], 0);
	}
	printc("<<< REVOKE TEST END! >>>\n\n");
	return;
}

static void
alias_test()
{
	int i;
	printc("<<< ALIAS TEST BEGIN! >>>\n");
	vaddr_t d_addr;
	for (i = 0; i<PAGE_NUM; i++) {
		d_addr = mm_test2();
		if ((vaddr_t)d_addr!= mman_alias_page(spd_root, (vaddr_t)s_addr[i], 
							 spd2, (vaddr_t)d_addr)) BUG();
	}
	for (i = 0; i<PAGE_NUM; i++) {
		d_addr = mm_test3();
		if ((vaddr_t)d_addr!= mman_alias_page(spd_root, (vaddr_t)s_addr[i], 
							 spd3, (vaddr_t)d_addr)) BUG();
	}

	printc("<<< ALIAS TEST END! >>>\n\n");
	return;
}


static void
get_test()
{
	int i;
	printc("<<< GET TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = valloc_alloc(spd_root, spd_root, 1);
		if (unlikely(!s_addr[i])) {
			printc("Cannot valloc for comp %d!\n", spd_root);
			BUG();
		}
		if ((vaddr_t)s_addr[i]!= mman_get_page(spd_root, (vaddr_t)s_addr[i], 0)) BUG();
	}
	printc("GET TEST END!\n\n");
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i;

	if(first == 0){
		first = 1;
		
		spd_root = cos_spd_id();
		spd2 = cos_spd_id() + 1;
		spd3 = cos_spd_id() + 2;

		for (i=0; i<PAGE_NUM; i++) s_addr[i] = NULL;
		
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_create_thd(spd_root, sp.v, 0, 0);
	} else {
		printc("<<< MM RECOVERY TEST START >>>\n");

		/* printc("7, 8valloc_alloc %x\n", valloc_alloc(7, 8, 1)); */
		/* printc("7, 9valloc_alloc %x\n", valloc_alloc(7, 9, 1)); */
		/* printc("8, 9valloc_alloc %x\n", valloc_alloc(8, 9, 1)); */

		get_test();
		alias_test();
		revoke_test();

		printc("<<< MM RECOVERY TEST DONE!! >>>\n");
	}
	
	return;
}

void alias_replay(vaddr_t s_addr);

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
		printc("UNIT_MMREC1 upcall: thread %d\n", cos_get_thd_id());
		alias_replay((vaddr_t)arg3); break;
	default:
		cos_init();
	}

	return;
}
