C_OBJS=noise.o
ASM_OBJS=
COMPONENT=noise_c.o
INTERFACES=
DEPENDENCIES=lock sched printc mem_mgr timed_blk periodic_wake ll_log

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
