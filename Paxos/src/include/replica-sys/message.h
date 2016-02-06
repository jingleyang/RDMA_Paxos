typedef enum sys_msg_code_t{
    REQUEST_SUBMIT = 2,
}sys_msg_code;

typedef struct request_submit_msg_t{
    sys_msg_header header; 
    char data[0];
}__attribute__((packed))req_sub_msg;
#define REQ_SUB_SIZE(M) (((M)->header.data_size)+sizeof(sys_msg_header))