C_OBJS=pgfault.o
ASM_OBJS=
COMPONENT=pfr.o
INTERFACES=pgfault
DEPENDENCIES=sched printc failure_notif lock mem_mgr_large valloc_local ll_log
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
