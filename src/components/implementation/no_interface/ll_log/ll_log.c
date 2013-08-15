#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print

#define LM_SYNC_PERIOD 50
static unsigned int lm_sync_period;

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct logmon_cs lmcs;

struct thd_trace thd_trace[MAX_NUM_THREADS];

static void 
init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id = i;
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].alpha_exec[j]    = 0;
			thd_trace[i].last_exec[j]     = 0;
			thd_trace[i].avg_exec[j]      = 0;
			thd_trace[i].tot_wcet[j]      = 0;
			thd_trace[i].tot_upto_wcet[j] = 0;
			thd_trace[i].upto_wcet[j]     = 0;
			thd_trace[i].this_wcet[j]     = 0;
			thd_trace[i].wcet[j]	      = 0;
			thd_trace[i].tot_spd_exec[j]  = 0;
			thd_trace[i].tot_inv[j]	      = 0;
		}
		thd_trace[i].tot_exec = 0;
		thd_trace[i].trace_head = NULL;

		for (j = 0; j < MAX_SPD_TRACK; j++) {
			thd_trace[i].all_spd_trace[j].spdid = 0;
			thd_trace[i].all_spd_trace[j].enter = 0;
			thd_trace[i].all_spd_trace[j].leave = 0;
		}
		thd_trace[i].total_pos = 0;
		for (j = 0; j < MAX_SERVICE_DEPTH; j++) {
			thd_trace[i].spd_trace[j] = 0;
		}
		thd_trace[i].curr_pos = 0;
	}
	return;
}


// set up shared ring buffer between spd and log manager, a page for now
static int 
shared_ring_setup(spdid_t spdid, vaddr_t cli_addr, int type) {
        struct logmon_info *spdmon = NULL;
        vaddr_t log_ring, cli_ring = 0;
	char *addr, *hp;

	assert(cli_addr);
        assert(spdid);
	addr = cos_get_heap_ptr();
	if (!addr) {
		printc("fail to allocate a page from the heap\n");
		goto err;
	}
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
	if ((vaddr_t)addr != __mman_get_page(cos_spd_id(), (vaddr_t)addr, 0)) {
		printc("fail to get a page in logger\n");
		goto err;
	}

	if (type == 0) {   // events
		spdmon = &logmon_info[spdid];
		assert(spdmon->spdid == spdid);
		spdmon->mon_ring = (vaddr_t)addr;
	} else {           // cs
		lmcs.mon_csring = (vaddr_t)addr;
	}

	cli_ring = cli_addr;
        if (unlikely(cli_ring != __mman_alias_page(cos_spd_id(), (vaddr_t)addr, spdid, cli_ring))) {
                printc("alias rings %d failed.\n", spdid);
		assert(0);
        }

	// FIXME: PAGE_SIZE - sizeof((CK_RING_INSTANCE(logevts_ring))	
	if (type == 0) {
		spdmon->cli_ring = cli_ring;
		CK_RING_INIT(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE)/ sizeof(struct event_info)));
	} else {
		lmcs.sched_csring = cli_ring;
		CK_RING_INIT(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE)/ sizeof(struct cs_info)));
	}

        return 0;
err:
        return -1;
}

static void 
copy_evt_entry(struct event_info *to, struct event_info *from)
{
	assert(to);
	assert(from);
	
	to->thd_id     = from->thd_id;
	to->from_spd   = from->from_spd;
	to->dest_info  = from->dest_info;
	to->time_stamp = from->time_stamp;
	
	return;
}


/* the first item should always be the most recent ones that need to be processed */
static int 
get_head_entry(int thdid)
{
	int i, ret = 0;
        struct logmon_info *spdmon;
	CK_RING_INSTANCE(logevts_ring) *evtring;
	struct event_info evt_entry;
	unsigned long long tmp_ts = LLONG_MAX;

	for (i = 1; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		if (!evtring) continue;

		if (spdmon->last_stop.thd_id == 0) {
			if (!CK_RING_DEQUEUE_SPSC(logevts_ring, evtring, &evt_entry)) {
				continue;
			}
			copy_evt_entry(&spdmon->last_stop, &evt_entry);
		}

		/* printc("last stop (spd %d) thd %d\n", i, spdmon->last_stop->thd_id); */
		if (spdmon->last_stop.thd_id != thdid) continue;
		if (spdmon->last_stop.time_stamp < tmp_ts) {
			tmp_ts = spdmon->last_stop.time_stamp;
			ret = i;
		}
	}

	return ret;
}


