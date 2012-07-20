C_OBJS=memory_rec1.o
ASM_OBJS=
COMPONENT=mm_rec1.o
INTERFACES=
DEPENDENCIES=sched printc lock mem_mgr valloc unit_mmrec2 unit_mmrec3
IF_LIB=

include ../../Makefile.subsubdir
ifeq (${ENABLE_STACK_MANAGER},1)
# $(info Using Simple Stack)
 MANDITORY_LIB=simple_stklib.o
endif
