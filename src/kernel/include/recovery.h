/***************************************************************
  Jiguo: This is the header file to help the kernel keep clean 
         for recovery related changes
         depends on gcc will optimize away for i = 0 case
 ***************************************************************/

#ifndef RECOVERY_H
#define RECOVERY_H

#include "spd.h"
#include "thread.h"

/*---------Threads that created by scheduler--------*/
static struct thread*
sched_thread_lookup(struct spd *spd, int thd_id)
{
	struct thread *thd, *n_thd;

	if (!(thd = spd->scheduler_all_threads)) return NULL;

	for (thd = thd->sched_prev; thd->thread_id != 0;) {
		n_thd = thd->sched_prev;
		/* printk("<<< thd %d  and look up id %d>>>\n", thd->thread_id, thd_id); */
		if (thd->thread_id == thd_id) {
			/* printk("found thd %d\n", thd->thread_id); */
			
			/* optimization for look up desired thread id */
			/* /\* "remove" the found thd from original location *\/ */
			thd->sched_next->sched_prev = thd->sched_prev;
			thd->sched_prev->sched_next = thd->sched_next;
			/* /\* put the found thd to the front of list *\/ */
			thd->sched_next = spd->scheduler_all_threads->sched_next;
			thd->sched_prev = spd->scheduler_all_threads;
			spd->scheduler_all_threads->sched_next = thd;
			thd->sched_next->sched_prev = thd;

			return thd;
		}
		thd = n_thd;
	}
	/* printk("Can not find an existing thread id %d!!\n", thd_id); */
	return NULL;
}

#if RECOVERY_ENABLE == 1
//*************************************************
           /* enable recovery */
//*************************************************

/*---------Fault Notification Operations--------*/
static inline int
init_spd_fault_cnt(struct spd *spd)
{
	spd->fault.cnt = 0;
	return 0;
}

static inline int
init_cap_fault_cnt(struct invocation_cap *cap)
{
	cap->fault.cnt = 0;
	return 0;
}

static inline int
init_invframe_fault_cnt(struct thd_invocation_frame *inv_frame)
{
	inv_frame->fault.cnt = 0;
	return 0;
}

static inline int
inv_frame_fault_cnt_update(struct thread *thd, struct spd *spd)
{
	struct thd_invocation_frame *inv_frame;
	inv_frame = thd_invstk_top(thd);
	inv_frame->fault.cnt = spd->fault.cnt;
	return 0;
}

static inline int
ipc_fault_detect(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	if (cap_entry->fault.cnt != dest_spd->fault.cnt) return 1;
	else return 0;
}

static inline int
pop_fault_detect(struct thd_invocation_frame *curr_frame)
{
	if (curr_frame->fault.cnt != curr_frame->spd->fault.cnt) return 1;
	else return 0;
}

static inline int
switch_thd_fault_detect(struct thread *next)
{
	struct spd *n;
	struct thd_invocation_frame *tif;

	tif  = thd_invstk_top(next);
	n    = tif->spd;
	
	/*  * Has the component we're returning to faulted since we were */
	/*  * last executing in it?  If so, return to the calling */
	/*  * component at eip-8 (at the fault handler). */

	if (unlikely(tif->fault.cnt != n->fault.cnt)) {
		printk("Fault is detected when context switch\n");
		next->flags  |= THD_STATE_PREEMPTED;
		next->regs.ip = tif->ip-8;
		next->regs.sp = tif->sp;
		tif = thd_invocation_pop(next);
		if (unlikely(!tif)) {
			next->regs.ip = 0;
			next->regs.sp = 0;
		}
	}
	return 0;
}

static inline int
interrupt_fault_update(struct thread *curr, struct spd *curr_spd)
{
	/* update when wait for an interrupt, this might be from a home spd */
	struct thd_invocation_frame *curr_frame;
	curr_frame = thd_invstk_top(curr);
	curr_frame->fault.cnt = curr_spd->fault.cnt;
	
	return 0;
}

static inline int
interrupt_fault_detect(struct thread *next)
{
	return 0;
}

extern struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];

/* cos_syscall_fault_cntl(int spdid, int option, spdid_t d_spdid, unsigned int cap_no) */

