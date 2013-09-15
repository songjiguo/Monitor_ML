C_OBJS=ll_logmon.o
ASM_OBJS=
COMPONENT=lllog.o
INTERFACES=sched_hier ll_log
DEPENDENCIES=printc sched_hier recovery_upcall
FN_PREPEND=parent_
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
