#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <mem_mgr.h>

#include <periodic_wake.h>
#include <timed_blk.h>
#include <lmon_ser2.h>

int low;
int mid;
int hig;

volatile int spin = 1;
volatile int spin2 = 1;
unsigned long long start, end, sum;

#include <../lmon_cli_1/test_monitor.h>

#include <cos_synchronization.h>
cos_lock_t lock1, lock2;
#define LOCK1_TAKE()    lock_take(&lock1)
#define LOCK1_RELEASE() lock_release(&lock1)
#define LOCK1_INIT()    lock_static_init(&lock1)
#define LOCK2_TAKE()    lock_take(&lock2)
#define LOCK2_RELEASE() lock_release(&lock2)
#define LOCK2_INIT()    lock_static_init(&lock2)


static void local_spin()
{
	int i = 0;
	while(i++ < 1000);
	
	return;
}

/*******************************/
#ifdef NORMAL
vaddr_t lmon_ser1_test(void)
{
	lmon_ser2_test();
	return 0;
}
/*******************************/
#elif defined EXAMINE_PI
int try_cs_mp(void) 
{
	while(1) {
		periodic_wake_wait(cos_spd_id());
		/* printc("thread m : %d is doing something\n", cos_get_thd_id()); */
		timed_event_block(cos_spd_id(), 1);

		LOCK2_TAKE();
		/* printc("thread m : %d has the lock2\n", cos_get_thd_id()); */
		spin = 0;
		/* printc("thread m : %d try to take lock1\n", cos_get_thd_id()); */
		LOCK1_TAKE();
		/* printc("thread m : %d has the lock1\n", cos_get_thd_id()); */
		LOCK1_RELEASE();
		/* printc("thread m : %d released lock1\n", cos_get_thd_id()); */
		LOCK2_RELEASE();
		/* printc("thread m : %d released lock2\n", cos_get_thd_id()); */
	}
	return 0;
}

int try_cs_hp(void)
{
	int i;
	while(1) {
		/* periodic_wake_wait(cos_spd_id()); */
		/* printc("thread h : %d is doing something\n", cos_get_thd_id()); */
		timed_event_block(cos_spd_id(), 5);
		spin2 = 0;
		/* printc("thread h : %d try to take lock2\n", cos_get_thd_id()); */
		LOCK2_TAKE();
		/* printc("thread h : %d has the lock2\n", cos_get_thd_id()); */
		LOCK2_RELEASE();
		/* printc("thread h : %d released lock2\n", cos_get_thd_id()); */
	}
	return 0;
}


int try_cs_lp(void)
{
	volatile int jj, kk;
	while (1) {
		/* periodic_wake_wait(cos_spd_id()); */
		/* printc("<<< thread l : %d is doing something \n", cos_get_thd_id()); */
		/* printc("thread l : %d try to take lock1\n", cos_get_thd_id()); */
		LOCK1_TAKE();
		/* printc("thread l : %d has the lock1\n", cos_get_thd_id()); */
		/* printc("thread l : %d spin\n", cos_get_thd_id()); */
		spin = 1;
		while (spin);
		spin = 1;
		/* printc("thread l : %d after spin\n", cos_get_thd_id()); */
		jj = 0;
		kk = 0;
		while(jj++ < 10000){
			while(kk++ < 100000);
		}
		/* printc("thread l : %d spin2\n", cos_get_thd_id()); */
		spin2 = 1;
		while(spin2);
		spin2 = 1;
		/* printc("thread l : %d after spin2\n", cos_get_thd_id()); */
 		/* printc("thread l : %d release lock1\n", cos_get_thd_id()); */
		LOCK1_RELEASE();
	}
	return 0;
}

/**************************/
#elif defined EXAMINE_SCHED
unsigned long long kevin, andy;

int try_cs_hp(void)
{
	if (hig == 0) {
		hig = cos_get_thd_id();
		printc("<<<high thd %d -- SCHED test>>>\n", hig);
	}
	kevin = 0;
	while(kevin++ < 1000) {
		sched_block(cos_spd_id(), 0);
		local_spin();
	}

	return 0;
}

