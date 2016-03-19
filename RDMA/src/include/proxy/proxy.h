#ifndef PROXY_H 
#define PROXY_H

#include "../util/common-header.h"
#include "../replica-sys/node.h"


// hash_key type def
typedef uint64_t hk_t;
typedef uint64_t sec_t;
typedef uint32_t nid_t;
typedef uint16_t nc_t;
typedef uint64_t counter_t;

// record number
typedef uint64_t rec_no_t;
typedef uint64_t flag_t;

struct proxy_node_t;


typedef struct proxy_node_t{
    nid_t node_id; 

    // log option
    int ts_log;
    int sys_log;
    int stat_log;
    int req_log;

    node* con_node;
    struct bufferevent* con_conn;
    struct event* re_con;
    FILE* req_log_file;
    FILE* sys_log_file;
    char* db_name;
    db* db_ptr;
    // for call back of the thread;
    pthread_mutex_t lock;

}proxy_node;

#ifdef __cplusplus
extern "C" {
#endif
    proxy_node* proxy_init(node_id_t node_id, const char* start_mode, const char* config_path, const char* log_path);

#ifdef __cplusplus
}
#endif

#endif