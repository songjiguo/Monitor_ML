/*
   Jiguo 2014 -- The header file for the log monitor latent fault
*/

#ifndef LOGGER_H
#define LOGGER_H

extern vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
extern void *valloc_alloc(spdid_t spdid, spdid_t dest, unsigned long npages);

extern int monitor_upcall(spdid_t spdid);

#include <cos_list.h>
#include <ck_ring_cos.h>

#include <cos_types.h>
PERCPU_EXTERN(cos_sched_notifications);

#define MEAS_WITH_LOG
//#define MEAS_WITHOUT_LOG

// hard code interface function number
enum{
	FN_SCHED_BLOCK = 1,
	FN_SCHED_WAKEUP
};

// event type
enum{
	EVT_CINV = 1,
	EVT_SINV,
	EVT_SRET,
	EVT_CRET,
	EVT_CS,
	EVT_TINT,   // timer interrupt
	EVT_NINT    // network interrupt
};

/* event entry in the ring buffer (per event) */
struct evt_entry {
	int from_thd, to_thd;
	unsigned long from_spd, to_spd; // can be cap_no
	unsigned long long time_stamp;
	int evt_type;

	int func_num;   // hard code which function call FIXME:naming space
	int para;   // record passed parameter (e.g. dep_thd)

	int index;  // used for heap
};

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __RING_DEFINED
#define __RING_DEFINED
CK_RING(evt_entry, logevt_ring);
CK_RING_INSTANCE(logevt_ring) *evt_ring;
#endif

/* static inline int cos_log_lock_take(void) */
/* { */
/* 	union cos_synchronization_atom *l = &PERCPU_GET(cos_sched_notifications)->cos_locks; */
/* 	u16_t curr_thd = cos_get_thd_id(), owner; */
	
/* 	/\* No recursively taking the lock *\/ */
/* 	if(l->c.owner_thd == curr_thd) return 0; */
/* 	do { */
/* 		union cos_synchronization_atom p, n; /\* previous and new *\/ */

/* 		do { */
/* 			p.v            = l->v; */
/* 			owner          = p.c.owner_thd; */
/* 			n.c.queued_thd = 0; /\* will be set in the kernel... *\/ */
/* 			if (unlikely(owner)) n.c.owner_thd = owner; */
/* 			else                 n.c.owner_thd = curr_thd; */
/* 		} while (unlikely(!cos_cas((unsigned long *)&l->v, p.v, n.v))); */

/* 		if (unlikely(owner)) { */
/* 			/\* If another thread holds the lock, notify */
/* 			 * kernel to switch *\/ */
/* /\* #ifdef LOG_MONITOR *\/ */
/* /\* 			evt_enqueue(owner, cos_spd_id(), 0, 0, 1); *\/ */
/* /\* #endif *\/ */

/* 			if (cos___switch_thread(owner, COS_SCHED_SYNC_BLOCK) == -1) return -1; */
/* 		} */
/* 		/\* If we are now the owner, we're done.  If not, try */
/* 		 * to take the lock again! *\/ */
/* 	} while (unlikely(owner)); */

/* 	return 0; */
/* } */

/* static inline int cos_log_lock_release(void) */
/* { */
/* 	union cos_synchronization_atom *l = &PERCPU_GET(cos_sched_notifications)->cos_locks; */
/* 	union cos_synchronization_atom p; */
/* 	u16_t queued_thd; */
	
/* 	assert(l->c.owner_thd == cos_get_thd_id()); */
/* 	do { */
/* 		p.v           = l->v; */
/* 		queued_thd    = p.c.queued_thd; */
/* 	} while (unlikely(!cos_cas((unsigned long *)&l->v, p.v, 0))); */
/* 	/\* If a thread is contending the lock. *\/ */
/* 	if (queued_thd) { */
/* /\* #ifdef LOG_MONITOR *\/ */
/* /\* 		evt_enqueue(queued_thd, cos_spd_id(), 0, 0, 1); *\/ */
/* /\* #endif *\/ */
/* 		return cos___switch_thread(queued_thd, COS_SCHED_SYNC_UNBLOCK); */
/* 	} */
	
/* 	return 0; */
/* } */

// print info for an event.
static void 
print_evt_info(struct evt_entry *entry)
{
	assert(entry);

	int dest;
	if (entry->evt_type == EVT_CINV && entry->to_spd > MAX_NUM_SPDS) {
		/* printc("par2 entry->to_spd %lu\n", entry->to_spd); */
		dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, entry->to_spd);
		if (dest <= 0) assert(0);
		entry->to_spd = dest;
	}
	else if (entry->evt_type == EVT_CRET && entry->from_spd > MAX_NUM_SPDS) {
		/* printc("par2 entry->from_spd %lu\n", entry->from_spd); */
		dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, 0, 0, entry->from_spd);
		if (dest <= 0) assert(0);
		entry->from_spd = dest;
	}

	printc("thd (%d --> ", entry->from_thd);
	printc("%d) ", entry->to_thd);

	printc("spd (%lu --> ", entry->from_spd);
	printc("%lu) ", entry->to_spd);

	printc("func_num %d ", entry->func_num);
	printc("para %d ", entry->para);

	printc("type %d ", entry->evt_type);

	printc("time_stamp %llu\n", entry->time_stamp);
	
	return;
}

