#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <log_process.h>   // check all constraints

#define LM_SYNC_PERIOD 50

volatile int prev_mthd, processed;
int logmgr_thd;

/* contention_omission ring buffer (corb) entry */
struct corb_entry {
	unsigned int owner, contender;
	int spdid, flag;
	unsigned long long time_stamp;
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __CORBRING_DEFINED
#define __CORBRING_DEFINED
CK_RING(corb_entry, corb_ring);
CK_RING_INSTANCE(corb_ring) *corb_evt_ring;
#endif

/* 
   Set up ring buffer (assume a page for now)
   1) cli_addr = 0 : the contention_omission RB
   2) cli_addr > 0 : shared RB between spd and log_mgr
*/
static int 
lmgr_setuprb(spdid_t spdid, vaddr_t cli_addr) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;
	
	assert(spdid);
	addr = llog_get_page();
	if (!addr) goto err;
	
	if (!cli_addr) {
		corb_evt_ring = (void *)addr;
		CK_RING_INIT(corb_ring, (CK_RING_INSTANCE(corb_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_corb_ring))/sizeof(struct corb_entry)));
	} else {
		assert(cli_addr > 0);
		spdmon = &logmon_info[spdid];
		assert(spdmon->spdid == spdid);
		spdmon->mon_ring = (vaddr_t)addr;
		cli_ring = cli_addr;
		if (unlikely(cli_ring != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_ring))) {
			printc("alias rings %d failed.\n", spdid);
			assert(0);
		}
		// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
		spdmon->cli_ring = cli_ring;
		CK_RING_INIT(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_logevt_ring))/sizeof(struct evt_entry)));
	}
	
	
	return 0;
err:
	return -1;
}

static void
lmgr_action()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

	assert(!processed);

	evt = find_next_evt(NULL);
	if (!evt) return;
	do {
		constraint_check(evt);
	} while ((evt = find_next_evt(evt)));

	return;
}


static void
lmgr_loop(void) { 
	while(1) {
		assert(cos_get_thd_id() == logmgr_thd);
		
		printc("Jiguo:core %ld: monitor thread %d\n", cos_cpuid(), logmgr_thd);
		int     pmthd	    = prev_mthd;
		int     processed_f = processed;
		if (processed_f) {
			assert(pmthd);
			processed = 0;   // start process
			lmgr_action();
		} else {
			assert(pmthd && pmthd != logmgr_thd);
			prev_mthd = 0;
			processed = 1;   // end process
			cos_switch_thread(pmthd, 0);
		}
	}

	return;
}

/* Initialize log manager here...do this only once */
static inline void
lmgr_initialize(void)
{
	/* /\* // ll_log is going to create/manage its threads *\/ */
	/* if (parent_sched_child_cntl_thd(cos_spd_id())) BUG(); */
	/* if (cos_sched_cntl(COS_SCHED_EVT_REGION, 0, (long)PERCPU_GET(cos_sched_notifications))) BUG(); */
	/* logmgr_thd = cos_create_thread((int)lmgr_loop, (int)0, 0); */
	/* assert(logmgr_thd >= 0); */
	/* printc("logmgr_thd %d is created here by %d\n", */
	/*        logmgr_thd, cos_get_thd_id()); */

	printc("logmgr is initialized here by %d\n", cos_get_thd_id());

	// init log manager
	memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
	int i, j;
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		logmon_info[i].spdid	= i;
		logmon_info[i].mon_ring = 0;
		logmon_info[i].cli_ring = 0;

		for (j = 0; j < MAX_STATIC_CAP; j++) {
			logmon_info[i].capdest[j].dest  = 0;
			logmon_info[i].capdest[j].capno = 0;
		}
		/* logmon_info[i].first_entry = (struct evt_entry *)malloc(sizeof(struct evt_entry)); */
		memset(&logmon_info[i].first_entry, 0, sizeof(struct evt_entry));
		// initialize the heap entry
		es[i].ts = 0; 
		es[i].spdid = i;
	}
		
	init_thread_trace();

        // get a page for heap (assume a page for now)
	h = log_heap_alloc(MAX_NUM_SPDS, evtcmp, evtupdate);
	assert(h);
	
	corb_evt_ring = NULL;
	processed = 1;

	return;
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		lmgr_initialize(); break;
	default:
		/* printc("thread here is %d\n", cos_get_thd_id()); */
		return;
	}

	return;
}

/***********************/
/*  External functions  */
/***********************/

/* activate logmgr_thd to process the thread (sync or async) */
static int first = 1;
int
llog_process()
{
	prev_mthd     = cos_get_thd_id();
	printc("logmgr_thd is activated here ...!!!!\n");
	/* while (prev_mthd == cos_get_thd_id()) cos_switch_thread(logmgr_thd, 0); */
	return 0;
}

/* Return the periodicity for asynchronously monitor processing  */
unsigned int
llog_getsyncp()
{
	return LM_SYNC_PERIOD;
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

	printc("llog_init_prio thd %d (passed from spd %d thd %d with prio %d)\n", 
	       cos_get_thd_id(), spdid, thd_id, prio);

	/* LOCK(); */
	ttl = &thd_trace[thd_id];
	assert(ttl);
	ttl->prio = prio;

	/* UNLOCK(); */

	return 0;
}

/* Initialize shared RB. Called from each spd when the first event
 * happens in that spd */
vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_init thd %d (passed spd %d for RB)\n", cos_get_thd_id(), spdid);
	/* LOCK(); */

	assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon);

	// limit one RB per component here, not per interface
	if (spdmon->mon_ring && spdmon->cli_ring){
		ret = spdmon->mon_ring;
		goto done;
	}

	/* printc("llog_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid); */
	if (lmgr_setuprb(spdid, addr)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(spdmon->cli_ring);
	ret = spdmon->cli_ring;
done:
	/* UNLOCK(); */
	return ret;
}

