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

#define US_PER_TICK 10000

// settings  (kevin andy)

/************mode******************/
//#define DETECTION_MODE
#define LOGEVENTS_MODE

/************spd execution time paras******************/
//#define MON_PPONG   // just for invocation cost measure

#define MON_SCHED
//#define MON_MM
//#define MON_FS

/**** sched spd error ****/
//#define MON_SCHED_DELAY
//#define MON_SCHED_MAX_EXEC 2500   // this is some number estimated in LOGEVENT MODE

//#define MON_MM_DELAY
//#define MON_MM_MAX_EXEC  20000   // this is some number estimated in LOGEVENT MODE

//#define MON_FS_DELAY
//#define MON_FS_MAX_EXEC

/***********************************************/
/************PI time paras******************/
//#define MON_PI_OVERRUN
//#define MON_MAX_PI  25000   // this is some number estimated in LOGEVENT MODE

//#define MON_PI_SCHEDULING
//#define MON_DEADLOCK
/***********************************************/

/************printing**************/
//#define LOGMGR_DEBUG_PI
//#define LOGMGR_DEBUG_SPDEXEC
//#define LOGMGR_DEBUG_THD_TIMING
//#define LOGMGR_DEBUG_INTNUM
//#define LOGMGR_DEBUG_ORDER
/***********************************************/

/************CRA printing**************/
//#define LOGMGR_DEBUG_CRA_PI
//#define LOGMGR_DEBUG_CRA_SPDEXEC
//#define LOGMGR_DEBUG_CRA_THD_TIMING
//#define LOGMGR_DEBUG_CRA_INTNUM
//#define LOGMGR_DEBUG_CRA_ORDER
/***********************************************/

/************CRA EMP option **************/
//#define LOGMGR_CRA_IPC_SEQUENCE
//#define LOGMGR_CRA_CONTEXT_SWITCH
//#define LOGMGR_CRA_TIMER_INTERRUPT
//#define LOGMGR_CRA_SPD_EXEC_TIME
//#define LOGMGR_CRA_SPD_INV_NUMS
//#define LOGMGR_CRA_EXEC_TIME_SINCE_ACTIVATION
/***********************************************/

/************measuring**************/
//#define MEAS_LOG_SYNCACTIVATION 
//#define MEAS_LOG_ASYNCACTIVATION
//#define MEAS_LOG_CASEIP      // cost of event_enqueue()
//#define MEAS_LOG_CHECKING      // per event processing time (with detection mode only)

#define  MEAS_DEADLOCK_CHECK
#ifdef MEAS_DEADLOCK_CHECK
volatile unsigned long long deadlock_start, deadlock_end;
#endif

/***********************************************/

//#define MON_OVERHEAD      /* infrastructure overhead over the interface */
//#define MON_CAS_TEST      //set eip back overhead

/************no testing**************/
//#define MON_NORM

#ifdef MEAS_LOG_CHECKING
#define DETECTION_MODE
#undef  LOGEVENTS_MODE
#endif

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr, int npages);
extern int llog_process(spdid_t spdid);
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);
extern int llog_contention(spdid_t spdid, int par1, int par2, int par3);
extern int llog_fkml_retrieve_data(spdid_t spdid);

#include <cos_list.h>
#include <ck_ring_cos.h>

#include <cos_types.h>
PERCPU_EXTERN(cos_sched_notifications);

#ifndef CFORCEINLINE
#define CFORCEINLINE __attribute__((always_inline))
#endif

#define LM_SYNC_PERIOD 10  // period for asynchronous processing

