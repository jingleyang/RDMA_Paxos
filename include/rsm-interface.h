typedef enum client_msg_code_t{
    C_SEND_WR = 0,
    C_SEND_RD = 1,
    //further structure is reserved for potential expansion
}client_msg_code;

typedef struct client_msg_header_t{
    client_msg_code type;
    size_t data_size;
}client_msg_header;
#define CLIENT_MSG_HEADER_SIZE (sizeof(client_msg_header))