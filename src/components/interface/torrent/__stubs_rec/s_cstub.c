#include <torrent.h>

struct __sg_tsplit_data {
	td_t tid;
	td_t desired_tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

extern int tmap_lookup(spdid_t spdid, cbuf_t cb, int sz);

static int
restore_tor(int fid)
{
	int ret = 0;
	int cbid = 0, sz = 0, offset = 0;
	int total_cbs = 0;

	total_cbs = cbuf_c_introspect(cos_spd_id(), fid, CBUF_INTRO_TOT);
	/* printc("total cbs %d\n", total_cbs); */

	while(total_cbs--) {
		cbid   = cbuf_c_introspect(cos_spd_id(), fid, CBUF_INTRO_CBID);
		sz     = cbuf_c_introspect(cos_spd_id(), fid, CBUF_INTRO_SIZE);
		offset = cbuf_c_introspect(cos_spd_id(), fid, CBUF_INTRO_OFFSET);
	
		printc("found cbid %d size %d offset %d\n", cbid, sz, offset);

		/* twmeta(spdid_t spdid, td_t td, cbuf_t cb, int sz, int offset, int flag); */
	}
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
	
	d = cbuf2buf(cb, len);
	if (unlikely(!d)) return -5;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -2; 
	if (unlikely(d->len[0] >= d->len[1])) return -3;
	if (unlikely(((int)(d->len[1] + sizeof(struct __sg_tsplit_data))) != len)) return -4;

	char *name;
	int fid = 0;
	int sz;
	cbuf_t cb_p;
	char *dd;
	
	if (unlikely(desired_tid)) {
		name = &d->data[0];
		sz = strlen(name) - 1; /* memcpy append '\' if there isno '\' ? */
		/* printc("s: subpath name %s size %d\n", name, sz); */
		dd = cbuf_alloc(sz, &cb_p);
		if (!dd) return -1;
		memcpy(dd, name, sz);
		
		fid = tmap_lookup(cos_spd_id(), cb_p, sz);
		assert(fid >= 0);
		printc("found fid %d\n", fid);

		restore_tor(fid);
		
		cbuf_free(dd);
	}

	return __tsplit(spdid, d->tid, &d->data[0], 
			d->len[1] - d->len[0], d->tflags, d->evtid, d->desired_tid);
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

