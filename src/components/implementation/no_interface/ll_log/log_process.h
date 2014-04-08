#ifndef   	LOG_PROCESS_H
#define   	LOG_PROCESS_H

/* Data structure for tracking information in log_manager */

#include <ll_log.h>
#include <hard_code_thd_spd.h>   // TODO: replace this with the name space
#include <heap.h>

/* ring buffer for event flow (one RB per component) */
struct logmon_info {
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
	
	struct evt_entry first_entry;
};

// FIXME: might not need save all information in log_process
/* per-thread event flow, updated within the log_monitor  */
struct thd_trace {
	int thd_id;
	int prio;  // hard code priority for PI testing
	
	// d and c for a periodic task
	unsigned long long period;
	unsigned long long execution;

	// Execution time of this thread
	unsigned long long tot_exec;
	// how long this thread has been in PI?
	unsigned long long pi_time; 
	// Last time stamp for this thread
	unsigned long long last_ts;

	// number of timer interrupt while this thread running
	unsigned long long timer_int;
	// number of network interrupt while this thread running
	unsigned long long network_int; 

        // used to detect scheduling decision
	unsigned long long epoch;
        // used to detect deadlock
	unsigned long long depth;
        // used to indicate the status of a thread (place holder)
	int status;

	// WCET in a spd due to an invocation to that spd by this thread
	unsigned long long inv_spd_exec[MAX_NUM_SPDS];
	// number of invocations made to a component by this thread
	unsigned int invocation[MAX_NUM_SPDS];
	
};

struct logmon_info logmon_info[MAX_NUM_SPDS];
struct thd_trace thd_trace[MAX_NUM_THREADS];
struct evt_entry last_entry;

/* /\* heap functions for next earliest event *\/ */
/* static int evt_cmp(void *a, void *b) */
/* { */
/* 	struct evt_entry *e1 = a, *e2 = b; */
/* 	return e1->time_stamp >= e2->time_stamp; */
/* } */

/* static void evt_update(void *a, int pos) */
/* { */
/* 	((struct evt_entry *)a)->index = pos; */
/* } */

static void 
init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id	 = i;

		thd_trace[i].period	    = 0;  // same as deadline d
		thd_trace[i].execution	    = 0;  // same as period p

		// timing info for a thread
		thd_trace[i].tot_exec = 0;    // the total exec time for thd
		thd_trace[i].pi_time = 0; // how long a thread is in PI status
		thd_trace[i].last_ts = 0;// the last time stamp for this thd

		// number of events 
		thd_trace[i].timer_int = 0;  // number of timer INT while thd
		thd_trace[i].network_int = 0; // number of network INT while thd
		// dependency tree info
		thd_trace[i].epoch = 0; // used to detect scheduling decision
		thd_trace[i].depth = 0; // used to detect deadlock
		thd_trace[i].status = 0;// ?? place holder

		// spd related (number of events and timing)
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].inv_spd_exec[j] = 0;
			thd_trace[i].invocation[j]   = 0;
		}
	}
	return;
}

// allocate a page from the heap
static inline void *
llog_get_page()
{
	char *addr = NULL;
	
	addr = cos_get_heap_ptr();
	if (!addr) goto done;
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
	if ((vaddr_t)addr != 
	    __mman_get_page(cos_spd_id(), (vaddr_t)addr, 0)) {
		printc("fail to get a page in logger\n");
		assert(0); // or return -1?
	}
done:
	return addr;
}

static int
cap_to_dest(unsigned long long cap_no)
{
	if (cap_no <= MAX_NUM_SPDS) return (int)cap_no;
	return cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, cap_no);
}

static void
update_dest_spd(struct evt_entry *entry)
{
	int d;
	assert(entry && entry->evt_type > 0);

	if (entry->evt_type == EVT_CINV) {
		if ((d = cap_to_dest(entry->to_spd) <= 0)) assert(0);
		entry->to_spd = d;
	}
	else if (entry->evt_type == EVT_CRET) {
		if ((d = cap_to_dest(entry->to_spd) <= 0)) assert(0);
		entry->from_spd = d;
	}

	return;
}

