struct server_t {
    void *ep;               // endpoint data (network related)
};

struct server_data_t {
    
    server_config_t config; // configuration 
    
    log_t  *log;       // local log (remotely accessible)

    db* db_ptr;
};