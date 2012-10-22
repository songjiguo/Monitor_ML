#include <torrent.h>

/* create an inconsistency list, so we can tell later when read/write
 * on an un-recovered file */

struct tor_list {
	int fid;
	int tid;
	struct tor_list *next, *prev;
};
struct tor_list *all_tor_list = NULL;

struct __sg_tsplit_data {
	td_t tid;
	td_t desired_tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

extern int tmap_lookup(spdid_t spdid, cbuf_t cb, int sz);

static void
find_update(int fid, int val)
{
	struct tor_list *item;
	assert(all_tor_list);
	
	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		if (fid == item->fid) {
			item->tid = val;
			break;
		}
	}

	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		printc("fid in the list %d has tid %d\n", item->fid, item->tid);
	}
	
	return;
}

static void
find_del(int fid)
{
	struct tor_list *item;
	assert(all_tor_list);
	
	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		if (fid == item->fid) {
			REM_LIST(item, next, prev);
			break;
		}
	}

	for (item = FIRST_LIST(all_tor_list, next, prev);
	     item != all_tor_list;
	     item = FIRST_LIST(item, next, prev)){
		printc("after delete : fid in the list %d\n", item->fid);
	}
	
	return;
}

static void
build_tor_list()
{
	int total_tor = 0, nth_fid = 0;
	int fid;
	struct tor_list *item;

	total_tor = cbuf_c_introspect(cos_spd_id(), 0, 0, CBUF_INTRO_TOT_TOR);
	printc("total file number %d\n", total_tor);

	while(total_tor--) {
		nth_fid++;
		fid   = cbuf_c_introspect(cos_spd_id(), 0, nth_fid, CBUF_INTRO_FID);
		item  = (struct tor_list *)malloc(sizeof(struct tor_list));
		if (!item) assert(0);
		
		item->fid = fid;
		item->tid = 0;
		INIT_LIST(item, next, prev);
		
		if (!all_tor_list) { /* initialize the head */
			all_tor_list = (struct tor_list *)malloc(sizeof(struct tor_list));
			INIT_LIST(all_tor_list, next, prev);
		}
		
		ADD_LIST(all_tor_list, item, next, prev);
		
		printc("FID: %d\n", fid);
	}
	return;
}

static int
restore_tor(int fid, td_t tid)
{
	int ret = 0;
	int cbid = 0, sz = 0, offset = 0;
	int nth_cb = 0, total_cbs = 0;

	total_cbs = cbuf_c_introspect(cos_spd_id(), fid, 0, CBUF_INTRO_TOT);
	/* printc("total cbs %d\n", total_cbs); */

	while(total_cbs--) {
		nth_cb++;
		cbid   = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_CBID);
		if (!cbid) goto no_found;
		sz     = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_SIZE);
		if (!sz) goto no_found;
		offset = cbuf_c_introspect(cos_spd_id(), fid, nth_cb, CBUF_INTRO_OFFSET);
	
		printc("found cbid %d size %d offset %d\n", cbid, sz, offset);
		printc("meta write:: tor id %d\n", tid);
		twmeta(cos_spd_id(), tid, cbid, sz, offset, 1);	/* 1 for recovery now */
	}
	
	find_del(fid);
done:
	return ret;
no_found: 
	printc("Not found the record\n");
	ret = -1;
	goto done;
}

