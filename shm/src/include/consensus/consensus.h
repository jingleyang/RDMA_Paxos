#ifndef CONSENSUS_H

#define CONSENSUS_H
#include "../util/common-header.h"
#include "../db/db-interface.h"

typedef uint64_t db_key_type;

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t{
    con_role my_role;
    uint32_t node_id;

    uint32_t group_size;

    FILE* con_log_file;

    view cur_view;
    view_stamp highest_seen_vs; 
    view_stamp committed;

    char* db_name;
    db* db_ptr;
    struct sockaddr_in my_address;
    size_t my_sock_len;

    /* lock */
    pthread_mutex_t mutex;
};
typedef struct consensus_component_t consensus_component;

#ifdef __cplusplus
extern "C" {
#endif

	consensus_component* init_consensus_comp(const char* config_path, const char* log_path, int64_t node_id, const char* start_mode);
	int rsm_op(consensus_component* comp, void* data, size_t data_size);
	void handle_accept_req(consensus_component* comp);

#ifdef __cplusplus
}
#endif

#endif
