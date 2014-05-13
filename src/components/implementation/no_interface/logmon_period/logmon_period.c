#include <cos_component.h>
#include <print.h>
#include <cos_component.h>
#include <res_spec.h>
#include <sched.h>

#include <periodic_wake.h>
#include <timed_blk.h>

#include <log.h>
#include <ll_log.h>

/* extern int llog_contention(spdid_t spdid, int par1, int par2, int par3); */

/* void test_logcontention() */
/* { */
/* 	int owner = 4; */
/* 	int contender = 7; */
/* 	int cont_spd = 9; */
/* 	int to_thd = 11;  */

/* 	int func_num = 3; */
/* 	int para = 13; */
/* 	int evt_type = 5; */
/* 	unsigned long from_spd = 132943294; */
/* 	unsigned long to_spd = 3459479; */

/* 	printc("test_logcontention here (thd %d spd %lu)\n", cos_get_thd_id(), cos_spd_id()); */
	
/*         /\* (op<<24) | (flags << 16) | (dest_spd) *\/ */
/* 	int par1 = (evt_type<<28) | (func_num<<24) | (para<<16) | (to_thd<<8) | (owner); */
/* 	unsigned long par2 = from_spd;  // this can be cap_no */
/* 	unsigned long par3 = to_spd;    // this can be cap_no */
		
/* 	llog_contention(cos_spd_id(), par1,par2,par3); */

/* 	return; */
/* } */


int high;
#ifdef MEAS_LOG_ASYNCACTIVATION
volatile unsigned long long log_start, log_end;
#endif

void cos_init(void)
{
	static int first = 0, flag = 0;
	union sched_param sp;

	if(first == 0){
		first = 1;
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 5;
		high = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		if (cos_get_thd_id() == high) {
			int lm_sync_period;
			lm_sync_period = llog_getsyncp(cos_spd_id());
			periodic_wake_create(cos_spd_id(), lm_sync_period);
			while(1){
				periodic_wake_wait(cos_spd_id());
				/* printc("periodic process log....(thd %d)\n", cos_get_thd_id()); */
#ifdef MEAS_LOG_ASYNCACTIVATION				
				rdtscll(log_start);
#endif
				llog_process(cos_spd_id());
#ifdef MEAS_LOG_ASYNCACTIVATION				
				rdtscll(log_end);
				printc("Periodic invoke/switch/process cost %llu\n", log_end-log_start);
#endif
			}
		}
	}
	
	return;
}
