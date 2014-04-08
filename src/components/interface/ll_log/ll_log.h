#ifndef LOG_INT_H
#define LOG_INT_H

#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
unsigned int llog_get_syncp(spdid_t spdid);

// called from scheduler when create the thread
int llog_init_prio(spdid_t spdid, unsigned int thd_id, unsigned int prio);

#endif /* LOG_INT_H */
