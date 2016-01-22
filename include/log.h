struct log_entry_t {
    int result[MAX_SERVER_COUNT];
    uint64_t committed;
    uint64_t idx;
    uint64_t term;
    uint16_t clt_id;    /* clt_id is the client’s unique identifier */
    uint64_t req_id;    /* req_id is a unique identifier for this request
                           Its purpose is to allow the primary cohort to recognize duplicate requests and avoid re-executing them */

    union {
        sm_cmd_t   cmd;
    } data; /* The entry data */
};

#define LOG_SIZE  16384*PAGE_SIZE
struct log_t
{
    uint64_t write; 
    uint64_t end;
    uint64_t tail;
    
    uint64_t len;
    
    uint8_t entries[0];
};

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

static uint64_t log_append_entry(log_t* log, uint64_t term, uint64_t req_id, uint16_t clt_id, void *data)
{
    sm_cmd_t *cmd = (sm_cmd_t*)data;

    uint64_t offset = log->tail;
    log_entry_t *last_entry = log_get_entry(log, &offset);
    uint64_t idx = last_entry ? last_entry->idx + 1 : 1;
    
    /* Create new entry */
    log_entry_t *entry = log_add_new_entry(log);

    entry->idx     = idx;
    entry->term    = term;
    entry->req_id  = req_id;
    entry->clt_id  = clt_id;

    if (!log_fit_entry_header(log, log->end)) 
    {
        log->end = 0;
    }

    entry->data.cmd.len = cmd->len;
    memcpy(entry->data.cmd.cmd, cmd->cmd, entry->data.cmd.len);
    
    log->tail = log->end;
    log->end += log_entry_len(entry);
    
    return idx;
}