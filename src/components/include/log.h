/*
  Jiguo 2014 -- The header file for the log monitor

  evt_enqueue() function is responsible for tracking the event
  occurrence, including CINV, CRET, SINV, SRET, CS, INT. We need track
  the type of event properly. Here are parameters for different
  events:

  par1 -- to_thread id
          (CINV, CRET, SINV and SRET: this is the current thread)
          (CS: this is the next thread that is about to be dispatched)
          (INT: this is the interrupt thread)
  par2 -- from_component id 
          (this is to_spd (for CINV and SRET))
          (this is from_spd (for SINV and CRET))
	  (INT : this is net_if)
	  (CS : this is scheduler)	  
  par3 -- to_component id 
          (this is to_spd (for CINV and SRET))
          (this is from_spd (for SINV and CRET))
	  (INT : this is net_if)
	  (CS : this is scheduler)	  
  par4 -- function number 
          (hard coded now: 0 for none, 1 for sched_block ...)
  par5 -- extra parameter
          e.g. dep_thd in sched_block, for PI detection. 

  1) Lock. Multiple producers are trying to record their events into a
  single consumer buffer. A lock is used around rb. The contention can
  cause the recursive lock issue here. (lock/booter/time_evt spd uses
  sched_component_take/release, sched/mm uses
  cos_sched_lock_take/release). Other component uses lock_take.
  
  2) Record consistency. For example, a thread can be preempted right
  after evt_conf and before writes into the shared RB. When the event
  is actually written, its time stamp could be wrong. TODO: directly
  write to RB entry (need change ck) 
*/

#ifndef LOGGER_H
#define LOGGER_H

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
extern int llog_process(spdid_t spdid);
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
extern int llog_contention(spdid_t spdid, int par1, int par2, int par3);

#include <cos_list.h>
#include <ck_ring_cos.h>

#include <cos_types.h>
PERCPU_EXTERN(cos_sched_notifications);

#ifndef CFORCEINLINE
#define CFORCEINLINE __attribute__((always_inline))
#endif

/********************************/
/*  Hard code spd and thread ID */
/********************************/
// component ID
#define LLLOG_SPD       2
#define SCHED_SPD	3
#define MM_SPD		4
#define BOOTER_SPD	6
#define LOCK_SPD        7
#define TE_SPD          8
#define NETIF_SPD	25

// thread ID
#define MONITOR_THD     4
#define TIMER_THD	8
#define TE_THD	        10
#define NETWORK_THD	20

#define NUM_PRIOS    32
#define PRIO_LOWEST  (NUM_PRIOS-1)
#define PRIO_HIGHEST 0

/*******************************/
/* Function ID and event type  */
/*******************************/
// function ID
enum{
	FN_SCHED_BLOCK = 1,
	FN_SCHED_WAKEUP,
	FN_SCHED_TIMEOUT,
	FN_LOCK_COMPONENT_TAKE,
	FN_LOCK_COMPONENT_RELEASE,
	FN_MAX
};
// event type
enum{
	EVT_CINV = 1,    // inv in client
	EVT_SINV,        // inv in server
	EVT_SRET,        // ret in server
	EVT_CRET,        // ret in client 
	EVT_CS,          // context switch
	EVT_TINT,        // timer interrupt
	EVT_NINT,        // network interrupt
	EVT_LOG_PROCESS, // log process starts
	EVT_MAX
};

/* event entry in the ring buffer (per event) */
struct evt_entry {
	int owner;   // id of thread that owns the entry
	int from_thd, to_thd;
	unsigned long from_spd, to_spd; // can be cap_no
	unsigned long long time_stamp;
	int evt_type;    
	int func_num;   // hard code which function call FIXME:naming space
	int para;   // record passed parameter (e.g. dep_thd)
	int committed;  // a flag to indicate if the event is committed
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __EVTRING_DEFINED
#define __EVTRING_DEFINED
CK_RING(evt_entry, logevt_ring);
CK_RING_INSTANCE(logevt_ring) *evt_ring;
#endif
			      
static void
print_evt_info(struct evt_entry *entry)
{
	int dest;

	assert(entry);
	printc("owner (%d --> ", entry->owner);
	printc("thd (%d --> ", entry->from_thd);
	printc("%d) ", entry->to_thd);

	if (entry->evt_type == EVT_CRET && entry->from_spd > MAX_NUM_SPDS) {
		dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, entry->to_spd, entry->from_spd);
		printc("spd (%d --> ", dest);
	} else printc("spd (%lu --> ", entry->from_spd);

	if (entry->evt_type == EVT_CINV && entry->to_spd > MAX_NUM_SPDS) {
		dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, entry->from_spd, entry->to_spd);
		printc("%d) ", dest);
	} else printc("%lu) ", entry->to_spd);

	printc("func_num %d ", entry->func_num);
	printc("para %d ", entry->para);
	printc("type %d ", entry->evt_type);
	printc("time_stamp %llu ", entry->time_stamp);
	printc("committed %d\n", entry->committed);
	
	return;
}

/* // par1: to_thd  par2: from spd  par3: to_spd  par4: func_num  par5: para */
/* static inline CFORCEINLINE void */
/* evt_omitted(int to_thd, unsigned long from_spd, unsigned long to_spd, int func_num, int para, int evt_type, int owner) */
/* { */
/* 	int cont_spd = cos_spd_id(); */
	
/* 	int par1 = (evt_type<<28) | (func_num<<24) | (para<<16) | (to_thd<<8) | (owner); */
/* 	unsigned long par2 = from_spd;  // this can be cap_no */
/* 	unsigned long par3 = to_spd;    // this can be cap_no */
	
