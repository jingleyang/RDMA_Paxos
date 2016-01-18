server_data_t data;

int server_init()
{     
    init_server_data();

}

void init_network()
{
    init_ib_device();

    init_ib_srv_data(&data);

    init_ib_rc();
}

/**
  * Suppose the client-server communication is based on Libevent 

client_msg_header* header = NULL;
struct evbuffer* input = bufferevent_get_input(bev);
size_t len = 0;
len = evbuffer_get_length(input);
while(len >= sizeof(client_msg_header)){
        header = (client_msg_header*)malloc(sizeof(client_msg_header));
        if(NULL==header){return;}
        evbuffer_copyout(input, header, sizeof(client_msg_header));
        size_t data_size = header->data_size;
        if(len >= (sizeof(client_msg_header) + data_size)){
        	process_data(bev, data_size); 
        }else{
            break;
        }
        free(header);
        header = NULL;
        len = evbuffer_get_length(input);
}

void process_data(struct bufferevent* bev, size_t data_size){
 	void* msg_buf = (char*)malloc(sizeof(client_msg_header) + data_size);
    req_msg* con_msg = NULL;
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb, msg_buf, sizeof(client_msg_header) + data_size);
    client_msg_header* msg_header = msg_buf;
    pthread_t Thread;
	switch(header->type){
        case PUT:
        con_msg->data_size = data_size;
        memcpy(con_msg->data, ((client_msg*)msg_header)->data, data_size);
        pthread_create(Thread, NULL, con_msg);
    }
    return;
}
  */
int init_server_data()
{
    //data.config.idx input

    /* Cannot have more than MAX_SERVER_COUNT servers */
    data.config.len = MAX_SERVER_COUNT;

    //data.config.cid.size input

    data.log = log_new();

    return 0;
}

/* Leader side */
req_msg* RSM_Op(req_msg* msg){
    /* update local data and build the entry */
    pthread_spinlock_lock(data.data_lock);
    uint64_t offset = log->tail;
    log_entry_t *last_entry = log_get_entry(data.log, &offset);
    uint64_t idx = last_entry ? last_entry->idx + 1 : 1;
    log_entry_t* new_entry = log_add_new_entry(data.log);
    new_entry->data_size = msg->data_size;
    new_entry->idx = idx;
    new_entry->committed = data.log->write;
    data.log->tail = log->end;
    data.log->end += new_entry->data_size + sizeof(log_entry_t);
    pthread_spinlock_unlock(data.data_lock);
    new_entry->term = SID_GET_TERM(data.cached_sid);
    memcpy(new_entry->data, msg->data, msg->data_size);

    /* record the data persistently */
    uint64_t record_no; //TODO: record_no should be a combination of term and idx
    request_record_t* record_data = (request_record_t*)malloc(msg->data_size + sizeof(request_record_t));
    gettimeofday(&record_data->created_time, NULL);
    record_data->bit_map = (1<<data.config.idx);
    record_data->data_size = msg->data_size;
    memcpy(record_data->data, msg->data, msg->data_size);
    store_record(data.db_ptr, sizeof(record_no), &record_no, sizeof(request_record_t) + record_data->data_size, record_data);

    /* RDMA write this new entry to all the other replicas */
    uint32_t remote_offset = (uint32_t)(offsetof(log_t, entries) + data.log->tail);
    uint32_t len = sizeof(log_entry_t) + new_entry->data_size;
    RDMA_write(new_entry, len, remote_offset, 0);

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
        req_msg* ret_val = NULL;
        uint64_t counter = 0;
        pthread_spinlock_lock(data.ret_lock);
execute:        
        log_entry_t* ret_entry = (log_entry_t*)(data.log->entries + data.log->write);
        if (ret_entry->result[data.config.idx - 1] == 1 && (ret_entry->idx <= new_entry->idx))
        {
            ret_val[counter]->data_size = ret_entry->data_size;
            memcpy(ret_val[counter]->data, ret_entry->data, ret_entry->data_size);
            data.log->write += ret_entry->data_size + sizeof(log_entry_t);
            counter++;
            goto execute;
        }
        pthread_spinlock_unlock(data.ret_lock);
        return ret_val;
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
        uint64_t record_no; //TODO: combination of new_entry->term and new_entry->idx
        request_record_t* record_data = (request_record_t*)malloc(new_entry->data_size + sizeof(request_record_t));
        record_data->data_size = new_entry->data_size;
        memcpy(record_data->data, new_entry->data, new_entry->data_size);
        store_record(data.db_ptr, sizeof(record_no), record_no, new_entry->data_size + sizeof(request_record_t), record_data);

        /* RDMA write result to the leader*/
        uint32_t remote_offset = (uint32_t)(offsetof(log_t, entries) + data.log->end);
        uint8_t leader = SID_GET_IDX(data.cached_sid);
        int* res;
        *res = 1;
        RDMA_write(res, sizeof(int), remote_offset, leader);

        data.log->tail = log->end;
        data.log->end += new_entry->data_size + sizeof(log_entry_t);

        /* compare committed*/
        req_msg* ret_val = NULL;
        uint64_t counter = 0;
        while (new_entry->committed > data.log->write)
        {
            log_entry_t* ret_entry = (log_entry_t*)(data.log->entries + data.log->write);
            ret_val[counter]->data_size = ret_entry->data_size;
            memcpy(ret_val[counter]->data, ret_entry->data, ret_entry->data_size);
            data.log->write += sizeof(log_entry_t) + ret_entry->data_size;
        }
        //TODO: send ret_val to server application
    }
}
