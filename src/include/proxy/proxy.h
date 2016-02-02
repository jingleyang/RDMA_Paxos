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
    char* db_name;
    db* db_ptr;
    // for call back of the thread;
    pthread_mutex_t lock;

}proxy_node;

typedef enum proxy_action_t{
    P_CONNECT=0,
    P_SEND=1,
}proxy_action;

typedef struct proxy_msg_header_t{
    proxy_action action;
    struct timeval received_time;
    struct timeval created_time;
    hk_t connection_id;
    counter_t counter;
}proxy_msg_header;
#define PROXY_MSG_HEADER_SIZE (sizeof(proxy_msg_header))

typedef struct proxy_connect_msg_t{
    proxy_msg_header header;
}proxy_connect_msg;
#define PROXY_CONNECT_MSG_SIZE (sizeof(proxy_connect_msg))

typedef struct proxy_send_msg_t{
    proxy_msg_header header;
    size_t data_size;
    char data[0];
}__attribute__((packed))proxy_send_msg;
#define PROXY_SEND_MSG_SIZE(M) (M->data_size+sizeof(proxy_send_msg))

#define MY_HASH_SET(value,hash_map) do{ \
    HASH_ADD(hh,hash_map,key,sizeof(hk_t),value);}while(0)

#define MY_HASH_GET(key,hash_map,ret) do{\
 HASH_FIND(hh,hash_map,key,sizeof(hk_t),ret);}while(0) 