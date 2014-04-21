#ifndef   	LOG_PROCESS_H
#define   	LOG_PROCESS_H

/* Data structure for tracking information in log_manager */

#include <ll_log.h>
#include <heap.h>
#include <log_util.h>

//#define MEAS_LOG_OVERHEAD
#ifdef MEAS_LOG_OVERHEAD
volatile unsigned long long logmeas_start, logmeas_end;
#endif

/* cache the dest for a cap_no */
struct spd_cap {
	int dest;
	unsigned long capno;
};

/* ring buffer for event flow (one RB per component) */
struct logmon_info {
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
	
	struct spd_cap capdest[MAX_STATIC_CAP];

	struct evt_entry first_entry;
};

/* per thread tracking data structure */
struct thd_trace {
	int thd_id, prio;

	// d and c for a periodic task
	unsigned long long period;
	unsigned long long execution;

	// Execution time of this thread
	unsigned long long tot_exec;
	// Last time stamp for this thread
	unsigned long long last_ts;

	// number of timer interrupt while this thread running
	unsigned long long timer_int;
	// number of network interrupt while this thread running
	unsigned int network_int; 

	// how long this thread has been in PI?
	unsigned long long pi_time, pi_start_t;
        // used to detect scheduling decision
	unsigned long long epoch;
        // used to detect deadlock
	unsigned int depth;
	// for dependency list (next, lowest and highest)
	struct thd_trace *n, *l, *h;

	// WCET in a spd due to an invocation to that spd by this thread
	unsigned long long inv_spd_exec[MAX_NUM_SPDS];
	// number of invocations made to a component by this thread
	unsigned int invocation[MAX_NUM_SPDS];
};

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct thd_trace thd_trace[MAX_NUM_THREADS];
struct evt_entry last_entry;
struct heap *h;  // the pointer to the max_heap

// time of start/end log processing and the duration of last time processing
volatile unsigned long long lpc_start, lpc_end, lpc_last;

static void 
init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id    = i;
		thd_trace[i].period    = 0;	// same as deadline d
		thd_trace[i].execution = 0;	// same as period p
		
		// timing info for a thread
		thd_trace[i].tot_exec = 0;	// the total exec time for thd
		thd_trace[i].last_ts  = 0;	// the last time stamp for this thd

		// number of events 
		thd_trace[i].timer_int	 = 0;	// number of timer INT while thd
		thd_trace[i].network_int = 0;	// number of network INT while thd
		// dependency tree info
		thd_trace[i].pi_time	 = 0;	// how long a thread is in PI status
		thd_trace[i].pi_start_t	 = 0;	// when a thread starts being in PI status
		thd_trace[i].epoch	 = 0;	// used to detect scheduling decision
		thd_trace[i].depth	 = 0;	// used to detect deadlock
		thd_trace[i].n = thd_trace[i].l = thd_trace[i].h = &thd_trace[i];
		
		// spd related (number of events and timing)
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].inv_spd_exec[j] = 0;
			thd_trace[i].invocation[j]   = 0;
		}
	}
	return;
}

// exclude the log processing time from accumulated timing for thread
static void
update_proc(unsigned long long process_time)
{
	int i;
	struct thd_trace *ttc;
	
	assert(process_time > 0);

	for (i = 0; i < MAX_NUM_THREADS; i++) {
		ttc = &thd_trace[i];
		assert(ttc);
		// update up running time for a thread
		if (ttc->last_ts > 0) {
			ttc->last_ts = ttc->last_ts + process_time;
			assert(ttc->last_ts > 0);
		}
		// other timing here...
	}
	return;
}


// cache dest spdid or use cached one
static int
lookup_dest(int spdid, unsigned long cap_no)
{
	int i, ret = 0;
	struct logmon_info *spdmon = &logmon_info[spdid];
	assert(spdmon && cap_no);

	/* printc("look for dest for cap_no %lu\n", cap_no);	 */
	for (i = 0; i < MAX_STATIC_CAP; i++){
		if (unlikely(!spdmon->capdest[i].capno)) {
			ret = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, spdid, cap_no);
			assert(ret > 0);
			spdmon->capdest[i].capno = cap_no;
			spdmon->capdest[i].dest	 = ret;
			/* printc("add dest %d for cap_no %lu\n", ret, cap_no); */
			goto done;
			
		} else if (spdmon->capdest[i].capno == cap_no) {
			ret = spdmon->capdest[i].dest;
			assert(ret > 0);
			/* printc("found dest %d for cap_no %lu\n", ret, cap_no); */
			goto done;
		}
	}
