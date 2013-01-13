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
int crt_sec_taken = 0;   	/* flag that indicates the critical section for the spd has been taken */
int crt_sec_owner = 0;         /* which thread has actually taken the critical section  */

unsigned int timer_thd = 0;
unsigned long wakeup_time = 0;
unsigned long long ticks = 0;   /* track the current ticks, need? */

/************************************/
/******interface tracking data ******/
/************************************/
/* 
   recovery data structure, mainly for block/wakeup for created
   threads, they should be already taken cared from the booter
   interface and kernel introspection (not used. since thread
   block/wakeup is taken care already by the priority)
 */

struct blked_thd {
	unsigned int thd_id;
	unsigned int dep_id;;
	struct blked_thd *prev, *next;
};

struct blked_thd *blked_thd_list = NULL;

static struct blked_thd *
blked_thd_init()
{
	return NULL;
}

static struct blked_thd *
blked_thd_add()
{
	return NULL;
}

static struct blked_thd *
blked_thd_rem()
{
	return NULL;
}

static struct blked_thd *
blked_thd_lookup()
{
	return NULL;
}

static void rebuild_net_brand() {
	unsigned short int net_bid = 0;
	unsigned short int net_tid = 0;

	net_tid = cos_brand_cntl(COS_BRAND_INTRO_TID, 0, 0, 0);
	if (likely(net_tid)){
		printc("net_tid %d\n", net_tid);
		if (cos_brand_cntl(COS_BRAND_INTRO_STATUS, 0, 0, 0)) {
			net_bid = cos_brand_cntl(COS_BRAND_INTRO_BID, 0, 0, 0);
			assert(net_bid && net_tid);
			printc("net_bid %d\n", net_bid);
			printc("rebuild the net brand\n");
			if (sched_add_thd_to_brand(cos_spd_id(), net_bid, net_tid)) BUG();
			/* cos_switch_thread(net_tid, 0); */
		}
	}

	return;
}

/************************************/
/******  client stub functions ******/
/************************************/

CSTUB_FN_ARGS_4(int, sched_create_thd, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)
       unsigned long long start, end;
redo:
       /* printc("thread %d calls << sched_create_thd >>\n", cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_CREATE
rdtscll(start);
#endif

CSTUB_ASM_4(sched_create_thd, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;
	       sched_param1 = 0; /* test use only, set 99 to trigger and set 0 to reset */
#ifdef MEASU_SCHED_INTERFACE_CREATE
	       rdtscll(end);
	       printc("<<< entire cost (sched_create_thd): %llu >>>>\n", (end-start));
#endif
	       if (crt_sec_taken && crt_sec_owner == cos_get_thd_id()) {
		       printc("take component crt_thd\n");
		       sched_component_take(cos_spd_id());
	       }

	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(int, sched_create_thread_default, spdid_t, spdid, u32_t, sched_param0, u32_t, sched_param1, unsigned int, desired_thd)
         unsigned long long start, end;
redo:
/* printc("\n<< sched_create_thread_default cli thread required by %d (desired tid %d)>>\n", cos_get_thd_id(), desired_thd); */
#ifdef MEASU_SCHED_INTERFACE_DEFAULT
               rdtscll(start);
#endif

CSTUB_ASM_4(sched_create_thread_default, spdid, sched_param0, sched_param1, desired_thd)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

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

	       desired_thd = 0;
	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_wakeup, spdid_t, spdid, unsigned short int, dep_thd)
        unsigned long long start, end;

        int crash_flag = 0;
redo:
/* printc("thread %d calls << sched_wakeup thd %d>>\n",cos_get_thd_id(),dep_thd); */
#ifdef MEASU_SCHED_INTERFACE_WAKEUP
               rdtscll(start);
#endif
CSTUB_ASM_3(sched_wakeup, spdid, dep_thd, crash_flag)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

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

	       /* test time event spd */
	       /* printc("thd %d wakeup failed and redo!!\n", cos_get_thd_id()); */
	       crash_flag = 1;
	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }

CSTUB_POST


static unsigned long long ttt = 0;

CSTUB_FN_ARGS_2(int, sched_block, spdid_t, spdid, unsigned short int, thd_id)
        unsigned long long start, end;
        struct period_thd *item;
        int crash_flag = 0;
redo:
	/* if (ttt++ % 8000 == 0) printc("thread %d calls << sched_block -- thd_id %d >>\n",cos_get_thd_id(), thd_id); */

#ifdef MEASU_SCHED_INTERFACE_BLOCK
              rdtscll(start);
#endif
CSTUB_ASM_3(sched_block, spdid, thd_id, crash_flag)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

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
		       /* printc("take component block\n"); */
		       sched_component_take(cos_spd_id());
	       }

	       /* printc("thd %d block failed and redo!!\n", cos_get_thd_id()); */
	       crash_flag = 1;
	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_take, spdid_t, spdid)
       unsigned long long start, end;
