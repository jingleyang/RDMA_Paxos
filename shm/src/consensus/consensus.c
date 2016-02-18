#include "../include/consensus/consensus.h"
#include "../include/consensus/consensus-msg.h"

#include "../include/shm/shm.h"
#include "../include/config-comp/config-comp.h"

#include <sys/stat.h>

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))

consensus_component* init_consensus_comp(const char* config_path, const char* log_path, node_id_t node_id, const char* start_mode){
    consensus_component* comp = (consensus_component*)malloc(sizeof(consensus_component));
    memset(comp, 0, sizeof(consensus_component));

    if(NULL != comp){
        comp->node_id = node_id;
        consensus_read_config(comp, config_path);
        if(*start_mode == 's'){
            comp->cur_view.view_id = 1;
            comp->cur_view.leader_id = comp->node_id;
            comp->cur_view.req_id = 0;
        }else{
            comp->cur_view.view_id = 1;
            comp->cur_view.req_id = 0;
            comp->cur_view.leader_id = 0; //TODO
        }

        int build_log_ret = 0;
        if(log_path == NULL){
            log_path = ".";
        }else{
            if((build_log_ret = mkdir(log_path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)) != 0){
                if(errno != EEXIST){
                    con_err_log("Log Directory Creation Failed, No Log Will Be Recorded.\n");
                }else{
                    build_log_ret = 0;
                }
            }
        }

        if(!build_log_ret){
            char* con_log_path = (char*)malloc(sizeof(char)*strlen(log_path) + 50);
            memset(con_log_path, 0, sizeof(char)*strlen(log_path) + 50);
            if(NULL != con_log_path){
                sprintf(con_log_path, "%s/node-%u-consensus.log", log_path, comp->node_id);
                comp->con_log_file = fopen(con_log_path, "w");
                free(con_log_path);
            }
        }

        if(comp->cur_view.leader_id == comp->node_id){
            comp->my_role = LEADER;
        }else{
            comp->my_role = SECONDARY;
        }
        comp->highest_seen_vs.view_id = 1;
        comp->highest_seen_vs.req_id = 0;
        comp->committed.view_id = 1; 
        comp->committed.req_id = 0;
        comp->db_ptr = initialize_db(comp->db_name, 0);
        
        pthread_mutex_init(&comp->mutex, NULL);
        printf("my db name is %s\n",comp->db_name);
    }
    return comp;
}

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs.view_id;
    next_vs.req_id = (comp->highest_seen_vs.req_id + 1);
    return next_vs;
};

static void view_stamp_inc(view_stamp vs){
    vs.req_id++;
    return;
};

static void update_record(request_record* record, uint32_t node_id){
    record->bit_map = (record->bit_map | (1<<node_id));
    return;
}

static int reached_quorum(request_record* record, int group_size){
    // this may be compatibility issue 
    if(__builtin_popcountl(record->bit_map) >= ((group_size/2)+1)){
        return 1;
    }else{
        return 0;
    }
}

int rsm_op(struct consensus_component_t* comp, void* data, size_t data_size){
    int ret = 1;
    pthread_mutex_lock(&comp->mutex);
    view_stamp next = get_next_view_stamp(comp);

    /* record the data persistently */
    db_key_type record_no = vstol(next);
    request_record* record_data = (request_record*)malloc(data_size + sizeof(request_record));
    gettimeofday(&record_data->created_time, NULL);
    record_data->bit_map = (1<<comp->node_id);
    record_data->data_size = data_size;
    memcpy(record_data->data, data, data_size);
    if(store_record(comp->db_ptr, sizeof(record_no), &record_no, REQ_RECORD_SIZE(record_data), record_data))
    {
        goto handle_submit_req_exit;
    }
    ret = 0;
    comp->highest_seen_vs.req_id = comp->highest_seen_vs.req_id + 1;
    printf("highest seen vs req id is %d\n", comp->highest_seen_vs.req_id);
    log_entry* new_entry = log_append_entry(comp, data_size, data, &next, shared_memory.shm[comp->node_id]);
    shared_memory.shm[comp->node_id] = (log_entry*)((char*)shared_memory.shm[comp->node_id] + log_entry_len(new_entry));//TODO pointer move
    pthread_mutex_unlock(&comp->mutex);
    CON_LOG(comp, "the new entry's msg_vs view id is %d and req id is %d, req_canbe_exed view id is %d and req id is %d\n", new_entry->msg_vs.view_id, new_entry->msg_vs.req_id, new_entry->req_canbe_exed.view_id, new_entry->req_canbe_exed.req_id);
    if(comp->group_size > 1){
        for (int i = 0; i < comp->group_size; i++) {
            //TODO RDMA write

            if (i == comp->node_id)
                continue;
            memcpy(shared_memory.shm[i], new_entry, log_entry_len(new_entry));
            shared_memory.shm[i] = (log_entry*)((char*)shared_memory.shm[i] + log_entry_len(new_entry));//TODO pointer move
        }

recheck:
        for (int i = 0; i < MAX_SERVER_COUNT; i++)
        {
            if (new_entry->ack[i].msg_vs.view_id == next.view_id && new_entry->ack[i].msg_vs.req_id == next.req_id)
            {
                update_record(record_data, new_entry->ack[i].node_id);
                store_record(comp->db_ptr, sizeof(record_no), &record_no, REQ_RECORD_SIZE(record_data), record_data);
            }
        }
        if (reached_quorum(record_data, comp->group_size))
        {
            goto handle_submit_req_exit;
        }else{
            goto recheck;
        }
    }else{
        CON_LOG(comp, "group_size <= 1, execute by myself.\n");
    }
handle_submit_req_exit: 
    if(record_data != NULL){
        free(record_data);
    }
    //TODO: do we need the lock here?`
    comp->committed.req_id = comp->committed.req_id + 1;
    return ret;
}

