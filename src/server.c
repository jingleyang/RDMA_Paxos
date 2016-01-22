#define IS_LEADER \
    ( !IS_NONE && (SID_GET_IDX(data.cached_sid) == data.config.idx) && \
      (SID_GET_L(data.cached_sid)) )

server_data_t data;

int server_init()
{     
    init_server_data();

    init_network();

    poll();

}

int init_server_data()
{
    //data.config.idx input

    /* Cannot have more than MAX_SERVER_COUNT servers */
    data.config.len = MAX_SERVER_COUNT;

    //data.config.cid.size input
    data.config.servers = (server_t*)malloc(data.config.len * sizeof(server_t));
    if (NULL == data.config.servers) {
        //Cannot allocate configuration array
    }
    memset(data.config.servers, 0, data.config.len * sizeof(server_t));

    data.log = log_new();

    //struct evconnlistener *evconnlistener_new_bind(struct event_base *base, evconnlistener_cb cb, void *ptr, unsigned flags, int backlog, const struct sockaddr *sa, int socklen);
    data.listener = evconnlistener_new_bind(data.base, server_on_accept, NULL, LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1, (struct sockaddr*)&data.sys_addr.m_addr, sizeof(data.sys_addr.m_addr));

    return 0;
}

/* ================================================================== */

void init_network()
{
    init_ib_device();

    init_ib_srv_data(&data);

    init_ib_rc();

    /* Start IB UD */
    rc = start_ib_ud();
    if (0 != rc) {
        //Cannot start IB UD
    }

    state |= INIT;
    
    /* Start poll event */   
    ev_idle_start(EV_A_ &poll_event);
    
    if (SRV_TYPE_JOIN == data.input->srv_type) { //TODO: read from the config file? Or input?
        /* Server joining the cluster */
        join_cluster();
    }
    else { 
        /* I'm the only one; I am the leader */
        //Starting with size=1
        SID_SET_TERM(data.cached_sid, 1);
        SID_SET_L(data.cached_sid);
        SID_SET_IDX(data.cached_sid, 0);
    }

}

/**
 * Send join requests to the cluster
 * Note: the timer is stopped when receiving a JOIN reply (poll_ud)
 */
static void join_cluster()
{
    int rc;
    
    rc = ib_join_cluster();
    if (0 != rc) {
        //Cannot join cluster
    }
    
    /* Retransmit after retransmission period */
    //TODO
    
    return;  
}

/**
 * Exchange RC info
 * Note: the timer is stopped when establishing connections with 
 * at least half of the servers  
 */
static void exchange_rc_info()
{
    int rc;
    
    rc = ib_exchange_rc_info();
    if (0 != rc) {
        //Exchanging RC info failed
    }
    
    /* Retransmit after retransmission period */
    //TODO
    
    return;
}

/* ================================================================== */

static void poll()
{
    polling();  
}

static void polling()
{
    /* Poll UD connection for incoming messages */
    poll_ud();
}

/**
 * Poll for UD messages
 */
static void poll_ud()
{
    uint8_t type = ib_poll_ud_queue();
}

/* ================================================================== */

static void server_on_accept(struct evconnlistener* listener, evutil_socket_t fd, struct sockaddr *address, int socklen, void *arg){
    socket_pair_t* new_conn = malloc(sizeof(socket_pair_t));
    memset(new_conn, 0, sizeof(socket_pair_t));
    struct timeval cur;
    gettimeofday(&cur, NULL);
    new_conn->key = gen_key(data.config.idx, data.pair_count++, cur.tv_sec); //TODO: How to generate e key based on the node's idx, pair_count and current time
    new_conn->p_c = bufferevent_socket_new(data.base, fd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(new_conn->p_c, client_side_on_read, NULL, NULL, new_conn);
    bufferevent_enable(new_conn->p_c, EV_READ|EV_PERSIST|EV_WRITE);

    int enable = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable));

    ep_insert(data.endpoints, new_conn->key);

    //TODO: try to reach a consensus on the request of type P_CONNECT and then send it to the real server
    client_req_t* conn_server;
    conn_server->hdr.type = P_CONNECT;
    conn_server->hdr.req_id = 0;
    con_server->cmd.len = 0;
    handle_one_csm_write_request(conn_server, new_conn->key);

    return;
}

static void real_do_action(client_req_t* req_canbe_exed){
    for (int i = 0; i < counter; i++)
    {
        //TODO: Do we need to retrieve the record from berkeley db?
        do_action_to_server(req_canbe_exed);
        req_canbe_exed--;
    }
}

