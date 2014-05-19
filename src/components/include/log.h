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

/*****  Work mode *********/
//#define DETECTION_MODE
#define LOGEVENTS_MODE
/**************************/

/*****  Measure mode *******/
//#define MEAS_LOG_SYNCACTIVATION 
//#define MEAS_LOG_ASYNCACTIVATION
//#define MEAS_LOG_CASEIP      // cost of event_enqueue()
//#define MEAS_LOG_CHECKING      // per event processing time (with detection mode only)
/**************************/

#ifdef MEAS_LOG_CHECKING
#define DETECTION_MODE
#undef  LOGEVENTS_MODE
#endif

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
extern int llog_process(spdid_t spdid);
/* extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages); */
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
	FN_MM_GET,
	FN_MM_ALIAS,
	FN_MM_REVOKE,
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

static inline CFORCEINLINE int 
evt_ring_is_full()
{
	int capacity, size;

	assert(evt_ring);
	capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
	size = CK_RING_SIZE(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
	if (unlikely(capacity == size + 1)) return 1;
	else return 0;
}

#ifdef MEAS_LOG_SYNCACTIVATION
volatile unsigned long long log_acti_start, log_acti_end;
#endif

#ifdef MEAS_LOG_CASEIP
volatile unsigned long long log_caseip_start, log_caseip_end;
#endif


/* if (cos_get_thd_id() == 15) some_dealy(evt_ring->p_tail); */
/* #define TOTAL_AMNT 128 */
/* static void */
/* some_dealy(int tail) */
/* { */
/* 	unsigned long long t; */
/* 	unsigned long val; */
/* 	volatile unsigned long i = 0; */
/* 	unsigned int delayi = 10;    /\* 10: %7 0: never call ss, 128: always call ss*\/ */

/* 	rdtscll(t); */
/* 	val = (int)(t & (TOTAL_AMNT-1)); */
/* 	if (val < delayi) { */
/* 		/\* while(i++ < 1000); *\/ */
/* 		while(i++ < 10) { */
/* 			printc("delay...thd %d (tail %d)\n", cos_get_thd_id(), evt_ring->p_tail */
/* 				); */
/* 		} */
/* 	} */
/* 	return; */
/* } */

static inline CFORCEINLINE void 
_evt_enqueue(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{
	// align the following instructions
	// change this to __atrribute__((aligned(512))) later
	__asm__ volatile(".balign 512");

	struct evt_entry *evt;
	unsigned int old, new, tail;

#ifdef MEAS_LOG_CASEIP
	rdtscll(log_caseip_start);
#endif
	while(1) {
		if (unlikely(evt_ring_is_full())) {
			/* printc("full !!!! \n"); */
			llog_process(cos_spd_id());
		}
		tail = evt_ring->p_tail;
		evt = (struct evt_entry *) CK_RING_GETTAIL_EVT(logevt_ring, evt_ring, tail);
		if (!evt) continue;  // this indicates the full RB and needs process

		old = 0;
		new = cos_get_thd_id();
		assert(evt && evt_ring);
		
		if (unlikely(!(cos_cas((unsigned long *)&evt->owner, (unsigned long)old,
				       (unsigned long )new)))) {
			cos_cas((unsigned long *)&evt_ring->p_tail, (unsigned long)tail, 
				(unsigned long)tail+1);
		}
	}

	cos_cas((unsigned long *)&evt_ring->p_tail, (unsigned long)tail, 
		(unsigned long)tail+1);

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

static inline CFORCEINLINE void 
evt_enqueue(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{
	if (unlikely(!evt_ring)) {        // create shared RB
		/* vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1); */
		/* printc("to create RB in spd %ld\n", cos_spd_id()); */
		char *addr;
		assert((addr = cos_get_heap_ptr()));
		cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
		if (!(evt_ring = (CK_RING_INSTANCE(logevt_ring) *)(llog_init(cos_spd_id(), (vaddr_t) addr)))) BUG();
		int capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
	}

#ifdef MEAS_LOG_SYNCACTIVATION  // why this does not end itself? (many measurements)
	rdtscll(log_acti_start);
	llog_process(cos_spd_id());
	rdtscll(log_acti_end);
	printc("sync activation cost -- %llu\n", log_acti_end - log_acti_start);
	return;
#endif

#ifdef MEAS_LOG_ASYNCACTIVATION  // this is fine
	return;
#endif

	_evt_enqueue(par1, par2, par3, par4, par5, evt_type);
	
	return;
}

#endif /* LOGGER_H */

