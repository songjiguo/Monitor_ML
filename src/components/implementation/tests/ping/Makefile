C_OBJS=ping.o
ASM_OBJS=
COMPONENT=pi.o
INTERFACES=
DEPENDENCIES=sched printc lock mem_mgr pong lock timed_blk pgfault
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
