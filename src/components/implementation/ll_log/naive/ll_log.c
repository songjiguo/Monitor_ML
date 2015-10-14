#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>
#include <log.h>

#include <log_process.h>   // check all constraints

int test_aysncthd;


//#define CRA_ENABLED

static int LOG_INIT_THD;
static int LOG_LOOP_THD;
static int LOG_PREV_THD;
static int LOG_PREV_SPD;

/* static int MULTIPLEXER_THD; */

void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages) 
{ return NULL; }

/* Set up ring buffer (assume a page for now, not including ll_log) */
static int 
lmgr_setuprb(spdid_t spdid, vaddr_t cli_addr, int npages) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;
	
	mon_assert(spdid && cli_addr);
	spdmon = &logmon_info[spdid];
	mon_assert(spdmon);
	spdmon->cli_ring = cli_addr;

	/* printc("monitor aliasing: addr %p to (spdid %d) cli_ring %p\n", */
	/*        (vaddr_t)addr, spdid, (vaddr_t)cli_ring); */
	addr = llog_get_page();
	if (!addr) goto err;
	spdmon->mon_ring = (vaddr_t)addr;

	int tmp = npages;
	char *start = addr;
	while(tmp--) {
		if (unlikely(cli_addr != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_addr))) {
			printc("alias rings %d failed.\n", spdid);
			mon_assert(0);
		}
		if (tmp) {
			cli_addr += PAGE_SIZE;
			addr = llog_get_page();
			if (!addr) goto err;
		}
	}
	addr = start;
	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	CK_RING_INIT(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)addr), NULL, 
		     get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_logevt_ring))/sizeof(struct evt_entry)));

	return 0;
err:
	return -1;
}

#ifdef MEAS_LOG_CHECKING
volatile unsigned long long logmeas_start, logmeas_end;
#endif

// naive way to clear all committed bit
static void
clear_owner_commit()
{
	int i, j;
	CK_RING_INSTANCE(logevt_ring) *evtring;
        struct logmon_info *spdmon;

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		mon_assert(spdmon);
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		if (!evtring) continue;

		int capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));

		struct evt_entry *tmp;
		unsigned int tail;
		for (j = 0; j < capacity; j++) {
			tail = evtring->p_tail;
			tmp = (struct evt_entry *) CK_RING_GETTAIL_EVT(logevt_ring, evtring, tail);
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

static void lllog_loop(void);
/* static void lllog_cra_loop(void); */

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
	mon_assert(h);

        // local RB for contention only
	evt_ring = (CK_RING_INSTANCE(logevt_ring) *)llog_get_page();
	mon_assert(evt_ring);

	logmon_info[cos_spd_id()].mon_ring = (vaddr_t)evt_ring;
	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	CK_RING_INIT(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring), NULL,
		     get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_logevt_ring))/sizeof(struct evt_entry)));
	
	lpc_start = lpc_end = lpc_last = 0;

	LOG_INIT_THD = cos_get_thd_id();
	sched_init(0);

	LOG_LOOP_THD = cos_create_thread((int)lllog_loop, (int)0, 0);
	printc("log_loop_thd %d is created by thd %d\n", LOG_LOOP_THD, cos_get_thd_id());

	/* MULTIPLEXER_THD = cos_create_thread((int)lllog_cra_loop, (int)2, 0); */
	/* printc("multiplexer_thd %d is created by thd %d\n",  */
	/*        MULTIPLEXER_THD, cos_get_thd_id()); */

	return;
}

volatile unsigned int event_num;   // how many evts processed
static void
lmgr_action()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

	event_num = 0;
	/* printc("start process log ....\n"); */

	rdtscll(lpc_start);
	evt = find_next_evt(NULL);
	if (!evt) goto done;
	
	do {
		event_num++;
		constraint_check(evt);
	} while ((evt = find_next_evt(evt)));

	/* mon_assert(test_num++ < 2);  // remove this later */
	/* printc("log process done (%d evts)\n", event_num); */
done:
	rdtscll(lpc_end);
	lpc_last = lpc_end - lpc_start;
	update_proc(lpc_last);

	return;
}


