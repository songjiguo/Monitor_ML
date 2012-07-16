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

/* mmap_cntl ERROR Code */
#define EINVALC -1         	/* invalid call */
#define EGETPHY -2		/* can not get a physical page */
#define EADDPTE -3		/* can not add entry to page table */

/* mmap_inctrospect ERROR Code */
#define ENOFRAM -4		/* introspect frame failed */

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

static inline int  frame_index(struct frame *f) { return f-frames; }
static inline int  frame_nrefs(struct frame *f) { return f->nmaps; }
static inline void frame_ref(struct frame *f)   { f->nmaps++; }

static struct mapping *mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to, int root_page);
static struct mapping *mapping_lookup(spdid_t spdid, vaddr_t addr);

static inline struct frame *
frame_alloc(void)
{
	vaddr_t root_addr;
	/* spdid_t root_spd; */

	/* int i; */

	struct frame *f = freelist;
	/* struct mapping *m = NULL; */

	if (!f) return NULL;

again:
	freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	root_addr = (vaddr_t)cos_mmap_introspect(COS_MMAP_INTRO_RTADDR, 
						 0, 0, 0, frame_index(f));
	/* printc("frame alloc: number %d root_addr %x\n", frame_index(f), root_addr); */
	/* an root address and its frame number found, just recreate it alone */
	if (unlikely(root_addr)) {
		/* printc("found an used root_addr!!!\n"); */

		/* for ( i = 1; i<10; i++) { */
		/* 	root_addr = (vaddr_t)cos_mmap_introspect(COS_MMAP_INTRO_RTADDR, */
		/* 						 0, 0, 0, frame_index(f)+i); */
		/* 	printc("root_addr %x\n", root_addr); */
		/* } */
		
		/* root_spd = (spdid_t)cos_mmap_introspect(COS_MMAP_INTRO_RTSPD,  */
		/* 					0, 0, 0, frame_index(f)); */
		/* printc("0\n"); */
		/* assert(root_spd); */
		/* frame_ref(f); */
		/* m = mapping_crt(NULL, f, root_spd, root_addr, 1); */
		/* printc("1\n"); */
		/* if (!m) { */
		/* 	printc("can not create mapping\n"); */
		/* 	goto err; */
		/* } */
		/* f->c.m = m; */
		
		/* assert(m->addr == root_addr); */
		/* assert(m->spdid == root_spd); */
		/* assert(m == mapping_lookup(root_spd, root_addr)); */
		f = freelist;
		if (!f) return NULL;
		goto again;
	}

/* done: */
	/* printc("return frame_alloc() number %d root_addr %x\n", frame_index(f), root_addr); */
	return f;       	/* return an unused frame or NULL */
/* err: */
/* 	f = NULL; */
/* 	goto done; */
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
static inline struct frame *frame_alloc(void);
static inline int frame_index(struct frame *f);
static inline void *
__page_get(void)
{
	void *hp = cos_get_vas_page(); /* use lib function if USE_VALLOC is not defined */
	struct frame *f = frame_alloc();
	/* printc("__page_get (hp %x frame_id %d)\n", hp, frame_index(f)); */
	assert(hp && f);
	frame_ref(f);
	f->nmaps  = -1; 	 /* belongs to us... */
	f->c.addr = (vaddr_t)hp; /* ...at this address */

	int ret;
	ret =  cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), (vaddr_t)hp, frame_index(f));
	if (ret == -2) {
		printc("can not get a physical page\n");
		BUG();
	}
	
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
	/* printc("create cvas for spd %d\n", spdid); */
	struct comp_vas *cv;
	assert(!cvas_lookup(spdid));
	cv = cslab_alloc_cvas();
	if (!cv) {
		printc("can not vas alloc\n");
		goto done;
	}
	cv->pages = cvect_alloc();
	/* printc("after cvect alloc\n"); */
	if (!cv->pages) goto free;
	cvect_init(cv->pages);
	/* printc("after cvect init\n"); */
	/* int i; */
	/* for(i=4;i<11;i++) printc("before cvect_add: id %d %x\n", i, (unsigned int)cvas_lookup(i)); */
	cvect_add(&comps, cv, spdid);
	/* for(i=4;i<11;i++) printc("after cvect_add: id %d %x\n", i, (unsigned int)cvas_lookup(i)); */

	/* printc("after cvect add\n"); */
	cv->nmaps = 0;
	cv->spdid = spdid;
done:
	return cv;
free:
	cslab_free_cvas(cv);
	/* printc("free cslab free\n"); */
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

	/* int i = 0; */
	/* struct mapping *mm = NULL; */
	
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
	if (unlikely(m)) {
		/* for (i = 0; i<1; i++) { */
		/* 	mm = cvect_lookup(cv->pages, idx+i); */
		/* 	printc("exist m : m->spd %d m->addr %x idx %ld\n", mm->spdid, (unsigned int)mm->addr, idx+i); */
		/* } */
		goto exist;
	}

	cvas_ref(cv);
	m = cslab_alloc_mapping();
	if (!m) goto collision;

