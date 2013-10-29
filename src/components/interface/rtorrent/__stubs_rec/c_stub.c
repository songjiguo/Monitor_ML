/* Jiguo Song: ramfs FT */
/* Note: 
  1. Need deal 3 cases with cbufs
     1) fault while writing, after cbuf2buf -- need save the contents on clients (make it read only?)
     2) fault while writing, before cbuf2buf -- no need save the contents on clients (for now)
     3) writing successfully, fault later (for now)
*/

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <cos_map.h>
#include <cos_list.h>

#include <rtorrent.h>
#include <cstub.h>

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#define USE_CMAP

#if ! defined MAX_RECUR_LEVEL || MAX_RECUR_LEVEL < 1
#define MAX_RECUR_LEVEL 20
#endif

#define RD_PRINT 0

#if RD_PRINT == 1
#define print_rd(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_rd(fmt,...) 
#endif

static volatile unsigned long fcounter = 0;

/* recovery data and related utility functions */
struct rec_data_tor {
	td_t	parent_tid;	// id which split from, root tid is 1
        td_t	s_tid;		// id that returned from the server (might be same)
        td_t	c_tid;		// id that viewed by the client (always unique)

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;
	long		 evtid;

	unsigned long	 fcnt;
#if (!LAZY_RECOVERY)
	int has_rebuilt;
	struct rec_data_tor *next, *prev;
#endif

};

#if (!LAZY_RECOVERY)
struct rec_data_tor* eager_list_head = NULL;
#endif

/**********************************************/
/* slab allocator and cvect for tracking recovery data */
/**********************************************/
COS_MAP_CREATE_STATIC(uniq_tids);

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

void print_rd_info(struct rec_data_tor *rd);

static struct rec_data_tor *
rd_lookup(td_t td)
{ 
	return cvect_lookup(&rec_vect, td); 
}

static struct rec_data_tor *
rd_alloc(td_t c_tid)    // should pass c_tid
{
	struct rec_data_tor *rd;
	rd = cslab_alloc_rd();
	assert(rd);
	cvect_add(&rec_vect, rd, c_tid);
	return rd;
}

static void
rd_dealloc(struct rec_data_tor *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->c_tid)) BUG();
	cslab_free_rd(rd);
}


static struct rec_data_tor *
map_rd_lookup(td_t tid)
{ 
	return (struct rec_data_tor *)cos_map_lookup(&uniq_tids, tid);
}

static int
map_rd_create()
{
	struct rec_data_tor *rd = NULL;
	int map_id = 0;
	// ramfs return torrent id from 2 (rd_root is 1), and cos_map starts from 0
	// here want cos_map to return some ids at least from 2 and later
	while(1) {
		rd = cslab_alloc_rd();
		assert(rd);	
		map_id = cos_map_add(&uniq_tids, rd);
		/* printc("record added %d\n", map_id); */
		if (map_id >= 2) break;
		rd->s_tid = -1;  // -1 means that this is a dummy record
	}
	assert(map_id >= 2);
	return map_id;	
}

static void
map_rd_delete(td_t tid)
{
	assert(tid >= 0);
	struct rec_data_tor *rd;
	rd = map_rd_lookup(tid);
	assert(rd);
	cslab_free_rd(rd);
	cos_map_del(&uniq_tids, tid);
	return;
}

static void 
rd_cons(struct rec_data_tor *rd, td_t tid, td_t ser_tid, td_t cli_tid, char *param, int len, tor_flags_t tflags, long evtid)
{
	/* printc("rd_cons: ser_tid %d  cli_tid %d\n", ser_tid, cli_tid); */
	assert(rd);

	rd->parent_tid	 = tid;
	rd->s_tid	 = ser_tid;
	rd->c_tid	 = cli_tid;
	rd->param	 = param;
	rd->param_len	 = len;
	rd->tflags	 = tflags;
	rd->evtid	 = evtid;
	rd->fcnt	 = fcounter;

#if (!LAZY_RECOVERY)
	rd->has_rebuilt  = 0;
#endif
	return;
}

static td_t
rd_replay(struct rec_data_tor *rd, long recur_lvl)
{
	assert(rd);
	return __tsplit(cos_spd_id(), rd->parent_tid, rd->param, recur_lvl, rd->tflags, rd->evtid, (void *)rd);
}

