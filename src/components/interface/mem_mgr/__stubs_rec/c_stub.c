/* Memory manager recovery: client stub interface */

/* Jiguo */

/* Question:  This is in client and in case that MM crashes */

/* 1. Where should all clients parameters(e.g. mem_alias's) saved? (on stack) */
/*     -- save all these paprameters on this page to be restored? no: not in kernel */
/*     -- can we preserve a page for saving purpose? no: writable and not corruptible?? */
/*     -- can we use a separate spd for saving purpose? no: overhead */
/*     -- since we can find the root page, can we save everything on */
/*        root page? no: not in kernel */
/*     -- I want them to be saved on interface */
/*           yes: track alias for now, when alloc() page to save, get_page is called */

/* 2. How can an event thread replay all dependent spds invocations? (on request) */
/*     -- if we know where to save all parameters, we can do it? no: on request */
/*     -- does this saving place have to be a central place? no: on each thread request, on each interface */
/*     -- create an additional thread for each spd and block, wakeup no: low priority crap */
/*     -- on crash, mem_mgr initiates a thread to introspect each spd, */
/*           find out what is saved in each spd's page table or cos_pages */
/*           structurte in kernel ? no: possible many low priority crap */

/* 3. How to tell all other threads that MM has failed? */
/*     -- can we use the version number? maybe: not now */
/*     -- can we use the global static counter? yes: similar to torrent */
/*     -- important: Difference between recovery and just get another page */

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <mem_mgr.h>
#include <cstub.h>

extern void *alloc_page(void);
extern void free_page(void *ptr);

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#define MM_PRINT 0

#if MM_PRINT == 1
#define print_mm(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_mm(fmt,...)
#endif

/* global fault counter, only increase, never decrease */
static unsigned long fcounter;

/* recovery data structure */
struct rec_data_mm {
	int idx;              // use ">> PAGE_SHIFT" to get an unique id for this client spd
	
	spdid_t s_spd;
	vaddr_t s_addr;
	spdid_t d_spd;
	vaddr_t d_addr;

	unsigned long fcnt;
};

/**********************************************/
/* slab allocator and cvect for tracking pages */
/**********************************************/

CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));
CVECT_CREATE_STATIC(rec_mm_vect);

void print_rdmm_info(struct rec_data_mm *rdmm);

static struct rec_data_mm *
rdmm_lookup(int idx)
{ return cvect_lookup(&rec_mm_vect, idx); }

static struct rec_data_mm *
rdmm_alloc(void)
{
	struct rec_data_mm *rdmm;
	rdmm = cslab_alloc_rdmm();

	if (!rdmm) {
		printc("can not slab alloc\n");
		BUG();
	}
	return rdmm;
}

static void
rdmm_dealloc(struct rec_data_mm *rdmm)
{
	if (!rdmm) {
		printc("null rdmm\n");
		BUG();
	}

	if (cvect_del(&rec_mm_vect, rdmm->idx)) BUG();
	cslab_free_rdmm(rdmm);

	return;
}


void 
print_rdmm_info(struct rec_data_mm *rdmm)
{
	if (!rdmm) {
		printc("null rdmm\n");
		BUG();
	}

	print_mm("rdmm->idx %d  ",rdmm->idx);
	print_mm("rdmm->s_spd %d  ",rdmm->s_spd);
	print_mm("rdmm->s_addr %d  ",rdmm->s_addr);
	print_mm("rdmm->d_spd %d  ",rdmm->d_spd);
	print_mm("rdmm->d_addr %d  ",rdmm->d_addr);
	print_mm("rdmm->fcnt %ld  ",rdmm->fcnt);
	print_mm("fcounter %ld  ",fcounter);

	return;
}

static void 
rdmm_cons(struct rec_data_mm *rdmm, spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	if (!rdmm) {
		printc("null rdmm\n");
		BUG();
	}
	
	long idx;
	idx = d_addr >> PAGE_SHIFT;
	
	rdmm->s_spd = s_spd;
	rdmm->s_addr = s_addr;
	rdmm->d_spd = d_spd;
	rdmm->d_addr = d_addr;
	
	rdmm->fcnt = fcounter;

	if (cvect_add(&rec_mm_vect, rdmm, idx) == -1) BUG();
	
	return;
}

static struct rec_data_mm *
retrieve_rdmm(vaddr_t addr)
{
	struct rec_data_mm *rdmm, *ret;

	long idx = addr >> PAGE_SHIFT;
	rdmm = rdmm_lookup(idx);
	if (!rdmm) return NULL;

	if(likely(rdmm->fcnt == fcounter)) {
		return rdmm;
	}

	/* if (!ret) BUG(); */
	ret = rdmm;

	return ret;
}

/************************************/
/******  client stub functions ******/
/************************************/

/* Most recovery work is done in MM since the object is distributed all over the system */

CSTUB_FN_ARGS_3(vaddr_t, mman_get_page, spdid_t, spdid, vaddr_t, addr, int, flags)

redo:

CSTUB_ASM_3(mman_get_page, spdid, addr, flags)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(vaddr_t, mman_alias_page, spdid_t, s_spd, vaddr_t, s_addr, spdid_t, d_spd, vaddr_t, d_addr)

       struct rec_data_mm *rdmm;

       rdmm = rdmm_alloc();
       assert(rdmm);
       rdmm_cons(rdmm, s_spd, s_addr, d_spd, d_addr);

redo:

CSTUB_ASM_4(mman_alias_page, s_spd, s_addr, d_spd, d_addr)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_3(int, mman_revoke_page, spdid_t, spdid, vaddr_t, addr, int, flags)

redo:

CSTUB_ASM_3(mman_revoke_page, spdid, addr, flags)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST
