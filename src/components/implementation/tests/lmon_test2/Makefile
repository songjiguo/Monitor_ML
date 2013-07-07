C_OBJS=logmon_test1.o
ASM_OBJS=
COMPONENT=lmon2.o
INTERFACES=lmon_test2
DEPENDENCIES=sched printc lock mem_mgr logmonitor
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
