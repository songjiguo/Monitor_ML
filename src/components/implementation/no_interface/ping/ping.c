#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <pong.h>

#include <mem_mgr_large.h>
#include <valloc.h>
#include <mem_pool.h>
 
#define ITER (1024*128)
u64_t meas[ITER];

/* ///////////////test clsab and cvect and bitmap////////////////////////////////// */
/* extern void *alloc_page(void); */
/* extern void free_page(void *ptr); */

/* #define CSLAB_ALLOC(sz)   alloc_page() */
/* #define CSLAB_FREE(x, sz) free_page(x) */
/* #include <cslab.h> */

/* #define CVECT_ALLOC() alloc_page() */
/* #define CVECT_FREE(x) free_page(x) */
/* #include <cvect.h> */

/* // only track alias now */
/* struct rec_data_mm { */
/* 	int idx;              // use ">> PAGE_SHIFT" to get an unique id for this client spd */
	
/* 	spdid_t s_spd; */
/* 	vaddr_t s_addr; */
/* 	spdid_t d_spd; */
/* 	vaddr_t d_addr; */

/* 	unsigned long fcnt; */
/* }; */

/* CSLAB_CREATE(rdping, sizeof(struct rec_data_mm)); */
/* CVECT_CREATE_STATIC(rec_mm_vect); */

/* static struct rec_data_mm * */
/* rdping_lookup(int idx) */
/* { return cvect_lookup(&rec_mm_vect, idx); } */

/* static struct rec_data_mm * */
/* rdping_alloc(void) */
/* { */
/* 	struct rec_data_mm *rdping; */
/* 	rdping = cslab_alloc_rdping(); */
/* 	if (!rdping) { */
/* 		printc("can not slab_alloc rdping\n"); */
/* 	} */
/* 	return rdping; */
/* } */

/* static void */
/* rdping_dealloc(struct rec_data_mm *rdping) */
/* { */
/* 	assert(rdping); */
/* 	cslab_free_rdping(rdping); */
/* } */


/* ///////////////////////////////////////////////// */



void cos_init(void)
{
	u64_t start, end, avg, tot = 0, dev = 0;
	int i, j;

/* 	struct rec_data_mm *rdping; */
/* 	for (i = 0 ; i<4000; i++) { */
/* 		rdping = rdping_alloc(); */
/* 		if (!rdping) goto err; */
/* 	} */

/* done: */
/* 	printc("------------\n"); */
/* 	return; */
/* err:   */
/* 	printc("%d get not get alloc\n",i); */
/* 	goto done; */

/* 	return; */
	call();			/* get stack */
	printc("Starting Invocations.\n");

	for (i = 0 ; i < ITER ; i++) {
		rdtscll(start);
		call();
		rdtscll(end);
		meas[i] = end-start;
	}

	for (i = 0 ; i < ITER ; i++) tot += meas[i];
	avg = tot/ITER;
	printc("avg %lld\n", avg);
	for (tot = 0, i = 0, j = 0 ; i < ITER ; i++) {
		if (meas[i] < avg*2) {
			tot += meas[i];
			j++;
		}
	}
	printc("avg w/o %d outliers %lld\n", ITER-j, tot/j);

	for (i = 0 ; i < ITER ; i++) {
		u64_t diff = (meas[i] > avg) ? 
			meas[i] - avg : 
			avg - meas[i];
		dev += (diff*diff);
	}
	dev /= ITER;
	printc("deviation^2 = %lld\n", dev);
	
//	printc("%d invocations took %lld\n", ITER, end-start);
	return;
}
