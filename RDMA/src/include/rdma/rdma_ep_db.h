#ifndef RDMA_EP_DB_H
#define RDMA_EP_DB_H

#include <rbtree.h>
#include <rdma_ibv.h>

struct ep_t {
    struct rb_node node;
    ud_ep_t ud_ep; // include <rdma_ibv.h>
    uint64_t last_req_id; /* this is the ID of the last request from this endpoint that I answer; ignore requests with lower IDs */
};
typedef struct ep_t ep_t;

/* ================================================================== */

ep_t* ep_search( struct rb_root *root, const uint16_t lid );
ep_t* ep_insert( struct rb_root *root, const uint16_t lid );

#endif /* RDMA_EP_DB_H */