static inline unsigned long
fault_cnt_syscall_helper(int spdid, int option, spdid_t d_spdid, unsigned int cap_no)
{
	unsigned long ret = 0;
	struct spd *d_spd, *this_spd, *dest_spd;
	struct invocation_cap *cap_entry;
	unsigned int cap_no_origin;
	
	this_spd = spd_get_by_index(spdid);
	d_spd      = spd_get_by_index(d_spdid);

	if (!this_spd || !d_spd) {
		printk("cos: invalid fault cnt  call for spd %d or spd %d\n",
		       spdid, d_spdid);
		return -1;
	}

	cap_no >>= 20;
	/* printk("cos: cap_no is %d base is %d\n", cap_no, d_spd->cap_base); */

	if (unlikely(cap_no >= MAX_STATIC_CAP)) {
		printk("cos: capability %d greater than max\n",
		       cap_no);
		return -1;
	}

	cap_entry = &invocation_capabilities[cap_no];

	if (unlikely(!cap_entry->owner)) {
		printk("cos: No owner for cap %d.\n", cap_no);
		return -1;
	}

	cap_no_origin = cap_no;
	switch(option) {
	case COS_SPD_FAULT_TRIGGER:
		d_spd->fault.cnt++;
		printk("cos: SPD %d Fault.Cnt is incremented by 1\n", spd_get_index(d_spd));
		break;
	case COS_CAP_FAULT_UPDATE: /* does not need destination spd, got by cap_no on interface */
		assert(cap_entry->owner == d_spd);
		dest_spd = cap_entry->destination;
		cap_entry->fault.cnt = dest_spd->fault.cnt;
		/* printk("cap_entry->fault.cnt %d\n",cap_entry->fault.cnt); */
		/* assume capabilities are continuse in invocation_capabilities[] */
		/* + direction */
		while(1) {
			cap_no_origin++;
			cap_entry = &invocation_capabilities[cap_no_origin];
			if (cap_entry->destination != dest_spd) break;
			cap_entry->fault.cnt = dest_spd->fault.cnt;
			/* printk("ker set cnt+(cap_no_origin %d): owner %d dest is %d\n", cap_no_origin, */
			/*        spd_get_index(cap_entry->owner),spd_get_index(cap_entry->destination)); */
			/* printk("cap_entry->fault.cnt %d\n",cap_entry->fault.cnt); */
		}
		cap_no_origin = cap_no;
		/* - direction */
		while(1) {
			cap_no_origin--;
			cap_entry = &invocation_capabilities[cap_no_origin];
			if (cap_entry->destination != dest_spd) break;
			cap_entry->fault.cnt = dest_spd->fault.cnt;			
		}

		printk("cos: CAP (owner %d dest %d) Fault is updated\n", spd_get_index(d_spd), spd_get_index(dest_spd));
		break;
	default:
		ret = -1;
		break;
	}
	
	return ret;
}


static inline int
cap_fault_cnt_update(struct invocation_cap *c, struct spd* dspd)
{
	c->fault.cnt   = dspd->fault.cnt;
	return 0;
}

#else
//*************************************************
           /* disable recovery */
//*************************************************
static inline int
init_spd_fault_cnt(struct spd *spd)
{
	return 0;
}

static inline int
init_cap_fault_cnt(struct invocation_cap *cap)
{
	return 0;
}

static inline int
init_invframe_fault_cnt(struct thd_invocation_frame *inv_frame)
{
	return 0;
}

static inline int
inv_frame_fault_cnt_update(struct thread *thd, struct spd *spd)
{
	return 0;
}

static inline int
ipc_fault_detect(struct invocation_cap *cap_entry, struct spd *dest_spd)
{
	return 0;
}

static inline int
pop_fault_detect(struct thd_invocation_frame *curr_frame)
{
	return 0;
}

static inline int
switch_thd_fault_detect(struct thread *next)
{
	return 0;
}

static inline int
interrupt_fault_update(struct thread *curr, struct spd *curr_spd)
{
	return 0;
}

static inline int
interrupt_fault_detect(struct thread *next)
{
	return 0;
}

static inline unsigned long
fault_cnt_syscall_helper(int spdid, int option, spdid_t d_spdid, unsigned int cap_no)
{
	return 0;
}

static inline int
cap_fault_cnt_update(struct invocation_cap *c, struct spd* dspd)
{
	return 0;
}

#endif

#endif  //RECOVERY_H
