#include "../include/log/log.h"

uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

log_entry* log_append_entry(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs, log_t* log, void* shm)
{
	log_entry* entry = (log_entry*)((char*)shm + log->tail);
    entry->node_id = comp->node_id;
    entry->req_canbe_exed.view_id = comp->committed.view_id;
    entry->req_canbe_exed.req_id = comp->committed.req_id;
    entry->data_size = data_size;
    entry->msg_vs = *vs;

    memcpy(entry->data, data, data_size);
    log->tail = log->tail + log_entry_len(entry);
   
    return entry;
}
