#ifndef CONFIG_COMP_H
#define CONFIG_COMP_H
#include "../util/common-header.h"
#include <libconfig.h>

int consensus_read_config(consensus_component* comp,const char* config_file);

#endif