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

//#define MEASU_SCHED_INTERFACE

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure */
struct rec_data_sched {
	int idx;
	
	unsigned int desired_thd;
	unsigned int dep_thd;

	unsigned long fcnt;
};

/* need to track the info over the interface???? */

/* SCHED: the object is the thread itself.... */
/* MM: object is the memory */
/* TOR: object is the torrent */

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN_ARGS_4(int, sched_create_thd, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)

redo:
       /* printc("thread %d calls << sched_create_thd >>\n", cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif

CSTUB_ASM_4(sched_create_thd, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
	       sched_param1 = 1; /* test use only */
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_create_thd): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(int, sched_create_thread_default, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)

redo:
       /* printc("<< sched_create_thread_default cli thread required by %d>>\n", cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif

CSTUB_ASM_4(sched_create_thread_default, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_create_thd_default): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)

redo:
	/* printc("thread %d calls << sched_wakeup >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif
CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_wakeup): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)

redo:

	/* printc("thread %d calls << sched_block >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif
CSTUB_ASM_2(sched_block, spdid, thd_id)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_block): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)

redo:
	/* printc("thread %d calls << sched_component_take >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_take, spdid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_component_take): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)

redo:
	/* printc("thread %d calls << sched_component_release >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE
unsigned long long start, end;
rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_release, spdid)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }	       
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE
	       rdtscll(end);
	       printc("<<< entire cost (sched_component_release): %llu >>>>\n", (end-start));
#endif
       	       goto redo;
       }

CSTUB_POST
