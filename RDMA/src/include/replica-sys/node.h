#ifndef NODE_H
#define NODE_H 
#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "../db/db-interface.h"
#include "./replica.h"

typedef struct peer_t{
    int peer_id;
    int active;

    struct sockaddr_in* peer_address;
    size_t sock_len;
}peer;

typedef struct node_t{

    node_id_t node_id;

    int stat_log;
    int sys_log;

    view cur_view;
    view_stamp highest_to_commit;
    view_stamp highest_committed;
    view_stamp highest_seen;

    //consensus component
    consensus_component* consensus_comp;

    // replica group
    struct sockaddr_in my_address;
    uint32_t group_size;
    peer* peer_pool;

    //databse part
    char* db_name;
    db* db_ptr;
    FILE* sys_log_file;

    char *zoo_host_port;
    
}node;

node* system_initialize(int64_t node_id,const char* start_mode,const char* config_path,const char* log_path,void* db_ptr,void* arg);

#endif