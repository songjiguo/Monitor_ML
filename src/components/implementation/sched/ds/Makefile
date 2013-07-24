C_OBJS=deferrable.o
ASM_OBJS=
COMPONENT=ds.o
INTERFACES=sched sched_hier log_publisher
DEPENDENCIES=printc sched_hier recovery_upcall mem_mgr
IF_LIB=../complib.o
FN_PREPEND=parent_

include Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