int try_cs_lp(void)
{
	if (low == 0) {
		low = cos_get_thd_id();
		printc("<<<low thd %d>>>\n", low);
	}
	andy = 0;
	while(andy++ < 1000) {
		sched_wakeup(cos_spd_id(), hig);
		local_spin();
	}

	return 0;
}

int try_cs_mp(void) { return 0;}
vaddr_t lmon_ser1_test(void) { return 0;}

/******************************/
#elif defined EXAMINE_MM
unsigned long long kevin, andy;

#define PAGE_NUM 10
vaddr_t s_addr[PAGE_NUM];
vaddr_t d_addr[PAGE_NUM];

int try_cs_hp(void)
{
	int i;
	if (hig == 0) {
		hig = cos_get_thd_id();
		printc("<<<high thd %d -- MM test>>>\n", hig);
	}

	kevin = 0;
	while(kevin++ < 10) {
		for (i = 0; i<PAGE_NUM; i++) {
			s_addr[i] = (vaddr_t)cos_get_vas_page();
			d_addr[i] = lmon_ser2_test();
			
			/* printc("s address -- %x\n", s_addr[i]); */
			/* printc("d address -- %x\n", d_addr[i]); */
		}

		// start testing here........
		i = 0;
		for (i = 0; i<PAGE_NUM; i++) {
			mman_get_page(cos_spd_id(), s_addr[i], 0);

			mman_alias_page(cos_spd_id(), s_addr[i], cos_spd_id()+1, d_addr[i]);
			mman_revoke_page(cos_spd_id(), s_addr[i], 0);
			/* rdtscll(end); */
			/* printc("grant-alias-revoke cost %llu\n", end - start); */
			/* mman_release_page(cos_spd_id(), s_addr[i], 0); */
		}

		local_spin();
	}

	return 0;
}

int try_cs_lp(void) { return 0; }
int try_cs_mp(void) { return 0;}
vaddr_t lmon_ser1_test(void) { return 0;}

/**************************/
#elif defined EXAMINE_FS
/* #include <rtorrent.h> */
/* #include <cbuf.h> */

char buffer[1024];

unsigned long long kevin, andy;
int try_cs_hp(void)
{
	if (hig == 0) {
		hig = cos_get_thd_id();
		printc("<<<high thd %d>>>\n", hig);
	}
	/* kevin = 0; */
	/* while(kevin++ < 1) { */
	/* 	td_t t1; */
	/* 	long evt1; */
	/* 	char *params1 = "foo/"; */
	/* 	char *params2 = "bar"; */
	/* 	char *params3 = "foo/bar"; */
	/* 	char *data1 = "1234567890", *data2 = "asdf;lkj"; */
	/* 	unsigned int ret1; */

	/* 	evt1 = evt_create(cos_spd_id()); */
	/* 	assert(evt1 > 0); */
	/* 	printc("1\n"); */
	/* 	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1), TOR_ALL, evt1); */
	/* 	if (t1 < 1) { */
	/* 		printc("UNIT TEST FAILED: tsplit failed %d\n", t1); return -1; */
	/* 	} */

	/* 	printc("2\n"); */
	/* 	ret1 = twrite_pack(cos_spd_id(), t1, data1, strlen(data1)); */
	/* 	printc("write %d , ret %d\n", strlen(data1), ret1); */

	/* 	ret1 = tread_pack(cos_spd_id(), t1, buffer, 1023); */
	/* 	if (ret1 > 0) buffer[ret1] = '\0'; */
	/* 	printc("read %d (%d): %s (%s)\n", ret1, strlen(data1), buffer, data1); */
	/* 	assert(!strcmp(buffer, data1)); */
	/* 	assert(ret1 == strlen(data1)); */
	/* 	buffer[0] = '\0'; */

	/* 	trelease(cos_spd_id(), t1); */
	/* } */

	return 0;
}

int try_cs_mp(void) { return 0;}
int try_cs_lp(void) { return 0;}
vaddr_t lmon_ser1_test(void) { return 0;}

/************************************/
#elif defined CAS_TEST
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

