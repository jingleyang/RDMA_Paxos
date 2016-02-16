struct consensus_component_t;

struct consensus_component_t* init_consensus_comp(const char* config_path, const char* log_path, node_id_t node_id, const char* start_mode);

void handle_accept_req(struct consensus_component_t* comp);

int rsm_op(struct consensus_component_t* comp, void* data, size_t data_size);