static void do_action_to_server(client_req_t* req_canbe_exed)
{
    switch(req_canbe_exed->hdr.type){
        case P_CONNECT:
            do_action_connect(req_canbe_exed);
            break;
        case P_SEND:
            do_action_send(req_canbe_exed);
            break;
        default:
            break;
    }
    return;
}

// when we have seen a connect method;
static void do_action_connect(client_req_t* req_canbe_exed){
    
    socket_pair* ret = NULL;

    if(NULL == ret){
        ret = malloc(sizeof(socket_pair));
        memset(ret,0,sizeof(socket_pair));
        ret->key = header->connection_id;
        ret->counter = 0;
        MY_HASH_SET(ret,proxy->hash_map);
    }
    
    ret->p_s = bufferevent_socket_new(proxy->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(ret->p_s, server_side_on_read, NULL, NULL, ret);
    bufferevent_socket_connect(ret->p_s, (struct sockaddr*)&data.sys_addr.s_addr, proxy->sys_addr.s_sock_len);
    
    return;
}

static void do_action_send(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;

    proxy_send_msg* msg = data;
    socket_pair* ret = NULL;

    if(NULL == ret){
        goto do_action_send_exit;
    }else{
        if(NULL == ret->p_s){
            goto do_action_send_exit;
        }else{
            bufferevent_write(ret->p_s, msg->data, msg->data_size);
             /* Needed for answering read requests */
            data.last_cmt_write_csm_idx = entry->idx;
        }
    }

    /* When new entries are applied, the leader verifies if there are pending read requests */
    ep_dp_reply_read_req(&data.endpoints, data.last_cmt_write_csm_idx);

do_action_send_exit:

    return;
}


static void client_side_on_read(struct bufferevent* bev, void* arg)
{
    socket_pair_t* pair = arg;
    client_req_t* req_header = NULL;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    while(len >= sizeof(ud_hdr_t)){
            req_header = (client_req_t*)malloc(sizeof(ud_hdr_t) + offsetof(sm_cmd_t, cmd);
            if(NULL == req_header){return;}
            evbuffer_copyout(input, req_header, sizeof(ud_hdr_t) + offsetof(sm_cmd_t, cmd));
            size_t data_size = req_header->cmd.len;
            if(len >= (sizeof(ud_hdr_t) + offsetof(sm_cmd_t, cmd) + data_size)){
                process_data(bev, data_size, pair->key); 
            }else{
                break;
            }
            free(req_header);
            req_header = NULL;
            len = evbuffer_get_length(input);
    }
}

void process_data(struct bufferevent* bev, size_t data_size, uint64_t key){
    void* msg_buf = (char*)malloc(sizeof(ud_hdr_t) + offsetof(sm_cmd_t, cmd) + data_size);
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb, msg_buf, sizeof(ud_hdr_t) + offsetof(sm_cmd_t, cmd) + data_size);
    client_req_t* client_req = msg_buf;
    switch(client_req->hdr.type){
        case CSM_WRITE:
            handle_one_csm_write_request(client_req, key);
        case CSM_READ:
            handle_one_csm_read_request(client_req, key);
        }
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

static void handle_one_csm_read_request(client_req_t *request, uint64_t key)
{
    /* Find the ep that send this request */
    ep_t *ep = ep_search(data.endpoints, key);

    /* Check the status of the last write request  */
    if (data.last_cmt_write_csm_idx < data.last_write_csm_idx) {
        /* There are not-committed write requests; so wait */
        ep->wait_for_idx = data.last_write_csm_idx;
        return;
    }

    /* Create reply */
    client_rep_t *reply;
    memset(reply, 0, sizeof(client_rep_t));
    reply->hdr.id = request->hdr.id;
    reply->hdr.type = CSM_REPLY;
    
    do_action_to_server(request);//TODO
    
    /* Send reply */
    //TODO

}

static void handle_one_csm_write_request(client_req_t *request, uint64_t key)
{   
    /* Find the endpoint that send this request */
    ep_t *ep = ep_search(data.endpoints, key);

    if (ep->last_req_id >= request->hdr.id) {
        /* Already received this request */
        //TODO bufferevent
        return;
    }
    ep->last_req_id = request->hdr.id;

    client_req_t* req_canbe_exed = RSM_Op(request);

    real_do_action(req_canbe_exed);
    //TODO: build the reply and then send it
}

/* Leader side */
client_req_t* RSM_Op(client_req_t* request){
    /* update local data and build the entry */
    pthread_spinlock_lock(data.data_lock);
    data.last_write_csm_idx = log_append_entry(data.log, SID_GET_TERM(data.cached_sid), request->hdr.id, request->hdr.clt_id, &request->cmd);
    uint64_t tail_offset = data.log->tail;
    log_entry_t* new_entry = log_get_entry(data.log, &tail_offset);
    pthread_spinlock_unlock(data.data_lock);

    /* record the data persistently */
    uint64_t record_no = vstol(SID_GET_TERM(data.cached_sid), new_entry->idx);
    request_record_t* record_data = (request_record_t*)malloc(request->cmd.len + sizeof(request_record_t));
    gettimeofday(&record_data->created_time, NULL);
    record_data->bit_map = (1<<data.config.idx);
    record_data->data_size = request->cmd.len;
    memcpy(record_data->data, request->cmd.data, request->cmd.len);
    store_record(data.db_ptr, sizeof(record_no), &record_no, sizeof(request_record_t) + record_data->data_size, record_data);

    /* RDMA write this new entry to all the other replicas */
    uint32_t remote_offset = (uint32_t)(offsetof(log_t, entries) + tail_offset);
    uint32_t len = log_entry_len(new_entry);
    for (int i = 0; i < size; ++i)
    {
        RDMA_write(i, new_entry, len, remote_offset);
    }
    

recheck:
    for (int i = 0; i < MAX_SERVER_COUNT; i++)
    {
        if (new_entry->result[i] == 1)
        {
            record_data->bit_map = (record_data->bit_map | (1<<(i + 1));
            store_record(data.db_ptr, sizeof(record_no), &record_no, sizeof(request_record_t) + record_data->data_size, record_data);
            new_entry->result[i] == 0;
        }
    }
    if (__builtin_popcountl(record_data->bit_map) >= ((get_group_size(data.config)/2) + 1))
    {
        new_entry->result[data.config.idx - 1] = 1;
        /* we can only execute thins in sequence */ 
        client_req_t* req_canbe_exed = NULL;
        uint64_t counter = 0;
        pthread_spinlock_lock(data.exed_lock);
execute:        
        log_entry_t* entry_canbe_exed = (log_entry_t*)(data.log->entries + data.log->write);
        if (entry_canbe_exed->result[data.config.idx - 1] == 1 && (entry_canbe_exed->idx <= new_entry->idx))
        {
            req_canbe_exed->cmd.cmd.size = entry_canbe_exed->data.cmd.size;
            memcpy(req_canbe_exed->cmd.cmd.data, entry_canbe_exed->data.cmd.cmd, entry_canbe_exed->data.cmd.size);
            data.log->write += log_get_entry(entry_canbe_exed);
            req_canbe_exed++;
            goto execute;
        }
        pthread_spinlock_unlock(data.exed_lock);
        return req_canbe_exed;
    }else{
        goto recheck;
    }
}

/* replica side */
while (TRUE)
{
    log_entry_t* new_entry = log_add_new_entry(data.log);
    if (new_entry->idx != 0)
    {
        /* record the data persistently */
        uint64_t record_no = vstol(new_entry->term, new_entry->idx);
        request_record_t* record_data = (request_record_t*)malloc(new_entry->data.cmd.len + sizeof(request_record_t));
        record_data->data_size = new_entry->data.cmd.len;
        memcpy(record_data->data, new_entry->data.cmd.cmd, new_entry->data.cmd.len);
        store_record(data.db_ptr, sizeof(record_no), record_no, new_entry->data.cmd.len + sizeof(request_record_t), record_data);

        /* RDMA write result to the leader*/
        uint32_t remote_offset = (uint32_t)(offsetof(log_t, entries) + data.log->end);
        uint8_t leader = SID_GET_IDX(data.cached_sid);
        int* res;
        *res = 1;
        RDMA_write(res, sizeof(int), remote_offset, leader);

        data.log->tail = log->end;
        data.log->end += log_entry_len(new_entry);

        /* compare committed*/
        client_req_t* req_canbe_exed = NULL;
        while (new_entry->committed > data.log->write)
        {
            log_entry_t* entry_canbe_exed = (log_entry_t*)(data.log->entries + data.log->write);
            req_canbe_exed->cmd.cmd.len = entry_canbe_exed->data.cmd.len;
            memcpy(req_canbe_exed->cmd.cmd.data, entry_canbe_exed->data.cmd.cmd, entry_canbe_exed->data.cmd.len);
            data.log->write += log_entry_len(entry_canbe_exed);
            req_canbe_exed++;
        }
        //TODO: send req_canbe_exed to server application (maybe wake up another thread to do this)
    }
}

int is_leader()
{
    return IS_LEADER;
}
