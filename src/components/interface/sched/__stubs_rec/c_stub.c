/* Scheduler recovery: client stub interface */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched.h>
#include <cstub.h>

#include <print.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

/* /\* global fault counter, only increase, never decrease *\/ */
static unsigned long fcounter;

/* recovery data structure */
struct rec_data_sched {
	int idx;
	u32_t sched_param0;
	u32_t sched_param1;
	u32_t sched_param2;
	
	spdid_t wakeup_spd;

	unsigned long fcnt;
};

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN_ARGS_4(int, sched_create_thd, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)

redo:
       /* printc("thread %d calls << sched_create_thd >>\n", cos_get_thd_id()); */

CSTUB_ASM_4(sched_create_thd, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       /* printc("failed!! now update the cap flt.cnt\n"); */
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
	       sched_param1 = 1; /* test */
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(int, sched_create_thread_default, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)

redo:
       /* printc("<< sched_create_thread_default cli thread required by %d>>\n", cos_get_thd_id()); */

CSTUB_ASM_4(sched_create_thread_default, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)

redo:
	/* printc("thread %d calls << sched_wakeup >>\n",cos_get_thd_id()); */

CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)

redo:

	/* printc("thread %d calls << sched_block >>\n",cos_get_thd_id()); */

CSTUB_ASM_2(sched_block, spdid, thd_id)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)

redo:
	/* printc("thread %d calls << sched_component_take >>\n",cos_get_thd_id()); */

CSTUB_ASM_1(sched_component_take, spdid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)

redo:
	/* printc("thread %d calls << sched_component_release >>\n",cos_get_thd_id()); */

CSTUB_ASM_1(sched_component_release, spdid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }	       
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST



/* CSLAB_CREATE(rdsched, sizeof(struct rec_data_sched)); */
/* CVECT_CREATE_STATIC(rec_sched_vect); */

/* void print_rdsched_info(struct rec_data_sched *rdsched); */

/* static struct rec_data_sched * */
/* rdsched_lookup(int idx) */
/* { return cvect_lookup(&rec_sched_vect, idx); } */

/* static struct rec_data_sched * */
/* rdsched_alloc(void) */
/* { */
/* 	struct rec_data_sched *rdsched; */
/* 	rdsched = cslab_alloc_rdsched(); */

/* 	if (!rdsched) { */
/* 		printc("can not slab alloc\n"); */
/* 		BUG(); */
/* 	} */
/* 	return rdsched; */
/* } */

/* static void */
/* rdsched_dealloc(struct rec_data_sched *rdsched) */
/* { */
/* 	if (!rdsched) { */
/* 		printc("null rdsched\n"); */
/* 		BUG(); */
/* 	} */

/* 	if (cvect_del(&rec_sched_vect, rdsched->idx)) BUG(); */
/* 	cslab_free_rdsched(rdsched); */

/* 	return; */
/* } */

/* static void */
/* rdsched_cons(struct rec_data_sched *rdsched, spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr) */
/* { */
/* 	if (!rdsched) { */
/* 		printc("null rdsched\n"); */
/* 		BUG(); */
/* 	} */
/* 	return; */
/* } */

/* static struct rec_data_sched * */
/* retrieve_rdsched(vaddr_t addr) */
/* { */
/* 	/\* struct rec_data_sched *rdsched, *ret; *\/ */
/* 	return NULL; */
/* } */
