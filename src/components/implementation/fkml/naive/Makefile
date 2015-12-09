C_OBJS=fkml.o
ASM_OBJS=
COMPONENT=cfkml.o
INTERFACES=fkml
DEPENDENCIES=periodic_wake timed_blk sched printc lock mem_mgr_large valloc cinfo mem_pool multiplexer ll_log 
IF_LIB=$(DIETDIR)/bin-i386/libm.a

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
