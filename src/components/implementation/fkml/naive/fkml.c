/* Jiguo: fake ML component */
// TODO: move this to the non-interface

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

#include <log.h>
#include <multiplexer.h>

/* the stream type is defined in multiplexer interface
 * (see mutiplexer.h) */

unsigned int streams = 
	STREAM_THD_EVT_SEQUENC | 
	STREAM_TEST;

static int
fkml_process(int streams)
{
	/* printc("fkml process (thd %d)\n", cos_get_thd_id()); */

	/* printc("[[[ test info ]]]\n"); */
	if ((streams & STREAM_TEST)) {
		struct mlmp_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpbuffer_ring, mlmp_ring, &mlmpentry))) {
			print_mlmpentry_info(&mlmpentry);
		}
	}
	       
	if ((streams & STREAM_THD_EVT_SEQUENC)) {
		printc("[[[ Event sequence info ]]]\n");
		struct mlmp_thdevtseq_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdevtseqbuffer_ring, 
					    mlmpthdevtseq_ring, &mlmpentry))) {
			print_mlmpthdevtseq_info(&mlmpentry);
		}
	}

	if (streams & STREAM_THD_EXEC_TIMING) {
		printc("[[[ Thread timing info ]]]\n");
		struct mlmp_thdtime_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdtimebuffer_ring, 
					    mlmpthdtime_ring, &mlmpentry))) {
			print_mlmpthdtime_info(&mlmpentry);
		}
		
	}

	if (streams & STREAM_THD_INTERACTION) {
		printc("[[[ Thread interrupts info ]]]\n");
		struct mlmp_thdevtseq_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdevtseqbuffer_ring, 
					    mlmpthdevtseq_ring, &mlmpentry))) {
			print_mlmpthdevtseq_info(&mlmpentry);
		}
	}

	if (streams & STREAM_THD_CONTEX_SWCH) {
		printc("[[[ Thread context switch info ]]]\n");
		struct mlmp_thdevtseq_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdevtseqbuffer_ring, 
					    mlmpthdevtseq_ring, &mlmpentry))) {
			print_mlmpthdevtseq_info(&mlmpentry);
		}
	}

	if (streams & STREAM_SPD_INVOCATIONS) {
		printc("[[[ Component invocations info ]]]\n");
		struct mlmp_thdevtseq_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdevtseqbuffer_ring, 
					    mlmpthdevtseq_ring, &mlmpentry))) {
			print_mlmpthdevtseq_info(&mlmpentry);
		}
	}

	if (streams & STREAM_SPD_EXEC_TIMING) {
		printc("[[[ Component execution info ]]]\n");
		struct mlmp_thdevtseq_entry mlmpentry;
		while((CK_RING_DEQUEUE_SPSC(mlmpthdevtseqbuffer_ring, 
					    mlmpthdevtseq_ring, &mlmpentry))) {
			print_mlmpthdevtseq_info(&mlmpentry);
		}
	}

	printc("fkml process done! (thd %d)\n", cos_get_thd_id());

	return 0;
}

static char *
_init_rb_mlmp(int buffer_size)
{
	char *addr = NULL;
	addr = cos_get_heap_ptr();
	if (!addr) {
		printc("fail to allocate pages from the heap\n");
		assert(0);
	}
	printc("set up rb: ML -- MP @addr %p \n", addr);
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE*buffer_size));
	printc("set up rb: ML -- MP @addr %p done! \n", addr);

	return addr;
}

