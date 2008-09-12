/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <cos_list.h>

#define NUM_THDS MAX_NUM_THREADS
#define BLOCKED 0x8000000
#define GET_CNT(x) (x & (~BLOCKED))
#define IS_BLOCKED(x) (x & BLOCKED)


/* 
 * We need page-aligned data for the network ring buffers.  This
 * structure lies at the beginning of a page and describes the data in
 * it.  When amnt_buffs = 0, we can dealloc the page.
 */
#define NP_NUM_BUFFS 2
#define MTU 1500
#define BUFF_ALIGN_VALUE 8
#define BUFF_ALIGN(sz)  ((sz + BUFF_ALIGN_VALUE) & ~(BUFF_ALIGN_VALUE-1))
struct buff_page {
	int amnt_buffs;
	struct buff_page *next, *prev;
	void *buffs[NP_NUM_BUFFS];
	short int buff_len[NP_NUM_BUFFS];
	/* FIXME: should be a bit-field */
	char buff_used[NP_NUM_BUFFS];
};

/* Meta-data for the circular queues */
typedef struct {
	int rb_head, rb_tail, curr_buffs, max_buffs;
	ring_buff_t *rb;
	struct buff_page used_pages, avail_pages;
} rb_meta_t;
static rb_meta_t rb1_md, rb2_md, rb3_md;
static ring_buff_t rb1, rb2, rb3;

struct thd_map {
	unsigned short int thd, upcall, port;
	rb_meta_t *uc_rb;
} tmap[NUM_THDS];

static struct thd_map *get_thd_map_port(unsigned short port) 
{
	int i;
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].port == port) {
			return &tmap[i];
		}
	}
	
	return NULL;
}

static struct thd_map *get_thd_map(unsigned short int thd_id)
{
	int i;
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd    == thd_id ||
		    tmap[i].upcall == thd_id) {
			return &tmap[i];
		}
	}
	
	return NULL;
}

static int add_thd_map(unsigned short int ucid, unsigned short int port, rb_meta_t *rbm)
{
	int i;
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd == 0) {
			tmap[i].thd    = cos_get_thd_id();
			tmap[i].upcall = ucid;
			tmap[i].port   = port;
			tmap[i].uc_rb  = rbm;
			break;
		}
	}
	assert(i != NUM_THDS);

	return 0;
}

static int rem_thd_map(unsigned short int tid)
{
	struct thd_map *tm;

	tm = get_thd_map(tid);
	if (!tm) return -1;
	tm->thd = 0;
	return 0;
}

static void rb_init(rb_meta_t *rbm, ring_buff_t *rb)
{
	int i;

	for (i = 0 ; i < RB_SIZE ; i++) {
		rb->packets[i].status = RB_EMPTY;
	}
	rbm->rb_head = 0;
	rbm->rb_tail = RB_SIZE-1;
	rbm->rb = rb;
	INIT_LIST(&rbm->used_pages, next, prev);
	INIT_LIST(&rbm->avail_pages, next, prev);
}

static int rb_add_buff(rb_meta_t *r, void *buf, int len)
{
	ring_buff_t *rb = r->rb;
	unsigned int head = r->rb_head;
	struct rb_buff_t *rbb;

	assert(rb);
	assert(head < RB_SIZE);
	rbb = &rb->packets[head];

	if (head == r->rb_tail) {
		return -1;
	}
	assert(rbb->status == RB_EMPTY);
	rbb->ptr = buf;
	rbb->len = len;
	/* 
	 * The status must be set last.  It is the manner of
	 * communication between the kernel and this component
	 * regarding which cells contain valid pointers. 
	 *
	 * FIXME: There should be a memory barrier here, but I'll
	 * cross my fingers...
	 */
	rbb->status = RB_READY;
	r->rb_head = (r->rb_head + 1) & (RB_SIZE-1);
	return 0;
}

/* 
 * -1 : there is no available buffer
 * 1  : the kernel found an error with this buffer, still set address
 *      and len.  Either the address was not mapped into this component, 
 *      or the memory region did not fit into a page.
 * 0  : successful, address contains data
 */
