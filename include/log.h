struct log_entry_t {
    uint64_t idx;
    uint64_t term;
    uint64_t req_id;    /* The request ID of the client */
    uint16_t clt_id;    /* LID of client */

    size_t data_size;
    char data[0];
};

#define LOG_SIZE  16384*PAGE_SIZE
struct log_t
{
    uint64_t read;  /* offset of the first not applied entry 
                    the server applies all entries from read to write */
    uint64_t write; /* offset of the first not committed entry 
                    the leader overlaps all entries from write to end of 
                    its own log */
    uint64_t end;  /* offset after the last entry; 
                    if end==len the buffer is empty;
                    if end==head the buffer is full */
    
    uint64_t len;
    
    uint8_t entries[0];
};

/**
 * Create new log
 */
log_t* log_new()
{
    log_t* log = (log_t*)malloc(sizeof(log_t) + LOG_SIZE);
 
    memset(log, 0, sizeof(log_t)+LOG_SIZE);
    log->len  = LOG_SIZE;

    return log;
}