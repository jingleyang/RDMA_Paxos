extern ib_device_t *ib_device;
#define IBDEV ib_device
#define SRV_DATA ((server_data_t*)ib_device->udata)

/**
 * Initialize RC data
 */
int rc_init()
{

    rc_prerequisite();

    /* Register memory for RC */
    rc_memory_reg();
    
    /* Create QPs for RC communication */
    for (i = 0; i < SRV_DATA->config.len; i++) {
        ib_ep_t* ep = (ib_ep_t*)
                        SRV_DATA->config.servers[i].ep;
        
        /* Create QPs for this endpoint */
        rc_qp_create(ep);
    }
    
    return 0;
}

int rc_prerequisite()
{   
    /* Allocate a RC protection domain */
    IBDEV->rc_pd = ibv_alloc_pd(IBDEV->ib_dev_context);

    return 0;
}

int rc_memory_reg()
{  
    /* Register memory for local log */    
    IBDEV->lcl_mr = ibv_reg_mr(IBDEV->rc_pd,
            SRV_DATA->log, sizeof(log_t) + SRV_DATA->log->len, 
            IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC | 
            IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE);

    return 0;
}

int rc_qp_create(ib_ep_t* ep )
{
    int i;
    struct ibv_qp_init_attr qp_init_attr;
    
    if (NULL == ep) return 0;

    /* Create RC QP */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.send_cq = IBDEV->rc_cq;
    qp_init_attr.recv_cq = IBDEV->rc_cq;
    qp_init_attr.cap.max_inline_data = IBDEV->rc_max_inline_data;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_wr = IBDEV->rc_max_send_wr;
    ep->rc_ep.rc_qp.qp = ibv_create_qp(IBDEV->rc_pd, &qp_init_attr);

    ep->rc_ep.rc_qp.signaled_wr_id = 0;
    ep->rc_ep.rc_qp.send_count = 0;
    ep->rc_ep.rc_qp.state = RC_QP_ACTIVE;

    return 0;
}

RSM_Op(req_msg){
    log_entry_t* new_entry = SRV_DATA->log->entries + SRV_DATA->log->end;
    SRV_DATA->log->end = LOG_ENTRY_SIZE + SRV_DATA->log->end;
    new_entry->idx = end / LOG_ENTRY_SIZE;
    
    new_entry->data_size = con_msg->data_size;
    memcpy(new_entry->data, req_msg->data, req_msg->data_size;);

    //store_record();

    //uint32_t offset
    uint8_t size = get_group_size(SRV_DATA->config);
    for (int i = 0; i < size; i++){
        if (i == SRV_DATA->config.idx){
            continue;
        }
        if (!CID_IS_SERVER_ON(SRV_DATA->config.cid, i)){
            continue;
        }

        ep = (ib_ep_t*)SRV_DATA->config.servers[i].ep;

        rem_mem_t rm;
        rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
        rm.rkey = ep->rc_ep.rmt_mr.rkey;
        //post_send(i, CTRL_QP, &SRV_DATA->ctrl_data->rsid[i], sizeof(uint64_t), IBDEV->lcl_mr[CTRL_QP], IBV_WR_RDMA_READ, SIGNALED, rm, posted_sends);
    }
}