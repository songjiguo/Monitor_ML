C_OBJS=multiplexer.o
ASM_OBJS=
COMPONENT=cmultiplexer.o
INTERFACES=multiplexer
DEPENDENCIES=periodic_wake timed_blk sched printc lock mem_mgr_large valloc cinfo mem_pool ll_log 
ADDITIONAL_LIBS=-lheap

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
