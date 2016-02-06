extern FILE *log_fp;

/* InfiniBand device */
ib_device_t *ib_device;
#define IBDEV ib_device
#define SRV_DATA ((server_data_t*)ib_device->udata)

int init_ib_device(uint32_t receive_count)
{
    int i;
    int num_devs;
    struct ibv_device **ib_devs = NULL;
    
    /* Get list of devices (HCAs) */ 
    ib_devs = ibv_get_device_list(&num_devs);
    if (0 == num_devs) {
        error_return(1, log_fp, "No HCAs available\n");
    }
    if (NULL == ib_devs) {
        error_return(1, log_fp, "Get device list returned NULL\n");
    }
    
    /* Go through all devices and find one that we can use */
    for (i = 0; i < num_devs; i++) {
        /* Init device */
        IBDEV = init_one_device(ib_devs[i]);
        if (NULL != IBDEV) {
            /* Found it */
            break;
        }
    }
    
    /* Free device list */
    ibv_free_device_list(ib_devs);
    
    if (NULL == IBDEV) {
        /* Cannot find device */
        return 1;
    }

    /* Initialize IB UD connection */
    ud_init(receive_count);

    return 0; 
}

/**
 * Post UD receives
 * Note: After this call, we may receive messages over UD
 */
int start_ib_ud()
{
    return ud_start();
}

int init_ib_srv_data(void *data)
{
    int i;
    IBDEV->udata = data;
     
    for (i = 0; i < SRV_DATA->config.len; i++) {
        SRV_DATA->config.servers[i].ep = malloc(sizeof(ib_ep_t));
        if (NULL == SRV_DATA->config.servers[i].ep) {
            error_return(1, log_fp, "Cannot allocate EP\n");
        }
        memset(SRV_DATA->config.servers[i].ep, 0, sizeof(ib_ep_t));
    }
    
    return 0;
}

/** 
 * Init IB reliable connection (RC)
 */
int init_ib_rc()
{
    return rc_init();
}

