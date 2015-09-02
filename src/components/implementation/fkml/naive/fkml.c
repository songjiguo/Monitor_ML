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

#include <multiplexer.h>

// in pages
#define	STREAM_THD_EVT_SEQUENC_BUFF 32
#define	STREAM_THD_EXEC_TIMING_BUFF  32
#define	STREAM_THD_INTERACTION_BUFF  32
#define	STREAM_SPD_INVOCATIONS_BUFF  32
#define	STREAM_SPD_EVT_SEQUENC_BUFF 32

int streams = 0;   // diable different type for now

/* The fkml thread will do 2 things: 
   1) set up the shared buffer between ML and multiplexer
   2) periodically retrieve event stream */
int fkml_thd;

static int
fkml_process()
{
	/* printc("fkml process (thd %d)\n", cos_get_thd_id()); */

	/* unsigned int tail = mlmp_ring->p_tail; */
	/* printc("current tail is %d\n", tail); */
	/* unsigned int size = mlmp_ring->size; */
	/* printc("current size is %d\n", size); */
	/* int capacity = CK_RING_CAPACITY(mlmpbuffer_ring, (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)mlmp_ring)); */
	/* printc("the mlmp ring cap is %d\n", capacity); */

	/* struct mlmp_entry mlmpentry; */
	/* while((CK_RING_DEQUEUE_SPSC(mlmpbuffer_ring, mlmp_ring, &mlmpentry))) { */
	/* 	/\* print_mlmpentry_info(&mlmpentry); *\/ */
	/* } */
	/* printc("fkml process done! (thd %d)\n", cos_get_thd_id()); */

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
	/* printc("set up rb: ML -- MP @addr %p \n", addr); */
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE*buffer_size));

	return addr;
}

static void
init_rb_mlmp()
{
	char *addr;
	int buffer_size, stream_type;

	if (streams & STREAM_THD_EVT_SEQUENC) {
		printc("event sequence stream setup\n");
		buffer_size = STREAM_THD_EVT_SEQUENC_BUFF;
		stream_type = STREAM_THD_EVT_SEQUENC;
		addr = _init_rb_mlmp(buffer_size);
		assert(addr);
		if (!(mlmpthdevtseq_ring = (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)(multiplexer_init(cos_spd_id(), (vaddr_t) addr, buffer_size, stream_type)))) BUG();
		assert(mlmpthdevtseq_ring == (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)addr);
		/* int capacity = CK_RING_CAPACITY(mlmpbuffer_ring, (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)mlmp_ring)); */
		/* printc("ML:the mlmp ring cap is %d\n", capacity); */
		/* printc("set up rb: ML -- MP @addr %p done! \n", addr); */
	} 
	if (streams & STREAM_THD_EXEC_TIMING) {
		buffer_size = STREAM_THD_EXEC_TIMING_BUFF;
		stream_type = STREAM_THD_EXEC_TIMING;
	}

	if (streams & STREAM_THD_INTERACTION) {
		buffer_size = STREAM_THD_INTERACTION_BUFF;
		stream_type = STREAM_THD_INTERACTION;
	}

	if (streams & STREAM_SPD_INVOCATIONS) {
		buffer_size = STREAM_SPD_INVOCATIONS_BUFF;
		stream_type = STREAM_SPD_INVOCATIONS;
	}

	if (streams & STREAM_SPD_EVT_SEQUENC) {
		buffer_size = STREAM_SPD_EVT_SEQUENC_BUFF;
		stream_type = STREAM_SPD_EVT_SEQUENC;
	}

	addr = _init_rb_mlmp(32);
	assert(addr);
	if (!(mlmp_ring = (CK_RING_INSTANCE(mlmpbuffer_ring) *)(multiplexer_init(cos_spd_id(), (vaddr_t) addr, 32, stream_type)))) BUG();
	assert(mlmp_ring == (CK_RING_INSTANCE(mlmpbuffer_ring) *)addr);
	
	return;
}

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
			
			/* initialize the shared buffer between EMP and ML */
			init_rb_mlmp();

			periodic_wake_create(cos_spd_id(), 67);
			while(1){
				periodic_wake_wait(cos_spd_id());
				/* printc("periodic process fkml....(thd %d)\n",  */
				/*        cos_get_thd_id()); */
				// TODO: define the protocol for stream
				multiplexer_retrieve_data(cos_spd_id());
				fkml_process();
				/* printc("periodic process fkml done!....(thd %d)\n",  */
				/*        cos_get_thd_id()); */
				/* printc("\n\n"); */
			}			
		}
	}
	return;
}