/* restore the server state */
static void
reinstate(td_t tid, long recur_lvl)  	/* relate the flag to the split/r/w fcnt */
{
	td_t ret;
	struct rec_data_tor *rd;

	/* printc("in reinstate...tid %d\n", tid); */
	// tid could be lost, so use tid_root here FIXME
	if (tid == td_root) return;
#ifndef USE_CMAP	
	if (!(rd = rd_lookup(tid))) {
#else
	if (!(rd = map_rd_lookup(tid))) {
#endif
		/* printc("found root...tid %d\n", tid); */
		return; 	/* root_tid */
	}
	if (rd->fcnt == fcounter) return; // Has been rebuilt before. FIXME: should recover as a tree
	if (recur_lvl <= 0) assert(0);  /* too many recursion !!*/

	ret = rd_replay(rd, recur_lvl);
	if (ret < 1) {
		printc("re-split failed %d\n", tid);
		BUG();
	}
	rd->fcnt = fcounter;

	/* if (rd->parent_tid == rd_root) return; // has reached the root, do not recover the root for now */
	/* printc("<<<<<< thread %d trying to reinstate....tid %d\n", cos_get_thd_id(), rd->c_tid); */
	/* volatile unsigned long long start, end; */
	/* rdtscll(start); */
	/* printc("parent tid %d\n", rd->parent_tid); */
	/* printc("param reinstate %s of length %d\n", rd->param, rd->param_len); */
	/* ret = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid, rd->c_tid); */
	/* rdtscll(end); */
	/* printc("reinstate cost: %llu\n", end-start); */

#if (!LAZY_RECOVERY)
	rd->has_rebuilt  = 1;
#endif
	/* printc("just reinstate c_tid %d s_tid %d (rd->fcnt %d) >>>>>>>>>>\n", rd->c_tid, rd->s_tid, rd->fcnt); */
	return;
}

extern void sched_active_uc(int thd_id);

static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;
	volatile unsigned long long start, end;
	rdtscll(start);
#ifndef USE_CMAP
        rd = rd_lookup(tid);
#else
        rd = map_rd_lookup(tid);
#endif
	if (!rd) return NULL;

#if (!LAZY_RECOVERY)
	return rd;
#endif
	/* fast path */
	if (likely(rd->fcnt == fcounter)) return rd;

	/* printc("rd->fcnt %lu fcounter %lu\n",rd->fcnt,fcounter); */
	reinstate(tid, MAX_RECUR_LEVEL);
	/* printc("rebuild fs is done\n\n"); */
	rdtscll(end);
	printc("recovery_rd cost: %llu\n", end-start);

	return rd;
}

/* // This is not used anymore, has changed to cos_map */
/* static int */
/* get_unique(void) */
/* { */
/* 	unsigned int i; */
/* 	cvect_t *v; */

/* 	v = &rec_vect; */

/* 	/\* 1 is already assigned to the td_root *\/ */
/* 	for(i = 2 ; i < CVECT_MAX_ID ; i++) { */
/* 		if (!cvect_lookup(v, i)) return i; */
/* 	} */
	
/* 	if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID; */

/* 	return -1; */
/* } */

static char*
param_save(char *param, int param_len)
{
	char *l_param;
	
	assert(param && param_len > 0);

	l_param = malloc(param_len);
	if (!l_param) {
		printc("cannot malloc \n");
		BUG();
		return NULL;
	}
	strncpy(l_param, param, param_len);

	return l_param;
}


#if (!LAZY_RECOVERY)
static struct rec_data_tor *
eager_lookup(td_t td)
{
	struct rec_data_tor *rd = NULL;

	if (!eager_list_head || EMPTY_LIST(eager_list_head, next, prev)) return NULL;
	
	for (rd = FIRST_LIST(eager_list_head, next, prev);
	     rd != eager_list_head;
	     rd = FIRST_LIST(rd, next, prev)){
		if (td == rd->c_tid) return rd;
	}
	return rd;
}

