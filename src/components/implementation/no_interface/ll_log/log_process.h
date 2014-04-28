#ifndef   	LOG_PROCESS_H
#define   	LOG_PROCESS_H

// TODO: thread local detection and global events

/* Data structure for tracking information in log_manager
   Currently, we are only tracking periodic tasks. */

#include <ll_log.h>
#include <heap.h>
#include <log_util.h>

//#define MEAS_LOG_LOCALIZATION  /* cost of localization */

#ifdef MEAS_LOG_LOCALIZATION
#define  DETECTION_MODE
//#define  LOGEVENTS_MODE
#endif

#define MONITOR(ttc)	(ttc->p > 0)  // if a thread is a periodic task
#define CONT_FLAG	(1<<24)       // contention flag on rb

/* constraint violation type */
enum{
	CONS_ONEINV_SPD_EXEC = 1,
	CONS_ALLINV_SPD_EXEC,
	CONS_INVOKNUM,
	CONS_WCET,
	CONS_DLMISS,
	CONS_SCHEDULING,
	CONS_PI,
	CONS_TIMEINT, 
	CONS_NETINT,
	// ordering
	CONS_SRETSPD,     // return to the incorrect spd
	CONS_CSINT,       // any missing context switch or interrupt 
	CONS_ALTERID,     // thread is or spd id sanity check fails
	CONS_SPDTIME,     // time stamp in a spd is incorrect
	CONS_CINV_TYPE,   // event type fault -- e.g, missing event in the log
	CONS_SINV_TYPE,   
	CONS_CRET_TYPE,
	CONS_SRET_TYPE,  
	CONS_CS_TYPE,    // this should indicate the faulty spd is shceduler
	CONS_NINT_TYPE,  // this should indicate the faulty spd is netif spd
	CONS_TINT_TYPE,  // this should indicate the faulty spd is shceduler
	CONS_MAX
};

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
	/* checking monotonically increasing time stamp in each spd */
	unsigned long long spd_last_ts;
} logmon_info[MAX_NUM_SPDS];

/* per thread tracking data structure */
struct thd_trace {
	int thd_id, prio;

	// d and c for a periodic task
	unsigned long long p, c;

	// Total execution time of this thread
	unsigned long long tot_exec, last_ts;

	// next deadline start and end. Exec time before that
	unsigned long long dl_s, dl_e, exec;
	// number of interrupts
	unsigned long long timer_int, timer_int_ts, timer_int_wd;
	unsigned long long network_int, network_int_ts, network_int_wd;

	// how long this thread has been in PI?
	unsigned long long pi_time, pi_start_t;
        // used to detect scheduling decision
	unsigned long long epoch;
        // used to detect deadlock
	unsigned int depth;
	// for dependency list (next, lowest and highest)
	struct thd_trace *n, *l, *h;

	// number of invocations made to a component by this thread
	unsigned int invocation[MAX_NUM_SPDS];
	// WCET including all lower spds between invoke and return
	unsigned long long exec_oneinv_allspd[MAX_NUM_SPDS];
	// exec time in a spd due to an invocation
	unsigned long long exec_oneinv_in_spd[MAX_NUM_SPDS];
	// exec time spent in a spd
	unsigned long long exec_allinv_in_spd[MAX_NUM_SPDS];
	unsigned long long exec_allinv_in_spd_max;
	unsigned long long exec_allinv_in_spd_max_spd;
	// track how long any higher thd's execution time while this thd in PI
	unsigned long long tot_exec_at_pi[MAX_NUM_THREADS];
} thd_trace[MAX_NUM_THREADS];

unsigned int timer_interrupts, network_interrupts;

/* specs from LOGEVENTS_MODE and used in DETECTION_MODE*/
unsigned long long window_sz = LLONG_MAX, window_start;
unsigned int timer_int_max;     // max # of interrupts in time_window
unsigned int network_int_max;
struct thd_specs {
	unsigned long long exec_max;
	unsigned long long pi_time_max;
	unsigned int invocation_max[MAX_NUM_SPDS];
	unsigned long long exec_oneinv_allspd_max[MAX_NUM_SPDS];
	unsigned long long exec_oneinv_in_spd_max[MAX_NUM_SPDS];
	unsigned long long exec_allinv_in_spd_max[MAX_NUM_SPDS];
} thd_specs[MAX_NUM_THREADS];

