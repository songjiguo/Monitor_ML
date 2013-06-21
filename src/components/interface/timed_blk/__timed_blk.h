#ifndef   	TIMED_BLK_H
#define   	TIMED_BLK_H

#include <cos_component.h>

int timed_event_block(spdid_t spdinv, unsigned int amnt);
int timed_event_wakeup(spdid_t spdinv, unsigned short int thd_id);

#endif 	    /* !TIMED_BLK_H */
