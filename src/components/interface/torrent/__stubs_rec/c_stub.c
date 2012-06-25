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

/* /\* used for multiple cbufs tracking  *\/ */
/* struct wrt_cbufs { */
/* 	struct wrt_cbufs *next, *prev; */
/* 	cbuf_t cbid; */
/* 	int sz; */
/* }; */

/* recovery data and related utility functions */
struct rec_data_tor {
	td_t parent_tid;    // id which split from
        td_t s_tid;         // id that returned from the server (might be same)
        td_t c_tid;         // id that viewed by the client (always unique)

	char *param;
	int param_len;
	tor_flags_t tflags;
	long evtid;

	unsigned long fcnt;

	u32_t offset;	
	cbuf_t cbid;
	int cbsz;

	/* struct wrt_cbufs rd_wrt_cbufs; */
};

/**********************************************/
/* slab allocator and cvect for tracking recovery data */
/**********************************************/

CSLAB_CREATE(rd, sizeof(struct rec_data_tor));
CVECT_CREATE_STATIC(rec_vect);

void print_rd_info(struct rec_data_tor *rd);

static struct rec_data_tor *
rd_lookup(td_t td)
{ return cvect_lookup(&rec_vect, td); }

static struct rec_data_tor *
rd_alloc(void)
{
	struct rec_data_tor *rd;
	/* assert(!rd_lookup(td)); */
	rd = cslab_alloc_rd();
	if (!rd) goto done;
done:
	return rd;
}

