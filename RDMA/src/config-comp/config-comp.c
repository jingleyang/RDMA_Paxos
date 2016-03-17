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
    if(group_size <= comp->node_id){
        goto goto_config_error;
    }
    comp->group_size = group_size;

    config_setting_t *zookeeper_config = NULL;
    zookeeper_config = config_lookup(&config_file,"zookeeper_config");

    if(NULL==zookeeper_config){
        goto goto_config_error;
    }

    config_setting_t *zoo_ele = config_setting_get_elem(zookeeper_config,comp->node_id);

    if(NULL == zoo_ele){
        goto goto_config_error;
    }

    const char* peer_ipaddr = NULL;
    int peer_port = -1;

    if(!config_setting_lookup_string(zoo_ele,"ip_address",&peer_ipaddr)){

        goto goto_config_error;
    }

    if(!config_setting_lookup_int(zoo_ele,"port",&peer_port)){
        goto goto_config_error;
    }

    char *port, str[16];
    sprintf(str, ":%d", peer_port);
    port = str;
    comp->zoo_host_port = (char*)malloc(strlen(peer_ipaddr) + strlen(port) + 1);
    sprintf(comp->zoo_host_port, "%s%s", peer_ipaddr, port);

    config_setting_t *server_config = NULL;
    server_config = config_lookup(&config_file,"server_config");

    if(NULL==server_config){
        goto goto_config_error;
    }

    config_setting_t *serv_ele = config_setting_get_elem(server_config,comp->node_id);

    if(NULL == serv_ele){
        goto goto_config_error;
    }

    peer_ipaddr=NULL;
    peer_port=-1;

    if(!config_setting_lookup_string(serv_ele,"ip_address",&peer_ipaddr)){

        goto goto_config_error;
    }

    if(!config_setting_lookup_int(serv_ele,"port",&peer_port)){
        goto goto_config_error;
    }

    comp->my_address.sin_port = htons(peer_port);
    comp->my_address.sin_family = AF_INET;
    inet_pton(AF_INET,peer_ipaddr,&comp->my_address.sin_addr);

    const char* db_name;
    if(!config_setting_lookup_string(serv_ele,"db_name",&db_name)){
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

    comp->peer_pool = (peer*)malloc(group_size * sizeof(peer));
    config_setting_t *nodes_config;
    nodes_config = config_lookup(&config_file, "rdma_config");

    if(NULL == nodes_config){
        goto goto_config_error;
    }

    peer* peer_pool = comp->peer_pool;
    for(uint32_t i = 0; i < group_size; i++){ 
        config_setting_t *node_config = config_setting_get_elem(nodes_config, i);
        if(NULL == node_config){
            goto goto_config_error;
        }

        peer_ipaddr = NULL;
        peer_port = -1;

        if(!config_setting_lookup_string(node_config, "ip_address", &peer_ipaddr)){
            goto goto_config_error;
        }
        if(!config_setting_lookup_int(node_config, "port", &peer_port)){
            goto goto_config_error;
        }

        peer_pool[i].peer_address = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        peer_pool[i].sock_len = sizeof(struct sockaddr_in);
        peer_pool[i].peer_address->sin_family =AF_INET;
        inet_pton(AF_INET,peer_ipaddr,&peer_pool[i].peer_address->sin_addr);
        peer_pool[i].peer_address->sin_port = htons(peer_port);
    }

    config_destroy(&config_file);
    return 0;

goto_config_error:
    con_err_log("%s:%d - %s\n", config_error_file(&config_file), config_error_line(&config_file), config_error_text(&config_file));
    config_destroy(&config_file);
    return -1;
};
