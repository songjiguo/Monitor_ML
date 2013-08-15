C_OBJS=logmon_ser1.o
ASM_OBJS=
COMPONENT=lmonser1.o
INTERFACES=lmon_ser1
DEPENDENCIES=periodic_wake sched printc lock mem_mgr timed_blk logmonitor lmon_ser2 ll_log valloc_local
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
