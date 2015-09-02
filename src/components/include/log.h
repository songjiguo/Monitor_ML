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

// for CRA fake ML component
#define MULTIPLEXER_SPD	14
#define FKML_SPD	13

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

	//Jiguo: hack for testing only. port_ns with network. Another hack is in inv.c
	if (cos_spd_id() == 21) return;

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

/**************************
  CRA ML Component        -- mlmp
**************************/
/* entry in the ring buffer between the ML component and the
 * Multiplexer component (stream....) */

enum stream_flags {
	STREAM_THD_EVT_SEQUENC     = (0x01 << 0),
	STREAM_THD_EXEC_TIMING     = (0x01 << 1),
	STREAM_THD_INTERACTION     = (0x01 << 2),
	STREAM_SPD_INVOCATIONS     = (0x01 << 3),
	STREAM_SPD_EVT_SEQUENC     = (0x01 << 4),
	STREAM_MAX                 = (0x01 << 10),   // max 10 different type of streams
};

/* stream 1 --------- */
struct mlmp_thdevtseq_entry {
	unsigned int thd_id;
	unsigned int thd_evt_seq[1024];
};

//TODO: use macro define

#ifndef __MLMPRING_THDEVTSEQ_DEFINED
#define __MLMPRING_THDEVTSEQ_DEFINED
CK_RING(mlmp_thdevtseq_entry, mlmpthdevtseqbuffer_ring);
CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *mlmpthdevtseq_ring;
#endif

static void
print_mlmpthdevtseqevt_info(struct mlmp_thdevtseq_entry *entry)
{
	assert(entry);
	printc("mlmp: thd_id (%d) -- ", entry->thd_id);
	printc("thd evt seq (%d)\n ", entry->thd_evt_seq[0]);
	
	return;
}

/* stream 2 --------- */
struct mlmp_thdactexec_entry {
	unsigned long long thd_activation_time;
	unsigned long long thd_execution_time;
};

struct mlmp_thdtime_entry {
	unsigned int thd_id;
	struct mlmp_thdactexec_entry thd_time_entry[256];
};

#ifndef __MLMPRING_THDTIME_DEFINED
#define __MLMPRING_THDTIME_DEFINED
CK_RING(mlmp_thdtime_entry, mlmpthdtimebuffer_ring);
CK_RING_INSTANCE(mlmpthdtimebuffer_ring) *mlmpthdtime_ring;
#endif

/* stream 3 --------- */
struct mlmp_thdcsint_entry {
	unsigned int from_thd;  // this is the current
	unsigned int to_thd;    // this is next thread or interrupting thread
	unsigned long long time_stamp;
};

struct mlmp_thdinteract_entry {
	struct mlmp_thdcsint_entry thd_cs[128];
	struct mlmp_thdcsint_entry thd_ints[128];
};

#ifndef __MLMPRING_THDINTERACT_DEFINED
#define __MLMPRING_THDINTERACT_DEFINED
CK_RING(mlmp_thdinteract_entry, mlmpthdinteractbuffer_ring);
CK_RING_INSTANCE(mlmpthdinteractbuffer_ring) *mlmpthdinteract_ring;
#endif

/* stream 4 --------- */
struct mlmp_spdinvnum_entry {
	unsigned int spd_id;
	unsigned int invocation_nums;
};

#ifndef __MLMPRING_SPDINVNUM_DEFINED
#define __MLMPRING_SPDINVNUMx_DEFINED
CK_RING(mlmp_spdinvnum_entry, mlmpspdinvnumbuffer_ring);
CK_RING_INSTANCE(mlmpspdinvnumbuffer_ring) *mlmpspdinvnum_ring;
#endif

/* stream 5 --------- */
struct mlmp_spdevtseq_entry {
	unsigned int spd_id;
	unsigned int spd_evt_seq[1024];
};


#ifndef __MLMPRING_SPDEVTSEQ_DEFINED
#define __MLMPRING_SPDEVTSEQ_DEFINED
CK_RING(mlmp_spdevtseq_entry, mlmpspdevtseqbuffer_ring);
CK_RING_INSTANCE(mlmpspdevtseqbuffer_ring) *mlmpspdevtseq_ring;
#endif

/* stream default ------------- */
struct mlmp_entry {
	int para1;
	int para2;
	int para3;
	unsigned long long time_stamp;
};

#ifndef __MLMPRING_DEFINED
#define __MLMPRING_DEFINED
CK_RING(mlmp_entry, mlmpbuffer_ring);
CK_RING_INSTANCE(mlmpbuffer_ring) *mlmp_ring;
#endif


static void
print_mlmpentry_info(struct mlmp_entry *entry)
{
	assert(entry);
	printc("mlmp: para1 (%d) -- ", entry->para1);
	printc("para2 (%d) -- ", entry->para2);
	printc("para3 (%d) -- ", entry->para3);
	printc("time_stamp (%llu) \n", entry->time_stamp);
	
	return;
}

#endif /* LOGGER_H */

