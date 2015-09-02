/* Jiguo: intermediate event stream multiplexer component 
   this component will do the followings:
   1) periodically retrieve the event data from the SMC
   2) preprocess the events (similar logic in the SMC, e.g., per
      thread, per spd....)multiplexing the events into different
      stream (different buffer and API protocol with fkML) */


// Note: 2 options to preprocess events (both periodically)
//  1) stream process when ML asks for the data from MP
//  2) stream process when MP asks for the data from SMC

#define STREAM_PROCESS_OPTION_1
//#define STREAM_PROCESS_OPTION_2

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
#include <ll_log.h>
#include <log.h>
#include <ck_ring_cos.h>

#include <filter.h>
#include <filter_util.h>

#define MULTIPLEXER_BUFF_SZ 32   // number of pages for the RB
int multiplexer_thd;

// statically allocate the buffer for sharing 
//char mpsmc_buffer[MULTIPLEXER_BUFF_SZ*PAGE_SIZE];

/* ask to set up the ring buffer between the MP (this component) and the SMC */
static void
init_rb_mpsmc()
{
	char *addr;

	addr = cos_get_heap_ptr();
	if (!addr) {
		printc("fail to allocate pages from the heap\n");
		assert(0);
	}
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE*MULTIPLEXER_BUFF_SZ));

	printc("set up rb: MP -- SMC @addr %p \n", addr);
	if (!(mpsmc_ring = (CK_RING_INSTANCE(mpsmcbuffer_ring) *)(llog_multiplexer_init(cos_spd_id(), (vaddr_t) addr, MULTIPLEXER_BUFF_SZ)))) BUG();
	assert(mpsmc_ring == (CK_RING_INSTANCE(mpsmcbuffer_ring) *)addr);
	printc("set up rb: MP -- SMC @addr %p done! \n", addr);
	return;
}

/* compute the highest power of 2 less or equal than 32-bit v */
static unsigned int get_powerOf2(unsigned int orig) {
        unsigned int v = orig - 1;

        v |= v >> 1;
        v |= v >> 2;
        v |= v >> 4;
        v |= v >> 8;
        v |= v >> 16;
        v++;

        return (v == orig) ? v : v >> 1;
}

//extern struct thd_trace thd_trace[MAX_NUM_THREADS];


/* Here we can process each event that is copied from SMC and
 * put into each shared buffer for different event stream,
 * which needs some protocol. For now, for simplicity assume
 * only one stream. */
static void
process_to_stream(struct mpsmc_entry *mpsmcevt)
{
	int i;
	assert(mpsmcevt);

	/* EMP needs to fill the period/priority into the data
	 * structure, this is because the value is only set in SMC
	 * (low level)*/
	static int getprioperiod_once = 0;
	if (!getprioperiod_once) {
		getprioperiod_once = 1;
		for (i = 1; i < MAX_NUM_THREADS+1; i++) {
			struct thd_trace *ttl = NULL;
			int prio, p;
			ttl = &thd_trace[i];
			if (!ttl) continue;
			prio = llog_getprio(cos_spd_id(), i);
			ttl->prio = prio;
			p = llog_getperiod(cos_spd_id(), i);
			ttl->p = p;
		}
	}

	struct evt_entry *entry = (struct evt_entry *)mpsmcevt;

	cra_constraint_check(entry);

	return;
}

/* main function to multiplexing the information */
static void
stream_event_info()
{
	/* printc("thread %d is doing streaming copy\n", cos_get_thd_id()); */
        struct logmon_info *spdmon;
	struct mlmp_entry *mlmpevt = NULL;
	struct thd_trace *ttc;
	struct thd_specs *ttc_spec;
	int i, j;

	printc("\n<<< event stream: max execution time in component >>>\n");
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		ttc = &thd_trace[i];
		assert(ttc);
		ttc_spec = &thd_specs[ttc->thd_id];
		assert(ttc_spec);
		/* print_thd_trace(ttc); */
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			if (ttc->exec_oneinv_in_spd[j]) {
				printc("thd %d max exec_oneinv_in_spd %llu in spd %d\n",
				       ttc->thd_id, ttc->exec_oneinv_in_spd[j], j);
			}
		}
	}

	printc("\n<<< event stream: execution time (since activation)  >>>\n");
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		ttc = &thd_trace[i];
		assert(ttc);
		ttc_spec = &thd_specs[ttc->thd_id];
		assert(ttc_spec);
		/* print_thd_trace(ttc); */
		if (ttc_spec->exec_max) {
			printc("thd %d max exec %llu (%llu)\n", ttc->thd_id, ttc_spec->exec_max, ttc->dl_s);
		}
	}


	printc("\n<<< event stream: component invocation numbers >>>\n");
	for (i = 0; i < MAX_NUM_THREADS; i++) {
		ttc = &thd_trace[i];
		assert(ttc);
		for (j = 0; j < MAX_NUM_SPDS; j++) {
			if (ttc->invocation[j]) {
				printc("thd %d invokes %d times to spd %d\n",
				       ttc->thd_id, ttc->invocation[j], j);
			}
		}
	}
	



	unsigned int tail = mlmp_ring->p_tail;
	mlmpevt = (struct mlmp_entry *) CK_RING_GETTAIL_EVT(mlmpbuffer_ring,
							    mlmp_ring, tail);
	assert(mlmpevt);
	//mlmpevent_copy(mlmpevt, &result);
	/* print_mlmpentry_info(mlmpevt); */

	/* do not increase the tail unless there is a copy */
	// mlmp_ring->p_tail = tail+1;


	/* /\* copy from the big buffer to different stream buffers *\/ */
	/* mlmpevt->trace_thd_info[0].thd_id = 10;  // test */
	/* mlmpevt->para1 = mpsmcevt->from_spd; */
	/* mlmpevt->para2 = mpsmcevt->to_spd; */
	/* mlmpevt->para3 = mpsmcevt->evt_type; */
	/* mlmpevt->time_stamp = mpsmcevt->time_stamp; */
	/* /\* print_mlmpentry_info(mlmpevt); *\/ */

	/* int capacity, size; */
	/* capacity = CK_RING_CAPACITY(mlmpbuffer_ring,  */
	/* 			    (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)mlmp_ring)); */
	/* size = CK_RING_SIZE(mlmpbuffer_ring,  */
	/* 		    (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)mlmp_ring)); */
	/* printc("current tail is %d\n", tail); */
	/* printc("current capacity is %d\n", capacity); */
	/* printc("current size is %d\n", size); */

	return;
}

