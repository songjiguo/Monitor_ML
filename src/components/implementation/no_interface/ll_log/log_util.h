/*  Jiguo :  utility functions for log processing / constraint check */

#ifndef LOG_UTIL
#define LOG_UTIL

#include <log_process.h>

/* get a page from the heap */
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

/* return which spd the event is logged in */
static int
evt_in_spd (struct evt_entry *entry)
{
	int logged_in_spd = 0;
	assert(entry);
	switch (entry->evt_type) {
	case EVT_CINV:
	case EVT_SRET:
		logged_in_spd = entry->from_spd;
		break;
	case EVT_SINV:
	case EVT_CRET:
	case EVT_TINT:
	case EVT_NINT:
	case EVT_CS:
		logged_in_spd = entry->to_spd;
		break;
	default:
		break;
	}
	
	return logged_in_spd;
}

/*************************/
/* PI related functions  */
/*************************/
/* PI begin condition:
   1) The dependency from high to low (sched_block(spdid, low))
   2) A context switch from high thd to low thd
*/
static int
pi_begin(struct evt_entry *pe, int p_prio, 
	   struct evt_entry *ce, int c_prio)
{
	return (ce->evt_type == EVT_CS &&
		ce->from_thd != ce->to_thd &&
		p_prio < c_prio &&
		pe->evt_type == EVT_SINV && 
		pe->func_num == FN_SCHED_BLOCK &&
		pe->para > 0);
}

/* PI end condition:
   1) Low thd wakes up high thd (sched_wakeup(spdid, high)); 
   2) A context switch from low to high
*/
static int
pi_end(struct evt_entry *pe, int p_prio, 
	   struct evt_entry *ce, int c_prio)
{
	return (ce->evt_type == EVT_CS &&
		ce->from_thd != ce->to_thd &&
		p_prio > c_prio &&
		pe->evt_type == EVT_SINV && 
		pe->func_num == FN_SCHED_WAKEUP &&
		pe->para > 0);
}

/*************************/
/* heap related functions  */
/*************************/
struct hevtentry {
	int index, spdid;
	unsigned long long ts;
}es[MAX_NUM_SPDS];

static int evtcmp(void *a, void *b)
{
	return ((struct hevtentry *)a)->ts >= ((struct hevtentry *)b)->ts;
}

static void evtupdate(void *a, int pos)
{
	((struct hevtentry *)a)->index = pos;
}

static void 
validate_heap_entries(struct heap *hp, int amnt)
{
	int i;
	struct hevtentry *prev;
	prev = hp->data[1];
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		struct hevtentry *curr = heap_highest(hp);
		if (!evtcmp((struct hevtentry*)prev, (struct hevtentry*)curr)) assert(0);
		printc("earliest spdid %d -- ts %llu\n", 
		       curr->spdid, LLONG_MAX - curr->ts);
		prev = curr;
	}
	assert(!heap_highest(hp));
	assert(heap_size(hp) == 0);
	
	return;
}

static struct heap *log_heap_alloc(int max_sz, cmp_fn_t c, update_fn_t u)
{
	struct heap *h;

	// assume a page for now
	h = (struct heap *)llog_get_page();
	if (NULL == h) return NULL;

	h->max_sz = max_sz+1;
	h->e = 1;
	h->c = c;
	h->u = u;
	h->data = (void *)&h[1];

	return h;
}

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

#endif   // LOG_UTIL

