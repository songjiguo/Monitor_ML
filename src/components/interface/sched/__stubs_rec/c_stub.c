/* Scheduler recovery: client stub interface */

/* SCHED: the object is the thread itself.... */
/* MM: object is the memory */
/* TOR: object is the torrent */

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

//#define MEASU_SCHED_INTERFACE_CREATE
//#define MEASU_SCHED_INTERFACE_DEFAULT
//#define MEASU_SCHED_INTERFACE_WAKEUP
//#define MEASU_SCHED_INTERFACE_BLOCK
//#define MEASU_SCHED_INTERFACE_COM_TAKE
//#define MEASU_SCHED_INTERFACE_COM_RELEASE

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure */
struct rec_data_sched {
	int idx;
	
	unsigned int desired_thd;
	unsigned int dep_thd;

	unsigned long fcnt;
};

int crt_sec_taken = 0;   	/* flag that indicates the critical section for the spd has been taken */
int crt_sec_owner = 0;         /* which thread has actually taken the critical section  */

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN_ARGS_4(int, sched_create_thd, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)
       unsigned long long start, end;
redo:
       /* printc("Inter: thread %d calls << sched_create_thd >>\n", cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_CREATE
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
#ifdef MEASU_SCHED_INTERFACE_CREATE
	       rdtscll(end);
	       printc("<<< entire cost (sched_create_thd): %llu >>>>\n", (end-start));
#endif
	       if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) {
		       printc("take component crt_thd\n");
		       sched_component_take(cos_spd_id());
	       }
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(int, sched_create_thread_default, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)
         unsigned long long start, end;
redo:
       /* printc("Inter: thread %d calls << sched_create_thread_default >>\n", cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_DEFAULT
               rdtscll(start);
#endif

CSTUB_ASM_4(sched_create_thread_default, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       /* printc("Inter:crt_default update cap %d \n", uc->cap_no); */
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE_DEFAULT
	       rdtscll(end);
	       printc("<<< entire cost (sched_create_thd_default): %llu >>>>\n", (end-start));
#endif
	       if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) {
		       printc("take component crt_thd_default\n");
		       sched_component_take(cos_spd_id());
	       }

       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)
        unsigned long long start, end;
redo:
	/* printc("Inter: thread %d calls << sched_wakeup >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_WAKEUP
               rdtscll(start);
#endif
CSTUB_ASM_2(sched_wakeup, spdid, dep_thd)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE_WAKEUP
	       rdtscll(end);
	       printc("<<< entire cost (sched_wakeup): %llu >>>>\n", (end-start));
#endif
	       if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) {
		       printc("take component wakeup\n");
		       sched_component_take(cos_spd_id());
	       }

       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)
        unsigned long long start, end;
redo:
	/* printc("Inter: thread %d calls << sched_block >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_BLOCK
              rdtscll(start);
#endif
CSTUB_ASM_2(sched_block, spdid, thd_id)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE_BLOCK
	       rdtscll(end);
	       printc("<<< entire cost (sched_block): %llu >>>>\n", (end-start));
#endif
	       if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) {
		       printc("take component block\n");
		       sched_component_take(cos_spd_id());
	       }

       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)
       unsigned long long start, end;
redo:
	/* printc("Inter: thread %d calls << sched_component_take >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
           rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_take, spdid)

       if (unlikely (fault)){
	       /* printc("failed!! when component take\n"); */
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
	       rdtscll(end);
	       printc("<<< entire cost (sched_component_take): %llu >>>>\n", (end-start));
#endif
	       if (unlikely(crt_sec_taken)) {
		       /* printc("thread %d testing lock take.... crit is taken by %d\n", cos_get_thd_id(), crt_sec_owner); */
		       assert(crt_sec_owner != cos_get_thd_id());
		       sched_block(cos_spd_id(), crt_sec_owner);
		       /* printc("now thread %d goto redo\n", cos_get_thd_id()); */
	       }
       	       goto redo;
       }

       crt_sec_taken = 1;
       crt_sec_owner = cos_get_thd_id();

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)
      unsigned long long start, end;
redo:
	/* printc("Inter: thread %d calls << sched_component_release >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
               rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_release, spdid)

       if (unlikely (fault)){
	       /* printc("Inter: spd_release update cap %d \n", uc->cap_no); */
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }	       
       	       fcounter++;
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
	       rdtscll(end);
	       printc("<<< entire cost (sched_component_release): %llu >>>>\n", (end-start));
#endif
	       /* if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) sched_component_take(cos_spd_id()); */

	       /* if not, this thread should not take the critical
		* section at the very first place */
	       /* thread that calls to release should have called the
		* component_take and had the critical section */

	       /* goto redo; */
       }

       crt_sec_owner = 0;
       crt_sec_taken = 0;

CSTUB_POST
