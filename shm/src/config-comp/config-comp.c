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

    if(group_size <= comp->node_id){
        con_err_log("CONSENSUS : Invalid Node Id\n");
        goto goto_config_error;
    }

    config_setting_t *consensus_config = NULL;
    consensus_config = config_lookup(&config_file, "consensus_config");

    if(NULL == consensus_config){
        con_err_log("CONSENSUS : Cannot Find Nodes Settings.\n");
        goto goto_config_error;
    }    

    config_setting_t *con_ele = config_setting_get_elem(consensus_config, comp->node_id);
    if(NULL == con_ele){
        con_err_log("CONSENSUS : Cannot Find Current Node's Address Section.\n");
        goto goto_config_error;
    }
    const char* my_ipaddr=NULL;
    int my_port=-1;

    if(!config_setting_lookup_string(con_ele, "ip_address", &my_ipaddr)){
        con_err_log("CONSENSUS : Cannot Find Current Node's IP Address.\n");	
        goto goto_config_error;
    }

    if(!config_setting_lookup_int(con_ele, "port", &my_port)){
        con_err_log("CONSENSUS : Cannot Find Current Node's Port.\n");
        goto goto_config_error;
    }

    comp->sys_addr.c_addr.sin_port = htons(my_port);
    comp->sys_addr.c_addr.sin_family = AF_INET;
    inet_pton(AF_INET, my_ipaddr, &comp->sys_addr.c_addr.sin_addr);
    comp->sys_addr.c_sock_len = sizeof(comp->sys_addr.c_addr);

    const char* db_name;
    if(!config_setting_lookup_string(con_ele, "db_name", &db_name)){
        goto goto_config_error;
    }
    size_t db_name_len = strlen(db_name);
    comp->db_name = (char*)malloc(sizeof(char)*(db_name_len + 1));
    if(comp->db_name == NULL){
        goto goto_config_error;
    }
    if(NULL == strncpy(comp->db_name, db_name, db_name_len)){
        free(comp->db_name);
        goto goto_config_error;
    }
    comp->db_name[db_name_len] = '\0';
    config_destroy(&config_file);
    return 0;

goto_config_error:
    con_err_log("%s:%d - %s\n", config_error_file(&config_file), config_error_line(&config_file), config_error_text(&config_file));
    config_destroy(&config_file);
    return -1;
};