/* copy event information from SMC buffers to the SMC-ML buffer */
static void
cra_copy(struct evt_entry *evt)
{
	/* printc("thread %d is doing CRA copy\n", cos_get_thd_id()); */

	assert(evt && mpsmc_ring);
	/* print_evt_info(evt); */
	CK_RING_INSTANCE(mpsmcbuffer_ring) *mpsmcevtring;
	mpsmcevtring = mpsmc_ring;
	unsigned int tail = mpsmc_ring->p_tail;
	/* printc("current tail is %d\n", tail); */

	/* It is possible that the copying events from SMC bufferes to
	 * the buffer between the SMC and EMP is faster than they are
	 * consumed to different stream buffer. The processing the
	 * events could be separated at the different speed. */
	struct evt_entry *mpsmcevt = NULL;
	mpsmcevt = (struct evt_entry *) CK_RING_GETTAIL_EVT(mpsmcbuffer_ring,
							      mpsmc_ring, tail);
	/* int capacity = CK_RING_CAPACITY( */
	/* 	mpsmcbuffer_ring,  */
	/* 	(CK_RING_INSTANCE(mpsmcbuffer_ring) *)((void *)mpsmc_ring)); */
	/* printc("SMC:the mpsmc ring cap is %d\n\n", capacity); */
	/* printc("SMC:the mpsmc ring tail is %d\n\n", mpsmc_ring->p_tail); */
	
	assert(mpsmcevt);
	event_copy(mpsmcevt, evt);
	mpsmc_ring->p_tail = tail+1;

	/* print_mpsmcentry_info(mpsmcevt); */

	return;
}

// TODO: use function pointer to unify these two
static void
mpsmc_action()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

	event_num = 0;
	/* printc("start cra copy process ....\n"); */

	evt = find_next_evt(NULL);
	if (!evt) goto done;
	
	do {
		event_num++;
		cra_copy(evt);
	} while ((evt = find_next_evt(evt)));

	/* printc("cra copy process done (thd %d %d evts)\n",  */
	/*        cos_get_thd_id(), event_num); */
done:
	return;
}

/* static void */
/* lllog_cra_loop(void) {  */
/* 	int tid, pthd; */
/* 	while(1) { */
/* 		pthd = LOG_PREV_THD; */
/* 		/\* mon_assert(cos_get_thd_id() == MULTIPLEXER_THD); *\/ */

/* 		mpsmc_action();  // CRA process */

/* 		LOG_PREV_THD = 0; */
/* 		LOG_PREV_SPD = 0; */
/* 		cos_switch_thread(pthd, 0); */
/* 	} */
/* 	mon_assert(0); */
/* 	return;  */
/* } */

static void
lllog_loop(void) { 
	int tid, pthd;
	while(1) {
		pthd = LOG_PREV_THD;
		test_aysncthd = pthd;  // test asyn cost only
		mon_assert(cos_get_thd_id() == LOG_LOOP_THD);


/* #if !defined(MEAS_LOG_SYNCACTIVATION) && !defined(MEAS_LOG_ASYNCACTIVATION) */
/* 		lmgr_action(); */
/* 		clear_owner_commit(); */
/* #endif */


		/* for CRA, the ring might not have been set up when any rb is
		 * full, so for now, just ignore any thing before the ring is
		 * set up. */
		if (mpsmc_ring) {
			mpsmc_action();
			clear_owner_commit();
		} else {
#if !defined(MEAS_LOG_SYNCACTIVATION) && !defined(MEAS_LOG_ASYNCACTIVATION)
			lmgr_action();
			clear_owner_commit();
#endif
		}


/* #ifdef CRA_ENABLED */
/* 		mpsmc_action();  // CRA */
/* #else */

/* #if !defined(MEAS_LOG_SYNCACTIVATION) && !defined(MEAS_LOG_ASYNCACTIVATION) */
/* 		/\* if (pthd == MULTIPLEXER_THD) { *\/ */
/* 		/\* 	mpsmc_action();  // CRA copy *\/ */
/* 		/\* } else { *\/ */
/* 		lmgr_action(); */
/* 		clear_owner_commit(); */
/* 		/\* } *\/ */
/* #endif */
/* #endif */

		LOG_PREV_THD = 0;
		LOG_PREV_SPD = 0;
		cos_switch_thread(pthd, 0);
	}
	mon_assert(0);
	return; 
}

