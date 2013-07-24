/**
 Jiguo: log monitor for latent fault: tracking all events
 */

#include <cos_synchronization.h>
#include <cos_component.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <mem_mgr_large.h>
#include <valloc.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <logmonitor.h>
#include <monitor.h>

#include <cs_monitor.h>
#include <log_publisher.h>

static cos_lock_t lm_lock;
#define LOCK() if (lock_take(&lm_lock)) BUG();
#define UNLOCK() if (lock_release(&lm_lock)) BUG();

#define LM_SYNC_PERIOD 200
static unsigned int lm_sync_period;

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct logmon_cs lmcs;

struct thd_trace thd_trace[MAX_NUM_THREADS];
/* struct spd_trace spd_trace[MAX_NUM_SPDS]; */

static void init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id = i;
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].exec[j] = 0;
			thd_trace[i].wcet[j] = 0;
			thd_trace[i].tot_spd_exec[j] = 0;
		}
		thd_trace[i].tot_exec = 0;
		thd_trace[i].tot_wcet = 0;
		thd_trace[i].pair = 0;
		/* thd_trace[i].entry  = NULL; */
		thd_trace[i].trace_head = NULL;
	}
	return;
}

// print info for an event
static void print_evtinfo(struct event_info *entry)
{
	assert(entry);
	printc("event id %p\n", (void *)entry->event_id);
	printc("call or return? %d\n", entry->call_ret);
	printc("thd id %d\n", entry->thd_id);
	printc("caller spd %d\n", entry->spd_id);
	printc("time_stamp %llu\n\n", entry->time_stamp);
	
	return;
}

// print context switch info
static void print_csinfo(struct cs_info *csentry)
{
	assert(csentry);
	printc("curr thd id %d\n", csentry->curr_thd);
	printc("switch to thd id %d\n", csentry->next_thd);
	printc("time_stamp %llu\n\n", csentry->time_stamp);
	
	return;
}

// print all event info for a thread
static void print_thd_history(int thd_id)
{
	assert(thd_id);
	struct thd_trace *evt_list;
	evt_list = &thd_trace[thd_id];
	assert(evt_list);
	if (evt_list->trace_head) {
		struct event_info *entry_iter;
		for (entry_iter = LAST_LIST((struct event_info *)evt_list->trace_head, next, prev) ;
		     entry_iter!= evt_list->trace_head;
		     entry_iter = LAST_LIST(entry_iter, next, prev)) {
			print_evtinfo(entry_iter);
		}
	}

	return;
}

// total wcet time of a thread to the point when the check is being done
static unsigned long long report_tot_wcet(int thd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	evt_list = &thd_trace[thd_id];
	if (!evt_list->trace_head) return 0;
	printc("thd %d total wcet time : %llu\n", thd_id, evt_list->tot_wcet);
	return evt_list->tot_wcet;
}

// total exec time of a thread to the point when the check is being done within a spd
static unsigned long long report_tot_spd_exec(int thd_id, int spd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spd_id >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_spd_exec[spd_id] || !evt_list->wcet[spd_id]) return 0;
	printc("thd %d (in spd %d) total exec --> %llu\n", thd_id, spd_id, evt_list->tot_spd_exec[spd_id]);
	return evt_list->tot_spd_exec[spd_id];
}

// total exec time of a thread to the point when the check is being done
static unsigned long long report_tot_exec(int thd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_exec) return 0;
	printc("thd %d overall exec --> %llu\n\n", thd_id, evt_list->tot_exec);
	return evt_list->tot_exec;
}

// wcet in a spd
static unsigned long long report_spd_wcet(int thd_id, int spd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spd_id >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->wcet[spd_id]) return 0;
	printc("thd %d (in spd %d) wcet --> %llu\n", thd_id, spd_id, evt_list->wcet[spd_id]);
	return evt_list->wcet[spd_id];
}

// average exec time in a spd
static unsigned long long report_spd_exec(int thd_id, int spd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spd_id >= 0);

	evt_list = &thd_trace[thd_id];
	assert(evt_list);
	if (!evt_list->exec[spd_id] || !evt_list->wcet[spd_id]) return 0;

	printc("thd %d (in spd %d) avg exec --> %llu\n", thd_id, spd_id, evt_list->exec[spd_id]);

	return evt_list->exec[spd_id];
}

static void lm_report()
{
	printc("\n<<print tracking info >>\n");
	int i, j;
	unsigned long long exec;

	for (i = 1; i < MAX_NUM_THREADS; i++) {
		for (j = 1; j < MAX_NUM_SPDS; j++) {
			exec = report_spd_exec(i, j);
			exec = report_spd_wcet(i, j);
			exec = report_tot_spd_exec(i, j);
		}
		exec = report_tot_exec(i);
	}
	return;
}

