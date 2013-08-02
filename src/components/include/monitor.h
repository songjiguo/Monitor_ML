/*
   The header file for only clients interface, mainly used for the log monitor
   latent fault (state machine)
*/

#ifndef   	MONITOR_H
#define   	MONITOR_H

#include <cos_list.h>
#include <ck_ring_cos.h>

#define MEAS_WITH_LOG
//#define MEAS_WITHOUT_LOG

extern vaddr_t lm_init(spdid_t spdid);
extern int lm_process(spdid_t spdid);

/* event entry in the ring buffer (per event) */
struct event_info {
	int thd_id;
	spdid_t from_spd;
	int dest_info;  // can be cap_no for dest spd or return spd from the stack
	unsigned long long time_stamp;
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
	if (capacity == size + 1) {
		/* printc("evt ring (spd %ld) is full\n", cos_spd_id()); */
		/* printc("cap %d size %d\n", capacity, size); */
		return 1;   // FIXME: the max size is not same as capacity??
	}
	return 0;
}

static inline void monevt_conf(struct event_info *monevt, int param)
{
	unsigned long long ts;
	rdtscll(ts);
	monevt->time_stamp = ts;

	monevt->thd_id = cos_get_thd_id();
	monevt->from_spd = cos_spd_id();
	monevt->dest_info = param;

	return;
}

static inline void monevt_enqueue(int param)
{
	struct event_info monevt;
	int capacity, size;

	if (unlikely(ring_is_full())) lm_process(cos_spd_id());

	monevt_conf(&monevt, param);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));
	return;
}

#endif
