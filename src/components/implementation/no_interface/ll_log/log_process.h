#ifndef   	LOG_PROCESS_H
#define   	LOG_PROCESS_H

/* Data structure for tracking information in log_manager */

#include <ll_log.h>
#include <heap.h>

/* ring buffer for event flow (one RB per component) */
struct logmon_info {
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
	
	struct evt_entry first_entry;
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

	unsigned long long last_spd_exec[MAX_NUM_SPDS];
	unsigned long long tot_spd_exec[MAX_NUM_SPDS]; // total exec in spd

	unsigned long long tot_exec;
	unsigned long long last_ts;
	unsigned long long last_exec;

	unsigned int tot_inv[MAX_NUM_SPDS];


	unsigned long long upto_wcet[MAX_NUM_SPDS];  // wcet below a spd
	unsigned long long tot_upto_wcet[MAX_NUM_SPDS];
	unsigned long long wcet[MAX_NUM_SPDS];        // wcet among all invocations in a spd
	unsigned long long tot_wcet[MAX_NUM_SPDS];   // all wcet below a spd
	unsigned long long single_wcet[MAX_NUM_SPDS];   // wcet in one invocation in a spd

	int curr_pos;
	struct curr_spd_info curr_spd_info;  // current spd info
};


struct logmon_info logmon_info[MAX_NUM_SPDS];
struct thd_trace thd_trace[MAX_NUM_THREADS];

static void 
init_thread_trace()
{
	int i, j;
	memset(thd_trace, 0, sizeof(struct thd_trace) * MAX_NUM_THREADS);
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		thd_trace[i].thd_id	 = i;
		thd_trace[i].block_dep	 = 0;
		thd_trace[i].pi_duration = 0;

		thd_trace[i].period	    = 0;
		thd_trace[i].execution	    = 0;

		thd_trace[i].release	    = 0;
		thd_trace[i].runtime	    = 0;
		thd_trace[i].completion	    = 0;
		thd_trace[i].wait_for_block = 0;

		for (j = 0; j < MAX_NUM_SPDS; j++) {
			thd_trace[i].last_spd_exec[j]     = 0;
			thd_trace[i].tot_spd_exec[j]  = 0;
			thd_trace[i].tot_inv[j]	      = 0;
			thd_trace[i].tot_wcet[j]      = 0;
			thd_trace[i].tot_upto_wcet[j] = 0;
			thd_trace[i].upto_wcet[j]     = 0;
			thd_trace[i].single_wcet[j]     = 0;
			thd_trace[i].wcet[j]	      = 0;
		}
		thd_trace[i].last_exec = 0;
		thd_trace[i].tot_exec = 0;
		thd_trace[i].last_ts = 0;

		thd_trace[i].curr_spd_info.spdid    = 0;
		thd_trace[i].curr_spd_info.from_spd = 0;
	}
	return;
}

static void 
check_timing(struct evt_entry *entry)
{
	struct thd_trace *ttc, *ttn;
	assert(entry);

	ttc = &thd_trace[entry->from_thd];
	assert(ttc);
	
	switch (entry->evt_type) {
	case EVT_CINV:
	case EVT_SINV:
	case EVT_SRET:
	case EVT_CRET:
		assert(entry->to_thd == entry->from_thd);
		ttc->tot_exec += entry->time_stamp - ttc->last_ts;
		ttc->last_ts = entry->time_stamp;
		break;
	case EVT_CS:
                // lock in sched, mm and booter?
		/* assert(entry->from_spd == SCHED_SPD); */  
		assert(entry->to_thd != entry->from_thd);
		ttc->tot_exec += entry->time_stamp - ttc->last_ts;

		ttn = &thd_trace[entry->to_thd];
		assert(ttn);	
		ttn->last_ts = entry->time_stamp;
		break;
	case EVT_TINT:
	case EVT_NINT:
		assert(entry->to_thd == entry->from_thd);
		ttc = &thd_trace[entry->to_thd];
		assert(ttc);
		ttc->last_ts = entry->time_stamp;
		break;
	default:
		printc("event type error\n");
		assert(0);
		break;
	}	

	printc("thd %d total exec %llu \n", ttc->thd_id, ttc->tot_exec);
	
	return;
}

/* /\* heap functions for next earliest event *\/ */
/* static int evt_cmp(void *a, void *b) */
/* { */
/* 	struct evt_entry *e1 = a, *e2 = b; */
/* 	return e1->time_stamp >= e2->time_stamp; */
/* } */

/* static void evt_update(void *a, int pos) */
/* { */
/* 	((struct evt_entry *)a)->index = pos; */
/* } */


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


#endif
