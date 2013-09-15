/* All report and debug functions for log manager here */

#ifndef   	LOG_REPORT_H
#define   	LOG_REPORT_H

#include <ll_log.h>

extern struct logmon_info logmon_info[MAX_NUM_SPDS];
extern struct logmon_cs lmcs;
extern struct thd_trace thd_trace[MAX_NUM_THREADS];

// print info for an event
static void 
print_evtinfo(struct event_info *entry)
{
	assert(entry);

	printc("thd id %d ", entry->thd_id);
	printc("from spd %d ", entry->from_spd);

	int dest = entry->dest_info;
	if (dest > MAX_NUM_SPDS) {
		if ((dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, dest)) <= 0) assert(0);
	}
	printc("dest spd %d ", dest);

	printc("func num %d ", entry->func_num);
	printc("dep thd %d ", entry->dep_thd);

	// FIXME: to_spd info can be pushed into stack an pop when return
	/* printc("next spd %d\n", entry->dest_info); */

	printc("time_stamp %llu\n", entry->time_stamp);
	
	return;
}

// print context switch info
static void 
print_csinfo(struct cs_info *csentry)
{
	assert(csentry);
	printc("curr thd id %d\n", csentry->curr_thd);
	printc("next thd id %d\n", csentry->next_thd);
	printc("time_stamp %llu\n", csentry->time_stamp);
	
	return;
}

static void
print_first_entry()
{
	int i;
        struct logmon_info *spdmon;
	CK_RING_INSTANCE(logevts_ring) *evtring;
	
	printc("\n\n all last stops\n");

	for (i = 1; i < MAX_NUM_SPDS; i++) {
		spdmon = &logmon_info[i];
		evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring;
		if (!evtring) continue;
		if (&spdmon->first_entry) {
			/* printc("last stop (spd %d) thd %d\n", i, spdmon->first_entry->thd_id); */
			print_evtinfo(&spdmon->first_entry);
		}
	}
	printc("END>>>\n");
	return;
}

/* // print all events within a spd */
/* static void */
/* print_spd_history(int spdid) */
/* { */
/* 	assert(spdid); */
/*         struct logmon_info *spdmon; */
/* 	CK_RING_INSTANCE(logevts_ring) *evtring; */

/* 	spdmon = &logmon_info[i]; */
/* 	assert(spdmon); */
	
/* 	evtring = (CK_RING_INSTANCE(logevts_ring) *)spdmon->mon_ring; */
/* 	if (!evtring) return; */

/* 	print_evtinfo(&spdmon->first_entry); */

/* 	return; */
/* } */

static void 
print_csrb(CK_RING_INSTANCE(logcs_ring) *csring)
{
	struct cs_info cs_entry;
	assert(csring);

	memset(&cs_entry, 0, sizeof(struct cs_info));
	while(CK_RING_DEQUEUE_SPSC(logcs_ring, csring, &cs_entry) == 1) {
		print_csinfo(&cs_entry);
	}
	
	return;
}

static void
print_evtrb(CK_RING_INSTANCE(logevts_ring) *evtring)
{
	struct event_info evt_entry;
	assert(evtring);

	memset(&evt_entry, 0, sizeof(struct event_info));
	while(CK_RING_DEQUEUE_SPSC(logevts_ring, evtring, &evt_entry) == 1) {
		print_evtinfo(&evt_entry);
	}
	
	return;
}

// total wcet time of a thread to the point when the check is being done
static unsigned long long 
report_tot_wcet(int thd_id)
{
	int i;
	unsigned long long tot_wcet = 0;
	unsigned int tot_invs = 0;
	struct thd_trace *evt_list;
	assert(thd_id);

	evt_list = &thd_trace[thd_id];

	int num_spd = 0;
	for (i = 1; i< MAX_NUM_SPDS; i++) {
		if (!evt_list->wcet[i]) continue;
		num_spd++;
		/* printc("spd %d wcet %llu invs %d\n", i, evt_list->wcet[i], evt_list->tot_inv[i]); */
		tot_wcet += evt_list->wcet[i];
		tot_invs += evt_list->tot_inv[i];
	}

	if (!num_spd) return 0;
	int invs = tot_invs/num_spd;
	/* printc("thd %d total wcet time : %llu (total invs %d, total spd %d)\n", thd_id, tot_wcet, tot_invs, num_spd); */
	assert(invs);
	/* evt_list->tot_wcet = tot_wcet; */
	return tot_wcet;
}