/***********************/
/*  External functions  */
/***********************/

/* activate to process log */
int
llog_process(spdid_t spdid)
{
	/* printc("llog_process thd %d is processing data in SMC now....(from spd %d)\n", */
	/*        cos_get_thd_id(), spdid); */
	
	LOCK();

	LOG_PREV_SPD     = spdid;
	LOG_PREV_THD     = cos_get_thd_id();
	/* printc("llog_process is setting LOG_PREV_THD to ....%d\n", LOG_PREV_THD); */
	/* printc("LOG_LOOP_THD is ....%d\n", LOG_LOOP_THD); */
	/*  Log the processing as an event for that any thread that is
	    still in PI state before process starts
	*/
#if !defined(MEAS_LOG_SYNCACTIVATION) && !defined(MEAS_LOG_ASYNCACTIVATION)
	evt_enqueue(LOG_LOOP_THD, LOG_PREV_SPD, cos_spd_id(),
		    0, 0, EVT_LOG_PROCESS);
#endif
	while (LOG_PREV_THD == cos_get_thd_id()) cos_switch_thread(LOG_LOOP_THD, 0);

	/* printc("llog_process thd %d is releasing the lock\n", cos_get_thd_id()); */
	UNLOCK();

	return 0;

}

/* Copy all left events to the shared buffer between the multiplexer and
 * SMC so the multiplexer thread can process/stream it. See
 * multiplexer.c */
