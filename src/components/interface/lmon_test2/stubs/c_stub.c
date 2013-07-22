#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <lmon_test2.h>
#include <cstub.h>

#include <stdint.h>
#include <ck_ring_cos.h>

struct tr_event
{
	int thd_id;
	spdid_t src_spd;
	spdid_t des_spd;
	char* event_name;
	unsigned long long time_stamp;
};

struct ck_ring *ring;
unsigned int size = 4096;  // a page

static unsigned int first = 0;

extern vaddr_t lm_init(spdid_t spdid);

CSTUB_FN_ARGS_1(int, lmon_test2, spdid_t, spdid)
printc("\n cli (spd %ld) interface\n\n", cos_spd_id());
if (first == 0) {
	/* ck_ring_init(ring, NULL, size); */
	lm_init(cos_spd_id());
	
	first = 1;
}
CSTUB_ASM_1(lmon_test2, spdid)

CSTUB_POST

