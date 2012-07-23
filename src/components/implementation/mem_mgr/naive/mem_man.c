/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Completely rewritten to use a sane data-structure based on the L4
 * mapping data-base -- Gabriel Parmer, gparmer@gwu.edu, 2011.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * I do _not_ use "embedded mapping nodes" here.  That is, I don't
 * embed the mapping nodes into the per-component "page tables" that
 * are used to look up individual mappings in each component.
 * Additionally, instead of the conventional implementation that has
 * these page table structures point to the frame structure that is
 * the base of the mapping tree, we point directly to the mapping to
 * avoid the O(N) cost when mapping where N is the number of nodes in
 * a mapping tree.  The combination of these design decisions means
 * that we might use more memory and have a few more data cache line
 * accesses.  We use a slab allocator to avoid excessive memory usage
 * for allocating memory mapping structures.  However, we use a very
 * fast (and predictable) lookup structure to perform the (component,
 * address)->mapping lookup.  Unfortunately the memory overhead of
 * that is significant (2 pages per component in the common case).
 * See cvectc.h for an alternative that trades (some) speed for memory
 * usage.
 */

/* 
 * FIXME: locking!
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <print.h>

#include <cos_list.h>
#include "../../sched/cos_sched_sync.h"
#define LOCK()   if (cos_sched_lock_take())    assert(0);
#define UNLOCK() if (cos_sched_lock_release()) assert(0);

#include <mem_mgr.h>
#include <recovery_upcall.h>

/***************************************************/
/*** Data-structure for tracking physical memory ***/
/***************************************************/
struct mapping {
	u16_t   flags;
	spdid_t spdid;
	vaddr_t addr;

	struct frame *f;
	/* child and sibling mappings */
	struct mapping *p, *c, *_s, *s_;
	struct mapping *head, *next;
} __attribute__((packed));

/* A tagged union, where the tag holds the number of maps: */
struct frame {
	int nmaps;
	union {
		struct mapping *m;  /* nmaps > 0 : root of all mappings */
		vaddr_t addr;	    /* nmaps = -1: local mapping */
		struct frame *free; /* nmaps = 0 : no mapping */
	} c;
} frames[COS_MAX_MEMORY];
struct frame *freelist;

#define MAX_UPCALL_LIST_DEPTH 8

static inline int  frame_index(struct frame *f) { return f-frames; }
static inline int  frame_nrefs(struct frame *f) { return f->nmaps; }
static inline void frame_ref(struct frame *f)   { f->nmaps++; }

static struct mapping *mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to, int root_page);
static struct mapping *mapping_lookup(spdid_t spdid, vaddr_t addr);

static inline struct frame *
frame_alloc()
{
	spdid_t root_spd;

	struct frame *f = freelist;
	if (!f) return NULL;
again:
	freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	root_spd = (spdid_t)cos_mmap_introspect(COS_MMAP_INTROSPECT_SPD, 0, 0, 0, frame_index(f));
	if (unlikely(root_spd)){
		if (root_spd == cos_spd_id()) return f;
		f = freelist;
		if (!f) return NULL;
		goto again;
	}
	
	return f;       	/* return an unused frame or NULL or reused */
}

static inline void 
frame_deref(struct frame *f)
{ 
	assert(f->nmaps > 0);
	f->nmaps--; 
	if (f->nmaps == 0) {
		f->c.free = freelist;
		freelist  = f;
	}
}

static void
frame_init(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY-1 ; i++) {
		frames[i].c.free = &frames[i+1];
		frames[i].nmaps  = 0;
	}
	frames[COS_MAX_MEMORY-1].c.free = NULL;
	freelist = &frames[0];
}

static inline void
mm_init(void)
{
	static int first = 1;
	if (unlikely(first)) {
		first = 0;
		frame_init();
	}
}