static void
init_rb_mlmp(int streams)
{
	char *addr;

	if ((streams & STREAM_TEST)) {
		addr = _init_rb_mlmp(STREAM_TEST_BUFF);
		assert(addr);
		if (!(mlmp_ring = (CK_RING_INSTANCE(mlmpbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr, 
					STREAM_TEST_BUFF, 
					STREAM_TEST)))) BUG();
		assert(mlmp_ring == 
		       (CK_RING_INSTANCE(mlmpbuffer_ring) *)addr);
	}
	
	if ((streams & STREAM_THD_EVT_SEQUENC)) {
		addr = _init_rb_mlmp(STREAM_THD_EVT_SEQUENC_BUFF);
		assert(addr);
		if (!(mlmpthdevtseq_ring = 
		      (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_THD_EVT_SEQUENC_BUFF, 
					STREAM_THD_EVT_SEQUENC)))) BUG();
		assert(mlmpthdevtseq_ring == 
		       (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)addr);
	} 

	if (streams & STREAM_THD_EXEC_TIMING) {
		addr = _init_rb_mlmp(STREAM_THD_EXEC_TIMING);
		assert(addr);
		if (!(mlmpthdtime_ring = 
		      (CK_RING_INSTANCE(mlmpthdtimebuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_THD_EXEC_TIMING_BUFF, 
					STREAM_THD_EXEC_TIMING)))) BUG();
		assert(mlmpthdtime_ring== 
		       (CK_RING_INSTANCE(mlmpthdtimebuffer_ring) *)addr);
	}
	
	if (streams & STREAM_THD_INTERACTION) {
		addr = _init_rb_mlmp(STREAM_THD_INTERACTION_BUFF);
		assert(addr);
		if (!(mlmpthdinteract_ring = 
		      (CK_RING_INSTANCE(mlmpthdinteractbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_THD_INTERACTION_BUFF, 
					STREAM_THD_INTERACTION)))) BUG();
		assert(mlmpthdinteract_ring == 
		       (CK_RING_INSTANCE(mlmpthdinteractbuffer_ring) *)addr);
	}

	if (streams & STREAM_THD_CONTEX_SWCH) {
		addr = _init_rb_mlmp(STREAM_THD_CONTEX_SWCH_BUFF);
		assert(addr);
		if (!(mlmpthdcs_ring = 
		      (CK_RING_INSTANCE(mlmpthdcsbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_THD_CONTEX_SWCH_BUFF, 
					STREAM_THD_CONTEX_SWCH)))) BUG();
		assert(mlmpthdcs_ring == 
		       (CK_RING_INSTANCE(mlmpthdcsbuffer_ring) *)addr);
	}

	if (streams & STREAM_SPD_INVOCATIONS) {
		addr = _init_rb_mlmp(STREAM_SPD_INVOCATIONS_BUFF);
		assert(addr);
		if (!(mlmpspdinvnum_ring = 
		      (CK_RING_INSTANCE(mlmpspdinvnumbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_SPD_INVOCATIONS_BUFF, 
					STREAM_SPD_INVOCATIONS)))) BUG();
		assert(mlmpspdinvnum_ring == 
		       (CK_RING_INSTANCE(mlmpspdinvnumbuffer_ring) *)addr);
	}

	if (streams & STREAM_SPD_EXEC_TIMING) {
		addr = _init_rb_mlmp(STREAM_SPD_EXEC_TIMING_BUFF);
		assert(addr);
		if (!(mlmpspdexec_ring = 
		      (CK_RING_INSTANCE(mlmpspdexecbuffer_ring) *)
		      (multiplexer_init(cos_spd_id(), (vaddr_t) addr,
					STREAM_SPD_EXEC_TIMING_BUFF, 
					STREAM_SPD_EXEC_TIMING)))) BUG();
		assert(mlmpspdexec_ring == 
		       (CK_RING_INSTANCE(mlmpspdexecbuffer_ring) *)addr);
	}
	
	return;
}

int fkml_thd = 0;

void 
cos_init(void *d)
{
	static int first = 0, flag = 0;
	union sched_param sp;

        /* The fkml thread will do 2 things: 
	   1) set up the shared buffer between ML and multiplexer
	   2) periodically retrieve event stream */

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 3;
		fkml_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		//printc("fkml thread is created as thd %d\n", fkml_thd);
	} else {
		if (cos_get_thd_id() == fkml_thd) {
			/* return;   // debug network, so disable this for now */
			/* printc("fkml thread is running %d\n", cos_get_thd_id()); */
			
                        /* initialize the shared stream buffers
			 * between EMP and ML */
			init_rb_mlmp(streams);

			periodic_wake_create(cos_spd_id(), FML_PERIOD);
			while(1){
				periodic_wake_wait(cos_spd_id());
				printc("PERIODIC: fkml....(thd %d)\n", cos_get_thd_id());
				multiplexer_retrieve_data(cos_spd_id(), streams);
				fkml_process(streams);
			}			
		}
	}
	return;
}
