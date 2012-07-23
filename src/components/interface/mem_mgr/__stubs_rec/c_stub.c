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
	spdid_t d_spd;
	vaddr_t d_addr;
	
	struct rec_data_mm *next;
};

struct rec_data_mm_list {
	int id;
	vaddr_t s_addr;
	int recordable;
	unsigned long fcnt;
	struct rec_data_mm first;
	struct rec_data_mm *head, *tail;
};

/**********************************************/
/* slab allocator and cvect for tracking pages */
/**********************************************/

CVECT_CREATE_STATIC(rec_mm_vect);
CSLAB_CREATE(rdmm, sizeof(struct rec_data_mm));
CSLAB_CREATE(rdmm_ls, sizeof(struct rec_data_mm_list));

void print_rdmm_info(struct rec_data_mm *rdmm);
void print_rdmm_list(struct rec_data_mm_list *rdmm_list);

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
/*   record list operation functions    */
/*--------------------------------------*/
static struct rec_data_mm_list *
rdmm_list_init(long id)
{
	struct rec_data_mm_list *rdmm_list;

	rdmm_list = cslab_alloc_rdmm_ls();
	assert(rdmm_list);
	rdmm_list->id = id;
	rdmm_list->fcnt = fcounter;
	rdmm_list->recordable = 1;

	rdmm_list->head = rdmm_list->tail = &rdmm_list->first;

	if (cvect_add(&rec_mm_vect, rdmm_list, rdmm_list->id)) {
		printc("Cli: can not add list into cvect\n");
		return NULL;
	}
	/* printc("list inint done @ %x\n", rdmm_list); */
	return rdmm_list;
}

static struct rec_data_mm_list *
rdmm_list_lookup(int id)
{ 
	return cvect_lookup(&rec_mm_vect, id); 
}

static int
rdmm_list_free(struct rec_data_mm_list *rdmm_list)
{
	assert(rdmm_list);
	if (cvect_del(&rec_mm_vect, rdmm_list->id)) {
		printc("Cli: can not del from cvect\n");
		return -1;
	}
	cslab_free_rdmm_ls(rdmm_list);
	return 0;
}

static struct rec_data_mm *
rdmm_list_rem(struct rec_data_mm_list *rdmm_list)
{
	struct rec_data_mm *rdmm;
	if (!rdmm_list || !rdmm_list->head) return NULL;
	rdmm = rdmm_list->head;
	assert(rdmm);
	if (!rdmm->next) {
		assert(rdmm == rdmm_list->tail);
		rdmm_list_free(rdmm_list);
		return NULL;
	}
	rdmm_list->head = rdmm->next;
	return rdmm;
}

static void
rdmm_list_add(struct rec_data_mm *rdmm, struct rec_data_mm_list *rdmm_list)
{
	assert(rdmm && rdmm_list->head);
	if (rdmm == rdmm_list->head) return;
	rdmm_list->tail->next = rdmm;
	rdmm_list->tail = rdmm;
}

/*--------------------------------------*/
/*        record operation functions    */
/*--------------------------------------*/

/* only called by alias */
static struct rec_data_mm_list *
record_add(struct rec_data_mm_list *rdmm_list, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	/* if (cos_spd_id() == 7 || cos_spd_id() == 8) printc("recording....\n"); */
	struct rec_data_mm *rdmm;
	if (!rdmm_list) {
		rdmm_list = rdmm_list_init(s_addr >> PAGE_SHIFT);
		assert(rdmm_list && rdmm_list->head);
		rdmm_list->s_addr = s_addr;
		rdmm = &rdmm_list->first;
		/* printc("list init\n"); */
	} else {
		rdmm = rdmm_alloc();
		assert(rdmm);
	}
	/* if (cos_spd_id() == 7 || cos_spd_id() == 8) printc("record added here rdmm_list %x s_addr %x d_addr %x\n", rdmm_list, s_addr, d_addr); */
	rdmm->d_spd = d_spd;
	rdmm->d_addr = d_addr;
	rdmm_list_add(rdmm, rdmm_list);
	rdmm->next = NULL;
	/* if (cos_spd_id() == 7 || cos_spd_id() == 8) printc("recording....done!\n"); */

	return rdmm_list;
}

