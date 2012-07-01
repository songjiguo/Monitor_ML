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

struct rec_data_mm_list;
/* recovery data structure for each alias */
struct rec_data_mm {
	vaddr_t s_addr;
	spdid_t d_spd;
	vaddr_t d_addr;
	
	struct rec_data_mm *next;
};

struct rec_data_mm_list {
	int id;              /* ">> PAGE_SHIFT" */
	unsigned long fcnt;
	struct rec_data_mm *head;
};

/**********************************************/
/* slab allocator and cvect for tracking pages */
/**********************************************/

CVECT_CREATE_STATIC(rec_mm_vect);
CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));
CSLAB_CREATE(rdmm_ls, sizeof(struct rec_data_mm_list));

void print_rdmm_info(struct rec_data_mm *rdmm);

/*------------------------------------------*/
/* item functions: each alias page mapping  */
/*------------------------------------------*/

static struct rec_data_mm *
rdmm_alloc(void)
{
	struct rec_data_mm *rdmm;
	rdmm = cslab_alloc_rdmm();
	assert(rdmm);

	return rdmm;
}

static void
rdmm_dealloc(struct rec_data_mm *rdmm)
{
	assert(rdmm);
	cslab_free_rdmm(rdmm);
}

/*--------------------------------------*/
/* list initialization functions        */
/*--------------------------------------*/
static struct rec_data_mm_list *
rdmm_list_init(long id)
{
	struct rec_data_mm_list *rdmm_list;

	rdmm_list = cslab_alloc_rdmm_ls();
	assert(rdmm_list);
	rdmm_list->id = id;
	rdmm_list->head = rdmm_alloc();
	rdmm_list->head->next = NULL;

	if (cvect_add(&rec_mm_vect, rdmm_list, id)) {
		printc("can not add list into cvect\n");
		return NULL;
	}
    
	return rdmm_list;
}

static int
rdmm_list_free(struct rec_data_mm_list *rdmm_list)
{
	assert(rdmm_list);
	if (cvect_del(&rec_mm_vect, rdmm_list->id)) {
		printc("can not del from cvect\n");
		return -1;
	}

	cslab_free_rdmm_ls(rdmm_list);
	return 0;
}

/*-------------------------------------------*/
/*   operation functions on list    */
/*-------------------------------------------*/

static struct rec_data_mm *
rdmm_list_head(struct rec_data_mm_list *rdmm_list)
{
	if (!rdmm_list || !rdmm_list->head) return NULL;
	/* if (!rdmm_list->head->next) return rdmm_list->head; */
	return rdmm_list->head;
}

static struct rec_data_mm_list *
rdmm_list_lookup(int id)
{ 
	return cvect_lookup(&rec_mm_vect, id); 
}

static struct rec_data_mm *
rdmm_list_rem(struct rec_data_mm_list *rdmm_list)
{
	struct rec_data_mm *rdmm;

	rdmm = rdmm_list_head(rdmm_list);
	if (!rdmm) return NULL;
	rdmm_list->head = rdmm->next;

	return rdmm;
}

static void
rdmm_list_add(struct rec_data_mm *rdmm, struct rec_data_mm_list *rdmm_list)
{
	assert(rdmm && rdmm_list->head);
	rdmm->next = rdmm_list->head;
	rdmm_list->head = rdmm;
}

static void
rdmm_cons(struct rec_data_mm *rdmm, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	assert(rdmm);

	rdmm->s_addr = s_addr;
	rdmm->d_spd = d_spd;
	rdmm->d_addr = d_addr;

	return;
}

