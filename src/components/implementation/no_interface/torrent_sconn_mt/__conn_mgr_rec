/**
 * Copyright 2012 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Adapted from the connection manager (no_interface/conn_mgr/) and
 * the file descriptor api (fd/api).
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <cvect.h>
#include <print.h>
#include <errno.h>
#include <cos_synchronization.h>
#include <sys/socket.h>
#include <stdio.h>
#include <torrent.h>
extern td_t from_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void from_trelease(spdid_t spdid, td_t tid);
extern int from_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int from_twrite(spdid_t spdid, td_t td, int cbid, int sz);
#include <sched.h>

unsigned long long start, end;
//#define MEAS_TREAD
//#define MEAS_TWRITE
//#define MEAS_TSPLIT
//#define MEAS_TRELEASE

//#define MEAS_FROM_TREAD
//#define MEAS_FROM_TWRITE
//#define MEAS_FROM_TSPLIT
//#define MEAS_FROM_TRELEASE


static cos_lock_t sc_lock;
#define LOCK() if (lock_take(&sc_lock)) BUG();
#define UNLOCK() if (lock_release(&sc_lock)) BUG();

#define BUFF_SZ 2048//1401 //(COS_MAX_ARG_SZ/2)

CVECT_CREATE_STATIC(tor_from);
CVECT_CREATE_STATIC(tor_to);

static inline int 
tor_get_to(int from, long *teid) 
{ 
	int val = (int)cvect_lookup(&tor_from, from);
	*teid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline int 
tor_get_from(int to, long *feid) 
{ 
	int val = (int)cvect_lookup(&tor_to, to);
	*feid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline void 
tor_add_pair(int from, int to, long feid, long teid)
{
#define MAXVAL (1<<16)
	assert(from < MAXVAL);
	assert(to   < MAXVAL);
	assert(feid < MAXVAL);
	assert(teid < MAXVAL);
	if (cvect_add(&tor_from, (void*)((teid << 16) | to), from) < 0) BUG();
	if (cvect_add(&tor_to, (void*)((feid << 16) | from), to) < 0) BUG();
}

static inline void
tor_del_pair(int from, int to)
{
	cvect_del(&tor_from, from);
	cvect_del(&tor_to, to);
}

CVECT_CREATE_STATIC(evts);
#define EVT_CACHE_SZ 0
int evt_cache[EVT_CACHE_SZ];
int ncached = 0;

long evt_all[MAX_NUM_THREADS] = {0,};

static inline long
evt_wait_all(void) { 
/* printc("evt wait all --  spd %ld thread %d\n", cos_spd_id(), cos_get_thd_id()); */
return evt_wait(cos_spd_id(), evt_all[cos_get_thd_id()]); }

/* 
 * tor > 0 == event is "from"
 * tor < 0 == event is "to"
 */
static inline long
evt_get_thdid(int thdid)
{
	long eid;

	if (!evt_all[thdid]) evt_all[thdid] = evt_split(cos_spd_id(), 0, 1);
	assert(evt_all[thdid]);

	eid = (ncached == 0) ?
		evt_split(cos_spd_id(), evt_all[thdid], 0) :
		evt_cache[--ncached];
	assert(eid > 0);

	return eid;
}

static inline long
evt_get(void) {return evt_get_thdid(cos_get_thd_id()); }

static inline void
evt_put(long evtid)
{
	if (ncached >= EVT_CACHE_SZ) evt_free(cos_spd_id(), evtid);
	else                         evt_cache[ncached++] = evtid;
}

/* positive return value == "from", negative == "to" */
static inline int
evt_torrent(long evtid) { return (int)cvect_lookup(&evts, evtid); }

static inline void
evt_add(int tid, long evtid) { cvect_add(&evts, (void*)tid, evtid); }

struct tor_conn {
	int  from, to;
	long feid, teid;
};

static inline void 
mapping_add(int from, int to, long feid, long teid)
{
	long tf, tt;

	LOCK();
	tor_add_pair(from, to, feid, teid);
	evt_add(from,    feid);
	evt_add(to * -1, teid);
	assert(tor_get_to(from, &tt) == to);
	assert(tor_get_from(to, &tf) == from);
	assert(evt_torrent(feid) == from);
	assert(evt_torrent(teid) == (-1*to));
	assert(tt == teid);
	assert(tf == feid);
	UNLOCK();
}

