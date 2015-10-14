#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <log.h>

#define TOTAL_AMNT 128
#define MAXULONG (~0)
unsigned int probability = 96;

#define STEP 100000
unsigned long long threshold = 1000000;
volatile unsigned long kkk = 0;

static inline void do_loop(unsigned long iter)
{
	unsigned long i;

 	for (i=0;i<iter;i++) kkk++;
}

static unsigned long get_loop_cost(unsigned long loop_num)
{
	u64_t start,end;

	rdtscll(start);
	do_loop(loop_num);
	rdtscll(end);

	return end-start; 
}

static void sinusoid_spike() 
{
	u64_t start, end, min, max;
	unsigned long long t;
	unsigned long temp = 0;
	unsigned long long pre_spin;
	unsigned long long val;
	static unsigned long long spin = STEP;
	static int spike = 0;

	unsigned long long ts;

	rdtscll(t);
	val = (unsigned long long)(t & (TOTAL_AMNT-1));
	if (val >= probability) {
		rdtscll(ts);
		printc("spike.....(@%llu)\n", ts);
		pre_spin = spin;
		spin = spin + 100*STEP;
		spike = 1;
	} else {
		if (spike == 1) {
			spike = 0;
			spin = pre_spin;
		}
		rdtscll(ts);
		printc("sinusoid-like.....(@%llu)\n", ts);
		if (spin > threshold) spin = spin - STEP;
		else spin = spin + STEP;
	}

	do {
		int i;
		min = MAXULONG;
		max= 0;
		for (i = 0 ; i < 10 ; i++) {
			temp = get_loop_cost(spin);
			if (temp < min) min = temp;
			if (temp > max) max = temp;
		}
	} while ((max-min) > (min/128));

	/* printc("spin:%llu cost :%lu\n", spin, temp); */
}

int high;
void cos_init(void)
{
	static int first = 0, flag = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 8;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		if (cos_get_thd_id() == high) {
			periodic_wake_create(cos_spd_id(), NOISE_PERIOD);
			while(1){
				periodic_wake_wait(cos_spd_id());
				/* printc("PERIODIC: noise....(thd %d in spd %ld)\n", */
				/*        cos_get_thd_id(), cos_spd_id()); */
				sinusoid_spike();
			}
		}
	}
	
	return;
}
