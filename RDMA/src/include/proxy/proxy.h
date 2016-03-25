#ifndef PROXY_H 
#define PROXY_H

#include "../util/common-header.h"
#include "../replica-sys/node.h"

typedef uint32_t nid_t;

struct proxy_node_t;


typedef struct proxy_node_t{
    nid_t node_id; 

    // log option
    int ts_log;
    int sys_log;
    int stat_log;
    int req_log;

    node* con_node;

    FILE* req_log_file;
    FILE* sys_log_file;
    char* db_name;
    db* db_ptr;

}proxy_node;

#ifdef __cplusplus
extern "C" {
#endif
    proxy_node* proxy_init(node_id_t node_id, const char* config_path, const char* log_path);

#ifdef __cplusplus
}
#endif

#endif