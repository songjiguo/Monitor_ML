#ifndef   	SCHED_H
#define   	SCHED_H

#include <res_spec.h>

int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
int sched_block(spdid_t spdid, unsigned short int dependency_thd);

void sched_timeout(spdid_t spdid, unsigned long amnt);
int sched_timeout_thd(spdid_t spdid);
unsigned int sched_tick_freq(void);
unsigned long sched_cyc_per_tick(void);
unsigned long sched_timestamp(void);
unsigned long sched_timer_stopclock(void);
int sched_priority(unsigned short int tid);

int sched_create_thd(spdid_t spdid, u32_t sched_param_0, u32_t sched_param_1, unsigned int desired_thd);
/* Should only be called by the booter/loader */
int sched_create_thread_default(spdid_t spdid, u32_t sched_param_0, u32_t sched_param_1, unsigned int desired_thd);

/* This function is deprecated...use sched_create_thd instead. */
int sched_create_thread(spdid_t spdid, struct cos_array *data);
/* where is this used? */
int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs);

int sched_create_net_brand(spdid_t spdid, unsigned short int port);
int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid);

int sched_component_take(spdid_t spdid);
int sched_component_release(spdid_t spdid);

int sched_create_thd_again(unsigned int tid);

#endif 	    /* !SCHED_H */
