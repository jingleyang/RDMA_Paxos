/* InfiniBand device */
extern ib_device_t *ib_device;
#define IBDEV ib_device
#define SRV_DATA ((server_data_t*)dare_ib_device->udata)

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))

typedef struct consensus_component_t{ 
	con_role my_role;
    uint32_t node_id;

    uint32_t group_size;
    struct node_t* my_node;

    view* cur_view;
    view_stamp* highest_seen_vs; 
    view_stamp* highest_to_commit_vs;
    view_stamp* highest_committed_vs;

    log_t* log;
}consensus_component;

consensus_component* init_consensus_comp(uint32_t node_id,int group_size,view* cur_view,view_stamp* to_commit,view_stamp* committed,view_stamp* highest,user_cb u_cb,void* arg){
    
    consensus_component* comp = (consensus_component*)malloc(sizeof(consensus_component));
    memset(comp, 0, sizeof(consensus_component));

    if(NULL != comp){
        comp->db_ptr = db_ptr;
        comp->my_node = node;
        comp->node_id = node_id;
        comp->group_size = group_size;
        comp->cur_view = cur_view;
        if(comp->cur_view->leader_id == comp->node_id){
            comp->my_role = LEADER;
        }else{
            comp->my_role = SECONDARY;
        }
        comp->ucb = u_cb;
        comp->highest_seen_vs = highest;
        comp->highest_seen_vs->view_id = 1;
        comp->highest_seen_vs->req_id = 0;
        comp->highest_committed_vs = committed;
        comp->highest_committed_vs->view_id = 1; 
        comp->highest_committed_vs->req_id = 0; 
        comp->highest_to_commit_vs = to_commit;
        comp->highest_to_commit_vs->view_id = 1;
        comp->highest_to_commit_vs->req_id = 0;
        /* Set up log */
        comp->log = log_new();
    	if (NULL == data.log) {
        	error_return(1, log_fp, "Cannot allocate log\n");
    	}
    }
    return comp;
}

int consensus_submit_request(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs){
    if(LEADER == comp->my_role){
       return leader_handle_submit_req(comp, data_size, data, vs);
    }else{
        return 0;
    }
}

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs->view_id;
    next_vs.req_id = (comp->highest_seen_vs->req_id + 1);
    return next_vs;
};

static void view_stamp_inc(view_stamp* vs){
    vs->req_id++;
    return;
};

