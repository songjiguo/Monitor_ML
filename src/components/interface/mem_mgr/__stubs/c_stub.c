#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#include <mem_mgr.h>
#include <cstub.h>

#include <log.h>

CSTUB_FN_ARGS_3(vaddr_t, mman_get_page, spdid_t, spdid, vaddr_t, addr, int, flags)

CSTUB_ASM_3(mman_get_page, spdid, addr, flags)

CSTUB_POST


CSTUB_FN_ARGS_4(vaddr_t, mman_alias_page, spdid_t, s_spd, vaddr_t, s_addr, spdid_t, d_spd, vaddr_t, d_addr)

CSTUB_ASM_4(mman_alias_page, s_spd, s_addr, d_spd, d_addr)

CSTUB_POST



CSTUB_FN_ARGS_3(int, mman_revoke_page, spdid_t, spdid, vaddr_t, addr, int, flags)

CSTUB_ASM_3(mman_revoke_page, spdid, addr, flags)

CSTUB_POST