struct evt_entry last_entry;
struct heap *h;  // the pointer to the max_heap

// time of start/end log processing
volatile unsigned long long lpc_start, lpc_end, lpc_last;
unsigned int faulty_spd_localizer(struct thd_trace *ttc, 
				  struct evt_entry *entry, int evt_type);

static void 
init_log()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id	   = i;
		thd_trace[i].p		   = 0;	// period
		thd_trace[i].c		   = 0;	// wcet. Set some number for now.
		thd_trace[i].dl_s	   = 0;	// a deadline start
		thd_trace[i].dl_e	   = 0;	// a deadline ends
		thd_trace[i].exec	   = 0;	// actual exec before deadline
		
		// timing info for a thread
		thd_trace[i].tot_exec = 0;	// the total exec time for thd
		thd_trace[i].last_ts  = 0;	// the last time stamp for this thd

		// interrupt events 
		thd_trace[i].timer_int	    = 0;	// number of timer INT while thd
		thd_trace[i].timer_int_ts   = 0;	// when an timer int happens
		thd_trace[i].timer_int_wd   = 0;	// time window for timer ints
		thd_trace[i].network_int    = 0;	// number of network INT while thd
		thd_trace[i].network_int_ts = 0;	// when an network int happens
		
		// priority inversion
		thd_trace[i].pi_time	 = 0;	// how long a thread is in PI status
		thd_trace[i].pi_start_t	 = 0;	// when a thread starts being in PI status
		thd_trace[i].epoch	 = 0;	// used to detect scheduling decision
		thd_trace[i].depth	 = 0;	// used to detect deadlock
		thd_trace[i].n = thd_trace[i].l = thd_trace[i].h = &thd_trace[i];

		// number of invocations and execution time in spd due to the invocation
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].exec_oneinv_allspd[j]   = 0;
			thd_trace[i].exec_oneinv_in_spd[j] = 0;
			thd_trace[i].exec_allinv_in_spd[j] = 0;
			thd_trace[i].invocation[j]     = 0;
		}
		thd_trace[i].exec_allinv_in_spd_max= 0;
		thd_trace[i].exec_allinv_in_spd_max_spd= 0;

		for (j = 0; j < MAX_NUM_THREADS; j++) {
			thd_trace[i].tot_exec_at_pi[j] = 0;
		}
		
		/* init thread specs (set in LOGEVENTS_MODE, use in
		 * DETECTION_MODE) */
		thd_specs[i].exec_max = 0;	// max exec before deadline
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_specs[i].exec_oneinv_allspd_max[j] = 0;
			thd_specs[i].exec_oneinv_in_spd_max[j] = 0;
			thd_specs[i].invocation_max[j]	       = 0;
		}
	}
	return;
}

/* sliding time window for interrupt number detection window size is
 * set to the min period for now */
static void
slide_twindow(struct evt_entry *entry)
{
	assert(entry);
	if (last_entry.time_stamp && 
	    (window_start + window_sz) < entry->time_stamp) {
		window_start = last_entry.time_stamp;
		network_interrupts = 0;
		timer_interrupts = 0;
	}
	return;
}


/* PI begin condition: 1) The dependency from high to low
   (sched_block(spdid, low)) 2) A context switch from high thd to low
   thd */
static int
pi_on(struct evt_entry *pe, int p_prio, 
	   struct evt_entry *ce, int c_prio)
{
	return (ce->evt_type == EVT_CS &&
		ce->from_thd != ce->to_thd &&
		p_prio < c_prio &&
		pe->evt_type == EVT_SINV && 
		pe->func_num == FN_SCHED_BLOCK &&
		pe->para > 0);
}

/* A thread could be in PI before process starts (no events since then) */
static int
pi_in(struct thd_trace *ttc) { return (ttc->pi_start_t > 0);}

/* PI end condition: 1) Low thd wakes up high thd (sched_wakeup(spdid,
   high)); 2) A context switch from low to high */
static int
pi_off(struct evt_entry *pe, int p_prio, 
	   struct evt_entry *ce, int c_prio)
{
	return (ce->evt_type == EVT_CS &&
		ce->from_thd != ce->to_thd &&
		p_prio > c_prio &&
		pe->evt_type == EVT_SINV && 
		pe->func_num == FN_SCHED_WAKEUP &&
		pe->para > 0);
}

