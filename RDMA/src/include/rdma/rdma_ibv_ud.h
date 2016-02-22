#ifndef RDMA_IBV_UD_H
#define RDMA_IBV_UD_H

#include <rdma_config.h>
#include <rdma_ibv.h>

#define REQ_FULL 13

/* ================================================================== */
/* UD messages */

struct ud_hdr_t {
    uint64_t id;
    uint8_t type;
    //uint8_t pad[7];
};
typedef struct ud_hdr_t ud_hdr_t;

struct reconf_rep_t {
    ud_hdr_t   hdr;
    uint8_t    idx;
    cid_t cid; // include <rdma_config.h>
    uint64_t cid_idx;
    uint64_t head;
};
typedef struct reconf_rep_t reconf_rep_t;

struct rc_syn_t {
    ud_hdr_t hdr;
    rem_mem_t log_rm; // include <rdma_ibv.h>
    uint8_t idx;
    uint8_t data[0];    // log QPNs
};
typedef struct rc_syn_t rc_syn_t;

struct rc_ack_t {
	ud_hdr_t hdr;
	uint8_t idx;
};
typedef struct rc_ack_t rc_ack_t;

/* ================================================================== */ 

int ud_init( uint32_t receive_count );

#endif /* RDMA_IBV_UD_H */