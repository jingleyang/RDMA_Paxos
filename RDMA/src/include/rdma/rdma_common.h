#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <string.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>
#include <inttypes.h>

#include "../util/common-header.h"
#include "../consensus/consensus.h"

#define CQ_CAPACITY (16)

#define MAX_SGE (2)

// TODO FIXME this is kind of guessing
#define Q_DEPTH 64
#define S_DEPTH 32
#define S_DEPTH_ 31

struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
	uint32_t buf_rkey;
};

struct dare_server_data_t {
  uint32_t tail;
  struct rdma_buffer_attr metadata_attr[MAX_SERVER_COUNT];
  void *log_mr;
  uint32_t local_key[MAX_SERVER_COUNT];
  struct ibv_qp *qp[MAX_SERVER_COUNT];
  struct ibv_cq *cq[MAX_SERVER_COUNT];
  uint32_t rc_max_inline_data;
  int req_num[MAX_SERVER_COUNT];
};
typedef struct dare_server_data_t dare_server_data_t;

extern dare_server_data_t srv_data;

int get_addr(char *dst, struct sockaddr *addr);

void show_rdma_buffer_attr(struct rdma_buffer_attr *attr);

int process_rdma_cm_event(struct rdma_event_channel *echannel, enum rdma_cm_event_type expected_event, struct rdma_cm_event **cm_event);

struct ibv_mr* rdma_buffer_alloc(struct ibv_pd *pd, uint32_t length, enum ibv_access_flags permission);

void rdma_buffer_free(struct ibv_mr *mr);

struct ibv_mr *rdma_buffer_register(struct ibv_pd *pd, void *addr, uint32_t length, enum ibv_access_flags permission);

void rdma_buffer_deregister(struct ibv_mr *mr);

int process_work_completion_events(struct ibv_comp_channel *comp_channel, 
		struct ibv_wc *wc, 
		int max_wc);


void show_rdma_cmid(struct rdma_cm_id *id);

int find_max_inline(struct ibv_context *context, struct ibv_pd *pd, uint32_t *max_inline_arg);
int rdma_write(uint8_t target, void* buf, uint32_t len, uint32_t offset);

#ifdef __cplusplus
extern "C" {
#endif

  int init_rdma(consensus_component* consensus_comp);

#ifdef __cplusplus
}
#endif

#endif /* RDMA_COMMON_H */