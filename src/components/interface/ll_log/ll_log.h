#ifndef LOG_INT_H
#define LOG_INT_H

#include <log.h>

vaddr_t llog_init(spdid_t spdid, vaddr_t addr);
vaddr_t llog_cs_init(spdid_t spdid, vaddr_t addr);
unsigned int llog_get_syncp(spdid_t spdid);

#endif /* LOG_INT_H */