/* [ 3938.886250] 	1: alpha */
/* [ 3938.886250] 	2: recov */
/* [ 3938.886251] 	3: init */
/* [ 3938.886256] cobj lllog:2 found at 0x40838000:97b0, size de2c8 -> 41800000 */
/* [ 3938.886258] cobj fprr:3 found at 0x408417c0:10844, size 3e364 -> 41c00000 */
/* [ 3938.886261] cobj mm:4 found at 0x40852040:9e64, size 3daf0 -> 42000000 */
/* [ 3938.886263] cobj print:5 found at 0x4085bec0:1558, size 2c3c8 -> 42400000 */
/* [ 3938.886264] cobj boot:6 found at 0x4085d440 -> 42800000 */
/* [ 3939.888135] cobj l:7 found at 0x42839000:9380, size 34fb0 -> 43800000 */
/* [ 3939.888140] cobj te:8 found at 0x42842380:bd60, size 398a0 -> 43c00000 */
/* [ 3939.888145] cobj mon_p:9 found at 0x4284e100:934c, size 3406c -> 44000000 */
/* [ 3939.888151] cobj lmoncli1:10 found at 0x42857480:b210, size 35eb4 -> 44400000 */
/* [ 3939.888156] cobj sm:11 found at 0x428626c0:c9f0, size 48e54 -> 44800000 */
/* [ 3939.888161] cobj eg:12 found at 0x4286f0c0:934c, size e2d4 -> 44c00000 */
/* [ 3939.888166] cobj e:13 found at 0x42878440:9870, size e878 -> 45000000 */
/* [ 3939.888170] cobj cfkml:14 found at 0x42881cc0:89cc, size 346e8 -> 45400000 */
/* [ 3939.888176] cobj cmultiplexer:15 found at 0x4288a6c0:d1b8, size f9290 -> 45800000 */
/* [ 3939.888181] cobj mpool:16 found at 0x42897880:855c, size 3741c -> 45c00000 */
/* [ 3939.888186] cobj buf:17 found at 0x4289fe00:f034, size 4b068 -> 46000000 */
/* [ 3939.888192] cobj bufp:18 found at 0x428aee40:b95c, size 3c504 -> 46400000 */
/* [ 3939.888197] cobj lmonser1:19 found at 0x428ba7c0:c67c, size 382b0 -> 46800000 */
/* [ 3939.888202] cobj lmonser2:20 found at 0x428c6e40:7de0, size 32b58 -> 46c00000 */
/* [ 3939.888208] cobj va:21 found at 0x428cec40:8ad8, size 34750 -> 47000000 */
/* [ 3939.888212] cobj stconnmt:22 found at 0x428d7740:11fd4, size 1bd5c -> 47400000 */
/* [ 3939.888218] cobj port:23 found at 0x428e9740:7a08, size bac8 -> 47800000 */
/* [ 3939.888223] cobj tif:24 found at 0x428f1180:12260, size 20088 -> 47c00000 */
/* [ 3939.888228] cobj tip:25 found at 0x42903400:13c10, size 1b9f0 -> 48000000 */
/* [ 3939.888234] cobj tnet:26 found at 0x42917040:25c4e, size 1e0756 -> 48400000 */
/* [ 3939.888239] cobj httpt:27 found at 0x4293ccc0:15a98, size 1d868 -> 48800000 */
/* [ 3939.888244] cobj rfs:28 found at 0x42952780:101c4, size 18084 -> 48c00000 */
/* [ 3939.888250] cobj initfs:29 found at 0x42962980:c514, size 3b09c -> 49000000 */
/* [ 3939.888255] cobj unique_map:30 found at 0x4296eec0:b4f8, size 3b088 -> 49400000 */
/* [ 3939.888260] cobj popcgi:31 found at 0x4297a3c0 -> 49800000 */

/********************************/
/*  Hard code spd and thread ID */
/********************************/
// TODO: auto update these ids
// component ID
#define LLLOG_SPD       2
#define SCHED_SPD	3
#define MM_SPD		4
#define BOOTER_SPD	6
#define LOCK_SPD        7
#define TE_SPD          8
#define NETIF_SPD	24

// MLC and EMC
#define FKML_SPD	14
#define MULTIPLEXER_SPD	15

#if defined MON_SCHED
#define TARGET_SPD SCHED_SPD
#elif defined MON_MM
#define TARGET_SPD MM_SPD
#elif defined MON_FS
#define TARGET_SPD FS_SPD
#else
#undef TARGET_SPD    // no specific spd in this case. Used for PI detection
#endif


