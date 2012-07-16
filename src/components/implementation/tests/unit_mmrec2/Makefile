C_OBJS=memory_rec2.o
ASM_OBJS=
COMPONENT=mm_rec2.o
INTERFACES=
DEPENDENCIES=sched printc mem_mgr
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
