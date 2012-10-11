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

int fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	unsigned long r_ip; 	/* the ip to return to */
	int tid = cos_get_thd_id();

	first++;
	if(first == 5) {
		printc("has failed %d times\n",first);
		assert(0);
	}


	printc("\nPGFAULT: thd %d faults in spd %d @ %p\n",
	       tid, spdid, fault_addr);
         /*
	 * Look at the booter: when recover is happening, the sstub is
	 * set to 0x1, thus we should just wait till recovery is done.
	 */

	/* this needs to be first synchronize the fault_cnt on
	 * frame. Otherwise the following remove frame will confuse
	 * the frame fault_cnt */

	/* remove from the invocation stack the faulting component! */
	assert(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0));
	/* Manipulate the return address of the component that called
	 * the faulting component... */
	assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
	/* ...and set it to its value -8, which is the fault handler
	 * of the stub. */
	assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));
	assert(!cos_fault_cntl(COS_SPD_FAULT_TRIGGER, spdid, 0)); /* increase the spd.fault_cnt in kernel */

	if ((int)ip == 1) failure_notif_wait(cos_spd_id(), spdid);
	else         failure_notif_fail(cos_spd_id(), spdid);

	return 0;
}


static  int test = 0;
int fault_flt_notif_handler(spdid_t spdid, void *fault_addr, int type, void *ip)
{
	unsigned long long start, end;
	/* unsigned long r_ip; */
	int tid = cos_get_thd_id();
	printc("<< thd %d in Fault FLT Notif >> \n", cos_get_thd_id());
	printc("....kevin.....andy.....\n");

	unsigned long r_ip; 	/* the ip to return to */
	if(!cos_thd_cntl(COS_THD_INV_FRAME_REM, tid, 1, 0)) {
		assert(r_ip = cos_thd_cntl(COS_THD_INVFRM_IP, tid, 1, 0));
		assert(!cos_thd_cntl(COS_THD_INVFRM_SET_IP, tid, 1, r_ip-8));
		printc("return 0\n");
		return 0;
	}

	assert(0);
}
