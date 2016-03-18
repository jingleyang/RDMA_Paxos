#include "../include/consensus/consensus.h"
#include "../include/consensus/consensus-msg.h"

#include "../include/rdma/rdma_common.h"
#include "../include/log/log.h"
#include "../include/config-comp/config-comp.h"

#include <sys/stat.h>

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))

void init_consensus_comp(consensus_component* consensus_comp, const char* log_path, node_id_t node_id){
        consensus_comp->cur_view.view_id = 1;
        consensus_comp->cur_view.req_id = 0;
        /* memset(consensus_comp, 0, sizeof(consensus_component))
         * LEADER = 0
         * during zookeeper_init, we go into rsm_op in recv accidentally*/
        consensus_comp->my_role = SECONDARY;
        consensus_comp->highest_seen_vs.view_id = 1;
        consensus_comp->highest_seen_vs.req_id = 0;
        consensus_comp->committed.view_id = 1; 
        consensus_comp->committed.req_id = 0;
        pthread_mutex_init(&consensus_comp->mutex, NULL);

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
                sprintf(con_log_path, "%s/node-%u-consensus.log", log_path, consensus_comp->node_id);
                consensus_comp->con_log_file = fopen(con_log_path, "w");
                free(con_log_path);
            }
        }
        consensus_comp->db_ptr = initialize_db(consensus_comp->db_name, 0);
}

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs.view_id;
    next_vs.req_id = (comp->highest_seen_vs.req_id + 1);
    return next_vs;
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
    CON_LOG(comp, "Leader trying to reach a consensus on view id %d, req id %d\n", next.view_id, next.req_id);

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
    uint64_t offset = srv_data.tail;
    log_entry* new_entry = log_append_entry(comp, data_size, data, &next, srv_data.log_mr, srv_data.tail);
    srv_data.tail = srv_data.tail + log_entry_len(new_entry);

    for (int i = 0; i < comp->group_size; i++) {
        if (i == comp->node_id)
            continue;
        rdma_write(i, new_entry, log_entry_len(new_entry), offset);
    }
    pthread_mutex_unlock(&comp->mutex);
    
recheck:
    for (int i = 0; i < MAX_SERVER_COUNT; i++) {
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
handle_submit_req_exit: 
    if(record_data != NULL){
        free(record_data);
    }
    //TODO: do we need the lock here?
    while (new_entry->msg_vs.req_id > comp->committed.req_id + 1);
    comp->committed.req_id = comp->committed.req_id + 1;
    CON_LOG(comp, "Leader finished the consensus on view id %d, req id %d\n", next.view_id, next.req_id);
    return ret;
}

void *handle_accept_req(void* arg)
{
    consensus_component* comp = arg;

    db_key_type start;
    db_key_type end;
    db_key_type index;

    size_t data_size;
    request_record* retrieve_data= NULL;
    
    while (1)
    {
        log_entry* new_entry = (log_entry*)((char*)srv_data.log_mr + srv_data.tail);
        
        if (new_entry->req_canbe_exed.view_id != 0)//TODO atmoic opeartion
        {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            connect(sock, (struct sockaddr*)&comp->my_address, sizeof(struct sockaddr_in)); //TODO: why? Broken pipe. Maybe the server closes the socket
            CON_LOG(comp, "Replica %d handling view id %d req id %d\n", comp->node_id, new_entry->msg_vs.view_id, new_entry->msg_vs.req_id);
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
            uint64_t offset = srv_data.tail + ACCEPT_ACK_SIZE * comp->node_id;
            srv_data.tail = srv_data.tail + log_entry_len(new_entry);

            accept_ack* reply = (accept_ack*)((char*)new_entry + ACCEPT_ACK_SIZE * comp->node_id);
            reply->node_id = comp->node_id;
            reply->msg_vs.view_id = new_entry->msg_vs.view_id;
            reply->msg_vs.req_id = new_entry->msg_vs.req_id;

            rdma_write(new_entry->node_id, reply, ACCEPT_ACK_SIZE, offset);

            free(record_data);
            if(view_stamp_comp(new_entry->req_canbe_exed, comp->committed) > 0)
            {
                start = vstol(comp->committed)+1;
                end = vstol(new_entry->req_canbe_exed);
                for(index = start; index <= end; index++)
                {
                    retrieve_record(comp->db_ptr, sizeof(index), &index, &data_size, (void**)&retrieve_data);
                    send(sock, retrieve_data->data, retrieve_data->data_size, 0);
                    CON_LOG(comp, "Replica %d try to exed view id %d req id %d\n", comp->node_id, ltovs(index).view_id, ltovs(index).req_id);
                }
                comp->committed = new_entry->req_canbe_exed;
            }
        }
    }
};