/* A periodic task begins if 1) sched_timeout (checked by timer
   thread) 2) switch to it from timer thread */
static int
periodic_task_begin(struct evt_entry *last, struct thd_trace *ttn, struct evt_entry *entry)
{
	return (entry->evt_type == EVT_CS &&
		entry->from_thd != entry->to_thd &&
		entry->from_thd == TE_THD &&
		last->evt_type == EVT_SINV &&
		last->func_num == FN_SCHED_TIMEOUT &&
		ttn->p > 0);
}

static void
last_evt_update(struct evt_entry *last, struct evt_entry *src)
{
	assert(last && src);
	
	last->to_thd	 = src->to_thd;
	last->to_spd	 = src->to_spd; 
	last->from_thd	 = src->from_thd;
	last->from_spd	 = src->from_spd; 

	last->evt_type	 = src->evt_type;
	last->para	 = src->para;
	last->func_num	 = src->func_num;
	last->time_stamp = src->time_stamp;

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

//debug only, print dependencies for PI only
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

// cache cap to destination spd id
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

/*********************************/
/*    Constraints Check  -- PI   */
/*********************************/
static void
log_or_det_pit(struct thd_trace *l, struct thd_trace *h, 
	       struct evt_entry *entry, unsigned long long pit)
{
	assert(l && h && entry);

	struct thd_specs *h_spec = &thd_specs[h->thd_id];
	assert(h_spec);
#ifdef LOGEVENTS_MODE
	updatemax_llu(&h_spec->pi_time_max, pit);
	PRINTD_PI("thd %d max pi_time %llu \n", h->thd_id, h_spec->pi_time_max);
#endif
#ifdef DETECTION_MODE
	if (pit > h_spec->pi_time_max) {
		faulty_spd_localizer(l, entry, CONS_PI);
	}
#endif
	return;
}

static unsigned long long
higher_exec(struct thd_trace *ttc, int t)
{
	int i;
	struct thd_trace *h;
	unsigned long long ret = 0;
	assert(ttc);
	
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		h = &thd_trace[i];
		if (h && MONITOR(h) && h->prio < ttc->prio) {
			(t == 0) ? (ttc->tot_exec_at_pi[h->thd_id] = h->tot_exec):
				(ret += h->tot_exec - ttc->tot_exec_at_pi[h->thd_id]);
		}
	}
	
	return ret;
}

static void
check_priority_inversion(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry)
{
	int i;
	struct thd_trace *d, *n, *m, *root;

	assert(ttc && entry);
	if (!ttn) return;

	/* add dependency */
	if (unlikely(pi_on(&last_entry, ttc->prio, entry, ttn->prio))) {
		root = &thd_trace[last_entry.para];
		assert(root);

		ttc->n	     = root->h;
		ttc->l	     = root;
		root->h	     = ttc;
		ttc->pi_start_t = root->tot_exec; // start accounting PI time
		ttc->pi_time    = 0;
		higher_exec(ttc, 0);
		return;
	}
	/* remove dependency ( update and check epoch )*/
	if (unlikely(pi_off(&last_entry, ttc->prio, entry, ttn->prio))) {
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
#ifdef DETECTION_MODE
			faulty_spd_localizer(root, entry, CONS_SCHEDULING);
#endif
		}
		
		m->pi_time = ttc->tot_exec - m->pi_start_t - higher_exec(m, 1);
		/* printc("thd %d pi_time %llu tot_exec %llu pi_start_t %llu track_all %llu\n",  */
		/*        m->thd_id, m->pi_time, ttc->tot_exec,  */
		/*        m->pi_start_t, higher_exec(m,1)); */
		
		log_or_det_pit(root, m, entry, m->pi_time);
		/* printc("After PI remove: "); */
		/* print_deps(m); */
		/* printc("thd %d is in PI for %llu\n", m->thd_id, m->pi_time); */
		m->pi_start_t = 0;
		root->h = m->n;
		
		return;
	}
	
	// some threads could still be in PI
	if (unlikely(entry->evt_type == EVT_LOG_PROCESS)) {
		for (i = 0; i < MAX_NUM_THREADS; i++) {
			struct thd_trace *l, *h;
			unsigned long long h_pitime;
			h = &thd_trace[i];
			if (unlikely(pi_in(h))) {
				/* printc("thd %d still in PI\n", h->thd_id); */
				l = h->l;
				h_pitime = l->tot_exec - h->pi_start_t - higher_exec(h, 1);
				log_or_det_pit(l, h, entry, h_pitime);
			}
		}
	}
	
	return;
}

