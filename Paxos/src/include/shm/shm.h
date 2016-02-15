#ifndef SHM_H
#define SHM_H
#include "../util/common-structure.h"

struct shm_data_t {
	log_t  *log;
	log_entry_t* shm[group_size];
};
typedef struct shm_data_t shm_data_t;

#ifdef __cplusplus
extern "C" {
#endif

	void init_shm(node_id_t node_id, int size);

#ifdef __cplusplus
}
#endif

#endif
