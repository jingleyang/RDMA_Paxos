#ifndef RDMA_SERVER_H
#define RDMA_SERVER_H 

#include <log.h>
#include <rdma_config.h>
#include <rdma.h>

/* Server types */
#define SRV_TYPE_START  1
#define SRV_TYPE_JOIN   2

struct server_t {
    void *ep;               // endpoint data (network related)
};

struct rdma_input_t {
    FILE* rdma_log;
    char* srv_type;
    int group_size;
    node_id_t server_idx;
};
typedef struct rdma_input_t rdma_input_t;

struct rdma_data_t {
	rdma_input_t* input;
	struct rb_root endpoints;   // RB-tree with remote endpoints
	rdma_config_t config; // configuration 
	log_t  *log;
};
typedef struct rdma_data_t rdma_data_t;

extern rdma_data_t rdma_data;
extern FILE* rdma_log_fp;

#ifdef __cplusplus
extern "C" {
#endif

	int rdma_init(node_id_t node_id, int size, const char* log_path, const char* start_mode);

#ifdef __cplusplus
}
#endif

#endif