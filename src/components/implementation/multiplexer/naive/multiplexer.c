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

#include <multiplexer.h>
#include <filter.h>
#include <util.h>

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

/* Here we can process each event that is copied from SMC and
 * put into each shared buffer for different event stream,
 * which needs some protocol. For now, for simplicity assume
 * only one stream. */
static void
prepare_for_stream(struct mpsmc_entry *mpsmcevt)
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

	// check each entry and update the information interested
	cra_constraint_check(entry);

	return;
}

static struct mlmp_thdevtseq_entry *
update_stream(struct thd_trace *ttc, unsigned long long ts)
{
	unsigned int tail;
	struct mlmp_thdevtseq_entry *mlmpevtseq;
	
	tail = mlmpthdevtseq_ring->p_tail;
	mlmpevtseq = (struct mlmp_thdevtseq_entry *) 
		CK_RING_GETTAIL_EVT(mlmpthdevtseqbuffer_ring,									 mlmpthdevtseq_ring, tail);
	assert(mlmpevtseq);
	mlmpevtseq->time_stamp = ts;
	mlmpevtseq->thd_id = ttc->thd_id;
	mlmpthdevtseq_ring->p_tail = tail+1;
	return mlmpevtseq;
}


/* main function to multiplexing the information. Assume that
 * prepare_for_stream has already prepared the information for each
 * stream. TODO: prepare the information in the way that each stream
 * can provide the correct information asked by the inquiry */
static void
stream_event_info(int streams)
{
	/* printc("thread %d is doing streaming copy\n", cos_get_thd_id()); */
        struct logmon_info *spdmon;
	struct mlmp_entry *mlmpevt = NULL;
	struct mlmp_thdevtseq_entry *mlmpevtseq;
	struct thd_trace *ttc;
	struct thd_specs *ttc_spec;
	int i, j;

	if (streams & STREAM_SPD_EXEC_TIMING) {
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
	}

	if (streams & STREAM_THD_EXEC_TIMING) {
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
	}

	if (streams & STREAM_SPD_INVOCATIONS) {
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
	}

	if (streams & STREAM_THD_INTERACTION) {
		printc("\n<<< event stream: interrupts  >>>\n");
		for (i = 0; i < TRACK_INTS; i++) {
			if (!thd_ints[i].ts) continue;
			printc("thd %d is interrupted by thd %d in spd %d (@ %llu)\n",
			       thd_ints[i].curr_thd, thd_ints[i].int_thd,
			       thd_ints[i].int_spd, thd_ints[i].ts);
		}
		for (i = 0; i < TRACK_INTS; i++) {   // reset
			thd_ints[track_ints_num].curr_thd = 0;
			thd_ints[track_ints_num].int_spd = 0;
			thd_ints[track_ints_num].int_thd = 0;
			thd_ints[track_ints_num].ts = 0;
			track_ints_num = 0;
		}
	}

	if (streams & STREAM_THD_CONTEX_SWCH) {
		printc("\n<<< event stream: context switch  >>>\n");
		for (i = 0; i < TRACK_CS; i++) {
			if (!thd_cs[i].ts) continue;
			printc("context switch from thd %d to %d (@ %llu)\n",
			       thd_cs[i].from_thd, thd_cs[i].to_thd, thd_cs[i].ts);
		}
		for (i = 0; i < TRACK_CS; i++) {   // reset
			thd_cs[track_cs_num].from_thd = 0;
			thd_cs[track_cs_num].to_thd   = 0;
			track_cs_num = 0;
		}
	}


	if ((streams & STREAM_THD_EVT_SEQUENC)) {
		printc("\n<<< event stream: component IPC sequence >>>\n");
		for (i = 0; i < MAX_NUM_THREADS; i++) {
			ttc = &thd_trace[i];
			assert(ttc);
			if (!ttc->exe_stack[0].ts) continue;  // no event for this thread
			printc("\nthd %d execution stack ---> \n", ttc->thd_id);

			for (j = 0; j < TRACK_SPDS; j++) {
				if (!ttc->exe_stack[j].ts) break;
				if (ttc->exe_stack[j].inv_from_spd) {
					mlmpevtseq = update_stream(ttc, ttc->exe_stack[j].ts);
					mlmpevtseq->inv_from_spd = ttc->exe_stack[j].inv_from_spd;
				}
				if (ttc->exe_stack[j].inv_into_spd) {
					mlmpevtseq = update_stream(ttc, ttc->exe_stack[j].ts);
					mlmpevtseq->inv_into_spd = ttc->exe_stack[j].inv_into_spd;
				}
				if (ttc->exe_stack[j].ret_from_spd) {
					mlmpevtseq = update_stream(ttc, ttc->exe_stack[j].ts);
					mlmpevtseq->ret_from_spd = ttc->exe_stack[j].ret_from_spd;
				}
				if (ttc->exe_stack[j].ret_back_spd) {
					mlmpevtseq = update_stream(ttc, ttc->exe_stack[j].ts);
					mlmpevtseq->ret_back_spd = ttc->exe_stack[j].ret_back_spd;
				}
				/* printc("@ %llu\n", ttc->exe_stack[j].ts); */
			}
		}
		for (i = 0; i < MAX_NUM_THREADS; i++) {   // reset
			ttc = &thd_trace[i];
			assert(ttc);
			for (j = 0; j < TRACK_SPDS; j++) {
				ttc->exe_stack[j].inv_from_spd = 0;
				ttc->exe_stack[j].inv_into_spd = 0;
				ttc->exe_stack[j].ret_from_spd = 0;
				ttc->exe_stack[j].ret_back_spd = 0;
				ttc->exe_stack[j].ts = 0;
			}
			thd_trace[i].track_exe_stack_num = 0;
		}
	}

	if ((streams & STREAM_TEST)) {
		
	}

	return;
}