void
eager_recovery_all()
{
	td_t ret;
	struct rec_data_tor *rd = NULL;

	if (!eager_list_head || EMPTY_LIST(eager_list_head, next, prev)) return;
	/* printc("\n [start eager recovery!]\n\n"); */

	for (rd = FIRST_LIST(eager_list_head, next, prev);
	     rd != eager_list_head;
	     rd = FIRST_LIST(rd, next, prev)){
		if (!rd->has_rebuilt) {
			/* printc("found one c %d s %d\n", rd->c_tid, rd->s_tid); */
			reinstate(rd->c_tid, MAX_RECUR_LEVEL);
		}
	}
	
	
	/* un_mark the has_rebuilt flag */
	for (rd = FIRST_LIST(eager_list_head, next, prev);
	     rd != eager_list_head;
	     rd = FIRST_LIST(rd, next, prev)){
		rd->has_rebuilt = 0;
	}

	/* printc("\n [eager recovery done!]\n\n"); */
}
#endif


/************************************/
/******  client stub functions ******/
/************************************/

struct __sg_tsplit_data {
	td_t tid;	
	/* int flag; */
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

/* Split the new torrent and get new ser object */
/* Client only needs know how to find server side object */

static int aaa = 0;

CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid)

/* printc("\ncli interface... param... %s\n", param); */
/* printc("<<< In: call recovery tsplit  (thread %d spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id()); */
        if (cos_get_thd_id() == 13) aaa++;   		/* test only */
/* printc("thread %d aaa is %d\n", cos_get_thd_id(), aaa); */
        ret = __tsplit(spdid, tid, param, len, tflags, evtid, NULL);

CSTUB_POST

static int first = 0;

CSTUB_FN_ARGS_7(td_t, __tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid, void *, rec_rd)
        struct __sg_tsplit_data *d = NULL;
        struct rec_data_tor *rd, *rd_p, *rd_c, *rd_rec;
        rd = rd_p = rd_c = rd_rec = NULL;
	char *l_param = NULL;
	cbuf_t cb = 0;
        long recur_lvl = 0;
        long l_evtid = 0;

        int		sz	   = 0;
        td_t		cli_tid	   = 0;
        td_t		ser_tid	   = 0;
        tor_flags_t	flags	   = tflags;
        td_t		parent_tid = tid;

        if (first == 0) {
		cos_map_init_static(&uniq_tids);
		first = 1;
	}

        /* printc("len %d param %s\n", len, param); */
	unsigned long long start, end;
        assert(param && len >= 0);
        assert(param[len] == '\0'); 

        sz = len + sizeof(struct __sg_tsplit_data);
        /* replay on slow path */
        // option1: pre allocate memory (limit amount mem, stack space!!! -- cflow)
        // option2: no memory, but N^2 in finding all parents 
        // For now, use MAX_RECUR_LEVEL to limit the max recursion depth
        if (unlikely(rec_rd)) {
		/* on rec path, evtid (already passed by rd) is passed as the recursion depth*/
		recur_lvl = evtid;
		rd_rec =  (struct rec_data_tor *) rec_rd;
		l_evtid = rd_rec->evtid;
		reinstate(parent_tid, recur_lvl-1);
	} else {
		l_evtid = evtid;
	}

#ifndef USE_CMAP
        if ((rd_p = rd_lookup(parent_tid))) parent_tid = rd_p->s_tid;
#else
        if ((rd_p = map_rd_lookup(parent_tid)) && rd_p->s_tid > 1 ) {
		parent_tid = rd_p->s_tid;
		/* printc("update parent_tid %d\n", parent_tid); */
	}
#endif
        if ((parent_tid > 1) && unlikely(!rd_p)) assert(0);  //if parent_tid > 1, there must a record exist already
redo:
        d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        d->tid	       = parent_tid;
        d->tflags      = flags;
	d->evtid       = l_evtid;
	d->len[0]      = 0;
	d->len[1]      = len;
        /* printc("c: subpath name %s len %d\n", param, len); */
	memcpy(&d->data[0], param, len);

#ifdef TEST_3
        if (aaa == 6 && cos_spd_id() == 17) { //for TEST_3 only   
#else
        if (aaa == 6) {
#endif
		/* d->flag = -10; /\* test purpose only *\/ */
		aaa = 100;
		/* rdtscll(start); */
	}

CSTUB_ASM_4(__tsplit, spdid, cb, sz, rec_rd)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		memset(&d->data[0], 0, len);
		cbuf_free(d);
		
#if (!LAZY_RECOVERY)
		eager_recovery_all();
#else
		reinstate(tid, MAX_RECUR_LEVEL);
#endif
		
		/* printc("rebuild is done, , maybe active uc 12\n\n"); */

		/* JIGUO: I think we'd better rebuild the web server status,
		 * since no coming request will trigger the even since
		 * the recovery */
		/* sched_active_uc(12); */
		/* cos_brand_cntl(COS_BRAND_ACTIVATE_UC, 13, 12, 0); */

		/* rdtscll(end); */
		/* printc("entire cost %llu\n", end-start); */
		goto redo;
	}