#ifdef MEA_ADD_ROOT
	unsigned long long start = 0;
	unsigned long long end = 0;

	if (root_page) { 
		mm_flag = COS_MMAP_SET_ROOT;
		/* printc("spd %d add root info!!!\n", dest); */
		if (dest == 7) rdtscll(start);		
	}
	ret = cos_mmap_cntl(COS_MMAP_GRANT, mm_flag, dest, to, frame_index(f));
	if (root_page && dest == 7) {
		rdtscll(end);
		printc("COST (add root page info into kernel) : %llu\n", end-start);
	}
#else
	if (root_page) { 
		mm_flag = COS_MMAP_SET_ROOT;
		/* printc("spd %d add root info!!!\n", dest); */
	}
	ret = cos_mmap_cntl(COS_MMAP_GRANT, mm_flag, dest, to, frame_index(f));
#endif

	if (ret == EGETPHY) {
		printc("Error: can not find physical page\n");
		goto no_mapping;
	}
	/* if (ret == EADDPTE) { */
	/* 	printc("no maping, but page exists -- must in recovery\n"); */
	/* } */
	mapping_init(m, dest, to, p, f);
	assert(!p || frame_nrefs(f) > 0);
	frame_ref(f);
	assert(frame_nrefs(f) > 0);
	
	/* if (m->spdid!=5 && m->spdid!=6 && m->spdid!=9) printc("add mapping: m->spd %d m->addr %x idx %ld\n", m->spdid, (unsigned int)m->addr, idx); */
	if (cvect_add(cv->pages, m, idx)) BUG();
	/* if (m->spdid!=5 && m->spdid!=6) printc("check m : m->spd %d m->addr %x idx %ld\n", m->spdid, (unsigned int)m->addr, idx); */


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
	/* if (m->spdid == 8) */
	/* 	printc("destroy in spd %d m->addr %p frame number %d\n", m->spdid, (void *)m->addr, frame_index(m->f)); */
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

/* static int get_test = 0; */
vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m = NULL;
	vaddr_t ret = -1;


	/* if (spd == 7) printc("mman_get_page....addr %x\n", (unsigned int)addr); */
	/* if (spd == 7 && get_test < 99) get_test++; */
	/* if (spd == 7 && get_test == 1) assert(0); */

#ifdef MEA_GET	
	if (spd == 7) assert(0);
#endif
	LOCK();

	f = frame_alloc();
	if (!f) goto done; 	/* -ENOMEM */
	assert(frame_nrefs(f) == 0);
	frame_ref(f);

	m = mapping_crt(NULL, f, spd, addr, 1);
	if (!m) goto dealloc;
	f->c.m = m;

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

static struct mapping *
restore_mapping(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	/* if mmaping does not exist, create with a 'NULL previous' node */
	/* this could result many independent trees */
	struct mapping *m = NULL;
	int fr_idx = 0;
	/* printc("<<<restore mapping spd %d addr %x\n", s_spd, (unsigned int)s_addr); */
	fr_idx = (int)cos_mmap_introspect(COS_MMAP_INTRO_FRAME, 
					  0, s_spd, s_addr, 0);

	if (fr_idx == ENOFRAM) {
		printc("can not find frame\n");
		goto done;
	}

	/* printc("@ fr_idx %d \n", fr_idx); */
	m = mapping_crt(NULL, &frames[fr_idx], s_spd, s_addr, 0);
	if (!m) goto err;

	/* printc("restore mapping done!>>>\n"); */
	assert(fr_idx == frame_index(m->f));
	assert(m->p == NULL);
done:
	return m;
err:
	m = NULL;
	goto done;
}

static int alias_test = 0;

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n;
	vaddr_t ret = 0;

	if (s_spd == 7 && alias_test < 99) alias_test++;
	if (s_spd == 7) {
		printc("mman_alias_page.... k %d\n", alias_test);
		printc("s_spd %d @ s_addr %x to d_spd %d @ %p\n", s_spd, (unsigned int)s_addr, d_spd, (void *)d_addr);
	}

#ifdef MEA_ALIAS
	if (s_spd == 7 && alias_test == 5) assert(0);
#else
	if (s_spd == 7 && alias_test == 5) {
		/* int i; */
		/* for(i=4;i<11;i++) printc("before assert : spd %d cvas %x\n", i, (unsigned int)cvas_lookup(i)); */
		assert(0);
	}