done:	
	assert(ret >0);
	return ret;
}

// convert cap to destination spd id
static void
cap_to_dest(struct evt_entry *entry)
{
	int d;
	
	assert(entry && (entry->evt_type == EVT_CINV || 
		entry->evt_type == EVT_CRET));
	/* print_evt_info(entry); */
	if (entry->to_spd < MAX_NUM_SPDS) return;
	
	d = lookup_dest(entry->from_spd, entry->to_spd);
	assert(d > 0 && d < MAX_NUM_SPDS);
	if (entry->evt_type == EVT_CINV) entry->to_spd = d;
	else entry->from_spd = d;
	
	return;
}

/* Add the earliest event from each spd onto the heap. Do this once */
static void
populate_evts()
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
		assert(entry);
		if (!CK_RING_DEQUEUE_SPSC(logevt_ring, evtring, entry)) {
			continue;
		}
		// if there is no event in a spd now, there should be
		// no event until the log manger finish the processing
		es[i].ts = LLONG_MAX - entry->time_stamp;			
		assert(!heap_add(h, &es[i]));
	}	
	/* validate_heap_entries(h, MAX_NUM_SPDS); */
	
	return;
}

// look for the next earliest event
static struct evt_entry *
find_next_evt(struct evt_entry *evt)
{
	int spdid;
	struct evt_entry *ret = NULL;
        struct logmon_info *spdmon;
	struct hevtentry *next;
	CK_RING_INSTANCE(logevt_ring) *evtring;

	if (unlikely(!evt)) {
		populate_evts();
	} else {	
		assert((spdid = evt_in_spd(evt)));
		// Check the constraints here too....TODO
		/* print_evt_info(evt); */
		spdmon = &logmon_info[spdid];
		assert(spdmon);
		evtring = (CK_RING_INSTANCE(logevt_ring) *)spdmon->mon_ring;
		assert(evtring);
		if (CK_RING_DEQUEUE_SPSC(logevt_ring, evtring, evt)) {
			es[spdid].ts = LLONG_MAX - evt->time_stamp;
			heap_add(h, &es[spdid]);
		}
	}

	next = heap_highest(h);
	if (!next || next->ts == 0) goto done;
	/* printc("earliest next spdid %d -- ts %llu\n", next->spdid, LLONG_MAX - next->ts); */
	spdmon = &logmon_info[next->spdid];
	assert(spdmon);
	ret = &spdmon->first_entry;
	assert(ret);
done:
	return ret;
}

/**************************/
/*    Constraints Check   */
/**************************/
//debug only, print dependencies
static void
print_deps(struct thd_trace *root)
{
	struct thd_trace *d, *n, *m;
	assert(root);

	if (root->h == root) return;
	m = d = root->h;
	while(1) {
		n = d;
		d = d->n;
		if (n == d) break;
		printc("%d<--", n->thd_id);
	}
	assert(n == d->n);
	printc("%d", n->thd_id);
	printc("\n");

	return;
}

/* If the log process starts after PI happens and before PI ends, we
 * might incorrectly account log processing time into PI
 * time. Need to subtract processing time. */

