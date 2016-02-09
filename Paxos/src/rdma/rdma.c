FILE *log_fp;

/* server data */
rdma_data_t rdma_data;
/* ================================================================== */
/* libEV events */

/* An idle event that polls for different stuff... */
ev_idle poll_event;

/* ================================================================== */

int rdma_init(uint32_t node_it, int group_size, FILE* sys_log_file, log_t* log)
{
    /* Initialize data fields to zero */
    memset(&rdma_data, 0, sizeof(rdma_data_t));
    
    /* Set log file handler */
    log_fp = ;

    init_rdma_data();

    init_network_cb();

    /* Init the poll event */
    ev_idle_init(&poll_event, poll_cb);
    ev_set_priority(&poll_event, EV_MAXPRI);
}

static int init_rdma_data()
{
    /* Set up the configuration */
    rdma_data.config.idx = node_it;
    rdma_data.config.len = MAX_SERVER_COUNT;

    /* Cannot have more than MAX_SERVER_COUNT servers */
    rdma_data.config.len = MAX_SERVER_COUNT;

    for (i = 0; i < group_size; i++) {
        CID_SERVER_ADD(rdma_data.config.cid, i);
    }

    rdma_data.config.servers = (server_t*)malloc(rdma_data.config.len * sizeof(server_t));
    if (NULL == rdma_data.config.servers) {
        error_return(1, log_fp, "Cannot allocate configuration array\n");
    }
    memset(rdma_data.config.servers, 0, rdma_data.config.len * sizeof(server_t));

    rdma_data.log = log_new();

    rdma_data.endpoints = RB_ROOT;

    return 0;
}

static void free_server_data()
{
    ep_db_free(&data.endpoints);
    
    /* Free servers */
    if (NULL != rdma_data.config.servers) {
        free(rdma_data.config.servers);
        rdma_data.config.servers = NULL;
    }
}

/* ================================================================== */

static void init_network_cb()
{
    int rc;

    /* Init IB device */
    rc = init_ib_device(MAX_SERVER_COUNT);
    if (0 != rc) {
        error(log_fp, "Cannot init IB device\n");
    }

    /* Init some IB data for the server */
    rc = init_ib_srv_data(&rdma_data);
    if (0 != rc) {
        error(log_fp, "Cannot init IB SRV data\n");
    }

    /* Init IB RC */
    rc = init_ib_rc();
        if (0 != rc) {
        error(log_fp, "Cannot init IB RC\n");
    }

    /* Start IB UD */
    rc = start_ib_ud();
    if (0 != rc) {
        error(log_fp, "Cannot start IB UD\n");
    }
    
    /* Start poll event */   
    ev_idle_start(EV_A_ &poll_event);
    
    if (*start_mode == 's') {
        /* Server joining the cluster */
        join_cluster_cb();
    }
    else {
    }

}

/**
 * Send join requests to the cluster
 * Note: the timer is stopped when receiving a JOIN reply (poll_ud)
 */
static void join_cluster_cb()
{
    int rc;
    
    rc = ib_join_cluster();
    if (0 != rc) {
        error(log_fp, "Cannot join cluster\n");
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
        error(log_fp, "Exchanging RC info failed\n");
    }
    
    /* Retransmit after retransmission period */
    //TODO
    
    return;
}

/* ================================================================== */

static void poll_cb()
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
    if (MSG_ERROR == type) {
        error(log_fp, "Cannot get UD message\n");
    }
    switch(type) {
        case CFG_REPLY:
        {
            info(log_fp, "I got accepted into the cluster: idx=%"PRIu8"\n", 
                rdma_data.config.idx); PRINT_CID_(rdma_data.config.cid);
            
            /* Start RC discovery */
            exchange_rc_info_cb()
            break;
        }
        case RC_SYNACK:
        case RC_ACK:
    }
}