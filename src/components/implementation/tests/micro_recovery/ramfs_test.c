#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

/* based on the code from unit test for ramfs */

#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

char buffer[1024];

void cos_init(void)
{
	td_t t1, t2, t0;
	long evt1, evt2, evt0;
	char *params0 = "who";
	char *params1 = "bar/";
	char *params2 = "foo/";
	char *params3 = "foo/bar/who";
	char *data1 = "1234567890", *data2 = "asdf;lkj", *data3 = "asdf;lkj1234567890";
	char *data0 = "kevinandy";
	unsigned int ret1, ret2, ret0;

	printc("<<< TEST START >>>\n");

	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());
	evt0 = evt_create(cos_spd_id());
	assert(evt1 > 0 && evt2 > 0 && evt0 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("  split2 failed %d\n", t1); return;
	}

	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	if (t2 < 1) {
		printc("  split1 failed %d\n", t2); return;
	}

	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t0 < 1) {
		printc("  split0 failed %d\n", t0); return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));
	/* ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
	ret2 = twrite_pack(cos_spd_id(), t2, data2, strlen(data2));
	ret0 = twrite_pack(cos_spd_id(), t0, data0, strlen(data0));
	/* printv("write %d & %d, ret %d & %d\n", strlen(data1), strlen(data2), ret1, ret2); */

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);
	/* printc("  2 split -> 2 write -> 2 release\n"); */

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2), TOR_ALL, evt1);
	t2 = tsplit(cos_spd_id(), t1, params1, strlen(params1), TOR_ALL, evt2);
	t0 = tsplit(cos_spd_id(), t2, params0, strlen(params0), TOR_ALL, evt0);
	if (t1 < 1 || t2 < 1 || t0 < 1) {
		printc("later splits failed\n");
		return;
	}
	printc("t1 tid %d\n", t1);
	printc("t2 tid %d\n", t2);
	printc("t0 tid %d\n", t0);
	printc("thread %ld\n", cos_get_thd_id());

	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023);
	if (ret1 > 0) buffer[ret1] = '\0';
	printv("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1);
	assert(!strcmp(buffer, data1));
	assert(ret1 == strlen(data1));
	buffer[0] = '\0';
	
	ret2 = tread_pack(cos_spd_id(), t2, buffer, 1023);
	if (ret2 > 0) buffer[ret2] = '\0';
	assert(!strcmp(buffer, data2));
	assert(ret2 == strlen(data2));
	printv("read %d: %s\n", ret2, buffer);
	buffer[0] = '\0';

	ret0 = tread_pack(cos_spd_id(), t0, buffer, 1023);
	if (ret0 > 0) buffer[ret0] = '\0';
	assert(!strcmp(buffer, data0));
	assert(ret0 == strlen(data0));
	printv("read %d: %s\n", ret0, buffer);
	buffer[0] = '\0';

	trelease(cos_spd_id(), t1);
	trelease(cos_spd_id(), t2);
	trelease(cos_spd_id(), t0);

	printc("<<< TEST PASSED >>>\n");

	return;
}