static inline void
mapping_remove(int from, int to, long feid, long teid)
{
	LOCK();
	tor_del_pair(from, to);
	cvect_del(&evts, feid);
	cvect_del(&evts, teid);
	UNLOCK();
}

static unsigned long long aaa = 0;

static void 
accept_new(int accept_fd)
{
	int from, to, feid, teid;

	while (1) {
		/* if (aaa++ % 1000 == 0) printc("conn: accept_new spinning %d\n", cos_get_thd_id()); */
		feid = evt_get();
		assert(feid > 0);
#ifdef MEAS_FROM_TSPLIT
		rdtscll(start);
#endif
		from = from_tsplit(cos_spd_id(), accept_fd, "", 0, TOR_RW, feid);
#ifdef MEAS_FROM_TSPLIT
		rdtscll(end);
		printc("conn_tnet_tsplit %llu\n", end-start);
#endif

		assert(from != accept_fd);
		if (-EAGAIN == from) {
			evt_put(feid);
			return;
		} else if (from < 0) {
			BUG();
			return;
		}
		/* printc("https evt_get\n"); */
		teid = evt_get();
		assert(teid > 0);

#ifdef MEAS_TSPLIT
		rdtscll(start);
#endif
		to = tsplit(cos_spd_id(), td_root, "", 0, TOR_RW, teid);
#ifdef MEAS_TSPLIT
		rdtscll(end);
		printc("conn_http_tsplit %llu\n", end-start);
#endif

		if (to < 0) {
			BUG();
		}

		mapping_add(from, to, feid, teid);
	}
}

