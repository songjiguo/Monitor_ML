C_OBJS=pid.o
ASM_OBJS=
COMPONENT=cpid.o
INTERFACES=
DEPENDENCIES=torrent printc mem_mgr_large sched valloc cbufp cbuf_c evt periodic_wake timed_blk lock ll_log
IF_LIB=
FN_PREPEND=from_

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
