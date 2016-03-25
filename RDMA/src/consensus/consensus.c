#include "../include/consensus/consensus.h"
#include "../include/consensus/consensus-msg.h"

#include "../include/rdma/rdma_common.h"

typedef struct log_entry_t{
    accept_ack ack[MAX_SERVER_COUNT];

    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    node_id_t node_id;
    size_t data_size;
    char data[0];
}log_entry;

uint32_t log_entry_len(log_entry* entry)
{
    return (uint32_t)(sizeof(log_entry) + entry->data_size);
}

consensus_component* init_consensus_comp(struct node_t* node,struct sockaddr_in my_address,pthread_mutex_t* lock,void* udata,uint32_t node_id,FILE* log,int sys_log,int stat_log,const char* db_name,void* db_ptr,int group_size,
        view* cur_view,view_stamp* to_commit,view_stamp* highest_committed_vs,view_stamp* highest,void* arg){
    consensus_component* comp = (consensus_component*)malloc(sizeof(consensus_component));
    memset(comp,0,sizeof(consensus_component));

    if(NULL!=comp){
        comp->db_ptr = db_ptr;  
        comp->sys_log = sys_log;
        comp->stat_log = stat_log;
        comp->sys_log_file = log;
        comp->my_node = node;
        comp->node_id = node_id;
        comp->group_size = group_size;
        comp->cur_view = cur_view;
        if(comp->cur_view->leader_id == comp->node_id){
            comp->my_role = LEADER;
        }else{
            comp->my_role = SECONDARY;
        }
        comp->highest_seen_vs = highest;
        comp->highest_seen_vs->view_id = 1;
        comp->highest_seen_vs->req_id = 0;
        comp->highest_committed_vs = highest_committed_vs; 
        comp->highest_committed_vs->view_id = 1; 
        comp->highest_committed_vs->req_id = 0; 
        comp->highest_to_commit_vs = to_commit;
        comp->highest_to_commit_vs->view_id = 1;
        comp->highest_to_commit_vs->req_id = 0;
        comp->my_address = my_address;
        comp->lock = lock;
        comp->udata = udata;

        goto consensus_init_exit;

    }
consensus_init_exit:
    return comp;
}

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs->view_id;
    next_vs.req_id = (comp->highest_seen_vs->req_id+1);
    return next_vs;
};

static int reached_quorum(uint64_t bit_map, int group_size){
    // this may be compatibility issue 
    if(__builtin_popcountl(bit_map) >= ((group_size/2)+1)){
        return 1;
    }else{
        return 0;
    }
}

int rsm_op(struct consensus_component_t* comp, void* data, size_t data_size){
    int ret = 1;
    pthread_mutex_lock(comp->lock);
    struct resources *res = comp->udata;
    view_stamp next = get_next_view_stamp(comp);
    SYS_LOG(comp, "Leader trying to reach a consensus on view id %d, req id %d\n", next.view_id, next.req_id);

    /* record the data persistently */
    db_key_type record_no = vstol(&next);
    uint64_t bit_map = (1<<comp->node_id);
    if(store_record(comp->db_ptr, sizeof(record_no), &record_no, data_size, data))
    {
        goto handle_submit_req_exit;
    }
    ret = 0;

    comp->highest_seen_vs->req_id = comp->highest_seen_vs->req_id + 1;
    uint64_t offset = res->end;
    log_entry *new_entry = (log_entry*)((char*)res->buf + res->end);
    new_entry->node_id = comp->node_id;
    new_entry->req_canbe_exed.view_id = comp->highest_committed_vs->view_id;
    new_entry->req_canbe_exed.req_id = comp->highest_committed_vs->req_id;
    new_entry->data_size = data_size;
    new_entry->msg_vs = next;
    memcpy(new_entry->data, data, data_size);
    res->tail = res->end;
    res->end = res->end + log_entry_len(new_entry);

    for (int i = 0; i < comp->group_size; i++) {
        if (i == comp->node_id)
            continue;
        rdma_write(i, new_entry, log_entry_len(new_entry), offset, comp->udata);
    }
    pthread_mutex_unlock(comp->lock);
    
recheck:
    for (int i = 0; i < MAX_SERVER_COUNT; i++) {
        if (new_entry->ack[i].msg_vs.view_id == next.view_id && new_entry->ack[i].msg_vs.req_id == next.req_id)
        {
            bit_map = bit_map | (1<<new_entry->ack[i].node_id);
        }
    }
    if (reached_quorum(bit_map, comp->group_size))
    {
        goto handle_submit_req_exit;
    }else{
        goto recheck;
    }
handle_submit_req_exit: 
    //TODO: do we need the lock here?
    while (new_entry->msg_vs.req_id > comp->highest_committed_vs->req_id + 1);
    comp->highest_committed_vs->req_id = comp->highest_committed_vs->req_id + 1;
    SYS_LOG(comp, "Leader finished the consensus on view id %d, req id %d\n", next.view_id, next.req_id);
    return ret;
}

