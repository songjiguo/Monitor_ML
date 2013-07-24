/*
   log monitor header file for latent fault (track all events)
*/

#ifndef LOGMON_H
#define LOGMON_H

#include <cos_component.h>

vaddr_t lm_init(spdid_t spdid);
int lm_process(spdid_t spdid);
void lm_sync_process();
unsigned int lm_get_sync_period();

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

#define INV_LOOP_1 1
#define INV_LOOP_2 2
#define INV_LOOP_START 3
#define INV_LOOP_END 4

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
	int pair; // test only (can be used to indicate if wait for the next pair, e.g, call-return)
	/* struct event_info *entry; */
	void *trace_head;
};

/* /\* per-spd event flow, updated within the log_monitor  *\/ */
/* struct spd_trace { */
/* 	spdid_t spd_id; */
/* 	struct event_info *entry; */
/* }; */

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
