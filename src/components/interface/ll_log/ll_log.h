#ifndef LOG_INT_H
#define LOG_INT_H

//#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
int llog_init_prio(spdid_t spdid, unsigned int thd_id, unsigned int prio);
int llog_corb(spdid_t spdid, unsigned int owner, unsigned int contender, int flag);
unsigned int llog_get_syncp(spdid_t spdid);


#endif /* LOG_INT_H */
