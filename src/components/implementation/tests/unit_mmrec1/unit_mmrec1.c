#include <cos_component.h>
#include <print.h>
#include <res_spec.h>
#include <sched.h>
#include <mem_mgr.h>
#include <valloc.h>

#include <unit_mmrec2.h>

#define TOTAL_AMNT 128

#define PAGE_NUM 10

void *s_addr[PAGE_NUM], *d_addr[PAGE_NUM];
spdid_t spd_root, spd2, spd3;

#define THREAD1 10
#define THREAD2 11

static void
revoke_test()
{
	int i;
	printc("<<< REVOKE TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		mman_revoke_page(spd_root, (vaddr_t)s_addr[i], 0);
	}
	printc("<<< REVOKE TEST END! >>>\n\n");
	/* printc("revoke_page done\n"); */
	/* printc("thread %d in spd %ld\n\n", cos_get_thd_id(), cos_spd_id()); */
	return;
}

static void
alias_test()
{
	/* unsigned long long t; */
	/* unsigned long val; */
	int i;
	printc("<<< ALIAS TEST BEGIN! >>>\n");
	for (i = 0; i<PAGE_NUM; i++) {
		d_addr[i] = valloc_alloc(spd_root, spd2, 1);
		if (unlikely(!d_addr[i])) {
			printc("Cannot valloc for comp %d!\n", spd2);
			BUG();
		}
		if ((vaddr_t)d_addr[i]!= mman_alias_page(spd_root, (vaddr_t)s_addr[0], 
							 spd2, (vaddr_t)d_addr[i])) BUG();
		/* printc("after alias page\n\n"); */
	}

	printc("<<< ALIAS TEST END! >>>\n\n");
	/* /\* By probability, the next level spd aliases pages *\/ */
	/* for (i = 0; i<PAGE_NUM; i++) { */
	/* 	rdtscll(t); */
	/* 	val = (int) (t & (TOTAL_AMNT-1)); */
	/* 	if (val >= 80)  */
	/* 		mm_rec2((vaddr_t)d_addr[i], 0); /\* continuously alias *\/ */
	/* 	else if (val < 80 && val > 30) */
	/* 		mm_rec2((vaddr_t)d_addr[i], 1); /\* create and alias another page *\/ */
	/* } */

	/* printc("alias_page done\n"); */
	/* printc("thread %d in spd %ld\n\n", cos_get_thd_id(), cos_spd_id()); */
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
	/* printc("root get_page done\n"); */
	/* printc("thread %d in spd %ld\n\n", cos_get_thd_id(), cos_spd_id()); */
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
		/* spd3 = cos_spd_id() + 2; */

		for (i=0; i<PAGE_NUM; i++) s_addr[i] = NULL;
		for (i=0; i<PAGE_NUM; i++) d_addr[i] = NULL;
		
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = THREAD1;
		sched_create_thd(spd_root, sp.v, 0, 0);
	} else {
		printc("<<< PAGE ALIAS TEST START >>>\n");

		get_test();
		alias_test();
		revoke_test();

		printc("<<< PAGE ALIAS TEST DONE!! >>>\n");
	}
	
	return;
}
