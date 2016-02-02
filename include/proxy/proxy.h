// hash_key type def
typedef uint64_t hk_t;
typedef uint64_t counter_t;

typedef struct proxy_address_t{
    struct sockaddr_in p_addr;
    size_t m_sock_len;
    struct sockaddr_in s_addr;
    size_t s_sock_len;
}proxy_address;

typedef struct socket_pair_t{
    hk_t key;
    counter_t counter;
    struct proxy_node_t* proxy;
    struct bufferevent* p_s;
    struct bufferevent* p_c;
}socket_pair;

typedef struct proxy_node_t{
    // socket pair
    nid_t node_id; 
    nc_t pair_count;
    socket_pair* hash_map;
    proxy_address sys_addr;
    
    // libevent part
    struct event_base* base;
    struct evconnlistener* listener;
    struct timeval recon_period;
    //signal handler
    struct event* sig_handler;

    // in the loop of libevent logical, we must give a way to periodically
    // invoke our actively working function
    struct event* do_action;
    db_key_type cur_rec;
    db_key_type highest_rec;
    // consensus module
    pthread_t sub_thread;
    pthread_t p_self;
    struct node_t* con_node;
    struct bufferevent* con_conn;
    struct event* re_con;
    FILE* req_log_file;
    FILE* sys_log_file;
    char* db_name;
    db* db_ptr;
    // for call back of the thread;
    pthread_mutex_t lock;

}proxy_node;