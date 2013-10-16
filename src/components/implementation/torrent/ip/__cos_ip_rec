/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

/* 
 * This is a place-holder for when all of IP is moved here from
 * cos_net.  There should really be no performance difference between
 * having this empty like this, and having IP functionality in here.
 * If anything, due to cache (TLB) effects, having functionality in
 * here will be slower.
 */
#include <cos_component.h>
#include <torlib.h>
#include <torrent.h>
#include <cos_synchronization.h>
#include <cbuf.h>


unsigned long long start, end;
//#define MEAS_TREAD
//#define MEAS_TWRITE
//#define MEAS_TSPLIT
//#define MEAS_TRELEASE

extern td_t parent_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
/* extern td_t parent___tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid, int flag); */
extern void parent_trelease(spdid_t spdid, td_t tid);
extern int parent_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int parent_twrite(spdid_t spdid, td_t td, int cbid, int sz);

/* required so that we can have a rodata section */
const char *name = "cos_ip";

/* int ip_xmit(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_xmit(cos_spd_id(), d); */
/* } */

/* int ip_wait(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_wait(cos_spd_id(), d); */
/* } */

/* int ip_netif_release(spdid_t spdid) */
/* { */
/* 	return netif_event_release(cos_spd_id()); */
/* } */

/* int ip_netif_create(spdid_t spdid) */
/* { */
/* 	return netif_event_create(cos_spd_id()); */
/* } */

td_t 
tsplit(spdid_t spdid, td_t tid, char *param, int len, 
       tor_flags_t tflags, long evtid)
{
	td_t ret = -ENOMEM, ntd;
	struct torrent *t;

	/* printc("cos_ip: tsplit (thd %d)\n", cos_get_thd_id()); */
	/* printc("spdid %d tid, %d param %s len %d tflags %d evtid %d\n", spdid, tid, param, len, tflags, evtid); */
	if (tid != td_root) return -EINVAL;
	/* ntd = parent___tsplit(cos_spd_id(), tid, param, len, tflags, evtid, 0); */
#ifdef MEAS_TSPLIT
	rdtscll(start);
#endif
	ntd = parent_tsplit(cos_spd_id(), tid, param, len, tflags, evtid);
#ifdef MEAS_TSPLIT
	rdtscll(end);
	printc("tip_tif_tsplit %llu\n", end-start);
#endif

	if (ntd <= 0) ERR_THROW(ntd, err);

	t = tor_alloc((void*)ntd, tflags);
	if (!t) ERR_THROW(-ENOMEM, err);
	ret = t->td;
err:
	return ret;
}

/* td_t __tsplit(spdid_t spdid, td_t tid, char *param, int len, */
/* 	      tor_flags_t tflags, long evtid, int flag) */
/* { */
/* 	printc("cos_ip: __tsplit\n"); */
/* 	return tsplit(spdid, tid, param, len, tflags, evtid); */
/* 	/\* return parent___tsplit(spdid, tid, param, len, tflags, evtid, flag); *\/ */
/* } */

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;
	td_t ntd;

	if (!tor_is_usrdef(td)) return;
	t = tor_lookup(td);
	if (!t) goto done;
	ntd = (td_t)t->data;

#ifdef MEAS_TRELEASE
	rdtscll(start);
#endif
	parent_trelease(cos_spd_id(), ntd);
#ifdef MEAS_TRELEASE
	rdtscll(end);
	printc("tip_tif_trelease %llu\n", end-start);
#endif

	tor_free(t);
done:
	return;
}

int 
twmeta(spdid_t spdid, td_t td, int cbid, int sz, int offset, int flag)
{
	return -ENOTSUP;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	return -ENOTSUP;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;
	/* printc("cos_ip: twrite (thd %d)\n", cos_get_thd_id()); */
	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	nbuf = cbuf_alloc(sz, &ncbid);
	assert(nbuf);
	memcpy(nbuf, buf, sz);

#ifdef MEAS_TWRITE
	rdtscll(start);
#endif
	ret = parent_twrite(cos_spd_id(), ntd, ncbid, sz);
#ifdef MEAS_TWRITE
	rdtscll(end);
	printc("tip_tif_twrite %llu\n", end-start);
#endif

	cbuf_free(nbuf);
done:
	return ret;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;

	/* printc("cos_ip: tread\n"); */
	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t)                      ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	/* printc("cos_ip: tread <<<2>>\n"); */
	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);
	nbuf = cbuf_alloc(sz, &ncbid);
	assert(nbuf);
#ifdef MEAS_TREAD
	rdtscll(start);
#endif
	ret = parent_tread(cos_spd_id(), ntd, ncbid, sz);
#ifdef MEAS_TREAD
	rdtscll(end);
	printc("tip_tif_tread %llu (thd %d)\n", end-start, cos_get_thd_id());
#endif

	if (ret < 0) goto free;
	/* printc("cos_ip: tread 2\n"); */
	memcpy(buf, nbuf, ret);
free:
	cbuf_free(nbuf);
done:
	return ret;
}

void cos_init(void)
{
	torlib_init();
}