static int
multiplexer_process(int streams)
{
	struct mpsmc_entry mpsmcentry;

	while((CK_RING_DEQUEUE_SPSC(mpsmcbuffer_ring, mpsmc_ring, &mpsmcentry))) {
		prepare_for_stream(&mpsmcentry);
		/* print_mpsmcentry_info(&mpsmcentry); */
	}
	
	stream_event_info(streams);

	return 0;
}

// called from ML component to get events for different stream. Now it
// is only one stream -- filter events to mlmpbuffer_ring
int
multiplexer_retrieve_data(spdid_t spdid, int streams)
{
	/* printc("fkml thd %d is retrieving event stream....\n", */
	/*        cos_get_thd_id()); */

#ifdef STREAM_PROCESS_OPTION_1
	multiplexer_process(streams);	
#endif
	return 0;
}

static void
emp_get_pages(int npages, char *addr)
{
	assert(addr);
	while(npages--) {
		if ((vaddr_t)addr != 
		    mman_get_page(cos_spd_id(), (vaddr_t)addr, 0)) {
			printc("fail to get a page in logger\n");
			assert(0);
		}
		assert(addr);
		addr += PAGE_SIZE;
	}
}

static void
emp_alias_pages(int npages, int spdid, char *s_addr, vaddr_t d_addr)
{
	int tmp = npages;
	char *_addr = s_addr;
	
	assert(npages >= 1);
	assert(s_addr && d_addr);

	while(tmp--) {
		/* printc("mlmp: try to alias,  %d pages left\n", tmp); */
		if (unlikely(d_addr != mman_alias_page(cos_spd_id(), (vaddr_t)_addr, 
						       spdid, d_addr))) {
			printc("alias rings %d failed.\n", spdid);
			assert(0);
		}
		if (tmp) {
			d_addr   += PAGE_SIZE;
			_addr    += PAGE_SIZE;
		}
	}
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

	emp_get_pages(npages, addr);

	switch (stream_type) {
	case STREAM_THD_EVT_SEQUENC:
		printc("init event sequence stream!!!!\n");

		assert(!mlmpthdevtseq_ring);
		mlmpthdevtseq_ring = (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)addr;
		
		emp_alias_pages(npages, spdid, addr, cli_ring);
		
		CK_RING_INIT(mlmpthdevtseqbuffer_ring,
			     (CK_RING_INSTANCE(mlmpthdevtseqbuffer_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mlmpthdevtseqbuffer_ring))/sizeof(struct mlmp_thdevtseq_entry)));
		break;
	case STREAM_THD_EXEC_TIMING:
		break;
	case STREAM_THD_INTERACTION:
		break;
	case STREAM_THD_CONTEX_SWCH:
		break;
	case STREAM_SPD_INVOCATIONS:
		break;
	case STREAM_SPD_EXEC_TIMING:
		break;
	case STREAM_TEST:
		printc("init test stream!!!!\n");

		assert(!mlmp_ring);
		mlmp_ring = (CK_RING_INSTANCE(mlmpbuffer_ring) *)addr;

		emp_alias_pages(npages, spdid, addr, cli_ring);
		
		CK_RING_INIT(mlmpbuffer_ring,
			     (CK_RING_INSTANCE(mlmpbuffer_ring) *)((void *)addr), NULL,
			     get_powerOf2((PAGE_SIZE*npages - sizeof(struct ck_ring_mlmpbuffer_ring))/sizeof(struct mlmp_entry)));
		break;
	default:
		assert(0);
	}
	
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
