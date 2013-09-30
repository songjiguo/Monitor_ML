#ifndef LOG_INT_H
#define LOG_INT_H

#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
vaddr_t llog_cs_init(spdid_t spdid, vaddr_t addr);
int llog_process(spdid_t spdid);
unsigned int llog_get_syncp(spdid_t spdid);

/* ring buffer for event flow (per spd) */
struct logmon_info {
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
	
	struct event_info first_entry;  // this indicates (where it stopped in the last time window) current head
};

/* ring buffer for context switch (only one) */
struct logmon_cs {
	vaddr_t mon_csring;
	vaddr_t sched_csring;
};


/* the timing about a thread enters and leaves a spd */
struct curr_spd_info {
	int spdid;
	int from_spd;
};

// FIXME: might not need save all information in log_process
/* per-thread event flow, updated within the log_monitor  */
struct thd_trace {
	int thd_id;
	int prio;  // hard code priority for PI testing
	
	int block_dep;
	unsigned long long pi_duration;  // how long this thread has been in PI?

	// parameters for periodic tasks
	unsigned long long period;
	unsigned long long execution;

	unsigned long long release;
	unsigned long long runtime;
	unsigned long long completion;
	int wait_for_block;

	unsigned long long alpha_exec[MAX_NUM_SPDS];  // initial entry in a spd
	unsigned long long avg_exec[MAX_NUM_SPDS]; // not moving avg FIXME:
	unsigned long long last_exec[MAX_NUM_SPDS];  // tmp use

	unsigned long long tot_spd_exec[MAX_NUM_SPDS]; // total exec in spd
	unsigned int tot_inv[MAX_NUM_SPDS];

	unsigned long long upto_wcet[MAX_NUM_SPDS];  // wcet below a spd
	unsigned long long tot_upto_wcet[MAX_NUM_SPDS];
	unsigned long long wcet[MAX_NUM_SPDS];        // wcet among all invocations in a spd
	unsigned long long tot_wcet[MAX_NUM_SPDS];   // all wcet below a spd
	unsigned long long this_wcet[MAX_NUM_SPDS];   // wcet in one invocation in a spd

	unsigned long long tot_exec;

	int curr_pos;
	struct curr_spd_info curr_spd_info;  // current spd info
};

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

#endif /* LOG_INT_H */
