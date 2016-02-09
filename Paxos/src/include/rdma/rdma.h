struct rdma_data_t {
	struct rb_root endpoints;   // RB-tree with remote endpoints
	rdma_config_t config; // configuration 
};
typedef struct rdma_data_t rdma_data_t;