#include "../include/log/log.h"

uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

log_entry* log_append_entry(consensus_component* comp, size_t data_size, void* data, view_stamp* vs, void* log, uint32_t tail)
{
	log_entry* entry = (log_entry*)((char*)log + tail);
    entry->node_id = comp->node_id;
    entry->req_canbe_exed.view_id = comp->committed.view_id;
    entry->req_canbe_exed.req_id = comp->committed.req_id;
    entry->data_size = data_size;
    entry->msg_vs = *vs;

    memcpy(entry->data, data, data_size);
   
    return entry;
}
