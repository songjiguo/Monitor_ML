/*
   The header file for only clients interface, mainly used for the log monitor
   latent fault (state machine)
*/
#include <cos_list.h>
#include <ck_ring_cos.h>

extern vaddr_t lm_init(spdid_t spdid);
extern int lm_process(spdid_t spdid);

enum{
	INV_CLI1,   // when the invocation starts in the client
	INV_CLI2,   // when the invocation returns in the client
	INV_SER1,   // when the execution starts in the server
	INV_SER2    // when the execution returns in the server
};

/* event entry in the ring buffer (per event) */
struct event_info {
	int thd_id;
	spdid_t from_spd;
	int type; // interface function can be (cli , ser , ret, ...)
	int dest_info;  // can be cap_no for dest spd or return spd from the stack
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
	if (capacity == size + 1) {
		printc("cap %d size %d\n", capacity, size);
		return 1;   // FIXME: the max size is not same as capacity??
	}
	return 0;
}

static inline void monevt_conf(struct event_info *monevt, int param, int type)
{
	unsigned long long ts;
	rdtscll(ts);
	monevt->time_stamp = ts;

	monevt->thd_id = cos_get_thd_id();
	monevt->from_spd = cos_spd_id();
	monevt->dest_info = param;
	monevt->type = type;

	INIT_LIST(monevt, next, prev);

	return;
}

static inline void monevt_enqueue(int param, int type)
{
	struct event_info monevt;
	int capacity, size;

	if (unlikely(ring_is_full())) {
		printc("evt ring (spd %ld) is full\n", cos_spd_id());
		lm_process(cos_spd_id());
	}

	if (type == INV_SER2) {   // Get the returned spd, only occurs at server side when return to cli
		assert(param == 0);
		param = cos_thd_cntl(COS_THD_INV_FRAME, cos_get_thd_id(), 1, 0);
		if (param <= 0) {
			printc("can not get the returned spd\n");
			assert(0);
		}
	}

	monevt_conf(&monevt, param, type);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));
	return;
}
