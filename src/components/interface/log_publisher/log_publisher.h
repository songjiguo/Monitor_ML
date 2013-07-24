/* Jiguo: context switch tracking
 * set up shared ring buffer for tracking CS
 * only between scheduler and log_manager
*/

#ifndef   	LOG_PUB_H
#define   	LOG_PUB_H

vaddr_t sched_logpub_setup(spdid_t spdid, vaddr_t ring_addr);
void sched_logpub_wait(void);

#endif 	    /* !LOG_PUB_H */