static void
record_rem(struct rec_data_mm_list *rdmm_list)
{
	struct rec_data_mm *rdmm;
	assert(rdmm_list);
	rdmm = rdmm_list_rem(rdmm_list);
	while(rdmm) {
		/* printc("keep removing item...\n"); */
		rdmm = rdmm_list_rem(rdmm_list);
		if(rdmm) rdmm_dealloc(rdmm);
	}
	assert(!rdmm_list_lookup(rdmm_list->id));
}

static void
record_replay(struct rec_data_mm_list *rdmm_list)
{
	struct rec_data_mm *rdmm;
	vaddr_t s_addr;
	assert(rdmm_list);
	/* printc("In Cli %ld: ready to replay now...thread %d\n", cos_spd_id(), cos_get_thd_id()); */

	/* print_rdmm_list(rdmm_list); */
	rdmm = rdmm_list->head;
	s_addr = rdmm_list->s_addr;
	assert(rdmm);
	while (1) {
		rdmm_list->recordable = 0; /* no more same records added during the replay alias */
		/* printc("replay::<rdmm->s_spd %ld rdmm->s_addr %x rdmm->d_spd %d rdmm->d_addr %x>\n", */
		/*        cos_spd_id(), (unsigned int)s_addr, rdmm->d_spd, (unsigned int)rdmm->d_addr); */
		if (rdmm->d_addr != mman_alias_page(cos_spd_id(), s_addr, rdmm->d_spd, rdmm->d_addr)) BUG();
		if (!(rdmm = rdmm->next)) break;
	}
	rdmm_list->recordable = 1;
	rdmm_list->fcnt = fcounter;
	/* printc("In Cli %ld: replay done...thread %d\n", cos_spd_id(), cos_get_thd_id()); */
	return;
}

/*--------------------------------------*/
/*        other operation functions     */
/*--------------------------------------*/

/* Do we need a client lock here in case that: when the record is
   replayed by one thread nad preempted by another thread? */
void
update_info(struct rec_data_mm_list *rdmm_list)
{
	/* printc("((Cstub:update crash status thd %d) spd %d)\n", cos_get_thd_id(), cos_spd_id()); */
	if (unlikely(rdmm_list->fcnt != fcounter)) {
		printc(" -- mm crashed before when this spd called it -- thd %d\n", cos_get_thd_id());
		record_replay(rdmm_list);
	}
	return;
}

/* replay all alias from root pages for this component */
void 
alias_replay(vaddr_t s_addr)
{
	/* printc("In Cli %ld: from upcall recovery, thread %d\n", cos_spd_id(), cos_get_thd_id()); */
	struct rec_data_mm_list *rdmm_list;
	rdmm_list = rdmm_list_lookup(s_addr >> PAGE_SHIFT);
	if (rdmm_list) record_replay(rdmm_list);
	return;
}

/************************************/
/******  client stub functions ******/
/************************************/
/* test purpose only */
static int measure_first = 0;
unsigned long long start = 0;
unsigned long long end = 0;

/////////////////////////////////////////
/*  ,---. ,--------.,--. ,--.,-----.   */
/* '   .-''--.  .--'|  | |  ||  |) /_  */
/* `.  `-.   |  |   |  | |  ||  .-.  \ */
/* .-'    |  |  |   '  '-'  '|  '--' / */
/* `-----'   `--'    `-----' `------'  */
/////////////////////////////////////////

CSTUB_FN_ARGS_3(vaddr_t, mman_get_page, spdid_t, spdid, vaddr_t, addr, int, flags)

redo:

CSTUB_ASM_3(mman_get_page, spdid, addr, flags)

       if (unlikely (fault)){
       	       fcounter++;
       	       goto redo;
       }

CSTUB_POST


