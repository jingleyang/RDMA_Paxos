#ifndef ZOO_H
#define ZOO_H
#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include <zookeeper.h>

int init_zookeeper(consensus_component* consensus_comp);

#endif