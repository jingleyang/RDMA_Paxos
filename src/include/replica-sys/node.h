typedef struct node_t{

    node_id_t node_id;

    view cur_view;
    view_stamp highest_to_commit;
    view_stamp highest_committed;
    view_stamp highest_seen;

    //consensus component
    struct consensus_component_t* consensus_comp;

    // replica group
    struct sockaddr_in my_address;
    uint32_t group_size;

    // libevent part
    struct evconnlistener* listener;
    struct event_base* base;

    msg_handler msg_cb;
    struct event* signal_handler;

    //databse part
    char* db_name;
    db* db_ptr;
}node;