CSTUB_FN_ARGS_4(vaddr_t, mman_alias_page, spdid_t, s_spd, vaddr_t, s_addr, spdid_t, d_spd, vaddr_t, d_addr)

        struct rec_data_mm_list *rdmm_list;
	rdmm_list = rdmm_list_lookup(s_addr >> PAGE_SHIFT);
	if (cos_spd_id() == 7 || cos_spd_id() == 8) {
		printc("Cli(spd %d): alias cstub from s_spd %d (s_addr %x) to d_spsd %d (d_addr %x)\n",
		       cos_spd_id(), s_spd, (unsigned int)s_addr, d_spd, (unsigned int)d_addr);
	}

        if (!rdmm_list || rdmm_list->recordable == 1) {
		rdmm_list = record_add(rdmm_list, s_addr, d_spd, d_addr);
	}

	assert(rdmm_list);
redo:

CSTUB_ASM_4(mman_alias_page, s_spd, s_addr, d_spd, d_addr)

       if (unlikely (fault)){
       	       fcounter++;
	       rdmm_list->recordable = 0;
       	       goto redo;
       }

CSTUB_POST


/* /\* test use only *\/ */
/* CSTUB_FN_ARGS_4(vaddr_t, mman_alias_page2, spdid_t, s_spd, vaddr_t, s_addr, spdid_t, d_spd, vaddr_t, d_addr) */

/*         struct rec_data_mm_list *rdmm_list; */
/* 	rdmm_list = rdmm_list_lookup(s_addr >> PAGE_SHIFT); */
/*         if (!rdmm_list || rdmm_list->recordable == 1) { */
/* 		rdmm_list = record_add(rdmm_list, s_addr, d_spd, d_addr); */
/* 	} */
/* 	assert(rdmm_list); */
/* redo: */

/* CSTUB_ASM_4(mman_alias_page2, s_spd, s_addr, d_spd, d_addr) */

/*        if (unlikely (fault)){ */
/*        	       fcounter++; */
/* 	       rdmm_list->recordable = 0; */
/*        	       goto redo; */
/*        } */

/* CSTUB_POST */


CSTUB_FN_ARGS_3(int, mman_revoke_page, spdid_t, spdid, vaddr_t, addr, int, flags)

       struct rec_data_mm_list *rdmm_list;
       measure_first = 0;
       rdmm_list = rdmm_list_lookup(addr >> PAGE_SHIFT);
       /* assert(rdmm_list); */
       if (!rdmm_list) goto done;

       flags = 0; 		/* test */

       /* update_info(rdmm_list); */   //can not deal with the further levels
redo:

CSTUB_ASM_3(mman_revoke_page, spdid, addr, flags)

       if (unlikely (fault)){
       	       fcounter++;
	       flags = 1;  	/* test */
       	       goto redo;
       }

       record_rem(rdmm_list);

done:

CSTUB_POST

/*****************/
/* Debug helper  */
/*****************/
void
print_rdmm_info(struct rec_data_mm *rdmm)
{
	assert(rdmm);

	print_mm("rdmm->idx %d  ",rdmm->idx);
	print_mm("rdmm->d_spd %d  ",rdmm->d_spd);
	print_mm("rdmm->d_addr %d  ",rdmm->d_addr);
	print_mm("rdmm->fcnt %ld  ",rdmm->fcnt);
	print_mm("fcounter %ld  ",fcounter);
	
	return;
}

void print_rdmm_list(struct rec_data_mm_list *rdmm_list)
{
	assert(rdmm_list);
	printc("list cnt %lu fcounter %lu\n", rdmm_list->fcnt, fcounter);
	struct rec_data_mm *rdmm;
	rdmm = rdmm_list->head;
	vaddr_t s_addr = rdmm_list->s_addr;
	int test = 0;
	while (1) {
		test++;
		printc("rdmm %x s_spd %ld s_addr %x rdmm->d_spd %d rdmm->d_addr %x\n",
		       (unsigned int)rdmm, cos_spd_id(), (unsigned int)s_addr, rdmm->d_spd, (unsigned int)rdmm->d_addr);
		rdmm = rdmm->next;
		if (!rdmm) break;
	}
	printc("total records %d\n", test);
}

