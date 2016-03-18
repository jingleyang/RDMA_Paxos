#ifndef ZOO_H
#define ZOO_H
#include "../util/common-header.h"
#include "../consensus/consensus.h"

struct znodes_data_t
{
	node_id_t node_id;
	uint32_t tail;
	int64_t c_time;
};
typedef struct znodes_data_t znodes_data;

#ifdef __cplusplus
extern "C" {
#endif

	int init_zookeeper(consensus_component* consensus_comp);

#ifdef __cplusplus
}
#endif

#endif