/* 	llog_contention(cos_spd_id(), par1, par2, par3); */
	
/* 	return; */
/* } */

/* static inline CFORCEINLINE int  */
/* evt_ring_is_full() */
/* { */
/* 	int capacity, size; */

/* 	assert(evt_ring); */
/* 	capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring)); */
/* 	size = CK_RING_SIZE(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring)); */
/* 	if (unlikely(capacity == size + 1)) return 1; */
/* 	else return 0; */
/* } */

/* static inline CFORCEINLINE void  */
/* evt_conf(struct evt_entry *evt, int par1, unsigned long par2, unsigned long par3, int par4, int par5, int type) */
/* { */
/* 	assert(evt); */

/* 	evt->from_thd	= cos_get_thd_id(); */
/* 	evt->to_thd	= par1; */
/* 	evt->from_spd	= par2; */
/* 	evt->to_spd	= par3; */
/* 	evt->func_num	= par4; */
/* 	evt->para	= par5; */
/* 	evt->evt_type	= type; */
/* 	rdtscll(evt->time_stamp); */

/* 	// do the introspection and set bit here */

/* 	/\* print_evt_info(evt); *\/ */

/* 	return; */
/* } */

//#define MEAS_LOG_SYNCACTIVATION 
#ifdef MEAS_LOG_SYNCACTIVATION
volatile unsigned long long log_acti_start, log_acti_end;
#endif

//#define MEAS_LOG_CASEIP
#ifdef MEAS_LOG_CASEIP
volatile unsigned long long log_caseip_start, log_caseip_end;
#endif

static inline CFORCEINLINE void 
evt_enqueue(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{
	__asm__ volatile(".balign 512");  // align instructions

#ifdef MEAS_LOG_CASEIP
	rdtscll(log_caseip_start);
#endif

	if (unlikely(!evt_ring)) {        // create shared RB
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cli_addr);
		if (!(evt_ring = (CK_RING_INSTANCE(logevt_ring) *)(llog_init(cos_spd_id(), cli_addr)))) BUG();
		int capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
		printc("created RB with capacity %d\n", capacity);

	}

	struct evt_entry *evt;
	int old, new;
	do {
		evt = (struct evt_entry *) CK_RING_GETTAIL(logevt_ring, evt_ring);
		if (unlikely(!evt)) {
			printc("full !!!! \n");
			llog_process(cos_spd_id());
			evt = (struct evt_entry *) CK_RING_GETTAIL(logevt_ring, evt_ring);
			assert(evt);
		}
		old = 0;
		new = cos_get_thd_id();
		assert(evt && evt_ring);
	} while(unlikely(!(cos_cas((unsigned long *)&evt->owner, (unsigned long)old,
			   (unsigned long )new))) && evt_ring->p_tail++);

	old = evt_ring->p_tail;
	new = evt_ring->p_tail + 1;
	cos_cas((unsigned long *)&evt_ring->p_tail, (unsigned long)old, 
		(unsigned long)new);

	evt->from_thd	= cos_get_thd_id();
	evt->to_thd	= par1;
	evt->from_spd	= par2;
	evt->to_spd	= par3;
	evt->func_num	= par4;
	evt->para	= par5;
	evt->evt_type	= evt_type;
	rdtscll(evt->time_stamp);
	evt->committed   = 1;   // after this, the event is said "committed"

#ifdef MEAS_LOG_CASEIP
	rdtscll(log_caseip_end);
	printc("one event log cost -- %llu\n", log_caseip_end - log_caseip_start);
#endif

	/* print_evt_info(evt); */

	return;
}

	/* int size; */
	/* size = CK_RING_SIZE(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring)); */

/* #ifdef LOG_MONITOR   // since ck library now has LOG_MONITOR */
/* 	int owner = CK_RING_ENQUEUE_SPSCCONT(logevt_ring, evt_ring); */
/* 	if (unlikely(owner)) {   // contention on this RB */
/* 		printc("<<<Contention on RB !!!!! %lu thd %d (owner %d)>>>\n",  */
/* 		       cos_spd_id(), cos_get_thd_id(), owner); */
/* 		evt_omitted(par1, par2, par3, par4, par5, evt_type, owner); */
/* 		return; */
/* 	} */

/* 	// log the event (take slot first, then record) */
/* 	struct evt_entry *evt; */
/* 	while(!(evt = (struct evt_entry *)CK_RING_ENQUEUE_SPSCPRE(logevt_ring, evt_ring))); */
/* 	evt_conf(evt, par1, par2, par3, par4, par5, evt_type); */

/* 	// process log if any RB is full */
/* #ifdef MEAS_LOG_SYNCACTIVATION   */
/* 	while (unlikely(evt_ring_is_full())) { */
/* 		assert(par1 != LLLOG_SPD); */
/* 		rdtscll(log_acti_start); */
/* 		// immediately return from the log_mgr (pretend all processed) */
/* 		llog_process(cos_spd_id()); */
/* 		rdtscll(log_acti_end); */
/* 		/\* printc("FULL! invoke/switch/process cost %llu\n", log_acti_end-log_acti_start); *\/ */
/* 	} */
/* #else */
/* 	if (unlikely(evt_ring_is_full())) { */
/* 		// contention RB better not be full before any other RB is full */
/* 		assert(par1 != LLLOG_SPD); */
/* 		/\* printc("<<<FULL !!!!! spd %lu thd %d>>>\n", cos_spd_id(), cos_get_thd_id()); *\/ */
/* 		llog_process(cos_spd_id()); */
/* 	} */
/* #endif	 */
/* #endif	 */
/* 	return; */
/* } */

#endif /* LOGGER_H */

