#ifndef CONSENSUS_H

#define CONSENSUS_H
#include "../util/common-header.h"
#include "../db/db-interface.h"

typedef uint64_t db_key_type;

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

typedef struct peer_t{
    struct sockaddr_in* peer_address;
    size_t sock_len;
    uint32_t tail;
}peer;

struct consensus_component_t{
    con_role my_role;
    uint32_t node_id;

    view cur_view;
    view_stamp highest_seen_vs; 
    view_stamp committed;

    struct sockaddr_in my_address;
    char *zoo_host_port;
    uint32_t group_size;
    peer* peer_pool;

    int zfd; // The descriptor used to talk to zookeeper.
    char *znode_name;
    
    FILE* con_log_file;

    char* db_name;
    db* db_ptr;
    
    /* lock */
    pthread_mutex_t mutex;
};
typedef struct consensus_component_t consensus_component;

#ifdef __cplusplus
extern "C" {
#endif

	void init_consensus_comp(consensus_component* consensus_comp, const char* log_path, node_id_t node_id);
	int rsm_op(consensus_component* comp, void* data, size_t data_size);
	void *handle_accept_req(void* arg);

#ifdef __cplusplus
}
#endif

#endif
