/**
 * The state identifier (SID)
 * the SID is a 64-bit value [TERM|L|IDX], where
    * TERM is the current term
    * L is the leader flag, set when there is a leader
 */
/* The IDX consists of the 8 least significant bits (lsbs) */
#define SID_GET_IDX(sid) (uint8_t)((sid) & (0xFF))
/* The TERM consists of the most significant 55 bits */
#define SID_GET_TERM(sid) ((sid) >> 9)
    
struct server_t {
    void *ep;               // endpoint data (network related)
};

struct server_data_t {
	uint64_t cached_sid;
    
    server_config_t config; // configuration 
    
    log_t  *log;       // local log (remotely accessible)

    db* db_ptr;

    pthread_spinlock_t *data_lock;
    pthread_spinlock_t *ret_lock;
};