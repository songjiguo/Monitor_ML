#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

//#define VERBOSE 1
#ifdef VERBOSE
#define printv(fmt,...) printc(fmt, ##__VA_ARGS__)
#else
#define printv(fmt,...) 
#endif

char buffer[1024];

void cos_init(void)
{
	td_t t1;
	long evt1;
	char *params1 = "bar";
	char *data1 = "1234567890";
	unsigned int ret1;

	evt1 = evt_create(cos_spd_id());
	assert(evt1 > 0);

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("split failed %d\n", t1);
		return;
	}

	trelease(cos_spd_id(), t1);

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1);
	if (t1 < 1) {
		printc("split failed %d\n", t1);
		return;
	}

	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1));

	printv("write %d, ret %d\n", strlen(data1), ret1);

	trelease(cos_spd_id(), t1);

	return;
}
