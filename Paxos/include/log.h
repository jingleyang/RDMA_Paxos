struct log_entry_t {
    view_stamp result[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
};
typedef struct log_entry_t log_entry_t;

#define LOG_SIZE  16384*PAGE_SIZE
struct log_t
{
    uint64_t read;
    uint64_t write; 
    uint64_t end;
    uint64_t tail;
    
    uint64_t len;
    
    uint8_t entries[0];
};
typedef struct log_t log_t;

/* ================================================================== */
/* Static functions to handle the log */

/**
 * Create new log
 */
static log_t* log_new()
{
    log_t* log = (log_t*)malloc(sizeof(log_t) + LOG_SIZE);
 
    memset(log, 0, sizeof(log_t) + LOG_SIZE);
    log->len  = LOG_SIZE;
    log->end  = log->len;
    log->tail = log->len;

    return log;
}

/**
 * Free log
 */
static void log_free(log_t* log)
{
    if (NULL != log) {
        free(log);
        log = NULL;
    }
}

/* ================================================================== */

/**
 * Check if the log is empty
 */
static inline int is_log_empty(log_t* log)
{
    return (log->end == log->len);
}

static inline int log_fit_entry_header(log_t* log, uint64_t offset)
{
    return (log->len - offset >= sizeof(og_entry_t));
}

/**
 * Add a new entry at the end of the log
 * @return the new added entry
 */  
static inline log_entry_t* log_add_new_entry(log_t* log)
{
    if (is_log_empty(log)) {
        return (log_entry_t*)(log->entries);
    }
    return (log_entry_t*)(log->entries + log->end);
}

static inline uint32_t log_entry_len(log_entry_t* entry)
{
    return (uint32_t)(sizeof(log_entry_t) + entry->data.cmd.len);
}

/* ================================================================== */

/**
 * Get an existing entry at a certain offset;
 */    
static log_entry_t* log_get_entry(log_t* log, uint64_t *offset)
{
    if (is_log_empty(log)) {
        /* Log is empty */
        return NULL;
    }
    return (log_entry_t*)(log->entries + *offset); 
}

static uint64_t log_append_entry(consensus_component* comp, size_t data_size, void* data, view_stamp* vs)
{

    uint64_t offset = comp->log->tail;
    log_entry_t *last_entry = log_get_entry(comp->log, &offset);
    uint64_t idx = last_entry ? last_entry->idx + 1 : 1;
    
    /* Create new entry */
    log_entry_t *entry = log_add_new_entry(comp->log);

    entry->node_id = comp->node_id;
    entry->req_canbe_exed.view_id = comp->highest_to_commit_vs->view_id;
    entry->req_canbe_exed.req_id = comp->highest_to_commit_vs->req_id;
    entry->data_size = data_size;
    entry->msg_vs = *vs;

    if (!log_fit_entry_header(comp->log, comp->log->end)) 
    {
        comp->log->end = 0;
    }

    memcpy(entry->data,data,data_size);
    
    comp->log->tail = comp->log->end;
    comp->log->end += log_entry_len(entry);
    
    return idx;
}