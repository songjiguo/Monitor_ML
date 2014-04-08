#include <cos_component.h>
#include <print.h>

#include <ll_log_deps.h>
#include <ll_log.h>

#include <log_report.h>      // for log report/ print
#include <log_process.h>   // data structure that check the violation
#include <hard_code_thd_spd.h>   // TODO: replace this with the name space

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

static void
update_pi_info(struct thd_trace *thd_trace_list, struct evt_entry *entry)
{
	struct thd_trace *h_thd_trace_list;
	
	assert(thd_trace_list);
	assert(entry);

	if (!(thd_trace_list->block_dep = entry->dep_thd)) return;

	int dest = entry->to_spd;
	assert(dest < MAX_NUM_SPDS);
	/* if (dest > MAX_NUM_SPDS) { */
	/* 	if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0); */
	/* } */

	/* printc("PI....(dest %d)d\n", dest); */
	print_evtinfo(entry);
	
	if (dest == 3 && entry->func_num == 11) {
		printc("(thd %d) PI start at %llu (dep on thd %d)\n", entry->from_thd, entry->time_stamp, entry->dep_thd); 
		thd_trace_list->pi_duration = entry->time_stamp;
		printc("\n");
	}

	if (dest == 3 && entry->func_num == 12) {
		h_thd_trace_list = &thd_trace[entry->dep_thd];
		assert(thd_trace_list);	
		printc("last PI %llu\n", h_thd_trace_list->pi_duration);
		if (h_thd_trace_list->pi_duration == 0)  return; // ensure it was blocked before
		printc("(thd %d) PI end at %llu\n", entry->dep_thd, entry->time_stamp);
		h_thd_trace_list->pi_duration = entry->time_stamp - h_thd_trace_list->pi_duration;
		printc("(thd %d) PI lasts for %llu (dep on thd %d)\n", entry->dep_thd, h_thd_trace_list->pi_duration, entry->dep_thd);
		h_thd_trace_list->pi_duration = 0;
		printc("\n");
	}

	
	return;
}

static void
update_stack_info(struct thd_trace *thd_trace_list, struct evt_entry *entry)
{
	assert(thd_trace_list);
	assert(entry);
	
	/* print_evtinfo(entry); */
	
	if (entry->from_spd && entry->to_spd) {
	    /* entry->from_spd != entry->dest_info) { */
		thd_trace_list->curr_spd_info.spdid = entry->from_spd;
		thd_trace_list->curr_spd_info.from_spd = entry->to_spd;
		/* printc("from spd %d to spd %d\n", entry->from_spd, entry->dest_info); */
	}

	return;
}

/* static void  */
/* update_timing_info(struct thd_trace *thd_trace_list, struct evt_entry *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next) */
/* { */
/* 	unsigned long long exec_tmp = 0; */

/* 	assert(thd_trace_list); */
/* 	assert(entry); */

/* 	int curr_spd = entry->from_spd; */
/* 	int next_spd = entry->dest_info; */

/* 	if (unlikely(!thd_trace_list->alpha_exec[curr_spd])) { */
/* 		thd_trace_list->alpha_exec[curr_spd] = entry->time_stamp; */
/* 		thd_trace_list->last_exec[curr_spd] = entry->time_stamp; */
/* 	} else { */
/* 		if (next_spd) { */
/* 			if (next_spd == curr_spd) { */
/* 				thd_trace_list->last_exec[curr_spd] = entry->time_stamp; */
/* 			} else { */
/* 				exec_tmp = entry->time_stamp - thd_trace_list->last_exec[curr_spd]; */
/* 				assert(exec_tmp); */
/* 				thd_trace_list->tot_exec += exec_tmp; */
/* 				thd_trace_list->tot_spd_exec[curr_spd] += exec_tmp; */
/* 				thd_trace_list->single_wcet[curr_spd] += exec_tmp; */
/* 			} */
/* 		} else { */
/* 			// update the wcet of this thd up to this spd */
/* 			exec_tmp = entry->time_stamp - thd_trace_list->alpha_exec[curr_spd]; */
/* 			thd_trace_list->alpha_exec[curr_spd] = 0; */
/* 			if (exec_tmp > thd_trace_list->upto_wcet[curr_spd]) */
/* 				thd_trace_list->upto_wcet[curr_spd] = exec_tmp; */
/* 			/\* printc(" thd %d << wcet %llu >>\n ",thd_trace_list->thd_id, exec_tmp);		 *\/ */
/* 			thd_trace_list->tot_upto_wcet[curr_spd] += thd_trace_list->upto_wcet[curr_spd]; */
			
