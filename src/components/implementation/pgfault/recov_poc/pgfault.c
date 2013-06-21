/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <pgfault.h>
#include <sched.h>
#include <print.h>
#include <fault_regs.h>

#include <failure_notif.h>

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
struct cos_regs regs;

static int first = 0;

extern 

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	spdid_t home_spd = 0;
	int tid = cos_get_thd_id();

	first++;
	if(first == 10) {
		printc("plain pgfault has failed %d times\n",first);
		assert(0);
	}


	/* printc("PGFAULT: thd %d faults in spd %d @ %p\n", */
	/*        tid, spdid, fault_addr); */

         /*
	 * Look at the booter: when recover is happening, the sstub is
	 * set to 0x1, thus we should just wait till recovery is done.
	 */

	/* this needs to be first synchronize the fault_cnt on
	 * frame. Otherwise the following remove frame will confuse
	 * the frame fault_cnt */

	home_spd = cos_thd_cntl(COS_THD_HOME_SPD, cos_get_thd_id(), 0, 0);
	if (!home_spd) BUG();
	/* printc("home spd is %d (thd %d, failed spd %d)\n", home_spd, cos_get_thd_id(), spdid); */

	if (likely(home_spd != spdid)) {
		/* remove from the invocation stack the faulting component! */
		assert(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0));
		/* Manipulate the return address of the component that called
		 * the faulting component... */
		assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
		/* ...and set it to its value -8, which is the fault handler
		 * of the stub. */
		assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));
	}

	assert(!cos_fault_cntl(COS_SPD_FAULT_TRIGGER, spdid, 0)); /* increase the spd.fault_cnt in kernel */

	volatile unsigned long long start, end;
	
	if ((int)ip == 1) failure_notif_wait(cos_spd_id(), spdid);
	else {
		/* rdtscll(start); */
		
		failure_notif_fail(cos_spd_id(), spdid);

		/* rdtscll(end); */
		/* printc("COST(failture_notif): %llu\n", (end-start)); */
	}
        // jiguo: the faulting thread in its home spd can be destroyed */
	if (unlikely(home_spd == spdid)) cos_upcall_args(COS_UPCALL_BOOTSTRAP, spdid, 1);
	return 0;
}

static  int test = 0;
int fault_flt_notif_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long long start, end;
	int tid = cos_get_thd_id();
	/* printc("pgf notif parameters: spdid %d fault_addr %p flags %d ip %p\n", spdid, fault_addr, flags, ip); */
	/* printc("<< thd %d in Fault FLT Notif>> \n", cos_get_thd_id()); */

	unsigned long r_ip; 	/* the ip to return to */
	unsigned long r_sp; 	/* the sp to return to */
	if(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0)) {
		assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
		assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));
		return 0;
	}

	//Jiguo: upcall to the home spd and let it die there...
	spdid_t home_spd = 0;
	home_spd = cos_thd_cntl(COS_THD_HOME_SPD, cos_get_thd_id(), 0, 0);
	if (unlikely(home_spd == spdid)) cos_upcall_args(COS_UPCALL_BOOTSTRAP, spdid, 1);
	/* /\* use the trick that set ip sp to 0 *\/ */
	assert(0);
	return 0;
}
