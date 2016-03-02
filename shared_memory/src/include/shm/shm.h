#ifndef SHM_H
#define SHM_H
#include "../util/common-structure.h"
#include "../log/log.h"

struct shm_data_t {
	void* shm[MAX_SERVER_COUNT];
	uint64_t tail;
};

typedef struct shm_data_t shm_data;

extern shm_data shared_memory;

#ifdef __cplusplus
extern "C" {
#endif

	void init_shm(node_id_t node_id, int size);

#ifdef __cplusplus
}
#endif

#endif