int
llog_multiplexer_retrieve_data(spdid_t spdid)
{
	/* printc("multiplexer thd %d is retrieving any data in SMC now....\n", */
	/*        cos_get_thd_id()); */

	return llog_process(spdid);

	/* LOCK(); */

	/* LOG_PREV_SPD     = spdid; */
	/* LOG_PREV_THD     = cos_get_thd_id(); */

	/* while (LOG_PREV_THD == cos_get_thd_id()) cos_switch_thread(MULTIPLEXER_THD, 0); */

	/* /\* printc("llog_process thd %d is releasing the lock\n", cos_get_thd_id()); *\/ */
	/* UNLOCK(); */

	/* return 0; */
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
   thread is created through periodic_wake_create
 */
int 
llog_setperiod(spdid_t spdid, unsigned int thd_id, unsigned int period)
{
        struct logmon_info *spdmon;
	struct thd_trace *ttl = NULL;

	mon_assert(spdid == TE_SPD && thd_id > 0);

	LOCK();

	ttl = &thd_trace[thd_id];
	mon_assert(ttl);
	ttl->p = period;

	printc("set period %d for thd %d (from spd %d)\n", period, thd_id, spdid);
	
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

	mon_assert(spdid == SCHED_SPD && thd_id > 0 && prio <= PRIO_LOWEST);

	LOCK();

	ttl = &thd_trace[thd_id];
	mon_assert(ttl);
	ttl->prio = prio;

	printc("set priority %d for thd %d\n", prio, thd_id);

	UNLOCK();
	return 0;
}

/* CRA: multiplexer component needs to get the period and priod of a
 * thread for processing */
unsigned int 
llog_getperiod(spdid_t spdid, unsigned int thd_id)
{
        struct logmon_info *spdmon;
	struct thd_trace *ttl = NULL;

	mon_assert(thd_id > 0);

	ttl = &thd_trace[thd_id];
	mon_assert(ttl);
	/* printc("get period %llu for thd %d\n", ttl->p, thd_id); */
	return ttl->p;
}

unsigned int 
llog_getprio(spdid_t spdid, unsigned int thd_id)
{
        struct logmon_info *spdmon;
	struct thd_trace *ttl = NULL;

	mon_assert(thd_id > 0);

	ttl = &thd_trace[thd_id];
	mon_assert(ttl);
	/* printc("get priority %d for thd %d\n", ttl->prio, thd_id); */
	return ttl->prio;
}

/* Initialize shared RB. Called from each client spd when the first
 * event happens in that spd. Not including logmgr itself. */
vaddr_t
llog_init(spdid_t spdid, vaddr_t addr, int npages)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	mon_assert(spdid);
	spdmon = &logmon_info[spdid];
	mon_assert(spdmon);

	mon_assert(!spdmon->mon_ring && !spdmon->cli_ring);
	LOCK();

	if (lmgr_setuprb(spdid, addr, npages)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	mon_assert(spdmon->mon_ring && spdmon->cli_ring);

	ret = spdmon->cli_ring;
done:
	UNLOCK();
	return ret;
}


static int 
mpsmc_setuprb(spdid_t spdid, vaddr_t cli_addr, int npages) {
        vaddr_t log_ring, cli_ring = 0;

	assert(!mpsmc_ring && cli_addr);
	char *addr = llog_get_page();
	if (!addr) goto err;
	mpsmc_ring = (CK_RING_INSTANCE(mpsmcbuffer_ring) *)addr;

	int tmp = npages;
	char *start = addr;
	while(tmp--) {
		/* printc("mpsmc: try to alias,  %d pages left\n", tmp); */
		if (unlikely(cli_addr != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_addr))) {
			printc("alias rings %d failed.\n", spdid);
			mon_assert(0);
		}
		if (tmp) {
			cli_addr += PAGE_SIZE;
			addr = llog_get_page();
			if (!addr) goto err;
		}
	}
	addr = start;
	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	CK_RING_INIT(mpsmcbuffer_ring, (CK_RING_INSTANCE(mpsmcbuffer_ring) *)((void *)addr), NULL, 
		     get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mpsmcbuffer_ring))/sizeof(struct evt_entry)));

	/* printc("\nmpsmc: test the size %d\n", get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mpsmcbuffer_ring))/sizeof(struct mpsmc_entry))); */
	/* printc("PAGE_SIZE*npages %d\n",PAGE_SIZE*npages); */
	/* printc("sizeof ck_ring_mpsmcbuffer_ring %d\n", */
	/*        sizeof(struct ck_ring_mpsmcbuffer_ring)); */
	/* printc("sizeof mpsmc_entry %d\n",sizeof(struct mpsmc_entry)); */
	/* int capacity = CK_RING_CAPACITY(mpsmcbuffer_ring, (CK_RING_INSTANCE(mpsmcbuffer_ring) *)((void *)mpsmc_ring)); */
	/* printc("SMC:the mpsmc ring cap is %d\n\n", capacity); */

	return 0;
err:
	return -1;
}

vaddr_t
llog_multiplexer_init(spdid_t spdid, vaddr_t addr, int npages)
{
	vaddr_t ret = 0;

	assert(spdid == MULTIPLEXER_SPD);
	/* MULTIPLEXER_THD = cos_get_thd_id(); */
	/* printc("set multiplexer thread to be %d\n", MULTIPLEXER_THD); */

	LOCK();

	if (mpsmc_setuprb(spdid, addr, npages)) {
		printc("failed to setup shared rings\n");
		goto done;
	}

	ret = addr;
done:
	UNLOCK();
	return ret;
}

typedef void (*crt_thd_fn_t)(void);

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	/* printc("thd %d passed in arg1 %d, arg2 %d arg3 %d\n", cos_get_thd_id(), */
	/*        (int)arg1, (int)arg2, (int)arg3); */
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		lmgr_initialize(); break;
	case COS_UPCALL_CREATE:
		/* if ((int)arg2 == 1) lllog_loop(); */
		/* if ((int)arg2 == 2) lllog_cra_loop(); */
		lllog_loop();
		((crt_thd_fn_t)arg1)();
		break;
	case COS_UPCALL_DESTROY:
		printc("DESTROY thd %d\n", cos_get_thd_id());
		mon_assert(0); break;
	default:
		return;
	}
	return;
}
