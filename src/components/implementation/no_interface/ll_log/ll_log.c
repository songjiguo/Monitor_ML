#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <log_process.h>   // check all constraints

extern int logmgr_active(spdid_t spdid);
#define LM_SYNC_PERIOD 50   // period for asynchronous processing

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

static void
lmgr_action()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

	printc("process log now!!!!!\n");

	rdtscll(lpc_start);
	evt = find_next_evt(NULL);
	if (!evt) goto done;
	do {
		constraint_check(evt);
	} while ((evt = find_next_evt(evt)));
	printc("process log done!!!!!\n");
done:
	rdtscll(lpc_end);
	lpc_last = lpc_end - lpc_start;
	update_proc(lpc_last);

	return;
}

/* Initialize log manager here...do this only once */
static void
lmgr_initialize(void)
{
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
		memset(&logmon_info[i].first_entry, 0, sizeof(struct evt_entry));

		// initialize the heap entry
		es[i].ts = 0; 
		es[i].spdid = i;
	}
		
	init_thread_trace();

        // alloc a page for heap operation (assume a page for now)
	h = log_heap_alloc(MAX_NUM_SPDS, evtcmp, evtupdate);
	assert(h);

        // local RB for contention only
	evt_ring = (CK_RING_INSTANCE(logevt_ring) *)llog_get_page();
	assert(evt_ring);
	logmon_info[cos_spd_id()].mon_ring = (vaddr_t)evt_ring;

	lpc_start = lpc_end = lpc_last = 0;

	return;
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		lmgr_initialize(); break;
	case COS_UPCALL_LOG_PROCESS:
		lmgr_action(); break;
	default:
		return;
	}
	return;
}

/***********************/
/*  External functions  */
/***********************/

/* activate to process log */
int
llog_process()
{
	return logmgr_active(cos_spd_id());
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
   RB, the lock is needed for that RB again) */

int
llog_contention(spdid_t spdid, unsigned int owner)
{
        struct evt_entry cont_evt;
        /* printc("Contention!!! spd %d owner %d contender %d\n", spdid, owner, contender); */
	assert(spdid != cos_spd_id());

        LOCK();

	evt_enqueue(owner, spdid, spdid, 0, 0, EVT_RBCONTEND);

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

/* Initialize shared RB. Called from each spd when the first event
 * happens in that spd */
vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_init thd %d (passed spd %d for RB)\n", cos_get_thd_id(), spdid);
	assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon);

	assert(!spdmon->mon_ring && !spdmon->cli_ring);

	LOCK();
	/* printc("llog_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid); */
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