/* only called alias */
static struct rec_data_mm_list *
add_record(vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct rec_data_mm_list *rdmm_list = NULL;
	struct rec_data_mm *rdmm;

	long id;
	id = s_addr >> PAGE_SHIFT;
	
	rdmm_list = rdmm_list_lookup(id);
	printc("id %ld\n", id);

	printc("1\n");
	if (!rdmm_list) {
		rdmm_list = rdmm_list_init(id);
		printc("2\n");
		assert(rdmm_list && rdmm_list->head);
		rdmm = rdmm_list->head;
	} else {
		printc("list id %ld\n", rdmm_list->id);
		rdmm = rdmm_alloc();
		printc("3\n");
	}
	assert(rdmm && rdmm_list);

	rdmm_list->fcnt = fcounter; /* update the failure counter on each page_alias*/

	rdmm_cons(rdmm, s_addr, d_spd, d_addr);
	printc("4\n");
	printc("spd %ld -> d_spd %ld \n", cos_spd_id(), d_spd);
	printc("s_addr %x -> d_addr %x \n", s_addr, d_addr);

	if (rdmm == rdmm_list->head) goto done;

	rdmm_list_add(rdmm, rdmm_list);
	printc("3\n");

done:
	return rdmm_list;
}

/* only called by revoke */
static void
rem_record(vaddr_t addr)
{
        struct rec_data_mm_list *rdmm_list;
	struct rec_data_mm *rdmm;
	
	long id;
	id = addr << PAGE_SHIFT;

	rdmm_list = rdmm_list_lookup(id);
        if (!rdmm_list) {
		printc("can not find list\n");
		BUG();
	}

	rdmm = rdmm_list_rem(rdmm_list);
	while(rdmm->next) {
		rdmm_dealloc(rdmm);
		rdmm = rdmm_list_rem(rdmm_list);
	}
	assert(rdmm && !rdmm->next);
	rdmm_dealloc(rdmm);

	rdmm_list_free(rdmm_list);
	assert(!rdmm_list_lookup(id));
}

static void
update_rdmm(struct rec_data_mm_list *list)
{
	if (likely(list->fcnt == fcounter)) return; /* no failure happened before */

	/* otherwise, failed before */
	/* active THE recovery thread here */
	/* find list, then traverse the list and replay all page_alias from that list */
	/* in this case, always introspect root page and start from root page */

	return;	
}


/* /\* this should be executed by THE recovery thread, from root page!!! *\/ */
/* void */
/* replay_record(struct rec_data_mm_list *list) */
/* { */
/* 	struct rec_data_mm *rdmm = NULL; */
	
/* 	rdmm = rdmm_list_head(list); */
/* 	if (!rdmm) return; */

/* 	while(1) { */
/* 		/\* do something here *\/ */
/* 		mman_alias_page(cos_spd_id(), rdmm->s_addr, rdmm->d_spd, rdmm->d_addr); */
/* 		/\* do something above *\/		 */
/* 		if (!rdmm->next) break; */
/* 		rdmm = rdmm->next; */
/* 	} */

/* 	return; */
/* } */

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

        /* if (!(add_record(s_addr, d_spd, d_addr))) { */
	/* 	printc("can not record the alias data onto list\n"); */
	/* 	BUG(); */
	/* } */
redo:
CSTUB_ASM_4(mman_alias_page, s_spd, s_addr, d_spd, d_addr)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_3(int, mman_revoke_page, spdid_t, spdid, vaddr_t, addr, int, flags)

        struct rec_data_mm_list *rdmm_list;
	rdmm_list = rdmm_list_lookup(addr << PAGE_SHIFT);
        if (!rdmm_list) {
		printc("can not record the alias data onto list\n");
		BUG();
	}

redo:
        update_rdmm(rdmm_list);	/* THE recovery thread could be actived -- fault happened before */
CSTUB_ASM_3(mman_revoke_page, spdid, addr, flags)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

       /* rem_record(addr); */

CSTUB_POST




/* Debug helper only */
void 
print_rdmm_info(struct rec_data_mm *rdmm)
{
	assert(rdmm);

	print_mm("rdmm->idx %d  ",rdmm->idx);
	print_mm("rdmm->s_addr %d  ",rdmm->s_addr);
	print_mm("rdmm->d_spd %d  ",rdmm->d_spd);
	print_mm("rdmm->d_addr %d  ",rdmm->d_addr);
	print_mm("rdmm->fcnt %ld  ",rdmm->fcnt);
	print_mm("fcounter %ld  ",fcounter);

	return;
}
