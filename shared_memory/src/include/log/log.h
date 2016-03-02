#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "../consensus/consensus-msg.h"

#define LOG_SIZE  16384*1024*4

typedef struct log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
}log_entry;

uint32_t log_entry_len(log_entry* entry);