/*************************************/
/*    Constraints Check  -- spdinvexec */
/*************************************/
static void
check_inv_spdexec(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry, unsigned long long up_t)
{
	int spdid, type;
	struct thd_specs *ttc_spec;
	assert(ttc && entry);
	assert(entry->evt_type > 0);
	
	if (!MONITOR(ttc)) return;

	ttc_spec = &thd_specs[ttc->thd_id];
	assert(ttc_spec);
	type = entry->evt_type;
	
	switch (type) {
	case EVT_SINV: // invocation number
		spdid = entry->to_spd;
		ttc->invocation[spdid]++;
		ttc->exec_oneinv_allspd[spdid] = ttc->tot_exec; // start invocation here
		ttc->exec_oneinv_in_spd[spdid] = 0;
#ifdef LOGEVENTS_MODE
		updatemax_int(&ttc_spec->invocation_max[spdid], ttc->invocation[spdid]);
		PRINTD_SPDEXEC("thd %d max inv_num %d by dl to spd %d\n", ttc->thd_id, ttc_spec->invocation_max[spdid], spdid);
#endif
#ifdef DETECTION_MODE
		if (ttc->invocation[spdid] > ttc_spec->invocation_max[spdid]) {
			faulty_spd_localizer(ttc, entry, CONS_INVOKNUM);
		}
#endif		
		break;
	case EVT_SRET: // invocation spd exec
		spdid = entry->from_spd;
		ttc->exec_oneinv_allspd[spdid] = ttc->tot_exec - ttc->exec_oneinv_allspd[spdid];
		ttc->exec_oneinv_in_spd[spdid] += up_t;
		ttc->exec_allinv_in_spd[spdid] += up_t;
#ifdef LOGEVENTS_MODE
		updatemax_llu(&ttc_spec->exec_oneinv_allspd_max[spdid], 
			      ttc->exec_oneinv_allspd[spdid]);
		updatemax_llu(&ttc_spec->exec_oneinv_in_spd_max[spdid], 
			      ttc->exec_oneinv_in_spd[spdid]);
		updatemax_llu(&ttc_spec->exec_allinv_in_spd_max[spdid], 
			      ttc->exec_allinv_in_spd[spdid]);

		if (ttc->exec_allinv_in_spd[spdid] > ttc->exec_allinv_in_spd_max) {
			ttc->exec_allinv_in_spd_max = ttc->exec_allinv_in_spd[spdid];
			ttc->exec_allinv_in_spd_max_spd = spdid;
		}

		PRINTD_SPDEXEC("thd %d max exec_oneinv_allspd %llu to spd %d\n",
		       ttc->thd_id, ttc_spec->exec_oneinv_allspd_max[spdid], spdid);
		PRINTD_SPDEXEC("thd %d max exec_oneinv_in_spd %llu in spd %d\n",
		       ttc->thd_id, ttc_spec->exec_oneinv_in_spd_max[spdid], spdid);
		PRINTD_SPDEXEC("thd %d max exec_allinv_in_spd %llu in spd %d\n",
		       ttc->thd_id, ttc_spec->exec_allinv_in_spd_max[spdid], spdid);
#endif
#ifdef DETECTION_MODE
		if (ttc->exec_oneinv_in_spd[spdid] > 
		    ttc_spec->exec_oneinv_in_spd_max[spdid]) {
			faulty_spd_localizer(ttc, entry, CONS_ONEINV_SPD_EXEC);
		}
		if (ttc->exec_allinv_in_spd[spdid] > 
		    ttc_spec->exec_allinv_in_spd_max[spdid]) {
			faulty_spd_localizer(ttc, entry, CONS_ALLINV_SPD_EXEC);
		}
#endif	
		break;
	case EVT_CINV:
	case EVT_NINT:
	case EVT_TINT:
	case EVT_CS:
		spdid = entry->from_spd;
		ttc->exec_oneinv_in_spd[spdid] += up_t;
		ttc->exec_allinv_in_spd[spdid] += up_t;
		break;
	default:
		break;
	}
	
	return;
}

