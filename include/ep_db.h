struct ep_t {
    struct rb_node node;
    ud_ep_t ud_ep;
    uint64_t last_req_id;   /* this is the ID of the last request from 
                            this endpoint that I answer; ignore requests 
                            with lower IDs */
};