// Events monitoring
static inline int 
evt_ring_is_full()
{
	int capacity, size;

	assert(evt_ring);
	capacity = CK_RING_CAPACITY(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
	size = CK_RING_SIZE(logevt_ring, (CK_RING_INSTANCE(logevt_ring) *)((void *)evt_ring));
	if (capacity == size + 1) return 1;
	else return 0;
}

// 0 for CINV, 1 for SINV, 2 for SRET, 3 for CRET
// 4 for CS, 5 for timer INT and 6 for network INT
static inline void 
evt_conf(struct evt_entry *evt, int par1, unsigned long par2, int par3, int par4, int type)
{
	unsigned long long ts;

	rdtscll(ts);
	evt->time_stamp = ts;
	evt->from_thd = cos_get_thd_id();
	evt->to_thd   = par1;

	if (type == EVT_CINV || type == EVT_SRET) {
		evt->from_spd = cos_spd_id();
		evt->to_spd   = par2;
	} else if (type == EVT_SINV || type == EVT_CRET) {
		evt->from_spd = par2;
		evt->to_spd   = cos_spd_id();
	} else if (type == EVT_TINT || type == EVT_NINT || type == EVT_CS) {
		evt->from_spd = evt->to_spd = cos_spd_id();
	} else assert(0);

	evt->func_num = par3;
	evt->para     = par4;

	evt->evt_type = type;

	evt->index = 0;  //used for heap only
	return;
}

/*
  This function is responsible for tracking the event occurrence,
  including CINV, CRET, SINV, SRET, CS, INT. We need track the type of
  event properly. Here are parameters for different events:

  par1 -- thread id
          (CINV, CRET, SINV and SRET: this is the current thread)
          (CS: this is the next thread that is about to be dispatched)
          (INT: this is the interrupt thread)
  par2 -- component id 
          (this is to_spd (for CINV and SRET))
          (this is from_spd (for SINV and CRET))
	  (INT : this is net_if)
	  (CS : this is scheduler)	  
  par3 -- function number 
          (0 for none, 1 for sched_block, 2 for periodic_wait..... on server side). 
	  This probably needs be hard coded now due to the lack of name space.
	  See top section in ll_log.c
  par4 -- extra parameter
          e.g. dep_thd in sched_block, for PI detection. This can be other parameter as well
  evt_type -- the type of event can affect how to check it
          (1 for CINV, 2 for SINV, 3 for SRET, 4 for CRET)
	  (5 for CS, 6 for Timer INT and 7 for Network INT)
	  (contention can also be in sched/mm and switch occurs??)
*/

static inline void 
evt_enqueue(int par1, unsigned long par2, int par3, int par4, int evt_type)
{
	struct evt_entry evt;

	if (unlikely(!evt_ring)) {
		printc("evt enqueue (spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id());
		/* a page now */
		vaddr_t cli_addr = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cli_addr);
		if (!(evt_ring = (CK_RING_INSTANCE(logevt_ring) *)(llog_init(cos_spd_id(), cli_addr)))) BUG();
	}

	if (unlikely(evt_ring_is_full())) {
		printc("evt_ring is full(spd %ld, thd %d) \n", cos_spd_id(), cos_get_thd_id());
		/* llog_process(cos_spd_id()); */
		monitor_upcall(cos_spd_id());
	}

	
	/* Two problems to be concerned: 

	 * 1) Race condition. Multiple producers are trying to record
	 * their events into a single consumer buffer. A lock is used
	 * around ck_ring_enqueue to solve this. Should this lock be
	 * the lock used in that component?  For example, lock/booter
	 * spd uses sched_component_take/release, sched/mm uses
	 * cos_sched_lock_take/release. Other component uses
	 * lock_take. Critical section is short and need avoid
	 * recursive locking.

	 * 2) Record consistency. For example, a thread can be
	 * preempted right after evt_conf and before writes into the
	 * shared RB. When the event is actually written, its time
	 * stamp could be wrong. TODO:add a flag before and after and
	 * check the log and record twice.
	 * 
	 * 3) When the log thread is activated due to the full buffer,
	 * should not log any more since it is full at that point
	 */

	evt_conf(&evt, par1, par2, par3, par4, evt_type);
	/* cos_log_lock_take(); */
	print_evt_info(&evt);
        while (unlikely(!CK_RING_ENQUEUE_SPSC(logevt_ring, evt_ring, &evt)));
	/* cos_log_lock_release(); */

	return;

}


#endif /* LOGGER_H */
