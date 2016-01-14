server_data_t data;

int server_init()
{     
    init_server_data();

}

void init_network(){
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
int init_server_data(){
    //data.config.idx input

    /* Cannot have more than MAX_SERVER_COUNT servers */
    data.config.len = MAX_SERVER_COUNT;

    //data.config.cid.size input

    data.log = log_new();

    return 0;
}