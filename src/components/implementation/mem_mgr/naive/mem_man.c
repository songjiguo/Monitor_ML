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
#define LOCK() if (cos_sched_lock_take()) assert(0);
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

/* mappings */
struct mapping {
	u16_t   flags;
	spdid_t spdid;
	vaddr_t addr;

	struct frame *f;
	/* child and sibling mappings */
	struct mapping *p, *c, *_s, *s_;
} __attribute__((packed));

static struct mapping *mapping_crt(struct mapping *p, struct frame *f, spdid_t dest, vaddr_t to, int root_page);
static struct mapping *mapping_lookup(spdid_t spdid, vaddr_t addr);


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


static inline struct frame *
frame_alloc(void)
{
	vaddr_t root_addr;
	spdid_t root_spd;

	struct frame *f = freelist;
	struct mapping *m = NULL;

	if (!f) return NULL;

again:
	freelist = f->c.free;
	f->nmaps = 0;
	f->c.m   = NULL;

	root_addr = (vaddr_t)cos_mmap_introspect(COS_MMAP_INTRO_RTADDR, 
						 0, 0, 0, frame_index(f));
	/* a crash happened before */
	if (unlikely(root_addr)) {
		root_spd = (spdid_t)cos_mmap_introspect(COS_MMAP_INTRO_RTSPD, 
							0, 0, 0, frame_index(f));
		assert(!root_spd);
		
		frame_ref(f);

		m = mapping_crt(NULL, f, root_spd, root_addr, 1);
		if (!m) {
			printc("can not create mapping\n");
			goto err;
		}
		f->c.m = m;
		
		assert(m->addr == root_addr);
		assert(m->spdid == root_spd);
		assert(m == mapping_lookup(root_spd, root_addr));
		f = freelist;

		goto again;
	}

done:
	return f;
err:
	f = NULL;
	goto done;
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
	void *hp = cos_get_vas_page();
	struct frame *f = frame_alloc();

	assert(hp && f);
	frame_ref(f);
	f->nmaps  = -1; 	 /* belongs to us... */
	f->c.addr = (vaddr_t)hp; /* ...at this address */
	if (cos_mmap_cntl(COS_MMAP_GRANT, 0, cos_spd_id(), (vaddr_t)hp, frame_index(f))) {
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
	struct comp_vas *cv;

	assert(!cvas_lookup(spdid));
	cv = cslab_alloc_cvas();
	if (!cv) goto done;
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


static struct mapping *
restore_mapping(spdid_t spd, vaddr_t addr)
{
	/* if mmaping does not exist, create with a 'NULL previous' node */
	/* this could result many independent trees */

	struct mapping *m = NULL;
	int fr_idx = 0;

	fr_idx = (int)cos_mmap_introspect(COS_MMAP_INTRO_FRAME, 
					  0, spd, addr, 0);

	if (fr_idx == ENOFRAM) {
		printc("can not find frame\n");
		goto done;
	}
	
	m = mapping_crt(NULL, &frames[fr_idx], spd, addr, 0);
	if (!m) goto err;

	assert(fr_idx == frame_index(m->f));
	assert(m->p == NULL);
done:
	return m;
err:
	m = NULL;
	goto done;
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

	cvas_ref(cv);
	m = cslab_alloc_mapping();
	if (!m) goto collision;

	if (root_page) mm_flag = COS_MMAP_SET_ROOT;
	ret = cos_mmap_cntl(COS_MMAP_GRANT, mm_flag, dest, to, frame_index(f));
	if (ret == EGETPHY) {
		printc("Error: can not find physical page\n");
		goto no_mapping;
	}
	/* no maping, but page in page table already */
	if (ret == EADDPTE) {
		printc("Error: can not add page entry into page table\n");
		printc("A mapping should be created anyway\n");
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

vaddr_t mman_get_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct frame *f;
	struct mapping *m = NULL;
	vaddr_t ret = -1;

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
done:
	UNLOCK();
	return ret;
dealloc:
	frame_deref(f);
	goto done;		/* -EINVAL */
}

vaddr_t mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	struct mapping *m, *n;
	vaddr_t ret = 0;

	LOCK();
	
	m = mapping_lookup(s_spd, s_addr);
	/* Now m will be recreated in mapping_crt */
	if (unlikely(!m)) m = restore_mapping(s_spd, s_addr);

	assert(m == mapping_lookup(s_spd,s_addr));

	n = mapping_crt(m, m->f, d_spd, d_addr, 0);
	if (!n) goto done;
	
	assert(n->addr  == d_addr);
	assert(n->spdid == d_spd);
	assert(n->p == m || n->p == NULL); 

	ret = d_addr;
done:
	UNLOCK();
	return ret;
}


static struct mapping *
rebuild_mapping_tree(spdid_t spd, vaddr_t addr, int flags)
{
	int fr_idx;
	vaddr_t root_addr;
	spdid_t root_spd;

	struct mapping *m = NULL;

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

int mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();
	m = mapping_lookup(spd, addr);
	/* a crash happened before */
	if (unlikely(!m)) {
		m = rebuild_mapping_tree(spd, addr, flags);
		if (!m) {
			printc("rebuild failed\n");
			BUG();
		}
	}
	mapping_del_children(m);

	UNLOCK();
	return ret;
}


/* might not be used in the future. A malloc manager should take care */
int mman_release_page(spdid_t spd, vaddr_t addr, int flags)
{
	struct mapping *m;
	int ret = 0;

	LOCK();

	if (cos_mmap_cntl(COS_MMAP_INTRO_FRAME, 0, spd, addr, 0) == -1) {
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

#include <errno.h>
#include <sched.h>

int sched_root_init(void) { mm_init(); return 0; }
void sched_exit(void)     { mman_release_all(); }

int sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff)
{
	BUG();
	return 0;
}

int sched_child_cntl_thd(spdid_t spdid)
{
	BUG();
	return 0;
}

int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd)
{
	BUG();
	return 0;
}

int sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	BUG();
	return -ENOTSUP;
}

int sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	BUG();
	return -ENOTSUP;
}

void sched_timeout(spdid_t spdid, unsigned long amnt) { BUG(); return; }

int sched_priority(unsigned short int tid) { BUG(); return 0; }

int sched_timeout_thd(spdid_t spdid)
{
	BUG();
	return -ENOTSUP;
}

unsigned int sched_tick_freq(void)
{
	BUG();
	return 0;
}

unsigned long sched_cyc_per_tick(void)
{
	BUG();
	return 0;
}

unsigned long sched_timestamp(void)
{
	BUG();
	return 0;
}

unsigned long sched_timer_stopclock(void)
{
	BUG();
	return 0;
}

int sched_create_thread(spdid_t spdid, struct cos_array *data)
{
	BUG();
	return -ENOTSUP;
}

int sched_create_thd(spdid_t spdid, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2)
{
	BUG();
	return -ENOTSUP;
}

int
sched_create_thread_default(spdid_t spdid, u32_t sched_param_0, 
			    u32_t sched_param_1, u32_t sched_param_2)
{
	BUG();
	return -ENOTSUP;
}

int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs)
{
	BUG();
	return -ENOTSUP;
}

int sched_create_net_brand(spdid_t spdid, unsigned short int port)
{
	BUG();
	return -ENOTSUP;
}

int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid)
{
	BUG();
	return -ENOTSUP;
}

int sched_component_take(spdid_t spdid) 
{ 
	BUG(); 
	return -ENOTSUP;
}

int sched_component_release(spdid_t spdid)
{
	BUG();
	return -ENOTSUP;
}

