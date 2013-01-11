#include <cos_component.h>
#include <mem_mgr.h>
#include <print.h>

vaddr_t __sg_mman_get_page(spdid_t spdid, vaddr_t addr, int flags)
{
	return mman_get_page(spdid, addr, flags);
}

vaddr_t __sg_mman_alias_page(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	return mman_alias_page(s_spd, s_addr, d_spd, d_addr);
}

vaddr_t __sg_mman_alias_page2(spdid_t s_spd, vaddr_t s_addr, spdid_t d_spd, vaddr_t d_addr)
{
	return __mman_alias_page_rec(s_spd, s_addr, d_spd, d_addr);
}

int __sg_mman_revoke_page(spdid_t spd, vaddr_t addr, int flags)
{
	int ret = 0;

	if (flags == 1) ret = __mman_revoke_page(spd, addr, flags);
	else ret = mman_revoke_page(spd, addr, flags);

	return ret;
}

/* int __sg_mman_release_page(spdid_t spd, vaddr_t addr, int flags) */
/* { */
/* 	int ret = 0; */

/* 	if (flags == 1) ret = __mman_release_page(spd, addr, flags); */
/* 	else ret = mman_release_page(spd, addr, flags); */

/* 	return ret; */
/* } */
