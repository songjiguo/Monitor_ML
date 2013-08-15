INTERFACES=sched sched_hier
DEPENDENCIES=printc sched_hier recovery_upcall mem_mgr ll_log valloc_local

include ../Makefile.subdir
ifeq (${ENABLE_STACK_MANAGER},1)
 MANDITORY_LIB=simple_stklib.o
endif
