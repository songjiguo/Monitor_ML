/* Jiguo Song: ramfs FT */
/* Note: 
  1. Need deal 3 cases with cbufs
     1) fault while writing, after cbuf2buf -- need save the contents on clients (make it read only?)
     2) fault while writing, before cbuf2buf -- no need save the contents on clients (for now)
     3) writing successfully, fault later (for now)
  2. One decision: once c_tid is created, keep it and associate with that file. only update
  3.
  4.
  5.
*/
/* 
   Situations: cbufs are owned/mapped by the spd
   that crashed.
 */
/*                     onwer               mapped        */
/*           a)      ser(crashed)           cli          */
/*           b)      ser(crashed)           N/A          */
/*           c)      ser(crashed)           other        */
/*           d)      cli                    ser(crashed)  -- claim */
/*           e)      cli                    N/A          */
/*           f)      cli                    other        */


#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <torrent.h>
#include <cstub.h>

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>


#define RD_PRINT 0

#if RD_PRINT == 1
#define print_rd(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define print_rd(fmt,...) 
#endif

static unsigned long fcounter = 0;

/* used for multiple cbufs tracking when twrite  */
struct w_cbuf {
	cbuf_t		 cb;
	int		 sz;
	u32_t		 actual_sz;	/* related to the offset */
	td_t		 owner_tid;
	struct w_cbuf	*next, *prev;	/* the list within the same tor_id */
	struct w_cbuf	*right, *left;	/* the list across different toe_id, only used by head node */
};

struct w_cbuf *all_wcbufs_list;

/* recovery data and related utility functions */
struct rec_data_tor {
	td_t	parent_tid;	// id which split from, root tid is 1
        td_t	s_tid;		// id that returned from the server (might be same)
        td_t	c_tid;		// id that viewed by the client (always unique)

	char		*param;
	int		 param_len;
	tor_flags_t	 tflags;
	long		 evtid;

	unsigned long	fcnt;
	
	struct w_cbuf	*wcbufs_head;
};

/**********************************************/
/* slab allocator and cvect for tracking recovery data */
/**********************************************/

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

CSLAB_CREATE(wcb, sizeof(struct w_cbuf));

void print_rd_info(struct rec_data_tor *rd);

static struct rec_data_tor *
rd_lookup(td_t td)
{ return cvect_lookup(&rec_vect, td); }

static struct rec_data_tor *
rd_alloc(void)
{
	struct rec_data_tor *rd;
	rd = cslab_alloc_rd();
	return rd;
}

static void
rd_dealloc(struct rec_data_tor *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->c_tid)) BUG();
	cslab_free_rd(rd);
}


void 
rd_init(struct rec_data_tor *rd)
{
	struct w_cbuf *wcb = NULL;

	rd->parent_tid = 0;
	rd->s_tid      = 0;
	rd->c_tid      = 0;
	rd->param      = 0;
	rd->param_len  = 0;
	rd->tflags     = 0;
	rd->evtid      = 0;
	rd->fcnt       = 0;

	/* initialize the list head for writing records, only do this once  */
	wcb = cslab_alloc_wcb();
	if (!wcb) {
		printc("failed to allocate the wcbuf\n");
		BUG();
	}

	INIT_LIST(wcb, next, prev);
	rd->wcbufs_head = wcb;

	INIT_LIST(wcb, right, left);
	if (!all_wcbufs_list) all_wcbufs_list = wcb;
	else ADD_LIST(all_wcbufs_list, wcb, right, left);

	return;
}

void 
rd_cons(struct rec_data_tor *rd, td_t tid, td_t ser_tid, td_t cli_tid, char *param, int len, tor_flags_t tflags, long evtid)
{
	printc("rd_cons: ser_tid %d  cli_tid %d\n", ser_tid, cli_tid);
	assert(rd);

	rd->parent_tid = tid;
	rd->s_tid      = ser_tid;
	rd->c_tid      = cli_tid;
	rd->param      = param;
	rd->param_len  = len;
	rd->tflags     = tflags;
	rd->evtid      = evtid;
	rd->fcnt       = fcounter;

	rd->wcbufs_head->owner_tid = cli_tid;

	cvect_add(&rec_vect, rd, cli_tid);

	return;
}

