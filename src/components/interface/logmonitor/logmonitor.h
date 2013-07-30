/*
   log monitor header file for latent fault (track all events)
*/

#ifndef LOGMON_H
#define LOGMON_H

#include <cos_component.h>

vaddr_t lm_init(spdid_t spdid);
int lm_process(spdid_t spdid);
unsigned int lm_get_sync_period();

#endif /* LOGMON_H*/ 
