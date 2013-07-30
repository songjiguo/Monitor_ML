/*
    header file for only log_mgr
*/

// MOVE these to implementation directory FIXME:
/* ring buffer for event flow (per spd) */
struct logmon_info {
	spdid_t spdid;
	vaddr_t mon_ring;
	vaddr_t cli_ring;
	
	struct event_info *last_stop;  // this indicates where it stopped in the last time window
};

/* ring buffer for context switch (only one) */
struct logmon_cs {
	vaddr_t mon_csring;
	vaddr_t sched_csring;
};

/*
  Track related
*/
// FIXME: might not need save all information in log_process
/* per-thread event flow, updated within the log_monitor  */
struct thd_trace {
	int thd_id;
	unsigned long long exec[MAX_NUM_SPDS]; // not moving avg FIXME:
	unsigned long long tot_spd_exec[MAX_NUM_SPDS]; // total exec in spd
	unsigned long long wcet[MAX_NUM_SPDS];
	unsigned long long tot_exec;
	unsigned long long tot_wcet;   // FIXME: should be the total wcet of thread w/ invk number
	int pair; // test only (can be used to indicate if wait for the next pair, e.g, call-return)
	/* struct event_info *entry; */
	void *trace_head;
};



/* /\* per-spd event flow, updated within the log_monitor  *\/ */
/* struct spd_trace { */
/* 	spdid_t spd_id; */
/* 	struct event_info *entry; */
/* }; */


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
