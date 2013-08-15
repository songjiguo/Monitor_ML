C_OBJS=logmon_ser2.o
ASM_OBJS=
COMPONENT=lmonser2.o
INTERFACES=lmon_ser2
DEPENDENCIES=sched printc lock mem_mgr logmonitor ll_log valloc_local
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