static void
add_cbufs_record(struct rec_data_tor *rd, cbuf_t cb, int sz, unsigned len)
{

	/* int szs; */
	/* u32_t id; */
	/* cbuf_unpack(cb, &id, (u32_t*)&szs); */
	/* printc("cb %u of size %d is written, actual len %d\n", cb, sz, len); */

	struct w_cbuf *wcb;

	wcb = cslab_alloc_wcb();
	if (!wcb) {
		printc("failed to allocate the wcbuf\n");
		BUG();
	}

	INIT_LIST(wcb, next, prev);

	wcb->cb	       = cb;
	wcb->sz	       = sz;
	wcb->actual_sz = len;	
	wcb->owner_tid = rd->c_tid;

	ADD_LIST(rd->wcbufs_head, wcb, next, prev);

	printc("rd->c_tid %d\n", rd->c_tid);
	struct w_cbuf *test = NULL, *list;
	list = rd->wcbufs_head;
	for (test  = FIRST_LIST(list, next, prev);
	     test != list;
	     test  = FIRST_LIST(test, next, prev)) {
		printc("write->cb %d\n", test->cb);
		printc("write->sz %d\n", test->sz);
		printc("write->actual_sz %d\n", test->actual_sz);
		printc("write->owner_tid %d\n", test->owner_tid);
	}

	return;
}


int
update_cbufs(spdid_t spdid)   
{
	int cbid, iter;

	iter = cbuf_c_introspect(spdid, -1);
	/* printc("restore all cbufs %d\n", iter); */
	if(likely(!iter)) return 0;

	while (iter > 0){
		cbid = cbuf_c_introspect(spdid, iter--);
		printc("restore cbuf %d\n", cbid);
		if (cbuf_vect_expand(&meta_cbuf, cbid) < 0) goto err;
		cbuf_c_claim(spdid, cbid);
	}
	return 0;
err:
	return -1;
}

/* repopulate all the cbufs in order to rebuild the fs object */

/* introspect the RO cbuf held by ramfs, this is done on server side interface */
/* get the offset position from the wcb->offset in order  */
/* copy the cbuf to the position, meta write */

static int
rebuild_fs_obj(struct w_cbuf *wcb)
{
	int ret = 0;

	/* ret = twmeta(wcb->owner_tid, wcb->cb, wcb->sz, wcb->actual_sz); */
	

	return ret;
}


static void
reindata()
{
	int ret = 0;
	cbuf_t cb;
	int sz;
	int offset;
	struct w_cbuf *h_wcb = NULL, *h_list;	/* v for vertical list for all wcbufs in the same t_id */
	struct w_cbuf *v_wcb = NULL, *v_list;	/* v for vertical list for all wcbufs in the same t_id */

	h_list = all_wcbufs_list;
	for (h_wcb  = FIRST_LIST(h_list, right, left);
	     h_wcb != h_list;
	     h_wcb  = FIRST_LIST(h_wcb, right, left)) {
		printc("found h_wcb->owner_tid %d\n", h_wcb->owner_tid);
		v_list = h_wcb;
		for (v_wcb  = FIRST_LIST(v_list, next, prev);
		     v_wcb != v_list;
		     v_wcb  = FIRST_LIST(v_wcb, next, prev)) {
			printc("found v_wcb->owner_tid %d\n", v_wcb->owner_tid);
			ret = rebuild_fs_obj(v_wcb);
		}
	}
	h_wcb = h_list;
	printc("found h_wcb->owner_tid %d\n", h_wcb->owner_tid);
	v_list = h_wcb;
	for (v_wcb  = FIRST_LIST(v_list, next, prev);
	     v_wcb != v_list;
	     v_wcb  = FIRST_LIST(v_wcb, next, prev)) {
		printc("found v_wcb->owner_tid %d\n", v_wcb->owner_tid);
		ret = rebuild_fs_obj(v_wcb);
	}
	
	return;
}


/* restore the server state */
static void
reinstate(td_t tid)
{
	td_t ret;

	struct rec_data_tor *rd;

	if (!(rd = rd_lookup(tid))) return; 	/* root_tid */

	printc("<<<<<< thread %d trying to reinstate....tid %d\n", cos_get_thd_id(), rd->c_tid);
	printc("parent tid %d\n", rd->parent_tid);
	ret = __tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid, rd->c_tid);
	if (ret < 1) {
		printc("re-split failed %d\n", tid);
		BUG();
	}
	if (ret > 0) rd->s_tid = ret;  	/* update the ramfs side tid, only if return a new server id */

	rd->fcnt = fcounter;
	printc("already reinstate c_tid %d s_tid %d >>>>>>>>>>\n", rd->c_tid, rd->s_tid);
	
	return;
}

static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;

        rd = rd_lookup(tid);
	if (!rd) return NULL;
	/* fast path */
	if (likely(rd->fcnt == fcounter)) 
		return rd;

	printc("rd->fcnt %lu  fcounter %lu\n",rd->fcnt,fcounter);
	reinstate(tid);
	return rd;
}

static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_vect;

	/* 1 is already assigned to the td_root */
	for(i = 2 ; i < CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}
	
	if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID;

	return -1;
}


