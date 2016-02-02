uint64_t vstol(view_stamp* vs){
    uint64_t result = ((uint64_t)vs->req_id)&0xFFFFFFFFl;
    uint64_t temp = (uint64_t)vs->view_id&0xFFFFFFFFl;
    result += temp<<32;
    return result;
};