/*************************************/
/*** Memory allocation shenanigans ***/
/*************************************/
static inline struct frame *frame_alloc();
static inline int frame_index(struct frame *f);
static inline void *
__page_get(void)
{
	spdid_t root_spd;
	vaddr_t root_addr;

	void *hp = cos_get_vas_page(); /* use lib function if USE_VALLOC is not defined */
	struct frame *f;

again:
	f = frame_alloc();

	assert(hp && f);
	frame_ref(f);
	f->nmaps  = -1; 	 /* belongs to us... */
	f->c.addr = (vaddr_t)hp; /* ...at this address */

	root_spd = (spdid_t)cos_mmap_introspect(COS_MMAP_INTROSPECT_SPD, 0, 0, 0, frame_index(f));
	if (unlikely(root_spd == cos_spd_id())){ /* reuse the same page for ourself */
		root_addr = (vaddr_t)cos_mmap_introspect(COS_MMAP_INTROSPECT_ADDR, 0, 0, 0, frame_index(f));
		if (root_addr != (vaddr_t)hp) {
			cos_release_vas_page(hp);
			frame_deref(f);
			goto again;
		}
	}
	
	int ret;
	ret =  cos_mmap_cntl(COS_MMAP_GRANT, COS_MMAP_SET_ROOT, cos_spd_id(), (vaddr_t)hp, frame_index(f));
	if (unlikely(ret)) memset(hp, 0, PAGE_SIZE);
	return hp;
}
#define CPAGE_ALLOC() __page_get()
#include <cpage_alloc.h>

#define CSLAB_ALLOC(sz)   cpage_alloc()
#define CSLAB_FREE(x, sz) cpage_free(x)
#include <cslab.h>

#define CVECT_ALLOC() cpage_alloc()
#define CVECT_FREE(x) cpage_free(x)
#include <cvect.h>

/**********************************************/
/*** Virtual address tracking per component ***/
/**********************************************/

CVECT_CREATE_STATIC(comps);
struct comp_vas {
	int nmaps, spdid;
	cvect_t *pages;
};
CSLAB_CREATE(cvas, sizeof(struct comp_vas));

static struct comp_vas *
cvas_lookup(spdid_t spdid)
{ return cvect_lookup(&comps, spdid); }

static struct comp_vas *
cvas_alloc(spdid_t spdid)
{
	struct comp_vas *cv;
	assert(!cvas_lookup(spdid));
	cv = cslab_alloc_cvas();
	if (!cv) {
		printc("can not vas alloc\n");
		goto done;
	}
	cv->pages = cvect_alloc();
	if (!cv->pages) goto free;
	cvect_init(cv->pages);
	cvect_add(&comps, cv, spdid);

	cv->nmaps = 0;
	cv->spdid = spdid;
done:
	return cv;
free:
	cslab_free_cvas(cv);
	cv = NULL;
	goto done;
}

static void
cvas_ref(struct comp_vas *cv)
{
	assert(cv);
	cv->nmaps++;
}

static void 
cvas_deref(struct comp_vas *cv)
{
	assert(cv && cv->nmaps > 0);
	cv->nmaps--;
	if (cv->nmaps == 0) {
		cvect_free(cv->pages);
		cvect_del(&comps, cv->spdid);
		cslab_free_cvas(cv);
	}
}

/**************************/
/*** Mapping operations ***/
/**************************/
CSLAB_CREATE(mapping, sizeof(struct mapping));

static void
mapping_init(struct mapping *m, spdid_t spdid, vaddr_t a, struct mapping *p, struct frame *f)
{
	assert(m && f);
	INIT_LIST(m, _s, s_);
	m->f     = f;
	m->flags = 0;
	m->spdid = spdid;
	m->addr  = a;
	m->p     = p;
	m->head  = m;
	m->next = NULL;
	if (p) {
		m->flags = p->flags;
		if (!p->c) p->c = m;
		else       ADD_LIST(p->c, m, _s, s_);
	}
}

static struct mapping *
mapping_lookup(spdid_t spdid, vaddr_t addr)
{
	struct comp_vas *cv = cvas_lookup(spdid);

	if (!cv) return NULL;
	return cvect_lookup(cv->pages, addr >> PAGE_SHIFT);
}