/******************************************/
/*    Constraints Check  -- thread timing */
/******************************************/
static void
check_thd_timing(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry, unsigned long long up_t)
{
	int i;
	assert(ttc && entry);

	ttc->tot_exec += up_t;  // update each thread total execution time here.

	// a periodic task starts its execution
	if (ttn && MONITOR(ttn) && periodic_task_begin(&last_entry, ttn, entry)) {
#ifdef LOGEVENTS_MODE
		struct thd_specs *ttn_spec;
		ttn_spec = &thd_specs[ttn->thd_id];
		assert(ttn_spec);
		for (i = 0; i < MAX_NUM_SPDS; i++) {
			updatemax_int(&ttn_spec->invocation_max[i], ttn->invocation[i]);
		}
#endif
		ttn->exec = 0;
		ttn->dl_s = entry->time_stamp;
		ttn->dl_e = ttn->dl_s + ttn->p*CYC_PER_TICK;
		for (i = 0; i < MAX_NUM_SPDS; i++) {
			ttn->invocation[i] = 0;
			ttn->exec_allinv_in_spd[i] = 0;
		}
		return;
	}
	
	// a periodic task has started (with non-zero next deadline now)
	if (MONITOR(ttc) && ttc->dl_e) {
		ttc->exec  = ttc->exec + up_t;
#ifdef LOGEVENTS_MODE
		struct thd_specs *ttc_spec;
		ttc_spec = &thd_specs[ttc->thd_id];
		assert(ttc_spec);
		updatemax_llu(&ttc_spec->exec_max, ttc->exec); // no use for localization
		PRINTD_THD_TIMING("thd %d max exec by dl is %llu\n", ttc->thd_id, ttc_spec->exec_max);
#endif
#ifdef DETECTION_MODE
		if (unlikely(entry->time_stamp > ttc->dl_e)) {
			faulty_spd_localizer(ttc, entry, CONS_DLMISS);  // deadline miss
		} else if (ttc->exec > ttc->c) {  
			faulty_spd_localizer(ttc, entry, CONS_WCET);  // wcet overrun
		}
#endif
		/* printc("thd %d tot_exec %llu exec by dl %llu (next dl %llu) entry ts %llu\n",  */
		/*        ttc->thd_id, ttc->tot_exec, ttc->exec, ttc->dl_e, entry->time_stamp); */
	}
	return;
}

/*************************************/
/*    Constraints Check  -- Interrupt */
/*************************************/
/* check number of interrupts within a time window (e.g, period of the
   highest prio task, so the min time window) */
static void
check_interrupt(struct thd_trace *ttc, struct thd_trace *ttn, struct evt_entry *entry)
{
	struct thd_specs *ttc_spec;
	int type;

	assert(ttc && entry);
	assert(entry->evt_type > 0);
	type = entry->evt_type;
	ttc_spec = &thd_specs[ttc->thd_id];
	assert(ttc_spec);	

	switch (type) {
	case EVT_NINT:
		assert(entry->to_spd == NETIF_SPD);
		assert(entry->to_thd == NETWORK_THD);
		ttc->network_int++;  // still need this?
		network_interrupts++;
#ifdef LOGEVENTS_MODE
		updatemax_int(&network_int_max, network_interrupts);
#endif
#ifdef DETECTION_MODE
		if (network_interrupts > network_int_max) {
			faulty_spd_localizer(ttc, entry, CONS_NETINT);
		}
#endif		
		break;
	case EVT_TINT:
		assert(entry->to_spd == SCHED_SPD);
		assert(entry->to_thd == TIMER_THD);
		ttc->timer_int++;  // still need this?
		timer_interrupts++;
#ifdef LOGEVENTS_MODE
		updatemax_int(&timer_int_max, timer_interrupts);
		PRINTD_INTNUM("max timer interrupts %d in time window %llu\n", timer_int_max, window_sz);
#endif
#ifdef DETECTION_MODE
		if (timer_interrupts > timer_int_max) {
			faulty_spd_localizer(ttc, entry, CONS_TIMEINT);
		}
#endif		
		break;
	default:
		break;
	}

	return;
}

