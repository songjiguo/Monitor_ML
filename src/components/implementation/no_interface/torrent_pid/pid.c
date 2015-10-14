/* adapted from conn_mgr */

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
#include <sched.h>
#include <periodic_wake.h>

#include <log.h>

extern td_t from_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void from_trelease(spdid_t spdid, td_t tid);
extern int from_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int from_twrite(spdid_t spdid, td_t td, int cbid, int sz);

/* pid control */
#include "pid.h"
#include <math.h>

float deriv_error = 0.0;
float integral_error = 0.0;
float last_error = 0.0;
#define TARGET_HEADING 90.0 

unsigned int pid_thd = 0;
int pid_torrent = 0;

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
evt_wait_all(void) { return evt_wait(cos_spd_id(), evt_all[cos_get_thd_id()]); }

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

	/* /\* if we do not cache, it is faster? (8000 reqs/sec) *\/ */
	/* eid = evt_split(cos_spd_id(), evt_all[thdid], 0); */

	eid = (ncached == 0) ?
		evt_split(cos_spd_id(), evt_all[thdid], 0) :
		evt_cache[--ncached];
	assert(eid > 0);

	return eid;
}

static inline long
evt_get(void) { return evt_get_thdid(cos_get_thd_id()); }

static inline int
evt_torrent(long evtid) { return (int)cvect_lookup(&evts, evtid); }

static inline void
evt_add(int tid, long evtid) { cvect_add(&evts, (void*)tid, evtid); }


static inline void 
mapping_add(int from, int to, long feid, long teid)
{
	LOCK();
	evt_add(from,    feid);
	assert(evt_torrent(feid) == from);
	UNLOCK();
}

/* simulator ==> pid */
static int
from_data_new(ap_data *in_data)
{
	char *buf;
	cbuf_t cb;
	int amnt;
	int ret = 0;

	/* printc("from_data_new\n"); */

	buf = cbuf_alloc(BUFF_SZ, &cb);
	assert(buf);
	amnt = from_tread(cos_spd_id(), pid_torrent, cb, BUFF_SZ-1);
	if (0 == amnt) {
		/* printc("0 amnt\n"); */
		goto done;
	}
	else if (-EPIPE == amnt) {
		printc("EPIPE close connection\n");
		goto close;
	} else if (amnt < 0) {
		/* printc("read from pid_torrent %d produced %d.\n", pid_torrent, amnt); */
		goto done;
	}

	/* copy the external information here*/
	// TODO

	printc("simulator ==> pid:: %s\n", buf);

	if (buf) ret = 1;
	
done:
	cbuf_free(buf);
	return ret;
close:
	from_trelease(cos_spd_id(), pid_torrent);
	goto done;
}

/* pid ==> simulator */
static void 
to_data_new(ap_data *out_data)
{
	int amnt, ret;
	char *buf;
	cbuf_t cb;

	/* printc("to_data_new\n"); */
	
	if (!(buf = cbuf_alloc(BUFF_SZ, &cb))) BUG();
	
	static int pid2out = 20;

	/* prepare the information to be sent simulator here */
	// TODO:

	/* char tmpstr[1024]; */
	/* char *test_str = tmpstr; */
	/* sprintf(test_str, "%d", pid2out++); */
	
	char *test_str = "fake msg from PID controller\n";

	memcpy(buf, test_str, strlen(test_str)+1);
	amnt = strlen(test_str);

	if (amnt != (ret = from_twrite(cos_spd_id(), pid_torrent, cb, amnt))) {
		printc("write failed w/ %d of %d\n", ret, amnt);
		goto close;
	}

	printc("pid ==> simulator:: %s\n", buf);

	memset(buf, 0, strlen(test_str)+1);
	
done:
	cbuf_free(buf);
	return;
close:
	from_trelease(cos_spd_id(), pid_torrent);
	goto done;
}