/* IMPORTANT: do not make assumption that virtual addresses are all different */
/*            Across components, vaddr could be same in the future */

/* Make a child mapping */
static struct mapping *
mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to, int root_page)
{
	struct comp_vas *cv = cvas_lookup(dest);
	struct mapping *m = NULL;
	long idx = to >> PAGE_SHIFT;
	int mm_flag, ret;

	ret = mm_flag = 0;
	assert(!p || p->f == f);
	assert(dest && to);

	/* no vas structure for this spd yet... */
	if (!cv) {
		cv = cvas_alloc(dest);
		if (!cv) goto done;
		assert(cv == cvas_lookup(dest));
	}
	assert(cv->pages);
	/* if (cvect_lookup(cv->pages, idx)) goto collision; */
	m = cvect_lookup(cv->pages, idx);
	if (unlikely(m)) goto exist;
	/* int i = 0; */
	/* struct mapping *mm = NULL; */
	/* for (i = 0; i<1; i++) { */
	/* 	mm = cvect_lookup(cv->pages, idx+i); */
	/* 	printc("exist m : m->spd %d m->addr %x idx %ld\n", mm->spdid, (unsigned int)mm->addr, idx+i); */
	/* } */
	/* 	goto exist; */
	
	
	cvas_ref(cv);
	m = cslab_alloc_mapping();
	if (!m) goto collision;

#ifdef MEA_ADD_ROOT
	unsigned long long start = 0;
	unsigned long long end = 0;

	if (root_page) { 
		mm_flag = COS_MMAP_SET_ROOT;
		if (dest == 7) rdtscll(start);		
	}
	ret = cos_mmap_cntl(COS_MMAP_GRANT, mm_flag, dest, to, frame_index(f));
	if (root_page && dest == 7) {
		rdtscll(end);
		printc("COST (add root page info into kernel) : %llu\n", end-start);
	}
#else
	if (root_page) mm_flag = COS_MMAP_SET_ROOT;
	ret = cos_mmap_cntl(COS_MMAP_GRANT, mm_flag, dest, to, frame_index(f));
#endif
	if (ret) {
		if (cos_mmap_introspect(COS_MMAP_INTROSPECT_FRAME, 0, dest, to, 0) != frame_index(f)) {
			printc("found mismatch here\n");
			goto no_mapping;
		}
	}
	
	mapping_init(m, dest, to, p, f);
	assert(!p || frame_nrefs(f) > 0);
	frame_ref(f);
	assert(frame_nrefs(f) > 0);
	
	if (cvect_add(cv->pages, m, idx)) BUG();
done:
	return m;
no_mapping:
	cslab_free_mapping(m);
collision:
	cvas_deref(cv);
	m = NULL;
	goto done;
exist:
	assert(m->addr  == to);
	assert(m->spdid == dest);
	m->p = p;   		/* connect two tree or root exists */
	goto done;
}

/* Take all decedents, return them in a list. */
static struct mapping *
__mapping_linearize_decendents(struct mapping *m)
{
	struct mapping *first, *last, *c, *gc;
	
	first = c = m->c;
	m->c = NULL;
	if (!c) return NULL;
	do {
		last = LAST_LIST(first, _s, s_);
		c->p = NULL;
		gc = c->c;
		c->c = NULL;
		/* add the grand-children onto the end of our list of decedents */
		if (gc) ADD_LIST(last, gc, _s, s_);
		c = FIRST_LIST(c, _s, s_);
	} while (first != c);
	
	return first;
}

