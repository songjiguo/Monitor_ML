#ifndef   	SCHED_H
#define   	SCHED_H

#include <res_spec.h>

// added to facilitate to wake up all threads calling sched_block from
// the same spd
int sched_wakeup_spd_all(spdid_t spdid);

int sched_wakeup_helper(spdid_t spdid, unsigned short int thd_id); /* fault tolerance used only */
int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
int sched_block_helper(spdid_t spdid, unsigned short int dependency_thd);
int sched_block(spdid_t spdid, unsigned short int dependency_thd);

void sched_timeout(spdid_t spdid, unsigned long amnt);
int sched_timeout_thd(spdid_t spdid);
unsigned int sched_tick_freq(void);
unsigned long sched_cyc_per_tick(void);
unsigned long sched_timestamp(void);
unsigned long sched_timer_stopclock(void);
int sched_priority(unsigned short int tid);

int sched_create_thd(spdid_t spdid, u32_t sched_param_0, u32_t sched_param_1, unsigned int desired_thd);
/* Should only be called by the booter/loader */
int sched_create_thread_default(spdid_t spdid, u32_t sched_param_0, u32_t sched_param_1, unsigned int desired_thd);

/* This function is deprecated...use sched_create_thd instead. */
int sched_create_thread(spdid_t spdid, struct cos_array *data);
/* where is this used? */
int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs);

int sched_create_net_brand(spdid_t spdid, unsigned short int port);
int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid);

int sched_component_take(spdid_t spdid);
int sched_component_release(spdid_t spdid);

int sched_cs_status(spdid_t spdid); // Jiguo: check if the critical section is being hold

int sched_create_thd_again(unsigned int tid);

void sched_active_uc(int thd_id);


// Jiguo  -------------->
// Jiguo: add ck_ring support here to track context switch
#include <ck_ring_cos.h>
vaddr_t sched_csring_setup(vaddr_t par1, int par2);

// context tracking
struct cs_info {
	int curr_thd;
	int next_thd;
	unsigned long long time_stamp;
};

vaddr_t sched_ring;
int logsync_process;
int logmon_spdid;

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __CSRING_DEFINED
#define __CSRING_DEFINED
CK_RING(cs_info, logcs_ring);
CK_RING_INSTANCE(logcs_ring) *cs_ring;
#endif

static inline void moncs_conf(struct cs_info *moncs)
{
	unsigned long long ts;
	moncs->curr_thd = cos_get_thd_id();
	rdtscll(ts);
	moncs->time_stamp = ts;
	return;
}

static inline void moncs_enqueue(unsigned short int thd_id)
{
	struct cs_info moncs;
	
	moncs_conf(&moncs);
	moncs.next_thd = thd_id;
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logcs_ring, cs_ring, &moncs)));
	return;
}

// Jiguo  <--------------

#endif 	    /* !SCHED_H */