static void
rd_dealloc(struct rec_data_tor *rd)
{
	assert(rd);
	if (cvect_del(&rec_vect, rd->c_tid)) BUG();
	cslab_free_rd(rd);
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

/* static void */
/* add_cbufs_record(struct rec_data_tor *rd, int cbid, int sz) */
/* { */
/* 	struct wrt_cbufs *wc; */
/* 	wc = malloc(sizeof(struct wrt_cbufs)); */
/* 	if (!wc) BUG(); */

/* 	INIT_LIST(wc, next, prev); */

/* 	wc->cbid = cbid; */
/* 	wc->sz = sz; */

/* 	ADD_LIST(&rd->rd_wrt_cbufs, wc, next, prev); */

/* 	return; */
/* } */

/* find all cbufs that are currently being owned/mapped
   by the spd that crashed. Situations:                  */
/*                     onwer               mapped        */
/*           a)      ser(crashed)           cli          */
/*           b)      ser(crashed)           N/A          */
/*           c)      ser(crashed)           other        */
/*           d)      cli                    ser(crashed) */
/*           e)      cli                    N/A          */
/*           f)      cli                    other        */

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


void 
rd_cons(struct rec_data_tor *rd, td_t tid, td_t ser_tid, td_t cli_tid, char *param, int len, tor_flags_t tflags, long evtid)
{

	assert(rd);

	rd->parent_tid = tid;
	rd->s_tid = ser_tid;
	rd->c_tid = cli_tid;
	
	rd->param = param;
	rd->param_len = len;
	rd->tflags = tflags;
	rd->evtid = evtid;
	
	rd->fcnt = fcounter;
	
	/* what if the server failed by other thread before this point? */
	cvect_add(&rec_vect, rd, cli_tid);

	/* INIT_LIST(&rd->rd_wrt_cbufs, next, prev); */
	return;
}

/* /\* repopulate all the cbufs in order for the torrent into the object *\/ */
/* static int */
/* rebuild_obj(struct rec_data_tor *rd) */
/* { */
/* 	int ret = -1; */
/* 	struct wrt_cbufs *wc = NULL, *list; */

/* 	list = &rd->rd_wrt_cbufs; */

/* 	for (wc = FIRST_LIST(list, next, prev) ;  */
/* 	     wc != list;  */
/* 	     wc = FIRST_LIST(wc, next, prev)) { */
/* 		ret = twrite(cos_spd_id(), rd->s_tid, wc->cbid, wc->sz); */
/* 		if (ret == -1) goto err; */
/* 	} */
/* done: */
/* 	return ret; */
/* err: */
/* 	goto done; */
/* } */


/* restore the server state */
static void
reinstate(struct rec_data_tor *rd)
{
	printc("trying to reinstate....\n");
	assert(rd);
	td_t s_tid;

	s_tid = tsplit(cos_spd_id(), rd->parent_tid, rd->param, rd->param_len, rd->tflags, rd->evtid);

	if (s_tid < 1) {
		printc(" re-split failed %d\n", s_tid);
		return;
	}

	rd->s_tid = s_tid;
	rd->fcnt = fcounter;

        printc("check offset to write %d\n",rd->offset);
        /* printc("check written offset %d\n",trmeta(rd->s_tid)); */
	printc("<<already re-split...reinstate>>\n");

	/* switch(rd->state)  */
	/* { */
	/* case WRITING: */
	/* 	break; */
	/* case READING: */
	/* 	twrite(cos_spd_id(), rd->s_tid, rd->cbid, rd->wrtcb_sz); */
	/* 	twmeta(rd->s_tid, rd->offset); */
	/* 	break; */
	/* case RELEASING: */
	/* 	break; */
	/* } */

	return;
}

static struct rec_data_tor *
update_rd(td_t tid)
{
        struct rec_data_tor *rd;

        rd = rd_lookup(tid);
        assert(rd);

	/* fast path */
	if (likely(rd->fcnt == fcounter)) 
		return rd;
	
	reinstate(rd);

	return rd;
}

static int
get_unique(void)
{
	unsigned int i;
	cvect_t *v;

	v = &rec_vect;

	for(i = 1 ; i < CVECT_MAX_ID ; i++) {
		if (!cvect_lookup(v, i)) return i;
	}
	
	if (!cvect_lookup(v, CVECT_MAX_ID)) return CVECT_MAX_ID;

	return -1;
}


/************************************/
/******  client stub functions ******/
/************************************/

struct __sg_tsplit_data {
	td_t tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

/* Always splits the new torrent and get new ser object */
/* Client only needs update and know how to find server side object */

CSTUB_FN_ARGS_6(td_t, tsplit, spdid_t, spdid, td_t, tid, char *, param, int, len, tor_flags_t, tflags, long, evtid)

        struct __sg_tsplit_data *d;
        struct rec_data_tor *rd;

	char *l_param;
	cbuf_t cb;
        int sz = 0;
        td_t cli_tid = 0;
        td_t ser_tid = 0;
        td_t parent_tid = tid;
        
        assert(param && len > 0);
        assert(param[len] == '\0'); 

        sz = len + sizeof(struct __sg_tsplit_data);

redo:		
        d = cbuf_alloc(sz, &cb);
	if (!d) return -1;

	d->tid = parent_tid;
        d->tflags = tflags;
	d->evtid = evtid;
	d->len[0] = 0;
	d->len[1] = len;
	memcpy(&d->data[0], param, len);

CSTUB_ASM_3(tsplit, spdid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
		cbuf_free(d);
		goto redo;
	}
        cbuf_free(d);
 
        ser_tid = ret;
        l_param = param_save(param, len);

        rd = rd_alloc();
        assert(rd);

        /* An existing tid indicates                                        */
        /* 1. the server must have failed before                            */
        /* 2. the occupied torrent not released yet                         */
        /* 3. same ret does not mean the same object in server necessarily  */
        /* A new tid indicates                                              */
        /* 1. normal path with a new server object or                       */
        /* 2. the server has failed before and just get a new server object */

        if (unlikely(rd_lookup(ser_tid))) {
		cli_tid = get_unique();
		assert(cli_tid > 0 && cli_tid != ser_tid);
		/* printc("found existing ser_tid %d >> get new cli_tid %d\n", ser_tid, cli_tid); */
	} else {
		cli_tid = ser_tid;
	}

        rd_cons(rd, parent_tid, ser_tid, cli_tid, l_param, len, tflags, evtid);

        rd = rd_lookup(ser_tid);
        assert(rd);
        /* rd->state = SPLITTING; */

CSTUB_POST


/* skip tmerge now */

/* assume that tid is always same as rd->c_tid?? Not!!             */
/* This is because the reinstate might change tid                  */
/* that has been used before. To user, this tid should not change  */

CSTUB_FN_ARGS_2(int, trelease, spdid_t, spdid, td_t, tid)

        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);

CSTUB_ASM_2(trelease, spdid, rd->s_tid)

        if (unlikely(fault)) {
		fcounter++;
                goto redo;
	}

        assert(rd);
        rd_dealloc(rd);

CSTUB_POST

CSTUB_FN_ARGS_4(int, tread, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);
        rd->cbid = cb;
        rd->cbsz = sz;

        print_rd_info(rd);

CSTUB_ASM_4(tread, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
                goto redo;
	}

        assert(rd);
        /* rd->offset = trmeta(rd->s_tid); */
        print_rd_info(rd);

CSTUB_POST


CSTUB_FN_ARGS_4(int, twrite, spdid_t, spdid, td_t, tid, cbuf_t, cb, int, sz)

        struct rec_data_tor *rd;

redo:
        rd = update_rd(tid);
        rd->cbid = cb;
        rd->cbsz = sz;

        print_rd_info(rd);

CSTUB_ASM_4(twrite, spdid, rd->s_tid, cb, sz)

        if (unlikely(fault)) {
		fcounter++;
                goto redo;
	}

        assert(rd);
        /* rd->offset = trmeta(rd->s_tid); */
        print_rd_info(rd);

CSTUB_POST


/* // not used now */
/* CSTUB_FN_ARGS_3(int, trmeta, td_t, tid, cbuf_t, cb, int, sz) */

/*         td_t this_tid = 0; */

/* redo: */
/* CSTUB_ASM_3(trmeta, tid, cb, sz) */

/*         if (unlikely(fault)) { */
/* 		this_tid = reinstate(tid); */
/*                 goto redo; */
/* 	} */
/* 	printc("In ctrmeta: tid %d\n", tid); */
/*         assert(ret >= 0); */

/* CSTUB_POST */