static void
__mapping_destroy(struct mapping *m)
{
	struct comp_vas *cv;
	int idx;
	/* printc("mapping_destroy: m->spd %d m->addr %x\n", m->spdid, (unsigned int)m->addr); */
	assert(m);
	assert(EMPTY_LIST(m, _s, s_));
	assert(m->p == NULL);
	assert(m->c == NULL);
	cv = cvas_lookup(m->spdid);

	assert(cv && cv->pages);
	assert(m == cvect_lookup(cv->pages, m->addr >> PAGE_SHIFT));
	cvect_del(cv->pages, m->addr >> PAGE_SHIFT);
	cvas_deref(cv);
	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, m->spdid, m->addr, 0);
	assert(idx == frame_index(m->f));
	frame_deref(m->f);

	cslab_free_mapping(m);
}

static void
mapping_del_children(struct mapping *m)
{
	struct mapping *d, *n; 	/* decedents, next */

	assert(m);
	d = __mapping_linearize_decendents(m);

	while (d) {
		n = FIRST_LIST(d, _s, s_);
		REM_LIST(d, _s, s_);
		__mapping_destroy(d);
		d = (n == d) ? NULL : n;
	}

	assert(!m->c);
}

static void
mapping_del(struct mapping *m)
{
	assert(m);
	mapping_del_children(m);
	assert(!m->c);
	if (m->p && m->p->c == m) {
		if (EMPTY_LIST(m, _s, s_)) m->p->c = NULL;
		else                       m->p->c = FIRST_LIST(m, _s, s_);
	}
	m->p = NULL;
	REM_LIST(m, _s, s_);
	__mapping_destroy(m);
}

/**********************************/
/*** Public interface functions ***/
/**********************************/

static void recovery_upcalls_helper(spdid_t spd, vaddr_t addr);
static struct mapping *recovery_rebuild_mapping(spdid_t s_spd, vaddr_t s_addr);

static int get_test = 0;
static int alias_test = 0;
static int revoke_test = 0;

////////////////////////////
/*                 ,--.   */
/*  ,---.  ,---. ,-'  '-. */
/* | .-. || .-. :'-.  .-' */
/* ' '-' '\   --.  |  |   */
/* .`-  /  `----'  `--'   */
/* `---'                  */
////////////////////////////

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m = NULL;
	vaddr_t ret = -1;

/* 	if (spd == 7) printc("mman_get_page....addr %x\n", (unsigned int)addr); */
/* 	if (spd == 7 && get_test < 99) get_test++; */

/* #ifdef MEA_GET	 */
/* 	if (spd == 7 && get_test == 5) assert(0); */
/* #endif */

	LOCK();

	f = frame_alloc();
	if (!f) goto done; 	/* -ENOMEM */
	assert(frame_nrefs(f) == 0);
	frame_ref(f);

	m = mapping_crt(NULL, f, spd, addr, 1);
	if (!m) goto dealloc;
	f->c.m = m;

	assert(!m->next);
	assert(m->addr == addr);
	assert(m->spdid == spd);
	assert(m == mapping_lookup(spd, addr));

	ret = m->addr;
	/* if (spd == 7) */
	/* 	printc("GET %d in spd %d @ %p\n", frame_index(m->f), spd, (void *)addr); */
done:
	UNLOCK();
	return ret;
dealloc:
	frame_deref(f);
	goto done;		/* -EINVAL */
}

/* ///////////////////////////////////////////////////////// */
/* vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr) */
/* { */
/* 	struct mapping *m, *n; */
/* 	vaddr_t ret = 0; */

/* 	if (s_spd == 7 && alias_test < 99) alias_test++; */
/* 	if (s_spd == 7 || s_spd == 8) { */
/* 		/\* printc("mman_alias_page.... k %d\n", alias_test); *\/ */
/* 		printc("s_spd %d @ s_addr %x to d_spd %d @ %p (thd %d)\n", s_spd, (unsigned int)s_addr, d_spd, (void *)d_addr, cos_get_thd_id()); */
/* 	} */

/* #ifdef MEA_ALIAS */
/* 	if (s_spd == 7 && alias_test == 5) assert(0); */
/* #else */
/* 	/\* if (s_spd == 7 && alias_test == 5) assert(0); *\/ */
/* #endif */

