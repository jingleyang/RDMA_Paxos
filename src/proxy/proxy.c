static hk_t gen_key(nid_t node_id,nc_t node_count,sec_t time){
    hk_t key = time;
    key |= ((hk_t)node_id<<52);
    key |= ((hk_t)node_count<<36);
    return key;
}

static void proxy_do_action(int fd,short what,void* arg){
    proxy_node* proxy = arg;
    real_do_action(proxy);
}

static void real_do_action(proxy_node* proxy){
    request_record* data = NULL;
    size_t data_size=0;
    db_key_type cur_higest;
    pthread_mutex_lock(&proxy->lock);
    cur_higest = proxy->highest_rec;
    pthread_mutex_unlock(&proxy->lock);
    while(proxy->cur_rec<=cur_higest){
        data = NULL;
        data_size = 0;
        retrieve_record(proxy->db_ptr,sizeof(db_key_type),&proxy->cur_rec,&data_size,(void**)&data);
        if(NULL != data){
            do_action_to_server(data->data_size,data->data,proxy);
            proxy->cur_rec++;
        }
    }
}

static void do_action_to_server(size_t data_size,void* data,void* arg)
{
    proxy_node* proxy = arg;

    proxy_msg_header* header = data;
    switch(header->action){
        case P_CONNECT:
            do_action_connect(data_size,data,arg);
            break;
        case P_SEND:
            do_action_send(data_size,data,arg);
            break;
        default:
            break; 
    return;
}

// when we have seen a connect method;
static void do_action_connect(size_t data_size,void* data,void* arg)
{
    
    proxy_node* proxy = arg;
    proxy_msg_header* header = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&header->connection_id,proxy->hash_map,ret);
    if(NULL==ret){
        ret = malloc(sizeof(socket_pair));
        memset(ret,0,sizeof(socket_pair));
        ret->key = header->connection_id;
        ret->counter = 0;
        ret->proxy = proxy;
        MY_HASH_SET(ret,proxy->hash_map);
    }
    
    if(ret->p_s==NULL){
        ret->p_s = bufferevent_socket_new(proxy->base,-1,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(ret->p_s,server_side_on_read,NULL,server_side_on_err,ret);
        bufferevent_enable(ret->p_s,EV_READ|EV_PERSIST|EV_WRITE);
        bufferevent_socket_connect(ret->p_s,(struct sockaddr*)&proxy->sys_addr.s_addr,proxy->sys_addr.s_sock_len);
    }
    
    return;
}

static void do_action_send(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;
    proxy_send_msg* msg = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&msg->header.connection_id,proxy->hash_map,ret);
    // this is error, TO-DO:error handler
    if(NULL==ret){
    }else{
        if(NULL==ret->p_s){
        }else{
            bufferevent_write(ret->p_s,msg->data,msg->data_size);
        }
    }
    return;
}

static void update_state(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;
    db_key_type* rec_no = data;
    proxy->highest_rec = (proxy->highest_rec<*rec_no)?*rec_no:proxy->highest_rec;
    wake_up(proxy);
    return;
}

static void wake_up(proxy_node* proxy){
    pthread_kill(proxy->p_self,SIGUSR2);
}

void connect_consensus(proxy_node* proxy){
    
    evutil_socket_t fd;
    fd = socket(AF_INET, SOCK_STREAM, 0);
    proxy->con_conn = bufferevent_socket_new(proxy->base,fd,BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(proxy->con_conn,NULL,NULL,consensus_on_event,proxy);
    bufferevent_enable(proxy->con_conn,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(proxy->con_conn,(struct sockaddr*)&proxy->sys_addr.c_addr,proxy->sys_addr.c_sock_len);
  
    return;
}

static void server_side_on_read(struct bufferevent* bev,void* arg){
    socket_pair* pair = arg;

    struct evbuffer* input = bufferevent_get_input(bev);
    struct evbuffer* output = NULL;
    size_t len = 0;
    int cur_len = 0;
    void* msg = NULL;
    len = evbuffer_get_length(input);

    if(len>0){
        cur_len = len;
        if(pair->p_c != NULL){
            output = bufferevent_get_output(pair->p_c);
        }
        if(output != NULL){
            evbuffer_add_buffer(output,input);
        }else{
            evbuffer_drain(input,len);
        }
    }
    return;
}

static void client_process_data(socket_pair* pair,struct bufferevent* bev,size_t data_size){
    proxy_node* proxy = pair->proxy;
    void* msg_buf = (char*)malloc(CLIENT_MSG_HEADER_SIZE+data_size);
    req_sub_msg* con_msg=NULL;
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb,msg_buf,CLIENT_MSG_HEADER_SIZE+data_size);
    client_msg_header* msg_header = msg_buf;
    switch(msg_header->type){
        case C_SEND_WR:
            break;
        default:
    }
    return;
}

static void client_side_on_read(struct bufferevent* bev, void* arg)
{
    socket_pair* pair = arg;
    proxy_node* proxy = pair->proxy;

    client_msg_header* header = NULL;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    while(len>=CLIENT_MSG_HEADER_SIZE){
        header = (client_msg_header*)malloc(CLIENT_MSG_HEADER_SIZE);
        if(NULL==header){return;}
        evbuffer_copyout(input,header,CLIENT_MSG_HEADER_SIZE);
        size_t data_size = header->data_size;
        if(len>=(CLIENT_MSG_HEADER_SIZE+data_size)){
           client_process_data(pair,bev,data_size); 
        }else{
            break;
        }
        free(header);
        header=NULL;
        len = evbuffer_get_length(input);
    }
    if(NULL!=header){free(header);}
    return;
};

static req_sub_msg* build_req_sub_msg(hk_t s_key,counter_t counter,int type,size_t data_size,void* data){
    req_sub_msg* msg = NULL;
    switch(type){
        case P_CONNECT:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+PROXY_CONNECT_MSG_SIZE);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = PROXY_CONNECT_MSG_SIZE;
            proxy_connect_msg* co_msg = (void*)msg->data;
            co_msg->header.action = P_CONNECT;
            co_msg->header.connection_id = s_key;
            co_msg->header.counter = counter;
            gettimeofday(&co_msg->header.created_time,NULL);
            break;
        case P_SEND:
            msg = (req_sub_msg*)malloc(SYS_MSG_HEADER_SIZE+sizeof(proxy_send_msg)+data_size);
            msg->header.type = REQUEST_SUBMIT;
            msg->header.data_size = sizeof(proxy_send_msg)+data_size;
            proxy_send_msg* send_msg = (void*)msg->data;
            send_msg->header.action = P_SEND;
            send_msg->header.connection_id = s_key;
            send_msg->header.counter = counter;
            send_msg->data_size = data_size;
            memcpy(send_msg->data,data,data_size);
            gettimeofday(&send_msg->header.created_time,NULL);
            break;
    }
    return msg;
}