/* 			// update the wcet of this thd in this spd */
/* 			if (thd_trace_list->single_wcet[curr_spd] > thd_trace_list->wcet[curr_spd]) */
/* 				thd_trace_list->wcet[curr_spd] = thd_trace_list->single_wcet[curr_spd]; */
/* 			thd_trace_list->tot_wcet[curr_spd] += thd_trace_list->wcet[curr_spd]; */
			
/* 			// check WCET here!!!! Any violation should be seen here!!! */
			
/* 			// reset local wcet record and increase the invs to this spd */
/* 			thd_trace_list->single_wcet[curr_spd] = 0; */
/* 			thd_trace_list->tot_inv[curr_spd]++; */
/* 		} */
		
/* 	} */
	

/* 	/\* printc("thd %d (total exec %llu, wcet %llu) in spd %d \n", thd_trace_list->thd_id, thd_trace_list->tot_spd_exec[curr_spd], thd_trace_list->wcet[curr_spd], curr_spd); *\/ */
/* 	/\* printc("thd %d (total wcet %llu) upto spd %d (invocations %d)\n\n ", thd_trace_list->thd_id, thd_trace_list->tot_wcet[curr_spd], curr_spd, thd_trace_list->tot_inv[curr_spd]); *\/ */
	
/* 	return; */
/* } */


static void
copy_evt(struct evt_entry *curr, struct evt_entry *next)
{
	
	curr->from_thd = next->from_thd;
	curr->from_spd = next->from_spd;
	curr->to_spd = next->to_spd;
	curr->to_thd = next->to_thd;
	curr->time_stamp= next->time_stamp;
	curr->func_num= next->func_num;
	curr->dep_thd= next->dep_thd;
	return;
}

static void 
cap_to_dest(struct evt_entry *entry)
{
	int dest = entry->to_spd;
	if (dest > MAX_NUM_SPDS) {
		/* printc("call cos cap_get_ser_spd %x, time %llu\n", dest, entry->time_stamp); */
		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0);
		entry->to_spd = dest;
	}
	
	return;
}


static unsigned long long old_curr_t;
static unsigned long long old_next_t;

/* static int */
/* deadline_check(struct evt_entry *entry) */
/* { */
/* 	struct thd_trace *thd_trace_list; */
/* 	thd_trace_list = &thd_trace[cs_entry_curr->next_thd]; */
/* 	assert(thd_trace_list); */
/* 	int spd = entry->dest_info; */
/* 	assert(spd < MAX_NUM_SPDS); */


/* 	if (thd_trace_list->release */
/* 	    && (old_curr_t != cs_entry_curr->time_stamp)  */
/* 	    && (old_next_t != cs_entry_next->time_stamp)) { */
/* 		printc("thd %d runtime update ", cs_entry_curr->next_thd); */
/* 		printc("cs curr %llu cs next %llu\n", cs_entry_curr->time_stamp,cs_entry_next->time_stamp); */
/* 		thd_trace_list->runtime  += cs_entry_next->time_stamp - cs_entry_curr->time_stamp; */
/* 		old_curr_t = cs_entry_curr->time_stamp; */
/* 		old_next_t = cs_entry_next->time_stamp; */
/* 	} */

/* 	// unblock from periodic_wake_wait */
/* 	if (entry->dest_info == 0 && entry->from_spd == 8 && entry->func_num == 212) { */
/* 		printc("unblock: thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); */
/* 		thd_trace_list->release = entry->time_stamp; */
/* 		thd_trace_list->wait_for_block = 0; */
/* 	} */

/* 	// call periodic_wake_wait to block (need wait till actual sched_block) */
/* 	if (entry->dest_info == 8 && entry->from_spd == 8 && entry->func_num == 212) { */
/* 		printc("wait to block: thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); */
/* 		assert(thd_trace_list->wait_for_block == 0); */
/* 		thd_trace_list->wait_for_block = 1;		 */
/* 	} */

/* 	// actual block in periodic_wake */
/* 	if (entry->dest_info == 3 && entry->from_spd == 8  */
/* 	    && entry->func_num == 1 && thd_trace_list->wait_for_block == 1) { */
/* 		printc("periodic wake blocked!:thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); */
/* 		thd_trace_list->completion = entry->time_stamp; */