/////////////////////////////////////
/*         ,--.,--.                */
/*  ,--,--.|  |`--' ,--,--. ,---.  */
/* ' ,-.  ||  |,--.' ,-.  |(  .-'  */
/* \ '-'  ||  ||  |\ '-'  |.-'  `) */
/*  `--`--'`--'`--' `--`--'`----'  */
/////////////////////////////////////

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n, *p;
	vaddr_t ret = 0;

	LOCK();
	m = mapping_lookup(s_spd, s_addr);
	if (unlikely(!m)) {
		printc("crashed --> rebuild s_addr mapping (thd %d)\n", cos_get_thd_id());
		m = recovery_rebuild_mapping(s_spd, s_addr);
		assert(m);
		assert(m->head == m);
		assert(!m->next);
	}
	assert(m == mapping_lookup(s_spd,s_addr));
	n = mapping_crt(m, m->f, d_spd, d_addr, 0);
	if (!n) goto done;

	assert(!n->next);
	assert(n->addr  == d_addr);
	assert(n->spdid == d_spd);
	assert(n->p == m || n->p == NULL); 

	ret = d_addr;

        /* recovery thread only */
	/* assume recovery thread won't fail */
	static int max_upcalls = 0;
	if (unlikely(2 == cos_get_thd_id())) { 
		/* create the sub upcall_list from n, first head node is m */
		assert(m->head == m);
		if (!m->next) m->next = n;
		else {
			p = m->next;
			m->next = n;
			n->next = p;
		}
		max_upcalls++;
		printc("upcall spd %d record is added to m->spd %d\n", n->spdid, m->spdid);
		struct mapping *a,*b;
		a = m->head;
		do {printc("on list %x\n", a); b = a->next;a=b;} while (a);
		/* assert(max_upcalls < MAX_UPCALL_LIST_DEPTH); */
	}

	/* if (s_spd != 5) */
	/* 	printc("ALI %d from s_spd %d @ s_addr %x to d_spd %d @ %p\n",  */
	/* 	       frame_index(n->f), s_spd, (unsigned int)s_addr, d_spd, (void *)d_addr); */
done:
	UNLOCK();
	return ret;
}

/* ------- test only ------- */
vaddr_t mman_alias_page2(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	get_test = 100;
	alias_test = 100;
	revoke_test = 100;
	return mman_alias_page(s_spd, s_addr, d_spd, d_addr);	
}

/////////////////////////////////////////////////
/*                              ,--.           */
/* ,--.--. ,---.,--.  ,--.,---. |  |,-. ,---.  */
/* |  .--'| .-. :\  `'  /| .-. ||     /| .-. : */
/* |  |   \   --. \    / ' '-' '|  \  \\   --. */
/* `--'    `----'  `--'   `---' `--'`--'`----' */
/////////////////////////////////////////////////

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	/* if (spd == 7) printc("MM: mman_revoke_page....thd %d\n", cos_get_thd_id()); */
	if (spd == 7 && revoke_test < 99) revoke_test++;

#ifdef MEA_REVOKE
	if (spd == 7) assert(0);
#else
	if (spd == 7 && revoke_test == 1 && flags == 0) assert(0);
#endif

	LOCK();

	m = mapping_lookup(spd, addr);
	/* a crash happened before */
	if (unlikely(!m)) {
	again:
		UNLOCK();  /* race condition */
		recovery_upcalls_helper(spd, addr);
		m = mapping_lookup(spd, addr);
		if (!m) goto again;
		LOCK();
		printc("READY thd %d\n", cos_get_thd_id());
	}
	mapping_del_children(m);
	UNLOCK();
	return ret;
}

/////////////////////////////////////////////////////////
/* might not be used in the future. A malloc manager should take
 * care */
int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();

	if (cos_mmap_introspect(COS_MMAP_INTROSPECT_FRAME, 0, spd, addr, 0) == -1) {
		printc("The page %p does not exist when release\n", (void *)addr);
		goto done;
	}

	m = mapping_lookup(spd, addr);
	if (!m) {
		ret = -1;	/* -EINVAL */
		goto done;
	}
	mapping_del(m);
