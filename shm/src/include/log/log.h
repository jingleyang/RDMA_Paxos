#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "../consensus/consensus-msg.h"

#define LOG_SIZE  16384*1024

typedef struct log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
}log_entry;

typedef struct log_t{
    uint64_t read;
    uint64_t write; 
    uint64_t end;
    uint64_t tail;
    
    uint64_t len;
    
    uint8_t entries[0];
}log;

log_entry* log_append_entry(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs, log_entry* entry);
uint32_t log_entry_len(log_entry* entry);
