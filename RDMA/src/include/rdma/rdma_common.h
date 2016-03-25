#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <infiniband/verbs.h>
#include <byteswap.h>

#include "../util/common-header.h"
#include "../replica-sys/node.h"

#define CQ_CAPACITY (16)

// TODO FIXME this is kind of guessing
#define Q_DEPTH 64
#define S_DEPTH 32
#define S_DEPTH_ 31

#define PAGE_SIZE 4096
#define LOG_SIZE 5 * PAGE_SIZE

struct configuration_t
{
	const char *dev_name;
	int ib_port;
	int gid_idx;
	int64_t node_id;
};

struct cm_con_data_t
{
	int64_t node_id;
	uint64_t addr;
	uint32_t rkey;
	uint32_t qp_num;
	uint16_t lid;
	uint8_t gid[16];
}__attribute__((packed));

struct resources
{
	struct ibv_device_attr device_attr;
	struct ibv_port_attr port_attr;
	struct cm_con_data_t remote_props[MAX_SERVER_COUNT];
	struct ibv_context *ib_ctx;
	struct ibv_pd *pd;
	struct ibv_cq *cq;
	struct ibv_qp *qp[MAX_SERVER_COUNT];
	struct ibv_mr *mr;
	char *buf;
	int sock;

	uint32_t rc_max_inline_data;
	int req_num[MAX_SERVER_COUNT];

	uint64_t end; /* offset after the last entry */
    uint64_t tail; /* offset of the last entry
                      Note: tail + sizeof(last_entry) == end */
};

int find_max_inline(struct ibv_context *context, struct ibv_pd *pd, uint32_t *max_inline_arg);

void *connect_peers(peer* peer_pool, int64_t node_id, uint32_t group_size);

int rdma_write(uint8_t target, void *buf, uint32_t len, uint32_t offset, void *udata);

#endif /* RDMA_COMMON_H */