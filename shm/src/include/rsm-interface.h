#include "util/common-structure.h"
#include "consensus/consensus.h"

#ifdef __cplusplus
extern "C" {
#endif

	consensus_component* init_consensus_comp(const char* config_path, const char* log_path, int64_t node_id, const char* start_mode);

	void handle_accept_req(consensus_component* comp);

	int rsm_op(consensus_component* comp, void* data, size_t data_size);
	void init_shm(int64_t node_id, int size);
#ifdef __cplusplus
}
#endif