void *handle_accept_req(void* arg)
{
    consensus_component* comp = arg;

    db_key_type start;
    db_key_type end;
    db_key_type index;

    size_t data_size;
    void* retrieve_data = NULL;

    struct resources *res = comp->udata;

    for (;;)
    {
    	while(comp->cur_view->leader_id != comp->node_id)
    	{
	        log_entry* new_entry = (log_entry*)((char*)res->buf + res->end);
	        
	        if (new_entry->req_canbe_exed.view_id != 0)//TODO atmoic opeartion
	        {
	            int sock = socket(AF_INET, SOCK_STREAM, 0);
	            connect(sock, (struct sockaddr*)&comp->my_address, sizeof(struct sockaddr_in)); //TODO: why? Broken pipe. Maybe the server closes the socket
	            SYS_LOG(comp, "Replica %d handling view id %d req id %d\n", comp->node_id, new_entry->msg_vs.view_id, new_entry->msg_vs.req_id);
	            if(new_entry->msg_vs.view_id < comp->cur_view->view_id){
	                // TODO
	                //goto reloop;
	            }
	            // if we this message is not from the current leader
	            if(new_entry->msg_vs.view_id == comp->cur_view->view_id && new_entry->node_id != comp->cur_view->leader_id){
	                // TODO
	                //goto reloop;
	            }

	            // update highest seen request
	            if(view_stamp_comp(&new_entry->msg_vs, comp->highest_seen_vs) > 0){
	                *(comp->highest_seen_vs) = new_entry->msg_vs;
	            }

	            db_key_type record_no = vstol(&new_entry->msg_vs);

	            // record the data persistently 
	            store_record(comp->db_ptr, sizeof(record_no), &record_no, new_entry->data_size, new_entry->data);
	            uint64_t offset = res->end + ACCEPT_ACK_SIZE * comp->node_id;
	            res->tail = res->end;
	            res->end = res->end + log_entry_len(new_entry);
	            
	            accept_ack* reply = (accept_ack*)((char*)new_entry + ACCEPT_ACK_SIZE * comp->node_id);
	            reply->node_id = comp->node_id;
	            reply->msg_vs.view_id = new_entry->msg_vs.view_id;
	            reply->msg_vs.req_id = new_entry->msg_vs.req_id;

	            rdma_write(new_entry->node_id, reply, ACCEPT_ACK_SIZE, offset, comp->udata);

	            if(view_stamp_comp(&new_entry->req_canbe_exed, comp->highest_committed_vs) > 0)
	            {
	                start = vstol(comp->highest_committed_vs)+1;
	                end = vstol(&new_entry->req_canbe_exed);
	                for(index = start; index <= end; index++)
	                {
	                    retrieve_record(comp->db_ptr, sizeof(index), &index, &data_size, (void**)&retrieve_data);
	                    send(sock, retrieve_data, data_size, 0);
	                    SYS_LOG(comp, "Replica %d try to exed view id %d req id %d\n", comp->node_id, ltovs(index).view_id, ltovs(index).req_id);
	                }
	                *(comp->highest_committed_vs) = new_entry->req_canbe_exed;
	            }
	        }
    	}
    }
};
