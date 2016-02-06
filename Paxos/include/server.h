struct server_t {
    void *ep;               // endpoint data (network related)
};
typedef struct server_t server_t;

struct server_input_t {
    FILE* log;
    char* name;
    uint8_t srv_type;
    uint8_t group_size;
    uint8_t server_idx;
};
typedef struct server_input_t server_input_t;

struct server_data_t {
    
    server_config_t config; // configuration 

    struct rb_root endpoints;   // RB-tree with remote endpoints
};
typedef struct server_data_t server_data_t;