static void
check_priority_inversion(struct thd_trace *ttc, struct evt_entry *entry)
{
	assert(ttc && entry);
	
	

	return;
}

//  Do we still nee this? 
/* static void */
/* check_invocation_stack(struct thd_trace *ttc, struct evt_entry *entry) */
/* { */
/* 	assert(ttc && entry); */
/* 	return; */
/* } */

static void
check_ordering(struct thd_trace *ttc, struct evt_entry *entry)
{
	assert(ttc && entry);
	
	int p_type = last_entry.evt_type; // previous event type
	if (p_type == 0) return;
	int n_type = entry->evt_type; // next event type
	assert(n_type > 0);

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

	// TODO: check the casual ordering here if something wrong
	// ...
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
	if (entry->evt_type == EVT_NINT || entry->evt_type == EVT_NINT) {
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
			printc("<<thd %d in spd %d in one invocation is %llu >>\n", ttc->thd_id, spdid, ttc->inv_spd_exec[spdid]);
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
check_timing(struct evt_entry *entry)
{
	struct thd_trace *ttc, *ttn;
	unsigned long long up_t;
	int from_thd, from_spd;

	assert(entry);
	// for CINV and CRET, convert cap_no to spdid here
	/* update_dest_spd(entry); */
	assert(entry->evt_type > 0);
	
	if (entry->evt_type == EVT_NINT || entry->evt_type == EVT_NINT) {
		if (last_entry.from_thd && last_entry.from_spd) {
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
	if (ttc->last_ts == 0) ttc->last_ts = entry->time_stamp;
	
	switch (entry->evt_type) {
	case EVT_CINV:
	case EVT_SINV:
	case EVT_SRET:
	case EVT_CRET:
		assert(entry->to_thd == entry->from_thd);
		up_t = entry->time_stamp - ttc->last_ts;
		ttc->tot_exec += up_t;
		ttc->last_ts = entry->time_stamp;
		break;
	case EVT_CS:
                // lock in sched, mm and booter?
		/* assert(entry->from_spd == SCHED_SPD); */  
		assert(entry->to_thd != entry->from_thd);
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
	printc("thd %d total exec %llu \n", ttc->thd_id, ttc->tot_exec);
	// TODO: check thread WCET here
	// ...
	
	check_inv_spdexec(ttc, ttn, entry, up_t);
	check_interrupt(ttc, entry);
	check_invocation(ttc, entry);
	check_ordering(ttc, entry);
	
        // save info for EVT_INT
	last_entry.from_thd = entry->to_thd;
	last_entry.from_spd = entry->to_spd; 
	last_entry.evt_type = entry->evt_type;
	
	return;
}

/**************************/
/*    Dependency Tree     */
/**************************/

#define CSLAB_ALLOC(sz)   llog_get_page()
#define CSLAB_FREE(x, sz) 
#include <cslab.h>
struct dependecy {
	u16_t   flags;
	int thd_id;
	
	struct thd_trace *t;
	/* child and sibling mappings */
	struct dependecy *p, *c, *_s, *s_;
} __attribute__((packed));
CSLAB_CREATE(dependecy, sizeof(struct dependecy));

static void
dependency_init(struct dependency *m, spdid_t spdid, vaddr_t a, struct dependency *p, struct frame *f)
{
	assert(m && f);
	INIT_LIST(m, _s, s_);
	m->f     = f;
	m->flags = 0;
	m->spdid = spdid;
	m->addr  = a;
	m->p     = p;
	if (p) {
		m->flags = p->flags;
		if (!p->c) p->c = m;
		else       ADD_LIST(p->c, m, _s, s_);
	}
}

static struct dependency *
dependency_lookup(spdid_t spdid, vaddr_t addr)
{
	struct comp_vas *cv = cvas_lookup(spdid);

	if (!cv) return NULL;
	return cvect_lookup(cv->pages, addr >> PAGE_SHIFT);
}

/* Make a child dependency */
static struct dependency *
dependency_crt(struct dependency *p, struct frame *f, spdid_t dest, vaddr_t to)
{
	struct comp_vas *cv = cvas_lookup(dest);
	struct dependency *m = NULL;
	long idx = to >> PAGE_SHIFT;

	assert(!p || p->f == f);
	assert(dest && to);

	/* no vas structure for this spd yet... */
	if (!cv) {
		cv = cvas_alloc(dest);
		if (!cv) goto done;
		assert(cv == cvas_lookup(dest));
	}
	assert(cv->pages);
	if (cvect_lookup(cv->pages, idx)) goto collision;

	cvas_ref(cv);
	m = cslab_alloc_dependency();
	if (!m) goto collision;

	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, dest, to, frame_index(f))) {
		printc("mem_man: could not grant at %x:%d\n", dest, (int)to);
		goto no_dependency;
	}
	dependency_init(m, dest, to, p, f);
	assert(!p || frame_nrefs(f) > 0);
	frame_ref(f);
	assert(frame_nrefs(f) > 0);
	if (cvect_add(cv->pages, m, idx)) BUG();
done:
	return m;
no_dependency:
	cslab_free_dependency(m);
collision:
	cvas_deref(cv);
	m = NULL;
	goto done;
}

/* Take all decedents, return them in a list. */
static struct dependency *
__dependency_linearize_decendents(struct dependency *m)
{
	struct dependency *first, *last, *c, *gc;
	
	first = c = m->c;
	m->c = NULL;
	if (!c) return NULL;
	do {
		last = LAST_LIST(first, _s, s_);
		c->p = NULL;
		gc = c->c;
		c->c = NULL;
		/* add the grand-children onto the end of our list of decedents */
		if (gc) ADD_LIST(last, gc, _s, s_);
		c = FIRST_LIST(c, _s, s_);
	} while (first != c);
	
	return first;
}

static void
__dependency_destroy(struct dependency *m)
{
	struct comp_vas *cv;
	int idx;

	assert(m);
	assert(EMPTY_LIST(m, _s, s_));
	assert(m->p == NULL);
	assert(m->c == NULL);
	cv = cvas_lookup(m->spdid);

	assert(cv && cv->pages);
	assert(m == cvect_lookup(cv->pages, m->addr >> PAGE_SHIFT));
	cvect_del(cv->pages, m->addr >> PAGE_SHIFT);
	cvas_deref(cv);

	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, m->spdid, m->addr, 0);
	assert(idx == frame_index(m->f));
	frame_deref(m->f);
	cslab_free_dependency(m);
}

static void
dependency_del_children(struct dependency *m)
{
	struct dependency *d, *n; 	/* decedents, next */

	assert(m);
	d = __dependency_linearize_decendents(m);
	while (d) {
		n = FIRST_LIST(d, _s, s_);
		REM_LIST(d, _s, s_);
		__dependency_destroy(d);
		d = (n == d) ? NULL : n;
	}
	assert(!m->c);
}

static void
dependency_del(struct dependency *m)
{
	assert(m);
	dependency_del_children(m);
	assert(!m->c);
	if (m->p && m->p->c == m) {
		if (EMPTY_LIST(m, _s, s_)) m->p->c = NULL;
		else                       m->p->c = FIRST_LIST(m, _s, s_);
	}
	m->p = NULL;
	REM_LIST(m, _s, s_);
	__dependency_destroy(m);
}

//FIXME: name
/* compute the highest power of 2 less or equal than 32-bit v */
static unsigned int get_powerOf2(unsigned int orig) {
        unsigned int v = orig - 1;

        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;

        return (v == orig) ? v : v >> 1;
}

#endif