static int rb_retrieve_buff(rb_meta_t *r, void **buf, int *len)
{
	ring_buff_t *rb = r->rb;
	unsigned int tail;
	struct rb_buff_t *rbb;
	unsigned short int status;

	assert(rb);
	tail = (r->rb_tail + 1) & (RB_SIZE-1);
	assert(tail < RB_SIZE);
	rbb = &rb->packets[tail];

	if (r->rb_tail == r->rb_head) {
		return -1;
	}
	status = rbb->status;
	if (status != RB_USED && status != RB_ERR) {
		return -1;
	}
	
	*buf = rbb->ptr;
	*len = rbb->len;
	/* Again: the status must be set last.  See comment in rb_add_buff. */
	rbb->status = RB_EMPTY;
	r->rb_tail = tail;

	if (status == RB_ERR) return 1;
	return 0;
}

static struct buff_page *alloc_buff_page(void)
{
	struct buff_page *page = alloc_page();
	int i;
	int buff_offset = BUFF_ALIGN(sizeof(struct buff_page));

	if (!page) {
		return NULL;
	}
	page->amnt_buffs = 0;
	INIT_LIST(page, next, prev);
	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		char *bs = (char *)page;

		page->buffs[i] = bs + buff_offset;
		page->buff_used[i] = 0;
		page->buff_len[i] = MTU;
		buff_offset += MTU;
	}
	return page;
}

static void *alloc_rb_buff(rb_meta_t *r)
{
	struct buff_page *p;
	int i;
	void *ret = NULL;

	if (EMPTY_LIST(&r->avail_pages, next, prev)) {
		if (NULL == (p = alloc_buff_page())) {
			return NULL;
		}
		ADD_LIST(&r->avail_pages, p, next, prev);
	}
	p = FIRST_LIST(&r->avail_pages, next, prev);
	assert(p->amnt_buffs < NP_NUM_BUFFS);
	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		if (p->buff_used[i] == 0) {
			p->buff_used[i] = 1;
			ret = p->buffs[i];
			p->amnt_buffs++;
			break;
		}
	}
	assert(NULL != ret);
	if (p->amnt_buffs == NP_NUM_BUFFS) {
		REM_LIST(p, next, prev);
		ADD_LIST(&r->used_pages, p, next, prev);
	}

	return r;
}

static void release_rb_buff(rb_meta_t *r, void *b)
{
	struct buff_page *p;
	int i;

	p = (struct buff_page *)(((unsigned long)b) & ~(4096-1));

	for (i = 0 ; i < NP_NUM_BUFFS ; i++) {
		if (p->buffs[i] == b) {
			p->buff_used[i] = 0;
			p->amnt_buffs--;
			REM_LIST(p, next, prev);
			ADD_LIST(&r->avail_pages, p, next, prev);
			return;
		}
	}
	/* b must be malformed such that p (the page descriptor) is
	 * not at the start of its page */
	assert(0);
}

extern int sched_create_net_upcall(unsigned short int port, int depth);
extern int sched_block(void);
extern int sched_wakeup(unsigned short int thd_id);

void cos_net_interrupt(void)
{
	unsigned short int ucid = cos_get_thd_id();
	void *buff;
	int len;
	struct thd_map *tm;

	tm = get_thd_map(ucid);
	assert(tm);
	assert(!rb_retrieve_buff(tm->uc_rb, &buff, &len));
	assert(!rb_add_buff(tm->uc_rb, buff, MTU));
		
	return;
}

static unsigned short int cos_net_create_net_upcall(unsigned short int port, rb_meta_t *rbm)
{
	unsigned short int ucid;
	
	ucid = sched_create_net_upcall(port, 1);
	add_thd_map(ucid, port, rbm);
	assert(!cos_buff_mgmt(rb1.packets, sizeof(rb1.packets), ucid, COS_BM_RECV_RING));
	return ucid;
}

int init(void) 
{
	unsigned short int ucid;
	void *b;
	
	rb_init(&rb1_md, &rb1);
	rb_init(&rb2_md, &rb2);
	rb_init(&rb3_md, &rb3);

	/* Wildcard upcall */
	ucid = cos_net_create_net_upcall(0, &rb1_md);

	assert((b = alloc_rb_buff(&rb1_md)));
	assert(!rb_add_buff(&rb1_md, b, MTU));
	assert((b = alloc_rb_buff(&rb1_md)));
	assert(!rb_add_buff(&rb1_md, b, MTU));

	sched_block();
	
	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		cos_net_interrupt();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
	{
		static int first = 1;

		if (first) {
			init();
			first = 0;
		}
		break;
	}
	default:
		print("Unknown type of upcall %d made to net. %d%d", t, 0,0);
		assert(0);
		return;
	}

	return;
}