static void* build_accept_ack(consensus_component* comp, view_stamp* vs){
    accept_ack* msg = (accept_ack*)malloc(ACCEPT_ACK_SIZE);
    if(NULL != msg){
        msg->node_id = comp->node_id;
        msg->msg_vs = *vs;
    }
    return msg;
};

void *handle_accept_req(void* arg)
{
    consensus_component* comp = arg;
  
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    int connected = 0;
    
    while (1)
    {
        log_entry* new_entry = shared_memory.shm[comp->node_id];
        
        if (new_entry->req_canbe_exed.view_id != 0)//TODO atmoic opeartion
        {
            if(connected == 0){
                connect(sock, (struct sockaddr*)&comp->sys_addr.c_addr, comp->sys_addr.c_sock_len);
                connected = 1;
            }
            CON_LOG(comp, "Replica handle new req. The new entry's msg_vs view id is %d and req id is %d, req_canbe_exed view id is %d and req id is %d\n", new_entry->msg_vs.view_id, new_entry->msg_vs.req_id, new_entry->req_canbe_exed.view_id, new_entry->req_canbe_exed.req_id)
            if(new_entry->msg_vs.view_id < comp->cur_view.view_id){
                // TODO
                //goto reloop;
            }
            // if we this message is not from the current leader
            if(new_entry->msg_vs.view_id == comp->cur_view.view_id && new_entry->node_id != comp->cur_view.leader_id){
                // TODO
                //goto reloop;
            }

            // update highest seen request
            if(view_stamp_comp(new_entry->msg_vs, comp->highest_seen_vs) > 0){
                comp->highest_seen_vs = new_entry->msg_vs;
            }

            db_key_type record_no = vstol(new_entry->msg_vs);
            request_record* record_data = (request_record*)malloc(new_entry->data_size + sizeof(request_record));

            gettimeofday(&record_data->created_time, NULL);
            record_data->data_size = new_entry->data_size;
            memcpy(record_data->data, new_entry->data, new_entry->data_size);

            // record the data persistently 
            store_record(comp->db_ptr, sizeof(record_no), &record_no, REQ_RECORD_SIZE(record_data), record_data);
            shared_memory.shm[comp->node_id] = (log_entry*)((char*)shared_memory.shm[comp->node_id] + log_entry_len(new_entry));//TODO pointer move

            accept_ack* reply = build_accept_ack(comp, &new_entry->msg_vs);

            accept_ack* offset = (accept_ack*)(shared_memory.shm[new_entry->node_id]);
            for (int i = 0; i < comp->node_id; ++i)
            {
                offset++;
            }

            memcpy(offset, reply, ACCEPT_ACK_SIZE);

            shared_memory.shm[new_entry->node_id] = (log_entry*)((char*)shared_memory.shm[new_entry->node_id] + log_entry_len(new_entry));//TODO pointer move

            size_t data_size;
            record_data = NULL;
            if(view_stamp_comp(new_entry->req_canbe_exed, comp->committed) > 0)
            {
                db_key_type start = vstol(comp->committed)+1;
                db_key_type end = vstol(new_entry->req_canbe_exed);
                for(db_key_type index = start; index <= end; index++)
                {
                    retrieve_record(comp->db_ptr, sizeof(index), &index, &data_size, (void**)&record_data);
                    CON_LOG(comp, "Now I can exed a request. view id is %d, req id is %d.\n", ltovs(index).view_id, ltovs(index).req_id);
                    send(sock, record_data->data, record_data->data_size, 0);
                }
                comp->committed = new_entry->req_canbe_exed;
            }
        }
    }
};
