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

static cos_lock_t lm_lock;
#define LOCK() if (lock_take(&lm_lock)) BUG();
#define UNLOCK() if (lock_release(&lm_lock)) BUG();

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct logmon_cs lmcs;

struct thd_trace thd_trace[MAX_NUM_THREADS];
struct spd_trace spd_trace[MAX_NUM_SPDS];

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
		thd_trace[i].entry  = NULL;
	}
	return;
}

static void lm_print()
{
	printc("<<< check ring lm_print 9>>>\n");
	lm_process(9);
	printc("<<< check ring lm_print 10>>>\n");
	lm_process(10);
	printc("<<< check ring lm_print 11>>>\n");
	lm_process(11);
	printc("<<< check ring lm_print 12>>>\n");
	lm_process(12);
	printc("\n");
	return;
}

static void print_thd_history(int thd_id)
{
	struct thd_trace *evt_head;
	evt_head = &thd_trace[15];
	/* if (!evt_head->entry) return; */

	printc("event id %p\n", (void *)evt_head->entry->event_id);
	/* printc("thd id %d\n", evt_head->entry->thd_id); */
	/* printc("caller spd %d\n", evt_head->entry->spd_id); */
	/* printc("time_stamp %llu\n\n", evt_head->entry->time_stamp); */
	
	return;
}

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

static void print_csinfo(struct cs_info *entry)
{
	assert(entry);
	printc("curr thd id %d\n", entry->curr_thd);
	printc("switch to thd id %d\n", entry->next_thd);
	printc("time_stamp %llu\n\n", entry->time_stamp);
	
	return;
}

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

