/*
   Jiguo:
   The header file for only interface, used for the log monitor
   latent fault (state machine)
*/

#ifndef LOGGER_H
#define LOGGER_H

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
extern vaddr_t llog_cs_init(spdid_t spdid, vaddr_t addr);
extern int llog_process(spdid_t spdid);
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);

#include "../implementation/sched/cos_sched_sync.h"
#define SCHED_SPD 3

#include <cos_list.h>
#include <ck_ring_cos.h>

#define MEAS_WITH_LOG
//#define MEAS_WITHOUT_LOG

/* event entry in the ring buffer (per event) */
struct event_info {
	int thd_id;
	spdid_t from_spd;
	int dest_info;  // can be cap_no for dest spd or return spd from the stack
	unsigned long long time_stamp;
};

// context tracking
struct cs_info {
	int curr_thd;
	int next_thd;
	unsigned long long time_stamp;
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(event_info, logevts_ring);
CK_RING_INSTANCE(logevts_ring) *cli_ring;
CK_RING(cs_info, logcs_ring);
CK_RING_INSTANCE(logcs_ring) *cs_ring;
#endif

// Events monitoring
static inline int 
ring_is_full()
{
	int capacity, size;

	assert(cli_ring);
	capacity = CK_RING_CAPACITY(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	size = CK_RING_SIZE(logevts_ring, (CK_RING_INSTANCE(logevts_ring) *)((void *)cli_ring));
	if (capacity == size + 1) {
		printc("evt ring (spd %ld) is full\n", cos_spd_id());
		printc("cap %d size %d\n", capacity, size);
		return 1;   // FIXME: the max size is not same as capacity??
	}
	return 0;
}

static inline void 
monevt_conf(struct event_info *monevt, int param)
{
	unsigned long long ts;
	rdtscll(ts);
	monevt->time_stamp = ts;

	monevt->thd_id = cos_get_thd_id();
	monevt->from_spd = cos_spd_id();
	monevt->dest_info = param;

	return;
}

static inline void 
monevt_enqueue(int param)
{
	struct event_info monevt;

	if (cos_get_thd_id() == 5) return;  // do not track booter initial thread here, test


	if (unlikely(!cli_ring)) {
		printc("evt enqueue (spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id());
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cli_addr);
		if (!(cli_ring = (CK_RING_INSTANCE(logevts_ring) *)(llog_init(cos_spd_id(), cli_addr)))) BUG();
	}

	if (unlikely(ring_is_full())) {
		llog_process(cos_spd_id());
	}

	monevt_conf(&monevt, param);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevts_ring, cli_ring, &monevt)));
	printc("evt enqueue (spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id());
	return;
}

// Context switch monitoring
static inline int 
csring_is_full()
{
	int capacity, size;

	assert(cs_ring);
	capacity = CK_RING_CAPACITY(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	size = CK_RING_SIZE(logcs_ring, (CK_RING_INSTANCE(logcs_ring) *)((void *)cs_ring));
	
	if (capacity == size + 1) {
		printc("cs ring (spd %ld) is full\n", cos_spd_id());
		printc("cap %d size %d\n", capacity, size);
		return 1;
	}

	return 0;
}

static inline void 
moncs_conf(struct cs_info *moncs)
{
	unsigned long long ts;

	moncs->curr_thd = cos_get_thd_id();
	rdtscll(ts);
	moncs->time_stamp = ts;

	return;
}

static int first  = 0;

/* only called by scheduler  */
static inline void 
moncs_enqueue(unsigned short int thd_id)
{
	struct cs_info moncs;

	// 7 is timer thread, 5 is initial booter thread
	/* printc("curr %d next %d\n", cos_get_thd_id(), thd_id); */

	/* if (cos_get_thd_id() != 7 && cos_get_thd_id() != 5) { */

	if (cos_get_thd_id() == 7) return; // not tracking timer interrupt for now
	if (cos_get_thd_id() == 5 && first == 0) { // not tracking init thread before any other threads created
		first = 1;
		return;
	}

	assert(cos_spd_id() == SCHED_SPD);

	// this will execute only one time 
	if (unlikely(!cs_ring)) {
		printc("cs enqueue (spd %ld, thd %d) ", cos_spd_id(), cos_get_thd_id());
		cos_sched_lock_release();
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		cos_sched_lock_take();
		assert(cli_addr);
		if (!(cs_ring = (CK_RING_INSTANCE(logcs_ring) *)(llog_cs_init(cos_spd_id(), cli_addr)))) BUG();
	}

	if (unlikely(csring_is_full())) {
		llog_process(cos_spd_id());
	}
	
	moncs_conf(&moncs);
	moncs.next_thd = thd_id;
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logcs_ring, cs_ring, &moncs)));
		
	return;
}

#endif /* LOGGER_H */