// avg wcet of a thread up to a spd
static unsigned long long 
report_avg_upto_wcet(int thd_id, int spdid)
{
	struct thd_trace *evt_list;
	unsigned long long avg_upto_wcet;

	assert(thd_id);
	assert(spdid >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_upto_wcet[spdid] || !evt_list->tot_inv[spdid]) return 0;
	printc("thd %d (total wcet %llu) up to spd %d (invocations %d) ", thd_id, evt_list->tot_upto_wcet[spdid], spdid, evt_list->tot_inv[spdid]);
	avg_upto_wcet = evt_list->tot_upto_wcet[spdid]/evt_list->tot_inv[spdid];
	printc("avg upto-wcet %llu\n\n", avg_upto_wcet);

	return avg_upto_wcet;
}

// avg wcet of a thread within a spd
static unsigned long long 
report_avg_in_wcet(int thd_id, int spdid)
{
	struct thd_trace *evt_list;
	unsigned long long avg_wcet;

	assert(thd_id);
	assert(spdid >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_wcet[spdid] || !evt_list->tot_inv[spdid]) return 0;
	printc("thd %d (total wcet %llu) in spd %d (invocations %d) ", thd_id, evt_list->tot_wcet[spdid], spdid, evt_list->tot_inv[spdid]);
	avg_wcet = evt_list->tot_wcet[spdid]/evt_list->tot_inv[spdid];
	printc("avg in-wcet %llu\n", avg_wcet);

	return avg_wcet;
}

// total exec time of a thread to the point when the check is being done within a spd
static unsigned long long 
report_tot_spd_exec(int thd_id, int spdid)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spdid >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_spd_exec[spdid] || !evt_list->wcet[spdid]) return 0;
	printc("thd %d (in spd %d) total exec --> %llu\n", thd_id, spdid, evt_list->tot_spd_exec[spdid]);
	return evt_list->tot_spd_exec[spdid];
}

// total exec time of a thread to the point when the check is being done
static unsigned long long 
report_tot_exec(int thd_id)
{
	struct thd_trace *evt_list;

	/* printc("total exec for thread %d\n", thd_id); */
	assert(thd_id);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->tot_exec) return 0;
	printc("thd %d overall exec --> %llu\n\n", thd_id, evt_list->tot_exec);
	return evt_list->tot_exec;
}

// wcet in a spd
static unsigned long long 
report_spd_wcet(int thd_id, int spd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spd_id >= 0);

	evt_list = &thd_trace[thd_id];
	if (!evt_list->wcet[spd_id]) return 0;
	printc("thd %d (in spd %d) wcet --> %llu\n", thd_id, spd_id, evt_list->wcet[spd_id]);
	return evt_list->wcet[spd_id];
}

// average exec time in a spd
static unsigned long long 
report_spd_exec(int thd_id, int spd_id)
{
	struct thd_trace *evt_list;

	assert(thd_id);
	assert(spd_id >= 0);

	evt_list = &thd_trace[thd_id];
	assert(evt_list);
	if (!evt_list->avg_exec[spd_id] || !evt_list->wcet[spd_id]) return 0;

	printc("thd %d (in spd %d) avg exec --> %llu\n", thd_id, spd_id, evt_list->avg_exec[spd_id]);

	return evt_list->avg_exec[spd_id];
}

static void 
report_thd_stack_list(int thdid)
{
	assert(thdid);
	int i, spd;

	struct thd_trace *thd_trace_list;
	thd_trace_list = &thd_trace[thdid];
	assert(thd_trace_list);

	spd = thd_trace_list->curr_spd_info.spdid;
	if (!spd) return;
	printc("thd %d (prio %d) : ", thdid, thd_trace_list->prio);
	printc("%d -> %d", thd_trace_list->curr_spd_info.from_spd, spd);
	printc("\n");

	return;
}


#endif