static void proxy_on_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg){
    req_sub_msg* req_msg = NULL;
    proxy_node* proxy = arg;
    if(proxy->con_conn==NULL){
        //We Have Lost The Connection To Consensus Component Now
        close(fd);
    }else{
        socket_pair* new_conn = malloc(sizeof(socket_pair));
        memset(new_conn,0,sizeof(socket_pair));
        struct timeval cur;
        gettimeofday(&cur,NULL);
        new_conn->key = gen_key(proxy->node_id,proxy->pair_count++,cur.tv_sec);
        new_conn->p_c = bufferevent_socket_new(proxy->base,fd,BEV_OPT_CLOSE_ON_FREE);
        new_conn->counter = 0;
        new_conn->proxy = proxy;
        bufferevent_setcb(new_conn->p_c, client_side_on_read, NULL, NULL, new_conn);
        bufferevent_enable(new_conn->p_c, EV_READ|EV_PERSIST|EV_WRITE);

        MY_HASH_SET(new_conn,proxy->hash_map);
        struct timeval recv_time;
        gettimeofday(&recv_time,NULL);
        req_msg = build_req_sub_msg(new_conn->key,new_conn->counter++,P_CONNECT,0,NULL); 
        ((proxy_connect_msg*)req_msg->data)->header.received_time = recv_time;
        bufferevent_write(proxy->con_conn,req_msg,REQ_SUB_SIZE(req_msg));
    }
    if(req_msg!=NULL){
        free(req_msg);
    }
    return;
}

static void proxy_singnal_handler(evutil_socket_t fid,short what,void* arg){
    proxy_node* proxy = arg;
    PROXY_ENTER(proxy);
    int sig;
    sig = event_get_signal((proxy->sig_handler));
    /* GET SIGUSR2,Now Check Pending Requests */
    real_do_action(proxy);
    PROXY_LEAVE(proxy);
    return;
}

proxy_node* proxy_init(node_id_t node_id, const char* start_mode)
{
    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    memset(proxy,0,sizeof(proxy_node));

    proxy->hash_map=NULL;
    struct event_base* base = event_base_new();
    proxy->base = base;

    proxy->db_ptr = initialize_db(proxy->db_name, 0);
    proxy->hash_map=NULL;
    //struct evconnlistener *evconnlistener_new_bind(struct event_base *base, evconnlistener_cb cb, void *ptr, unsigned flags, int backlog, const struct sockaddr *sa, int socklen);
    proxy->listener = evconnlistener_new_bind(proxy->base,proxy_on_accept,
                (void*)proxy,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,
                -1,(struct sockaddr*)&proxy->sys_addr.p_addr,
                sizeof(proxy->sys_addr.p_addr));

    proxy->con_node = system_initialize(node_id,start_mode,update_state,proxy->db_ptr,proxy);

    proxy->sig_handler = evsignal_new(proxy->base,SIGUSR2,proxy_singnal_handler,proxy);
    evsignal_add(proxy->sig_handler,NULL);

    return proxy;

}

void proxy_run(proxy_node* proxy){
    proxy->re_con = evtimer_new(proxy->base,
            reconnect_on_timeout,proxy);
    connect_consensus(proxy);
    if(proxy->do_action!=NULL){
        evtimer_add(proxy->do_action,&proxy->action_period);
    }
    event_base_dispatch(proxy->base);
    proxy_exit(proxy);
    return;
}

void proxy_exit(proxy_node* proxy){
    return;
}
