#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

//#define DETECTION_MODE
#define LOGEVENTS_MODE

#include <log_process.h>   // check all constraints

//#define MEAS_LOG_CHECKING      /* per event processing time */
//#define MEAS_LOG_ASYNCACTIVATION  /* cost of async acti from periodic processing */
int test_aysncthd;

#define LM_SYNC_PERIOD 10  // period for asynchronous processing

static int LOG_INIT_THD;
static int LOG_LOOP_THD;
static int LOG_PREV_THD;
static int LOG_PREV_SPD;

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages) 
{ return NULL; }

/* Set up ring buffer (assume a page for now, not including ll_log) */
static int 
lmgr_setuprb(spdid_t spdid, vaddr_t cli_addr) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;
	
	assert(spdid && cli_addr);
	addr = llog_get_page();
	if (!addr) goto err;
	spdmon = &logmon_info[spdid];
	assert(spdmon);

	cli_ring = cli_addr;
	spdmon->mon_ring = (vaddr_t)addr;

	if (unlikely(cli_ring != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_ring))) {
		printc("alias rings %d failed.\n", spdid);
		assert(0);
	}
	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	spdmon->cli_ring = cli_ring;
	CK_RING_INIT(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)addr), NULL,
		     get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_logevt_ring))/sizeof(struct evt_entry)));
	
	return 0;
err:
	return -1;
}


// control how many time the thread can be activated before processed
#if defined(MEAS_LOG_SYNCACTIVATION) || defined(MEAS_LOG_ASYNCACTIVATION)
static int test_num;
#endif

#ifdef MEAS_LOG_CHECKING
volatile unsigned long long logmeas_start, logmeas_end;
#endif

volatile unsigned int event_num;

static void
lmgr_action()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

#ifdef MEAS_LOG_SYNCACTIVATION
	if (test_num++ < 20) {
		printc("sync return here\n");
		return;
	}
#endif
#ifdef MEAS_LOG_ASYNCACTIVATION
	/* printc("test_num %d\n", test_num); */
	/* printc("thd %d\n", test_aysncthd); */
	if (test_aysncthd == 11 && test_num++ < 5) return;
#endif

	event_num = 0;
	printc("start process log ....\n");

	rdtscll(lpc_start);
	evt = find_next_evt(NULL);
	if (!evt) goto done;
	
	do {
#ifdef MEAS_LOG_CHECKING		
		rdtscll(logmeas_start);
#endif
		event_num++;
		constraint_check(evt);
#ifdef MEAS_LOG_CHECKING		
		rdtscll(logmeas_end);
		printc("per evt check cost %llu\n", logmeas_end - logmeas_start);
#endif
	} while ((evt = find_next_evt(evt)));

	/* assert(test_num++ < 2);  // remove this later */
	printc("log process done (%d evts)\n", event_num);
done:
	rdtscll(lpc_end);
	lpc_last = lpc_end - lpc_start;
	update_proc(lpc_last);

	return;
}

static void
clear_owner_commit()
{
	int i, j;
	CK_RING_INSTANCE(logevt_ring) *evtring;
        struct logmon_info *spdmon;

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		assert(spdmon);
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		if (!evtring) continue;

		int capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));

		struct evt_entry *tmp;
		for (j = 0; j < capacity; j++) {
			tmp = (struct evt_entry *) CK_RING_GETTAIL(logevt_ring, evtring);
			if (tmp) {
				tmp->owner     = 0;
				tmp->committed = 0;
			}
			evtring->p_tail++;
			evtring->c_head++;
		}
	}
	return;
}

static void
lllog_loop(void) { 
	int tid, pthd;
	while(1) {
		pthd = LOG_PREV_THD;
		test_aysncthd = pthd;  // test asyn cost only
		assert(cos_get_thd_id() == LOG_LOOP_THD);
		lmgr_action();
		clear_owner_commit();
		LOG_PREV_THD = 0;
		LOG_PREV_SPD = 0;
		cos_switch_thread(pthd, 0);
	}
	assert(0);
	return; 
}

/* Initialize log manager here...do this only once */
static void
lmgr_initialize(void)
{
	memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
	int i, j;
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		logmon_info[i].spdid	   = i;
		logmon_info[i].mon_ring	   = 0;
		logmon_info[i].cli_ring	   = 0;
		logmon_info[i].spd_last_ts = 0;
		
		for (j = 0; j < MAX_STATIC_CAP; j++) {
			logmon_info[i].capdest[j].dest  = 0;
			logmon_info[i].capdest[j].capno = 0;
		}
		memset(&logmon_info[i].first_entry, 0, sizeof(struct evt_entry));

		// initialize the heap entry
		es[i].ts = 0; 
		es[i].spdid = i;
	}
		
	init_log();

        // alloc a page for heap operation (assume a page for now)
	h = log_heap_alloc(MAX_NUM_SPDS, evtcmp, evtupdate);
	assert(h);

        // local RB for contention only
	evt_ring = (CK_RING_INSTANCE(logevt_ring) *)llog_get_page();
	assert(evt_ring);

	logmon_info[cos_spd_id()].mon_ring = (vaddr_t)evt_ring;
	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	CK_RING_INIT(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring), NULL,
		     get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_logevt_ring))/sizeof(struct evt_entry)));
	
	lpc_start = lpc_end = lpc_last = 0;

	LOG_INIT_THD = cos_get_thd_id();
	sched_init(0);
	LOG_LOOP_THD = cos_create_thread((int)lllog_loop, (int)0, 0);

	printc("log_loop_thd %d is created by thd %d\n", LOG_LOOP_THD, cos_get_thd_id());

	return;
}

