#ifndef CONSENSUS_MSG_H
#define CONSENSUS_MSG_H

#include "../util/common-header.h"

typedef struct accept_ack_t{
    view_stamp msg_vs;
    node_id_t node_id;
}accept_ack;
#define ACCEPT_ACK_SIZE (sizeof(accept_ack))

#endif