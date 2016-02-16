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

log* log_new(){
    log* lg = (log*)malloc(sizeof(log) + LOG_SIZE);
 
    memset(lg, 0, sizeof(lg) + LOG_SIZE);
    lg->len  = LOG_SIZE;
    lg->end  = lg->len;
    lg->tail = lg->len;

    return lg;
}

/**
 * Free log
 */
static void log_free(log* lg)
{
    if (NULL != lg) {
        free(lg);
        lg = NULL;
    }
}

/* ================================================================== */

/**
 * Check if the log is empty
 */
static inline int is_log_empty(log* lg)
{
    return (lg->end == lg->len);
}

static inline int log_fit_entry_header(log* lg, uint64_t offset)
{
    return (lg->len - offset >= sizeof(log_entry));
}

/**
 * Add a new entry at the end of the log
 * @return the new added entry
 */  
static inline log_entry* log_add_new_entry(log* lg)
{
    if (is_log_empty(lg)) {
        return (log_entry*)(lg->entries);
    }
    return (log_entry*)(lg->entries + lg->end);
}

static inline uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

/* ================================================================== */

/**
 * Get an existing entry at a certain offset;
 */    
static log_entry* log_get_entry(log* lg, uint64_t *offset)
{
    if (is_log_empty(lg)) {
        /* Log is empty */
        return NULL;
    }
    return (log_entry*)(lg->entries + *offset); 
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

    if (!log_fit_entry_header(lg, lg->end)) 
    {
        lg->end = 0;
    }

    memcpy(entry->data, data, data_size);
    
    lg->tail = lg->end;
    lg->end += log_entry_len(entry);
    
    return entry;
}