static int
check_priority_inversion(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry)
{
	struct thd_trace *d, *n, *m, *root;

	assert(ttc && entry);
	if (!ttn) return 0;

	if (unlikely(pi_begin(&last_entry, ttc->prio, entry, ttn->prio))) {
		root = &thd_trace[last_entry.para];
		assert(root);

		ttc->n	     = root->h;
		ttc->l	     = root;
		root->h	     = ttc;
		ttc->pi_start_t = root->tot_exec; // start accounting PI time
		ttc->pi_time    = 0;
		return 0;
	}
	/* remove dependency ( update and check epoch )*/
	if (unlikely(pi_end(&last_entry, ttc->prio, entry, ttn->prio))) {
		root = ttc;
		assert(root);
		m = d = root->h;
		while((d = d->n) && d!=root);
		d->epoch++;
		m->epoch = d->epoch;
                /* if not match, an error occurs */
		if (m->epoch != ttn->epoch) {
			/* printc("epoch mismatch: m %d (%d) ttn %d (%d) ", m->thd_id, m->epoch, ttn->thd_id, ttn->epoch); */
			assert(0);
			// TODO: localize the faulty spd ...here
		}

		m->pi_time = ttc->tot_exec - m->pi_start_t + ttc->pi_time;

		/* // should all lower threads execution time be added after m is in PI? */
		/* int i; */
		/* struct thd_trace *tmp; */
		/* for (i = 0; i < MAX_NUM_THREADS; i++) { */
		/* 	tmp = &thd_trace[i]; */
		/* 	assert(tmp); */
		/* 	if (tmp->prio >= root->prio && tmp->tot_exec > m->pi_start_t) { */
		/* 		printc("lower thd %d tot_exe %llu\n", tmp->thd_id, tmp->tot_exec); */
		/* 		printc("m->pi_time %llu m->pi_start_t %llu\n", m->pi_time, m->pi_start_t); */
		/* 		m->pi_time = m->pi_time + tmp->tot_exec - m->pi_start_t; */
		/* 	} */
		/* } */

		/* printc("After PI remove: "); */
		/* print_deps(m); */
		printc("thd %d is in PI for %llu\n", m->thd_id, m->pi_time);
		printc("\n");
		// TODO: localize the faulty spd ...here
		m->pi_start_t = 0;

		root->h = m->n;
	}

	
	return 0;
}

static void
check_ordering(struct thd_trace *ttc, struct evt_entry *entry)
{
	assert(ttc && entry);
	
	int p_type = last_entry.evt_type; // previous event type
	if (p_type == 0) return;
	int n_type = entry->evt_type; // next event type
	assert(n_type > 0);

	// thread local detection and global events !!! TODO:
	switch (p_type) {
	case EVT_CINV:
		if (n_type == EVT_SINV || n_type == EVT_NINT || 
		    n_type == EVT_TINT) goto done;
		break;
	case EVT_SINV:
		if (n_type == EVT_CINV || n_type == EVT_SRET ||
		    n_type == EVT_CS || n_type == EVT_NINT ||
		    n_type == EVT_TINT) goto done;
		break;
	case EVT_SRET:
		if (n_type == EVT_CRET || n_type == EVT_NINT || 
		    n_type == EVT_TINT) goto done;
		break;
	case EVT_CRET:
		if (n_type == EVT_CINV || n_type == EVT_SRET || 
		    n_type == EVT_CS || n_type == EVT_TINT || 
		    n_type == EVT_TINT) goto done;
		break;
	case EVT_CS:
		if (n_type == EVT_CINV || n_type == EVT_SRET || 
		    n_type == EVT_NINT || n_type == EVT_TINT) goto done;
		break;
	case EVT_NINT:
	case EVT_TINT:
		if (n_type == EVT_CINV || n_type == EVT_SRET || 
		    n_type == EVT_CS   || n_type == EVT_NINT ||
		    n_type == EVT_TINT) goto done;
		break;
	default:
		break;
	}

	// TODO: localize the faulty spd ... here
done:
	return;
}

static void
check_invocation(struct thd_trace *ttc, struct evt_entry *entry)
{
	assert(ttc && entry);
	assert(entry->evt_type > 0);

	switch (entry->evt_type) {
	case EVT_SINV:
		ttc->invocation[entry->to_spd]++;
		break;
	default:
		break;
	}
	
	// TODO: check number of invocation to a spd here
	// ...
	return;
}

static void
check_interrupt(struct thd_trace *ttc, struct evt_entry *entry)
{
	assert(ttc && entry);
	assert(entry->evt_type > 0);

	switch (entry->evt_type) {
	case EVT_NINT:
		/* assert(entry->to_spd == NETIF_SPD); */
		/* assert(entry->to_thd == NETWORK_THD); */
		ttc->network_int++;
		break;
	case EVT_TINT:
		/* assert(entry->to_spd == SCHED_SPD); */
		/* assert(entry->to_thd == TIMER_THD); */
		ttc->timer_int++;
		break;
	default:
		break;
	}
	// TODO:check number of interrupts in a time window here
	// ...
	return;
}

