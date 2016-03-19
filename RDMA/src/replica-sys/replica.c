#include "../include/proxy/proxy.h"
#include "../include/util/common-header.h"
#include "../include/replica-sys/node.h"
#include "../include/config-comp/config-comp.h"
#include "../include/rdma/rdma_common.h"
#include <zookeeper.h>

#include <sys/stat.h>
/*
static zhandle_t *zh;
static int is_connected;

#define ZDATALEN 1024 * 1024

static void get_znode_name(const char *pathbuf, char *znode_name)
{
    const char *p = pathbuf;
    int i;
    for (i = strlen(pathbuf); i >= 0; i--)
    {
        if (*(p + i) == '/')
        {
            break;
        }
    }
    strcpy(znode_name, "/election/");
    strcat(znode_name, p + i + 1);
}

void zookeeper_init_watcher(zhandle_t *izh, int type, int state, const char *path, void *context)
{
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            is_connected = 1;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            is_connected = 0;
            zookeeper_close(izh);
        }
    }
}

uint32_t comp_tail(const void *a, const void *b)
{
    return *(uint32_t*)b-*(uint32_t*)a;
}

static int check_leader(consensus_component* consensus_comp)
{
    int rc, i, zoo_data_len = ZDATALEN;
    char str[512];
    
    sprintf(str, "%"PRId64",%"PRIu32"", consensus_comp->node_id, srv_data.tail);
    rc = zoo_set(zh, consensus_comp->znode_name, str, strlen(str), -1);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_set\n", rc);
    }
    struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    rc = zoo_get_children(zh, "/election", 0, children_list);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_get_children\n", rc);
    }

    char *p;
    //znodes_data *znodes = (znodes_data*)malloc(sizeof(znodes_data) * MAX_SERVER_COUNT);
    int64_t znodes_ctime[MAX_SERVER_COUNT];
    uint32_t znodes_tail[MAX_SERVER_COUNT];
    node_id_t znodes_nid[MAX_SERVER_COUNT];
    for (i = 0; i < children_list->count; ++i)
    {
        struct Stat stat;
        char *zoo_data = malloc(ZDATALEN * sizeof(char));
        char temp_znode_name[512];
        get_znode_name(children_list->data[i], temp_znode_name);

        rc = zoo_get(zh, temp_znode_name, 0, zoo_data, &zoo_data_len, &stat);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_get\n", rc);
        }
        p = strtok(zoo_data, ",");
        znodes_nid[i] = atoi(p);
        p = strtok(NULL, ",");
        znodes_tail[i] = atoi(p);
        znodes_ctime[i] = stat.ctime; // milliseconds
        free(zoo_data);
    }
    
    qsort(znodes_tail, children_list->count, sizeof(uint32_t), comp_tail);
    uint32_t max_tail = znodes_tail[0];

    qsort(znodes_ctime, children_list, sizeof(int64_t), comp_ctime);
    uint32_t max_c_time = znodes_ctime[0];

    for (i = 0; i < children_list->count; ++i)
    {
        if (znodes[i].c_time == max_c_time && znodes[i].tail == max_tail)
        {
            consensus_comp->cur_view.leader_id = znodes[i].node_id;
            break;
        }
    }

    if (consensus_comp->cur_view.leader_id == consensus_comp->node_id)
    {
        consensus_comp->my_role = LEADER;
        //fprintf(stderr, "I am the leader\n");
        for (i = 0; i < children_list->count - 1; ++i)
        {
            if (znodes[i].tail != znodes[i + 1].tail)
            {
                //recheck
            }
        }
    }else{
        consensus_comp->my_role = SECONDARY;
        //fprintf(stderr, "I am a follower\n");
        // RDMA read
        // update view
        // zoo_set
    }
    free(znodes);
    return 0;
}

void zoo_wget_children_watcher(zhandle_t *wzh, int type, int state, const char *path, void *watcherCtx) {
    if (type == ZOO_CHILD_EVENT)
    {
        // a change as occurred in the list of children.
        int rc;
        consensus_component* consensus_comp = (consensus_component*)watcherCtx;
        // block the threads
        rc = zoo_wget_children(wzh, "/election", zoo_wget_children_watcher, watcherCtx, NULL);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
        }
        check_leader(consensus_comp);   
    }
}

int init_zookeeper()
{
    srv_data.tail = 0;
    int rc;
    char path_buffer[512];
    zh = zookeeper_init(consensus_comp->zoo_host_port, zookeeper_init_watcher, 15000, 0, 0, 0);

    while(is_connected != 1);
    int interest, fd;
    struct timeval tv;
    zookeeper_interest(zh, &fd, &interest, &tv);
    consensus_comp->zfd = fd;

    rc = zoo_create(zh, "/election/guid-n_", NULL, 0, &ZOO_OPEN_ACL_UNSAFE, ZOO_SEQUENCE|ZOO_EPHEMERAL, path_buffer, 512);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_create\n", rc);
    }

    char znode_name[512];
    get_znode_name(path_buffer, znode_name);
    consensus_comp->znode_name = (char*)malloc(strlen(znode_name));
    strcpy(consensus_comp->znode_name, znode_name);
    //check_leader(consensus_comp);
    if ()
    {
        
    }

    rc = zoo_wget_children(zh, "/election", zoo_wget_children_watcher, (void*)consensus_comp, NULL);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
    }
    return 0;
}
*/
int initialize_node(node* my_node,const char* log_path, void* db_ptr,void* arg){

    int flag = 1;
    if(my_node->cur_view.leader_id==my_node->node_id){
        // zookeeper
    } else{
    }

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("CONSENSUS MODULE : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }
    if(!build_log_ret){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-consensus-sys.log",log_path,my_node->node_id);
                my_node->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==my_node->sys_log_file && (my_node->sys_log || my_node->stat_log)){
                err_log("CONSENSUS MODULE : System Log File Cannot Be Created.\n");
            }
    }

    if (my_node->cur_view.leader_id==my_node->node_id)
    {
        connect_peers(my_node->peer_pool[my_node->node_id].peer_address, 1, my_node->node_id);
    } else{
        connect_peers(my_node->peer_pool[my_node->cur_view.leader_id].peer_address, 0, my_node->node_id);
    }
    
    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,my_node->my_address,
            my_node->node_id,my_node->sys_log_file,my_node->sys_log,
            my_node->stat_log,my_node->db_name,db_ptr,my_node->group_size,
            &my_node->cur_view,&my_node->highest_to_commit,&my_node->highest_committed,
            &my_node->highest_seen,arg);
    if(NULL==my_node->consensus_comp){
        goto initialize_node_exit;
    }
    flag = 0;
initialize_node_exit:

    return flag;
}

node* system_initialize(node_id_t node_id, const char* start_mode,const char* config_path, const char* log_path, void* db_ptr,void* arg){

    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node,0,sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }

    my_node->node_id = node_id;
    my_node->db_ptr = db_ptr;

    if(*start_mode=='s'){
        my_node->cur_view.view_id = 1;
        my_node->cur_view.leader_id = my_node->node_id;
        my_node->cur_view.req_id = 0;
    }else{
        my_node->cur_view.view_id = 0;
        my_node->cur_view.leader_id = 9999;
        my_node->cur_view.req_id = 0;
    }

    if(consensus_read_config(my_node,config_path)){
        err_log("CONSENSUS MODULE : Configuration File Reading Failed.\n");
        goto exit_error;
    }


    if(initialize_node(my_node,log_path,db_ptr,arg)){
        err_log("CONSENSUS MODULE : Network Layer Initialization Failed.\n");
        goto exit_error;
    }

    return my_node;

exit_error:
    if(NULL!=my_node){
    }

    return NULL;
}