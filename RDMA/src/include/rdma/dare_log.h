#ifndef DARE_LOG_H
#define DARE_LOG_H

#include "dare.h"
#include "dare_config.h"
#include "../util/common-structure.h"
#include "../consensus/consensus-msg.h"

struct dare_log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];
    uint8_t  type;
    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
};
typedef struct dare_log_entry_t dare_log_entry_t;

#define LOG_SIZE  4000*PAGE_SIZE
struct dare_log_t
{
    uint64_t head;
    uint64_t read;
    uint64_t write;
    uint64_t end;  /* offset after the last entry; 
                    if end==len the buffer is empty;
                    if end==head the buffer is full */
    uint64_t tail;  /* offset of the last entry
                    Note: tail + sizeof(last_entry) == end */
    
    uint64_t len;
    
    uint8_t entries[0];
}; 
typedef struct dare_log_t dare_log_t;

/* ================================================================== */
/* Static functions to handle the log */

static dare_log_t* log_new()
{
    dare_log_t* log = (dare_log_t*)malloc(sizeof(dare_log_t)+LOG_SIZE);
    if (NULL == log) {
        rdma_error(log_fp, "Cannot allocate log memory\n");
        return NULL;
    }    
    /* Initialize log offsets */
    memset(log, 0, sizeof(dare_log_t)+LOG_SIZE);
    log->len  = LOG_SIZE;
    log->end  = log->len;
    log->tail = log->len;

    return log;
}

static void log_free( dare_log_t* log )
{
    if (NULL != log) {
        free(log);
        log = NULL;
    }
}

static inline int is_log_empty( dare_log_t* log )
{
    return (log->end == log->len);
}

static inline int log_fit_entry_header( dare_log_t* log, uint64_t offset )
{
    return (log->len - offset >= sizeof(dare_log_entry_t));
}

static inline dare_log_entry_t* log_add_new_entry( dare_log_t* log )
{
    if (is_log_empty(log)) {
        return (dare_log_entry_t*)(log->entries);
    }
    return (dare_log_entry_t*)(log->entries + log->end);
}

static inline uint32_t log_entry_len(dare_log_entry_t* entry)
{
    return (uint32_t)(sizeof(dare_log_entry_t) + entry->data_size);
}                            

static dare_log_entry_t* log_get_entry(dare_log_t* log, uint64_t *offset)
{
    if (!log_fit_entry_header(log, *offset)) {
        /* The entry starts from the beginning */
        *offset = 0;
    }
    return (dare_log_entry_t*)(log->entries + *offset); 
}

static dare_log_entry_t* log_append_entry(dare_log_t* log, size_t data_size, void* data, view_stamp* vs, uint32_t node_id, view_stamp *highest_committed_vs, uint8_t type)
{   
    /* Create new entry */
    dare_log_entry_t *entry = log_add_new_entry(log);

    entry->node_id = node_id;
    entry->type = type;
    entry->data_size = data_size;

    if (!log_fit_entry_header(log, log->end)) {
        log->end = 0;
    }

    if (highest_committed_vs != NULL)
    {
        entry->msg_vs = *vs;
        entry->req_canbe_exed.view_id = highest_committed_vs->view_id;
        entry->req_canbe_exed.req_id = highest_committed_vs->req_id;
    }

    memcpy(entry->data,data,data_size);
    
    log->tail = log->end;

    log->end += log_entry_len(entry);
    
    return entry;
}

#endif /* DARE_LOG_H */