redo:
	/* printc("thread %d calls << sched_component_take >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_COM_TAKE
           rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_take, spdid)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

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
	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }

       crt_sec_taken = 1;
       crt_sec_owner = cos_get_thd_id();

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_component_release, spdid_t, spdid)
      unsigned long long start, end;
redo:
	/* printc("thread %d calls << sched_component_release >>\n",cos_get_thd_id()); */
#ifdef MEASU_SCHED_INTERFACE_COM_RELEASE
               rdtscll(start);
#endif

CSTUB_ASM_1(sched_component_release, spdid)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

	       /* printc("spd_release update cap %d \n", uc->cap_no); */
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

	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
	       goto redo;
       }

       crt_sec_owner = 0;
       crt_sec_taken = 0;
       /* printc("thread %d calls << sched_component_release >>done!!!\n",cos_get_thd_id()); */

CSTUB_POST


CSTUB_FN_ARGS_1(int, sched_timeout_thd, spdid_t, spdid)

       /* printc("<< cli: sched_timeout_thd >>>\n"); */
redo:

CSTUB_ASM_1(sched_timeout_thd, spdid)

       if (unlikely (fault)){
	       /* cos_brand_cntl(COS_BRAND_REMOVE_THD, 0, 0, 0); */

	       /* printc("spd_ update cap %d \n", uc->cap_no); */
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;

	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
		       printc("replay the sched_timeout_thd\n");
	       }

	       /* rebuild_net_brand(); */
	       goto redo;
       }

       if(!ret) timer_thd = cos_get_thd_id(); /* record the wakeup thread over the interface */
       /* printc("interface: time event  thread is %d\n",timer_thd); */

CSTUB_POST


CSTUB_FN_ARGS_2(int, sched_timeout, spdid_t, spdid, unsigned long, amnt)
	struct period_thd *item;
       /* printc("<< cli: sched_timeout >>>\n"); */
redo:
CSTUB_ASM_2(sched_timeout, spdid, amnt)

       if (unlikely (fault)){
	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
		       printc("set cap_fault_cnt failed\n");
		       BUG();
	       }
       	       fcounter++;

	       if (unlikely(cos_get_thd_id() == timer_thd)) {
		       sched_timeout_thd(cos_spd_id());
		       sched_timeout(cos_spd_id(), wakeup_time);
	       }

	       /* rebuild_net_brand(); */
       	       goto redo;
       }
       /* printc("cli: thd %d amnt %lu for the sched_timeout \n", cos_get_thd_id(), amnt); */
       if (cos_get_thd_id() == timer_thd) wakeup_time = amnt; 
       /* printc("wakeup time is set to  %lu \n", wakeup_time); */

CSTUB_POST


/* CSTUB_FN_ARGS_2(int, sched_create_net_brand, spdid_t, spdid, unsigned short int, port) */

/* 	printc("<< cli: thd %d sched_create_net_brand >>>\n", cos_get_thd_id()); */
/* redo: */

/* CSTUB_ASM_2(sched_create_net_brand, spdid, port) */

/*        if (unlikely (fault)){ */
/* 	       if (unlikely(net_tid && net_bid)) { /\* remove the brand *\/ */
/* 	       	       cos_brand_cntl(COS_BRAND_REMOVE_THD, net_bid, net_tid, 0); */
/* 	       } */

/* 	       /\* printc("spd_ update cap %d \n", uc->cap_no); *\/ */
/* 	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 		       printc("set cap_fault_cnt failed\n"); */
/* 		       BUG(); */
/* 	       } */
/*        	       fcounter++; */


/* 	       goto redo; */
/*        } */

/*        /\* if(!ret) port = port; *\/ */
/*        printc("interface: create_net_brand is %d\n",ret); */

/* CSTUB_POST */

/* CSTUB_FN_ARGS_3(int, sched_add_thd_to_brand, spdid_t, spdid, unsigned short int, bid, unsigned short int, tid) */

/* 	printc("<< cli: thd %d sched_add_thd_to_brand >>>\n", cos_get_thd_id()); */
/* redo: */

/* CSTUB_ASM_3(sched_add_thd_to_brand, spdid, bid, tid) */

/*        if (unlikely (fault)){ */
/* 	       if (unlikely(net_tid && net_bid)) { /\* remove the brand *\/ */
/* 	       	       cos_brand_cntl(COS_BRAND_REMOVE_THD, net_bid, net_tid, 0); */
/* 	       } */

/* 	       /\* printc("spd_ update cap %d \n", uc->cap_no); *\/ */
/* 	       if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 		       printc("set cap_fault_cnt failed\n"); */
/* 		       BUG(); */
/* 	       } */
/*        	       fcounter++; */
/* 	       goto redo; */
/*        } */

/*        printc("add: bid %d tid %d\n", bid, tid); */
/*        net_bid = bid; */
/*        net_tid = tid; */

/* CSTUB_POST */
