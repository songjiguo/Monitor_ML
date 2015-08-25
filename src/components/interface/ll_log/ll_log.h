#ifndef LOG_INT_H
#define LOG_INT_H

//#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr, int npages, int flag);
unsigned int llog_getsyncp();
int llog_setperiod(spdid_t spdid, unsigned int thd_id, unsigned int period);
int llog_setprio(spdid_t spdid, unsigned int thd_id, unsigned int prio);

int llog_process(spdid_t spdid);
int llog_contention(spdid_t spdid, int par1, int par2, int par3);

int llog_fkml_retrieve_data(spdid_t spdid);  // only called from fkml component

#endif /* LOG_INT_H */