static unsigned long long bbb = 0;
static void 
from_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;

	from = tc->from;
	to   = tc->to;
	while (1) {
		int ret;
		cbuf_t cb;
		/* if (bbb++ % 1000 == 0) printc("conn: from_data_new spinning %d\n", cos_get_thd_id()); */
		buf = cbuf_alloc(BUFF_SZ, &cb);
		assert(buf);

#ifdef MEAS_FROM_TREAD
		rdtscll(start);
#endif
		amnt = from_tread(cos_spd_id(), from, cb, BUFF_SZ-1);
#ifdef MEAS_FROM_TREAD
		rdtscll(end);
		printc("conn_tnet_tr %llu (thd %d)\n", end-start, cos_get_thd_id());
#endif
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		assert(amnt <= BUFF_SZ);

#ifdef MEAS_TWRITE
		rdtscll(start);
#endif
		if (amnt != (ret = twrite(cos_spd_id(), to, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, to);
			goto close;

		}
#ifdef MEAS_TWRITE
		rdtscll(end);
		printc("conn_http_twr %llu\n", end-start);
#endif

		cbuf_free(buf);
	}
done:
	cbuf_free(buf);
	return;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	from_trelease(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	evt_put(tc->feid);
	evt_put(tc->teid);
	goto done;
}

static unsigned long long ccc = 0;
static void 
to_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;

	from = tc->from;
	to   = tc->to;
	while (1) {
		int ret;
		cbuf_t cb;
		/* if (ccc++ % 1000 == 0) printc("conn: to data new %d spinning \n", cos_get_thd_id()); */
		if (!(buf = cbuf_alloc(BUFF_SZ, &cb))) BUG();
		/* printc("conn_mgr: tread\n"); */
		
#ifdef MEAS_TREAD
		rdtscll(start);
#endif
		amnt = tread(cos_spd_id(), to, cb, BUFF_SZ-1);
#ifdef MEAS_TREAD
		rdtscll(end);
		printc("conn_http_tr %llu (thd %d)\n", end-start, cos_get_thd_id());
#endif
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		assert(amnt <= BUFF_SZ);

#ifdef MEAS_FROM_TWRITE
		rdtscll(start);
#endif
		if (amnt != (ret = from_twrite(cos_spd_id(), from, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d of %d on fd %d\n", 
			       ret, amnt, to);
			goto close;
		}
#ifdef MEAS_FROM_TWRITE
		rdtscll(end);
		printc("conn_tnet_twr %llu\n", end-start);
#endif

		cbuf_free(buf);
	}
done:
	cbuf_free(buf);
	return;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	from_trelease(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	evt_put(tc->feid);
	evt_put(tc->teid);
	goto done;
}

char *create_str;
int   __port, __prio, hpthd;

static void init(char *init_str)
{
	int nthds;

	cvect_init_static(&evts);
	cvect_init_static(&tor_from);
	cvect_init_static(&tor_to);
	lock_static_init(&sc_lock);
		
	sscanf(init_str, "%d:%d:%d", &nthds, &__prio, &__port);
	printc("nthds:%d, prio:%d, port %d\n", nthds, __prio, __port);
	create_str = strstr(init_str, "/");
	assert(create_str);

	for (; nthds > 0 ; nthds--) {
		union sched_param sp;
		int thdid;
		
		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = __prio++;
		thdid = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		if (!hpthd) hpthd = thdid;
	}
}

u64_t meas, avg, total = 0, vartot;
int meascnt = 0, varcnt;

void
meas_record(u64_t meas)
{
	if (cos_get_thd_id() != hpthd) return;
	total += meas;
	meascnt++;
	assert(meascnt > 0);
	avg = total/meascnt;
	if (meas > avg) {
		vartot += meas-avg;
		varcnt++;
	}
}

static unsigned long long ttt = 0;
void
cos_init(void *arg)
{
	int c, accept_fd, ret;
	long eid;
	char *init_str = cos_init_args();
	char __create_str[128];
	static volatile int first = 1, off = 0;
	int port;
	u64_t start, end;

	if (first) {
		first = 0;
		init(init_str);
		return;
	}
	
	printc("Thread %d, port %d\n", cos_get_thd_id(), __port+off);
	port = off++;
	port += __port;
	/* printc("init Network: evt_get\n"); */
	eid = evt_get();
	if (snprintf(__create_str, 128, create_str, port) < 0) BUG();
	ret = c = from_tsplit(cos_spd_id(), td_root, __create_str, strlen(__create_str), TOR_ALL, eid);
	if (ret <= td_root) BUG();
	accept_fd = c;
	evt_add(c, eid);

	rdtscll(start);
	/* event loop... */
	while (1) {
		struct tor_conn tc;
		int t;
		long evt;

		/* printc("conn_mgr: 1\n"); */
		memset(&tc, 0, sizeof(struct tor_conn));
		rdtscll(end);
		meas_record(end-start);
		/* printc("conn: thd %d wait event\n", cos_get_thd_id()); */
		evt = evt_wait_all();
		/* printc("conn: thd %d event comes\n", cos_get_thd_id()); */
		/* if (ttt++ % 1000 == 0) printc("conn: event loop %d spinning \n", cos_get_thd_id()); */
		rdtscll(start);
		t   = evt_torrent(evt);
		/* printc("conn_mgr: 2\n"); */
		if (t > 0) {
			tc.feid = evt;
			tc.from = t;
			if (t == accept_fd) {
				tc.to = 0;
				accept_new(accept_fd);
				/* if (ttt++ % 1000 == 0) printc("conn: after accept new (thd %d)\n", cos_get_thd_id()); */
				/* printc("conn_mgr: 3 (thd %d)\n", cos_get_thd_id()); */
			} else {
				tc.to = tor_get_to(t, &tc.teid);
				assert(tc.to > 0);
				from_data_new(&tc);
				/* printc("conn_mgr: 4 (thd %d)\n", cos_get_thd_id()); */
			}
		} else {
			t *= -1;
			tc.teid = evt;
			tc.to   = t;
			/* printc("conn_mgr: 5\n"); */
			tc.from = tor_get_from(t, &tc.feid);
			assert(tc.from > 0);
			/* if (ttt++ % 1000 == 0) printc("conn: after get from (thd %d)\n", cos_get_thd_id()); */
			/* printc("conn_mgr: 6\n"); */
			to_data_new(&tc);
		}

		cos_mpd_update();
		/* printc("conn: 17 is back!!\n"); */
	}
}

int
periodic_wake_get_misses(unsigned short int tid)
{
	return 0;
}

int
periodic_wake_get_deadlines(unsigned short int tid) 
{
	return 0;
}

long
periodic_wake_get_lateness(unsigned short int tid)
{
	return 0;
}

long
periodic_wake_get_miss_lateness(unsigned short int tid)
{
	long long avg;

	if (varcnt == 0) return 0;
	avg = vartot/varcnt;
	/* right shift 20 bits and round up, 2^20 - 1 = 1048575 */	
///	avg = (avg >> 20) + ! ((avg & 1048575) == 0);
	avg = (avg >> 8) + 1;//! ((avg & 1048575) == 0);
	vartot = 0;
	varcnt = 0;

	return avg;
}

int
periodic_wake_get_period(unsigned short int tid)
{
	if (tid == hpthd) return 1;
	return 0;
}
