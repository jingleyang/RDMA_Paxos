static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){

    node* my_node = arg;
    struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
    bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
    return;
};

node* system_initialize(node_id_t node_id,const char* start_mode,void(*user_cb)(size_t data_size,void* data,void* arg),void* db_ptr,void* arg){
    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node, 0, sizeof(node));

    my_node->node_id = node_id;
    my_node->db_ptr = db_ptr;

    //seed, currently the node is the leader
    if(){
        my_node->cur_view.view_id = 1;
        my_node->cur_view.leader_id = my_node->node_id;
        my_node->cur_view.req_id = 0;
    }

    initialize_node(my_node,user_cb,db_ptr,arg)

    my_node->listener = evconnlistener_new_bind(base,replica_on_accept,(void*)my_node,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,(struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));
    //TODO replica never receives a message
	return my_node;
}


static void handle_msg(node* my_node,struct bufferevent* bev,size_t data_size){

    void* msg_buf = (char*)malloc(SYS_MSG_HEADER_SIZE+data_size);
    if(NULL==msg_buf){
        goto handle_msg_exit;
    }
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb,msg_buf,SYS_MSG_HEADER_SIZE+data_size);
    sys_msg_header* msg_header = msg_buf;
    switch(msg_header->type){
        case REQUEST_SUBMIT:
                handle_request_submit(my_node,(req_sub_msg*)msg_buf,bev);
            break;
        default:
            goto handle_msg_exit;
    }

    if(NULL!=msg_buf){free(msg_buf);}
    return;
}

//general data handler by the user, test if there is enough data to read
static void replica_on_read(struct bufferevent* bev,void* arg){
    node* my_node = arg;
    sys_msg_header* buf = NULL;;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    int counter = 0;
    while(len>=SYS_MSG_HEADER_SIZE){
        buf = (sys_msg_header*)malloc(SYS_MSG_HEADER_SIZE);
        if(NULL==buf){
          return;
        }
        evbuffer_copyout(input,buf,SYS_MSG_HEADER_SIZE);
        int data_size = buf->data_size;
        if(len>=(SYS_MSG_HEADER_SIZE+data_size)){
           my_node->msg_cb(my_node,bev,data_size); 
           counter++;
        }else{
          break;
        }
        free(buf);
        buf=NULL;
        len = evbuffer_get_length(input);
    }
    if(my_node->stat_log){
    }
    if(NULL!=buf){free(buf);}
    return;
}

int initialize_node(node* my_node,void (*user_cb)(size_t data_size,void* data,void* arg),void* db_ptr,void* arg){
    my_node->msg_cb = handle_msg;
    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,
            my_node->node_id,my_node->db_name,db_ptr,my_node->group_size,
            &my_node->cur_view,&my_node->highest_to_commit,&my_node->highest_committed,
            &my_node->highest_seen,user_cb,arg);
}

static void handle_request_submit(node* my_node,req_sub_msg* msg){
    if(NULL!=my_node->consensus_comp){
        view_stamp return_vs;
        consensus_submit_request(my_node->consensus_comp,msg->header.data_size,(void*)msg+SYS_MSG_HEADER_SIZE,&return_vs);
    }
    return;
}