/*************************************/
/*    Constraints Check  -- Ordering */
/*************************************/
static void
check_ordering(struct thd_trace *ttc, struct evt_entry *c_entry)
{
	int flt_type;
	assert(ttc && c_entry);
	struct evt_entry *p_entry = &last_entry;
	assert(p_entry);
	
	int p_type = p_entry->evt_type; // previous event type
	if (unlikely(p_type == 0)) return;  // first time only
	int c_type = c_entry->evt_type; // this event type
	assert(c_type > 0);

	/* always check if event's time stamp in a spd is
	 * monotonically increasing */
	if (c_entry->time_stamp < logmon_info[evt_in_spd(c_entry)].spd_last_ts) {
		flt_type = CONS_SPDTIME; goto fault;		
	}

       /* whenever there is a change from a thread to another, check
	  there is a context switch or interrupt */
	if (p_entry->to_thd != c_entry->to_thd) {
		if (c_type != EVT_CS && c_type != EVT_TINT && c_type != EVT_NINT) {
			flt_type = CONS_CSINT; goto fault;
		}
	}
		    
	switch (p_type) {
	case EVT_CINV:
		switch (c_type) {
		case EVT_SINV: SINV_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		default: flt_type = CONS_CINV_TYPE; goto fault;
		}
		break;
	case EVT_SRET:
		switch (c_type) {
		case EVT_CRET: CRET_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		default: flt_type = CONS_SRET_TYPE; goto fault;
		}
		break;
	case EVT_CRET:
		switch (c_type) {
		case EVT_CINV: CINV_ORDER(); break;
		case EVT_SRET: SRET_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		case EVT_CS:   CS_ORDER();   break;
		default:
			flt_type = CONS_CRET_TYPE;
			goto fault;
		}
		break;
	case EVT_CS:
		switch (c_type) {
		case EVT_CINV: CINV_ORDER(); break;
		case EVT_SRET: SRET_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		case EVT_CS:   CS_ORDER();   break;
		default: flt_type = CONS_CS_TYPE; goto fault;
		}
		break;
	case EVT_NINT:
		switch (c_type) {
		case EVT_CINV: CINV_ORDER(); break;
		case EVT_SRET: SRET_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		default: flt_type = CONS_NINT_TYPE; goto fault;
		}
		break;
	case EVT_TINT:
		switch (c_type) {
		case EVT_CINV: CINV_ORDER(); break;
		case EVT_SRET: SRET_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		case EVT_CS:   CS_ORDER();   break;
		default: flt_type = CONS_NINT_TYPE; goto fault;
		}
		break;
	case EVT_SINV:
		switch (c_type) {
		case EVT_SRET:
			SRET_ORDER();
			/* here also check if thd returns back to
			 * where it is from */
			if (p_entry->from_spd != c_entry->to_spd) {
				flt_type = CONS_SRETSPD;
				goto fault;
			}
			break;
		case EVT_CINV: CINV_ORDER(); break;
		case EVT_NINT: NINT_ORDER(); break;
		case EVT_TINT: TINT_ORDER(); break;
		case EVT_CS:   CS_ORDER();   break;
		default: flt_type = CONS_SINV_TYPE; goto fault;
		}
		break;
	case EVT_LOG_PROCESS:
		break;
	default:
		break;
	}

done:
	/* printc("current entry :\n"); */
	/* print_evt_info(c_entry); */
	/* printc("last entry :\n"); */
	/* print_evt_info(p_entry); */
	
	return;
fault:
#ifdef DETECTIOC_MODE
	printc("event type and ordering issue :\n");
	faulty_spd_localizer(ttc, c_entry, flt_type);
#endif
	goto done;
}

