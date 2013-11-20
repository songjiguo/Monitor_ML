#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <cos_list.h>
#include <cos_map.h>
#include <cos_synchronization.h>
#include <cos_net.h>
#include <print.h>
#include <sched.h>
#include <res_spec.h>


#define ITER 1000*10
volatile unsigned long long start, end;

void bench_memcpy()
{
	int *a,*b,*c,*d;
	int i,size;
	long int te;

	size= 1024*256; //256KB 

	a=(int*)malloc(size);
	b=(int*)malloc(size);
	c=(int*)malloc(size);
	d=(int*)malloc(size);
	memset(c, 0xFF, size);
	memset(d, 0xAA, size);
  
	rdtscll(start);
	for(i = 0; i < ITER; i++){
		memcpy(a, c, size);
		memcpy(b, d, size);
		memcpy(b, c, size);
		memcpy(a, d, size);
	}

	rdtscll(end);
	
	printc("\n memcpy of 256KB data %d times in %llu \n\n", ITER*4,end-start);
	printc("\n avg cost %llu \n\n", (end-start)/ITER/4);
}


void cos_init(void)
{
	static int flag = 0;
	union sched_param sp;
	int i;

	if(flag == 0){
		flag = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		bench_memcpy();
	}


	return;
}