ib_device_t* init_one_device(struct ibv_device* ib_dev)
{
    int i;
    ib_device_t *device = NULL;
    struct ibv_context *dev_context = NULL;
    
    /* Open up the device */
    dev_context = ibv_open_device(ib_dev);
    if (NULL == dev_context) {
        goto error;
    }
    
    /* Allocate new device */
    device = (ib_device_t*)malloc(sizeof(ib_device_t));
    if (NULL == device) {
        goto error;
    }
    memset(device, 0, sizeof(ib_device_t));
    
    /* Init device */
    device->ib_dev = ib_dev;
    device->ib_dev_context = dev_context;
    device->request_id = 1;
    
    /* Get device's attributes */
    if(ibv_query_device(device->ib_dev_context, &device->ib_dev_attr)){
        goto error;
    }

    if (IBV_ATOMIC_NONE == device->ib_dev_attr.atomic_cap) {
        info(log_fp, "# HCA %s does not support atomic operations\n", 
             ibv_get_device_name(device->ib_dev));
    }
    else {
        info(log_fp, "# HCA %s supports atomic operations\n", 
             ibv_get_device_name(device->ib_dev));
    }
    info(log_fp, "# max_qp_wr=%d\n", device->ib_dev_attr.max_qp_wr);
    info(log_fp, "# max_qp_rd_atom=%d\n", device->ib_dev_attr.max_qp_rd_atom);
    
    if (0 == device->ib_dev_attr.max_srq) {
        info(log_fp, "# HCA %s does not support Shared Receive Queues.\n", 
             ibv_get_device_name(device->ib_dev));
    }
    else {
        info(log_fp, "# HCA %s supports Shared Receive Queues.\n",
             ibv_get_device_name(device->ib_dev));
    }
    
    if (0 == device->ib_dev_attr.max_mcast_grp) {
        info(log_fp, "# HCA %s does not support multicast groups.\n", 
             ibv_get_device_name(device->ib_dev));
    }
    else {
        info(log_fp, "# HCA %s supports multicast groups.\n",
             ibv_get_device_name(device->ib_dev));
    }
    
    info(log_fp, "# HCA %s supports maximum %d WRs.\n", 
             ibv_get_device_name(device->ib_dev), device->ib_dev_attr.max_qp_wr);


    /* Find port */
    device->port_num = 0;
    for (i = 1; i <= device->ib_dev_attr.phys_port_cnt; i++) {
        struct ibv_port_attr ib_port_attr;
        if (ibv_query_port(device->ib_dev_context, i, &ib_port_attr)) {
            goto error;
        }
        if (IBV_PORT_ACTIVE != ib_port_attr.state) {
            continue;
        }

        /* find index of pkey 0xFFFF */
        uint16_t pkey, j;
        for (j = 0; j < device->ib_dev_attr.max_pkeys; j++) {
            if (ibv_query_pkey(device->ib_dev_context, i, j, &pkey)) {
                goto error;
            }
            pkey = ntohs(pkey);// & IB_PKEY_MASK;
            if (pkey == 0xFFFF) {
                device->pkey_index = j;
                break;
            }
        }
       //device->mtu = ib_port_attr.max_mtu;
        //device->mtu = ib_port_attr.active_mtu;
        device->mtu = IBV_MTU_4096;
        info(log_fp, "# ib_port_attr.active_mtu = %"PRIu32" (%d bytes)\n", 
             ib_port_attr.active_mtu, mtu_value(ib_port_attr.active_mtu));
        info(log_fp, "# device->mtu = %"PRIu32" (%d bytes)\n", 
             device->mtu, mtu_value(device->mtu));
 //       info(log_fp, "# ib_port_attr.lmc = %"PRIu8"\n", ib_port_attr.lmc);

        device->port_num = i;
        info(log_fp, "# device->port_num = %"PRIu8"\n", device->port_num);
        device->lid = ib_port_attr.lid;
        info(log_fp, "# ib_port_attr.lid = %"PRIu16"\n", ib_port_attr.lid);
        
        break;
    }
    if (0 == device->port_num) {
        goto error;
    }
      
    return device;

error:
    /* Free device */
    if (NULL != device) {
        free(device);
    }
    /* Free the device context */
    if (NULL != dev_context) {
        ibv_close_device(dev_context);
    }
    return NULL; 
}

/* ================================================================== */
/* Starting a server */

uint8_t ib_poll_ud_queue()
{
    return ud_get_message();
}

int ib_join_cluster()
{
    return ud_join_cluster();
}

int ib_exchange_rc_info()
{
    return ud_exchange_rc_info();
}

/* ================================================================== */

int find_max_inline(struct ibv_context *context, struct ibv_pd *pd, uint32_t *max_inline_arg)
{
    int rc;
    struct ibv_qp *qp = NULL;
    struct ibv_cq *cq = NULL;
    struct ibv_qp_init_attr init_attr;
    uint32_t max_inline_data;
    
    *max_inline_arg = 0;

    /* Make a dummy CQ */
    cq = ibv_create_cq(context, 1, NULL, NULL, 0);
    if (NULL == cq) {
        return 1;
    }
    
    /* Setup the QP attributes */
    memset(&init_attr, 0, sizeof(init_attr));
    init_attr.qp_type = IBV_QPT_RC;
    init_attr.send_cq = cq;
    init_attr.recv_cq = cq;
    init_attr.srq = 0;
    init_attr.cap.max_send_sge = 1;
    init_attr.cap.max_recv_sge = 1;
    init_attr.cap.max_recv_wr = 1;
    
    /* Loop over max_inline_data values; just check powers of 2 --
       that's good enough */
    init_attr.cap.max_inline_data = max_inline_data = 1 << 20;
    rc = 1;
    while (max_inline_data > 0) {
        qp = ibv_create_qp(pd, &init_attr);
        if (NULL != qp) {
            *max_inline_arg = max_inline_data;
            ibv_destroy_qp(qp);
            rc = 0;
            break;
        }
        max_inline_data >>= 1;
        init_attr.cap.max_inline_data = max_inline_data;
    }
    
    /* Destroy the temp CQ */
    if (NULL != cq) {
        ibv_destroy_cq(cq);
    }

    return rc;
}