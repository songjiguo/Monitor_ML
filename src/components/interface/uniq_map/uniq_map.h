/**
   mapping unique id and path for torrent
 */

#ifndef UNIQMAP_H
#define UNIQMAP_H

#include <cos_component.h>

int uniq_map_set(spdid_t spdid, cbuf_t cb, int sz);
void uniq_map_del(spdid_t spdid, int id);
int uniq_map_lookup(spdid_t spdid, cbuf_t cb, int sz);

#endif /* UNIQMAP_H*/ 