static int shared_csring_setup() {
        vaddr_t mon_csring, sched_csring;
	int sched_spdid;
	char *addr, *hp;

        // can not get page through mmgr since this will create mapping
        /* mon_csring = (vaddr_t)alloc_page(); */ 
	hp = cos_get_heap_ptr();
	mon_csring = (vaddr_t)hp;
	cos_set_heap_ptr((void*)(((unsigned long)hp)+PAGE_SIZE));
	hp = cos_get_heap_ptr();

        if (!mon_csring) {
                printc(" alloc ring buffer failed in logmonitor %ld.\n", cos_spd_id());
                goto err;
        }
	lmcs.mon_csring = mon_csring;

	sched_spdid = sched_csring_setup(cos_spd_id(), 0);
	assert(sched_spdid);

	sched_csring = sched_csring_setup(mon_csring, 1);
	assert(sched_csring);

	/* printc("mon_csring addr %p\n", (void *)mon_csring); */
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

// total wcet time of a thread to the point when the check is being done
static unsigned long long report_tot_wcet(int thd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	evt_list = &thd_trace[thd_id];
	if (!evt_list->entry) return 0;
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
	printc("thd %d overall exec --> %llu\n", thd_id, evt_list->tot_exec);
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

	/*  iterate entries in the list */
	/* if (evt_list->entry) { */
	/* 	if (spd_id == evt_list->entry->spd_id) { */
	/* 		evt_list->exec[spd_id] = evt_list->entry->time_stamp; */
	/* 	} */
	/* 	for (entry = LAST_LIST(evt_list->entry, next, prev) ; */
	/* 	     entry!= evt_list->entry; */
	/* 	     entry = LAST_LIST(entry, next, prev)) { */
	/* 		print_evtinfo(entry); */
	/* 	} */
	/* } */
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

static void walk_cs_events()
{
	int i, head, tail;
	CK_RING_INSTANCE(logcs_ring) *csring;
	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring;
	if (csring) {
		head = csring->c_head;
		tail = csring->p_tail;
		struct cs_info *cs_entry;
		for (i = head; i < tail; i++){
			cs_entry = &csring->ring[i];
			print_csinfo(cs_entry);
		}
	}
	
	return;
}


static void walk_thd_events()
{
	int i, j, thd_id;
	int head, tail;
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	CK_RING_INSTANCE(logevts_ring) *wk_evts;

	struct event_info *entry;
	struct thd_trace *evt_list;

	unsigned long long exec_cal, exec_cal_prev;
	unsigned long long exec_prev, exec_curr;

	int pair = 0;

	init_thread_trace();

	for (i = 0; i < MAX_NUM_SPDS; i++) {
		exec_prev = exec_curr = exec_cal = exec_cal_prev = 0;
		
		spdmon = &logmon_info[i];
		wk_evts = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		if(!wk_evts) continue;
		
		head = wk_evts->c_head;
		tail = wk_evts->p_tail;
		/* printc("wk: spd id %d (head %d tail %d)\n", i, head, tail); */
		for (j = head; j < tail; j++){
			entry = &wk_evts->ring[j];
			evt_list = &thd_trace[entry->thd_id];
			if (unlikely(!evt_list->entry)) {
				evt_list->entry = entry;
				INIT_LIST(evt_list->entry, next, prev);
			}
			else {
				ADD_LIST(evt_list->entry, entry, next, prev);
			}
			if (pair == 1) {
				pair = 0;
				exec_curr = entry->time_stamp;
				/* printc("entry->thd_id is %d (spd %d)\n", entry->thd_id, i); */
				exec_cal += exec_curr - exec_prev;

				evt_list->tot_exec += exec_cal;
				evt_list->tot_spd_exec[i] += exec_cal;
				/* evt_list->tot_wcet += evt_list->wcet[i];// ??? */

				if (entry->call_ret == 4) {
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
				pair = 1;
				exec_prev = entry->time_stamp;
			}
		}
	}
	
	return;
}

static void lm_sync_process()
{
	while(1) {
		sched_block(cos_get_thd_id(), 0);
		printc("<<< check ring lm_sync clean 9>>>\n");
		lm_process(9);
	}
	return;
}

static int first = 0;

static void lm_async_process()
{
	if (first < 3) {
		first += 1;
		/* printc("<--- walking through events --->\n"); */
		walk_thd_events();
		/* walk_cs_events(); */
		lm_report();
	}
	return;
}


static int lm_clean()

{
	struct event_info monevt;
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;

	printc("lm_clean...(thd %d)\n", cos_get_thd_id());

	int spdid = 9; // clean spd 9 ring, test only
        assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon->spdid == spdid);

	if ((void *)spdmon->mon_ring) monevt_dequeue((CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring, &monevt);
	
	return 0;
}

int lm_process(spdid_t spdid)
{
        struct logmon_info *spdmon;
        vaddr_t mon_ring, cli_ring;
	int i, head, tail;

	LOCK();
	/* printc("lm_process...(thd %d spd %d)\n", cos_get_thd_id(), spdid); */


	// Event flow track
        assert(spdid);
	spdmon = &logmon_info[spdid];
	assert(spdmon->spdid == spdid);

	/* printc("mon:mon_ring addr %p\n", (void *)spdmon->mon_ring); */
	/* printc("mon:cli ring addr %p\n", (void *)spdmon->cli_ring); */

	if ((void *)spdmon->mon_ring) {
		CK_RING_INSTANCE(logevts_ring) *tmp_ring;
		struct event_info *entry;

		tmp_ring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		assert(tmp_ring);
 		/* printc("ring size: %u\n", CK_RING_SIZE(logevts_ring, tmp_ring)); */
		head = tmp_ring->c_head;
		tail = tmp_ring->p_tail;

		for (i = head; i < tail; i++){
			entry = &tmp_ring->ring[i];
			print_evtinfo(entry);
		}
	}


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


int logmon_process, sync_process;

void 
cos_init(void *d)
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

		memset(spd_trace, 0, sizeof(struct spd_trace) * MAX_NUM_SPDS);
		for (i = 0; i < MAX_NUM_SPDS; i++) {
			spd_trace[i].spd_id = i;
			spd_trace[i].entry  = NULL;
		}

		memset(&lmcs, 0, sizeof(struct logmon_cs));

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 9;
		logmon_process = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sync_process = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		if (cos_get_thd_id() == logmon_process) {
			periodic_wake_create(cos_spd_id(), 50);
			while(1) {
				lm_async_process();
				/* lm_print(); */
				periodic_wake_wait(cos_spd_id());
			}
		}
		if (cos_get_thd_id() == sync_process) {
			printc("lm_sync process init...(thd %d)\n", cos_get_thd_id());
			if (shared_csring_setup()) {
				printc("failed to set up shared cs ");
				BUG();
			}
			lm_sync_process();
		}
	}
	return;
}
 
