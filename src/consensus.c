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

    //log_t  *log;       // local log (remotely accessible)
}consensus_component;

consensus_component* init_consensus_comp(struct node_t* node,uint32_t node_id,int group_size,view* cur_view,view_stamp* to_commit,view_stamp* committed,view_stamp* highest,user_cb u_cb,void* arg){
    
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

static int leader_handle_submit_req(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs){
    int ret = 1;
    view_stamp next = get_next_view_stamp(comp);
    if(NULL != vs){
        vs->view_id = next.view_id;
        vs->req_id = next.req_id;
    }
    db_key_type record_no = vstol(&next);
    request_record* record_data = (request_record*)malloc(data_size + sizeof(request_record));
    gettimeofday(&record_data->created_time,NULL);
    record_data->bit_map = (1<<comp->node_id);
    record_data->data_size = data_size;
    memcpy(record_data->data,data,data_size);
    if(store_record(comp->db_ptr,sizeof(record_no),&record_no,REQ_RECORD_SIZE(record_data),record_data)){
        goto handle_submit_req_exit;
    }
    ret = 0;
    view_stamp_inc(comp->highest_seen_vs);
    append_log_entry(comp, REQ_RECORD_SIZE(record_data), record_data, &next);
    if(comp->group_size>1){
    	//RDMA_write;
    }else{
        try_to_execute(comp);
    }
handle_submit_req_exit: 
    if(record_data != NULL){
        free(record_data);
    }
    
    return ret;
}

int consensus_submit_request(struct consensus_component_t* comp, size_t data_size, void* data, view_stamp* vs){
    if(LEADER == comp->my_role){
       return leader_handle_submit_req(comp, data_size, data, vs);
    }else{
        //TODO replica loop
        return 0;
    }
}

static void deliver_msg_data(consensus_component* comp,view_stamp* vs){

    // in order to accelerate deliver process of the program
    // we may just give the record number instead of the real data 
    // to the proxy, and then the proxy will take the overhead of database operation
    
    db_key_type vstokey = vstol(vs);
    if(comp->deliver_mode==1)
    {
        request_record* data = NULL;
        size_t data_size=0;
        retrieve_record(comp->db_ptr,sizeof(db_key_type),&vstokey,&data_size,(void**)&data);
        if(NULL!=data){
            if(comp->ucb!=NULL){
                comp->ucb(data->data_size,data->data,comp->up_para);
            }
        }
    }else{
        if(comp->ucb!=NULL){
            comp->ucb(sizeof(db_key_type),&vstokey,comp->up_para);
        }
    }
    return;
}
