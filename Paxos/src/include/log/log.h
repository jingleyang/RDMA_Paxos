#include "../util/common-header.h"
#include "../consensus/consensus.h"

#define LOG_SIZE  16384*PAGE_SIZE

typedef struct log_entry_t log_entry;

typedef struct log_t log;

log_entry* log_append_entry(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs, log* lg);