/* 		printc("thd %d -- release time %llu\n", thd_trace_list->thd_id, thd_trace_list->release); */
/* 		printc("thd %d -- completion time %llu\n", thd_trace_list->thd_id, thd_trace_list->completion); */

/* 		printc("thd %d -- expected execution time %llu\n", thd_trace_list->thd_id, thd_trace_list->execution); */
/* 		printc("thd %d -- actual   execution time %llu\n", thd_trace_list->thd_id, thd_trace_list->runtime); */
/* 		thd_trace_list->runtime = 0; */
/* 		thd_trace_list->release = 0; */
/* 		old_curr_t = 0; */
/* 		old_next_t = 0; */
/* 	} */

/* 	return 1; */
/* } */

/* static int */
/* sync_evt_cs(struct evt_entry *entry, struct cs_info *cs_entry_curr, struct cs_info *cs_entry_next) */
/* { */
/* 	CK_RING_INSTANCE(logcs_ring) *csring; */
/* 	csring = (CK_RING_INSTANCE(logcs_ring) *)lmcs.mon_csring; */
/* 	assert(csring); */

/* 	/\* printc("before: cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr->time_stamp, cs_entry_next->time_stamp, entry->time_stamp); *\/ */
/* 	/\* printc("thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); *\/ */

/* 	cap_to_dest(entry); 	// convert the cap to spd if this is an invocation */

/* 	// same thread, no other thread switch occurred */
/* 	if (entry->time_stamp < cs_entry_next->time_stamp && entry->thd_id == cs_entry_curr->next_thd) { */

/* 		if (cs_entry_next->next_thd == 0 && cs_entry_next->flags == 0) { */
/* 			cs_entry_next->next_thd = cs_entry_next->curr_thd; */
/* 			assert(cs_entry_next->next_thd == 8); */
/* 			cs_entry_next->curr_thd = cs_entry_curr->next_thd; */
/* 		} */

/* 		// deadline_check(entry, cs_entry_curr, cs_entry_next);   we do not check deadline every time */

/* 		/\* if (entry->dest_info == 8 && entry->func_num == 22) { *\/ */
/* 		/\* 	printc("thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); *\/ */
/* 		/\* } *\/ */
/* 		/\* printc("same thread (%d) within the same cs\n", entry->thd_id); *\/ */
/* 		return 1; */
/* 	} */

/* 	if (cs_entry_curr->time_stamp == 0 && cs_entry_next->time_stamp == 0) { 	// first time */
/* 		if (!CK_RING_DEQUEUE_SPSC(logcs_ring, csring, cs_entry_curr)) return 0; */
/* 	} */
/* 	if (cs_entry_curr->time_stamp != 0 && cs_entry_next->time_stamp != 0) { 	// during the process */
/* 		copy_cs(cs_entry_curr, cs_entry_next); */
/* 	} */

/* 	while (CK_RING_DEQUEUE_SPSC(logcs_ring, csring, cs_entry_next)) {  	      // get 2nd cs entry */

/* 		if (cs_entry_next->next_thd == 0 && cs_entry_next->flags == 0) { */
/* 			cs_entry_next->next_thd = cs_entry_next->curr_thd; */
/* 			assert(cs_entry_next->next_thd == 8); */
/* 			cs_entry_next->curr_thd = cs_entry_curr->next_thd; */
/* 		} */

/* 		//deadline_check(entry, cs_entry_curr, cs_entry_next);  we do not check deadline every time */

/* 		/\* if (entry->dest_info == 8 && entry->func_num == 22) { *\/ */
/* 		/\* 	printc("thd %d spd form %d to %d (entry thd %d) -- %llu\n",cs_entry_curr->next_thd, entry->from_spd, entry->dest_info, entry->thd_id, entry->time_stamp); *\/ */
/* 		/\* } *\/ */
/* 		/\* deadline_check(entry, cs_entry_curr, cs_entry_next); *\/ */
/* 		/\* printc("sync: cs_curr  %llu (thd %d)  cs_next %llu\n",cs_entry_curr->time_stamp, cs_entry_curr->curr_thd, cs_entry_next->time_stamp); *\/ */
/* 		/\* printc("sync: thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); *\/ */
/* 		if (cs_entry_next->time_stamp < entry->time_stamp) { */
/* 			copy_cs(cs_entry_curr, cs_entry_next); */
/* 		} */
/* 		else { */
/* 			/\* printc("BREAK HERE\n"); *\/ */
/* 			return 1; */
/* 		} */
/* 	} */

