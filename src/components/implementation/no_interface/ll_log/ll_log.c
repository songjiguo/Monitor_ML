#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <log_process.h>   // check all constraints

#define LM_SYNC_PERIOD 50

// set up shared ring buffer between spd and log manager, a page for now
static int 
shared_ring_setup(spdid_t spdid, vaddr_t cli_addr) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;

	assert(cli_addr);
        assert(spdid);

	addr = llog_get_page();
	if (!addr) goto err;

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
	
        return 0;
err:
        return -1;
}

static int
llog_process()
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	struct evt_entry *evt;

	printc("llog_process is called by thd %d\n", cos_get_thd_id());

	evt = find_next_evt(NULL);
	if (!evt) return 0;
	do {
		/* constraint_check(evt); */
	} while ((evt = find_next_evt(evt)));

	return 0;
}

static inline void
log_mgr_init(void)
{
	int i;
	if (cos_get_thd_id() == MONITOR_THD) {
		llog_process();
		return;
	}

	/* do this once */
	printc("ll_log (spd %ld) init as thread %d\n", cos_spd_id(), cos_get_thd_id());
	// Initialize log manager here...
	memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		logmon_info[i].spdid = i;
		logmon_info[i].mon_ring = 0;
		logmon_info[i].cli_ring = 0;
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

	return;
}

/***********************/
/*  External functions  */
/***********************/

/* Initialize thread priority. This should only be called when a
   thread is created and the priority is used to detect the priority
   inversion, scheduling decision, deadlock, etc.
 */
int llog_init_prio(spdid_t spdid, unsigned int thd_id, unsigned int prio)
{
        struct logmon_info *spdmon;
	struct thd_trace *thd_trace_list;

	assert(spdid == SCHED_SPD);
	printc("thd is %d, prio is %d\n", thd_id, prio);
	assert(thd_id > 0);
	assert(prio <= PRIO_LOWEST);

	printc("llog_init_prio thd %d (passed from spd %d thd %d with prio %d)\n", 
	       cos_get_thd_id(), spdid, thd_id, prio);
	/* LOCK(); */

	thd_trace_list = &thd_trace[thd_id];
	assert(thd_trace_list);
	
	thd_trace_list->prio = prio;

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
	if (shared_ring_setup(spdid, addr)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(spdmon->cli_ring);
	ret = spdmon->cli_ring;
done:
	/* UNLOCK(); */
	return ret;
}

/* Return the periodicity for asynchronously monitor processing  */
unsigned int
llog_get_syncp(spdid_t spdid)
{
	assert(spdid);
	return LM_SYNC_PERIOD;
}

void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		log_mgr_init(); break;
	default:
		BUG(); return;
	}

	return;
}