/***********************/
/*  External functions  */
/***********************/

/* activate to process log */
int
llog_process(spdid_t spdid)
{
	LOCK();

	LOG_PREV_SPD     = spdid;
	LOG_PREV_THD     = cos_get_thd_id();

	/*  Log the processing as an event for that any thread that is
	    still in PI state before process starts
	*/
	evt_enqueue(LOG_LOOP_THD, LOG_PREV_SPD, cos_spd_id(),
		    0, 0, EVT_LOG_PROCESS);

	while (LOG_PREV_THD == cos_get_thd_id()) cos_switch_thread(LOG_LOOP_THD, 0);

	UNLOCK();

	return 0;

}

/* Return the periodicity for asynchronously monitor processing  */
unsigned int
llog_getsyncp()
{
	return LM_SYNC_PERIOD;
}

/* Log the event of contention. This should only be called when
   contention happens on RB and we want to skip that event to avoid
   the recursive lock issue (e.g. contends for the lock in the
   scheduler and when the contention event (CS) needs be logged in a
   RB, the lock is needed for that RB again) 
   For EVT_RBCONTEND, we save the followings:
   owner, contender, contender's evt_type, contention spd
*/

int
llog_contention(spdid_t spdid, int par2, int par3, int par4)
{
	int owner, cont_thd, cont_spd, this_spd;
	int to_thd, func_num, para, evt_type;
	unsigned long from_spd, to_spd;

        LOCK();

	cont_spd = spdid;
	cont_thd = cos_get_thd_id();
	this_spd = cos_spd_id();

	evt_type = par2>>28;
	func_num = par2>>24 & 0x0000000F;
	para	 = par2>>16 & 0x000000FF;
	to_thd	 = par2>>8  & 0x000000FF;
	owner	 = par2     & 0x000000FF;
	from_spd = par3;
	to_spd	 = par4;

	/* printc("owner %d cont_spd %d to_thd %d func_num %d para %d evt_type %d from_spd %lu to_spd %lu\n",  */
	/*        owner, cont_spd, to_thd, func_num, para, evt_type, from_spd, to_spd); */
	
	evt_enqueue(CONT_FLAG | (to_thd<<16) | (cont_thd<<8) | (owner), 
		    from_spd, to_spd, func_num, para, evt_type);	
	
        UNLOCK();
        return 0;
}


// really do not need this since we need read c from somewhere anyway!!!
/* Initialize thread period. This should only be called when a period
   thread is created by periodic_wake_create
 */
int 
llog_setperiod(spdid_t spdid, unsigned int thd_id, unsigned int period)
{
        struct logmon_info *spdmon;
	struct thd_trace *ttl = NULL;

	assert(spdid == TE_SPD && thd_id > 0);

	LOCK();

	ttl = &thd_trace[thd_id];
	assert(ttl);
	ttl->p = period;
	
	// set time_window to be the minimum period of all task for interrupts log
	if (period*CYC_PER_TICK < window_sz) window_sz = period*CYC_PER_TICK;
	/* window_sz = 2*CYC_PER_TICK; */  // test only. Remove later
	
	UNLOCK();
	return 0;
}

/* Initialize thread priority. This should only be called when a
   thread is created and the priority is used to detect the priority
   inversion, scheduling decision, deadlock, etc.
 */
int 
llog_setprio(spdid_t spdid, unsigned int thd_id, unsigned int prio)
{
        struct logmon_info *spdmon;
	struct thd_trace *ttl = NULL;

	assert(spdid == SCHED_SPD && thd_id > 0 && prio <= PRIO_LOWEST);

	LOCK();

	ttl = &thd_trace[thd_id];
	assert(ttl);
	ttl->prio = prio;

	UNLOCK();
	return 0;
}

/* Initialize shared RB. Called from each client spd when the first
 * event happens in that spd. Not including logmgr itself. */
vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon);

	assert(!spdmon->mon_ring && !spdmon->cli_ring);

	LOCK();
	if (lmgr_setuprb(spdid, addr)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(spdmon->mon_ring && spdmon->cli_ring);

	ret = spdmon->cli_ring;
done:
	UNLOCK();
	return ret;
}

typedef void (*crt_thd_fn_t)(void);

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		lmgr_initialize(); break;
	case COS_UPCALL_CREATE:
		lllog_loop();
		((crt_thd_fn_t)arg1)();
		break;
	case COS_UPCALL_DESTROY:
		printc("DESTROY thd %d\n", cos_get_thd_id());
		assert(0); break;
	default:
		return;
	}
	return;
}
