#ifndef SWIFI_REGS_H
#define SWIFI_REGS_H

#include <cos_types.h>
#include <print.h>

struct cos_regs {
	spdid_t spdid;
	int tid;
	struct pt_regs regs;
};

/**********************/
/* Borrowed this from the Nooks */
/*
 * Pseudo-random number generator for randomizing the profiling clock,
 * and whatever else we might use it for.  The result is uniform on
 * [0, 2^31 - 1]. 
 */
unsigned long
random()
{
	unsigned long x, hi, lo, t;

	/*
	 * Compute x[n + 1] = (7^5 * x[n]) mod (2^31 - 1).
	 * From "Random number generators: good ones are hard to find",
	 * Park and Miller, Communications of the ACM, vol. 31, no. 10,
	 * October 1988, p. 1195.
	 */
	rdtscll(x);
	hi = x / 127773;
	lo = x % 127773;
	t = 16807 * lo - 2836 * hi;
	if (t <= 0) t += 0x7fffffff;
	return t;
}
/**********************/

static void cos_regs_read(int tid, spdid_t spdid, struct cos_regs *r)
{
	r->regs.ip = cos_thd_cntl(COS_THD_GET_IP, tid, 0, 0);
	r->regs.sp = cos_thd_cntl(COS_THD_GET_SP, tid, 0, 0);
	r->regs.bp = cos_thd_cntl(COS_THD_GET_FP, tid, 0, 0);
	r->regs.ax = cos_thd_cntl(COS_THD_GET_1,  tid, 0, 0);
	r->regs.bx = cos_thd_cntl(COS_THD_GET_2,  tid, 0, 0);
	r->regs.cx = cos_thd_cntl(COS_THD_GET_3,  tid, 0, 0);	
	r->regs.dx = cos_thd_cntl(COS_THD_GET_4,  tid, 0, 0);
	r->regs.di = cos_thd_cntl(COS_THD_GET_5,  tid, 0, 0);
	r->regs.si = cos_thd_cntl(COS_THD_GET_6,  tid, 0, 0);
	r->tid     = tid;
	r->spdid   = spdid;
}

static void cos_regs_set(struct cos_regs *r)
{
	cos_thd_cntl(COS_THD_SET_IP, r->tid, r->regs.ip, 0);
	cos_thd_cntl(COS_THD_SET_SP, r->tid, r->regs.sp, 0);
	cos_thd_cntl(COS_THD_SET_FP, r->tid, r->regs.bp, 0);
	cos_thd_cntl(COS_THD_SET_1,  r->tid, r->regs.ax, 0);
	cos_thd_cntl(COS_THD_SET_2,  r->tid, r->regs.bx, 0);
	cos_thd_cntl(COS_THD_SET_3,  r->tid, r->regs.cx, 0);	
	cos_thd_cntl(COS_THD_SET_4,  r->tid, r->regs.dx, 0);
	cos_thd_cntl(COS_THD_SET_5,  r->tid, r->regs.di, 0);
	cos_thd_cntl(COS_THD_SET_6,  r->tid, r->regs.si, 0);
}

static void cos_regs_print(struct cos_regs *r)
{
	printc("EIP:%10x\tESP:%10x\tEBP:%10x\n"
	       "EAX:%10x\tEBX:%10x\tECX:%10x\n"
	       "EDX:%10x\tEDI:%10x\tESI:%10x\n",
	       (unsigned int)r->regs.ip, (unsigned int)r->regs.sp, (unsigned int)r->regs.bp,
	       (unsigned int)r->regs.ax, (unsigned int)r->regs.bx, (unsigned int)r->regs.cx,
	       (unsigned int)r->regs.dx, (unsigned int)r->regs.di, (unsigned int)r->regs.si);

	return;
}


static void flip_reg_bit(long *reg)
{
	int flip_bit = 0;

	flip_bit = random() & 0x1f;
	/* printc("%2dth bit is going to be flipped ==> \n", flip_bit + 1); */
	flip_bit = 1 << flip_bit;
	
	*reg = (*reg) ^ flip_bit;
}

/* do not flip eip. Now this is just flipping every register, called nuclear bomb style */
static void flip_all_regs(struct cos_regs *r) {
	/* printc("flip all registers for the next instruction, except eip\n"); */

	/* printc("ESP's "); */
	flip_reg_bit(&r->regs.sp); /* esp */
	/* printc("EBP's "); */
	flip_reg_bit(&r->regs.bp); /* ebp */
	/* printc("EAX's "); */
	flip_reg_bit(&r->regs.ax); /* eax */
	/* printc("EBX's "); */
	flip_reg_bit(&r->regs.bx); /* ebx */
	/* printc("ECX's "); */
	flip_reg_bit(&r->regs.cx); /* ecx */
	/* printc("EDX's "); */
	flip_reg_bit(&r->regs.dx); /* edx */
	/* printc("EDI's "); */
	flip_reg_bit(&r->regs.di); /* edi */
	/* printc("ESI's "); */
	flip_reg_bit(&r->regs.si); /* esi */

	cos_regs_set(r);

	return;
}

#endif	/* SWIFI_REGS_H */
