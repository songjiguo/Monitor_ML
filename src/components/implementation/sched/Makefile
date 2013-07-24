INTERFACES=sched sched_hier log_publisher
DEPENDENCIES=printc sched_hier recovery_upcall mem_mgr

include ../Makefile.subdir
ifeq (${ENABLE_STACK_MANAGER},1)
 MANDITORY_LIB=simple_stklib.o
endif
