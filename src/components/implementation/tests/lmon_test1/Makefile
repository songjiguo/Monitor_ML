C_OBJS=logmon_test1.o
ASM_OBJS=
COMPONENT=lmon1.o
INTERFACES=
DEPENDENCIES=periodic_wake sched printc lock mem_mgr lmon_test2 timed_blk logmonitor
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
