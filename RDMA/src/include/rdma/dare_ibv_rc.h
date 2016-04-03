#ifndef DARE_IBV_RC_H
#define DARE_IBV_RC_H

#include <infiniband/verbs.h> /* OFED stuff */
#include "dare_ibv.h"

#define S_DEPTH  8192
#define S_DEPTH_ 8191

struct cm_con_data_t {
	rem_mem_t log_mr;
	uint32_t idx;
	uint16_t lid;
	uint8_t gid[16];
	uint32_t qpns;
}__attribute__ ((packed));

int rc_init();
void rc_free();

int post_send( uint32_t server_id, void *buf, uint32_t len, struct ibv_mr *mr, enum ibv_wr_opcode opcode, rem_mem_t rm);

#endif /* DARE_IBV_RC_H */