static int
multiplexer_process()
{
	struct mpsmc_entry mpsmcentry;

	while((CK_RING_DEQUEUE_SPSC(mpsmcbuffer_ring, mpsmc_ring, &mpsmcentry))) {
		process_to_stream(&mpsmcentry);
		/* print_mpsmcentry_info(&mpsmcentry); */
	}
	
	stream_event_info();

	return 0;
}

// called from ML component to get events for different stream. Now it
// is only one stream -- filter events to mlmpbuffer_ring
int
multiplexer_retrieve_data(spdid_t spdid)
{
	/* printc("fkml thd %d is retrieving event stream....\n", */
	/*        cos_get_thd_id()); */

#ifdef STREAM_PROCESS_OPTION_1
	multiplexer_process();	
#endif
	return 0;
}

/* called from ML component to set up shared RB between MP and ML */
vaddr_t 
multiplexer_init(spdid_t spdid, vaddr_t cli_addr, int npages, int stream_type)
{
	vaddr_t ret = 0;

        vaddr_t log_ring, cli_ring = cli_addr;
	char *addr, *hp;
	
	assert(spdid && cli_addr);

	// not sure why this is not working, has to use get_page?
	/* addr = mpsmc_buffer; */
	/* addr = (char *)malloc(MULTIPLEXER_BUFF_SZ*PAGE_SIZE); */
	addr = cos_get_heap_ptr();
	assert(addr);
	cos_set_heap_ptr((void*)(((unsigned long)addr)+MULTIPLEXER_BUFF_SZ*PAGE_SIZE));
	int test = npages;
	char *test_addr = addr;
	while(test--) {
		if ((vaddr_t)test_addr != 
		    mman_get_page(cos_spd_id(), (vaddr_t)test_addr, 0)) {
			printc("fail to get a page in logger\n");
			assert(0);
		}
		assert(test_addr);
		test_addr += PAGE_SIZE;
	}
	assert(!mlmp_ring);
	mlmp_ring = (CK_RING_INSTANCE(mlmpbuffer_ring) *)addr;

	int tmp = npages;
	char *_addr = addr;
	while(tmp--) {
		/* printc("mlmp: try to alias,  %d pages left\n", tmp); */
		if (unlikely(cli_ring != mman_alias_page(cos_spd_id(), (vaddr_t)_addr, spdid, cli_ring))) {
			printc("alias rings %d failed.\n", spdid);
			assert(0);
		}
		if (tmp) {
			cli_ring += PAGE_SIZE;
			_addr    += PAGE_SIZE;
		}
	}

	CK_RING_INIT(mlmpbuffer_ring,
		     (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)addr), NULL,
		     get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mlmpbuffer_ring))/sizeof(struct mlmp_entry)));

	/* printc("\nmlmp: test the size %d\n", get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mlmpbuffer_ring))/sizeof(struct mlmp_entry))); */
	/* printc("PAGE_SIZE*npages %d\n",PAGE_SIZE*npages); */
	/* printc("sizeof ck_ring_mlmpbuffer_ring %d\n", */
	/*        sizeof(struct ck_ring_mlmpbuffer_ring)); */
	/* printc("sizeof mlmp_entry %d\n",sizeof(struct mlmp_entry)); */
	/* int capacity = CK_RING_CAPACITY(mlmpbuffer_ring, (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)mlmp_ring)); */
	/* printc("MP:the mlmp ring cap is %d\n\n", capacity); */

/* // debug */
/* 	struct ck_ring_mlmpbuffer_ring *test = (CK_RING_INSTANCE(mlmpbuffer_ring) *)addr; */
/* 	assert(test); */
/* 	unsigned int tail = test->p_tail; */
/* 	printc("current tail is %d\n", tail); */
/* 	unsigned int size = test->size; */
/* 	printc("current size is %d\n", size); */
/* 	struct mlmp_entry *mlmpevt = NULL; */
/* 	mlmpevt = (struct mlmp_entry *) CK_RING_GETTAIL_EVT(mlmpbuffer_ring, */
/* 							    mlmp_ring, tail); */
/* 	assert(mlmpevt); */
/* 	printc("done debug\n"); */
/* // end of debug */

	return cli_addr;
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
		multiplexer_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (cos_get_thd_id() == multiplexer_thd) {
			/* initialize the shared buffer between SMC and multiplexer */
			init_rb_mpsmc();

			init_log();

			periodic_wake_create(cos_spd_id(), 54);
			while(1){
				periodic_wake_wait(cos_spd_id());
				/* printc("periodic process multiplexer....(thd %d)\n",  */
				/*        cos_get_thd_id()); */
				llog_multiplexer_retrieve_data(cos_spd_id());
#ifdef STREAM_PROCESS_OPTION_2
				multiplexer_process();	
#endif
				/* printc("periodic process multiplexer done!....(thd %d)\n",
				   cos_get_thd_id()); */
			}			
		}
	}
	return;
}