/*************************************/
/*    Constraints Check  -- main  */
/*************************************/
static void 
constraint_check(struct evt_entry *entry)
{
	struct thd_trace *ttc, *ttn = NULL;
	unsigned long long up_t;
	int from_thd, from_spd;
	int owner = 0, contender = 0;  // for RB contention

	assert(entry && entry->evt_type > 0);
	
	slide_twindow(entry);

	// "paste" the omitted event here
	if (unlikely(entry->to_thd & CONT_FLAG)) {
		owner	      = entry->to_thd & 0x000000FF;
		contender     = entry->to_thd>>8 & 0x000000FF;
		entry->to_thd = entry->to_thd>>16 & 0x000000FF;
		assert(contender == entry->from_thd);
	}

	if (entry->evt_type == EVT_TINT || entry->evt_type == EVT_NINT) {
		if (likely(last_entry.from_thd && last_entry.from_spd)) {
			entry->from_thd = last_entry.to_thd;
			entry->from_spd = last_entry.to_spd;
		}
	}
	from_thd = entry->from_thd;
	from_spd = entry->from_spd;
	assert(from_thd && from_spd);
	
	// ttc tracks thread that runs up to this point (not the next)
	ttc = &thd_trace[from_thd];
	assert(ttc);
	if (unlikely(!ttc->last_ts)) ttc->last_ts = entry->time_stamp;
	
	int type = entry->evt_type;
	switch (type) {
	case EVT_CINV:
	case EVT_CRET:
		cap_to_dest(entry);
	case EVT_SINV:
	case EVT_SRET:
		assert(entry->to_thd == entry->from_thd);
		up_t = entry->time_stamp - ttc->last_ts;
		ttc->last_ts = entry->time_stamp;
		assert(!ttn);
		break;
	case EVT_CS:
		assert(entry->to_spd == SCHED_SPD);  
		assert(entry->from_thd != entry->to_thd);
	case EVT_TINT:
	case EVT_NINT:
		up_t = entry->time_stamp - ttc->last_ts;
		ttn = &thd_trace[entry->to_thd];
		assert(ttn);
		ttn->last_ts = entry->time_stamp;
		break;
	case EVT_LOG_PROCESS:  // log this for detecting PI
		assert(entry->to_spd == LLLOG_SPD);
		assert(entry->to_thd == MONITOR_THD);
		assert(entry->from_thd != entry->to_thd);
		ttn = &thd_trace[entry->to_thd];
		assert(ttn);
		break;
	default:
		printc("event type error\n");
		assert(0);
		break;
	}	

	/***************************/
	/* Constraint check begins */

        // run in the following orders
	check_ordering(ttc, entry);  
	check_interrupt(ttc, ttn, entry);
	
	check_thd_timing(ttc, ttn, entry, up_t);
	check_inv_spdexec(ttc, ttn, entry, up_t);
	check_priority_inversion(ttc, ttn, entry);

	/* Constraint check done ! */
	/***************************/

	if (likely(type != EVT_LOG_PROCESS)) {
		last_evt_update(&last_entry, entry);
		logmon_info[evt_in_spd(entry)].spd_last_ts = entry->time_stamp;
	}
	
	return;
}


/***********************/
/*    Fault Localizer  */
/***********************/

// find in which spd lower task spends the most time
static unsigned int
find_max_spdexec(struct thd_trace *l, struct evt_entry *entry, int flt_type)
{
	int i, ret = 0;
	assert(l && entry);
	assert(flt_type == CONS_PI || flt_type == CONS_WCET || flt_type == CONS_DLMISS);
	assert(MONITOR(l));

	return l->exec_allinv_in_spd_max_spd;
	/* unsigned long long tmp = 0;	 */
	/* for (i = 0; i < MAX_NUM_SPDS; i++) { */
	/* 	if (l->exec_allinv_in_spd[i] > tmp) { */
	/* 		tmp = l->exec_allinv_in_spd[i]; */
	/* 		ret = i; */
	/* 	} */
	/* } */
	
	/* return ret; */
}

static unsigned int
fltspd_alg1(struct thd_trace *ttc, struct evt_entry *entry, int flt_type)
{
	struct evt_entry *earliest_evt;
	assert(ttc && entry);
	assert(flt_type == CONS_CINV_TYPE || flt_type == CONS_SRET_TYPE);

	earliest_evt = find_next_evt(entry);
	
	if (!earliest_evt || earliest_evt->to_spd != entry->to_spd) {
		if (earliest_evt->evt_type == EVT_TINT ||
		    earliest_evt->evt_type == EVT_NINT) return earliest_evt->from_spd;
		else return entry->from_spd;
	} else {
		return entry->to_spd;
	}
}

// measurement for localizer only
#ifdef MEAS_LOG_LOCALIZATION
volatile unsigned long long max_localizer_cost;
volatile unsigned long long localizer_start, localizer_end;
#endif

