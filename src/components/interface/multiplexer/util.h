/*  Jiguo :  utility functions for log processing / constraint check */

#ifndef FILTER_UTIL
#define FILTER_UTIL

#include <log.h>
#include <filter.h>

/* required timing data. Same as sched_timing.h */
#define CPU_FREQUENCY  (CPU_GHZ*1000000000)
#define TIMER_FREQ     (CPU_TIMER_FREQ)
#define CYC_PER_TICK   (CPU_FREQUENCY/TIMER_FREQ)

#ifdef LOGMGR_DEBUG_CRA_PI
#define PRINTD_PI(s, args...) printc(s, args);
#else
#define PRINTD_PI(s, args...) 
#endif

#ifdef LOGMGR_DEBUG_CRA_SPDEXEC
#define PRINTD_SPDEXEC(s, args...) printc(s, args);
#else
#define PRINTD_SPDEXEC(s, args...) 
#endif

#ifdef LOGMGR_DEBUG_CRA_THD_TIMING
#define PRINTD_THD_TIMING(s, args...) printc(s, args);
#else
#define PRINTD_THD_TIMING(s, args...) 
#endif

#ifdef LOGMGR_DEBUG_CRA_INTNUM
#define PRINTD_INTNUM(s, args...) printc(s, args);
#else
#define PRINTD_INTNUM(s, args...) 
#endif

#ifdef LOGMGR_DEBUG_CRA_ORDER
#define PRINTD_ORDER(s, args...) printc(s, args);
#else
#define PRINTD_ORDER(s, args...) 
#endif

static void
updatemax_llu(unsigned long long *p, unsigned long long val) { if (val > *p) *p = val; return;}
static void
updatemax_int(unsigned int *p, unsigned int val) { if (val > *p) *p = val; return; }


/* casual ordering constraint macros */
#define CINV_ORDER()						\
	if (p_entry->to_thd != c_entry->from_thd ||	        \
	    p_entry->to_spd != c_entry->from_spd ) {		\
		flt_type = CONS_ALTERID;			\
		goto fault;					\
	}							\
	
#define SINV_ORDER()						\
	if (p_entry->to_thd != c_entry->from_thd ||             \
	    p_entry->from_spd != c_entry->from_spd ||		\
	    p_entry->to_spd != c_entry->to_spd ) {		\
		flt_type = CONS_ALTERID;			\
		goto fault;					\
	}							\

#define SRET_ORDER()                                            \
	CINV_ORDER()						\
	
#define CRET_ORDER()						\
	if (p_entry->to_thd != c_entry->from_thd ||		\
	    p_entry->from_spd != c_entry->from_spd ||		\
	    p_entry->to_spd != c_entry->to_spd ) {		\
		flt_type = CONS_ALTERID;			\
		goto fault;					\
	}							\

#define CS_ORDER()                                              \
	if (p_entry->to_thd != c_entry->from_thd ||		\
	    c_entry->to_thd == c_entry->from_thd ||		\
	    p_entry->to_spd != c_entry->from_spd ||		\
	    c_entry->from_spd != SCHED_SPD) {			\
		flt_type = CONS_CS_TYPE;			\
		goto fault;					\
	}							\
	
#define NINT_ORDER()						\
	if (c_entry->to_thd != NETWORK_THD ||			\
	    c_entry->to_spd != NETIF_SPD) {			\
		flt_type = CONS_NINT_TYPE;			\
		goto fault;					\
	}							\
	if (c_entry->from_thd != p_entry->to_thd ) {		\
		flt_type = CONS_ALTERID;			\
		goto fault;					\
	}							\
	
#define TINT_ORDER()						\
	if (c_entry->to_thd != TIMER_THD ||			\
	    c_entry->to_spd != SCHED_SPD ) {			\
		flt_type = CONS_TINT_TYPE;			\
		goto fault;					\
	}							\
	if (c_entry->from_thd != p_entry->to_thd ) {		\
		flt_type = CONS_ALTERID;			\
		goto fault;					\
	}							\
	


/* return which spd the event is logged in */
static int
evt_in_spd(struct evt_entry *entry)
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
	case EVT_LOG_PROCESS:
		logged_in_spd = entry->to_spd;
		break;
	default:
		break;
	}
	
	return logged_in_spd;
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

#endif   // FILTER_UTIL

