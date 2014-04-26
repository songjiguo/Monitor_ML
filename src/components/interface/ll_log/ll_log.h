#ifndef LOG_INT_H
#define LOG_INT_H

//#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
unsigned int llog_getsyncp();
int llog_setperiod(spdid_t spdid, unsigned int thd_id, unsigned int period);
int llog_setprio(spdid_t spdid, unsigned int thd_id, unsigned int prio);

int llog_process(spdid_t spdid);
int llog_contention(spdid_t spdid, int par1, int par2, int par3);

#endif /* LOG_INT_H */
