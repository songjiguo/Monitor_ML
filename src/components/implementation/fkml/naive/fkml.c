/* Jiguo: fake ML component */

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>
#include <periodic_wake.h>
#include <timed_blk.h>
#include <mem_mgr_large.h>
#include <valloc.h>
#include <log.h>
#include <ck_ring_cos.h>

#define BUFF_SZ 32   // number of pages for the RB
int fkml_thd;

static void
init_ml_rb()
{
	char *addr;

	addr = (char *)valloc_alloc(cos_spd_id(), cos_spd_id(), BUFF_SZ);
	assert(addr);
	printc("fkml addr %p \n", addr);
	if (!(ml_ring = (CK_RING_INSTANCE(mlbuffer_ring) *)(llog_init(cos_spd_id(), (vaddr_t) addr, BUFF_SZ, 1)))) BUG();
	
	int capacity = CK_RING_CAPACITY(mlbuffer_ring, (CK_RING_INSTANCE(mlbuffer_ring) *)((void *)ml_ring));
	printc("the fkml shared buffer_ring cap is %d\n", capacity);
	return;
}

/* Here the fkml thread will do 2 things: 1) call SMC to set up the
   shared buffer between ML and SMC 2) periodically process the
   events */
static int
fkml_process()
{
	printc("fkml process\n");
	unsigned int tail;
	struct ml_entry entry;
	CK_RING_DEQUEUE_SPSC(mlbuffer_ring, ml_ring, &entry);
	print_mlentry_info(&entry);

	/* int test_num = 0; */
	/* while(test_num++ < 10) { */
	/* 	// start ML processing here */
	/* 	printc("fkml thread %d is doing ML now....\n", cos_get_thd_id()); */
	/* 	tail = ml_ring->p_tail; */
	/* 	entry = (struct ml_entry *)  */
	/* 		CK_RING_GETTAIL_EVT(mlbuffer_ring, ml_ring, tail); */
	/* 	if (!entry) break; */
	/* 	print_mlentry_info(entry); */
	/* } */

	printc("fkml process done!\n");
	return 0;
}

static unsigned long long sum_test = 0;

void 
cos_init(void *d)
{
	static int first = 0, flag = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 3;
		fkml_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (cos_get_thd_id() == fkml_thd) {
			
			/* initialize the shared buffer between SMC and ML */
			init_ml_rb();

			periodic_wake_create(cos_spd_id(), 54);
			while(1){
				periodic_wake_wait(cos_spd_id());
				printc("periodic process fkml....(thd %d)\n", 
				       cos_get_thd_id());
				llog_fkml_retrieve_data(cos_spd_id());
				fkml_process();
				printc("periodic process fkml done!....(thd %d)\n", 
				       cos_get_thd_id());
			}			
		}
	}
	return;
}
