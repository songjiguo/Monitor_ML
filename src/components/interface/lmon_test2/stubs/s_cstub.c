#include <cos_component.h>
#include <print.h>

#include <lmon_test2.h>

static unsigned int first = 0;

extern vaddr_t lm_init(spdid_t spdid);

vaddr_t __sg_lmon_test2(spdid_t spdid)
{
	printc("\n ser (spd %ld) interface\n\n", cos_spd_id());
	if (first == 0) {
		lm_init(cos_spd_id());
		first = 1;
	}
	return lmon_test2();
}

