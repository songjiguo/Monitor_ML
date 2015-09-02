#ifndef MULTIPLEXER_H
#define MULTIPLEXER_H

// only called from fkml
vaddr_t multiplexer_init(spdid_t spdid, vaddr_t addr, int npages, int stream_type);
void multiplexer_retrieve_data(spdid_t spdid);

#endif /* MULTIPLEXER_H */