//memset(&d->data[0], 0, len);  no reason to zero??
        /* printc("tsplit ready to cbuf free\n"); */
        cbuf_free(d);

        if (unlikely(rec_rd)) {
		assert(ret > 0);
		rd_rec->s_tid = ret;  	/* update server side tid */
		return ret;
	}

        ser_tid = ret;
	l_param = NULL;
	if (len > 0) { // len can be zero
		l_param = param_save(param, len);
		assert(l_param);
	}
#ifndef USE_CMAP
	// cmap_get() here. Maybe unlikely needs to be removed
        if (unlikely(rd_lookup(ser_tid))) {
		cli_tid = get_unique();
		assert(cli_tid > 0 && cli_tid != ser_tid);
		/* printc("found existing tid %d >> get new cli_tid %d\n", ser_tid, cli_tid); */
#if (!LAZY_RECOVERY)
		struct rec_data_tor *tmp;
		tmp  = rd_lookup(ser_tid);
		REM_LIST(tmp, next, prev);
#endif
	} else {
		cli_tid = ser_tid;
	}
	
        /* client side tid should be guaranteed to be unique now */
        rd = rd_alloc(cli_tid);   // pass unique client side id here
#else
	cli_tid = map_rd_create();
	rd = map_rd_lookup(cli_tid);
#endif
        assert(rd);
        rd_cons(rd, tid, ser_tid, cli_tid, l_param, len, tflags, l_evtid);
	/* printc("After create rd: rd->c_tid %d rd->s_tid %d rd->parent_tid %d\n",rd->c_tid, rd->s_tid, rd->parent_tid); */

#if (!LAZY_RECOVERY)
	/* only initialize the eager head */
	if (unlikely(!eager_list_head)) {
		eager_list_head = (struct rec_data_tor*)malloc(sizeof(struct rec_data_tor));
		INIT_LIST(eager_list_head, next, prev);
		rd_cons(eager_list_head, 0, 0, 0, NULL, 0, 0, 0);
	}
	ADD_LIST(eager_list_head, rd, next, prev);
#endif
        ret = cli_tid;
	/* printc("tsplit done!!!\n\n"); */
CSTUB_POST

/* struct __sg_twmeta_data { */
/* 	td_t td; */
/* 	cbuf_t cb; */
/* 	int sz; */
/* 	int offset; */
/* 	int flag; */
/* }; */

/* CSTUB_FN_ARGS_6(int, twmeta, spdid_t, spdid, td_t, td, cbuf_t, cb, int, sz, int, offset, int, flag) */
/*         struct rec_data_tor *rd; */
/* 	struct __sg_twmeta_data *d; */
/* 	cbuf_t cb_m; */
/* 	int sz_m = sizeof(struct __sg_twmeta_data); */
/* redo: */
/* 	d = cbuf_alloc(sz_m, &cb_m); */
/* 	if (!d) return -1; */

/* 	/\* int buf_sz; *\/ */
/* 	/\* u32_t id; *\/ */
/* 	/\* cbuf_unpack(cb, &id, (u32_t*)&buf_sz); *\/ */
/*         /\* printc("cbid is %d\n", id); *\/ */

/* 	d->td	  = td; */
/* 	d->cb	  = cb; */
/*         d->sz	  = sz; */
/*         d->offset = offset; */
/*         d->flag   = flag; */

/* CSTUB_ASM_3(twmeta, spdid, cb_m, sz_m) */
/*         if (unlikely(fault)) { */
/* 		fcounter++; */
/* 		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) { */
/* 			printc("set cap_fault_cnt failed\n"); */
/* 			BUG(); */
/* 		} */
/* 		cbuf_free(d); */
/*                 goto redo; */
/* 	} */

/* 	cbuf_free(d); */
/* CSTUB_POST */

struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};
CSTUB_FN_ARGS_5(int, tmerge, spdid_t, spdid, td_t, td, td_t, td_into, char *, param, int, len)
	struct __sg_tmerge_data *d;
        struct rec_data_tor *rd;
	cbuf_t cb;
	int sz = len + sizeof(struct __sg_tmerge_data);

        /* printc("<<< In: call tmerge (thread %d) >>>\n", cos_get_thd_id()); */

        assert(param && len > 0);
	assert(param[len] == '\0');

redo:
/* printc("tmerge\n"); */
        rd = update_rd(td);
	if (!rd) {
		printc("try to merge a non-existing tor\n");
		return -1;
	}

	d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        /* printc("c: tmerge td %d (server td %d) len %d param %s\n", td, rd->s_tid, len, param);	 */
	d->td = rd->s_tid;   	/* actual server side torrent id */
	d->td_into = td_into;
        d->len[0] = 0;
        d->len[1] = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(tmerge, spdid, cb, sz)
        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		cbuf_free(d);
                goto redo;
	}

	cbuf_free(d);
#ifndef USE_CMAP
        if (!ret) map_rd_delete(rd->c_tid); /* this must be a leaf */
#else
        if (!ret) rd_dealloc(rd); /* this must be a leaf */
#endif

#if (!LAZY_RECOVERY)
	REM_LIST(rd, next, prev);
#endif

CSTUB_POST


CSTUB_FN_ARGS_2(int, trelease, spdid_t, spdid, td_t, tid)

        /* printc("<<< In: call trelease (thread %d) >>>\n", cos_get_thd_id()); */
        struct rec_data_tor *rd;

redo:
/* printc("trelease\n"); */
        rd = update_rd(tid);
	if (!rd) {
		printc("try to release a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_2(trelease, spdid, rd->s_tid)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}
        assert(rd);

#if (!LAZY_RECOVERY)
	REM_LIST(rd, next, prev);
#endif

CSTUB_POST

CSTUB_FN_ARGS_4(int, tread, spdid_t, spdid, td_t, tid, cbufp_t, cb, int, sz)

        /* printc("<<< In: call tread (thread %d, spd %ld) >>>\n", cos_get_thd_id(), cos_spd_id()); */
        struct rec_data_tor *rd;
        volatile unsigned long long start, end;

	int flag_fail = 0;
redo:
	/* if (flag_fail == 1){ */
	/* 	rdtscll(start); */
	/* } */
/* printc("tread\n"); */
        rd = update_rd(tid);

	/* if (flag_fail == 1){ */
	/* 	rdtscll(end); */
	/* 	printc("tread recovery -end cost %llu\n", end - start); */
	/* } */

	flag_fail = 0;

	if (!rd) {
		printc("try to read a non-existing tor\n");
		return -1;
	}


CSTUB_ASM_4(tread, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		/* printc("failed!!! in tread\n"); */
		flag_fail = 1;
                goto redo;
	}

        /* flag_fail = 0; */
        print_rd_info(rd);

CSTUB_POST


CSTUB_FN_ARGS_4(int, twrite, spdid_t, spdid, td_t, tid, cbufp_t, cb, int, sz)

        /* printc("<<< In: call twrite  (thread %d) >>>\n", cos_get_thd_id()); */
        struct rec_data_tor *rd;

redo:

/* printc("twrite\n"); */
        rd = update_rd(tid);
	if (!rd) {
		printc("try to write a non-existing tor\n");
		return -1;
	}

CSTUB_ASM_4(twrite, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
                goto redo;
	}

        assert(rd);

        print_rd_info(rd);

CSTUB_POST

void 
print_rd_info(struct rec_data_tor *rd)
{
	assert(rd);
	print_rd("rd->parent_tid %d  ",rd->parent_tid);
	print_rd("rd->s_tid %d  ",rd->s_tid);
	print_rd("rd->c_tid %d  ",rd->c_tid);

	print_rd("rd->param %s  ",rd->param);
	print_rd("rd->pram_len %d  ",rd->param_len);
	print_rd("rd->tflags %d  ",rd->tflags);
	print_rd("rd->evtid %ld  ",rd->evtid);

	print_rd("rd->fcnt %ld  ",rd->fcnt);
	print_rd("fcounter %ld  ",fcounter);

	print_rd("rd->offset %d \n ",rd->offset);

	return;
}