/* 	// No cs record is found to cover the event entry, so will process in the next time */
/* 	copy_evt(&last_evt_entry, entry); */
/* 	copy_cs(&last_cs_entry, cs_entry_next); */
/* 	/\* printc("after: cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr->time_stamp, cs_entry_next->time_stamp, entry->time_stamp); *\/ */
/* 	/\* printc("thd %d spd %d  entry time %llu (entry thd %d)\n",cs_entry_curr->next_thd, entry->from_spd, entry->time_stamp, entry->thd_id); *\/ */
/* 	/\* } *\/ */
	
/* 	return 0; */
/* } */

/* static struct evt_entry * */
/* find_next_entry(int dest, int thdid) */
/* { */
/* 	struct evt_entry *ret = NULL; */
/*         struct logmon_info *spdmon; */

/* 	if (dest > MAX_NUM_SPDS) { */
/* 		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0); */
/*  		spdmon = &logmon_info[dest]; // this must be a different spd (invocation) */
/* 		if (spdmon->first_entry.thd_id != thdid) { */
/* 			spdmon = find_earliest_entry_spd(); */
/* 		} */
/* 	} else { */
/* 		spdmon = find_earliest_entry_spd(); */
/* 	} */

/* 	if (!spdmon) return NULL; */
/* 	ret = &spdmon->first_entry; */
/* 	return ret; */
/* } */

/*  
    Find the first event entry within a context switch (t1 to t2)
    There might be no event between t1 and t2
*/
static struct logmon_info *
find_event_entry(unsigned long long t1, unsigned long long t2)
{
	int i, j, spdid = 0;
        struct logmon_info *spdmon, *min = NULL;
	struct thd_trace *thd_trace_list, *this_list;;
	unsigned long long ts = LLONG_MAX;
	CK_RING_INSTANCE(logevt_ring) *evtring;
	struct evt_entry *entry;

	for (i = 1; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		if (!evtring) continue;

		entry = &spdmon->first_entry;
		if (!CK_RING_DEQUEUE_SPSC(logevt_ring, evtring, entry)) 
			continue;
		if (entry->time_stamp < t1 || entry->time_stamp > t2) continue;

		if (entry->time_stamp < ts) {
			ts = entry->time_stamp;
			min = spdmon;
			printc("min spd is %d\n", i);
		}
	}
	printc("t1 %llu t2 %llu\n",t1, t2);
	return min;
}

static struct evt_entry *
get_first_entry(struct logmon_info *spdmon)
{
	assert(spdmon);
	
	if (!&spdmon->first_entry) {
		CK_RING_INSTANCE(logevt_ring) *evtring;
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		assert(evtring);
		if (!CK_RING_DEQUEUE_SPSC(logevt_ring, evtring, &spdmon->first_entry)) return NULL;
	}
	
	return &spdmon->first_entry;
}

static struct logmon_info *
check_interrupt(int next_spd)
{
	struct logmon_info * ret = NULL;

	struct evt_entry * next_timer_evt;
	struct evt_entry * next_network_evt;
	struct evt_entry * next_evt;

	struct logmon_info *spdmon_timer = NULL;
	struct logmon_info *spdmon_network = NULL;
	struct logmon_info *spdmon = NULL;

	spdmon_timer = &logmon_info[SCHED_SPD];
	assert(spdmon_timer);
	next_timer_evt = get_first_entry(spdmon_timer);

	spdmon_network = &logmon_info[NETIF_SPD];
	assert(spdmon_network);
	next_network_evt = get_first_entry(spdmon_network);

	spdmon = &logmon_info[next_spd];
	assert(spdmon);
	next_evt = get_first_entry(spdmon);

	// find event with the minimum time stamp
	ret = spdmon;
	if (next_evt && next_timer_evt) {
		if (next_evt->time_stamp > next_timer_evt->time_stamp){
			ret = spdmon_timer;
		}
	}

	if (next_evt && next_network_evt) {
		if (ret->first_entry.time_stamp > next_network_evt->time_stamp){
			ret = spdmon_network;
		}
	}
	return ret;
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
	/* update_stack_info(entry); */
	/* update_pi_info(entry); */

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
	
	printc("<< The earliest event >>\n");
	print_evt_info(evt);
	
	int test_num = 0;
	
	do {
		constraint_check(evt);
		/* if (test_num++ > 5) break; */
	} while ((evt = find_next_entry(evt)));

	/* do */
	/* { */
	/* 	t1 = cs_entry_curr.time_stamp; */
	/* 	t2 = cs_entry_next.time_stamp; */
	/* 	if (!(spdmon = find_event_entry(t1, t2))) continue; */
	/* 	assert(cs_entry_curr.next_thd == spdmon->first_entry.thd_id); */
	/* 	printc("2.3\n"); */
	/* 	last_entry = process_events(&spdmon->first_entry, t1, t2); */
		
	/* 	// update cs */
	/* } while ((copy_cs(&cs_entry_curr, &cs_entry_next)) && */
	/* 	 CK_RING_DEQUEUE_SPSC(logcs_ring, csring, &cs_entry_next)); */
	
	return;
}

