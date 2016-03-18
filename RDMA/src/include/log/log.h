#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "../consensus/consensus-msg.h"

#define PAGE_SIZE 4096

#define LOG_SIZE  1*PAGE_SIZE

typedef struct log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
}log_entry;

log_entry* log_append_entry(consensus_component* comp, size_t data_size, void* data, view_stamp* vs, void* log, uint32_t tail);
uint32_t log_entry_len(log_entry* entry);