/* get a page from the heap */
static inline void *
test_get_page()
{
	char *addr = NULL;
	
	addr = cos_get_heap_ptr();
	if (!addr) goto done;
	cos_set_heap_ptr((void*)(((unsigned long)addr)+PAGE_SIZE));
	if ((vaddr_t)addr !=
	    mman_get_page(cos_spd_id(), (vaddr_t)addr, 0)) {
		printc("fail to get a page in logger\n");
		assert(0); // or return -1?
	}
done:
	return addr;
}

#include <ck_ring_cos.h>

struct test_evt_entry {
	int from_thd, to_thd;
	unsigned long from_spd, to_spd;
	unsigned long long time_stamp;
	int evt_type;    
	int func_num;   
	int para;   
	int commited;
};

struct test_evt_entry tmp_entry;

#ifndef CK_RING_CONTINUOUS
#define CK_RING_CONTINUOUS
#endif

#ifndef __EVTRING_TEST_DEFINED
#define __EVTRING_TEST_DEFINED
CK_RING(test_evt_entry, test_logevt_ring);
CK_RING_INSTANCE(test_logevt_ring) *test_evt_ring;
#endif

#ifndef FUNALIGN
#define FUNALIGN __attribute__((aligned(32)))
#endif

#ifndef CFORCEINLINE
#define CFORCEINLINE __attribute__((always_inline))
#endif


static inline CFORCEINLINE void 
test_loop(int par1, unsigned long par2, unsigned long par3, int par4, int par5, int evt_type)
{
	__asm__ volatile(".balign 512");

	struct test_evt_entry *evt;
	int old, new;
	int size;
	
	do {
		evt = (struct test_evt_entry *) CK_RING_GETTAIL(test_logevt_ring, test_evt_ring);
		if (!evt) {
			printc("full\n");
			assert(0); // should call logmgr
		}
		old = 0;
		new = cos_get_thd_id();
	} while((!(cos_cas((unsigned long *)&evt->from_thd, (unsigned long)old, 
			   (unsigned long )new))) && test_evt_ring->p_tail++);
	
	old = test_evt_ring->p_tail;
	new = test_evt_ring->p_tail + 1;
	cos_cas((unsigned long *)&test_evt_ring->p_tail, (unsigned long)old, 
		(unsigned long)new);

	/* evt->from_thd	= cos_get_thd_id(); */
	evt->to_thd	= par1;
	evt->from_spd	= par2;
	evt->to_spd	= par3;
	evt->func_num	= par4;
	evt->para	= par5;
	evt->evt_type	= evt_type;
	rdtscll(evt->time_stamp);
	evt->commited = 1;

	size = CK_RING_SIZE(test_logevt_ring, (CK_RING_INSTANCE(test_logevt_ring) *)((void *)test_evt_ring));
	printc("rb size %d\n", size);
	printc("rb head %d\n", test_evt_ring->c_head);
	printc("rb tail %d\n", test_evt_ring->p_tail);

	printc("evt->from_thd %d\n", evt->from_thd);
	printc("evt->to_thd %d\n", evt->to_thd);
	printc("evt->from_spd %d\n", evt->from_spd);
	printc("evt->to_thd %d\n", evt->to_spd);
	printc("evt->func_num %d\n", evt->func_num);
	printc("evt->para %d\n", evt->para);
	printc("evt->evt_type %d\n", evt->evt_type);
	printc("evt->commited %d\n", evt->commited);

	while(1) {}

	return;
}

int try_cs_mp(void)
{
	int i, j;
	test_evt_ring = (CK_RING_INSTANCE(test_logevt_ring) *)test_get_page();
	assert(test_evt_ring);

	CK_RING_INIT(test_logevt_ring, (CK_RING_INSTANCE(test_logevt_ring) *)((void *)test_evt_ring), NULL, get_powerOf2((PAGE_SIZE - sizeof(struct ck_ring_test_logevt_ring))/sizeof(struct test_evt_entry)));

	test_loop(55, 2, 3, 4, 5, 6);
	return;
}
#else 
int try_cs_hp(void) { return 0;}
int try_cs_mp(void) { return 0;}
int try_cs_lp(void) { return 0;}
vaddr_t lmon_ser1_test(void) { return 0;}
#endif



void 
cos_init(void) 
{
	LOCK1_INIT();
	LOCK2_INIT();
	return;
}

