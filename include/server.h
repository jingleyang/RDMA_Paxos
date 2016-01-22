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

struct sys_address_t{
    struct sockaddr_in m_addr;
    size_t m_sock_len;
    struct sockaddr_in s_addr;
    size_t s_sock_len;
};

struct socket_pair_t{
    uint64_t key;
    struct bufferevent* p_s;
    struct bufferevent* p_c;
};

struct ud_hdr_t {
    uint64_t req_id;
    uint8_t type;
};

struct client_req_t {
    ud_hdr_t hdr;
    sm_cmd_t cmd;
};

/* ================================================================== */

struct server_t {
    void *ep;               // endpoint data (network related)
};

struct server_data_t {
	uint64_t cached_sid;
    
    server_config_t config; // configuration 
    
    log_t  *log;       // local log (remotely accessible)

    struct rb_root endpoints;   // RB-tree with remote endpoints
    uint64_t last_write_csm_idx;
    uint64_t last_cmt_write_csm_idx;

    uint16_t pair_count;
    sys_address_t sys_addr;

	// libevent part
    struct event_base* base;
    struct evconnlistener* listener;

    db* db_ptr;

    pthread_spinlock_t *data_lock;
    pthread_spinlock_t *exed_lock;
};