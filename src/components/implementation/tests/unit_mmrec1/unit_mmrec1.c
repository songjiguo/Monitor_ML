#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>

#include <unit_mmrec2.h>

#define TOTAL_AMNT 128

#define PAGE_NUM 10

vaddr_t s_addr[PAGE_NUM];
vaddr_t d_addr[PAGE_NUM];

#define THREAD1 10

//#define TEN2TEN
#define ONE2TEN

static void
revoke_test()
{
	int i;
	vaddr_t addr;
	printc("<<< REVOKE TEST BEGIN! >>>\n");

#ifdef ONE2TEN
	for (i = 0; i<1; i++) {
#else
	for (i = 0; i<PAGE_NUM; i++) {
#endif
		printc("\nrevoke... # %d\n", i);

		#ifdef TEN2TEN
		addr = s_addr[i];
		#endif
		#ifdef ONE2TEN
		addr = s_addr[0];
		#endif

		mman_revoke_page(cos_spd_id(), addr, 0);
	}
	printc("<<< REVOKE TEST END! >>>\n\n");
	return;
}

static void
alias_test()
{
	int i;
	vaddr_t addr;
	printc("<<< ALIAS TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		d_addr[i] = mm_test2();

		#ifdef TEN2TEN
		addr = s_addr[i];
		#endif
		#ifdef ONE2TEN
		addr = s_addr[0];
		#endif

		if (d_addr[i]!= mman_alias_page(cos_spd_id(), addr, cos_spd_id()+1, d_addr[i])) BUG();
	}
	mm_test2_34();
	
	printc("<<< ALIAS TEST END! >>>\n\n");
	return;
}


static void
get_test()
{
	int i;
	printc("<<< GET TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		s_addr[i] = (vaddr_t)cos_get_vas_page();
		if (unlikely(!s_addr[i])) {
			printc("Cannot get vas for comp %ld!\n", cos_spd_id());
			BUG();
		}
		if (s_addr[i]!= mman_get_page(cos_spd_id(), s_addr[i], 0)) BUG();
	}
	printc("GET TEST END!\n\n");
	return;
}

void 
cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int i, thd_id;

	if(first == 0){
		first = 1;

		for (i=0; i<PAGE_NUM; i++) s_addr[i] = 0;
		for (i=0; i<PAGE_NUM; i++) d_addr[i] = 0;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		printc("<<< MM RECOVERY TEST START >>>\n");

		get_test();
		alias_test();
		revoke_test();

		printc("<<< MM RECOVERY TEST DONE!! >>>\n");
	}
	
	return;
}

#ifdef CLI_UPCALL_ENABLE
void alias_replay(vaddr_t s_addr);
#endif

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_RECOVERY:
#ifdef CLI_UPCALL_ENABLE
		/* printc("UNIT_MMREC 1 upcall: thread %d\n", cos_get_thd_id()); */
		alias_replay((vaddr_t)arg3);
#endif
		break;
	default:
		cos_init();
	}

	return;
}
