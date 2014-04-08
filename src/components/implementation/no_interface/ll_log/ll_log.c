#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <log_process.h>   // check all constraints

#define LM_SYNC_PERIOD 50
static unsigned int lm_sync_period;
static struct evt_entry last_evt_entry;  // to continue from last time on evt_ring in some spd

/* struct evt_heap { */
/* 	struct heap h; */
/* 	void *ptr[MAX_NUM_SPDS]; */
/* 	char a; */
/* }; */

/* struct evt_heap evt_h; */
/* struct heap *hp; */

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

static struct logmon_info *
find_earliest_entry_spd()
{
	int i;
        struct logmon_info *ret = NULL, *spdmon;
	unsigned long long ts = LLONG_MAX;
	CK_RING_INSTANCE(logevt_ring) *evtring;
	
	struct evt_entry *entry;
	for (i = 1; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		assert(spdmon);
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		if (!evtring) continue;

		entry = &spdmon->first_entry;
		if (!entry->from_spd &&
		    (!CK_RING_DEQUEUE_SPSC(logevt_ring, evtring, entry))) {
			continue;
		}

		/* heap_add(hp, entry); */
		/* heap_adjust(hp, entry->index); */
		if (entry->time_stamp < ts) {
			ts = entry->time_stamp;
			ret = spdmon;
		}
	}
	
	/* ret = heap_highest(hp); */
	return ret;
}

static struct evt_entry *
find_next_entry(struct evt_entry *evt)
{
	struct evt_entry *ret = NULL;
        struct logmon_info *spdmon;
	CK_RING_INSTANCE(logevt_ring) *evtring;

	assert(evt->from_spd);
	memset(evt, 0, sizeof(struct evt_entry));

	spdmon = find_earliest_entry_spd();
	if (!spdmon) return NULL;

	ret = &spdmon->first_entry;
	assert(ret && ret->from_spd);
	printc("<<< find next ..... >>>\n");
	print_evt_info(ret);
	
	return ret;
}

static int
constraint_check(struct evt_entry *entry)
{
	assert(entry);

	check_timing(entry);

	return 1;
}

static void
walk_through_events()
{
        struct logmon_info *spdmon = NULL;

	struct evt_entry *entry_curr = NULL, *entry_next = NULL;
	struct evt_entry *last_entry = NULL;
	struct thd_trace *thd_trace_list;

	spdmon = find_earliest_entry_spd();
	if (!spdmon) return;  // no event happens since last process

	struct evt_entry *evt = &spdmon->first_entry;
	assert(evt && evt->from_spd);
	
	/* printc("<< The earliest event >>\n"); */
	/* print_evt_info(evt); */
	
	do {
		constraint_check(evt);
	} while ((evt = find_next_entry(evt)));

	return;
}

static int
llog_process(spdid_t spdid)
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	int i;

	printc("llog_process is called by thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);

	/* LOCK(); */

	walk_through_events();
	/* llog_report(); */

	/* UNLOCK(); */

	return 0;
}

/* 
   External function for initialization of thread priority. This
   should only be called when a thread is created and the priority is
   used to detect the priority inversion, scheduling decision,
   deadlock, etc.   
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

/* external function for initialization of shared RB */
vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_init thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
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

unsigned int
llog_get_syncp(spdid_t spdid)
{
	assert(spdid);
	return LM_SYNC_PERIOD;
}

static inline void
log_mgr_init(void)
{
	int i;
	if (cos_get_thd_id() == MONITOR_THD) {
		llog_process(0);
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
	}
		
	memset(&last_evt_entry, 0, sizeof(struct evt_entry));
		
	init_thread_trace();

	/* hp = (struct heap *)&evt_h; */
	/* hp->max_sz = MAX_NUM_SPDS+1; */
	/* hp->e = 1; */
	/* hp->c = evt_cmp; */
	/* hp->u = evt_update; */
	/* hp->data = (void *)&hp[1]; */
		
	return;
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
