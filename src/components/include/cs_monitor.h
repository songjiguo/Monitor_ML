/* Header file for tracking context switch, within scheduler(cos_scheduler.h) */

#include <ck_ring_cos.h>

// context tracking
struct cs_info {
	int curr_thd;
	int next_thd;
	unsigned long long time_stamp;
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __CSRING_DEFINED
#define __CSRING_DEFINED
CK_RING(cs_info, logcs_ring);
CK_RING_INSTANCE(logcs_ring) *cs_ring;
#endif

static inline int csring_is_full()
{
	int capacity, size;

	assert(cs_ring);
	capacity = CK_RING_CAPACITY(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	size = CK_RING_SIZE(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	
	if (capacity == size + 50) {
		return 1;
	}

	return 0;
}

static inline void moncs_conf(struct cs_info *moncs)
{
	unsigned long long ts;

	moncs->curr_thd = cos_get_thd_id();
	rdtscll(ts);
	moncs->time_stamp = ts;

	return;
}

/* only done by scheduler  */
static inline void moncs_enqueue(unsigned short int thd_id)
{
	struct cs_info moncs;
	
	assert(cs_ring);
	moncs_conf(&moncs);
	moncs.next_thd = thd_id;

        while (unlikely(!CK_RING_ENQUEUE_SPSC(logcs_ring, cs_ring, &moncs)));

	return;
}

/* only done by the log monitor  */
static inline void moncs_dequeue(CK_RING_INSTANCE(logcs_ring) *ring, struct cs_info *moncs)
{
	int capacity, size;

	assert(ring);

	size = CK_RING_SIZE(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)ring));
	if (unlikely (size == 0)) {
		printc("Queue is already empty before dequeue\n");
		return;
	}

	/* printc("CALL DEQUEUE szie %d!!!!\n", size); */
	while (CK_RING_DEQUEUE_SPSC(logcs_ring, ring, moncs) == 0);
	return;
}
