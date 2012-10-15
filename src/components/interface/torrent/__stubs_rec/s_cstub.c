#include <torrent.h>

struct __sg_tsplit_data {
	td_t tid;
	td_t desired_tid;
	tor_flags_t tflags;
	long evtid;
	int len[2];
	char data[0];
};

td_t __sg_tsplit(spdid_t spdid, cbuf_t cbid, int len)
{
	struct __sg_tsplit_data *d;
	
	d = cbuf2buf(cbid, len);
	if (unlikely(!d)) return -5;
	/* mainly to inform the compiler that optimizations are possible */
	if (unlikely(d->len[0] != 0)) return -2; 
	if (unlikely(d->len[0] >= d->len[1])) return -3;
	if (unlikely(((int)(d->len[1] + sizeof(struct __sg_tsplit_data))) != len)) return -4;

	td_t desired_tid;
	
	return __tsplit(spdid, d->tid, &d->data[0], 
			d->len[1] - d->len[0], d->tflags, d->evtid, d->desired_tid);
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