// thread ID
#define MONITOR_THD     4
#define TIMER_THD	8
#define TE_THD	        10
#define NETWORK_THD	14

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
	FN_SCHED_COMPONENT_TAKE,
	FN_SCHED_COMPONENT_RELEASE,
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
	printc("owner (%d) ", entry->owner);
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

static inline CFORCEINLINE void 
_evt_enqueue(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{
	// align the following instructions
	// change this to __atrribute__((aligned(512))) later
	__asm__ volatile(".balign 512");

	struct evt_entry *evt;
	unsigned int tail;

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
		if (!evt) continue;  // this indicates full!! RB and need process
		
		if (unlikely(!(cos_cas((unsigned long *)&evt->owner, (unsigned long)0,
				       (unsigned long )cos_get_thd_id())))) {
			cos_cas((unsigned long *)&evt_ring->p_tail, (unsigned long)tail, 
				(unsigned long)tail+1);
		} else {
			break;
		}
	}

	/* This is updated by any thread. If already "helped", we will
	 * fail to increment but not repeat */
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

	/* if (evt->from_spd == 4 ||  evt->to_spd	== 4) print_evt_info(evt); */

	// deadlock
	/* if (evt->from_thd == 15 || evt->from_thd == 17) { */
	/* 	if (evt->func_num == 9) { */
	/* 		printc("testing entry\n"); */
	/* 		print_evt_info(evt); */
	/* 	} */
	/* } */

	/* print_evt_info(evt); */

	return;
}

static inline CFORCEINLINE void 
evt_enqueue(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{

	/* //Jiguo: hack for testing only. port_ns with network. Another hack is in inv.c */
	if (cos_spd_id() == 23) {
		printc("kkkkkkkevin andy\n");
		return;
	}

	if (unlikely(!evt_ring)) {        // create shared RB
		/* vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1); */
		printc("to create RB in spd %ld\n", cos_spd_id());
		char *addr;

		/* All spds requires page from mm, except mm and
		 * llog (they get from heap). Hard code here!!! */
		if (cos_spd_id() == LLLOG_SPD)
		{
			assert((addr = cos_get_heap_ptr()));
			cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
		}
		else {
#ifdef USE_VALLOC
			addr = (char *)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
			if (!addr) return NULL;
#else
			addr = (char *)cos_get_vas_page();
#endif
		}

		if (!(evt_ring = (CK_RING_INSTANCE(logevt_ring) *)(llog_init(cos_spd_id(), (vaddr_t) addr, 1)))) BUG();
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

/**************************
  CRA Multiplexer Component -- mpsmc
**************************/
/* entry in the ring buffer between Multiplexer component and the SMC,
 * for now, just let the structure to be same --> copy everything */

struct mpsmc_entry {
	int owner;   // id of thread that owns the entry
	int from_thd, to_thd;
	unsigned long from_spd, to_spd; // can be cap_no
	unsigned long long time_stamp;
	int evt_type;    
	int func_num;   // hard code which function call FIXME:naming space
	int para;   // record passed parameter (e.g. dep_thd)
	int committed;  // a flag to indicate if the event is committed
};

#ifndef __MPSMCRING_DEFINED
#define __MPSMCRING_DEFINED
CK_RING(mpsmc_entry, mpsmcbuffer_ring);
CK_RING_INSTANCE(mpsmcbuffer_ring) *mpsmc_ring;
#endif

static void
event_copy(struct mpsmc_entry *to, struct evt_entry *from)
{
	assert(to && from);

	to->owner = from->owner;
	to->from_thd = from->from_thd;
	to->to_thd = from->to_thd;
	to->from_spd = from->from_spd;
	to->to_spd = from->to_spd;
	to->time_stamp = from->time_stamp;
	to->evt_type = from->evt_type;
	to->func_num = from->func_num;
	to->para = from->para;
	to->committed = from->committed;	

	return;
}

static void
print_mpsmcentry_info(struct mpsmc_entry *entry)
{
	int dest;

	assert(entry);
	printc("mpsmcentry:owner (%d) ", entry->owner);
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

#endif /* LOGGER_H */