#endif

	LOCK();
	
	m = mapping_lookup(s_spd, s_addr);
	/* crashed!! */
	if (unlikely(!m)) {
		/* d_spd = 8; */
		/* printc("crashed!!\n"); */
		/* int i; */
		/* for(i=4;i<11;i++) printc("before restore : spd %d cvas %x\n", i, (unsigned int)cvas_lookup(i)); */
		/* unsigned long long t1, t2; */
		/* rdtscll(t1); */
		m = restore_mapping(s_spd, s_addr, 0, 0);
		/* rdtscll(t2); */
		/* printc("mapping cost of s_spd is %llu\n", t2-t1); */
		/* for(i=4;i<11;i++) printc("after restore : spd %d cvas %x\n", i, (unsigned int)cvas_lookup(i)); */

		/* restore cvas for dest */		
		/* if (cvas_lookup(d_spd)) cvect_del(&comps, d_spd); */
		/* for(i=4;i<11;i++) printc("restore cvas 2: id %d %x\n", i, (unsigned int)cvas_lookup(i)); */
		/* cvas_alloc(d_spd); */
		/* for(i=4;i<11;i++) printc("restore cvas 3: id %d %x\n", i, (unsigned int)cvas_lookup(i)); */
	}
	assert(m == mapping_lookup(s_spd,s_addr));

	n = mapping_crt(m, m->f, d_spd, d_addr, 0);
	if (!n) goto done;
	assert(n->addr  == d_addr);
	assert(n->spdid == d_spd);
	assert(n->p == m || n->p == NULL); 

	ret = d_addr;
	/* if (s_spd != 5) */
	/* 	printc("ALI %d from s_spd %d @ s_addr %x to d_spd %d @ %p\n",  */
	/* 	       frame_index(n->f), s_spd, (unsigned int)s_addr, d_spd, (void *)d_addr); */
done:
	UNLOCK();
	return ret;
}


vaddr_t mman_alias_page2(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	alias_test = 100;
	return mman_alias_page(s_spd, s_addr, d_spd, d_addr);	
}

static struct mapping *
rebuild_mapping_tree(spdid_t spd, vaddr_t addr, int flags)
{
	int fr_idx;
	vaddr_t root_addr;
	spdid_t root_spd;

	struct mapping *m = NULL;
	printc("rebuild mapping tree\n");
	assert(!mapping_lookup(spd, addr));

	fr_idx = (int)cos_mmap_introspect(COS_MMAP_INTRO_FRAME, 
					  0, spd, addr, 0);
	if (fr_idx == -1) {
		printc("can not find frame\n");
		goto done;
	}

	root_addr = (vaddr_t)cos_mmap_introspect(COS_MMAP_INTRO_RTADDR, 
						 0, spd, addr, fr_idx);
	if (root_addr == 0) {
		printc("can not find root page address\n");
		goto done;
	}
	
	root_spd = (spdid_t)cos_mmap_introspect(COS_MMAP_INTRO_RTSPD, 
						0, spd, addr, fr_idx);
	if (root_spd == 0) {
		printc("can not find root spd\n");
		goto done;
	}

	/* Start to replay get/alias from root_addr in root_spd */
	/* maybe called from super_booter at lower level? */
	/* like on page fault and have a thread call mm? */

	/* THE recovery thread ... ? */
done:	
	return m;
}

static int revoke_test = 0;

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	/* printc("\n mman_revoke_page....\n"); */
	if (spd == 7 && revoke_test < 99) revoke_test++;

#ifdef MEA_REVOKE
	if (spd == 7 && revoke_test == 3) assert(0);
#endif

	LOCK();

	m = mapping_lookup(spd, addr);
	/* if (spd == 7) */
	/* 	printc("REV %d from spd %d @ %p\n", frame_index(m->f),spd, (void *)addr); */
	/* a crash happened before */

	if (unlikely(!m)) {
		printc("rebuild...\n");
		m = rebuild_mapping_tree(spd, addr, flags);
		if (!m) {
			printc("rebuild failed\n");
			BUG();
		}
	}
	mapping_del_children(m);

	/* printc("page %x 's children revoked...\n", addr); */
	UNLOCK();
	return ret;
}


/* might not be used in the future. A malloc manager should take care */
int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();

	if (cos_mmap_introspect(COS_MMAP_INTRO_FRAME, 0, spd, addr, 0) == -1) {
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
	/* kill local mappings */
	/* for (i = 0 ; i < COS_MAX_MEMORY ; i++) { */
	/* 	struct frame *f = &frames[i]; */
	/*      int idx; */

	/* 	if (frame_nrefs(f) >= 0) continue; */
	/* 	idx = cos_mmap_cntl(COS_MMAP_REVOKE, 0, cos_spd_id(), f->c.addr, 0); */
	/* 	assert(idx == frame_index(f)); */
	/* } */
	UNLOCK();
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
		mm_init(); break;
	default:
		BUG(); return;
	}

	return;
}
