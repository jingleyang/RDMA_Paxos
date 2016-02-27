#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <string.h>

#include <netdb.h>
#include <arpa/inet.h>

#include <rdma/rdma_cma.h>
#include <infiniband/verbs.h>

#include "../util/common-header.h"
#include "../consensus/consensus.h"

#define CQ_CAPACITY (16)

#define MAX_SGE (2)

#define MAX_WR (8)

struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
  union stag {
	  uint32_t local_stag;
	  uint32_t remote_stag;
  }stag;
};

struct dare_server_data_t {
  uint32_t tail;
  struct rdma_buffer_attr metadata_attr[MAX_SERVER_COUNT];
  struct ibv_mr *log_mr;
  struct ibv_qp *qp[MAX_SERVER_COUNT];
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

int rdma_write(uint8_t target, void* buf, uint32_t len, uint32_t offset);

#ifdef __cplusplus
extern "C" {
#endif

  int init_rdma(consensus_component* consensus_comp);

#ifdef __cplusplus
}
#endif

#endif /* RDMA_COMMON_H */