#include "../include/log/log.h"

static log_t* log_new()
{
    log_t* log = (log_t*)malloc(sizeof(log_t) + LOG_SIZE);
    if (NULL == log) {
        rdma_error(log_fp, "Cannot allocate log memory\n");
        return NULL;
    }    
    /* Initialize log offsets */
    memset(log, 0, sizeof(log_t)+LOG_SIZE);
    log->len  = LOG_SIZE;
    log->tail = 0;

    return log;
}

uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

static inline log_entry_t* log_add_new_entry(log_t* log)
{
    return (log_entry_t*)(log->entries + log->tail);
}

log_entry* log_append_entry(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs, log_t* log)
{

	log_entry* entry = log_add_new_entry(log);
    entry->node_id = comp->node_id;
    entry->req_canbe_exed.view_id = comp->committed.view_id;
    entry->req_canbe_exed.req_id = comp->committed.req_id;
    entry->data_size = data_size;
    entry->msg_vs = *vs;

    memcpy(entry->data, data, data_size);
    log->tail = log->tail + log_entry_len(entry);
   
    return entry;
}