static char*
param_save(char *param, int param_len)
{
	char *l_param;
	l_param = malloc(param_len);
	if (!l_param) {
		printc("cannot malloc \n");
		BUG();
	}
	strncpy(l_param, param, param_len);

	return l_param;
}

/************************************/
/******  client stub functions ******/
/************************************/

struct __sg_tsplit_data {
	td_t tid;	
	td_t desired_tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

/* Split the new torrent and get new ser object */
/* Client only needs know how to find server side object */

CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid)

        printc("<<< In: call tsplit >>>\n");
        ret = __tsplit(spdid, tid, param, len, tflags, evtid, 0);

CSTUB_POST

CSTUB_FN_ARGS_7(td_t, __tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid, td_t, desired_tid)

        printc("<<< In: call __tsplit >>>\n");
        struct __sg_tsplit_data *d;
        struct rec_data_tor *rd, *rd_p;

	char *l_param;
	cbuf_t cb;
        int		sz	   = 0;
        td_t		cli_tid	   = 0;
        td_t		ser_tid	   = 0;
        tor_flags_t	flags	   = tflags;
        td_t		parent_tid = tid;

        assert(param && len > 0);
        assert(param[len] == '\0'); 

        sz = len + sizeof(struct __sg_tsplit_data);
        /* replay on slow path */
        if (unlikely(desired_tid)) reinstate(tid);
        if ((rd_p = rd_lookup(parent_tid))) parent_tid = rd_p->s_tid;

redo:
        d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

        printc("parent_tid: %d\n", parent_tid);
        d->tid	       = parent_tid;
        d->desired_tid = desired_tid;
        d->tflags      = flags;
	d->evtid       = evtid;
	d->len[0]      = 0;
	d->len[1]      = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(__tsplit, spdid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		if (cos_fault_cntl(COS_CAP_FAULT_UPDATE, cos_spd_id(), uc->cap_no)) {
			printc("set cap_fault_cnt failed\n");
			BUG();
		}
		cbuf_free(d);
		printc("\n\n ------- reinstate start\n");
		reinstate(tid);
		printc("reinstate done ------ \n\n");

		printc("\n\n ------- reindata start\n");
		reindata();
		printc("reindata done ------ \n\n");
		goto redo;
	}

        printc("ret from ramfs: %d\n",ret);
        cbuf_free(d);

        if (unlikely(desired_tid)) return ret;

        ser_tid = ret;
        l_param = param_save(param, len);

        rd = rd_alloc();
        assert(rd);
        rd_init(rd);

        if (unlikely(rd_lookup(ser_tid))) {
		cli_tid = get_unique();
		assert(cli_tid > 0 && cli_tid != ser_tid);
		printc("found existing tid %d >> get new cli_tid %d\n", ser_tid, cli_tid);
	} else {
		cli_tid = ser_tid;
	}
        /* client side tid should be guaranteed to be unique now */
        rd_cons(rd, tid, ser_tid, cli_tid, l_param, len, tflags, evtid);

        assert(rd_lookup(cli_tid));
        ret = cli_tid;

CSTUB_POST

CSTUB_FN_ARGS_2(int, trelease, spdid_t, spdid, td_t, tid)

        printc("<<< In: call trelease >>>\n");
        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);

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

CSTUB_POST

CSTUB_FN_ARGS_4(int, tread, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        printc("<<< In: call tread tid %d>>>\n", tid);
        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);

	/* printc("rd->c_tid %d\n", rd->c_tid); */
	/* struct w_cbuf *test = NULL, *list; */
	/* list = rd->cbufs_head; */
	/* for (test  = FIRST_LIST(list, next, prev); */
	/*      test != list; */
	/*      test  = FIRST_LIST(test, next, prev)) { */
	/* 	printc("read->cb %d\n", test->cb); */
	/* 	printc("read->sz %d\n", test->sz); */
	/* 	printc("read->actual_sz %d\n", test->actual_sz); */
	/* } */

        print_rd_info(rd);

CSTUB_ASM_4(tread, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		printc("read failed!!!!\n");
                //no need this anymore? we still need this
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


CSTUB_FN_ARGS_4(int, twrite, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        printc("<<< In: call twrite >>>\n");
        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);
        print_rd_info(rd);

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
        /* ret is the actual incremented offset on server side */
        /* data of size sz is not guaranteed completely written to ramfs  */
        assert(ret > 0);      	
        add_cbufs_record(rd, cb, sz, ret);

        print_rd_info(rd);

CSTUB_POST

void 
print_rd_info(struct rec_data_tor *rd)
{
	assert(rd);
	return;
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

