int server_read_config(struct consensus_component_t* comp, const char* config_path){
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
        err_log("SERVER : Invalid Node Id\n");
        goto goto_config_error;
    }

    const char* log_path;
    if(config_lookup_int(&config_file, "log_path", &log_path){
        comp->log_path = log_path;
    }

    config_setting_t *server_config = NULL;
    server_config = config_lookup(&config_file, "server_config");

    if(NULL == server_config){
        err_log("SERVER : Cannot Find Nodes Settings.\n");
        goto goto_config_error;
    }    

    config_setting_t *ser_ele = config_setting_get_elem(server_config, comp->node_id);

    if(NULL == ser_ele){
        err_log("SERVER : Cannot Find Current Node's Address Section.\n");
        goto goto_config_error;
    }

// read the option for log, if it has some sections
    
    config_setting_lookup_int(pro_ele,"time_stamp_log",&cur_node->ts_log);
    config_setting_lookup_int(pro_ele,"sys_log",&cur_node->sys_log);
    config_setting_lookup_int(pro_ele,"stat_log",&cur_node->stat_log);
    config_setting_lookup_int(pro_ele,"req_log",&cur_node->req_log);

    const char* peer_ipaddr=NULL;
    int peer_port=-1;

    if(!config_setting_lookup_string(ser_ele,"ip_address",&peer_ipaddr)){
        err_log("SERVER : Cannot Find Current Node's IP Address.\n")
        goto goto_config_error;
    }

    if(!config_setting_lookup_int(ser_ele,"port",&peer_port)){
        err_log("SERVER : Cannot Find Current Node's Port.\n")
        goto goto_config_error;
    }

    comp->sys_addr.my_addr.sin_port = htons(peer_port);
    comp->sys_addr.my_addr.sin_family = AF_INET;
    inet_pton(AF_INET, peer_ipaddr, &comp->sys_addr.my_addr.sin_addr);
    comp->sys_addr.my_sock_len = sizeof(comp->sys_addr.my_addr);

    const char* db_name;
    if(!config_setting_lookup_string(ser_ele, "db_name", &db_name)){
        goto goto_config_error;
    }
    size_t db_name_len = strlen(db_name);
    comp->db_name = (char*)malloc(sizeof(char)*(db_name_len+1));
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
    err_log("%s:%d - %s\n", config_error_file(&config_file), config_error_line(&config_file), config_error_text(&config_file));
    config_destroy(&config_file);
    return -1;
}