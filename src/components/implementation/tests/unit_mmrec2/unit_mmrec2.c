/* #include <cos_component.h> */
/* #include <print.h> */
/* #include <res_spec.h> */
/* #include <sched.h> */
/* #include <mem_mgr_large.h> */
/* #include <valloc.h> */

/* #include <unit_mmrec3.h> */

/* void mm_rec2(vaddr_t addr, int flag) */
/* { */
/* 	void *s_addr, *d_addr; */

/* 	s_addr = (void *)addr; */
/* 	d_addr = valloc_alloc(cos_spd_id(), cos_spd_id() + 1, 1); */
/* 	if (unlikely(!d_addr)) { */
/* 		printc("Cannot valloc for comp %ld!\n", cos_spd_id() + 1); */
/* 		BUG(); */
/* 	} */
	
/* 	if (flag) { */
/* 		s_addr = valloc_alloc(cos_spd_id(), cos_spd_id(), 1); */
/* 		if (unlikely(!s_addr)) { */
/* 			printc("Cannot valloc for comp %ld!\n", cos_spd_id()); */
/* 			BUG(); */
/* 		} */
/* 		if ((vaddr_t)s_addr!= mman_get_page(cos_spd_id(), (vaddr_t)s_addr, 0)) BUG(); */
/* 	} */
	
/* 	if ((vaddr_t)d_addr!= mman_alias_page(cos_spd_id(), (vaddr_t)s_addr, */
/* 					      cos_spd_id()+1, (vaddr_t)d_addr)) BUG(); */
	
	
/* 	/\* mm_rec3(); *\/ */

/* 	mman_revoke_page(cos_spd_id(), (vaddr_t)s_addr, 0); */

/* 	return; */
/* } */


#include <cos_component.h>
#include <print.h>
#include <sched.h>


void mm_rec2(vaddr_t addr, int flag)
{
	printc("<<**in rec2**>>\n");
	return;
}

void cos_init(void)
{
	while(1);
}
