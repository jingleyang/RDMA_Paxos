#include "../include/log/log.h"

struct log_entry_t{
    struct accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
};

struct log_t{
    uint64_t read;
    uint64_t write; 
    uint64_t end;
    uint64_t tail;
    
    uint64_t len;
    
    uint8_t entries[0];
};

/**
 * Add a new entry at the end of the log
 * @return the new added entry
 */  
static inline log_entry* log_add_new_entry(log* lg)
{
    return (log_entry*)(lg->entries + lg->end);
}

static inline uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

static log_entry* log_append_entry(consensus_component* comp, size_t data_size, void* data, view_stamp* vs, log* lg)
{   
    /* Create new entry */
    log_entry *entry = log_add_new_entry(lg);

    entry->node_id = comp->node_id;
    entry->req_canbe_exed.view_id = comp->committed->view_id;
    entry->req_canbe_exed.req_id = comp->committed->req_id;
    entry->data_size = data_size;
    entry->msg_vs = *vs;

    memcpy(entry->data, data, data_size);
    
    lg->tail = lg->end;
    lg->end += log_entry_len(entry);
    
    return entry;
}