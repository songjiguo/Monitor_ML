C_OBJS=logmon_ser1.o
ASM_OBJS=
COMPONENT=lmonser1.o
INTERFACES=lmon_ser1
DEPENDENCIES=sched printc evt lock mem_mgr timed_blk lmon_ser2 ll_log  periodic_wake
IF_LIB=$(DIETDIR)/bin-i386/libm.a

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
