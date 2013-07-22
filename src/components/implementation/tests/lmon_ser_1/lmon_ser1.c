#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <lmon_ser2.h>
#include <logmonitor.h>

vaddr_t lmon_ser1_test(void)
{
	lmon_ser2_test();
	return 0;
}