// set up shared ring buffer between spd and log manager
static int shared_ring_setup(spdid_t spdid) {
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;

        assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon->spdid == spdid);

        mon_ring = (vaddr_t)alloc_page();
        if (!mon_ring) {
                printc(" alloc ring buffer failed in logmonitor %ld.\n", cos_spd_id());
                goto err;
        }
	spdmon->mon_ring = mon_ring;

        cli_ring = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
        if (unlikely(!cli_ring)) {
                printc("vaddr alloc failed in client spd %d.\n", spdid);
                goto err_cli;
        }
  
        if (unlikely(cli_ring != mman_alias_page(cos_spd_id(), mon_ring, spdid, cli_ring))) {
                printc("alias rings %d failed.\n", spdid);
                goto err_cli_alias;
        }
        spdmon->cli_ring = cli_ring;

	/* printc("mon:mon_ring addr %p\n", (void *)spdmon->mon_ring); */
	/* printc("mon:cli ring addr %p\n", (void *)spdmon->cli_ring); */

        /* Initialize the ring buffer. Passing NULL because we use
         * continuous ring (struct + data region) */
        CK_RING_INIT(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring), NULL,
                     get_powerOf2((PAGE_SIZE)/ sizeof(struct event_info)));

	if (CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring)) != 0) {
		printc("Ring should be empty: %u (cap %u)\n",
		       CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring)), 
		       CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring)));
		BUG();
	}

        return 0;

err_cli_alias:
        valloc_free(cos_spd_id(), spdid, (void *)cli_ring, 1);
err_cli:
        free_page((void *)mon_ring);
err:
        return -1;
}


// set up shared ring buffer between scheduler and log manager
static int shared_csring_setup() {
        vaddr_t mon_csring, sched_csring;
	char *addr, *hp;

        mon_csring = (vaddr_t)cos_get_vas_page();
        if (!mon_csring) {
                printc(" alloc ring buffer failed in logmonitor %ld.\n", cos_spd_id());
                goto err;
        }

	lmcs.mon_csring = mon_csring;

	/* printc("mon_spd %d mon_csring addr %p\n", cos_spd_id(), (void *)mon_csring); */
	sched_csring = sched_logpub_setup(cos_spd_id(), mon_csring);
	assert(sched_csring);

	/* printc("sched_csring addr %p\n", (void *)sched_csring); */
		
        lmcs.sched_csring = sched_csring;

	/* printc("mon:mon_ring addr %p\n", (void *)spdmon->mon_ring); */
	/* printc("mon:cli ring addr %p\n", (void *)spdmon->cli_ring); */

        CK_RING_INIT(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)mon_csring), NULL,
                     get_powerOf2((PAGE_SIZE)/ sizeof(struct cs_info)));

	/* if (CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring)) != 0) { */
	/* 	printc("Ring should be empty: %u (cap %u)\n", */
	/* 	       CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring)),  */
	/* 	       CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)mon_ring))); */
	/* 	BUG(); */
	/* } */

        return 0;

err:
        return -1;
}

static void walk_thd_events()
{
	int i, j, thd_id;
	int head, tail, size;
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	CK_RING_INSTANCE(logevts_ring) *wk_evts;

	struct event_info *entry;
	struct thd_trace *evt_list;

	unsigned long long exec_cal, exec_cal_prev;
	unsigned long long exec_prev, exec_curr;

	init_thread_trace();

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		exec_prev = exec_curr = exec_cal = exec_cal_prev = 0;
		
		spdmon = &logmon_info[i];
		wk_evts = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		if(!wk_evts) continue;

		size = CK_RING_SIZE(logevts_ring, wk_evts);
		/* head = wk_evts->c_head; */
		/* tail = wk_evts->p_tail; */
		/* printc("wk: spd id %d (head %d tail %d)\n", i, head, tail); */
		/* for (j = head; j < tail; j++){ */
		for (j = 0; j < size; j++){
			entry = &wk_evts->ring[j];
			evt_list = &thd_trace[entry->thd_id];
			if (unlikely(!evt_list->trace_head)) {
				evt_list->trace_head = (void *)entry;
				INIT_LIST(entry, next, prev);
			}
			else {
				assert(evt_list->trace_head);
				struct event_info *entry_tmp;
				entry_tmp = (struct event_info *)evt_list->trace_head;
				ADD_LIST(entry_tmp, entry, next, prev);
			}
			if (evt_list->pair == 1) {
				evt_list->pair = 0;
				exec_curr = entry->time_stamp;
				/* printc("entry->thd_id is %d (spd %d)\n", entry->thd_id, i); */
				exec_cal += exec_curr - exec_prev;

				evt_list->tot_exec += exec_cal;
				evt_list->tot_spd_exec[i] += exec_cal;
				/* evt_list->tot_wcet += evt_list->wcet[i];// ??? */

				if (entry->call_ret == INV_LOOP_END) {
					if (exec_cal > evt_list->wcet[i]) {
						evt_list->wcet[i] = exec_cal;
					}
					
					if (!exec_cal_prev) {
						evt_list->exec[i] = exec_cal;
						exec_cal_prev = exec_cal;
					}
					else {
						evt_list->exec[i] = (exec_cal + exec_cal_prev) >> 1;
						exec_cal_prev = exec_cal = 0;
					}
				}
			} else {
				evt_list->pair = 1;
				exec_prev = entry->time_stamp;
			}
		}
	}

	return;
}