static int leader_handle_submit_req(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs){
    int ret = 1;
    view_stamp next = get_next_view_stamp(comp);
    if(NULL != vs){
        vs->view_id = next.view_id;
        vs->req_id = next.req_id;
    }

    /* record the data persistently */
    db_key_type record_no = vstol(&next);
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
    view_stamp_inc(comp->highest_seen_vs);
    append_log_entry(comp, REQ_RECORD_SIZE(record_data), record_data, &next);
    if(comp->group_size>1){
        //TODO RDMA write
        for (i = 0; i < comp->group_size; i++) {
            if (i == comp->node_id) {
                continue;
            }
            if (!CID_IS_SERVER_ON(SRV_DATA->config.cid, i)) {
                /* Server is off */
                continue;
            }
            ep = (ib_ep_t*)SRV_DATA->config.servers[i].ep;
            
            rem_mem_t rm;
            rm.raddr = ep->rc_ep.rmt_mr[LOG_QP].raddr + offset;
            rm.rkey = ep->rc_ep.rmt_mr[LOG_QP].rkey;
            posted_sends[i] = 1;
            /* server_id, qp_id, buf, len, mr, opcode, signaled, rm, posted_sends */ 
            rc = post_send(i, LOG_QP, sizeof(uint64_t), IBDEV->lcl_mr[LOG_QP], IBV_WR_RDMA_WRITE, SIGNALED, rm, posted_sends);
        }

recheck:
        for (int i = 0; i < MAX_SERVER_COUNT; i++)
        {
            if (/* TODO */)
            {
                record_data->bit_map = (record_data->bit_map | (1<<(i + 1));
                store_record(comp->db_ptr, sizeof(record_no), &record_no, REQ_RECORD_SIZE(record_data), record_data);
            }
        }
        if (reached_quorum(record_data,comp->group_size))
        {
            return 0;
        }else{
        goto recheck;
        }
    }else{
            return 0;
    }
handle_submit_req_exit: 
    if(record_data != NULL){
        free(record_data);
    }
    
    return ret;
}

static int reached_quorum(request_record* record,int group_size){
    // this may be compatibility issue 
    if(__builtin_popcountl(record->bit_map)>=((group_size/2)+1)){
        return 1;
    }else{
        return 0;
    }
}

static void handle_accept_req(consensus_component* comp)
{
    while(1){
        log_entry_t* new_entry = log_add_new_entry(comp->log);
        if(new_entry->msg_vs.view_id< comp->cur_view->view_id){
            // TODO
        }
        // if we this message is not from the current leader
        if(new_entry->msg_vs.view_id == comp->cur_view->view_id && new_entry->node_id!=comp->cur_view->leader_id){
            // TODO
        }
        // if we have committed the operation, then safely ignore it
        if(view_stamp_comp(&new_entry->msg_vs,comp->highest_committed_vs)<=0){
            // TODO
        }else{
            // update highest seen request
            if(view_stamp_comp(&new_entry->msg_vs,comp->highest_seen_vs)>0){
                *(comp->highest_seen_vs) = new_entry->msg_vs;
            }
            if(view_stamp_comp(&new_entry->req_canbe_exed,comp->highest_to_commit_vs)>0){
                *(comp->highest_to_commit_vs) = new_entry->req_canbe_exed;
            }
            db_key_type record_no = vstol(&new_entry->msg_vs);
            request_record* origin_data = (request_record*)msg->data;
            request_record* record_data = (request_record*)malloc(REQ_RECORD_SIZE(origin_data));

            gettimeofday(&record_data->created_time,NULL);
            record_data->data_size = origin_data->data_size;
            memcpy(record_data->data,origin_data->data,origin_data->data_size);

            // record the data persistently 
            store_record(comp->db_ptr,sizeof(record_no),&record_no,REQ_RECORD_SIZE(record_data),record_data)

            //TODO: RDMA reply to the leader
        }

handle_accept_req_exit:
        try_to_execute(comp);
        return;
};
    }
}

static void leader_try_to_execute(consensus_component* comp){
    db_key_type start;
    db_key_type end = vstol(comp->highest_seen_vs);;

    size_t data_size;
    // TODO: address the boundary
        
    start = vstol(comp->highest_to_commit_vs)+1;

    int exec_flag = (!view_stamp_comp(comp->highest_committed_vs,comp->highest_to_commit_vs));
    request_record* record_data = NULL;
    for(db_key_type index=start;index<=end;index++){
        retrieve_record(comp->db_ptr,sizeof(index),&index,&data_size,(void**)&record_data);
        if(reached_quorum(record_data,comp->group_size)){
            view_stamp temp = ltovs(index);
            
            view_stamp_inc(comp->highest_to_commit_vs);

            if(exec_flag)
            {
                view_stamp vs = ltovs(index);
                //deliver_msg_data(comp,&vs);
                //TODO: process_req
                view_stamp_inc(comp->highest_committed_vs);
            }
        }else{
            return;
        }
    }
}

static void try_to_execute(consensus_component* comp){

    if(comp->cur_view->view_id==0){

        goto try_to_execute_exit;
    }
    if(LEADER == comp->my_role){
        leader_try_to_execute(comp);
    }
    db_key_type start = vstol(comp->highest_committed_vs)+1;
    db_key_type end;
    size_t data_size;
        
    //TODO: address the boundary

    end = vstol(comp->highest_to_commit_vs);

    request_record* record_data = NULL;
    // we can only execute thins in sequence
    int exec_flag = 1;
    view_stamp missing_vs;
    for(db_key_type index = start;index<=end;index++){
        missing_vs = ltovs(index);
        if(0!=retrieve_record(comp->db_ptr, sizeof(index), &index, &data_size, (void**)&record_data))
        {
            exec_flag = 0;
            //TODO: send_missing_req(comp,&missing_vs);
        }
        if(exec_flag){
            //deliver_msg_data(comp,&missing_vs);
            //TODO:
            view_stamp_inc(comp->highest_committed_vs);
        }
        record_data = NULL;
    }
try_to_execute_exit:
    return;
};