/* return the faulty spd id, or 0 */
unsigned int
faulty_spd_localizer(struct thd_trace *ttc, struct evt_entry *entry, int flt_type)
{
	/* printc("find faulty spd here (fault type %d)\n", flt_type); */
	/* print_evt_info(entry); */
	unsigned int faulty_spd = 0;
	assert(ttc && entry);

#ifdef MEAS_LOG_LOCALIZATION
	rdtscll(localizer_start);
#endif
	struct evt_entry *p_entry = &last_entry;
	assert(p_entry);
	int p_type = p_entry->evt_type; // previous event type
	int c_type = entry->evt_type;   // current event type

	switch (flt_type) {
	case CONS_ONEINV_SPD_EXEC:
		faulty_spd = entry->from_spd;
		break;
	case CONS_ALLINV_SPD_EXEC:
		faulty_spd = entry->from_spd; // not sure....
		break;
        // excessive number of invocations to a spd
	case CONS_INVOKNUM:
		faulty_spd = entry->to_spd; // not sure....
		break;
	case CONS_WCET:
		faulty_spd = find_max_spdexec(ttc, entry, flt_type);
		break;
	case CONS_DLMISS:
		faulty_spd = find_max_spdexec(ttc, entry, flt_type);
		break;
	// incorrect scheduling decision after PI ends
	case CONS_SCHEDULING:
		faulty_spd = SCHED_SPD;
		break;
	// too long to be in PI state
	case CONS_PI:
		// dependency runs too long? where?????
		faulty_spd = find_max_spdexec(ttc, entry, flt_type);
		break;
	// too much timer interrupt in a time window
	case CONS_TIMEINT:
		faulty_spd = SCHED_SPD;
		break;
	// too much network interrupt in a time window
	case CONS_NETINT:
		faulty_spd = NETIF_SPD;
		break;
	// not sure
	case CONS_SRETSPD:
		faulty_spd = fltspd_alg1(ttc, entry, flt_type);
		break;
	// if time stamp in a spd is not monotonically increasing
	case CONS_SPDTIME:
		faulty_spd = evt_in_spd(entry);
		break;
        // missing interrupt or context switch evt
	case CONS_CSINT: 
		if (entry->to_thd == NETWORK_THD) faulty_spd = NETIF_SPD;
		else faulty_spd = SCHED_SPD;
		break;
	// sanity check finds that thread id or spd id does not match
	case CONS_ALTERID:
		faulty_spd = p_entry->to_spd;
		break;
	// EVT_CINV does not find the right following event
	case CONS_CINV_TYPE:
		faulty_spd = fltspd_alg1(ttc, entry, flt_type);
		break;
	// EVT_SINV does not find the right following event
	case CONS_SINV_TYPE:
		if (entry->evt_type == EVT_SINV) faulty_spd = entry->from_spd;
		else if (entry->evt_type == EVT_CRET) faulty_spd = entry->from_spd;
		break;
	// EVT_CRET does not find the right following event
	case CONS_CRET_TYPE:
		if (entry->evt_type == EVT_SINV) faulty_spd = entry->from_spd;
		else if (entry->evt_type == EVT_CRET) faulty_spd = entry->from_spd;
		break;
	// EVT_SRET does not find the right following event
	case CONS_SRET_TYPE:
		faulty_spd = fltspd_alg1(ttc, entry, flt_type);
		break;
	// EVT_CS does not find the right following event
	case CONS_CS_TYPE:
		if (entry->evt_type == EVT_SINV) faulty_spd = SCHED_SPD;
		else if (entry->evt_type == EVT_CRET) faulty_spd = SCHED_SPD;
		else if (entry->evt_type == EVT_CS)   faulty_spd = SCHED_SPD;
		break;
	// EVT_NINT does not find the right following event
	case CONS_NINT_TYPE:
		if (entry->evt_type == EVT_SINV) faulty_spd = NETIF_SPD;
		else if (entry->evt_type == EVT_CRET) faulty_spd = NETIF_SPD;
		break;
	// EVT_TINT does not find the right following event
	case CONS_TINT_TYPE:
		if (entry->evt_type == EVT_SINV) faulty_spd = SCHED_SPD;
		else if (entry->evt_type == EVT_CRET) faulty_spd = SCHED_SPD;
		break;
	default:
		printc("constraint violation type invalid \n");
		assert(0);
		break;
	}	

#ifdef MEAS_LOG_LOCALIZATION
	rdtscll(localizer_end);
	if (localizer_end - localizer_start > max_localizer_cost) {
		max_localizer_cost = localizer_end - localizer_start;
	}
	printc("fault spd localizer overhead %llu ", localizer_end - localizer_start);
	printc(" max overhead %llu", max_localizer_cost);
	printc(" found faulty spd %d\n", faulty_spd);
#endif

	return 0;
}

#endif
