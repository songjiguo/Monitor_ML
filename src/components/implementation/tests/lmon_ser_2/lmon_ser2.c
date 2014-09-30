#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>
#include <log.h>

#define PAGE_NUM 10

vaddr_t addr[PAGE_NUM];
static int idx = 0;

vaddr_t lmon_ser2_test(void)
{
	/* return (vaddr_t)cos_get_vas_page(); */
	vaddr_t ret;
	addr[idx]  = (vaddr_t)cos_get_vas_page();
	ret = addr[idx];
	idx++;
	return ret;
}

