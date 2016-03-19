#include "../include/proxy/proxy.h"
#include "../include/config-comp/config-proxy.h"
#include <sys/stat.h>

proxy_node* proxy_init(node_id_t node_id, const char* start_mode, const char* config_path, const char* log_path){
    
    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        err_log("PROXY : Cannot Malloc Memory For The Proxy.\n");
        goto proxy_exit_error;
    }

    memset(proxy, 0, sizeof(proxy_node));

    proxy->node_id = node_id;

    if(proxy_read_config(proxy,config_path)){
        err_log("PROXY : Configuration File Reading Error.\n");
        goto proxy_exit_error;
    }

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("PROXY : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }

    if(!build_log_ret){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-proxy-sys.log",log_path,proxy->node_id);
                proxy->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==proxy->sys_log_file && (proxy->sys_log || proxy->stat_log)){
                err_log("PROXY : System Log File Cannot Be Created.\n");
            }
            char* req_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(req_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=req_log_path){
                sprintf(req_log_path,"%s/node-%u-proxy-req.log",log_path,proxy->node_id);
                proxy->req_log_file = fopen(req_log_path,"w");
                free(req_log_path);
            }
            if(NULL==proxy->req_log_file && proxy->req_log){
                err_log("PROXY : Client Request Log File Cannot Be Created.\n");
            }
    }

    proxy->db_ptr = initialize_db(proxy->db_name,0);

    if(proxy->db_ptr==NULL){
        err_log("PROXY : Cannot Set Up The Database.\n");
        goto proxy_exit_error;
    }
    
    proxy->con_node = system_initialize(node_id,start_mode,config_path,log_path,proxy->db_ptr,proxy);

    if(NULL==proxy->con_node){
        err_log("PROXY : Cannot Initialize Consensus Component.\n");
        goto proxy_exit_error;
    }

	return proxy;

proxy_exit_error:
    if(NULL!=proxy){
        if(NULL!=proxy->con_node){
        }
        free(proxy);
    }
    return NULL;
}