static void 
accept_new(int accept_fd)
{
	int eid;

	eid = evt_get();
	assert(eid > 0);
	pid_torrent = from_tsplit(cos_spd_id(), accept_fd, "", 0, TOR_RW, eid);
	assert(pid_torrent!= accept_fd);
	
	printc("accept_new: eid %d pid_torrent %d (accept_fd %d)\n", 
	       eid, pid_torrent, accept_fd);
	
	if (-EAGAIN == pid_torrent) {
		evt_free(cos_spd_id(), eid);
		return;
	} else if (pid_torrent < 0) {
		printc("pwrite to id_torrent %d\n", pid_torrent);
		BUG();
		return;
	}
	
	mapping_add(pid_torrent, 0, eid, 0);
}

char *create_str;
int   __port, __prio;

static void init(char *init_str)
{
	int nthds;

	cvect_init_static(&evts);
	cvect_init_static(&tor_from);
	cvect_init_static(&tor_to);
	lock_static_init(&sc_lock);
		
	sscanf(init_str, "%d:%d:%d", &nthds, &__prio, &__port);
	/* printc("nthds:%d, prio:%d, port %d\n", nthds, __prio, __port); */
	create_str = strstr(init_str, "/");
	assert(create_str);

	for (; nthds > 0 ; nthds--) {
		union sched_param sp;
		int thdid;
		
		sp.c.type  = SCHEDP_PRIO;
		sp.c.value = __prio++;
		thdid = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	}
}


static void
ap_control(ap_data *in_data, ap_data *out_data)
{
	float pid_out;
	float cur_heading = in_data->heading_deg;
	float cur_roll = in_data->roll_deg;
	//float actual_error = fabs(cur_heading - TARGET_HEADING);
	float actual_error = TARGET_HEADING - cur_heading;
	float target_roll = 0.0;
	out_data->aileron = in_data->aileron;
	float kp, kd, ki;

	kp = 1.0;
	ki = 0.00002;
	kd = 3.0;

	deriv_error = actual_error - last_error;
	last_error = actual_error;

	integral_error += last_error;

	target_roll = kp*actual_error + kd*deriv_error + ki*integral_error;
	/* printc("p: %f, i: %f, d: %f, target_roll: %f\n", kp*actual_error, */
	/*        ki*integral_error, */
	/*        kd*deriv_error, target_roll); */
	if(fabs(target_roll) > 20.0) {
		target_roll = target_roll < 0.0 ? -20.0 : 20.0;
	} 
	
	out_data->aileron = 0.5 * (target_roll - cur_roll) / 20.0;
	/* printc("\n\ncurrent heading: %f, aileron setting: %f, roll_deg: %f\n\n", in_data->heading_deg, out_data->aileron, in_data->roll_deg); */
}

/* periodic PID controller : read sensor data and send control data*/
static void
pid_process()
{
	ap_data in_data;
	ap_data out_data;

	if (periodic_wake_create(cos_spd_id(), PID_PERIOD)) BUG();
	while(1) {
		periodic_wake_wait(cos_spd_id());
		/* printc("PERIODIC: pid....(thd %d in spd %ld)\n",  */
		/*        cos_get_thd_id(), cos_spd_id()); */

		/* simulator ==> pid */
		if (from_data_new(&in_data)) {
			/* If there is data to process, we process and
			 * send back to simulator */
			/* pid */
			ap_control(&in_data, &out_data);			
			/* pid ==> simulator */
			to_data_new(&out_data);
		}
	}
}

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

	if (cos_get_thd_id() == pid_thd) {
		pid_process();
	}
	
	union sched_param sp;

	if (first) {
		first = 0;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		pid_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		init(init_str);
		return;
	}


	printc("Thread %d, port %d\n", cos_get_thd_id(), __port+off);	
	port = off++;
	port += __port;
	eid = evt_get();
	if (snprintf(__create_str, 128, create_str, port) < 0) BUG();
	ret = c = from_tsplit(cos_spd_id(), td_root, __create_str, strlen(__create_str), TOR_ALL, eid);
	if (ret <= td_root) BUG();
	accept_fd = c;
	printc("accept_fd %d (eid %d)\n", accept_fd, eid);
	evt_add(c, eid);

	/* event loop... */
	while (1) {
		int t;
		long evt;
		
		evt = evt_wait_all();
		t   = evt_torrent(evt);
		printc("an interrupt comes in (thd %d, evt %d t %d)\n",
		       cos_get_thd_id(), evt, t);
		accept_new(accept_fd);
		break;
	}
}