td_t __sg_tsplit(spdid_t spdid, cbuf_t cb, int len, int desired_tid)
{
	struct __sg_tsplit_data *d;

	td_t s_torid;
	char *name;
	int fid = 0;
	int sz;
	cbuf_t cb_p;
	char *dd;
	
	int total_fids;

	/* if (!all_tor_list) printc("empty list sssssss\n"); */

	d = cbuf2buf(cb, len);
	if (unlikely(!d)) return -5;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -2; 
	if (unlikely(d->len[0] >= d->len[1])) return -3;
	if (unlikely(((int)(d->len[1] + sizeof(struct __sg_tsplit_data))) != len)) return -4;

	s_torid =  __tsplit(spdid, d->tid, &d->data[0], 
			d->len[1] - d->len[0], d->tflags, d->evtid, d->desired_tid);
	printc("new obtained torretn id %d\n", s_torid);
	

	name = &d->data[0];
	sz = strlen(name) - 1; /* memcpy append '\' if there isno '\' ? */
	/* printc("s: subpath name %s size %d\n", name, sz); */
	printc("saved torretn id %d\n", d->tid);
	dd = cbuf_alloc(sz, &cb_p);
	if (!dd) return -1;
	memcpy(dd, name, sz);
	
	fid = tmap_lookup(cos_spd_id(), cb_p, sz);
	if (fid == -1) {
		printc("can not find the file\n");
		fid = tmap_get(cos_spd_id(), cb_p, sz);
		printc("fid %d\n", fid);
		assert(fid >= 0);
	}
	cbuf_free(dd);

	if (unlikely(desired_tid)) { /* recovery */
		if (!all_tor_list) build_tor_list(); 
                /* FIXME: we should really use cbuf to introspect these parameter with 1 invocation */
		restore_tor(fid, s_torid); 
		printc("rebuild meta done\n");
	}

	if (all_tor_list) find_update(fid, s_torid);

	return s_torid;
}

struct __sg_twmeta_data {
	td_t td;
	cbuf_t cb;
	int sz;
	int offset;
	int flag;
};

int __sg_twmeta(spdid_t spdid, cbuf_t cb_m, int len_m)
{
	struct __sg_twmeta_data *d;

	d = cbuf2buf(cb_m, len_m);
	if (unlikely(!d)) return -1;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->sz == 0)) return -1;
	if (unlikely(d->cb == 0)) return -1;
	if (unlikely(d->offset < 0)) return -1;


	/* printc("\n\n [[[calling twmeta]]] \n\n"); */
	/* printc("server: d->td %d\n", d->td); */
	/* printc("server: d->cb %d\n", d->cb); */
	/* printc("server: d->sz %d\n", d->sz); */
	/* printc("server: d->offset %d\n", d->offset); */
	/* printc("server: d->flag %d\n", d->flag); */

	/* **************************************** */
	/* This is the work that tries to bring the meta recovery to the interface */

	/* char *buf; */
	/* u32_t id; */
	/* int buf_sz; */
	/* cbuf_unpack(d->cb, &id, (u32_t *)&buf_sz); */

	/* if (d->flag == 1) { 		/\* strict in the order for now? recovery*\/ */
	/* 	buf = cbuf_c_introspect(cos_spd_id(), id, CBUF_INTRO_PAGE); */
	/* 	if (!buf) ERR_THROW(-EINVAL, done); */
	/* } */

	/* return twrite(spdid, d->td, cb_m, len_m); */

	/* **************************************** */

	return twmeta(spdid, d->td, d->cb, d->sz, d->offset, d->flag);
}


int __sg_twrite(spdid_t spdid, td_t tid, cbuf_t cb, int sz)
{
	return twrite(spdid, tid, cb, sz);	
}

int __sg_tread(spdid_t spdid, td_t tid, cbuf_t cb, int sz)
{
	struct tor_list *item;
	if (unlikely(all_tor_list)) { /* has crashed before, need check if the file still presents */
		printc("when tread, tid is %d\n", tid);

		for (item = FIRST_LIST(all_tor_list, next, prev);
		     item != all_tor_list;
		     item = FIRST_LIST(item, next, prev)){
			if (tid == item->tid) {
				restore_tor(item->fid, tid); /* tid is unique in the list?? */
				break;
			}
		}
	}
	return tread(spdid, tid, cb, sz);	
}


struct __sg_tmerge_data {
	td_t td;
	td_t td_into;
	int len[2];
	char data[0];
};
int __sg_tmerge(spdid_t spdid, cbuf_t cbid, int len)
{
	struct __sg_tmerge_data *d;

	d = cbuf2buf(cbid, len);
	if (unlikely(!d)) return -1;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -1; 
	if (unlikely(d->len[0] >= d->len[1])) return -1;
	if (unlikely(((int)(d->len[1] + (sizeof(struct __sg_tmerge_data)))) != len)) return -1;

	return tmerge(spdid, d->td, d->td_into, &d->data[0], d->len[1] - d->len[0]);
}

