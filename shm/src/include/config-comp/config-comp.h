#ifndef CONFIG_COMP_H
#define CONFIG_COMP_H
#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include <libconfig.h>

int consensus_read_config(struct consensus_component_t* comp,const char* config_file);

#endif