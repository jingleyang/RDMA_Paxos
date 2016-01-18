struct log_entry_t {
    int result[MAX_SERVER_COUNT];
    uint64_t committed;
    uint64_t idx;
    uint64_t term;

    size_t data_size;
    char data[0];
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
static inline int 
is_log_empty(log_t* log)
{
    return (log->end == log->len);
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