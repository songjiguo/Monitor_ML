/*
   The header file for only clients interface, mainly used for the log monitor
   latent fault (state machine)
*/
#include <cos_list.h>
#include <ck_ring_cos.h>

extern vaddr_t lm_init(spdid_t spdid);
extern int lm_process(spdid_t spdid);

/* event entry in the ring buffer (per event) */
struct event_info {
	int event_id;
	int call_ret; // interface function call should be a pair (call + return), test only!!
	int thd_id;
	spdid_t spd_id;
	unsigned long long time_stamp;

	struct event_info *next, *prev;  // track the events that belong to the same thread
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(event_info, logevts_ring);
CK_RING_INSTANCE(logevts_ring) *cli_ring;
#endif

static inline int ring_is_full()
{
	int capacity, size;

	assert(cli_ring);
	capacity = CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	if (capacity == size) {
		printc("cap %d size %d\n", capacity, size);
		return 1;   // FIXME: the max size is not same as capacity??
	}
	return 0;
}

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

	return;
}

/* only done by the clients  */
static inline void monevt_enqueue(int event_id, int call_ret)
{
	struct event_info monevt;
	int capacity, size;

	assert(event_id);
	assert(call_ret); // 1,2,3,4 represents the local event flow of an interface invocation 1->2->3->4
	assert(cli_ring);

	if (unlikely(ring_is_full())) {
		printc("evt ring (spd %ld) is full\n", cos_spd_id());
		lm_process(cos_spd_id());
	}

	monevt_conf(&monevt, event_id, call_ret);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));
	return;
}

/* only done by the log monitor  */
static inline void monevt_dequeue(CK_RING_INSTANCE(logevts_ring) *ring, struct event_info *monevt)
{
	int capacity, size;

	assert(ring);

	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)ring));
	if (unlikely (size == 0)) {
		printc("Queue is already empty before dequeue\n");
		return;
	}

	/* printc("CALL DEQUEUE szie %d!!!!\n", size); */
	while (CK_RING_DEQUEUE_SPSC(logevts_ring, ring, monevt) == 0);
	return;
}
