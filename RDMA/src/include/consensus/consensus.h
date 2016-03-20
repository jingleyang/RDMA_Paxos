#ifndef CONSENSUS_H

#define CONSENSUS_H
#include "../util/common-header.h"
#include "../db/db-interface.h"

typedef uint64_t db_key_type;
struct node_t;

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t{ con_role my_role;
    uint32_t node_id;

    uint32_t group_size;
    struct node_t* my_node;
    struct sockaddr_in my_address;

    FILE* sys_log_file;
    int sys_log;
    int stat_log;

    view* cur_view;
    view_stamp* highest_seen_vs; 
    view_stamp* highest_to_commit_vs;
    view_stamp* highest_committed_vs;

    db* db_ptr;
    
    pthread_mutex_t* lock;
};
typedef struct consensus_component_t consensus_component; 

consensus_component* init_consensus_comp(struct node_t*,struct sockaddr_in,pthread_mutex_t*,uint32_t,FILE*,int,int,
        const char*,void*,int,
        view*,view_stamp*,view_stamp*,view_stamp*,void*);

#ifdef __cplusplus
extern "C" {
#endif
    int rsm_op(consensus_component* comp, void* data, size_t data_size);
    void *handle_accept_req(void* arg);
#ifdef __cplusplus
}
#endif

#endif