#include "../include/config-comp/config-comp.h"

int consensus_read_config(struct consensus_component_t* comp, const char* config_path){
    config_t config_file;
    config_init(&config_file);

    if(!config_read_file(&config_file, config_path)){
        goto goto_config_error;
    }
    
    uint32_t group_size;

    if(!config_lookup_int(&config_file, "group_size", (int*)&group_size)){
        goto goto_config_error;
    }
    comp->group_size = group_size;
    comp->peer_pool = (peer*)malloc(group_size * sizeof(peer));

    if(group_size <= comp->node_id){
        con_err_log("CONSENSUS : Invalid Node Id\n");
        goto goto_config_error;
    }

    config_setting_t *nodes_config;
    nodes_config = config_lookup(&config_file, "consensus_config");

    if(NULL == nodes_config){
        con_err_log("CONSENSUS : Cannot Find Nodes Settings.\n");
        goto goto_config_error;
    }

    peer* peer_pool = comp->peer_pool;
    for(uint32_t i = 0; i < group_size; i++){ 
        config_setting_t *node_config = config_setting_get_elem(nodes_config, i);
        if(NULL==node_config){
            con_err_log("CONSENSUS : Cannot Find Node%u's Address.\n", i);
            goto goto_config_error;
        }

        const char* peer_ipaddr;
        int peer_port;
        if(!config_setting_lookup_string(node_config,"ip_address",&peer_ipaddr)){
            goto goto_config_error;
        }
        if(!config_setting_lookup_int(node_config,"port",&peer_port)){
            goto goto_config_error;
        }

        peer_pool[i].peer_address = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        peer_pool[i].sock_len = sizeof(struct sockaddr_in);
        peer_pool[i].peer_address->sin_family =AF_INET;
        inet_pton(AF_INET,peer_ipaddr,&peer_pool[i].peer_address->sin_addr);
        peer_pool[i].peer_address->sin_port = htons(peer_port);

        if(i == comp->node_id){
            const char* db_name;
            if(!config_setting_lookup_string(node_config,"db_name",&db_name)){
                goto goto_config_error;
            }
            size_t db_name_len = strlen(db_name);
            comp->db_name = (char*)malloc(sizeof(char)*(db_name_len+1));
            if(comp->db_name==NULL){
                goto goto_config_error;
            }
            if(NULL==strncpy(comp->db_name,db_name,db_name_len)){
                free(comp->db_name);
                goto goto_config_error;
            }

            comp->db_name[db_name_len] = '\0';
            comp->my_address.sin_port = htons(peer_port);
            comp->my_address.sin_family = AF_INET;
            inet_pton(AF_INET,peer_ipaddr,&comp->my_address.sin_addr);
        }
    }

    config_destroy(&config_file);
    return 0;

goto_config_error:
    con_err_log("%s:%d - %s\n", config_error_file(&config_file), config_error_line(&config_file), config_error_text(&config_file));
    config_destroy(&config_file);
    return -1;
};
