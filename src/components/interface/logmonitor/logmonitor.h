/**
   log monitor for latent fault (track all events)
 */

#ifndef LOGMON_H
#define LOGMON_H

#include <cos_component.h>

struct tr_event_mon
{
	int thd_id;
	spdid_t src_spd;
	spdid_t des_spd;
	char* event_name;
	unsigned long long time_stamp;
};

vaddr_t lm_init(spdid_t spdid);
int lm_process(spdid_t spdid);

#endif /* LOGMON_H*/ 
