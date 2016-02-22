#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "../consensus/consensus-msg.h"

#include "../rdma/rdma.h"
#define LOG_SIZE  16384*PAGE_SIZE

typedef struct log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
}log_entry;

struct log_t
{ 
    uint64_t tail;  /* offset of the last entry */

    uint64_t len;
    
    uint8_t entries[0];
};
typedef struct log_t log_t;

log_entry* log_append_entry(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs, log_t* log, void* shm);
uint32_t log_entry_len(log_entry* entry);
