#include <cos_component.h>
#include <mem_mgr.h>
#include <print.h>

#ifdef LOG_MONITOR
#include <log.h>
#endif

vaddr_t __sg_mman_get_page(spdid_t spdid, vaddr_t addr, int flags)
{
	int ret;

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_MM_GET, 0, EVT_SINV);
#endif
	ret = mman_get_page(spdid, addr, flags);

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), cos_spd_id(), spdid, FN_MM_GET, 0, EVT_SRET);
#endif
	return ret;
}

vaddr_t __sg_mman_alias_page(spdid_t spdid, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	int ret;

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_MM_ALIAS, 0, EVT_SINV);
#endif

	ret = mman_alias_page(spdid, s_addr, d_spd, d_addr);

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_MM_ALIAS, 0, EVT_SRET);
#endif

	return ret;
}

int __sg_mman_revoke_page(spdid_t spdid, vaddr_t addr, int flags)
{
	int ret;
	
#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_MM_REVOKE, 0, EVT_SINV);
#endif

	ret = mman_revoke_page(spdid, addr, flags);

#ifdef LOG_MONITOR
	evt_enqueue(cos_get_thd_id(), spdid, cos_spd_id(), FN_MM_REVOKE, 0, EVT_SRET);
#endif

	return ret;
}