/* static void */
/* walk_through_events_new() */
/* { */
/* 	int this_thd; */
/* 	unsigned long long s,e; */

/*         struct logmon_info *spdmon; */
/* 	struct evt_entry *entry_curr = NULL, *entry_next = NULL; */
/* 	struct thd_trace *thd_trace_list; */
/* 	struct cs_info cs_entry_curr, cs_entry_next; */

/* 	memset(&cs_entry_curr, 0, sizeof(struct cs_info)); */
/* 	memset(&cs_entry_next, 0, sizeof(struct cs_info)); */

/* 	// if there is any record left from last process? */
/* 	if (last_cs_entry.time_stamp && last_evt_entry.time_stamp) { */
/* 		copy_cs(&cs_entry_curr, &last_cs_entry); */
/* 		if (!constraint_check(&last_evt_entry, &cs_entry_curr, &cs_entry_next)) return; */
/* 		last_evt_entry.time_stamp = 0; */
/* 		last_cs_entry.time_stamp = 0; */
/* 	} */

/* 	/\* printc("cs_curr  %llu  cs_next %llu (entry time %llu)\n",cs_entry_curr.time_stamp, cs_entry_next.time_stamp, entry_curr->time_stamp); *\/ */
/* 	/\* printc("curr_thd %d next_thd %d from_spd %d next_spd %d entry time %llu (entry thd %d)\n",cs_entry_curr.curr_thd, cs_entry_curr.next_thd, entry_curr->from_spd, dest_tmp, entry_curr->time_stamp, entry_curr->thd_id); *\/ */
/* 	spdmon = find_earliest_entry_spd(); */
/* 	if (!spdmon) return; */
/* 	entry_curr = &spdmon->first_entry; */
/* 	if (!constraint_check(entry_curr, &cs_entry_curr, &cs_entry_next)) return; */

/* 	int dest, thdid; */
/* 	int from_spd; */
/* 	while(1) { */
/* 		dest = entry_curr->dest_info; */
/* 		thdid = entry_curr->thd_id; */
/* 		from_spd = entry_curr->from_spd; */
/* 		entry_curr->thd_id = 0; */

/* 		entry_next = find_next_entry(dest, thdid); */
/* 		if (!entry_next) break; */

/* 		/\* printc("curr_thd %d next_thd %d from_spd %d next_spd %d entry time %llu (entry thd %d) ",cs_entry_curr.curr_thd, cs_entry_curr.next_thd, from_spd, entry_next->from_spd, entry_next->time_stamp, entry_next->thd_id); *\/ */
/* 		/\* printc("cs curr %llu cs next %llu\n",cs_entry_curr.time_stamp, cs_entry_next.time_stamp); *\/ */

/* 		if (!constraint_check(entry_next, &cs_entry_curr, &cs_entry_next)) break; */

/* 		entry_curr = entry_next;		 */
/* 	} */
/* 	return; */
/* } */

static void 
llog_report()
{
	printc("\n<<print tracking info >>\n");
	int i, j;
	unsigned long long exec, wcet;

	for (i = 1; i < MAX_NUM_THREADS; i++) {
		/* report_thd_stack_list(i); */
		for (j = 1; j < MAX_NUM_SPDS; j++) {
			exec = report_avg_in_wcet(i, j);
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