static void
check_inv_spdexec(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry, unsigned long long up_t)
{
	int spdid;

	assert(ttc && entry);
	assert(entry->evt_type > 0);

	// reset to 0 when interrupt happens (start execution)
	if (entry->evt_type == EVT_NINT || entry->evt_type == EVT_TINT) {
		assert(ttn);
		ttn->inv_spd_exec[entry->to_spd] = 0;
	}		

	switch (entry->evt_type) {
	case EVT_SINV:
		ttc->inv_spd_exec[entry->to_spd] = 0;
		break;
	case EVT_CINV:
	case EVT_CS:
	case EVT_TINT:
	case EVT_NINT:
	case EVT_SRET:
		if ((spdid = last_entry.from_spd)) {
			ttc->inv_spd_exec[spdid] += up_t;
		}
		
		if (entry->evt_type == EVT_SRET) {
			/* if (spdid == SCHED_SPD) printc("<<thd %d in spd %d in one invocation is %llu >>\n", ttc->thd_id, spdid, ttc->inv_spd_exec[spdid]); */
			// TODO:check WCET in spd due to invocation here
			// ....
			ttc->inv_spd_exec[spdid] = 0;
		}
		break;
	case EVT_CRET:
		break;
	default:
		printc("event type error type %d\n", entry->evt_type);
		assert(0);
		break;
	}

	return;
}

static void 
constraint_check(struct evt_entry *entry)
{
	struct thd_trace *ttc, *ttn = NULL;
	unsigned long long up_t;
	int from_thd, from_spd;

	/* print_evt_info(entry); */

	assert(entry && entry->evt_type > 0);
	
	if (entry->evt_type == EVT_TINT || entry->evt_type == EVT_NINT) {
		if (likely(last_entry.from_thd && last_entry.from_spd)) {
			from_thd = last_entry.from_thd;
			from_spd = last_entry.from_spd;
		} else {
			from_thd = entry->from_thd;
			from_spd = entry->from_spd;
		}
	} else {
		from_thd = entry->from_thd;
		from_spd = entry->from_spd;
	}
	assert(from_thd && from_spd);

	// ttc tracks thread that runs up to this point (not the next)
	ttc = &thd_trace[from_thd];
	assert(ttc);
	// first event for a thread here
	if (unlikely(!ttc->last_ts)) ttc->last_ts = entry->time_stamp;
	
	switch (entry->evt_type) {
	case EVT_CINV:
	case EVT_CRET:
		cap_to_dest(entry); // convert cap_no to spdid (TODO:cache this!)
	case EVT_SINV:
	case EVT_SRET:
		assert(entry->to_thd == entry->from_thd);
		up_t = entry->time_stamp - ttc->last_ts;
		ttc->tot_exec += up_t;
		ttc->last_ts = entry->time_stamp;
		break;
	case EVT_CS:
		assert(entry->from_spd == SCHED_SPD);  
		assert(entry->from_thd != entry->to_thd);
	case EVT_TINT:
	case EVT_NINT:
		up_t = entry->time_stamp - ttc->last_ts;
		ttc->tot_exec += up_t;
		ttn = &thd_trace[entry->to_thd];
		assert(ttn);
		ttn->last_ts = entry->time_stamp;
		break;
	default:
		printc("event type error\n");
		assert(0);
		break;
	}	
	/* if (ttc->thd_id == 14) */
	/* 	printc("thd %d total exec %llu \n", ttc->thd_id, ttc->tot_exec); */

	/***************************/
	/* Constraint check begins */
	/***************************/
	check_inv_spdexec(ttc, ttn, entry, up_t);
	check_interrupt(ttc, entry);
	check_invocation(ttc, entry);
	check_ordering(ttc, entry);

	check_priority_inversion(ttc, ttn, entry);

	/* check_thd_wcet(ttc, ttn, entry, up_t); */
	/***************************/
	/* Constraint check done ! */
	/***************************/
	
        // save info for the last event
	last_evt_update(&last_entry, entry);
	
	return;
}

#endif
