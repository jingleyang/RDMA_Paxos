#ifndef CONSENSUS_H

#define CONSENSUS_H

typedef uint64_t db_key_type;
struct consensus_component_t;

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t* init_consensus_comp(const char* config_path, const char* log_path, uint32_t node_id, const char* start_mode);

#endif