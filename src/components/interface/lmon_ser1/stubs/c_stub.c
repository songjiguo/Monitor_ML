#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <lmon_ser1.h>
#include <cstub.h>

#include <log.h>

CSTUB_FN_0(int, lmon_ser1_test)

/* #ifdef LOG_MONITOR */

/* int test = ((uc->cap_no << 16)|(cos_spd_id() & 0xFF)); */
/* printc("<< ser1 inter: uc->cap_no %x uc->cap_>> 20 %x >>\n", uc->cap_no, uc->cap_no >> 20); */
/* printc("<< ser1 inter: spd %x spd &  0xFF %x >>\n", cos_spd_id(), cos_spd_id() & 0xFF); */
/* printc("<< ser1 inter: %x >>\n", (uc->cap_no|(cos_spd_id() & 0xFF))); */

/* int dest; */
/* dest = cos_cap_cntl(COS_CAP_GET_SER_SPD, cos_spd_id(), 0, uc->cap_no); */
/* printc("ser1 inter: dest is %d\n", dest); */
/* // assume the spd ls less than 255 */
/* monevt_enqueue((uc->cap_no|(cos_spd_id() & 0xFF)),1, 0); */
/* #endif */

CSTUB_ASM_0(lmon_ser1_test)

CSTUB_POST