static void 
update_stack_info(struct thd_trace *thd_trace_list, int spdid)
{
	assert(thd_trace_list);
		
	if (spdid) {
		assert (thd_trace_list->curr_pos < MAX_SERVICE_DEPTH);
		thd_trace_list->spd_trace[thd_trace_list->curr_pos++] = spdid;
		assert (thd_trace_list->total_pos < MAX_SPD_TRACK);
		thd_trace_list->all_spd_trace[thd_trace_list->total_pos++].spdid = spdid;
	} else {
		assert (thd_trace_list->curr_pos > 0);
		thd_trace_list->spd_trace[--thd_trace_list->curr_pos] = 0;
	}
	
	return;
}

// Decide what the next spd will be, based on where the execution is within invocation 
static int
find_next_spd(struct event_info *entry)
{
	int dest, ret = 0;
	struct thd_trace *thd_trace_list;

	assert(entry);
	dest = entry->dest_info;

	thd_trace_list = &thd_trace[entry->thd_id];
	assert(thd_trace_list);

	// dest: 0(need a returned spd) or spdid or cap_no 
	if (dest > MAX_NUM_SPDS) { // dest_info must be cap_no
		if ((ret = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) {
			printc("can not find ser spd\n");
			BUG();
		}
		update_stack_info(thd_trace_list, ret);		
		return ret;
	}
	return dest;
}


static void 
update_timing_info(struct thd_trace *thd_trace_list, struct event_info *entry, int curr_spd)
{
	unsigned long long exec_tmp = 0;

	assert(thd_trace_list);
	assert(entry);
	assert(curr_spd);

	if (unlikely(!thd_trace_list->alpha_exec[curr_spd])) {
		thd_trace_list->alpha_exec[curr_spd] = entry->time_stamp;
		thd_trace_list->last_exec[curr_spd] = entry->time_stamp;
	} else {
		if (entry->dest_info && entry->dest_info == curr_spd) {
			thd_trace_list->last_exec[curr_spd] = entry->time_stamp;
		} else {
			exec_tmp = entry->time_stamp - thd_trace_list->last_exec[curr_spd];
			assert(exec_tmp);
			thd_trace_list->tot_exec += exec_tmp;
			thd_trace_list->tot_spd_exec[curr_spd] += exec_tmp;
			thd_trace_list->this_wcet[curr_spd] += exec_tmp;
		}
	}

        // this must be the return from server side, then update the wcet in this spd
	if (!entry->dest_info) {
		// update the wcet of this thd up to this spd
		exec_tmp = entry->time_stamp - thd_trace_list->alpha_exec[curr_spd];
		thd_trace_list->alpha_exec[curr_spd] = 0;
		if (exec_tmp > thd_trace_list->upto_wcet[curr_spd])
			thd_trace_list->upto_wcet[curr_spd] = exec_tmp;
		/* printc(" thd %d << wcet %llu >>\n ",thd_trace_list->thd_id, exec_tmp);		 */
		thd_trace_list->tot_upto_wcet[curr_spd] += thd_trace_list->upto_wcet[curr_spd];

		// update the wcet of this thd in this spd
		if (thd_trace_list->this_wcet[curr_spd] > thd_trace_list->wcet[curr_spd])
			thd_trace_list->wcet[curr_spd] = thd_trace_list->this_wcet[curr_spd];
		thd_trace_list->tot_wcet[curr_spd] += thd_trace_list->wcet[curr_spd];

		// reset local wcet record and increase the invs to this spd
		thd_trace_list->this_wcet[curr_spd] = 0;
		thd_trace_list->tot_inv[curr_spd]++;
	}

	/* printc("thd %d (total exec %llu, wcet %llu) in spd %d \n", thd_trace_list->thd_id, thd_trace_list->tot_spd_exec[curr_spd], thd_trace_list->wcet[curr_spd], curr_spd); */
	/* printc("thd %d (total wcet %llu) upto spd %d (invocations %d)\n\n ", thd_trace_list->thd_id, thd_trace_list->tot_wcet[curr_spd], curr_spd, thd_trace_list->tot_inv[curr_spd]); */
	
	return;
}

static void
walk_through_events()
{
	int i, j, thd_id, size;
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	CK_RING_INSTANCE(logevts_ring) *evts_ring;

	struct event_info entry_curr, entry_next;
	struct thd_trace *thd_trace_list;


	int cs_size;
	struct cs_info cs_entry_curr, cs_entry_next;
	int curr_spd;
	CK_RING_INSTANCE(logcs_ring) *csring;

	memset(&cs_entry_curr, 0, sizeof(struct cs_info));
	memset(&cs_entry_next, 0, sizeof(struct cs_info));
	memset(&entry_curr, 0, sizeof(struct event_info));
	memset(&entry_next, 0, sizeof(struct event_info));

	init_thread_trace();

	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	assert(csring);
	cs_size = CK_RING_SIZE(logcs_ring, csring);

	while(cs_size--) {
		if (cs_entry_next.curr_thd) {
			cs_entry_curr.curr_thd = cs_entry_next.curr_thd;
			cs_entry_curr.next_thd = cs_entry_next.next_thd;
			cs_entry_curr.time_stamp = cs_entry_next.time_stamp;
		} else {
			if (!CK_RING_DEQUEUE_SPSC(logcs_ring, csring, &cs_entry_curr)){
				break;
			}
		}
		
		if (!CK_RING_DEQUEUE_SPSC(logcs_ring, csring, &cs_entry_next)){
			break;
		}

		thd_trace_list = &thd_trace[cs_entry_curr.next_thd];
		assert(thd_trace_list);

		unsigned long long s,e;
		int cs_thd = cs_entry_curr.next_thd;
		s = cs_entry_curr.time_stamp;
		e = cs_entry_next.time_stamp;

		printc("(cs curr thd %d -- ", cs_entry_curr.curr_thd);
		printc("cs next thd %d)\n", cs_thd);
		curr_spd = get_head_entry(cs_thd);
		if (!curr_spd) {
			continue;
		}

		update_stack_info(thd_trace_list, curr_spd);

		while (1) {   // for now, only deal with threads has home spd
			/* printc("spd %d\n", curr_spd); */
			spdmon = &logmon_info[curr_spd];
			assert(spdmon);

			if (spdmon->last_stop.thd_id) {
				copy_evt_entry(&entry_curr, &spdmon->last_stop);
				spdmon->last_stop.thd_id = 0;
			} else {
				evts_ring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
				if(!evts_ring) break;
				if (!CK_RING_DEQUEUE_SPSC(logevts_ring, evts_ring, &entry_curr)) {
					break;
				}
			}

			if (entry_curr.time_stamp < s || 
			    entry_curr.time_stamp >= e || 
			    entry_curr.thd_id != cs_thd) {
				copy_evt_entry(&spdmon->last_stop, &entry_curr);
				break;
			}

			/* print_evtinfo(&entry_curr); */
			update_timing_info(thd_trace_list, &entry_curr, curr_spd);
			
			if (!(curr_spd = find_next_spd(&entry_curr))) {
				if (!(curr_spd = get_head_entry(cs_thd))) break;
				update_stack_info(thd_trace_list, 0);
				update_stack_info(thd_trace_list, curr_spd);
			}
		}
	}
	
	return;
}

static void 
walk_through_cs()
{
	int i, size;
	CK_RING_INSTANCE(logcs_ring) *csring;
	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	if (csring) {
		size = CK_RING_SIZE(logcs_ring, csring);
		struct cs_info *cs_entry;
		for (i = 0; i < size; i++){
			cs_entry = &csring->ring[i];
			print_csinfo(cs_entry);
		}
	}
	
	return;
}

/* empty the context switch ring buffer  */
static void 
llog_csring_empty()
{
	int i, size;
	struct cs_info cs_entry;
	CK_RING_INSTANCE(logcs_ring) *csring;
	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	if (csring) {
		while (CK_RING_DEQUEUE_SPSC(logcs_ring, csring, &cs_entry));
	}
	return;
}

/* empty the events ring buffer for spd */
static void 
llog_evtsring_empty(spdid_t spdid)
{
	int i, size;
        struct logmon_info *spdmon;
	CK_RING_INSTANCE(logevts_ring) *evtring;
	struct event_info evt_entry;

        assert(spdid);
	spdmon = &logmon_info[spdid];
	evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
	if (evtring) {
		while (CK_RING_DEQUEUE_SPSC(logevts_ring, evtring, &evt_entry));
	}

	return;
}

static void 
llog_report()
{
	printc("\n<<print tracking info >>\n");
	int i, j;
	unsigned long long exec, wcet;

	for (i = 1; i < MAX_NUM_THREADS; i++) {
		report_thd_stack_list(i);
		for (j = 1; j < MAX_NUM_SPDS; j++) {
			exec = report_avg_in_wcet(i, j);
			exec = report_avg_upto_wcet(i, j);
		}
		exec = report_tot_exec(i);
		wcet = report_tot_wcet(i);
	}
	printc("\n");
	return;
}

unsigned int
llog_get_syncp(spdid_t spdid)
{
	return LM_SYNC_PERIOD;
}


int
llog_process(spdid_t spdid)
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	int i;

	printc("llog_process is called by thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
	LOCK();

	walk_through_events();
	llog_report();

	/* llog_evtsring_empty(spdid); */
	/* llog_csring_empty(); */

	UNLOCK();
	return 0;
}

vaddr_t
llog_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_init thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
	LOCK();

	assert(spdid);
	spdmon = &logmon_info[spdid];
	
	assert(spdmon);
	// limit one RB per component here, not per interface
	if (spdmon->mon_ring && spdmon->cli_ring){
		ret = spdmon->mon_ring;
		goto done;
	}

	/* printc("llog_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid); */
	// 1 for cs rb and 0 for event rb
	if (shared_ring_setup(spdid, addr, 0)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(spdmon->cli_ring);
	ret = spdmon->cli_ring;
done:
	UNLOCK();
	return ret;
}

vaddr_t
llog_cs_init(spdid_t spdid, vaddr_t addr)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	printc("llog_cs_init thd %d (passed spd %d)\n", cos_get_thd_id(), spdid);
	LOCK();

	assert(spdid == LLBOOT_SCHED);

	// limit one RB per component here, not per interface
	if (lmcs.mon_csring && lmcs.sched_csring){
		ret = lmcs.sched_csring;
		goto done;
	}

	printc("llog_cs init...(thd %d spd %d)\n", cos_get_thd_id(), spdid);
	// 1 for cs rb and 0 for event rb
	if (shared_ring_setup(spdid, addr, 1)) {
		printc("failed to setup shared rings\n");
		goto done;
	}
	assert(lmcs.sched_csring);
	ret = lmcs.sched_csring;
done:
	UNLOCK();
	return ret;
}

static inline void
log_spin(void)
{
	printc("log thread is spinning here\n");
	while(1);
	return;
}

// do this once
static inline void
log_init(void)
{
	int i;
	printc("ll_log (spd %ld) has thread %d\n", cos_spd_id(), cos_get_thd_id());
	// Initialize log manager here...
	memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
	for (i = 0; i < MAX_NUM_SPDS; i++) {
		logmon_info[i].spdid = i;
		logmon_info[i].mon_ring = 0;
		logmon_info[i].cli_ring = 0;
		/* logmon_info[i].last_stop = (struct event_info *)malloc(sizeof(struct event_info)); */
	}
	
	memset(&lmcs, 0, sizeof(struct logmon_cs));
	lmcs.mon_csring = 0;
	lmcs.sched_csring = 0;

	return;
}


void 
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		log_init(); break;
	case COS_UPCALL_LOG_PROCESS:
		log_spin(); break;
	default:
		BUG(); return;
	}

	return;
}