done:
	UNLOCK();
	return ret;
}

void mman_print_stats(void) {}

void mman_release_all(void)
{
	int i;

	LOCK();
	/* kill all mappings in other components */
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		struct frame *f = &frames[i];
		struct mapping *m;

		if (frame_nrefs(f) <= 0) continue;
		m = f->c.m;
		assert(m);
		mapping_del(m);
	}
	UNLOCK();
}


////////////////////////////////////////////////////////////////                                                       
/* ,--.--. ,---.  ,---. ,---.,--.  ,--.,---. ,--.--.,--. ,--. */
/* |  .--'| .-. :| .--'| .-. |\  `'  /| .-. :|  .--' \  '  /  */
/* |  |   \   --.\ `--.' '-' ' \    / \   --.|  |     \   '   */
/* `--'    `----' `---' `---'   `--'   `----'`--'   .-'  /    */
/*                                                  `---'     */
////////////////////////////////////////////////////////////////

static struct mapping *
recovery_rebuild_mapping(spdid_t s_spd, vaddr_t s_addr)
{
	/* if mmaping does not exist, create with a 'NULL previous' node */
	/* this could result many independent trees */
	struct mapping *m = NULL;
	int fr_idx = 0;
	fr_idx = (int)cos_mmap_introspect(COS_MMAP_INTROSPECT_FRAME, 
					  0, s_spd, s_addr, 0);

	assert(fr_idx);
	m = mapping_crt(NULL, &frames[fr_idx], s_spd, s_addr, 0);
	if (!m) goto err;
	
	assert(!m->next);
	assert(fr_idx == frame_index(m->f));
	assert(m->p == NULL);
done:
	return m;
err:
	m = NULL;
	goto done;
}

static void
recovery_upcalls_helper(spdid_t spd, vaddr_t addr)
{
	struct mapping *m, *n, *p;
	int thd;

	printc("\n[.....entering upcall helper........] thd %d\n", cos_get_thd_id());
	thd = cos_get_thd_id();
	recovery_upcall(cos_spd_id(), spd, addr);
	assert(thd == cos_get_thd_id());

	/* a upcall list that facilitates upcall should have been
	 * created by above replay alias */
	m = mapping_lookup(spd, addr); /* m should exist now  */
	assert(m);

	n = m->head->next;
	while (n) {
		printc("<<upcall to n->spdid %d, n->addr %x thd %d>>\n", n->spdid, (unsigned int)n->addr, cos_get_thd_id());
		printc("before:list is %x, thd %d\n", n, cos_get_thd_id());
		recovery_upcall(cos_spd_id(), n->spdid, n->addr);
		printc("after:list is %x, thd %d\n", n, cos_get_thd_id());
		p = n->next;
		n = p;
	}
	printc("leaving upcall helper......... thd %d\n", cos_get_thd_id());
}

/*******************************/
/*** The base-case scheduler ***/
/*******************************/

#include <sched_hier.h>

int  sched_init(void)   { return 0; }
extern void parent_sched_exit(void);
void 
sched_exit(void)   
{
	mman_release_all(); 
	parent_sched_exit();
}

int sched_isroot(void) { return 1; }

int 
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff) { BUG(); return 0; }

extern int parent_sched_child_cntl_thd(spdid_t spdid);

int 
sched_child_cntl_thd(spdid_t spdid) 
{ 
	if (parent_sched_child_cntl_thd(cos_spd_id())) BUG();
	if (cos_sched_cntl(COS_SCHED_PROMOTE_CHLD, 0, spdid)) BUG();
	if (cos_sched_cntl(COS_SCHED_GRANT_SCHED, cos_get_thd_id(), spdid)) BUG();

	return 0;
}

int 
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }


void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
		/* printc("MM: thread %d\n", cos_get_thd_id()); */
		mm_init(); break;
	default:
		BUG(); return;
	}

	return;
}
