/**
   log monitor for latent fault (track all events)
 */

#ifndef LOGMON_H
#define LOGMON_H

/* Test define  */
#define TEST_INIT_RING
#define TEST_ENQU_RING

#include <cos_component.h>
#include <sched.h>

#include <cos_list.h>

#include <ck_ring_cos.h>

vaddr_t lm_init(spdid_t spdid);
int lm_process(spdid_t spdid);


/* Not OK to maintain a list for the same thread, some events can happen much later
   Can have a data structure per-thread as well to help lookup?? do not know how to make shared at the same time
   We can think the ring buffers maintained in logmonitor as a matrix as following (for process the info)

     (x)           ringBuffers Position (y)
    spdA   * * * * * * * * * * * * * * * * * *
    spdB   * * * * * * * * * * * * * * * * * *
    ...    . . . .. . . .. . . .. . . .. . . .
    spdN   * * * * * * * * * * * * * * * * * *
    spdM   * * * * * * * * * * * * * * * * * *

    We can maintain {x,y} info for each thread as following:

    thd1 {x,y}{x,y}{x,y}{x,y}{x,y}{x,y}.....
    .......
    thdn {x,y}{x,y}{x,y}{x,y}{x,y}{x,y}.....

    such that when track each thread execution path, we can use {x,y} info to locate where it is in the matrix
*/

/*
  Ring Buffer Related
*/
/* ring buffer for event flow (per spd) */
struct logmon_info
{
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
};

/* ring buffer for context switch (only one) */
struct logmon_cs
{
	vaddr_t mon_csring;
	vaddr_t sched_csring;
};

/* event entry in the ring buffer (per event) */
struct event_info {
	int event_id;
	int call_ret; // interface function call should be a pair (call + return), test only!!
	int thd_id;
	spdid_t spd_id;
	unsigned long long time_stamp;

	struct event_info *next, *prev;  // track the events that belong to the same thread
};

/*
  Track related
*/
/* per-thread event flow, updated within the log_monitor  */
struct thd_trace {
	int thd_id;
	unsigned long long exec[MAX_NUM_SPDS]; // avg
	unsigned long long tot_spd_exec[MAX_NUM_SPDS]; // total exec in spd
	unsigned long long wcet[MAX_NUM_SPDS];
	unsigned long long tot_exec;
	unsigned long long tot_wcet;
	struct event_info *entry;
};

/* per-spd event flow, updated within the log_monitor  */
struct spd_trace {
	spdid_t spd_id;
	struct event_info *entry;
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(event_info, logevts_ring);
CK_RING_INSTANCE(logevts_ring) *cli_ring;
#endif

static inline void monevt_conf(struct event_info *monevt, int event_id, int call_ret)
{
	unsigned long long ts;
	rdtscll(ts);
	monevt->time_stamp = ts;

	monevt->thd_id = cos_get_thd_id();
	monevt->spd_id = cos_spd_id();
	monevt->event_id = event_id;
	monevt->call_ret = call_ret;

	INIT_LIST(monevt, next, prev);

	/* printc("event id %p\n", (void *)monevt->event_id); */
	/* printc("call or return? %d\n", monevt->call_ret); */
	/* printc("thd id %d\n", monevt->thd_id); */
	/* printc("caller spd %d\n", monevt->spd_id); */
	/* printc("time_stamp %llu\n\n", monevt->time_stamp); */

	return;
}

static inline void monevt_enqueue(int event_id, int call_ret)
{
	struct event_info monevt;
	int capacity, size;

	assert(event_id);
	assert(call_ret); // 1,2,3,4 represents the local event flow of an interface invocation 1->2->3->4
	assert(cli_ring);

	capacity = CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	if (unlikely (capacity == size)) lm_process(cos_spd_id());

	monevt_conf(&monevt, event_id, call_ret);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));
	return;
}

static inline void monevt_dequeue(CK_RING_INSTANCE(logevts_ring) *ring, struct event_info *monevt)
{
	int capacity, size;

	assert(ring);

	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)ring));
	if (unlikely (size == 0)) {
		printc("Queue is already empty before dequeue\n");
		return;
	}

	while (CK_RING_DEQUEUE_SPSC(logevts_ring, ring, monevt));
	return;
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

#endif /* LOGMON_H*/ 
