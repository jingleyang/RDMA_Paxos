#ifndef SHM_H
#define SHM_H
#include "../util/common-structure.h"
#include "../log/log.h"

struct shm_data_t {
	log_entry* shm[MAX_SERVER_COUNT];
	log*  shm_log;
};

typedef struct shm_data_t shm_data;

extern shm_data shared_memory;

void init_shm(node_id_t node_id, int size);

#endif
