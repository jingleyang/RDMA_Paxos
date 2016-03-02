#include "../include/log/log.h"

uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}