static void walk_cs_events()
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
static void lm_csring_empty()
{
	int i, size;
	CK_RING_INSTANCE(logcs_ring) *csring;
	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	if (csring) {
		size = CK_RING_SIZE(logcs_ring, csring);
		struct cs_info *cs_entry;
		for (i = 0; i < size; i++){
			cs_entry = &csring->ring[i];
			moncs_dequeue(csring, cs_entry);
		}
	}
	
	return;
}

/* empty the events ring buffer for spd */
static void lm_evtsring_empty(spdid_t spdid)
{
	int i, size;
        struct logmon_info *spdmon;
	CK_RING_INSTANCE(logevts_ring) *evtring;

        assert(spdid);
	spdmon = &logmon_info[spdid];
	evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
	if (evtring) {
		size = CK_RING_SIZE(logevts_ring, evtring);
		struct event_info *evt_entry;
		for (i = 0; i < size; i++){
			evt_entry = &evtring->ring[i];
			printc("caller spd %d\n", evt_entry->spd_id);
			monevt_dequeue(evtring, evt_entry);
		}
	}

	return;
}

static int first = 0;

// some spds above logmonitor is periodically calling into and do the violation check here
void lm_sync_process()
{
	LOCK();

	/* walk_thd_events(); */
	// browse the event history for threads
	/* int i; */
	/* for (i = 1; i < MAX_NUM_THREADS; i++) print_thd_history(i); */
	
	/* walk_cs_events(); */
	/* lm_report(); */

	UNLOCK();
	return;
}

// Now only one thread to check the log, there can be multiple threads
// we can have something like lm_sync_period[NUM]
unsigned int lm_get_sync_period()
{
	assert(lm_sync_period);
	return lm_sync_period;
}


// call this when ring buffer of spd is full and need process
int lm_process(spdid_t spdid)
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	int i;

	LOCK();
	printc("lm_process is called\n");

	walk_thd_events();

	/* walk_cs_events(); */
	/* lm_csring_empty(); */
	/* walk_cs_events(); */

	/* lm_evtsring_empty(11); */

	lm_report();

	UNLOCK();
	return 0;
}

vaddr_t lm_init(spdid_t spdid)
{
	vaddr_t ret = 0;
        struct logmon_info *spdmon;

	LOCK();

	spdmon = &logmon_info[spdid];

	printc("lm_init...(thd %d spd %d)\n", cos_get_thd_id(), spdid);
	assert(spdid);
	if (shared_ring_setup(spdid)) {
		printc("failed to setup shared rings\n");
		BUG();
	}
	assert(spdmon->cli_ring);
	ret = spdmon->cli_ring;
	/* printc("cli_ring addr %p\n", (void *)ret); */

	UNLOCK();
	return ret;
}

void cos_init(void *d)
{
	static int first = 0;
	union sched_param sp;
	int i, j;
	int ttt = 0;
	
	if(first == 0){
		first = 1;

		lock_static_init(&lm_lock);
		printc("log monitor init...(thd %d spd %ld)\n", cos_get_thd_id(), cos_spd_id());

		memset(logmon_info, 0, sizeof(struct logmon_info) * MAX_NUM_SPDS);
		for (i = 0; i < MAX_NUM_SPDS; i++) {
			logmon_info[i].spdid = i;
		}

		/* memset(spd_trace, 0, sizeof(struct spd_trace) * MAX_NUM_SPDS); */
		/* for (i = 0; i < MAX_NUM_SPDS; i++) { */
		/* 	spd_trace[i].spd_id = i; */
		/* 	spd_trace[i].entry  = NULL; */
		/* } */

		memset(&lmcs, 0, sizeof(struct logmon_cs));

		/* sp.c.type = SCHEDP_PRIO; */
		/* sp.c.value = 9; */
		/* logmon_process = sched_create_thd(cos_spd_id(), sp.v, 0, 0); */

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		printc("lm_sync process init...(thd %d)\n", cos_get_thd_id());
		lm_sync_period = LM_SYNC_PERIOD;
		timed_event_block(cos_spd_id(), 30);
		if (shared_csring_setup()) {
			printc("failed to set up shared cs ");
			BUG();
		}
		while(1) {
			sched_logpub_wait();
			lm_process(cos_spd_id());
		}
	}
	return;
}
 

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_LOGMON_PROCESS:
		printc("logmonitor upcall: thread %d\n", cos_get_thd_id());
		break;
	default:
		cos_init(NULL);
	}
	return;
}


